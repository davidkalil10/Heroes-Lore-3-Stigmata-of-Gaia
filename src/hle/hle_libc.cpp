#include "hle_libc.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>

namespace hl3 {

namespace {

inline double read_dbl_pair(u32 lo, u32 hi) {
    return bits_to_f64(u64(lo) | (u64(hi) << 32));
}

inline TrapResult ret_dbl(double v) {
    u64 bits = f64_to_bits(v);
    return TrapResult::pair(u32(bits & 0xFFFFFFFFu), u32((bits >> 32) & 0xFFFFFFFFu));
}

inline TrapResult ret_flt(double v) {
    float fv = static_cast<float>(v);
    return TrapResult::val(f32_to_bits(fv));
}

inline i64 read_s64_pair(u32 lo, u32 hi) {
    return static_cast<i64>(u64(lo) | (u64(hi) << 32));
}

inline u32 clamp_double_to_i32_bits(double v) {
    if (std::isnan(v)) return 0u;
    if (v >= 2147483647.0) return 0x7FFFFFFFu;
    if (v <= -2147483648.0) return 0x80000000u;
    return static_cast<u32>(static_cast<i32>(v));
}

i64 trunc_div64(i64 a, i64 b) {
    return a / b;
}

std::string describe_objc_arg(Runtime& rt, u32 obj) {
    if (obj == 0) return "(null)";
    std::string s;
    if (rt.objc().try_get_str(obj, s)) return s;
    auto [cls, is_cls] = rt.objc().class_of(obj);
    std::string cname = cls ? cls->name : "id";
    return "<" + cname + " " + hex32(obj) + ">";
}

void flush_virtual_file(VirtualFile& f) {
    if (f.has_host_path && f.dirty) {
        write_file_bytes(f.host_path, f.data.data(), f.data.size());
        f.dirty = false;
    }
}

}  // namespace

std::string format_guest_c(Runtime& rt, const std::string& fmt, GuestArgReader& rd, bool with_objc) {
    Machine& m = rt.machine();
    std::string out;
    size_t i = 0;
    while (i < fmt.size()) {
        if (fmt[i] != '%') {
            out.push_back(fmt[i++]);
            continue;
        }
        size_t j = i + 1;
        std::string flags;
        while (j < fmt.size() && std::string_view("-+ #0").find(fmt[j]) != std::string_view::npos) {
            flags.push_back(fmt[j++]);
        }
        std::string width;
        if (j < fmt.size() && fmt[j] == '*') {
            width = std::to_string(static_cast<i32>(rd.next_word()));
            ++j;
        }
        while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j]))) {
            width.push_back(fmt[j++]);
        }
        bool has_prec = false;
        std::string prec;
        if (j < fmt.size() && fmt[j] == '.') {
            has_prec = true;
            ++j;
            if (j < fmt.size() && fmt[j] == '*') {
                prec = std::to_string(static_cast<i32>(rd.next_word()));
                ++j;
            }
            while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j]))) {
                prec.push_back(fmt[j++]);
            }
            if (prec.empty()) prec = "0";
        }
        std::string length;
        while (j < fmt.size() && std::string_view("hlqLzjt").find(fmt[j]) != std::string_view::npos) {
            length.push_back(fmt[j++]);
        }
        if (j >= fmt.size()) break;
        char conv = fmt[j];
        std::string spec = "%" + flags + width + (has_prec ? ("." + prec) : "");

        char buf[1024];
        if (conv == '%') {
            out.push_back('%');
        } else if (conv == 'd' || conv == 'i') {
            if (length == "ll" || length == "q") {
                u32 lo = rd.next_word(), hi = rd.next_word();
                i64 v = read_s64_pair(lo, hi);
                std::string f = spec + "lld";
                std::snprintf(buf, sizeof(buf), f.c_str(), static_cast<long long>(v));
                out += buf;
            } else {
                i32 v = static_cast<i32>(rd.next_word());
                if (length == "h") v = static_cast<i16>(v & 0xFFFF);
                std::string f = spec + "d";
                std::snprintf(buf, sizeof(buf), f.c_str(), static_cast<int>(v));
                out += buf;
            }
        } else if (conv == 'u' || conv == 'x' || conv == 'X' || conv == 'o') {
            if (length == "ll" || length == "q") {
                u32 lo = rd.next_word(), hi = rd.next_word();
                u64 v = u64(lo) | (u64(hi) << 32);
                std::string f = spec + "ll" + conv;
                std::snprintf(buf, sizeof(buf), f.c_str(), static_cast<unsigned long long>(v));
                out += buf;
            } else {
                u32 v = rd.next_word();
                if (length == "h") v &= 0xFFFFu;
                std::string f = spec + conv;
                std::snprintf(buf, sizeof(buf), f.c_str(), static_cast<unsigned int>(v));
                out += buf;
            }
        } else if (conv == 'f' || conv == 'F' || conv == 'e' || conv == 'E' || conv == 'g' || conv == 'G') {
            u32 lo = rd.next_word(), hi = rd.next_word();
            double v = read_dbl_pair(lo, hi);
            std::string f = spec + conv;
            std::snprintf(buf, sizeof(buf), f.c_str(), v);
            out += buf;
        } else if (conv == 'c') {
            char ch = static_cast<char>(rd.next_word() & 0xFFu);
            std::string f = spec + "c";
            std::snprintf(buf, sizeof(buf), f.c_str(), ch);
            out += buf;
        } else if (conv == 's') {
            u32 ptr = rd.next_word();
            std::string s = ptr ? m.cstr(ptr) : "(null)";
            std::string f = spec + "s";
            int need = std::snprintf(nullptr, 0, f.c_str(), s.c_str());
            if (need > 0 && size_t(need) < sizeof(buf)) {
                std::snprintf(buf, sizeof(buf), f.c_str(), s.c_str());
                out += buf;
            } else if (need > 0) {
                std::string dyn(static_cast<size_t>(need) + 1u, '\0');
                std::snprintf(dyn.data(), dyn.size(), f.c_str(), s.c_str());
                out.append(dyn.data(), static_cast<size_t>(need));
            }
        } else if (conv == 'p') {
            std::snprintf(buf, sizeof(buf), "0x%x", static_cast<unsigned int>(rd.next_word()));
            out += buf;
        } else if (conv == '@') {
            u32 obj = rd.next_word();
            std::string s = with_objc ? describe_objc_arg(rt, obj) : "<obj>";
            std::string f = spec + "s";
            int need = std::snprintf(nullptr, 0, f.c_str(), s.c_str());
            if (need > 0 && size_t(need) < sizeof(buf)) {
                std::snprintf(buf, sizeof(buf), f.c_str(), s.c_str());
                out += buf;
            } else {
                out += s;
            }
        } else {
            out.append(fmt, i, j - i + 1);
        }
        i = j + 1;
    }
    return out;
}

