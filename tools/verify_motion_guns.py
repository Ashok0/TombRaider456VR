"""Check every opt-in motion-gun hook and firing return address against DLLs.

Run: python tools/verify_motion_guns.py
Requires pefile (already used by the other address tools).
"""
from pathlib import Path
import struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

flash_builds = {
    "PDB/tomb4.dll": (0x8EDD0,0x1B6D18,0x2F763,0x2F819),
    "PDB/tomb5.dll": (0x8D790,0x1B2AC8,0x306A3,0x30756),
    "retail/tomb4.dll": (0x8F7E0,0x1B7D18,0x2F5D3,0x2F689),
    "retail/tomb5.dll": (0x8DAA0,0x1B2AC8,0x30B23,0x30BD6),
}
decoder = Cs(CS_ARCH_X86, CS_MODE_64)
decoder.detail = True

root = Path(__file__).resolve().parents[1]
# path, stamp, GetJoints, FireWeapon, FireWeapon->W2V return,
# right/left AnimatePistols->FireWeapon returns, phd_GenerateW2V,
# PistolHandler (decouples the camera from arm aim), GetTargetOnLOS and
# both FireWeapon->GetTargetOnLOS return sites.
builds = [
    ("PDB/tomb4.dll", 0x696B4999, 0xC09E0, 0x5E4B0, 0x5E5C4,
     0x5A97F, 0x5AC2A, 0xD50B0, 0x5A270, 0x15860, 0x5E701, 0x5E79B),
    ("PDB/tomb5.dll", 0x696B499C, 0xB5880, 0x5B790, 0x5B8A4,
     0x57CBF, 0x57F6A, 0xA1170, 0x576A0, 0x132F0, 0x5BA0C, 0x5BAA3),
    ("retail/tomb4.dll", 0x68C12FDA, 0xC1440, 0x5E1B0, 0x5E2C6,
     0x5A67F, 0x5A92A, 0xD5BD0, 0x59F70, 0x15440, 0x5E401, 0x5E49B),
    ("retail/tomb5.dll", 0x68C12FE9, 0xB5A00, 0x5C4E0, 0x5C5F6,
     0x58A0F, 0x58CBA, 0xA14D0, 0x583F0, 0x133A0, 0x5C759, 0x5C7F0),
]

def call_target(data, return_rva):
    start = return_rva - 5
    assert data[start] == 0xE8, f"expected relative call at {start:#x}"
    return return_rva + struct.unpack_from("<i", data, start + 1)[0]

for (path, stamp, get_joints, fire, view_ret, right_ret, left_ret,
     w2v, pistol, los, hit_ret, miss_ret) in builds:
    pe = pefile.PE(str(root / path))
    data = pe.get_memory_mapped_image()
    assert pe.FILE_HEADER.TimeDateStamp == stamp, path
    assert data[get_joints:get_joints+5] == bytes.fromhex("44 89 44 24 18"), path
    assert data[fire:fire+5] == bytes.fromhex("4c 89 44 24 18"), path
    pistol_bytes = "48 89 5c 24 18" if "tomb4" in path else "48 89 5c 24 10"
    assert data[pistol:pistol+5] == bytes.fromhex(pistol_bytes), path
    los_bytes = "40 55 56 57 41 56" if "tomb4" in path else "40 55 53 57 41 55"
    assert data[los:los+6] == bytes.fromhex(los_bytes), path
    assert call_target(data, view_ret) == w2v, (path, "shot view")
    assert call_target(data, right_ret) == fire, (path, "right gun")
    assert call_target(data, left_ret) == fire, (path, "left gun")
    assert call_target(data, hit_ret) == los, (path, "target hit LOS")
    assert call_target(data, miss_ret) == los, (path, "wall impact LOS")
    flash, fmx, right_flash, left_flash = flash_builds[path]
    prologue = bytes.fromhex("4c 8b dc 48 81 ec 98 00 00 00" if "tomb4" in path
                             else "48 81 ec a8 00 00 00")
    assert data[flash:flash+len(prologue)] == prologue, (path,"flash prologue")
    instructions = list(decoder.disasm(prologue,flash))
    assert sum(i.size for i in instructions) == len(prologue), "whole instructions"
    assert call_target(data,right_flash) == flash, (path,"right flash")
    assert call_target(data,left_flash) == flash, (path,"left flash")
    assert any("qword ptr [rip" in i.op_str and
               i.address+i.size+i.disp == fmx
               for i in decoder.disasm(data[flash:flash+340],flash)), (path,"float stack")
    print(f"{path}: hook bytes and both shot sites OK")

print("motion-gun addresses: all four builds verified")
