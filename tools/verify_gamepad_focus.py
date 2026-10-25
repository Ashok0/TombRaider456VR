"""Verify native VR background controller polling hooks in stock and retail EXEs."""
from pathlib import Path
import struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

root = Path(__file__).resolve().parents[1]
source = (root / "src/Gamepad.cpp").read_text()
decoder = Cs(CS_ARCH_X86, CS_MODE_64)
decoder.detail = True
for file, stamp, update, messages, active, cookie, xinput in (
    ("PDB/tomb456.exe", 0x696B49A7, 0x5C10, 0x5A70, 0x698668, 0x167080, 0x692858),
    ("retail/tomb456.exe", 0x68C12FEB, 0x5C30, 0x5A90, 0x69A808, 0x169080, 0x6949F8),
):
    pe = pefile.PE(str(root / file))
    data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp
    assert "{" + ",".join(f"0x{x:X}" for x in (stamp, update, messages, active, cookie)) + "}" in source
    for rva in (update, messages):
        prefix = bytes.fromhex("4883ec78488b05") + struct.pack("<i", cookie - rva - 11)
        assert data[rva:rva+11] == prefix
        ins = list(decoder.disasm(prefix, rva))
        assert len(ins) == 2 and sum(i.size for i in ins) == 11
        assert ins[1].disp_offset == 3 and ins[1].address - rva + ins[1].disp_offset == 7
    head = list(decoder.disasm(data[update:update+0x40], update))
    assert head[4].mnemonic == "call" and int(head[4].op_str, 16) == messages
    gate = head[5]
    assert gate.mnemonic == "cmp" and gate.address + gate.size + gate.disp == active
    assert head[6].mnemonic == head[7].mnemonic == "mov"  # Clear stick axes, preserving compare flags.
    assert head[8].mnemonic == "je"
    exit_rva = int(head[8].op_str, 16)
    body = list(decoder.disasm(data[update:exit_rva], update))
    refs = [i for i in body if "rip" in i.op_str and i.address + i.size + i.disp == xinput]
    assert len(refs) == 1 and refs[0].address > gate.address
    # Window messages are processed before our override. No keyboard polling
    # or further message pumping is hidden inside the native pad-only section.
    direct_calls = [int(i.op_str, 16) for i in body if i.mnemonic == "call" and i.op_str.startswith("0x")]
    assert direct_calls.count(messages) == 1
    assert "const int rip=7;" in source
    print(f"{file}: relocated 11-byte hooks, native message pump/focus gate, and subsequent XInput polling verified")
