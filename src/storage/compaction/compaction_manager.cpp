#include "src/storage/compaction/compaction_manager.h"
#include "src/storage/sstable/sstable_reader.h"
#include "src/storage/sstable/sstable_writer.h"
#include "src/storage/memtable/skip_list.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <queue>
#include <sstream>
#include <iomanip>

namespace vectordb {
namespace storage {

using common::Status;
using common::StatusCode;
using common::ErrorOr;

// ─── Constructor ────────────────────────────────────────────────────────────

CompactionManager::CompactionManager(std::string data_dir, int max_level)
    : data_dir_(std::move(data_dir))
    , max_level_(max_level) {
    std::filesystem::create_directories(data_dir_);
}

// ─── Catalogue Management ────────────────────────────────────────────────────

Status CompactionManager::AddFile(SSTableMeta meta) {
    if (meta.level < 0 || meta.level >= max_level_) {
        return Status(StatusCode::kInvalidArgument,
                      "Invalid level: " + std::to_string(meta.level));
    }
    std::lock_guard<std::mutex> lock(catalogue_mutex_);
    files_.push_back(std::move(meta));
    return Status::Ok();
}

void CompactionManager::RemoveFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(catalogue_mutex_);
    files_.erase(
        std::remove_if(files_.begin(), files_.end(),
                       [&](const SSTableMeta& f) { return f.filepath == filepath; }),
        files_.end());
}

std::vector<SSTableMeta> CompactionManager::GetFilesAtLevel(int level) const {
    std::lock_guard<std::mutex> lock(catalogue_mutex_);
    std::vector<SSTableMeta> result;
    for (const auto& f : files_) {
        if (f.level == level) result.push_back(f);
    }
    std::sort(result.begin(), result.end(),
              [](const SSTableMeta& a, const SSTableMeta& b) {
                  return a.smallest_key < b.smallest_key;
              });
    return result;
}

uint64_t CompactionManager::TotalBytesAtLevel(int level) const {
    std::lock_guard<std::mutex> lock(catalogue_mutex_);
    uint64_t total = 0;
    for (const auto& f : files_) {
        if (f.level == level) total += f.file_size_bytes;
    }
    return total;
}

size_t CompactionManager::Level0FileCount() const {
    std::lock_guard<std::mutex> lock(catalogue_mutex_);
    return static_cast<size_t>(
        std::count_if(files_.begin(), files_.end(),
                      [](const SSTableMeta& f) { return f.level == 0; }));
}

std::vector<SSTableMeta> CompactionManager::AllFiles() const {
    std::lock_guard<std::mutex> lock(catalogue_mutex_);
    return files_;
}

// ─── Compaction Decision Logic ───────────────────────────────────────────────

bool CompactionManager::NeedsCompaction() const {
    return PickCompactionLevel() != -1;
}

int CompactionManager::PickCompactionLevel() const {
    // Level 0: count-based trigger
    size_t l0_count;
    {
        std::lock_guard<std::mutex> lock(catalogue_mutex_);
        l0_count = static_cast<size_t>(
            std::count_if(files_.begin(), files_.end(),
                          [](const SSTableMeta& f) { return f.level == 0; }));
    }
    if (l0_count >= leveled::kLevel0CompactionTrigger) {
        return 0;
    }

    // Level 1+: size-based trigger
    for (int level = 1; level < max_level_ - 1; ++level) {
        uint64_t total = TotalBytesAtLevel(level);
        if (total > leveled::MaxBytesForLevel(level)) {
            return level;
        }
    }

    return -1;
}

// ─── File Selection ──────────────────────────────────────────────────────────

std::pair<std::vector<SSTableMeta>, std::vector<SSTableMeta>>
CompactionManager::SelectCompactionFiles(int source_level) const {
    auto source_files = GetFilesAtLevel(source_level);
    if (source_files.empty()) return {{}, {}};

    std::vector<SSTableMeta> inputs;
    if (source_level == 0) {
        inputs = source_files; // Compact ALL Level 0 files
    } else {
        inputs.push_back(source_files.front()); // Compact smallest file first
    }

    // Determine key range of selected source files
    std::string range_lo = inputs.front().smallest_key;
    std::string range_hi = inputs.front().largest_key;
    for (const auto& f : inputs) {
        if (f.smallest_key < range_lo) range_lo = f.smallest_key;
        if (f.largest_key > range_hi) range_hi = f.largest_key;
    }

    // Find overlapping files in the target level
    int target_level = source_level + 1;
    auto target_files = GetFilesAtLevel(target_level);
    std::vector<SSTableMeta> overlapping;
    for (const auto& f : target_files) {
        if (f.OverlapsWith(range_lo, range_hi)) {
            overlapping.push_back(f);
        }
    }

    return {inputs, overlapping};
}

// ─── Output Path Generation ──────────────────────────────────────────────────

std::string CompactionManager::GenerateOutputPath(int level) const {
    std::ostringstream oss;
    oss << data_dir_ << "/level" << level
        << "_" << std::setw(8) << std::setfill('0') << next_file_number_.fetch_add(1)
        << ".sst";
    return oss.str();
}

// ─── Vector-based SSTable Writer ─────────────────────────────────────────────
// A lightweight adapter: takes a pre-sorted vector of BlockEntries and 
// writes them directly to an SSTable without going through a SkipList roundtrip.

namespace {

/**
 * A minimal SkipList-compatible iterator that walks a pre-sorted vector.
 * This allows us to reuse SSTableWriter::Write (which takes a SkipList::Iterator)
 * without rebuilding a SkipList from already-merged, already-sorted data.
 *
 * We achieve this by actually building a SkipList from the sorted entries.
 * The SkipList Put() will maintain sorted order since we insert in sorted order.
 */
Status WriteVectorToSSTable(const std::string& path,
                            const std::vector<BlockEntry>& entries) {
    if (entries.empty()) {
        return Status(StatusCode::kInvalidArgument, "Cannot write empty SSTable");
    }

    // Build a SkipList from the sorted entries
    // This is O(N log N) but entries are already sorted, so the skip list
    // will be built quickly with mostly right-side insertions
    SkipList sl;
    for (const auto& e : entries) {
        if (e.is_tombstone) {
            sl.Delete(e.key);
        } else {
            sl.Put(e.key, e.value);
        }
    }

    // Check if all entries were tombstones (SkipList may be non-empty with tombstones)
    // SSTableWriter::Write handles tombstones in the iterator output
    auto iter = sl.Begin();
    return SSTableWriter::Write(path, iter);
}

} // anonymous namespace

// ─── N-Way Merge ─────────────────────────────────────────────────────────────

struct MergeItem {
    BlockEntry entry;
    uint64_t seq; // Higher seq = newer = wins on duplicate keys

