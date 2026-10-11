"""Verify physical-switch entry points and data references against local game DLLs.
Usage: python tools/verify_physical_switch_binaries.py <game directory>
Requires pefile and capstone. Reads game files without modifying them.
"""
import pathlib
import re
import sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_REG_RIP

root = pathlib.Path(sys.argv[1])
source = pathlib.Path(__file__).resolve().parents[1] / "src" / "PhysicalSwitch.inl"
rows = re.findall(r"\{(0x[0-9A-Fa-f]+(?:,0x[0-9A-Fa-f]+){13})\}", source.read_text())
assert len(rows) == 4
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
for n, row in enumerate(rows):
    stamp, collision, test, move, bounds, get_bounds, items, count, animate, calc, changes, ranges, commands, clip = [int(x, 16) for x in row.split(",")]
    game = 4 + n % 2
    path = source.parents[1] / "PDB" / f"tomb{game}.dll" if n < 2 else root / str(game) / f"tomb{game}.dll"
    pe = pefile.PE(str(path))
    assert pe.FILE_HEADER.TimeDateStamp == stamp
    signatures = {test: "40 55 41 55 41 57", move: "48 89 5c 24 18",
                  collision: "48 89 5c 24 " + ("10" if game == 4 else "08"),
                  animate: "48 8b c4 53 55 57 41 56",
                  calc: "40 53 55 56 57 41 54 41" if game == 4 else "48 89 4c 24 08 53 55 56"}
    for address, signature in signatures.items():
        expected = bytes.fromhex(signature)
        assert pe.get_data(address, len(expected)) == expected, (path, hex(address))

    def refs(address, size):
        code = list(md.disasm(pe.get_data(address, size), address))
        memory = {i.address + i.size + o.mem.disp for i in code for o in i.operands
                  if o.type == X86_OP_MEM and o.mem.base == X86_REG_RIP}
        calls = {i.operands[0].imm for i in code if i.mnemonic == "call" and i.operands[0].type == X86_OP_IMM}
        return memory, calls

    memory, calls = refs(collision, 953 if game == 4 else 1125)
    assert {test, move, get_bounds, animate} <= calls
    assert {items, bounds} <= memory
    memory, _ = refs(calc, 2541 if game == 4 else 2708)
    assert {clip, clip + 16, clip + 20, clip + 36} <= memory
    memory, calls = refs(animate, 1540 if game == 4 else 1685)
    assert commands in memory
    # AnimateItem calls GetChange; verify the transition tables belong to it.
    assert any({changes, ranges} <= refs(call, 120)[0] for call in calls)
    print(f"PASS: {path}: hook/pose signatures, native calls, transition/command tables and viewport state")
