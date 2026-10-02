"""Check every opt-in motion-gun hook and firing return address against DLLs.

Run: python tools/verify_motion_guns.py
Requires pefile (already used by the other address tools).
"""
from pathlib import Path
import struct
import re
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
aim_builds = {
    "PDB/tomb4.dll": (0x5E2F0,0x149F0,0x158B8),
    "PDB/tomb5.dll": (0x5B5D0,0x12450,0x13346),
    "retail/tomb4.dll": (0x5DFF0,0x145D0,0x15498),
    "retail/tomb5.dll": (0x5C320,0x12500,0x133F6),
}
lara_gun_builds = {
    "PDB/tomb4.dll": 0x5CF80,
    "PDB/tomb5.dll": 0x5A2A0,
    "retail/tomb4.dll": 0x5CC80,
    "retail/tomb5.dll": 0x5AFF0,
}

root = Path(__file__).resolve().parents[1]
game_source = (root / "src/GameDll.cpp").read_text()
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
    gun=lara_gun_builds[path]
    setup=list(decoder.disasm(data[gun:gun+190],gun))
    assert any(i.mnemonic=="cmp" and "rax + r10 + 0x9ec" in i.op_str
               for i in setup), (path,"native per-control draw setting")
    assert any(i.mnemonic=="shr" and i.op_str=="rax, 1" for i in setup)
    assert any(i.mnemonic=="and" and i.op_str=="eax, 1" for i in setup)
    assert data[get_joints:get_joints+5] == bytes.fromhex("44 89 44 24 18"), path
    assert data[fire:fire+5] == bytes.fromhex("4c 89 44 24 18"), path
    pistol_bytes = "48 89 5c 24 18" if "tomb4" in path else "48 89 5c 24 10"
    assert data[pistol:pistol+5] == bytes.fromhex(pistol_bytes), path
    los_bytes = "40 55 56 57 41 56" if "tomb4" in path else "40 55 53 57 41 55"
    assert data[los:los+6] == bytes.fromhex(los_bytes), path
    assert call_target(data, view_ret) == w2v, (path, "shot view")
    assert call_target(data, right_ret) == fire, (path, "right gun")
    assert call_target(data, left_ret) == fire, (path, "left gun")
    # Verify identities from native ammo dispatch, independently of the C++
    # routing tests. PDB lara_inv fields: pistols +0x190, revolver +0x194,
    # Uzis +0x192. The latter two were previously reversed in motion input.
    lara_match = re.search(
        rf'0x{stamp:08X}, L"tomb[45]\.dll", "[^"]+",\s*'
        r'/\* lara\s*\*/\s*(0x[0-9A-F]+)', game_source)
    assert lara_match, (path, "production Lara address")
    lara = int(lara_match[1], 16)
    early_calls = [i for i in decoder.disasm(data[fire:fire+100], fire)
                   if i.mnemonic == "call"]
    assert len(early_calls) == 2, (path, "joint then ammo lookup")
    ammo = int(early_calls[1].op_str, 16)
    for weapon, field in ((1, 0x190), (2, 0x194), (3, 0x192)):
        pc, value, zero, result = ammo, weapon, False, None
        for _ in range(20):
            ins = next(decoder.disasm(data[pc:pc+15], pc))
            pc += ins.size
            if ins.mnemonic in ("sub", "cmp"):
                assert ins.op_str.startswith("ecx, "), (path, "ammo selector")
                comparison = value - ins.operands[1].imm
                zero = comparison == 0
                if ins.mnemonic == "sub":
                    value = comparison
            elif ins.mnemonic == "je":
                if zero:
                    pc = ins.operands[0].imm
            elif ins.mnemonic == "lea":
                assert ins.op_str.startswith("rax, [rip"), (path, "ammo field")
                result = pc + ins.disp
            elif ins.mnemonic == "ret":
                break
            else:
                raise AssertionError((path, weapon, "unexpected ammo path", ins.op_str))
        assert result == lara + field, (path, weapon, "native gun ID / ammo field")
    # Weapon 2 skips the right call and instead uses the shared left-arm call
    # with a RIGHT-hand flash. Weapon 3 uses both calls and matching flashes.
    branch = list(decoder.disasm(data[right_ret-56:right_ret-40], right_ret-56))
    assert branch[0].mnemonic == "cmp" and branch[0].op_str == "r14d, 2"
    assert branch[1].mnemonic == "je" and branch[1].operands[0].imm > right_ret
    effects = list(decoder.disasm(data[left_ret:left_ret+140], left_ret))
    assert any(i.mnemonic == "cmp" and i.op_str == "r14d, 2" for i in effects)
    flash_fields = [i.address+i.size+i.disp for i in effects
                    if i.mnemonic == "mov" and i.op_str.startswith("word ptr [rip")]
    assert flash_fields[:2] == [lara+0x11c, lara+0x104], (path, "single/dual flash counters")
    for ret in (right_ret,left_ret):
        ins=list(decoder.disasm(data[ret:ret+12],ret))
        assert ins[0].mnemonic=="test" and ins[0].op_str=="eax, eax", (path,"shot result test")
        assert ins[1].mnemonic=="je" and int(ins[1].op_str,16)>ret+64, (path,"zero suppresses shot effects")
    assert call_target(data, hit_ret) == los, (path, "target hit LOS")
    assert call_target(data, miss_ret) == los, (path, "wall impact LOS")
    target_point, sight, sight_return = aim_builds[path]
    helper_prefix=bytes.fromhex("48 89 5c 24 08 48 89 6c 24 10 48 89 74 24 18")
    assert data[target_point:target_point+15]==helper_prefix, (path,"target point")
    assert data[sight:sight+15]==helper_prefix, (path,"LOS helper")
    assert call_target(data,sight_return)==sight, (path,"native visibility call")
    # The target helper writes room at +12 in addition to XYZ; never use a
    # twelve-byte PHD_VECTOR for this call. Confirm all four implementations.
    assert bytes.fromhex("66 41 89 47 0c") in data[target_point:target_point+269], (path,"target room")
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
    print(f"{path}: hook bytes, native weapon IDs, both shot sites and single/dual flashes OK")

