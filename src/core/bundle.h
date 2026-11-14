#pragma once

#include "common.h"
#include "zip_archive.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace hl3 {

class Bundle {
public:
    bool open(const std::filesystem::path& root_or_ipa = {}, std::string* err = nullptr);

    const std::filesystem::path& root_dir() const { return root_dir_; }
    const std::filesystem::path& assets_dir() const { return assets_dir_; }
    const Bytes& macho_bytes() const { return macho_; }
    const std::string& language() const { return language_; }
    void set_language(std::string lang);

    bool has_asset(const std::string& rel_path) const;
    bool has_asset_or_dir(const std::string& rel_path) const;
    bool read_asset(const std::string& rel_path, Bytes& out) const;
    std::string resolve_override_rel(const std::string& rel_path) const;

private:
    std::filesystem::path root_dir_;
    std::filesystem::path assets_dir_;
    ZipArchive ipa_;
    Bytes macho_;
    std::string language_ = "en";
    mutable std::unordered_map<std::string, Bytes> cache_;
};

std::filesystem::path find_project_or_exe_root();
std::filesystem::path find_writable_data_root();
bool read_bundle_asset_file(const std::filesystem::path& path, Bytes& out);

}  // namespace hl3
