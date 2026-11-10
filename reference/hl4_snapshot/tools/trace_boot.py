#!/usr/bin/env python3
"""Local WIPI research harness: real guest code, explicit host services, no game hooks."""
import sys,io,json,struct,time,zipfile,hashlib,re,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from probe_abi import Probe,TEMP,TRAPS,RETURN,REGS
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import *

class Wipi(Probe):
    def __init__(self):
        project=json.loads((ROOT/'project.json').read_text(encoding='utf-8'))
        apk=(ROOT/project['original_apk']['path']).read_bytes()
        if hashlib.sha256(apk).hexdigest()!=project['original_apk']['sha256']:raise ValueError('APK hash')
        with zipfile.ZipFile(io.BytesIO(apk)) as z:
            data=z.read('lib/armeabi/libbinary.so');jar=z.read('res/raw/jarfile.mp3')
        self.resources={n:zdata for n,zdata in []}
        with zipfile.ZipFile(io.BytesIO(jar)) as z:
            self.resources={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
        self.resource_names=list(self.resources);self.resource_ids={n:i+1 for i,n in enumerate(self.resource_names)}
        super().__init__(data,0x02000000)
        # Parent probe is intentionally instruction-traced; use bounded trap-only
        # hooks here, letting Unicorn execute guest loops at native engine speed.
        # Constructed hook can be removed using the engine's registered handles.
        for handle in list(self.uc._callbacks):self.uc.hook_del(handle)
        self.uc.hook_add(UC_HOOK_CODE,self.hook,begin=TRAPS,end=TRAPS+0xffff)
        self.uc.mem_map(0x06000000,0x04000000)
        self.free_blocks=[(0x06000000,0x04000000)];self.allocations={}
        self.frames={};self.next_handle=1;self.contexts={};self.timers={};self.callbacks=0
        self.start=time.perf_counter();self.clock=0;self.flushes=0;self.seen={};self.trace=[]
        self.files={};self.file_handles={};self.next_file=1
        self.framebuffer=self.new_frame(240,400);self.last_frame=self.framebuffer
        self.observe_names={a:n for a,n in self.traps.items()}
        for a in self.traps:self.observers[a]=lambda args,a=a:self.service(self.traps[a],args)

    def alloc(self,n):
        n=(max(1,n)+7)&~7
        for i,(a,size) in enumerate(self.free_blocks):
            if size>=n:
                self.free_blocks[i:i+1]=[(a+n,size-n)] if size>n else []
                self.allocations[a]=n;self.uc.mem_write(a,bytes(n));return a
        return 0
    def free(self,a):
        if not a:return 0
        if a not in self.allocations:raise RuntimeError(f'invalid free {a:x}')
        self.free_blocks.append((a,self.allocations.pop(a)));self.free_blocks.sort()
        merged=[]
        for start,size in self.free_blocks:
            if merged and merged[-1][0]+merged[-1][1]==start:merged[-1]=(merged[-1][0],merged[-1][1]+size)
            else:merged.append((start,size))
        self.free_blocks=merged;return 0
    def cstr(self,a):
        if not a:raise RuntimeError('null string')
        out=bytearray()
        while len(out)<65536:
            b=bytes(self.uc.mem_read(a+len(out),1))
            if b==b'\0':return bytes(out)
            out+=b
        raise RuntimeError('unterminated string')
    def args(self,i):return self.uc.reg_read(REGS[i]) if i<4 else self.read(self.uc.reg_read(UC_ARM_REG_SP)+4*(i-4))
    def new_frame(self,w,h):
        if not 0<w<=4096 or not 0<h<=4096:raise RuntimeError(f'frame size {w} {h}')
        handle=self.next_handle;self.next_handle+=1
        self.frames[handle]=(w,h,self.alloc(w*h*2));return handle
    def long(self,value):
        self.uc.reg_write(UC_ARM_REG_R1,(value>>32)&0xffffffff);return value&0xffffffff
    def sprintf(self,fmt,start):
        raw=self.cstr(fmt).decode('latin1');index=start
        def part(m):
            nonlocal index
            token=m.group()
            if token=='%%':return '%'
            v=self.args(index);index+=1
            if token[-1]=='s':v=self.cstr(v).decode('latin1')
            elif token[-1] in 'di' and v&0x80000000:v-=0x100000000
            elif token[-1]=='c':v&=255
            if token[-1]=='p':return hex(v)
            return token%v
        return re.sub(r'%%|%[-+ #0]*\d*(?:\.\d+)?[sdixXucp]',part,raw).encode('latin1')
    def service(self,n,a):
        self.seen[n]=self.seen.get(n,0)+1
        if self.seen[n]<=3:
            print(n,[hex(v) for v in a],flush=True)
            self.trace.append({'name':n,'r0_r3':[hex(v) for v in a],'lr':hex(self.uc.reg_read(UC_ARM_REG_LR))})
        x,y,z,t=a
        if n in ('memset','dlib_memset'):self.uc.mem_write(x,bytes([y&255])*z);return x
        if n in ('memcpy','dlib_memcpy','memmove','dlib_memmove'):self.uc.mem_write(x,bytes(self.uc.mem_read(y,z)));return x
        if n=='dlib_strlen':return len(self.cstr(x))
        if n in ('dlib_strcpy','dlib_strncpy'):
            data=self.cstr(y)+b'\0'
            if n.endswith('ncpy'):data=data[:z].ljust(z,b'\0')
            self.uc.mem_write(x,data);return x
        if n in ('dlib_strcmp','dlib_strncmp','dlib_memcmp'):
            left,right=(bytes(self.uc.mem_read(x,z)),bytes(self.uc.mem_read(y,z))) if n.endswith('memcmp') else (self.cstr(x),self.cstr(y))
            if n.endswith('ncmp'):left,right=left[:z],right[:z]
            return (left>right)-(left<right)
        if n=='dlib_strstr':
            i=self.cstr(x).find(self.cstr(y));return x+i if i>=0 else 0
        if n=='MC_knlSprintk':
            data=self.sprintf(y,2);self.uc.mem_write(x,data+b'\0');return len(data)
        if n=='MC_knlPrintk':
            print('guest:',self.sprintf(x,1).decode('cp949',errors='replace'),flush=True);return 0
        if n in ('MC_knlAlloc','MC_knlCalloc'):return self.alloc(x)
        if n=='MC_knlFree':return self.free(x)
        if n in ('MC_knlGetFreeMemory','MC_knlGetTotalMemory'):return sum(size for _,size in self.free_blocks) if n.endswith('FreeMemory') else 0x4000000
        if n=='dlet_set_property':return 0 # host window properties recorded; dimensions established by init
        if n=='wipic_event_handler':self.callbacks=x;return 0
        if n=='MC_knlGetSystemProperty':
            key=self.cstr(x);value={b'PHONEMODEL':b'PC',b'TIMEZONE':b'GMT-0300'}.get(key)
            if key==b'PHONENUMBER':
                if z:self.uc.mem_write(y,b'\0')
                return -1 # PC has no telephone number; report unavailable
            if value is None:raise RuntimeError(f'Unknown property {key}')
            if len(value)+1>z:return -1
            self.uc.mem_write(y,value+b'\0');return 0
        if n in ('MC_miscBackLight','MC_mdaVibrator'):return 0 # optional device-only effects: deliberate no-op on PC
        if n=='MC_mdaSetMuteState':self.mute=bool(y);return 0
        if n=='MC_knlCurrentTime':return self.long(int((time.perf_counter()-self.start)*1000))
        if n=='MC_knlDefTimer':self.timers[x]={'callback':y,'armed':False};return 0
        if n=='MC_knlSetTimer':
            timer=self.timers[x];timer.update(armed=True,due=int((time.perf_counter()-self.start)*1000)+(z|(t<<32)),user=self.args(4));return 0
        if n=='MC_knlUnsetTimer':self.timers[x]['armed']=False;return 0
        if n=='MC_knlGetResourceID':
            name=self.cstr(x).decode('ascii')
            if name not in self.resource_ids:print('MISSING RESOURCE',name,flush=True);return -12
            if y:self.write(y,len(self.resources[name]))
            return self.resource_ids[name]
        if n=='MC_knlGetResource':
            if not 1<=x<=len(self.resource_names):return -12
            blob=self.resources[self.resource_names[x-1]]
            if z<len(blob):return -18
            self.uc.mem_write(y,blob);return 0
        if n=='MC_fsFileAttribute':
            name=self.cstr(x)
            if name not in self.files:return -1
            self.uc.mem_write(y,struct.pack('<4I',0,0,len(self.files[name]),0));return 0
        if n=='MC_fsOpen':
            name=self.cstr(x)
            if y==1 and name not in self.files:return -1
            if y not in (1,8):raise RuntimeError(f'file mode {y}')
            if name not in self.files:self.files[name]=bytearray()
            handle=self.next_file;self.next_file+=1;self.file_handles[handle]=[name,0];return handle
        if n=='MC_fsClose':self.file_handles.pop(x);return 0
        if n=='MC_fsSeek':
            f=self.file_handles[x];delta=y if y<0x80000000 else y-0x100000000
            target=delta+(0 if z==0 else f[1] if z==1 else len(self.files[f[0]]) if z==2 else -0x100000000)
            if target<0:return -1
            f[1]=target;return target
        if n in ('MC_fsRead','MC_fsWrite'):
            f=self.file_handles[x];blob=self.files[f[0]]
            if n.endswith('Read'):
                data=bytes(blob[f[1]:f[1]+z])
                if data:self.uc.mem_write(y,data)
                f[1]+=len(data);return len(data)
            if f[1]+z>len(blob):blob.extend(bytes(f[1]+z-len(blob)))
            blob[f[1]:f[1]+z]=self.uc.mem_read(y,z);f[1]+=z;return z
        if n=='MC_fsRemove':
            name=self.cstr(x)
            if name not in self.files:return -1
            del self.files[name];return 0
        if n=='MC_grpGetScreenFrameBuffer':
            if x!=0:raise RuntimeError('display index')
            return self.framebuffer
        if n=='MC_grpCreateOffScreenFrameBuffer':return self.new_frame(x,y)
        if n=='MC_grpDestroyOffScreenFrameBuffer':
            self.free(self.frames.pop(x)[2]);return 0
        if n.startswith('wipic_get_frame_'):
            w,h,p=self.frames[x];return w if n.endswith('width') else h if n.endswith('height') else p
        if n=='MC_grpInitContext':self.contexts[x]={1:0xffff,2:0};return 0
        if n=='MC_grpSetContext':self.contexts[x][y]=z;return 0
        if n=='MC_grpGetContext':return self.contexts[x].get(y,0)
        if n=='MC_grpGetPixelFromRGB':return ((x>>3)<<11)|((y>>2)<<5)|(z>>3)
        if n=='MC_grpGetRGBFromPixel':
            self.write(y,((x>>11)&31)*255//31);self.write(z,((x>>5)&63)*255//63);self.write(t,(x&31)*255//31);return 0
        if n=='MC_grpFillRect':
            width=self.args(4);context=self.args(5);w,h,p=self.frames[x];color=self.contexts[context][1]&65535
            for yy in range(max(0,z),min(h,z+width)):
                left=max(0,y);right=min(w,y+t)
                if right>left:self.uc.mem_write(p+(yy*w+left)*2,struct.pack('<H',color)*(right-left))
            return 0
        if n=='MC_grpFlushLcd':self.last_frame=y;self.flushes+=1;return 0
        if n.startswith('__aeabi_'):
            if n in ('__aeabi_idiv','__aeabi_idivmod','__aeabi_uidivmod'):
                if n!='__aeabi_uidivmod':x=x if x<0x80000000 else x-0x100000000;y=y if y<0x80000000 else y-0x100000000
                if y==0:raise RuntimeError('divide by zero')
                q=abs(x)//abs(y)*(1 if (x<0)==(y<0) else -1);self.uc.reg_write(UC_ARM_REG_R1,(x-q*y)&0xffffffff);return q
            isdouble=n.startswith(('__aeabi_d','__aeabi_i2d','__aeabi_f2d'))
            f=lambda value:struct.unpack('<f',struct.pack('<I',value))[0]
            d=lambda lo,hi:struct.unpack('<d',struct.pack('<II',lo,hi))[0]
            if n in ('__aeabi_i2d','__aeabi_i2f'):value=float(x if x<0x80000000 else x-0x100000000)
            elif n=='__aeabi_f2d':value=f(x)
            elif n=='__aeabi_d2f':value=d(x,y);isdouble=False
            elif n=='__aeabi_d2iz':return int(d(x,y))
            else:
                left,right=(d(x,y),d(z,t)) if isdouble else (f(x),f(y))
                op=n.split('_')[-1][1:]
                if op.startswith('cmp'):return int({'cmpeq':left==right,'cmpge':left>=right,'cmpgt':left>right,'cmplt':left<right}[op])
                value={'add':lambda:left+right,'sub':lambda:left-right,'mul':lambda:left*right,'div':lambda:left/right}[op]()
            if isdouble:
                lo,hi=struct.unpack('<II',struct.pack('<d',value));self.uc.reg_write(UC_ARM_REG_R1,hi);return lo
            return struct.unpack('<I',struct.pack('<f',value))[0]
        raise RuntimeError(f'Unmapped import {n} args={list(map(hex,a))} caller={self.uc.reg_read(UC_ARM_REG_LR)-self.base:x}')

    def hook(self,uc,address,size,_):
        if address==RETURN:self.returned=True;uc.emu_stop();return
        if address not in self.traps:raise RuntimeError(f'Unknown trap {address:x}')
        result=self.service(self.traps[address],[uc.reg_read(r) for r in REGS])
        uc.reg_write(UC_ARM_REG_R0,result&0xffffffff);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
    def call(self,name,args=()):
        self.returned=False
        for reg,value in zip(REGS,args):self.uc.reg_write(reg,value)
        self.uc.reg_write(UC_ARM_REG_SP,TEMP+0xff000);self.uc.reg_write(UC_ARM_REG_LR,RETURN)
        self.uc.emu_start(self.exports[name] if isinstance(name,str) else name,RETURN+4,count=2000000000)
        if not self.returned:raise RuntimeError(f'instruction budget PC={self.uc.reg_read(UC_ARM_REG_PC):x}')
    def boot(self):
        self.call('_init_module',(TEMP,TEMP+0x1000,TEMP+0x2000));self.call('init')
        self.call(self.read(self.callbacks))
    def tick(self):
        now=int((time.perf_counter()-self.start)*1000)
        for handle,timer in list(self.timers.items()):
            if timer['armed'] and timer['due']<=now:
                timer['armed']=False;self.call(timer['callback'],(handle,timer.get('user',0)))
    def capture(self,path):
        from PIL import Image
        w,h,p=self.frames[self.last_frame];raw=bytes(self.uc.mem_read(p,w*h*2));rgb=bytearray()
        for (v,) in struct.iter_unpack('<H',raw):rgb.extend((((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
        Image.frombytes('RGB',(w,h),bytes(rgb)).save(path)

if __name__=='__main__':
    runtime=Wipi()
    try:
        runtime.boot()
        print('BOOT RETURNED',runtime.timers)
        end=time.perf_counter()+float(sys.argv[1] if len(sys.argv)>1 else 0)
        next_press=time.perf_counter()+1
        while time.perf_counter()<end:
            if len(sys.argv)>2 and time.perf_counter()>=next_press:
                runtime.call('handleCletEvent',(502,53,0));next_press+=2
            runtime.tick();time.sleep(.001)
        if runtime.flushes:runtime.capture(ROOT/'scratch/first_frame.png')
        print('FLUSHES',runtime.flushes)
    finally:
        (ROOT/'scratch/boot_trace.json').write_text(json.dumps({'seen':runtime.seen,'calls':runtime.trace},indent=2))

