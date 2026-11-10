"""Constant specialization of arm_hot_decoder.inc, without template expansion.

Unusual instructions retain the decoder template. Keep load/writeback and flag
ordering identical to that decoder; Unicorn remains the independent oracle.
"""

def emit(pc,ins,update_pc=True,count=True):
    cc=ins>>28;kind=(ins>>25)&7;rn=(ins>>16)&15;rd=(ins>>12)&15
    reg=lambda n:hex(pc+8)+'u' if n==15 else f'r[{n}]'
    body=None
    # MUL/MLA, before the halfword encoding overlap.
    if ins&0x0fc000f0==0x00000090:
        d=(ins>>16)&15;a=(ins>>12)&15;rs=(ins>>8)&15;rm=ins&15
        body=f'u32 value={reg(rm)}*{reg(rs)};'
        if ins&(1<<21):body+=f'value+={reg(a)};'
        body+=f'r[{d}]=value;'
        if ins&(1<<20):body+='N=value>>31;Z=value==0;'
    elif kind in (2,3) and not(kind==3 and ins&0x10) and rd!=15:
        offset=hex(ins&0xfff)+'u'
        if kind==3:offset=f'shiftImm({reg(ins&15)},{(ins>>5)&3},{(ins>>7)&31},ignored)'
        body=f'bool ignored=C;u32 base={reg(rn)};u32 offset={offset};u32 eff=base'+('+' if ins&(1<<23) else '-')+'offset;'
        body+='u32 addr='+('eff' if ins&(1<<24) else 'base')+';'
        load=bool(ins&(1<<20));byte=bool(ins&(1<<22))
        wb=f'r[{rn}]=eff;' if not ins&(1<<24) or ins&(1<<21) else ''
        if load:body+=('u32 value=load8(addr);' if byte else 'u32 value=ror32(load32(addr&~3u),(addr&3)*8);')+wb+f'r[{rd}]=value;'
        else:body+=(f'store8(addr,(u8){reg(rd)});' if byte else f'store32(addr&~3u,{reg(rd)});')+wb
    elif kind==0 and ins&0x90==0x90 and ins&0x60 and rd!=15:
        # Exclude multiply/DSP encodings, already handled by the decoder before
        # transfers. Standard transfers have the same field layout below.
        sh=(ins>>5)&3;load=bool(ins&(1<<20))
        if load or sh==1:
            offset=hex(((ins>>4)&0xf0)|(ins&15))+'u' if ins&(1<<22) else reg(ins&15)
            body=f'u32 base={reg(rn)};u32 offset={offset};u32 eff=base'+('+' if ins&(1<<23) else '-')+'offset;'
            body+='u32 addr='+('eff' if ins&(1<<24) else 'base')+';'
            wb=f'r[{rn}]=eff;' if not ins&(1<<24) or ins&(1<<21) else ''
            if load:
                expr={1:'load16(addr)',2:'(u32)(s32)(int8_t)load8(addr)',3:'(u32)(s32)(int16_t)load16(addr)'}[sh]
                body+=f'u32 value={expr};'+wb+f'r[{rd}]=value;'
            else:body+=f'store16(addr,(u16){reg(rd)});'+wb
    elif kind in (0,1):
        special=(kind==0 and (ins&0x90==0x90 or ins&0x01900000==0x01000000)) or (kind==1 and ins&0x01900000==0x01000000)
        if not special and rd!=15 and not(kind==0 and ins&0x10 and (ins>>8)&15==15):
            op=(ins>>21)&15;setf=bool(ins&(1<<20)) or op in (8,9,10,11)
            body='bool sc=C;'
            if kind==1:
                rot=((ins>>8)&15)*2;value=ins&255
                value=((value>>rot)|(value<<(32-rot)))&0xffffffff if rot else value
                body+=f'u32 rhs=0x{value:x}u;'
                if rot:body+=f'sc={value>>31};'
            else:
                rm=ins&15;typ=(ins>>5)&3
                if ins&0x10:
                    value=hex(pc+12)+'u' if rm==15 else reg(rm)
                    body+=f'u32 rhs=shiftReg({value},{typ},r[{(ins>>8)&15}],sc);'
                else:body+=f'u32 rhs=shiftImm({reg(rm)},{typ},{(ins>>7)&31},sc);'
            value=reg(rn)
            if kind==0 and ins&0x10 and rn==15:value=hex(pc+12)+'u'
            body+=f'u32 lhs={value};'
            f='true' if setf else 'false'
            expr={0:'lhs&rhs',1:'lhs^rhs',2:f'addc(lhs,~rhs,1,{f})',3:f'addc(rhs,~lhs,1,{f})',
                  4:f'addc(lhs,rhs,0,{f})',5:f'addc(lhs,rhs,C,{f})',6:f'addc(lhs,~rhs,C,{f})',
                  7:f'addc(rhs,~lhs,C,{f})',8:'lhs&rhs',9:'lhs^rhs',10:'addc(lhs,~rhs,1,true)',
                  11:'addc(lhs,rhs,0,true)',12:'lhs|rhs',13:'rhs',14:'lhs&~rhs',15:'~rhs'}[op]
            body+='u32 value='+expr+';'
            if setf:
                body+='N=value>>31;Z=value==0;'
                if op in (0,1,8,9,12,13,14,15):body+='C=sc;'
            if op not in (8,9,10,11):body+=f'r[{rd}]=value;'
    if body is None:return f'step<0x{pc:x},0x{ins:x}u>();'
    if cc!=14:body=f'if(cond({cc})){{'+body+'}'
    return '{'+('++icount;' if count else '')+body+(f'r[15]=0x{pc+4:x}u;' if update_pc else '')+'}'
