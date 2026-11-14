#include "png_codec.h"
#include "../core/bundle.h"

#include <zlib.h>

#include <algorithm>
#include <cstring>

namespace hl3 {

namespace {

u32 read_u32_be(const u8* p) {
    return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | u32(p[3]);
}

void write_u32_be(Bytes& out, u32 v) {
    out.push_back(static_cast<u8>((v >> 24) & 0xFFu));
    out.push_back(static_cast<u8>((v >> 16) & 0xFFu));
    out.push_back(static_cast<u8>((v >> 8) & 0xFFu));
    out.push_back(static_cast<u8>(v & 0xFFu));
}

u8 paeth_predictor(int a, int b, int c) {
    int p = a + b - c;
    int pa = std::abs(p - a);
    int pb = std::abs(p - b);
    int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return static_cast<u8>(a);
    if (pb <= pc) return static_cast<u8>(b);
    return static_cast<u8>(c);
}

bool inflate_zlib_stream(const u8* src, size_t src_len, size_t expected_len, Bytes& out) {
    z_stream zs{};
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
    zs.avail_in = static_cast<uInt>(src_len);
    if (inflateInit(&zs) != Z_OK) return false;

    out.resize(expected_len);
    zs.next_out = out.data();
    zs.avail_out = static_cast<uInt>(expected_len);

    int rc = Z_OK;
    while (rc == Z_OK) {
        if (zs.avail_out == 0) {
            size_t old = out.size();
            out.resize(old + 65536);
            zs.next_out = out.data() + old;
            zs.avail_out = 65536;
        }
        rc = inflate(&zs, Z_NO_FLUSH);
    }
    size_t final_sz = static_cast<size_t>(zs.total_out);
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) return false;
    out.resize(final_sz);
    return true;
}

void append_png_chunk(Bytes& dst, const char type[4], const u8* data, size_t len) {
    write_u32_be(dst, static_cast<u32>(len));
    size_t crc_start = dst.size();
    dst.insert(dst.end(), type, type + 4);
    if (len > 0 && data) {
        dst.insert(dst.end(), data, data + len);
    }
    uLong crc = ::crc32(0L, Z_NULL, 0);
    crc = ::crc32(crc, dst.data() + crc_start, static_cast<uInt>(4 + len));
    write_u32_be(dst, static_cast<u32>(crc));
}

}  // namespace

bool decode_png_rgba(const Bytes& png_bytes, int& out_w, int& out_h, Bytes& out_rgba, std::string* err) {
    static constexpr u8 kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (png_bytes.size() < 33 || std::memcmp(png_bytes.data(), kSig, 8) != 0) {
        if (err) *err = "Invalid PNG signature";
        return false;
    }

    int w = 0, h = 0, bit_depth = 0, color_type = 0, interlace = 0;
    bool have_ihdr = false;
    Bytes idat;
    std::vector<u8> plte;
    std::vector<u8> trns;

    size_t pos = 8;
    while (pos + 12 <= png_bytes.size()) {
        u32 chunk_len = read_u32_be(png_bytes.data() + pos);
        const char* ctype = reinterpret_cast<const char*>(png_bytes.data() + pos + 4);
        pos += 8;
        if (pos + chunk_len + 4 > png_bytes.size()) {
            if (err) *err = "Truncated PNG chunk";
            return false;
        }
        const u8* cdata = png_bytes.data() + pos;
        if (std::memcmp(ctype, "IHDR", 4) == 0 && chunk_len >= 13) {
            w = static_cast<int>(read_u32_be(cdata));
            h = static_cast<int>(read_u32_be(cdata + 4));
            bit_depth = cdata[8];
            color_type = cdata[9];
            interlace = cdata[12];
            have_ihdr = true;
        } else if (std::memcmp(ctype, "PLTE", 4) == 0) {
            plte.assign(cdata, cdata + chunk_len);
        } else if (std::memcmp(ctype, "tRNS", 4) == 0) {
            trns.assign(cdata, cdata + chunk_len);
        } else if (std::memcmp(ctype, "IDAT", 4) == 0) {
            idat.insert(idat.end(), cdata, cdata + chunk_len);
        } else if (std::memcmp(ctype, "IEND", 4) == 0) {
            break;
        }
        pos += chunk_len + 4;
    }

    if (!have_ihdr || w <= 0 || h <= 0 || w > 8192 || h > 8192 || interlace != 0) {
        if (err) *err = "Unsupported or invalid PNG IHDR";
        return false;
    }
    if (bit_depth != 8 && bit_depth != 16) {
        if (err) *err = "Unsupported PNG bit depth";
        return false;
    }

    int channels = 0;
    switch (color_type) {
    case 0: channels = 1; break;
    case 2: channels = 3; break;
    case 3: channels = 1; break;
    case 4: channels = 2; break;
    case 6: channels = 4; break;
    default:
        if (err) *err = "Unsupported PNG color type";
        return false;
    }

    size_t bpp = static_cast<size_t>(channels) * (bit_depth == 16 ? 2u : 1u);
    size_t row_bytes = static_cast<size_t>(w) * bpp;
    size_t expected_raw = static_cast<size_t>(h) * (row_bytes + 1u);

    Bytes raw;
    if (!inflate_zlib_stream(idat.data(), idat.size(), expected_raw, raw) || raw.size() < expected_raw) {
        if (err) *err = "PNG inflate failed";
        return false;
    }

    Bytes recon(static_cast<size_t>(h) * row_bytes, 0);
    for (int y = 0; y < h; ++y) {
        const u8* src_row = raw.data() + static_cast<size_t>(y) * (row_bytes + 1u);
        u8 filter = src_row[0];
        const u8* in_px = src_row + 1;
        u8* dst_px = recon.data() + static_cast<size_t>(y) * row_bytes;
        const u8* prev_px = (y > 0) ? (recon.data() + static_cast<size_t>(y - 1) * row_bytes) : nullptr;

        for (size_t i = 0; i < row_bytes; ++i) {
            u8 a = (i >= bpp) ? dst_px[i - bpp] : 0u;
            u8 b = prev_px ? prev_px[i] : 0u;
            u8 c = (prev_px && i >= bpp) ? prev_px[i - bpp] : 0u;
            u8 x = in_px[i];
            switch (filter) {
            case 0: dst_px[i] = x; break;
            case 1: dst_px[i] = static_cast<u8>(x + a); break;
            case 2: dst_px[i] = static_cast<u8>(x + b); break;
            case 3: dst_px[i] = static_cast<u8>(x + ((static_cast<int>(a) + static_cast<int>(b)) >> 1)); break;
            case 4: dst_px[i] = static_cast<u8>(x + paeth_predictor(a, b, c)); break;
            default:
                if (err) *err = "Invalid PNG filter mode";
                return false;
            }
        }
    }

    out_w = w;
    out_h = h;
    out_rgba.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
    int step = (bit_depth == 16) ? 2 : 1;

    for (int y = 0; y < h; ++y) {
        const u8* row = recon.data() + static_cast<size_t>(y) * row_bytes;
        u8* dst = out_rgba.data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4u;
        for (int x = 0; x < w; ++x) {
            const u8* p = row + static_cast<size_t>(x) * bpp;
            u8 r = 0, g = 0, b = 0, a = 255;
            if (color_type == 0) {
                r = g = b = p[0];
            } else if (color_type == 2) {
                r = p[0];
                g = p[step];
                b = p[step * 2];
            } else if (color_type == 3) {
                size_t idx = p[0];
                if (idx * 3 + 2 < plte.size()) {
                    r = plte[idx * 3];
                    g = plte[idx * 3 + 1];
                    b = plte[idx * 3 + 2];
                }
                if (idx < trns.size()) a = trns[idx];
            } else if (color_type == 4) {
                r = g = b = p[0];
                a = p[step];
            } else if (color_type == 6) {
                r = p[0];
                g = p[step];
                b = p[step * 2];
                a = p[step * 3];
            }
            dst[x * 4 + 0] = r;
            dst[x * 4 + 1] = g;
            dst[x * 4 + 2] = b;
            dst[x * 4 + 3] = a;
        }
    }
    return true;
}

