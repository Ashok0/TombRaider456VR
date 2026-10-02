"""Audit TR4 effect hooks against stock symbols and installed executable bytes."""
from pathlib import Path
import re, struct, subprocess, sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP
root=Path(__file__).resolve().parent.parent
source=(root/'src/TR4Effects.cpp').read_text()
rows=[tuple(int(n,16) for n in row) for row in re.findall(r'\{(0x[0-9A-Fa-f]+),(0x[0-9A-Fa-f]+),(0x[0-9A-Fa-f]+)\}',source)]
checks=0
def check(ok,label):
    global checks
    checks+=1
    if not ok: raise AssertionError(label)
stock=root/'PDB/tomb456.exe'
retail=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered\tomb456.exe')
symbols={}
for line in subprocess.check_output([sys.executable,str(root/'tools/pdbdump.py'),str(stock)],text=True).splitlines():
    p=line.split(None,3)
    if len(p)==4 and p[0].startswith('0x'): symbols.setdefault(p[3],int(p[0],16))
check(rows[0][1:]==(symbols['vidLoadTexture'],symbols['ogl_texUpdate']),'stock symbols agree')
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
available={}
for path in (stock,retail):
    pe=pefile.PE(str(path));available[pe.FILE_HEADER.TimeDateStamp]=(path,pe)
for stamp,load,update in rows:
    if stamp not in available:
        print(f'SKIP 0x{stamp:08X}: executable unavailable; runtime prologue guards remain enabled')
        continue
    path,pe=available[stamp]
    app=0x5833E0 if stamp==0x696B49A7 else 0x585580
    textures=0xE9ADF30 if stamp==0x696B49A7 else 0xE9B00C0
    for rva,prologue in ((load,bytes.fromhex('40 53 55 56 57')),(update,bytes.fromhex('48 89 5C 24 08'))):
        check(pe.get_data(rva,5)==prologue,'five-byte hook signature')
        ins=list(md.disasm(prologue,rva))
        check(sum(i.size for i in ins)==5,'stolen window contains complete instructions')
        check(all(not (op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP) for i in ins for op in i.operands),'stolen instructions position independent')
    load_ins=list(md.disasm(pe.get_data(load,1100),load))
    refs=[i.address+i.size+op.mem.disp for i in load_ins for op in i.operands if op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP]
    check(app in refs and app+0x218 in refs,'loader resolves resource path and calls native texture update')
    desc=next(i.address+i.size+i.operands[1].mem.disp for i in load_ins if i.mnemonic=='lea' and i.operands[1].type==X86_OP_MEM and i.operands[1].mem.base==X86_REG_RIP)
    for index,w,d in ((4,512,768),(5,512,128),(6,2048,3)):
        check(struct.unpack('<6i',pe.get_data(desc+24*index,24))==(w,w,d,6,9,0),'six-mip BC7 array descriptor')
    update_ins=list(md.disasm(pe.get_data(update,420),update))
    check(any(op.type==X86_OP_MEM and op.mem.disp==textures for i in update_ins for op in i.operands),'uploader indexes engine texture handles')
    # Locate the same resource resolver in each executable, allowing its data RVA to move.
    resolver=pefile.PE(str(stock)).get_data(symbols['appGetFilePath'],48)
    image=pe.get_memory_mapped_image()
    hits=[];start=0
    while True:
        hit=image.find(resolver[7:18],start)
        if hit<0: break
        if image[hit-7:hit-4]==resolver[:3]: hits.append(hit-7)
        start=hit+1
    check(len(hits)==1,'resource resolver uses next-game byte plus ASCII 4')
    print(f'PASS {path.name} 0x{stamp:08X}: load 0x{load:X}, update 0x{update:X}, texDesc 0x{desc:X}')
print(f'PASS: {checks} TR4 native texture-path checks')
