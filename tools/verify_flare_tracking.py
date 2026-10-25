"""Verify scoped flare hooks and wrist/light calls in all four supported DLLs."""
from pathlib import Path
import struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

root = Path(__file__).resolve().parents[1]
source = (root / "src/FirstPerson.cpp").read_text()
builds = (
    ("PDB/tomb4.dll", 0x696B4999, 0x5F7F0, 0xA3920, 0xA37C0, 0x5F490, 0xAD150),
    ("PDB/tomb5.dll", 0x696B499C, 0x5C9D0, 0x9CB20, 0x9C9C0, 0x5C670, 0xD0890),
    ("retail/tomb4.dll", 0x68C12FDA, 0x5F500, 0xA44C0, 0xA4360, 0x5F1A0, 0xADD00),
    ("retail/tomb5.dll", 0x68C12FE9, 0x5D700, 0x9CE90, 0x9CD30, 0x5D3A0, 0xD0B30),
)
decoder = Cs(CS_ARCH_X86, CS_MODE_64)
for name, stamp, hand, previous, joint, light, dynamic in builds:
    pe = pefile.PE(str(root / name))
    data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp
    assert f"{{0x{stamp:X},0x{hand:X},0x{previous:X},0x{dynamic:X}}}" in source
    for rva, code in ((hand, "40534883ec40"), (previous, "48895c2408"), (dynamic, "48895c2408")):
        prefix = bytes.fromhex(code)
        assert data[rva:rva + len(prefix)] == prefix, (name, "hook prologue")
        instructions = list(decoder.disasm(prefix, rva))
        assert sum(i.size for i in instructions) == len(prefix)
        assert all("rip" not in i.op_str and not i.mnemonic.startswith("j") for i in instructions)
    # Both games query current and previous LEFT wrist (13) with the same HD
    # tip offset (11,-64,41), then call the original light routine with hand=1.
    instructions = list(decoder.disasm(data[hand:hand + 0x91], hand))
    calls = []
    for ins in instructions:
        if ins.mnemonic == "call" and data[ins.address] == 0xE8:
            calls.append(ins.address + 5 + struct.unpack_from("<i", data, ins.address + 1)[0])
    assert calls == [joint, previous, light], (name, calls)
    operands = "\n".join(i.op_str for i in instructions)
    for expected in ("ecx, 0xffffffc0", "eax, 0x20", "r8d, 0xd", "r9d, 1"):
        assert expected in operands, (name, expected)
    assert "r8d, [rcx + 0x4d]" in operands or operands.count("r8d, 0xd") == 2
    assert "0xb" in operands and "0x29" in operands
    light_code=list(decoder.disasm(data[light:light+862],light))
    calls=[int(i.op_str,16) for i in light_code if i.mnemonic=="call"]
    assert calls==[dynamic], (name,calls)
    dynamic_code="\n".join(i.op_str for i in decoder.disasm(data[dynamic:dynamic+190],dynamic))
    for operand in ("[rdx + 4], ebx", "[rdx], ebx", "[rdx + 0xc], edi", "[rdx + 8], edi",
                    "[rdx + 0x14], esi", "[rdx + 0x10], esi", "[rdx + 0x18], 1", "rax, rdx"):
        assert operand in dynamic_code,(name,operand)
    print(f"{name}: hook boundaries, wrist/light calls and returned dynamic-light XYZ layout verified")