print("motion-gun addresses: all four builds verified")

# Parse the actual C++ extension table so stale test-only addresses cannot
# accidentally validate an incorrect production row.
source = (root / "src/FirstPerson.cpp").read_text()
table = source.split("constexpr LongGunDll kLongGunDlls[] = {", 1)[1].split("\n};", 1)[0]
rows = re.findall(r"\{(0x[0-9A-F]+),([^{}]+),\s*\{([^}]+)\}\}", table)
assert len(rows) == 4
for build, row in zip(builds, rows):
    path, stamp, _, fire, _, _, _, _, _, los, _, _ = build
    row_stamp, numbers, optic_returns = row
    assert int(row_stamp, 16) == stamp
    rifle, shotgun, special, crossbow, joint, initialise, items = [
        int(n, 16) for n in numbers.split(",")]
    pe = pefile.PE(str(root / path))
    data = pe.get_memory_mapped_image()
    tr4 = "tomb4" in path
    prefixes = [
        (rifle, "40 53 57 48 81 ec 98 00 00 00" if tr4 else "40 53 56 57 41 56"),
        (shotgun, "48 89 5c 24 18"),
        (special, "41 55 48 81 ec b0 00 00 00" if tr4 else "89 4c 24 08 48 83 ec 38"),
        (crossbow, "40 57 48 83 ec 60" if tr4 else "48 89 7c 24 18"),
        (joint, "48 89 5c 24 08"),
        (initialise, "48 89 5c 24 08"),
    ]
    for address, prefix in prefixes:
        expected = bytes.fromhex(prefix)
        assert data[address:address+len(expected)] == expected, (path, hex(address), "hook bytes")
        ins = list(decoder.disasm(expected, address))
        assert sum(i.size for i in ins) == len(expected), (path, "whole instructions")
        assert not any("rip" in i.op_str or i.mnemonic.startswith("j") for i in ins)
    for ret in [int(n,16) for n in optic_returns.split(",")]:
        assert call_target(data, ret) == los, (path, "optic call")
    def calls(address, size, target):
        return [i for i in decoder.disasm(data[address:address+size], address)
                if i.mnemonic == "call" and i.op_str == hex(target)]
    assert len(calls(shotgun,1602,fire)) == 6, (path, "six-pellet shotgun")
    assert len(calls(shotgun,1602,joint)) == 2, (path, "shotgun smoke origin/direction")
    if tr4:
        assert len(calls(special,983,joint)) == 2, (path, "grenade muzzle/direction")
        assert len(calls(special,983,initialise)) == 1, (path, "grenade initialization")
        assert len(calls(crossbow,682,joint)) == 1, (path, "normal bolt muzzle")
        assert len(calls(crossbow,682,initialise)) == 2, (path, "normal/optic bolt initialization")
    else:
        assert len(calls(special,276,fire)) == 1, (path, "HK native shot")
        assert len(calls(crossbow,333,initialise)) == 1, (path, "grapple initialization")
    assert any("qword ptr [rip" in i.op_str and i.address+i.size+i.disp == items
               for i in decoder.disasm(data[crossbow:crossbow+(682 if tr4 else 333)],crossbow)), (
                   path, "projectile item table")
    print(f"{path}: all-weapon hooks, six pellets, launchers and optic calls OK")
