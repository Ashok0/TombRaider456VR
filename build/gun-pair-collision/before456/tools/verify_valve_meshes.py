"""Read-only wall-valve rim contacts and native completion-marker checks."""
from pathlib import Path
import math,sys
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from capstone.x86 import X86_OP_IMM
from verify_revolver_laser_mesh import vertices
root=Path(sys.argv[1]);repo=Path(__file__).resolve().parents[1]
for name,cx,z in [('AIRLOCK_SWITCH_ANDREA2.TRM',3.,-83.),('AIRLOCK_SWITCH_JOBY3_JOBY5.TRM',0.,-82.)]:
    points=list(vertices(root/'5/ITEM'/name))
    for k in range(32):
        angle=k*2*math.pi/32
        ring=[p for p in points if math.hypot(p[0]-cx,p[1]-6.4)>90 and
              abs(math.remainder(math.atan2(p[1]-6.4,p[0]-cx)-angle,2*math.pi))<math.pi/12]
        assert ring,(name,k)
        radii=[math.hypot(p[0]-cx,p[1]-6.4) for p in ring]
        assert min(radii)<112<max(radii),(name,k,min(radii),max(radii))
        assert min(p[2] for p in ring)<z<max(p[2] for p in ring)
    print('PASS: grip centre line inside visible rim at 32 angles:',name)
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
for path,stamp,control,trigger,animate in [
    (repo/'PDB/tomb5.dll',0x696B499C,0x8A770,0x8A5C0,0xF8D0),
    (root/'5/tomb5.dll',0x68C12FE9,0x8AA80,0x8A8E0,0xF9C0)]:
    pe=pefile.PE(str(path));assert pe.FILE_HEADER.TimeDateStamp==stamp
    code=list(md.disasm(pe.get_data(control,535),control))
    immediates={o.imm for i in code for o in i.operands if o.type==X86_OP_IMM}
    calls={i.operands[0].imm for i in code if i.mnemonic=='call' and i.operands[0].type==X86_OP_IMM}
    assert {274,8,0x3e00}<=immediates and {trigger,animate}<=calls
    assert any(i.mnemonic=='dec' and i.op_str=='edx' for i in code)
    assert any(i.mnemonic=='xor' and '0x2c]' in i.op_str for i in code)
    print('PASS: native airlock completion marker and linked-item toggle:',hex(stamp))
