#pragma once

#include "common.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace hl3 {

struct ZipEntry {
    std::string name;
    u16 method = 0;
    u32 crc32 = 0;
    u32 compressed_size = 0;
    u32 uncompressed_size = 0;
    u32 local_header_offset = 0;
    bool is_dir = false;
};

class ZipArchive {
public:
    bool open_from_memory(Bytes data);
    bool open_from_file(const std::filesystem::path& path);

    bool is_open() const { return !raw_.empty() && parsed_; }
    bool has_entry(const std::string& name) const;
    bool read_entry(const std::string& name, Bytes& out) const;
    const std::vector<ZipEntry>& entries() const { return entries_; }

    // Safe extraction checking against path traversal (../ or drive letters).
    bool extract_all_safe(const std::filesystem::path& dest_dir, std::string& err) const;

private:
    bool parse_central_directory();
    bool decompress_entry(const ZipEntry& entry, Bytes& out) const;

    Bytes raw_;
    bool parsed_ = false;
    std::vector<ZipEntry> entries_;
    std::unordered_map<std::string, size_t> by_name_;
};

// Path safety validation reusable by zip extractor and OTA updater.
bool is_safe_archive_rel_path(const std::string& rel);

}  // namespace hl3