u32 new_guest_std_string(Machine& m, const Bytes& data) {
    constexpr u32 kStringRep = 12u;
    u32 rep = m.malloc_bytes(kStringRep + static_cast<u32>(data.size()) + 1u);
    m.wr32(rep, static_cast<u32>(data.size()));
    m.wr32(rep + 4u, static_cast<u32>(data.size()));
    m.wr32(rep + 8u, 0u);
    Bytes nul = data;
    nul.push_back(0);
    m.wrbytes(rep + kStringRep, nul);
    return rep + kStringRep;
}

void register_hle_libc(Runtime& rt, HleRegistry& reg) {
    // ---- VFP soft-float arithmetic ---------------------------------------------------
    reg.add_func("___adddf3vfp", [](Machine& m) {
        return ret_dbl(read_dbl_pair(m.reg(0), m.reg(1)) + read_dbl_pair(m.reg(2), m.reg(3)));
    });
    reg.add_func("___subdf3vfp", [](Machine& m) {
        return ret_dbl(read_dbl_pair(m.reg(0), m.reg(1)) - read_dbl_pair(m.reg(2), m.reg(3)));
    });
    reg.add_func("___muldf3vfp", [](Machine& m) {
        return ret_dbl(read_dbl_pair(m.reg(0), m.reg(1)) * read_dbl_pair(m.reg(2), m.reg(3)));
    });
    reg.add_func("___divdf3vfp", [](Machine& m) {
        return ret_dbl(read_dbl_pair(m.reg(0), m.reg(1)) / read_dbl_pair(m.reg(2), m.reg(3)));
    });

    reg.add_func("___addsf3vfp", [](Machine& m) {
        return ret_flt(bits_to_f32(m.reg(0)) + bits_to_f32(m.reg(1)));
    });
    reg.add_func("___subsf3vfp", [](Machine& m) {
        return ret_flt(bits_to_f32(m.reg(0)) - bits_to_f32(m.reg(1)));
    });
    reg.add_func("___mulsf3vfp", [](Machine& m) {
        return ret_flt(bits_to_f32(m.reg(0)) * bits_to_f32(m.reg(1)));
    });
    reg.add_func("___divsf3vfp", [](Machine& m) {
        return ret_flt(bits_to_f32(m.reg(0)) / bits_to_f32(m.reg(1)));
    });

    auto reg_cmp = [&](const char* name, i32 nan_val, auto pred) {
        reg.add_func(std::string("___") + name + "df2vfp", [nan_val, pred](Machine& m) {
            double a = read_dbl_pair(m.reg(0), m.reg(1));
            double b = read_dbl_pair(m.reg(2), m.reg(3));
            i32 r = (std::isnan(a) || std::isnan(b)) ? nan_val : pred(a, b);
            return TrapResult::val(static_cast<u32>(r));
        });
        reg.add_func(std::string("___") + name + "sf2vfp", [nan_val, pred](Machine& m) {
            float a = bits_to_f32(m.reg(0));
            float b = bits_to_f32(m.reg(1));
            i32 r = (std::isnan(a) || std::isnan(b)) ? nan_val : pred(static_cast<double>(a), static_cast<double>(b));
            return TrapResult::val(static_cast<u32>(r));
        });
    };

    reg_cmp("eq", 1, [](double a, double b) -> i32 { return (a == b) ? 0 : 1; });
    reg_cmp("ne", 1, [](double a, double b) -> i32 { return (a != b) ? 0 : 1; });
    reg_cmp("ge", -1, [](double a, double b) -> i32 { return (a >= b) ? 1 : -1; });
    reg_cmp("gt", -1, [](double a, double b) -> i32 { return (a > b) ? 1 : -1; });
    reg_cmp("le", 1, [](double a, double b) -> i32 { return (a <= b) ? -1 : 1; });
    reg_cmp("lt", 1, [](double a, double b) -> i32 { return (a < b) ? -1 : 1; });

    reg.add_func("___unorddf2vfp", [](Machine& m) {
        double a = read_dbl_pair(m.reg(0), m.reg(1));
        double b = read_dbl_pair(m.reg(2), m.reg(3));
        return TrapResult::val((std::isnan(a) || std::isnan(b)) ? 1u : 0u);
    });
    reg.add_func("___unordsf2vfp", [](Machine& m) {
        float a = bits_to_f32(m.reg(0));
        float b = bits_to_f32(m.reg(1));
        return TrapResult::val((std::isnan(a) || std::isnan(b)) ? 1u : 0u);
    });

    reg.add_func("___fixdfsivfp", [](Machine& m) {
        return TrapResult::val(clamp_double_to_i32_bits(read_dbl_pair(m.reg(0), m.reg(1))));
    });
    reg.add_func("___fixsfsivfp", [](Machine& m) {
        return TrapResult::val(clamp_double_to_i32_bits(bits_to_f32(m.reg(0))));
    });
    reg.add_func("___floatsidfvfp", [](Machine& m) {
        return ret_dbl(static_cast<double>(static_cast<i32>(m.reg(0))));
    });
    reg.add_func("___floatsisfvfp", [](Machine& m) {
        return ret_flt(static_cast<double>(static_cast<i32>(m.reg(0))));
    });
    reg.add_func("___floatunssidfvfp", [](Machine& m) {
        return ret_dbl(static_cast<double>(m.reg(0)));
    });
    reg.add_func("___extendsfdf2vfp", [](Machine& m) {
        return ret_dbl(static_cast<double>(bits_to_f32(m.reg(0))));
    });
    reg.add_func("___truncdfsf2vfp", [](Machine& m) {
        return ret_flt(read_dbl_pair(m.reg(0), m.reg(1)));
    });

    // ---- integer division ------------------------------------------------------------
    reg.add_func("___divsi3", [](Machine& m) {
        i32 a = static_cast<i32>(m.reg(0));
        i32 b = static_cast<i32>(m.reg(1));
        return TrapResult::val(b == 0 ? 0u : static_cast<u32>(a / b));
    });
    reg.add_func("___modsi3", [](Machine& m) {
        i32 a = static_cast<i32>(m.reg(0));
        i32 b = static_cast<i32>(m.reg(1));
        return TrapResult::val(b == 0 ? 0u : static_cast<u32>(a % b));
    });
    reg.add_func("___umodsi3", [](Machine& m) {
        u32 a = m.reg(0), b = m.reg(1);
        return TrapResult::val(b == 0 ? 0u : (a % b));
    });
    reg.add_func("___divdi3", [](Machine& m) {
        i64 a = read_s64_pair(m.reg(0), m.reg(1));
        i64 b = read_s64_pair(m.reg(2), m.reg(3));
        u64 r = (b == 0) ? 0u : static_cast<u64>(trunc_div64(a, b));
        return TrapResult::pair(u32(r & 0xFFFFFFFFu), u32(r >> 32));
    });
    reg.add_func("___moddi3", [](Machine& m) {
        i64 a = read_s64_pair(m.reg(0), m.reg(1));
        i64 b = read_s64_pair(m.reg(2), m.reg(3));
        u64 r = (b == 0) ? 0u : static_cast<u64>(a % b);
        return TrapResult::pair(u32(r & 0xFFFFFFFFu), u32(r >> 32));
    });
    reg.add_func("_ceilf", [](Machine& m) {
        return ret_flt(std::ceil(bits_to_f32(m.reg(0))));
    });

    // ---- memory / string -------------------------------------------------------------
    reg.add_func("_malloc", [](Machine& m) { return TrapResult::val(m.malloc_bytes(m.reg(0))); });
    reg.add_func("_calloc", [](Machine& m) { return TrapResult::val(m.calloc_bytes(m.reg(0), m.reg(1))); });
    reg.add_func("_realloc", [](Machine& m) { return TrapResult::val(m.realloc_bytes(m.reg(0), m.reg(1))); });
    reg.add_func("_free", [](Machine&) { return TrapResult::val(0u); });

    reg.add_func("_memcpy|_memmove", [](Machine& m) {
        u32 dst = m.reg(0), src = m.reg(1), n = m.reg(2);
        if (n > 0) m.wrbytes(dst, m.rdbytes(src, n));
        return TrapResult::val(dst);
    });
    reg.add_func("_memset", [](Machine& m) {
        u32 dst = m.reg(0);
        u8 c = static_cast<u8>(m.reg(1) & 0xFFu);
        u32 n = m.reg(2);
        if (n > 0) {
            Bytes b(n, c);
            m.wrbytes(dst, b);
        }
        return TrapResult::val(dst);
    });
    reg.add_func("_memcmp", [](Machine& m) {
        Bytes a = m.rdbytes(m.reg(0), m.reg(2));
        Bytes b = m.rdbytes(m.reg(1), m.reg(2));
        int cmp = (a == b) ? 0 : (a < b ? -1 : 1);
        return TrapResult::val(static_cast<u32>(cmp));
    });

    reg.add_func("_strlen", [](Machine& m) {
        return TrapResult::val(static_cast<u32>(m.cstr(m.reg(0)).size()));
    });
    reg.add_func("_strcmp", [](Machine& m) {
        std::string a = m.cstr(m.reg(0));
        std::string b = m.cstr(m.reg(1));
        int cmp = (a == b) ? 0 : (a < b ? -1 : 1);
        return TrapResult::val(static_cast<u32>(cmp));
    });
    reg.add_func("_strncmp", [](Machine& m) {
        u32 n = m.reg(2);
        std::string a = m.cstr(m.reg(0)).substr(0, n);
        std::string b = m.cstr(m.reg(1)).substr(0, n);
        int cmp = (a == b) ? 0 : (a < b ? -1 : 1);
        return TrapResult::val(static_cast<u32>(cmp));
    });
    reg.add_func("_strcpy", [](Machine& m) {
        std::string s = m.cstr(m.reg(1));
        Bytes b(s.begin(), s.end());
        b.push_back(0);
        m.wrbytes(m.reg(0), b);
        return TrapResult::val(m.reg(0));
    });
    reg.add_func("_strncpy", [](Machine& m) {
        u32 n = m.reg(2);
        std::string s = m.cstr(m.reg(1));
        if (s.size() > n) s.resize(n);
        Bytes b(n, 0);
        std::memcpy(b.data(), s.data(), s.size());
        m.wrbytes(m.reg(0), b);
        return TrapResult::val(m.reg(0));
    });
    reg.add_func("_strcat", [](Machine& m) {
        std::string d = m.cstr(m.reg(0));
        std::string s = m.cstr(m.reg(1));
        Bytes b(s.begin(), s.end());
        b.push_back(0);
        m.wrbytes(m.reg(0) + static_cast<u32>(d.size()), b);
        return TrapResult::val(m.reg(0));
    });
    reg.add_func("_strchr", [](Machine& m) {
        std::string s = m.cstr(m.reg(0));
        u8 c = static_cast<u8>(m.reg(1) & 0xFFu);
        if (c == 0) return TrapResult::val(m.reg(0) + static_cast<u32>(s.size()));
        size_t pos = s.find(static_cast<char>(c));
        return TrapResult::val(pos == std::string::npos ? 0u : (m.reg(0) + static_cast<u32>(pos)));
    });
    reg.add_func("_strrchr", [](Machine& m) {
        std::string s = m.cstr(m.reg(0));
        u8 c = static_cast<u8>(m.reg(1) & 0xFFu);
        if (c == 0) return TrapResult::val(m.reg(0) + static_cast<u32>(s.size()));
        size_t pos = s.rfind(static_cast<char>(c));
        return TrapResult::val(pos == std::string::npos ? 0u : (m.reg(0) + static_cast<u32>(pos)));
    });
    reg.add_func("_strstr", [](Machine& m) {
        std::string s = m.cstr(m.reg(0));
        std::string t = m.cstr(m.reg(1));
        size_t pos = s.find(t);
        return TrapResult::val(pos == std::string::npos ? 0u : (m.reg(0) + static_cast<u32>(pos)));
    });
    reg.add_func("_strcspn", [](Machine& m) {
        std::string s = m.cstr(m.reg(0));
        std::string rej = m.cstr(m.reg(1));
        size_t pos = s.find_first_of(rej);
        return TrapResult::val(static_cast<u32>(pos == std::string::npos ? s.size() : pos));
    });
    reg.add_func("_strdup", [](Machine& m) {
        return TrapResult::val(m.new_cstr(m.cstr(m.reg(0))));
    });
    reg.add_func("_atoi", [](Machine& m) {
        std::string s = m.cstr(m.reg(0));
        return TrapResult::val(static_cast<u32>(std::atoi(s.c_str())));
    });
    reg.add_func("_rand|_arc4random", [](Machine&) {
        static u32 state = 12345u;
        state = (state * 1103515245u + 12345u) & 0x7FFFFFFFu;
        return TrapResult::val(state);
    });
    reg.add_func("___error", [](Machine& m) {
        static u32 errno_addr = 0;
        if (errno_addr == 0) errno_addr = m.malloc_bytes(16);
        return TrapResult::val(errno_addr);
    });
    reg.add_func("___assert_rtn", [](Machine& m) -> TrapResult {
        throw GuestFault("assert failed: " + m.cstr(m.reg(0)) + " " + m.cstr(m.reg(1)) + ":" +
                         std::to_string(m.reg(2)) + " " + m.cstr(m.reg(3)));
    });
    reg.add_func("___cxa_atexit|___cxa_pure_virtual|___cxa_rethrow", [](Machine&) {
        return TrapResult::val(0u);
    });
    reg.add_func("___maskrune", [](Machine& m) {
        u32 c = m.reg(0), mask = m.reg(1);
        u32 flags = 0;
        if (c < 256u) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalpha(uc)) flags |= 0x100u;
            if (std::isdigit(uc)) flags |= 0x400u;
            if (std::isprint(uc) && uc != ' ') flags |= 0x800u;
            if (std::islower(uc)) flags |= 0x1000u;
            if (std::isprint(uc) && !std::isalnum(uc) && uc != ' ') flags |= 0x2000u;
            if (std::isspace(uc)) flags |= 0x4000u;
            if (std::isupper(uc)) flags |= 0x8000u;
            if (std::isxdigit(uc)) flags |= 0x10000u;
            if (uc == ' ') flags |= 0x20000u;
            if (std::isprint(uc)) flags |= 0x40000u;
        }
        return TrapResult::val((flags & mask) ? 1u : 0u);
    });
    reg.add_func("_puts", [](Machine& m) {
        m.log("puts: " + m.cstr(m.reg(0)));
        return TrapResult::val(1u);
    });
    reg.add_func("_perror", [](Machine& m) {
        m.log("perror: " + m.cstr(m.reg(0)));
        return TrapResult::val(0u);
    });
    reg.add_func("__ZNSsC1EPKcRKSaIcE", [](Machine& m) {
        std::string s = m.cstr(m.reg(1));
        Bytes b(s.begin(), s.end());
        m.wr32(m.reg(0), new_guest_std_string(m, b));
        return TrapResult::val(m.reg(0));
    });
    reg.add_func("__ZNSsD2Ev", [](Machine&) { return TrapResult::val(0u); });

    // Runtime-dependent libc traps (stdio, SJLJ, formatted printf, exit)
    reg.add_func("_sprintf", [&rt](Machine& m) {
        GuestArgReader rd(m, 2, false);
        std::string s = format_guest_c(rt, m.cstr(m.reg(1)), rd, false);
        Bytes b(s.begin(), s.end());
        b.push_back(0);
        m.wrbytes(m.reg(0), b);
        return TrapResult::val(static_cast<u32>(s.size()));
    });
    reg.add_func("_vsprintf", [&rt](Machine& m) {
        GuestArgReader rd(m, m.reg(2), true);
        std::string s = format_guest_c(rt, m.cstr(m.reg(1)), rd, false);
        Bytes b(s.begin(), s.end());
        b.push_back(0);
        m.wrbytes(m.reg(0), b);
        return TrapResult::val(static_cast<u32>(s.size()));
    });
    reg.add_func("_vsnprintf", [&rt](Machine& m) {
        GuestArgReader rd(m, m.reg(3), true);
        std::string s = format_guest_c(rt, m.cstr(m.reg(2)), rd, false);
        u32 n = m.reg(1);
        if (n > 0) {
            size_t copy_len = std::min<size_t>(s.size(), n - 1u);
            Bytes b(s.begin(), s.begin() + copy_len);
            b.push_back(0);
            m.wrbytes(m.reg(0), b);
        }
        return TrapResult::val(static_cast<u32>(s.size()));
    });
    reg.add_func("_printf", [&rt](Machine& m) {
        GuestArgReader rd(m, 1, false);
        std::string s = format_guest_c(rt, m.cstr(m.reg(0)), rd, false);
        m.log("printf: " + s);
        return TrapResult::val(static_cast<u32>(s.size()));
    });

    reg.add_func("_exit", [&rt](Machine& m) -> TrapResult {
        rt.exit_code = static_cast<int>(m.reg(0));
        rt.exited = true;
        throw GuestFault("exit(" + std::to_string(rt.exit_code) + ")");
    });

    reg.add_func("__Unwind_SjLj_Register", [&rt](Machine& m) {
        u32 fc = m.reg(0);
        m.wr32(fc, rt.sjlj_top);
        rt.sjlj_top = fc;
        return TrapResult::val(0u);
    });
    reg.add_func("__Unwind_SjLj_Unregister", [&rt](Machine& m) {
        rt.sjlj_top = m.rd32(m.reg(0));
        return TrapResult::val(0u);
    });
    reg.add_func("__Unwind_SjLj_Resume|___gxx_personality_sj0|___objc_personality_v0", [](Machine&) -> TrapResult {
        throw GuestFault("C++/ObjC exception unwinding is not implemented");
    });

    reg.add_func("_fopen", [&rt](Machine& m) {
        std::string path = m.cstr(m.reg(0));
        std::string mode = m.cstr(m.reg(1));
        auto r = rt.resolve_path(path);
        m.log("fopen(" + path + ", " + mode + ")");
        if (r.kind == Runtime::ResolvedPath::Kind::None) return TrapResult::val(0u);

        auto vf = std::make_unique<VirtualFile>();
        vf->path = path;
        vf->mode = mode;
        const bool is_write = mode.find('w') != std::string::npos;
        const bool is_append = mode.find('a') != std::string::npos;
        if (r.kind == Runtime::ResolvedPath::Kind::Bundle) {
            if (is_write || is_append || !rt.bundle().read_asset(r.bundle_rel, vf->data)) {
                return TrapResult::val(0u);
            }
        } else {
            vf->has_host_path = true;
            vf->host_path = r.host_path;
            if (!is_write) {
                std::error_code ec;
                if (std::filesystem::exists(r.host_path, ec)) {
                    read_file_bytes(r.host_path, vf->data);
                } else if (!is_append) {
                    return TrapResult::val(0u);
                }
            }
        }
        if (is_append) vf->pos = vf->data.size();
        if (is_write) vf->dirty = true;
        u32 handle = m.malloc_bytes(16);
        rt.files[handle] = std::move(vf);
        return TrapResult::val(handle);
    });
    reg.add_func("_fclose", [&rt](Machine& m) {
        auto it = rt.files.find(m.reg(0));
        if (it == rt.files.end()) return TrapResult::val(0xFFFFFFFFu);
        flush_virtual_file(*it->second);
        rt.files.erase(it);
        return TrapResult::val(0u);
    });
    reg.add_func("_fflush", [&rt](Machine& m) {
        auto it = rt.files.find(m.reg(0));
        if (it != rt.files.end()) flush_virtual_file(*it->second);
        return TrapResult::val(0u);
    });
    reg.add_func("_fread", [&rt](Machine& m) {
        u32 buf = m.reg(0), size = m.reg(1), count = m.reg(2), h = m.reg(3);
        auto it = rt.files.find(h);
        if (it == rt.files.end() || size == 0) return TrapResult::val(0u);
        auto& f = *it->second;
        size_t want = static_cast<size_t>(size) * count;
        size_t avail = (f.pos < f.data.size()) ? (f.data.size() - f.pos) : 0u;
        size_t take = std::min(want, avail);
        if (take > 0) {
            m.wrbytes(buf, f.data.data() + f.pos, take);
            f.pos += take;
        }
        return TrapResult::val(static_cast<u32>(take / size));
    });
    reg.add_func("_fwrite", [&rt](Machine& m) {
        u32 buf = m.reg(0), size = m.reg(1), count = m.reg(2), h = m.reg(3);
        auto it = rt.files.find(h);
        if (it == rt.files.end()) return TrapResult::val(0u);
        auto& f = *it->second;
        size_t n = static_cast<size_t>(size) * count;
        Bytes chunk = m.rdbytes(buf, n);
        if (f.pos + n > f.data.size()) f.data.resize(f.pos + n, 0);
        if (n > 0) std::memcpy(f.data.data() + f.pos, chunk.data(), n);
        f.pos += n;
        f.dirty = true;
        return TrapResult::val(count);
    });
    reg.add_func("_fputs", [&rt](Machine& m) {
        std::string s = m.cstr(m.reg(0));
        auto it = rt.files.find(m.reg(1));
        if (it != rt.files.end()) {
            auto& f = *it->second;
            size_t n = s.size();
            if (f.pos + n > f.data.size()) f.data.resize(f.pos + n, 0);
            if (n > 0) std::memcpy(f.data.data() + f.pos, s.data(), n);
            f.pos += n;
            f.dirty = true;
        } else {
            m.log("stderr: " + s);
        }
        return TrapResult::val(1u);
    });
    reg.add_func("_fseek", [&rt](Machine& m) {
        auto it = rt.files.find(m.reg(0));
        if (it == rt.files.end()) return TrapResult::val(0xFFFFFFFFu);
        auto& f = *it->second;
        i64 off = static_cast<i32>(m.reg(1));
        u32 whence = m.reg(2);
        i64 base = (whence == 0) ? 0 : (whence == 1 ? static_cast<i64>(f.pos) : static_cast<i64>(f.data.size()));
        f.pos = static_cast<size_t>(std::max<i64>(0, base + off));
        return TrapResult::val(0u);
    });
    reg.add_func("_ftell", [&rt](Machine& m) {
        auto it = rt.files.find(m.reg(0));
        if (it == rt.files.end()) return TrapResult::val(0xFFFFFFFFu);
        return TrapResult::val(static_cast<u32>(it->second->pos));
    });
    reg.add_func("_remove", [&rt](Machine& m) {
        auto r = rt.resolve_path(m.cstr(m.reg(0)));
        std::error_code ec;
        if (r.kind == Runtime::ResolvedPath::Kind::Docs && std::filesystem::exists(r.host_path, ec)) {
            std::filesystem::remove(r.host_path, ec);
            return TrapResult::val(0u);
        }
        return TrapResult::val(0xFFFFFFFFu);
    });
}

}  // namespace hl3
