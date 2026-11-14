#include "macho_loader.h"

#include <cstring>
#include <map>

namespace hl3 {

namespace {

constexpr u32 kMachMagic = 0xFEEDFACEu;
constexpr u32 kLcSegment = 0x1u;
constexpr u32 kLcSymtab = 0x2u;
constexpr u32 kLcUnixThread = 0x5u;
constexpr u32 kLcDysymtab = 0xBu;
constexpr u32 kLcLoadDylib = 0xCu;
constexpr u32 kLcIdDylib = 0xDu;
constexpr u32 kLcLoadWeakDylib = 0x18u;

constexpr u8 kSectNonLazySymPtrs = 0x6u;
constexpr u8 kSectLazySymPtrs = 0x7u;
constexpr u8 kSectSymbolStubs = 0x8u;

constexpr u32 kIndirectSymbolLocal = 0x80000000u;
constexpr u32 kIndirectSymbolAbs = 0x40000000u;

struct DysymtabInfo {
    u32 ilocalsym = 0;
    u32 nlocalsym = 0;
    u32 iextdefsym = 0;
    u32 nextdefsym = 0;
    u32 iundefsym = 0;
    u32 nundefsym = 0;
    u32 indirectsymoff = 0;
    u32 nindirectsyms = 0;
    u32 extreloff = 0;
    u32 nextrel = 0;
};

std::string read_fixed_name(const u8* p, size_t max_len) {
    size_t len = 0;
    while (len < max_len && p[len] != 0) ++len;
    return std::string(reinterpret_cast<const char*>(p), len);
}

std::string strip_class_import_prefix(const std::string& sym) {
    static constexpr const char* kPrefix = "_OBJC_CLASS_$_";
    if (sym.rfind(kPrefix, 0) == 0) {
        return sym.substr(std::strlen(kPrefix));
    }
    return sym;
}

}  // namespace

bool MachOImage::vaddr_to_offset(u32 vaddr, size_t& out_off) const {
    for (const auto& seg : segments) {
        if (vaddr >= seg.vmaddr && u64(vaddr) < u64(seg.vmaddr) + seg.vmsize) {
            u32 delta = vaddr - seg.vmaddr;
            if (delta < seg.filesize && size_t(seg.fileoff) + delta <= data.size()) {
                out_off = size_t(seg.fileoff) + delta;
                return true;
            }
            return false;
        }
    }
    return false;
}

bool MachOImage::read_u32(u32 vaddr, u32& out) const {
    size_t off = 0;
    if (!vaddr_to_offset(vaddr, off) || off + 4 > data.size()) return false;
    out = read_u32_le(data.data() + off);
    return true;
}

std::string MachOImage::read_cstr(u32 vaddr) const {
    if (vaddr == 0) return {};
    size_t off = 0;
    if (!vaddr_to_offset(vaddr, off)) return {};
    size_t end = off;
    while (end < data.size() && data[end] != 0) ++end;
    return std::string(reinterpret_cast<const char*>(data.data() + off), end - off);
}

const MachSection* MachOImage::find_section(const std::string& full_name) const {
    auto it = sections.find(full_name);
    return (it == sections.end()) ? nullptr : &it->second;
}

bool parse_macho_image(Bytes macho_data, MachOImage& out, std::string* err) {
    out = MachOImage{};
    out.data = std::move(macho_data);
    const auto& d = out.data;

    auto fail = [&](const std::string& msg) {
        if (err) *err = msg;
        return false;
    };

    if (d.size() < 28) return fail("Mach-O data too small");
    u32 magic = read_u32_le(d.data());
    if (magic != kMachMagic) return fail("Invalid 32-bit Mach-O magic");

    u32 ncmds = read_u32_le(d.data() + 16);
    size_t off = 28;

    u32 symoff = 0, nsyms = 0, stroff = 0, strsize = 0;
    DysymtabInfo dysym{};
    bool has_dysym = false;

    for (u32 c = 0; c < ncmds; ++c) {
        if (off + 8 > d.size()) return fail("Truncated Mach-O load command");
        u32 cmd = read_u32_le(d.data() + off);
        u32 cmdsize = read_u32_le(d.data() + off + 4);
        if (cmdsize < 8 || off + cmdsize > d.size()) return fail("Invalid Mach-O cmdsize");

        if (cmd == kLcSegment && cmdsize >= 56) {
            MachSegment seg;
            seg.segname = read_fixed_name(d.data() + off + 8, 16);
            seg.vmaddr = read_u32_le(d.data() + off + 24);
            seg.vmsize = read_u32_le(d.data() + off + 28);
            seg.fileoff = read_u32_le(d.data() + off + 32);
            seg.filesize = read_u32_le(d.data() + off + 36);
            seg.maxprot = read_u32_le(d.data() + off + 40);
            seg.initprot = read_u32_le(d.data() + off + 44);
            u32 nsects = read_u32_le(d.data() + off + 48);
            seg.flags = read_u32_le(d.data() + off + 52);

            size_t sec_off = off + 56;
            for (u32 s = 0; s < nsects; ++s) {
                if (sec_off + 68 > off + cmdsize) return fail("Truncated Mach-O section");
                MachSection sec;
                sec.sectname = read_fixed_name(d.data() + sec_off, 16);
                sec.segname = read_fixed_name(d.data() + sec_off + 16, 16);
                sec.addr = read_u32_le(d.data() + sec_off + 32);
                sec.size = read_u32_le(d.data() + sec_off + 36);
                sec.offset = read_u32_le(d.data() + sec_off + 40);
                sec.align = read_u32_le(d.data() + sec_off + 44);
                sec.flags = read_u32_le(d.data() + sec_off + 56);
                sec.reserved1 = read_u32_le(d.data() + sec_off + 60);
                sec.reserved2 = read_u32_le(d.data() + sec_off + 64);
                out.sections[sec.full_name()] = sec;
                seg.sections.push_back(std::move(sec));
                sec_off += 68;
            }
            out.segments.push_back(std::move(seg));
        } else if (cmd == kLcSymtab && cmdsize >= 24) {
            symoff = read_u32_le(d.data() + off + 8);
            nsyms = read_u32_le(d.data() + off + 12);
            stroff = read_u32_le(d.data() + off + 16);
            strsize = read_u32_le(d.data() + off + 20);
        } else if (cmd == kLcDysymtab && cmdsize >= 80) {
            dysym.ilocalsym = read_u32_le(d.data() + off + 8);
            dysym.nlocalsym = read_u32_le(d.data() + off + 12);
            dysym.iextdefsym = read_u32_le(d.data() + off + 16);
            dysym.nextdefsym = read_u32_le(d.data() + off + 20);
            dysym.iundefsym = read_u32_le(d.data() + off + 24);
            dysym.nundefsym = read_u32_le(d.data() + off + 28);
            dysym.indirectsymoff = read_u32_le(d.data() + off + 56);
            dysym.nindirectsyms = read_u32_le(d.data() + off + 60);
            dysym.extreloff = read_u32_le(d.data() + off + 64);
            dysym.nextrel = read_u32_le(d.data() + off + 68);
            has_dysym = true;
        } else if (cmd == kLcUnixThread && cmdsize >= 16 + 17 * 4) {
            u32 count = read_u32_le(d.data() + off + 12);
            if (count >= 17) {
                out.entry_sp = read_u32_le(d.data() + off + 16 + 13 * 4);
                out.entry_pc = read_u32_le(d.data() + off + 16 + 15 * 4);
            }
        } else if ((cmd == kLcLoadDylib || cmd == kLcIdDylib || cmd == kLcLoadWeakDylib) && cmdsize >= 24) {
            u32 name_off = read_u32_le(d.data() + off + 8);
            if (name_off < cmdsize) {
                out.dylibs.push_back(read_fixed_name(d.data() + off + name_off, cmdsize - name_off));
            }
        }
        off += cmdsize;
    }

    if (nsyms > 0) {
        if (size_t(symoff) + size_t(nsyms) * 12 > d.size() || size_t(stroff) + strsize > d.size()) {
            return fail("Corrupt Mach-O symbol/string table");
        }
        out.symbols.reserve(nsyms);
        for (u32 i = 0; i < nsyms; ++i) {
            const u8* s = d.data() + symoff + size_t(i) * 12;
            MachSymbol sym;
            sym.index = i;
            u32 n_strx = read_u32_le(s);
            sym.n_type = s[4];
            sym.n_sect = s[5];
            sym.n_desc = read_u16_le(s + 6);
            sym.n_value = read_u32_le(s + 8);
            sym.defined = (sym.n_type & 0x0Eu) != 0;
            if (n_strx < strsize) {
                sym.name = read_fixed_name(d.data() + stroff + n_strx, strsize - n_strx);
            }
            out.symbols.push_back(std::move(sym));
        }
    }

    if (const auto* mod_sec = out.find_section("__DATA.__mod_init_func")) {
        u32 count = mod_sec->size / 4;
        for (u32 i = 0; i < count; ++i) {
            u32 ptr = 0;
            if (out.read_u32(mod_sec->addr + i * 4, ptr) && ptr != 0) {
                out.mod_init_funcs.push_back(ptr);
            }
        }
    }

    std::unordered_map<u32, std::string> bound_relocs;
    if (has_dysym) {
        std::map<u32, MachImportBinding> imports_map;
        for (u32 idx = dysym.iundefsym; idx < dysym.iundefsym + dysym.nundefsym; ++idx) {
            if (idx >= out.symbols.size()) return fail("Undefined symbol index out of range");
            const auto& sym = out.symbols[idx];
            MachImportBinding b;
            b.symbol_index = idx;
            b.name = sym.name;
            u32 ordinal = (sym.n_desc >> 8) & 0xFFu;
            if (ordinal >= 1 && ordinal <= out.dylibs.size()) {
                b.dylib = out.dylibs[ordinal - 1];
            }
            imports_map.emplace(idx, std::move(b));
        }

        if (size_t(dysym.indirectsymoff) + size_t(dysym.nindirectsyms) * 4 > d.size()) {
            return fail("Corrupt Mach-O indirect symbol table");
        }
        std::vector<u32> indirect(dysym.nindirectsyms);
        for (u32 i = 0; i < dysym.nindirectsyms; ++i) {
            indirect[i] = read_u32_le(d.data() + dysym.indirectsymoff + size_t(i) * 4);
        }

        for (const auto& [sec_name, sec] : out.sections) {
            u8 stype = static_cast<u8>(sec.flags & 0xFFu);
            u32 step = 0;
            int kind = 0;  // 1=stub, 2=nl, 3=la
            if (stype == kSectSymbolStubs) {
                step = sec.reserved2;
                kind = 1;
            } else if (stype == kSectNonLazySymPtrs) {
                step = 4;
                kind = 2;
            } else if (stype == kSectLazySymPtrs) {
                step = 4;
                kind = 3;
            } else {
                continue;
            }
            if (step == 0) continue;
            u32 count = sec.size / step;
            for (u32 i = 0; i < count; ++i) {
                if (sec.reserved1 + i >= indirect.size()) return fail("Indirect index out of range");
                u32 entry = indirect[sec.reserved1 + i];
                u32 addr = sec.addr + i * step;
                if (entry & (kIndirectSymbolLocal | kIndirectSymbolAbs)) continue;
                auto it = imports_map.find(entry);
                if (it == imports_map.end()) {
                    if (entry < out.symbols.size()) {
                        out.defined_pointers.push_back({addr, out.symbols[entry].name, out.symbols[entry].n_value});
                    }
                    continue;
                }
                if (kind == 1) it->second.stub_addrs.push_back(addr);
                else if (kind == 2) it->second.nl_symbol_ptr_addrs.push_back(addr);
                else it->second.la_symbol_ptr_addrs.push_back(addr);
            }
        }

        u32 data_base = out.segments.empty() ? 0u : out.segments[0].vmaddr;
        if (size_t(dysym.extreloff) + size_t(dysym.nextrel) * 8 > d.size()) {
            return fail("Corrupt Mach-O external relocations");
        }
        for (u32 i = 0; i < dysym.nextrel; ++i) {
            const u8* rp = d.data() + dysym.extreloff + size_t(i) * 8;
            i32 r_address = read_s32_le(rp);
            u32 word = read_u32_le(rp + 4);
            u32 symnum = word & 0x00FFFFFFu;
            u32 addr = static_cast<u32>(i64(data_base) + r_address);
            auto it = imports_map.find(symnum);
            if (it == imports_map.end()) {
                return fail("External relocation references non-import symbol");
            }
            it->second.data_relocation_addrs.push_back(addr);
            bound_relocs[addr] = it->second.name;
        }

        out.imports.reserve(imports_map.size());
        for (auto& kv : imports_map) {
            out.imports.push_back(std::move(kv.second));
        }
    }

    auto read_methods = [&](u32 list_addr, std::vector<ObjCMethodMeta>& dest) {
        if (list_addr == 0) return;
        u32 entsize_flags = 0, count = 0;
        if (!out.read_u32(list_addr, entsize_flags) || !out.read_u32(list_addr + 4, count)) return;
        u32 entsize = entsize_flags & ~3u;
        if (entsize < 12) return;
        for (u32 i = 0; i < count; ++i) {
            u32 m = list_addr + 8 + i * entsize;
            u32 name_ptr = 0, types_ptr = 0, imp = 0;
            if (!out.read_u32(m, name_ptr) || !out.read_u32(m + 4, types_ptr) || !out.read_u32(m + 8, imp)) break;
            dest.push_back({out.read_cstr(name_ptr), out.read_cstr(types_ptr), imp});
        }
    };

    std::unordered_map<u32, std::string> by_class_addr;
    if (const auto* classlist = out.find_section("__DATA.__objc_classlist")) {
        u32 count = classlist->size / 4;
        for (u32 i = 0; i < count; ++i) {
            u32 caddr = 0;
            if (!out.read_u32(classlist->addr + i * 4, caddr) || caddr == 0) continue;
            u32 isa = 0, sup = 0, data_word = 0;
            out.read_u32(caddr, isa);
            out.read_u32(caddr + 4, sup);
            out.read_u32(caddr + 16, data_word);
            u32 ro = data_word & ~3u;
            u32 inst_size = 0, name_ptr = 0, base_methods = 0;
            out.read_u32(ro + 8, inst_size);
            out.read_u32(ro + 16, name_ptr);
            out.read_u32(ro + 20, base_methods);

            ObjCClassMeta meta;
            meta.name = out.read_cstr(name_ptr);
            meta.class_addr = caddr;
            meta.metaclass_addr = isa;
            meta.superclass_addr = sup;
            meta.instance_size = inst_size;
            if (auto it = bound_relocs.find(caddr + 4); it != bound_relocs.end()) {
                meta.superclass_import = it->second;
            }
            read_methods(base_methods, meta.instance_methods);
            if (isa != 0) {
                u32 mdata = 0, m_methods = 0;
                out.read_u32(isa + 16, mdata);
                out.read_u32((mdata & ~3u) + 20, m_methods);
                read_methods(m_methods, meta.class_methods);
            }
            by_class_addr[caddr] = meta.name;
            out.objc_classes.push_back(std::move(meta));
        }
        for (auto& c : out.objc_classes) {
            if (auto it = by_class_addr.find(c.superclass_addr); it != by_class_addr.end()) {
                c.superclass_in_image = it->second;
            }
            if (!c.superclass_import.empty()) {
                c.superclass_name = strip_class_import_prefix(c.superclass_import);
            } else {
                c.superclass_name = c.superclass_in_image;
            }
        }
    }

    if (const auto* selrefs = out.find_section("__DATA.__objc_selrefs")) {
        u32 count = selrefs->size / 4;
        for (u32 i = 0; i < count; ++i) {
            u32 slot = selrefs->addr + i * 4;
            u32 name_ptr = 0;
            out.read_u32(slot, name_ptr);
            out.objc_selrefs.push_back({slot, out.read_cstr(name_ptr)});
        }
    }

    if (const auto* classrefs = out.find_section("__DATA.__objc_classrefs")) {
        u32 count = classrefs->size / 4;
        for (u32 i = 0; i < count; ++i) {
            u32 slot = classrefs->addr + i * 4;
            u32 val = 0;
            out.read_u32(slot, val);
            ObjCClassRefMeta cr;
            cr.slot = slot;
            if (auto it = bound_relocs.find(slot); it != bound_relocs.end()) {
                cr.import_name = it->second;
            } else if (auto it2 = by_class_addr.find(val); it2 != by_class_addr.end()) {
                cr.in_image_name = it2->second;
            }
            out.objc_classrefs.push_back(std::move(cr));
        }
    }

    return true;
}

}  // namespace hl3

