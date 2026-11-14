#pragma once

#include "common.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace hl3 {

struct MachSection {
    std::string sectname;
    std::string segname;
    u32 addr = 0;
    u32 size = 0;
    u32 offset = 0;
    u32 align = 0;
    u32 flags = 0;
    u32 reserved1 = 0;
    u32 reserved2 = 0;

    std::string full_name() const { return segname + "." + sectname; }
};

struct MachSegment {
    std::string segname;
    u32 vmaddr = 0;
    u32 vmsize = 0;
    u32 fileoff = 0;
    u32 filesize = 0;
    u32 maxprot = 0;
    u32 initprot = 0;
    u32 flags = 0;
    std::vector<MachSection> sections;
};

struct MachSymbol {
    u32 index = 0;
    std::string name;
    u8 n_type = 0;
    u8 n_sect = 0;
    u16 n_desc = 0;
    u32 n_value = 0;
    bool defined = false;
};

struct MachImportBinding {
    u32 symbol_index = 0;
    std::string name;
    std::string dylib;
    std::vector<u32> stub_addrs;
    std::vector<u32> nl_symbol_ptr_addrs;
    std::vector<u32> la_symbol_ptr_addrs;
    std::vector<u32> data_relocation_addrs;
};

struct MachDefinedPointer {
    u32 slot = 0;
    std::string symbol;
    u32 value = 0;
};

struct ObjCMethodMeta {
    std::string selector;
    std::string types;
    u32 imp = 0;  // includes Thumb bit 0 if original was Thumb!
};

struct ObjCClassMeta {
    std::string name;
    u32 class_addr = 0;
    u32 metaclass_addr = 0;
    u32 superclass_addr = 0;
    std::string superclass_import;
    std::string superclass_in_image;
    std::string superclass_name;
    u32 instance_size = 0;
    std::vector<ObjCMethodMeta> instance_methods;
    std::vector<ObjCMethodMeta> class_methods;
};

struct ObjCSelectorRefMeta {
    u32 slot = 0;
    std::string selector;
};

struct ObjCClassRefMeta {
    u32 slot = 0;
    std::string import_name;
    std::string in_image_name;
};

struct MachOImage {
    Bytes data;
    u32 entry_pc = 0;
    u32 entry_sp = 0;
    std::vector<MachSegment> segments;
    std::unordered_map<std::string, MachSection> sections;
    std::vector<std::string> dylibs;
    std::vector<MachSymbol> symbols;
    std::vector<u32> mod_init_funcs;
    std::vector<MachImportBinding> imports;
    std::vector<MachDefinedPointer> defined_pointers;
    std::vector<ObjCClassMeta> objc_classes;
    std::vector<ObjCSelectorRefMeta> objc_selrefs;
    std::vector<ObjCClassRefMeta> objc_classrefs;

    bool vaddr_to_offset(u32 vaddr, size_t& out_off) const;
    bool read_u32(u32 vaddr, u32& out) const;
    std::string read_cstr(u32 vaddr) const;
    const MachSection* find_section(const std::string& full_name) const;
};

bool parse_macho_image(Bytes macho_data, MachOImage& out, std::string* err = nullptr);

}  // namespace hl3

