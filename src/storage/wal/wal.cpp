#include "src/storage/wal/wal.h"
#include "src/common/serialization/binary_serializer.h"
#include <fstream>
#include <filesystem>
#include <cstring>

namespace vectordb {
namespace storage {

using common::Status;
using common::StatusCode;
using common::ErrorOr;
using common::BinarySerializer;

// ─── WALEntry helpers ───────────────────────────────────────────────

size_t WALEntry::PayloadSize() const {
    // type(1) + seq(8) + key_len(4) + key + value_len(4) + value
    size_t size = 1 + 8 + 4 + key.size();
    if (type == WALEntryType::kPut) {
        size += 4 + value.size();
    }
    return size;
}

// ─── CRC32 (WAL-specific — integrity checking for crash recovery) ───

// CRC32 lookup table (IEEE polynomial 0xEDB88320)
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void InitCRC32Table() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

uint32_t WAL::ComputeCRC32(const uint8_t* data, size_t length) {
    if (!crc32_table_initialized) InitCRC32Table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

// ─── WAL-specific helper (raw byte append — not in BinarySerializer) ─

static void WriteRawBytes(std::vector<uint8_t>& buf, const void* data, size_t len) {
    size_t pos = buf.size();
    buf.resize(pos + len);
    std::memcpy(buf.data() + pos, data, len);
}

// ─── Serialization (using Phase 1 BinarySerializer) ─────────────────

std::vector<uint8_t> WAL::SerializeEntry(const WALEntry& entry) {
    // Build the payload: type(1) + seq(8) + key_len(4) + key + [value_len(4) + value]
    std::vector<uint8_t> payload;
    payload.reserve(entry.PayloadSize());

    payload.push_back(static_cast<uint8_t>(entry.type));
    BinarySerializer::WriteUint64(payload, entry.sequence_number);
    BinarySerializer::WriteUint32(payload, static_cast<uint32_t>(entry.key.size()));
    WriteRawBytes(payload, entry.key.data(), entry.key.size());

    if (entry.type == WALEntryType::kPut) {
        BinarySerializer::WriteUint32(payload, static_cast<uint32_t>(entry.value.size()));
        WriteRawBytes(payload, entry.value.data(), entry.value.size());
    }

    // Build the full record: length(4) + crc32(4) + payload
    uint32_t payload_len = static_cast<uint32_t>(payload.size());
    uint32_t crc = ComputeCRC32(payload.data(), payload.size());

    std::vector<uint8_t> record;
    record.reserve(kWALHeaderSize + payload.size());
    BinarySerializer::WriteUint32(record, payload_len);
    BinarySerializer::WriteUint32(record, crc);
    record.insert(record.end(), payload.begin(), payload.end());

    return record;
}

ErrorOr<WALEntry> WAL::DeserializeEntry(const std::vector<uint8_t>& payload) {
    if (payload.size() < 13) { // type(1) + seq(8) + key_len(4) minimum
        return Status(StatusCode::kStorageCorruption, "WAL payload too small");
    }

    size_t offset = 0;
    WALEntry entry;

    // Read type byte manually (single byte — not worth a BinarySerializer call)
    entry.type = static_cast<WALEntryType>(payload[offset++]);

    // Read sequence number using BinarySerializer (with bounds checking)
    ASSIGN_OR_RETURN(entry.sequence_number, BinarySerializer::ReadUint64(payload, offset));

    // Read key length and key data
    ASSIGN_OR_RETURN(auto key_len, BinarySerializer::ReadUint32(payload, offset));

    if (offset + key_len > payload.size()) {
        return Status(StatusCode::kStorageCorruption, "WAL key extends past payload");
    }
    entry.key = std::string(reinterpret_cast<const char*>(payload.data() + offset), key_len);
    offset += key_len;

    // Read value for Put entries
    if (entry.type == WALEntryType::kPut) {
        ASSIGN_OR_RETURN(auto val_len, BinarySerializer::ReadUint32(payload, offset));

        if (offset + val_len > payload.size()) {
            return Status(StatusCode::kStorageCorruption, "WAL value extends past payload");
        }
        entry.value.assign(payload.data() + offset, payload.data() + offset + val_len);
    }

    return entry;
}

// ─── WAL Core ───────────────────────────────────────────────────────

WAL::WAL(const std::string& path, std::ofstream file, uint64_t next_seq)
    : path_(path)
    , write_file_(std::move(file))
    , next_sequence_number_(next_seq) {}

WAL::~WAL() {
    if (write_file_.is_open()) {
        write_file_.flush();
        write_file_.close();
    }
}

ErrorOr<std::unique_ptr<WAL>> WAL::Open(const std::string& path) {
    // Create parent directories if they don't exist
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    uint64_t next_seq = 1;

    // If the file already exists, scan it to find the highest sequence number
    if (std::filesystem::exists(path)) {
        std::ifstream reader(path, std::ios::binary);
        if (!reader.is_open()) {
            return Status(StatusCode::kStorageIoError, "Cannot open existing WAL: " + path);
        }

        while (reader.good() && !reader.eof()) {
            // Read the 8-byte header into a buffer and parse with BinarySerializer
            std::vector<uint8_t> header_buf(kWALHeaderSize);
            reader.read(reinterpret_cast<char*>(header_buf.data()), kWALHeaderSize);
            if (reader.gcount() < static_cast<std::streamsize>(kWALHeaderSize)) break;

            size_t hdr_offset = 0;
            auto payload_len_or = BinarySerializer::ReadUint32(header_buf, hdr_offset);
            if (!payload_len_or.IsOk()) break;
            auto expected_crc_or = BinarySerializer::ReadUint32(header_buf, hdr_offset);
            if (!expected_crc_or.IsOk()) break;

            uint32_t payload_len = payload_len_or.value();
            uint32_t expected_crc = expected_crc_or.value();

            // Read payload
            std::vector<uint8_t> payload(payload_len);
            reader.read(reinterpret_cast<char*>(payload.data()), payload_len);
            if (reader.gcount() < static_cast<std::streamsize>(payload_len)) break;

            // Verify CRC
            uint32_t actual_crc = ComputeCRC32(payload.data(), payload.size());
            if (actual_crc != expected_crc) break;

            // Parse to get sequence number
            auto entry_or = DeserializeEntry(payload);
            if (!entry_or.IsOk()) break;

            if (entry_or.value().sequence_number >= next_seq) {
                next_seq = entry_or.value().sequence_number + 1;
            }
        }
    }

    // Open for appending
    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        return Status(StatusCode::kStorageIoError, "Cannot open WAL for writing: " + path);
    }

    return std::unique_ptr<WAL>(new WAL(path, std::move(file), next_seq));
}

ErrorOr<uint64_t> WAL::Append(const WALEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Assign sequence number
    WALEntry stamped_entry = entry;
    stamped_entry.sequence_number = next_sequence_number_++;

    // Serialize and write
    auto record = SerializeEntry(stamped_entry);
    write_file_.write(reinterpret_cast<const char*>(record.data()), 
                      static_cast<std::streamsize>(record.size()));

    if (!write_file_.good()) {
        return Status(StatusCode::kStorageIoError, "WAL write failed");
    }

    // fsync to guarantee durability
    write_file_.flush();

    return stamped_entry.sequence_number;
}

ErrorOr<std::vector<WALEntry>> WAL::ReadAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream reader(path_, std::ios::binary);
    if (!reader.is_open()) {
        return Status(StatusCode::kStorageIoError, "Cannot open WAL for reading: " + path_);
    }