bool load_png_rgba_file(const std::filesystem::path& path, int& out_w, int& out_h, Bytes& out_rgba, std::string* err) {
    Bytes file_bytes;
    if (!read_bundle_asset_file(path, file_bytes)) {
        if (err) *err = "Could not open PNG file";
        return false;
    }
    return decode_png_rgba(file_bytes, out_w, out_h, out_rgba, err);
}

bool save_rgba_png_file(const std::filesystem::path& path, int w, int h, const u8* rgba, bool flip_y, bool strip_alpha) {
    if (w <= 0 || h <= 0 || !rgba) return false;
    size_t ch = strip_alpha ? 3u : 4u;
    size_t row_bytes = static_cast<size_t>(w) * ch;
    Bytes raw(static_cast<size_t>(h) * (row_bytes + 1u));

    for (int y = 0; y < h; ++y) {
        int src_y = flip_y ? (h - 1 - y) : y;
        const u8* srow = rgba + static_cast<size_t>(src_y) * static_cast<size_t>(w) * 4u;
        u8* drow = raw.data() + static_cast<size_t>(y) * (row_bytes + 1u);
        drow[0] = 0;  // filter None
        u8* dpx = drow + 1;
        if (strip_alpha) {
            for (int x = 0; x < w; ++x) {
                dpx[x * 3 + 0] = srow[x * 4 + 0];
                dpx[x * 3 + 1] = srow[x * 4 + 1];
                dpx[x * 3 + 2] = srow[x * 4 + 2];
            }
        } else {
            std::memcpy(dpx, srow, row_bytes);
        }
    }

    uLongf comp_cap = compressBound(static_cast<uLong>(raw.size()));
    Bytes comp(comp_cap);
    if (compress2(comp.data(), &comp_cap, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK) {
        return false;
    }
    comp.resize(comp_cap);

    Bytes png;
    png.reserve(comp.size() + 64);
    static constexpr u8 kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    png.insert(png.end(), kSig, kSig + 8);

    Bytes ihdr;
    write_u32_be(ihdr, static_cast<u32>(w));
    write_u32_be(ihdr, static_cast<u32>(h));
    ihdr.push_back(8);                  // bit depth
    ihdr.push_back(strip_alpha ? 2 : 6);  // color type RGB or RGBA
    ihdr.push_back(0);                  // compression
    ihdr.push_back(0);                  // filter
    ihdr.push_back(0);                  // interlace
    append_png_chunk(png, "IHDR", ihdr.data(), ihdr.size());
    append_png_chunk(png, "IDAT", comp.data(), comp.size());
    append_png_chunk(png, "IEND", nullptr, 0);

    return write_file_bytes(path, png.data(), png.size());
}

}  // namespace hl3

