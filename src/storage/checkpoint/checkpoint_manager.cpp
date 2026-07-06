#include "src/storage/checkpoint/checkpoint_manager.h"
#include "src/common/serialization/binary_serializer.h"
#include <fstream>
#include <filesystem>
#include <cstdio>

namespace vectordb {
namespace storage {

CheckpointManager::CheckpointManager(std::string data_dir)
    : data_dir_(std::move(data_dir)) {
    checkpoint_path_ = data_dir_ + "/MANIFEST.checkpoint";
    temp_path_ = data_dir_ + "/MANIFEST.checkpoint.tmp";
    
    std::filesystem::create_directories(data_dir_);
}

common::Status CheckpointManager::SaveCheckpoint(const Checkpoint& checkpoint) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Serialize data
    std::vector<uint8_t> buffer;
    common::BinarySerializer::WriteUint64(buffer, checkpoint.last_flushed_lsn);
    common::BinarySerializer::WriteUint32(buffer, checkpoint.sstables.size());

    for (const auto& meta : checkpoint.sstables) {
        common::BinarySerializer::WriteUint32(buffer, meta.level);
        common::BinarySerializer::WriteUint64(buffer, meta.file_size_bytes);
        common::BinarySerializer::WriteUint64(buffer, meta.sequence_number);
        common::BinarySerializer::WriteString(buffer, meta.smallest_key);
        common::BinarySerializer::WriteString(buffer, meta.largest_key);
        common::BinarySerializer::WriteString(buffer, meta.filepath);
    }

    // 2. Write to temp file
    std::ofstream out(temp_path_, std::ios::binary | std::ios::trunc);
    if (!out) {
        return common::Status(common::StatusCode::kStorageIoError, "Failed to open temp checkpoint file for writing: " + temp_path_);
    }
    out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    out.flush();
    if (out.fail()) {
        return common::Status(common::StatusCode::kStorageIoError, "Failed to write to temp checkpoint file: " + temp_path_);
    }
    out.close();

    // 3. Atomic rename (POSIX guarantee)
    if (std::rename(temp_path_.c_str(), checkpoint_path_.c_str()) != 0) {
        return common::Status(common::StatusCode::kStorageIoError, "Failed to atomically rename checkpoint file");
    }

    return common::Status::Ok();
}

common::ErrorOr<Checkpoint> CheckpointManager::LoadCheckpoint() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!std::filesystem::exists(checkpoint_path_)) {
        // Return default empty checkpoint if it doesn't exist
        return Checkpoint{0, {}};
    }

    std::ifstream in(checkpoint_path_, std::ios::binary | std::ios::ate);
    if (!in) {
        return common::Status(common::StatusCode::kStorageIoError, "Failed to open checkpoint file");
    }

    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return common::Status(common::StatusCode::kStorageIoError, "Failed to read checkpoint file");
    }

    Checkpoint checkpoint;
    size_t offset = 0;

    auto lsn_res = common::BinarySerializer::ReadUint64(buffer, offset);
    if (!lsn_res.IsOk()) return lsn_res.status();
    checkpoint.last_flushed_lsn = lsn_res.value();

    auto num_res = common::BinarySerializer::ReadUint32(buffer, offset);
    if (!num_res.IsOk()) return num_res.status();
    uint32_t num_files = num_res.value();

    for (uint32_t i = 0; i < num_files; ++i) {
        SSTableMeta meta;

        auto lvl_res = common::BinarySerializer::ReadUint32(buffer, offset);
        if (!lvl_res.IsOk()) return lvl_res.status();
        meta.level = lvl_res.value();

        auto fs_res = common::BinarySerializer::ReadUint64(buffer, offset);
        if (!fs_res.IsOk()) return fs_res.status();
        meta.file_size_bytes = fs_res.value();

        auto sq_res = common::BinarySerializer::ReadUint64(buffer, offset);
        if (!sq_res.IsOk()) return sq_res.status();
        meta.sequence_number = sq_res.value();

        auto sk_res = common::BinarySerializer::ReadString(buffer, offset);
        if (!sk_res.IsOk()) return sk_res.status();
        meta.smallest_key = sk_res.value();

        auto lk_res = common::BinarySerializer::ReadString(buffer, offset);
        if (!lk_res.IsOk()) return lk_res.status();
        meta.largest_key = lk_res.value();

        auto path_res = common::BinarySerializer::ReadString(buffer, offset);
        if (!path_res.IsOk()) return path_res.status();
        meta.filepath = path_res.value();

        checkpoint.sstables.push_back(meta);
    }

    return checkpoint;
}

} // namespace storage
} // namespace vectordb
