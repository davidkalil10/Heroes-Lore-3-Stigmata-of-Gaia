#include "zip_archive.h"

#include <algorithm>
#include <cstring>
#include <zlib.h>

namespace hl3 {

namespace {

bool inflate_raw_deflate(const u8* src, size_t src_len, size_t expected_size, Bytes& out) {
    out.resize(expected_size);
    if (expected_size == 0) return true;

    z_stream zs{};
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
    zs.avail_in = static_cast<uInt>(src_len);
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
        out.clear();
        return false;
    }
    zs.next_out = reinterpret_cast<Bytef*>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());

    int rc = inflate(&zs, Z_FINISH);
    size_t produced = static_cast<size_t>(zs.total_out);
    inflateEnd(&zs);
    if (rc != Z_STREAM_END && !(rc == Z_OK && produced == expected_size)) {
        out.clear();
        return false;
    }
    out.resize(produced);
    return true;
}

}  // namespace

bool is_safe_archive_rel_path(const std::string& rel) {
    if (rel.empty()) return false;
    if (rel[0] == '/' || rel[0] == '\\') return false;
    if (rel.size() >= 2 && rel[1] == ':') return false;
    std::string norm = rel;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    size_t i = 0;
    while (i <= norm.size()) {
        size_t slash = norm.find('/', i);
        std::string part = (slash == std::string::npos) ? norm.substr(i) : norm.substr(i, slash - i);
        if (part == "..") return false;
        if (slash == std::string::npos) break;
        i = slash + 1;
    }
    return true;
}

bool ZipArchive::open_from_memory(Bytes data) {
    raw_ = std::move(data);
    parsed_ = parse_central_directory();
    return parsed_;
}

bool ZipArchive::open_from_file(const std::filesystem::path& path) {
    Bytes data;
    if (!read_file_bytes(path, data)) return false;
    return open_from_memory(std::move(data));
}

bool ZipArchive::parse_central_directory() {
    entries_.clear();
    by_name_.clear();
    if (raw_.size() < 22) return false;

    const size_t min_pos = (raw_.size() > 65557) ? (raw_.size() - 65557) : 0;
    size_t eocd_pos = std::string::npos;
    for (size_t pos = raw_.size() - 22;; --pos) {
        if (read_u32_le(raw_.data() + pos) == 0x06054b50u) {
            u16 comment_len = read_u16_le(raw_.data() + pos + 20);
            if (pos + 22 + comment_len == raw_.size()) {
                eocd_pos = pos;
                break;
            }
        }
        if (pos == min_pos) break;
    }
    if (eocd_pos == std::string::npos) return false;

    u16 total_entries = read_u16_le(raw_.data() + eocd_pos + 10);
    u32 cd_size = read_u32_le(raw_.data() + eocd_pos + 12);
    u32 cd_offset = read_u32_le(raw_.data() + eocd_pos + 16);
    if (size_t(cd_offset) + size_t(cd_size) > raw_.size()) return false;

    size_t p = cd_offset;
    for (u16 idx = 0; idx < total_entries; ++idx) {
        if (p + 46 > raw_.size()) return false;
        if (read_u32_le(raw_.data() + p) != 0x02014b50u) return false;

        ZipEntry e;
        e.method = read_u16_le(raw_.data() + p + 10);
        e.crc32 = read_u32_le(raw_.data() + p + 16);
        e.compressed_size = read_u32_le(raw_.data() + p + 20);
        e.uncompressed_size = read_u32_le(raw_.data() + p + 24);
        u16 name_len = read_u16_le(raw_.data() + p + 28);
        u16 extra_len = read_u16_le(raw_.data() + p + 30);
        u16 comment_len = read_u16_le(raw_.data() + p + 32);
        e.local_header_offset = read_u32_le(raw_.data() + p + 42);
        if (p + 46 + name_len + extra_len + comment_len > raw_.size()) return false;

        e.name.assign(reinterpret_cast<const char*>(raw_.data() + p + 46), name_len);
        std::replace(e.name.begin(), e.name.end(), '\\', '/');
        e.is_dir = !e.name.empty() && e.name.back() == '/';

        size_t index = entries_.size();
        by_name_[e.name] = index;
        entries_.push_back(std::move(e));
        p += 46 + name_len + extra_len + comment_len;
    }
    return true;
}

bool ZipArchive::has_entry(const std::string& name) const {
    return by_name_.find(name) != by_name_.end();
}

bool ZipArchive::decompress_entry(const ZipEntry& entry, Bytes& out) const {
    size_t lh = entry.local_header_offset;
    if (lh + 30 > raw_.size()) return false;
    if (read_u32_le(raw_.data() + lh) != 0x04034b50u) return false;

    u16 name_len = read_u16_le(raw_.data() + lh + 26);
    u16 extra_len = read_u16_le(raw_.data() + lh + 28);
    size_t data_off = lh + 30 + name_len + extra_len;
    if (data_off + entry.compressed_size > raw_.size()) return false;

    const u8* src = raw_.data() + data_off;
    if (entry.method == 0) {
        out.assign(src, src + entry.compressed_size);
        return true;
    }
    if (entry.method == 8) {
        return inflate_raw_deflate(src, entry.compressed_size, entry.uncompressed_size, out);
    }
    return false;
}

bool ZipArchive::read_entry(const std::string& name, Bytes& out) const {
    auto it = by_name_.find(name);
    if (it == by_name_.end()) return false;
    return decompress_entry(entries_[it->second], out);
}

bool ZipArchive::extract_all_safe(const std::filesystem::path& dest_dir, std::string& err) const {
    if (!is_open()) {
        err = "Archive is not open";
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(dest_dir, ec);
    for (const auto& entry : entries_) {
        if (!is_safe_archive_rel_path(entry.name)) {
            err = "Unsafe zip entry path: " + entry.name;
            return false;
        }
        std::filesystem::path target = dest_dir / std::filesystem::u8path(entry.name);
        if (entry.is_dir) {
            std::filesystem::create_directories(target, ec);
            continue;
        }
        Bytes payload;
        if (!decompress_entry(entry, payload)) {
            err = "Failed to extract entry: " + entry.name;
            return false;
        }
        if (!write_file_bytes(target, payload.data(), payload.size())) {
            err = "Failed to write extracted file: " + target.u8string();
            return false;
        }
    }
    return true;
}

}  // namespace hl3

