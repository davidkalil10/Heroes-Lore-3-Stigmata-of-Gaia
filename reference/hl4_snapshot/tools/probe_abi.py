#!/usr/bin/env python3
"""Execute bounded HL4 ABI probes using Unicorn, never a playable runtime.

Imports trap by default. Selected tests may explicitly stop at a call boundary
or observe calls with a declared fixture result. That is a mock of the external
service, not proof that WIPI/HLE, boot, rendering or input works.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import zipfile
from importlib.metadata import version

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scratch/analysis_deps'))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

REGS = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3]
TEMP = 0x04000000
TRAPS = 0x05000000
RETURN = TRAPS + 0xf000


class Probe:
    def __init__(self, data, base):
        self.base = base
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.uc.mem_map(base, 0x200000)
        self.uc.mem_map(TEMP, 0x100000)
        self.uc.mem_map(TRAPS, 0x10000)
        self.elf = ELFFile(io.BytesIO(data))
        self.symbols = {s.name: s for s in self.elf.get_section_by_name('.dynsym').iter_symbols() if s.name}
        self.exports = {n: base+s['st_value'] for n,s in self.symbols.items() if s['st_shndx'] != 'SHN_UNDEF'}
        self.traps = {TRAPS + 4*i: n for i,n in enumerate(n for n,s in self.symbols.items() if s['st_shndx'] == 'SHN_UNDEF')}
        trap_address = {n: a for a,n in self.traps.items()}
        self.segments = [s for s in self.elf.iter_segments() if s['p_type'] == 'PT_LOAD']
        for seg in self.segments:
            self.uc.mem_write(base+seg['p_vaddr'], seg.data())
            assert self.uc.mem_read(base+seg['p_vaddr'],seg['p_filesz']) == seg.data()
            bss_size = seg['p_memsz']-seg['p_filesz']
            assert self.uc.mem_read(base+seg['p_vaddr']+seg['p_filesz'],bss_size) == bytes(bss_size)
        dyn = list(self.elf.get_section_by_name('.dynsym').iter_symbols())
        self.relocation_count = 0
        for sec in self.elf.iter_sections():
            if sec['sh_type'] != 'SHT_REL':
                continue
            for r in sec.iter_relocations():
                where = base+r['r_offset']
                assert any(base+s['p_vaddr'] <= where and where+4 <= base+s['p_vaddr']+s['p_memsz'] for s in self.segments)
                typ = r['r_info_type']
                if typ == 23:
                    assert r['r_info_sym'] == 0
                    value = self.read(where)+base
                elif typ in (21,22):
                    sym = dyn[r['r_info_sym']]
                    value = self.exports[sym.name] if sym['st_shndx'] != 'SHN_UNDEF' else trap_address[sym.name]
                else:
                    raise ValueError(f'Unsupported relocation {typ}')
                self.write(where,value)
                self.relocation_count += 1
        self.events = []
        self.observers = {}
        self.returned = False
        self.stopped = False
        self.uc.hook_add(UC_HOOK_CODE, self.hook)

    def read(self, address):
        return struct.unpack('<I',self.uc.mem_read(address,4))[0]

    def write(self, address, value):
        self.uc.mem_write(address,struct.pack('<I',value & 0xffffffff))

    def hook(self, uc, address, size, _):
        if address == RETURN:
            self.returned = True
            uc.emu_stop()
        elif address in self.traps or address in self.observers:
            name = self.traps.get(address,hex(address))
            args = [uc.reg_read(reg) for reg in REGS]
            self.events.append({'name':name,'args':[hex(a) for a in args],
                                'stack_arg0':hex(self.read(uc.reg_read(UC_ARM_REG_SP))),
                                'lr':hex(uc.reg_read(UC_ARM_REG_LR))})
            if address not in self.observers:
                raise RuntimeError(f'Unmapped import {name} args={args}')
            action = self.observers[address]
            if action == 'stop':
                self.stopped = True
                uc.emu_stop()
            else:
                result = action(args)
                uc.reg_write(UC_ARM_REG_R0,result)
                uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))

    def observe(self, name, action):
        address = self.exports.get(name)
        if address is None:
            address = next(a for a,n in self.traps.items() if n == name)
        self.observers[address] = action

    def call(self, name, args=(), stack_arg0=0):
        self.returned = self.stopped = False
        for reg,val in zip(REGS,args):
            self.uc.reg_write(reg,val)
        self.uc.reg_write(UC_ARM_REG_SP,TEMP+0xff000)
        self.write(TEMP+0xff000,stack_arg0)
        self.uc.reg_write(UC_ARM_REG_LR,RETURN)
        self.uc.emu_start(self.exports[name],RETURN+4,count=20000)
        assert self.returned or self.stopped, 'Instruction budget exhausted'


def run(data):
    results = []
    for base in (0x02000000,0x02800000):
        p = Probe(data,base)
        p.call('_init_module',(TEMP,TEMP+0x1000,TEMP+0x2000))
        assert p.read(TEMP+0x214) == p.exports['_export_table']
        # Derive the three global destinations directly from the init GOT loads.
        got = 0xe01a8
        destinations = [p.read(base+got+off) for off in (0x698,0x4e8,0x594)]
        assert [p.read(a) for a in destinations] == [TEMP,TEMP+0x1000,TEMP+0x2000]
        callback_names = ['startClet','pauseClet','resumeClet','destroyClet','paintClet','handleCletEvent']
        assert [p.read(p.exports['cfunc']+4*i) for i in range(6)] == [p.exports[n] for n in callback_names]
        assert p.read(p.exports['_export_table']) == 3
        assert p.read(p.exports['_export_table']+4) == p.exports['init']
        results.append({'probe':'_init_module three-argument stores','base':hex(base),'result':'pass',
                        'context_export_table_offset':'0x214','table':'_export_table',
                        'global_virtual_addresses':[hex(a-base) for a in destinations],
                        'cfunc_slots':callback_names,
                        'relocations':p.relocation_count})
        # Observe the actual init calls; property service results are a fixture.
        p.observe('dlet_set_property',lambda _: 0)
        p.observe('wipic_event_handler','stop')
        p.call('init')
        assert [e['name'] for e in p.events] == ['dlet_set_property']*3+['wipic_event_handler']
        assert [int(x,16) for x in p.events[1]['args']] == [0,100,240,0]
        assert [int(x,16) for x in p.events[2]['args']] == [0,101,400,0]
        assert [int(x,16) for x in p.events[-1]['args'][:2]] == [p.exports['cfunc'],0]
        results.append({'probe':'init stops at WIPI boundary','base':hex(base),'result':'pass',
                        'fixture':'dlet_set_property return 0 only; no WIPI event handler implemented', 'events':p.events})
    p = Probe(data,0x02000000)
    p.call('paintClet')
    assert p.returned and not p.events
    results.append({'probe':'paintClet returns without imports','result':'pass'})
    for subtype,event in ((0,0x5064),(1,0x5065),(2,0x5066)):
        p = Probe(data,0x02000000)
        p.write(TEMP,123)
        p.write(TEMP+4,456)
        p.observe('clsidAuthHandle','stop')
        p.call('handleCletEvent',(0x708,subtype,TEMP))
        actual = [int(x,16) for x in p.events[-1]['args']]
        assert actual == [event,123,456,0xffffffff]
        results.append({'probe':'touch event adapter','input':[0x708,subtype,123,456],
                        'output':actual,'result':'pass','boundary':'stopped before clsidAuthHandle; no input gameplay test'})
    for width,height in ((240,400),(320,240)):
        p = Probe(data,0x02000000)
        handle = TEMP+0x1000
        pixels = TEMP+0x2000
        p.uc.mem_write(TEMP,bytes([0xa5])*32)
        p.observe('wipic_get_frame_width',lambda args: width if args[0] == handle else -1)
        p.observe('wipic_get_frame_height',lambda args: height if args[0] == handle else -1)
        p.observe('wipic_get_frame_pointer',lambda args: pixels if args[0] == handle else 0)
        p.call('DM_GET_FRAME_BUFFER_INFO',(TEMP,handle))
        expected = [width*2,2,width,height,pixels]
        assert [p.read(TEMP+i*4) for i in range(5)] == expected
        assert p.uc.mem_read(TEMP+20,4) == bytes([2,0xa5,0xa5,0xa5])
        results.append({'probe':'framebuffer descriptor written by original code','result':'pass',
                        'fixture_dimensions':[width,height], 'words':expected,'byte_0x14':2,
                        'scope':'WIPI handle queries mocked; no image produced'})
    p = Probe(data,0x02000000)
    p.observe('MC_knlSetTimer','stop')
    p.call('CUSTOM_extSetTimer',(TEMP,0,50,0),stack_arg0=0x5678)
    assert [int(x,16) for x in p.events[-1]['args']] == [TEMP,0,50,0]
    assert int(p.events[-1]['stack_arg0'],16) == 0x5678
    results.append({'probe':'timer 64-bit delay and stack argument passthrough','result':'pass','events':p.events})
    for event,offset in ((502,0x30),(503,0x34)):
        p = Probe(data,0x02000000)
        p.observe('LoadFrame_isLoading',lambda _:0)
        p.call('onHandleEvent',(event,0x32,0))
        assert p.read(p.exports['mainFrame']+offset) == 0x32
        results.append({'probe':'key event stores pending code','result':'pass','event':event,
                        'key_code':0x32,'mainFrame_offset':hex(offset),
                        'fixture':'LoadFrame_isLoading returns 0; globals otherwise start at BSS zero; no frame/gameplay'})
    for name in ('CM/GRADE_BM','/CM/GRADE_BM','\\CM/GRADE_BM','cm/Grade_BM'):
        p = Probe(data,0x02000000)
        p.uc.mem_write(TEMP,name.encode()+b'\0')
        def cstr(address):
            return bytes(p.uc.mem_read(address,300)).split(b'\0',1)[0]
        def memset_fixture(args):
            p.uc.mem_write(args[0],bytes([args[1]&255])*args[2])
            return args[0]
        def strcpy_fixture(args):
            p.uc.mem_write(args[0],cstr(args[1])+b'\0')
            return args[0]
        def sprintf_fixture(args):
            assert cstr(args[1]) == b'res/%s'
            value = b'res/'+cstr(args[2])
            p.uc.mem_write(args[0],value+b'\0')
            return len(value)
        p.observe('dlib_memset',memset_fixture)
        p.observe('dlib_strcpy',strcpy_fixture)
        p.observe('MC_knlSprintk',sprintf_fixture)
        p.observe('MC_knlGetResourceID','stop')
        p.call('CUSTOM_knlGetResourceID',(TEMP,TEMP+0x1000))
        last = p.events[-1]
        normalized = cstr(int(last['args'][0],16)).decode()
        assert normalized == 'res/'+name.lstrip('/\\')
        assert int(last['args'][1],16) == TEMP+0x1000
        results.append({'probe':'resource name normalization','result':'pass','input':name,'output':normalized,
                        'fixture':'memory/string formatting services only; stops before resource lookup'})
    # Isolate ownership/error paths in UM_LoadRes. Resource lookup and allocator
    # are fixtures; this does not assert SDK-wide return conventions.
    for lookup, read_result in ((7,0),(7,0xffffffee),(0xfffffff4,0)):
        p = Probe(data,0x02000000)
        allocated = TEMP+0x2000
        def lookup_fixture(args):
            p.write(args[1],32)
            return lookup
        p.observe('CUSTOM_knlGetResourceID',lookup_fixture)
        p.observe('MC_knlCalloc',lambda args: allocated if args[0] == 32 else 0)
        p.observe('MC_knlGetResource',lambda _: read_result)
        p.observe('MC_knlFree',lambda _: 0)
        p.call('UM_LoadRes',(TEMP+0x1000,TEMP))
        failed = lookup == 0xfffffff4 or read_result == 0xffffffee
        assert p.uc.reg_read(UC_ARM_REG_R0) == (0 if failed else allocated)
        assert p.read(TEMP) == (0 if failed else 32)
        frees = [e for e in p.events if e['name']=='MC_knlFree']
        assert len(frees) == (1 if read_result == 0xffffffee else 0)
        results.append({'probe':'UM_LoadRes ownership/error branch','fixture_lookup':hex(lookup),
                        'fixture_read_result':hex(read_result),'result':'pass','events':p.events})
    p = Probe(data,0x02000000)
    try:
        p.call('init')
    except RuntimeError as exc:
        assert 'Unmapped import dlet_set_property' in str(exc)
        results.append({'probe':'unknown import fails closed','result':'pass','diagnostic':str(exc)})
    else:
        raise AssertionError('Unknown import silently accepted')
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args = parser.parse_args()
    project = json.loads((ROOT/'project.json').read_text(encoding='utf-8'))
    apk = (ROOT/project['original_apk']['path']).read_bytes()
    assert hashlib.sha256(apk).hexdigest() == project['original_apk']['sha256']
    with zipfile.ZipFile(io.BytesIO(apk)) as z:
        data = z.read('lib/armeabi/libbinary.so')
    report = {'scope':'isolated original ARM instructions in Unicorn on Windows; no game boot or native runtime',
              'elf_sha256':hashlib.sha256(data).hexdigest(),'unicorn':version('unicorn'),'tests':run(data)}
    blob = (json.dumps(report,ensure_ascii=False,indent=2)+'\n').encode()
    path = ROOT/'reference/analysis/ABI_PROBES.json'
    if args.check:
        assert path.read_bytes() == blob,'ABI report mismatch'
    else:
        path.write_bytes(blob)
    print(f'PASS: {len(report["tests"])} bounded ARM ABI probes; no gameplay claim.')


if __name__ == '__main__':
    main()