    bool operator>(const MergeItem& o) const {
        if (entry.key != o.entry.key) return entry.key > o.entry.key;
        return seq < o.seq; // Lower seq = older = loses (max-heap by seq)
    }
};

ErrorOr<std::vector<SSTableMeta>> CompactionManager::MergeFiles(
    const std::vector<SSTableMeta>& inputs,
    int target_level,
    CompactionStats& stats) {

    // Read all entries from every input file
    std::vector<std::pair<std::vector<BlockEntry>, size_t>> file_data;
    file_data.reserve(inputs.size());

    for (const auto& meta : inputs) {
        auto reader_or = SSTableReader::Open(meta.filepath);
        if (!reader_or.IsOk()) return reader_or.status();

        auto all_or = reader_or.value()->ReadAll();
        if (!all_or.IsOk()) return all_or.status();

        stats.keys_merged += all_or.value().size();
        stats.bytes_read += meta.file_size_bytes;
        file_data.push_back({std::move(all_or).value(), 0});
    }

    // N-way merge using a min-heap
    using HeapItem = std::pair<MergeItem, size_t>; // {merge_item, file_index}
    auto cmp = [](const HeapItem& a, const HeapItem& b) {
        return a.first > b.first;
    };
    std::priority_queue<HeapItem, std::vector<HeapItem>, decltype(cmp)> heap(cmp);

    // Seed heap with first entry from each file
    for (size_t i = 0; i < file_data.size(); ++i) {
        if (!file_data[i].first.empty()) {
            MergeItem item;
            item.entry = file_data[i].first[0];
            item.seq = inputs[i].sequence_number;
            heap.push({item, i});
            file_data[i].second = 1;
        }
    }

    bool is_bottom_level = (target_level == max_level_ - 1);
    std::vector<SSTableMeta> output_metas;
    std::vector<BlockEntry> output_entries;
    std::string last_emitted_key;
    uint64_t out_bytes_estimate = 0;
    std::string current_output_path = GenerateOutputPath(target_level);

    // Helper: flush accumulated output_entries to a new SSTable file
    auto flush_output = [&]() -> Status {
        if (output_entries.empty()) return Status::Ok();

        auto ws = WriteVectorToSSTable(current_output_path, output_entries);
        if (!ws.IsOk()) return ws;

        SSTableMeta out_meta;
        out_meta.filepath = current_output_path;
        out_meta.level = target_level;
        out_meta.smallest_key = output_entries.front().key;
        out_meta.largest_key = output_entries.back().key;
        out_meta.file_size_bytes = std::filesystem::exists(current_output_path)
                                       ? std::filesystem::file_size(current_output_path)
                                       : out_bytes_estimate;
        out_meta.sequence_number = next_file_number_.load();

        output_metas.push_back(std::move(out_meta));
        stats.files_written++;
        stats.bytes_written += output_metas.back().file_size_bytes;

        output_entries.clear();
        out_bytes_estimate = 0;
        current_output_path = GenerateOutputPath(target_level);
        return Status::Ok();
    };

    // Drain the heap in merge-sorted order
    while (!heap.empty()) {
        auto [item, file_idx] = heap.top();
        heap.pop();

        // Push the next entry from the same file
        size_t& next_idx = file_data[file_idx].second;
        if (next_idx < file_data[file_idx].first.size()) {
            MergeItem next_item;
            next_item.entry = file_data[file_idx].first[next_idx];
            next_item.seq = inputs[file_idx].sequence_number;
            heap.push({next_item, file_idx});
            ++next_idx;
        }

        // Skip duplicate keys (older version already emitted or about to be)
        if (item.entry.key == last_emitted_key) {
            stats.duplicate_keys_dropped++;
            continue;
        }
        last_emitted_key = item.entry.key;

        // Drop tombstones at the bottom level
        if (item.entry.is_tombstone && is_bottom_level) {
            stats.tombstones_dropped++;
            continue;
        }

        out_bytes_estimate += 4 + item.entry.key.size() + 4 + item.entry.value.size() + 1;
        output_entries.push_back(item.entry);

        // Split output at 64MB boundaries
        if (out_bytes_estimate >= 64ULL * 1024 * 1024) {
            auto s = flush_output();
            if (!s.IsOk()) return s;
        }
    }

    // Flush the final batch
    auto s = flush_output();
    if (!s.IsOk()) return s;

    return output_metas;
}

// ─── RunCompaction ───────────────────────────────────────────────────────────

ErrorOr<CompactionStats> CompactionManager::RunCompaction() {
    std::lock_guard<std::mutex> compaction_lock(compaction_mutex_);

    int source_level = PickCompactionLevel();
    if (source_level == -1) {
        return Status(StatusCode::kNotFound, "No compaction needed");
    }

    auto start = std::chrono::steady_clock::now();

    CompactionStats stats;
    stats.source_level = source_level;
    stats.target_level = source_level + 1;

    // ── 1. Select files ──────────────────────────────────────────────
    auto [source_files, target_files] = SelectCompactionFiles(source_level);
    if (source_files.empty()) {
        return Status(StatusCode::kNotFound, "No source files selected for compaction");
    }

    // Assign sequence numbers: source files are newer (higher seq wins on conflict)
    size_t total_inputs = source_files.size() + target_files.size();
    for (size_t i = 0; i < source_files.size(); ++i) {
        source_files[i].sequence_number = total_inputs - i; // e.g. 4, 3, 2 ...
    }
    for (size_t i = 0; i < target_files.size(); ++i) {
        target_files[i].sequence_number = i; // e.g. 0, 1 ... (older)
    }

    std::vector<SSTableMeta> all_inputs;
    all_inputs.insert(all_inputs.end(), source_files.begin(), source_files.end());
    all_inputs.insert(all_inputs.end(), target_files.begin(), target_files.end());
    stats.files_read = all_inputs.size();

    // ── 2. Merge → write new SSTables ───────────────────────────────
    ASSIGN_OR_RETURN(auto new_files, MergeFiles(all_inputs, stats.target_level, stats));

    // ── 3. Atomically update catalogue ──────────────────────────────
    for (auto& meta : new_files) {
        auto s = AddFile(meta);
        if (!s.IsOk()) return s;
    }
    for (const auto& old_file : all_inputs) {
        RemoveFile(old_file.filepath);
        std::filesystem::remove(old_file.filepath);
    }

    auto end = std::chrono::steady_clock::now();
    stats.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return stats;
}

} // namespace storage
} // namespace vectordb
