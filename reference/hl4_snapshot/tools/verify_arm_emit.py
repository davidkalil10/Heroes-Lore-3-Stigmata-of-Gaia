"""Compile actual emitted ALU operations and compare edge/random states to Unicorn."""
import ctypes, hashlib, json, os, random, re, shutil, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'scratch/analysis_deps'),str(ROOT/'tools')]
from arm_emit import emit
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import *

def main():
    pc=0x04001000;selected={}
    for value in re.findall(r'\{0x[0-9a-f]+,0x([0-9a-f]+)u\}',(ROOT/'src/arm_hot_blocks.inc').read_text()):
        ins=int(value,16);body=emit(pc,ins)
        if any(x in body for x in ['load','store','step<']):continue
        if 'u32 lhs=' not in body and not ins&0x0fc000f0==0x00000090:continue
        if ins&0x0fc000f0==0x00000090 and (ins>>16)&15==15:continue
        kind=(ins>>25)&7
        key=(ins>>28,kind,(ins>>21)&15,(ins>>20)&1,
             ((ins>>5)&3, bool(ins&0x10), ((ins>>7)&31) in (0,31)) if kind==0 else bool(ins&0xf00))
        selected.setdefault(key,ins)
    ops=list(selected.values());assert len(ops)>16
    out=ROOT/'scratch/arm-emit-probe';out.mkdir(parents=True,exist_ok=True)
    source='''#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <algorithm>
using u32=uint32_t;using u64=uint64_t;using s32=int32_t;
constexpr u32 BASE=0x02000000,TEMP=0x04000000,TRAP=0x05000000,HEAP=0x06000000;
'''+f'#include "{(ROOT/"src/arm_hot_decoder.inc").as_posix()}"\n'
    source+='struct Probe:HotArm{Probe():HotArm(nullptr){} void run(unsigned i){switch(i){\n'
    for i,ins in enumerate(ops):source+=f'case {i}:{{'+emit(pc,ins)+'return;}\n'
    source+='default:throw std::runtime_error("probe index");}}};\n'
    source+='''#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
extern "C" EXPORT void probe(unsigned index,const u32* before,u32* after){
 Probe p;std::copy_n(before,16,p.r);p.N=before[16]>>31;p.Z=(before[16]>>30)&1;
 p.C=(before[16]>>29)&1;p.V=(before[16]>>28)&1;p.run(index);
 std::copy_n(p.r,16,after);after[16]=(before[16]&0x0fffffff)|(u32(p.N)<<31)|(u32(p.Z)<<30)|(u32(p.C)<<29)|(u32(p.V)<<28);
}
'''
    cpp=out/'probe.cpp';cpp.write_text(source)
    if os.name=='nt':
        vswhere=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
        vs=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
        dll=out/'probe.dll';batch=out/'build.cmd'
        args=['cl','/nologo','/std:c++17','/O2','/EHsc','/MT','/LD',str(cpp),'/Fo:'+str(out/'probe.obj'),'/Fe:'+str(dll)]
        batch.write_text('@echo off\ncall "'+str(Path(vs)/'VC/Auxiliary/Build/vcvars64.bat')+'"\nif errorlevel 1 exit /b 1\n'+subprocess.list2cmdline(args)+'\n')
        subprocess.run(['cmd','/c',str(batch)],cwd=out,check=True,stdout=subprocess.DEVNULL)
    else:
        dll=out/'probe.so';subprocess.run(['c++','-std=c++17','-O2','-shared','-fPIC',str(cpp),'-o',str(dll)],check=True)
    lib=ctypes.CDLL(str(dll));array=ctypes.c_uint32*17
    lib.probe.argtypes=[ctypes.c_uint,ctypes.POINTER(ctypes.c_uint32),ctypes.POINTER(ctypes.c_uint32)]
    uc=Uc(UC_ARCH_ARM,UC_MODE_ARM);uc.mem_map(0x04000000,0x10000)
    registers=[UC_ARM_REG_R0+i for i in range(13)]+[UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_CPSR]
    rng=random.Random(0x484c34);edges=[0,1,31,32,33,255,0x7fffffff,0x80000000,0xffffffff];count=0
    for i,ins in enumerate(ops):
        uc.mem_write(pc,ins.to_bytes(4,'little'));uc.ctl_remove_cache(pc,pc+4)
        for trial in range(32):
            values=[edges[trial%len(edges)] if trial<9 else rng.getrandbits(32) for _ in range(16)]
            # Dynamic shifts need explicit low-byte boundary coverage.
            if (ins>>25)&7==0 and ins&0x10:values[(ins>>8)&15]=edges[trial%len(edges)]
            values[15]=pc;values.append(0x10|((trial%16)<<28));before=array(*values);actual=array()
            uc.reg_write(UC_ARM_REG_CPSR,values[16])
            for reg,value in zip(registers[:16],values[:16]):uc.reg_write(reg,value)
            uc.emu_start(pc,pc+4,count=1)
            expected=[uc.reg_read(reg) for reg in registers]
            lib.probe(i,before,actual)
            assert list(actual)==expected,(hex(ins),trial,list(actual),expected)
            count+=1
    report={'result':'PASS','opcode_forms':len(ops),'states':count,'seed':'0x484c34','oracle':'Unicorn ARM; R0-R15 and CPSR; actual compiled arm_emit output',
            'emitter_sha256':hashlib.sha256((ROOT/'tools/arm_emit.py').read_bytes()).hexdigest()}
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS emitted ARM ALU:',len(ops),'forms,',count,'edge/random register/flag comparisons')

if __name__=='__main__':main()
