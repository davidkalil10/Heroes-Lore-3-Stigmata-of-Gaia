"""Validate NRO identity and exact embedded game/language payloads; no gameplay claim."""
import argparse,struct,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('nro');a=p.parse_args()
raw=Path(a.nro).read_bytes()
assert len(raw)>0x80 and raw[0x10:0x14]==b'NRO0','Not a Switch NRO'
native_size=struct.unpack_from('<I',raw,0x18)[0]
assert native_size<len(raw) and raw[native_size:native_size+4]==b'ASET','Missing NRO assets'
icon_offset,icon_size=struct.unpack_from('<QQ',raw,native_size+8)
assert raw[native_size+icon_offset:native_size+icon_offset+icon_size]==(ROOT/'build/runtime/hl4-icon.jpg').read_bytes(),'Missing/stale NRO icon'
nacp_offset,nacp_size=struct.unpack_from('<QQ',raw,native_size+24)
assert nacp_size==0x4000,'NACP size'
nacp=raw[native_size+nacp_offset:native_size+nacp_offset+nacp_size]
title=int(json.loads((ROOT/'project.json').read_text())['identifiers']['switch_title_id'],16)
for offset in [0x3038,0x3078,0x30b0]:assert struct.unpack_from('<Q',nacp,offset)[0]==title,'Stale NACP title ID'
for name in ['hl4.dat','hl4.lang']:
    assert (ROOT/'build/runtime'/name).read_bytes() in raw[native_size:],'Missing/stale embedded payload '+name
print('PASS Switch NRO and exact embedded runtime/language data; hardware execution pending')
