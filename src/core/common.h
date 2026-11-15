#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hl3 {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using Bytes = std::vector<u8>;

inline constexpr const char* kPortVersion = "1.0.6";
inline constexpr const char* kOriginalVersion = "1.0.1";
inline constexpr const char* kPortAuthor = "David Kalil Braga (2026)";
inline constexpr const char* kGithubRepo = "davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia";
inline constexpr const char* kAppTitle = "Heroes Lore 3: Stigmata of Gaia";
inline constexpr const char* kBundleId = "com.ea.heroesloresg.bv";
inline constexpr const char* kAndroidPackageId = "com.davidbraga.heroeslore3";
inline constexpr const char* kSwitchTitleId = "0100F3A300030000";
inline constexpr const char* kAppPrefix = "Payload/Heroes Lore.app/";

inline constexpr const char* kExpectedGameIpaSha256 =
    "56dd27225077df0afa61082d5e40590a7dea730c16d47ed6b7ea93f9725558f3";
inline constexpr const char* kExpectedExecutableSha256 =
    "39c6ed1db25cbf6020c9edd2cfa145410ff391f3361a9150d9646b9a4fada849";

inline u16 read_u16_le(const u8* p) {
    return u16(p[0]) | (u16(p[1]) << 8);
}

inline i32 read_s16_le(const u8* p) {
    u16 v = read_u16_le(p);
    return (v & 0x8000u) ? i32(u32(v) | 0xFFFF0000u) : i32(v);
}

inline u32 read_u32_le(const u8* p) {
    return u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24);
}

inline i32 read_s32_le(const u8* p) {
    return static_cast<i32>(read_u32_le(p));
}

inline u64 read_u64_le(const u8* p) {
    return u64(read_u32_le(p)) | (u64(read_u32_le(p + 4)) << 32);
}

inline void write_u16_le(u8* p, u16 v) {
    p[0] = u8(v & 0xFFu);
    p[1] = u8((v >> 8) & 0xFFu);
}

inline void write_u32_le(u8* p, u32 v) {
    p[0] = u8(v & 0xFFu);
    p[1] = u8((v >> 8) & 0xFFu);
    p[2] = u8((v >> 16) & 0xFFu);
    p[3] = u8((v >> 24) & 0xFFu);
}

inline void write_u64_le(u8* p, u64 v) {
    write_u32_le(p, u32(v & 0xFFFFFFFFu));
    write_u32_le(p + 4, u32((v >> 32) & 0xFFFFFFFFu));
}

inline u32 f32_to_bits(float f) {
    u32 u = 0;
    static_assert(sizeof(float) == sizeof(u32), "float must be 32-bit");
    __builtin_memcpy(&u, &f, 4);
    return u;
}

inline float bits_to_f32(u32 u) {
    float f = 0.0f;
    __builtin_memcpy(&f, &u, 4);
    return f;
}

inline u64 f64_to_bits(double d) {
    u64 u = 0;
    static_assert(sizeof(double) == sizeof(u64), "double must be 64-bit");
    __builtin_memcpy(&u, &d, 8);
    return u;
}

inline double bits_to_f64(u64 u) {
    double d = 0.0;
    __builtin_memcpy(&d, &u, 8);
    return d;
}

// Portable SHA-256 (FIPS 180-4) adapted cleanly for C++17.
inline std::array<u8, 32> sha256_bytes(const u8* data, size_t len) {
    static constexpr u32 k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };
    auto rotr = [](u32 x, int n) -> u32 { return (x >> n) | (x << (32 - n)); };
    u32 state[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    u64 bits = u64(len) * 8u;
    size_t total = (len + 9u + 63u) & ~size_t(63);
    for (size_t pos = 0; pos < total; pos += 64) {
        u8 block[64]{};
        for (size_t i = 0; i < 64; ++i) {
            size_t at = pos + i;
            if (at < len) {
                block[i] = data[at];
            } else if (at == len) {
                block[i] = 0x80u;
            } else if (at >= total - 8) {
                block[i] = u8(bits >> ((total - 1 - at) * 8));
            }
        }
        u32 w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (u32(block[i * 4]) << 24) | (u32(block[i * 4 + 1]) << 16) |
                   (u32(block[i * 4 + 2]) << 8) | u32(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            u32 a = w[i - 15];
            u32 b = w[i - 2];
            w[i] = w[i - 16] + (rotr(a, 7) ^ rotr(a, 18) ^ (a >> 3)) + w[i - 7] +
                   (rotr(b, 17) ^ rotr(b, 19) ^ (b >> 10));
        }
        u32 a = state[0], b = state[1], c = state[2], d = state[3];
        u32 e = state[4], f = state[5], g = state[6], h = state[7];
        for (int i = 0; i < 64; ++i) {
            u32 t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            u32 t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }
    std::array<u8, 32> out{};
    for (int i = 0; i < 32; ++i) {
        out[i] = u8(state[i / 4] >> (24 - 8 * (i % 4)));
    }
    return out;
}

inline std::string sha256_hex(const u8* data, size_t len) {
    static constexpr char kHex[] = "0123456789abcdef";
    auto digest = sha256_bytes(data, len);
    std::string out(64, '0');
    for (size_t i = 0; i < 32; ++i) {
        out[i * 2] = kHex[(digest[i] >> 4) & 0xFu];
        out[i * 2 + 1] = kHex[digest[i] & 0xFu];
    }
    return out;
}

inline std::string sha256_hex(const Bytes& bytes) {
    return sha256_hex(bytes.data(), bytes.size());
}

inline bool read_file_bytes(const std::filesystem::path& path, Bytes& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto sz = f.tellg();
    if (sz < 0) return false;
    out.resize(static_cast<size_t>(sz));
    f.seekg(0, std::ios::beg);
    if (sz > 0 && !f.read(reinterpret_cast<char*>(out.data()), sz)) {
        out.clear();
        return false;
    }
    return true;
}

inline bool write_file_bytes(const std::filesystem::path& path, const void* data, size_t len) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    if (len > 0) {
        f.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
    }
    return f.good();
}

inline std::string trim_ascii(std::string_view sv) {
    size_t b = 0;
    while (b < sv.size() && (sv[b] == ' ' || sv[b] == '\t' || sv[b] == '\r' || sv[b] == '\n')) ++b;
    size_t e = sv.size();
    while (e > b && (sv[e - 1] == ' ' || sv[e - 1] == '\t' || sv[e - 1] == '\r' || sv[e - 1] == '\n')) --e;
    return std::string(sv.substr(b, e - b));
}

inline std::string hex32(u32 v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08x", v);
    return std::string(buf);
}

}  // namespace hl3