# Controller acquisition and animation dependencies, independently checked in
# each shipped image. RVAs must also match the production MotionDll row.
source = (root / 'src/FirstPerson.cpp').read_text()
for path, stamp, joints, fire, view_ret, right_ret, left_ret, w2v, pistol, *_ in builds:
    data = pefile.PE(str(root / path)).get_memory_mapped_image()
    table = source.split('constexpr MotionDll kMotionDlls[] = {', 1)[1].split('\n};', 1)[0]
    row = next(re.findall(r'0x[0-9A-Fa-f]+', r) for r in re.findall(r'\{([^{}]+)\}', table)
               if int(re.findall(r'0x[0-9A-Fa-f]+', r)[0],16) == stamp)
    values = [int(v,16) for v in row]
    animate, spheres, active, items = values[-4:]
    assert data[animate:animate+5] == bytes.fromhex('48 89 5c 24 10'), path
    assert data[spheres:spheres+5] == bytes.fromhex('44 89 44 24 18'), path
    firing = list(decoder.disasm(data[fire:fire+450], fire))
    assert any(i.mnemonic == 'call' and i.operands[0].imm == spheres for i in firing), (path, 'native sphere helper')
    animation = list(decoder.disasm(data[animate:animate+1884], animate))
    lara_match = re.search(rf'0x{stamp:08X}, L"tomb[45]\.dll", "[^"]+",\s*'
                          r'/\* lara\s*\*/\s*(0x[0-9A-F]+)', game_source)
    lara = int(lara_match[1],16)
    assert any('rip' in i.op_str and i.address+i.size+i.disp == lara+208 for i in animation), (path,'native target field')
    handler = list(decoder.disasm(data[pistol:pistol+1100],pistol))
    assert any(i.mnemonic == 'call' and i.operands[0].imm == animate for i in handler), (path,'native pistol animation')
    # Expected symbols/mapped data, verified from PDB and independent structural
    # mapping (TR4 active-list 22 votes, TR5 18; GetSpheres exact function match).
    expected = {
        0x696B4999:(0x5A680,0xA3C30,0x63DAB0,0x699CA0),
        0x696B499C:(0x579C0,0x9CF90,0x65AC92,0x66D1A8),
        0x68C12FDA:(0x5A380,0xA47D0,0x63E9F0,0x69ABE0),
        0x68C12FE9:(0x58710,0x9D300,0x65ABD2,0x66D0E8),
    }
    assert (animate,spheres,active,items) == expected[stamp], path
    print(f'{path}: controller target acquisition and AnimatePistols verified')
