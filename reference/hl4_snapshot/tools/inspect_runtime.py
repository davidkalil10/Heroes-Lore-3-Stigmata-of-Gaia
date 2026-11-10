#!/usr/bin/env python3
"""S02 offline evidence. Install tools/requirements-analysis.txt in scratch/analysis_deps.

No original code is executed. Virtual addresses are unrebased. Full disassembly
stays in ignored reference/disassembly; compact evidence is reproducible with --check.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
from collections import Counter
from importlib.metadata import version

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scratch/analysis_deps'))
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB, __version__ as cap_version
from elftools.elf.elffile import ELFFile
import elftools

# Caller-side contracts only. These observations do not implement SDK services.
PARTIAL = {
    'dlet_set_property': 'init: (0,100,240,0), (0,101,400,0); ABI_PROBES.json; property semantics open',
    'wipic_event_handler': 'init tail-call (cfunc,0); ABI_PROBES.json; scheduler ownership open',
    'wipic_get_frame_width': 'DM_GET_FRAME_BUFFER_INFO: r0=handle, r0 return width; fixture tested',
    'wipic_get_frame_height': 'DM_GET_FRAME_BUFFER_INFO: r0=handle, r0 return height; fixture tested',
    'wipic_get_frame_pointer': 'DM_GET_FRAME_BUFFER_INFO: r0=handle, r0 return guest pixels; fixture tested',
    'MC_knlGetResourceID': 'CUSTOM_knlGetResourceID: r0=name, r1=size_out; -12 handled by UM_LoadRes',
    'MC_knlGetResource': 'UM_LoadRes: r0=id r1=guest_buffer r2=bytes; -18 causes free/null; fixture tested',
    'MC_knlCalloc': 'UM_LoadRes: r0=bytes; r0 returned allocation, guest owns/free; fixture tested',
    'MC_knlFree': 'UM_LoadRes: r0=allocation on read error; fixture tested',
    'MC_knlDefTimer': 'MyTimer_setTimer via CUSTOM_knlDefTimer: r0=timer storage r1=timerCB; static',
    'MC_knlSetTimer': 'CUSTOM_extSetTimer: r0=timer r1=0 r2:r3=delay [sp]=arg5 (meaning open); boundary tested',
    'MC_knlCurrentTime': 'CUSTOM_extSetTimer: r0:r1 used as 64-bit time; unit/epoch open',
    'MC_grpGetScreenFrameBuffer': 'DM_Initialize r0=0; return opaque handle; static',
    'MC_grpCreateOffScreenFrameBuffer': 'DM_Initialize r0=width r1=height; return handle; static',
    'MC_grpFlushLcd': '__knlGR_draw: r0=0 r1=handle r2=x r3=y [sp]=width [sp+4]=height; static',
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def csv_bytes(rows, fields):
    out = io.StringIO(newline='')
    writer = csv.DictWriter(out, fieldnames=fields, lineterminator='\n')
    writer.writeheader()
    writer.writerows(rows)
    return out.getvalue().encode('utf-8')


def category(name):
    for prefix, group in [('MC_knl', 'kernel/resources/timers'), ('MC_grp', 'graphics'),
                          ('MC_mda', 'audio'), ('MC_fs', 'filesystem'), ('MC_net', 'network'),
                          ('__aeabi_', 'aeabi'), ('dlib_', 'libc/dlib'), ('wipic_', 'wipi bridge')]:
        if name.startswith(prefix):
            return group
    return 'libc/dlib' if name in ('memcpy', 'memmove', 'memset') else 'legacy/platform'


def analyze_elf(data):
    elf = ELFFile(io.BytesIO(data))
    dyn = elf.get_section_by_name('.dynsym')
    syms = list(dyn.iter_symbols())
    allsyms = list(elf.get_section_by_name('.symtab').iter_symbols())
    sections = {s.name: s for s in elf.iter_sections()}
    funcs = {s.name: s for s in allsyms if s['st_info']['type'] == 'STT_FUNC' and s['st_size']}
    labels = {s['st_value'] & ~1: s.name for s in syms if s['st_shndx'] != 'SHN_UNDEF'}
    relocs = []
    for sec in elf.iter_sections():
        if sec['sh_type'] != 'SHT_REL':
            continue
        for r in sec.iter_relocations():
            sym = syms[r['r_info_sym']]
            relocs.append({'section': sec.name, 'address': hex(r['r_offset']), 'type': r['r_info_type'],
                           'symbol': sym.name, 'symbol_index': r['r_info_sym'],
                           'defined': sym['st_shndx'] != 'SHN_UNDEF'})
    # The actual .plt uses a 20-byte resolver header followed by 12-byte slots.
    plt = sections['.plt']['sh_addr']
    slots = [r for r in relocs if r['section'] == '.rel.plt']
    for i, r in enumerate(slots):
        address = plt + 20 + 12 * i
        # Validate the three ARM instructions and their effective GOT address;
        # do not label slots solely by assuming relocation-table order.
        words = struct.unpack_from('<3I', sections['.plt'].data(),20+12*i)
        def immediate(word):
            imm, rot = word & 255, ((word >> 8) & 15)*2
            return ((imm >> rot) | (imm << ((32-rot) & 31))) & 0xffffffff
        if words[0] & 0xfffff000 != 0xe28fc000 or words[1] & 0xfffff000 != 0xe28cc000 or words[2] & 0xfffff000 != 0xe5bcf000:
            raise ValueError(f'Unexpected PLT instruction at {address:#x}')
        got_address = address+8+immediate(words[0])+immediate(words[1])+(words[2]&0xfff)
        if got_address != int(r['address'],16):
            raise ValueError('PLT slot/relocation disagreement')
        labels[address] = r['symbol'] + '@plt'
    mappings = sorted((s['st_value'], s.name[:2]) for s in allsyms if s.name.startswith(('$a', '$t', '$d')))
    arms = {False: Cs(CS_ARCH_ARM, CS_MODE_ARM), True: Cs(CS_ARCH_ARM, CS_MODE_THUMB)}
    calls = []
    listings = {}
    for name, s in sorted(funcs.items(), key=lambda x: x[1]['st_value']):
        start = s['st_value'] & ~1
        sec = elf.get_section(s['st_shndx'])
        off = start - sec['sh_addr']
        blob = sec.data()[off:off + s['st_size']]
        lines = [f'\n{name} {start:#x} size={s["st_size"]}']
        # Mapping symbols distinguish literal pools from instructions.
        points = [(a, k) for a, k in mappings if start < a < start + len(blob)]
        # This ELF has mapping symbols only in its compiler-generated prelude.
        # A previous function's final $d must never classify later STT_FUNCs.
        initial = '$t' if s['st_value'] & 1 else '$a'
        points = [(start, initial)] + points + [(start + len(blob), '')]
        for (a, kind), (end, _) in zip(points, points[1:]):
            chunk = blob[a-start:end-start]
            if kind == '$d':
                lines.append(f'{a:08x}: {chunk.hex()} .data')
                continue
            decoder = arms[kind == '$t']
            decoder.skipdata = True
            decoded = list(decoder.disasm(chunk, a))
            consumed = sum(ins.size for ins in decoded)
            if consumed != len(chunk):
                lines.append(f'{a+consumed:08x}: {chunk[consumed:].hex()} .undecoded')
            for ins in decoded:
                annotation = ''
                if ins.mnemonic.startswith(('bl', 'b')) and ins.op_str.startswith('#'):
                    target = int(ins.op_str[1:], 0)
                    annotation = labels.get(target, '')
                    if ins.mnemonic in ('bl', 'blx', 'b') and annotation:
                        calls.append({'caller': name, 'address': hex(ins.address), 'target': hex(target), 'callee': annotation})
                lines.append(f'{ins.address:08x}: {ins.bytes.hex():<10} {ins.mnemonic} {ins.op_str}' + (f' ; {annotation}' if annotation else ''))
        listings[name] = '\n'.join(lines)
    imports = []
    for s in syms:
        if not s.name or s['st_shndx'] != 'SHN_UNDEF':
            continue
        refs = [c for c in calls if c['callee'] == s.name + '@plt']
        group = category(s.name)
        imports.append({'name': s.name, 'group': group,
                        'priority': 'P2' if group in ('network', 'legacy/platform') else 'P1' if group in ('audio', 'filesystem') else 'P0',
                        'contract': 'partial' if s.name in PARTIAL else 'unmapped',
                        'abi_notes': PARTIAL.get(s.name,'signature, return semantics and ownership remain unverified'),
                        'direct_call_count': len(refs),
                        'evidence': '; '.join(f'{c["caller"]}@{c["address"]}' for c in refs[:8]),
                        'policy': 'trap with symbol and guest state until verified'})
    dynamic = [{'tag': d.entry.d_tag, 'value': d.entry.d_val} for d in sections['.dynamic'].iter_tags()]
    attributes = sections['.ARM.attributes'].data().hex()
    info = {'sha256': sha(data), 'entry': hex(elf['e_entry']), 'flags': hex(elf['e_flags']),
            'segments': [{k: seg[k] for k in ('p_type','p_offset','p_vaddr','p_filesz','p_memsz','p_flags','p_align')} for seg in elf.iter_segments()],
            'dynamic': dynamic, 'arm_attributes_hex': attributes,
            'mapping_symbols': dict(Counter(k for _, k in mappings)),
            'odd_function_symbols': [s.name for s in allsyms if s['st_info']['type'] == 'STT_FUNC' and s['st_value'] & 1],
            'relocation_types': dict(Counter(str(r['type']) for r in relocs)),
            'function_count': len(funcs), 'calls': calls}
    return info, relocs, imports, listings


def analyze_dex(data):
    from loguru import logger
    logger.disable('androguard')
    from androguard.core.dex import DEX
    dex = DEX(data)
    methods = []
    for cls in dex.get_classes():
        for method in cls.get_methods():
            if not method.get_code():
                continue
            offset = 0
            insns = []
            for ins in method.get_instructions():
                insns.append({'offset_bytes': hex(offset), 'raw': ins.get_raw().hex(),
                              'instruction': ins.get_name(), 'operands': ins.get_output()})
                offset += ins.get_length()
            if offset != method.get_code().get_insns_size() * 2:
                raise ValueError('Incomplete DEX method decoding')
            methods.append({'class': cls.get_name(), 'method': method.get_name(),
                            'descriptor': method.get_descriptor(), 'code_offset': hex(method.get_code_off()),
                            'registers': method.get_code().get_registers_size(),
                            'input_register_words': method.get_code().get_ins_size(), 'instructions': insns})
    return {'sha256': sha(data), 'tool': 'androguard ' + version('androguard'),
            'offset_unit': 'bytes relative to method instructions, not code_item', 'methods': methods}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    # Reuse the existing pinned-input guard; never silently analyze a replaced APK.
    sys.path.insert(0, str(ROOT / 'tools'))
    import zipfile
    project = json.loads((ROOT / 'project.json').read_text(encoding='utf-8'))
    apk = (ROOT / project['original_apk']['path']).read_bytes()
    if sha(apk) != project['original_apk']['sha256']:
        raise ValueError('Pinned APK SHA-256 mismatch')
    with zipfile.ZipFile(io.BytesIO(apk)) as archive:
        data = archive.read('lib/armeabi/libbinary.so')
        dex = archive.read('classes.dex')
        jar_data = archive.read('res/raw/jarfile.mp3')
    info, relocs, imports, listings = analyze_elf(data)
    info['tools'] = {'capstone_package': version('capstone'), 'capstone_binding': cap_version, 'pyelftools': elftools.__version__, 'analyzer': 'inspect_runtime v1'}
    info['dex_sha256'] = sha(dex)
    resources = []
    with zipfile.ZipFile(io.BytesIO(jar_data)) as jar:
        mod = jar.read('binary.mod')
        if mod[:len(data)] != data:
            raise ValueError('Module prefix differs from ELF')
        info['module'] = {'sha256': sha(mod), 'prefix_identical': True, 'trailer_offset': len(data),
                          'trailer_hex': mod[len(data):].hex(),
                          'trailer_words_le': list(struct.unpack('<20I', mod[len(data):]))}
        for item in jar.infolist():
            if item.is_dir() or item.filename == 'binary.mod':
                continue
            payload = jar.read(item)
            fmt = 'png' if payload.startswith(b'\x89PNG\r\n\x1a\n') else 'midi' if payload.startswith(b'MThd') else 'ogg' if payload.startswith(b'OggS') else 'smaf-container' if payload.startswith(b'MMMD') else 'wave' if payload.startswith(b'RIFF') and payload[8:12] == b'WAVE' else 'unidentified'
            resources.append({'name': item.filename, 'bytes': len(payload), 'zip_method': item.compress_type,
                              'format': fmt, 'header_16_hex': payload[:16].hex(), 'sha256': sha(payload)})
    info['resources'] = {'count': len(resources), 'formats': dict(Counter(r['format'] for r in resources))}
    artifacts = {
        'reference/analysis/abi_disassembly.txt': ('HL4 S02: unrebased virtual addresses; Capstone disassembly.\n'
            'Sparse ARM mapping symbols: unmarked literal pools may appear as instructions.\n'
            'Only executed ABI probes and manually traced call sites establish contracts.\n' +
            '\n'.join(listings[n] for n in ['init','_init_module','android_c_trampoline','startClet','paintClet',
                'handleCletEvent','onStartApp','onHandleEvent','CUSTOM_knlGetResourceID','tempLocalGetExactFilename',
                'UM_LoadRes','DM_GET_FRAME_BUFFER_INFO','MyTimer_setTimer','MyTimer_setTimerAfterPaint',
                'CUSTOM_extSetTimer','timerCB','CUSTOM_onStartClet','clsidAuth','clsidAuthDo',
                'auth_startApp','gamerating_close','__knlGR_draw','DM_Initialize'])+'\n').encode(),
        'reference/analysis/dex_contract.json': (json.dumps(analyze_dex(dex), ensure_ascii=False, indent=2)+'\n').encode(),
        'reference/analysis/runtime_inventory.json': (json.dumps(info, ensure_ascii=False, indent=2)+'\n').encode(),
        'reference/analysis/runtime_relocations.csv': csv_bytes(relocs, ['section','address','type','symbol','symbol_index','defined']),
        'reference/analysis/runtime_imports.csv': csv_bytes(imports, ['name','group','priority','contract','abi_notes','direct_call_count','evidence','policy']),
        'reference/analysis/resource_headers.csv': csv_bytes(resources, ['name','bytes','zip_method','format','header_16_hex','sha256']),
    }
    for name, blob in artifacts.items():
        path = ROOT / name
        if args.check:
            if not path.is_file() or path.read_bytes() != blob:
                raise ValueError('Report mismatch: ' + name)
        else:
            path.write_bytes(blob)
    destination = ROOT / 'reference/disassembly'
    destination.mkdir(exist_ok=True)
    if not args.check:
        (destination / 'hl4_arm.txt').write_text('\n'.join(listings.values())+'\n', encoding='utf-8')
        chosen = ['init','_init_module','startClet','onStartApp','onHandleEvent','paintClet']
        (destination / 'hl4_boot.txt').write_text('\n'.join(listings[n] for n in chosen)+'\n', encoding='utf-8')
    print(f'PASS: S02 static evidence {len(imports)} imports, {len(relocs)} relocations, {len(listings)} functions; no guest execution.')


if __name__ == '__main__':
    main()
