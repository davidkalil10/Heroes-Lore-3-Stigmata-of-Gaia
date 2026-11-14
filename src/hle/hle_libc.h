#pragma once

#include "../core/runtime.h"

#include <string>

namespace hl3 {

class GuestArgReader {
public:
    GuestArgReader(Machine& m, u32 ptr_or_first, bool from_va_list)
        : m_(m), from_va_list_(from_va_list), va_ptr_(ptr_or_first), first_word_(ptr_or_first) {}

    u32 next_word() {
        if (from_va_list_) {
            u32 v = m_.rd32(va_ptr_);
            va_ptr_ += 4u;
            return v;
        }
        return m_.arg(static_cast<int>(first_word_ + idx_++));
    }

private:
    Machine& m_;
    bool from_va_list_;
    u32 va_ptr_;
    u32 first_word_;
    u32 idx_ = 0;
};

std::string format_guest_c(Runtime& rt, const std::string& fmt, GuestArgReader& rd, bool with_objc = false);
u32 new_guest_std_string(Machine& m, const Bytes& data);

}  // namespace hl3