    std::vector<WALEntry> entries;

    while (reader.good() && !reader.eof()) {
        // Read the 8-byte header and parse with BinarySerializer
        std::vector<uint8_t> header_buf(kWALHeaderSize);
        reader.read(reinterpret_cast<char*>(header_buf.data()), kWALHeaderSize);
        if (reader.gcount() < static_cast<std::streamsize>(kWALHeaderSize)) break;

        size_t hdr_offset = 0;
        auto payload_len_or = BinarySerializer::ReadUint32(header_buf, hdr_offset);
        if (!payload_len_or.IsOk()) break;
        auto expected_crc_or = BinarySerializer::ReadUint32(header_buf, hdr_offset);
        if (!expected_crc_or.IsOk()) break;

        uint32_t payload_len = payload_len_or.value();
        uint32_t expected_crc = expected_crc_or.value();

        // Sanity check — reject obviously corrupted length values
        if (payload_len > 64 * 1024 * 1024) break; // > 64MB is clearly wrong

        // Read payload
        std::vector<uint8_t> payload(payload_len);
        reader.read(reinterpret_cast<char*>(payload.data()), payload_len);
        if (reader.gcount() < static_cast<std::streamsize>(payload_len)) break;

        // Verify CRC32 — if mismatch, this is a partial/corrupted write
        uint32_t actual_crc = ComputeCRC32(payload.data(), payload.size());
        if (actual_crc != expected_crc) break;

        // Deserialize
        auto entry_or = DeserializeEntry(payload);
        if (!entry_or.IsOk()) break;

        entries.push_back(std::move(entry_or).value());
    }

    return entries;
}

Status WAL::Sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    write_file_.flush();
    if (!write_file_.good()) {
        return Status(StatusCode::kStorageIoError, "WAL sync failed");
    }
    return Status::Ok();
}

} // namespace storage
} // namespace vectordb
