#pragma once

#include "../core/common.h"

#include <filesystem>
#include <string>

namespace hl3 {

bool decode_png_rgba(const Bytes& png_bytes, int& out_w, int& out_h, Bytes& out_rgba, std::string* err = nullptr);
bool load_png_rgba_file(const std::filesystem::path& path, int& out_w, int& out_h, Bytes& out_rgba, std::string* err = nullptr);

// Saves RGB or RGBA buffer (top-down row order) to PNG file via zlib deflate.
bool save_rgba_png_file(const std::filesystem::path& path, int w, int h, const u8* rgba, bool flip_y = false, bool strip_alpha = false);

}  // namespace hl3

