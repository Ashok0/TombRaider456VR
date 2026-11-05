"""Audit shipped material passes and extract unchanged native shaders, without loading game code."""
from pathlib import Path
import struct
import pefile

root=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider IV-VI Remastered')
for name,expected in [('LARA_HEAD',(27345,2508,2040)),('OUTFIT_VCI',(87813,12177,0))]:
    data=(root/f'4/ITEM/{name}.TRM').read_bytes()
    assert struct.unpack_from('<I',data)[0]==0x024d5254
    chunks=struct.unpack_from('<I',data,4)[0]
    counts=tuple(sum(struct.unpack_from('<I',data,8+c*44+24+p*8)[0] for c in range(chunks)) for p in range(3))
    assert counts==expected,(name,counts)
    print(f'PASS {name}: native opaque/transparent/additive index counts {counts}')

reference=pefile.PE(r'C:\dev\PDB\tomb456.exe',fast_load=True)
memory=reference.get_memory_mapped_image()
retail=(root/'tomb456.exe').read_bytes()
symbols={'NativeCreatureVertex':0x83538,'NativeCreatureFragment':0x83578,
         'NativeEnvVertex':0x834B0,'NativeEnvFragment':0x833E8}
fixture=['// Extracted from native shader sources; exact bytes also verified in installed retail EXE.']
for name,rva in symbols.items():
    ptr=struct.unpack_from('<Q',memory,rva)[0]-reference.OPTIONAL_HEADER.ImageBase
    source=memory[ptr:memory.index(b'\0',ptr)]
    assert source.startswith(b'#version 150') and source in retail
    fixture.append(f'const char {name}[]=R"MIRROR({source.decode("ascii")})MIRROR";')
Path('build/mirror/native-shaders.h').write_text('\n'.join(fixture)+'\n')
print('PASS native creature and catsuit environment shader sources match installed retail')
