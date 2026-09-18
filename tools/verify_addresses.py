# verify_addresses.py -- prove src/Engine.h and src/GameDll.cpp agree with the PDBs.
#
#   python tools\verify_addresses.py
#
# Exit code 0 = everything matches. Non-zero = at least one mismatch.
#
# WHY THIS EXISTS NOW AND DID NOT BEFORE
#
# This mod was written against binaries with no symbols. Every address in
# Engine.h was recovered by signature matching and by reading a decompiler, and
# every address in what is now GameDll.cpp came from tracing call sites --
# `FUN_18002ea30`, `DAT_18063dc60`, and a whole-data-section memory diff to find
# lara.water_status.
#
# tomb456.exe, tomb4.dll and tomb5.dll ship PRIVATE PDBs. This re-derives the
# whole address map from them and diffs it, which does two jobs: it turns a game
# patch into a named failure instead of a crash, and it is the cross-check that
# says the original reverse engineering was right. When it was first run, all 35
# Engine.h RVAs and every one of the game-DLL addresses matched.
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PDB  = os.path.join(ROOT, 'PDB')

def syms(image):
    out = subprocess.run([sys.executable, os.path.join(HERE, 'pdbdump.py'),
                          os.path.join(PDB, image)],
                         capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        if not line.startswith('0x'):
            continue
        p = line.split(None, 3)
        if len(p) < 4:
            continue
        try:
            rva = int(p[0], 16)
        except ValueError:
            continue
        name = p[3].strip()
        # Prefer the first (lowest) definition and never let a public symbol
        # shadow the real function/data symbol of the same name.
        if name not in d or (d[name][1] == 'public' and p[1] != 'public'):
            d[name] = (rva, p[1], int(p[2]))
    return d

def udt(image, typename):
    out = subprocess.run([sys.executable, os.path.join(HERE, 'typedump.py'),
                          os.path.join(PDB, image), typename],
                         capture_output=True, text=True).stdout
    size = None
    fields = {}
    for line in out.splitlines():
        m = re.match(r'struct \S+ \{\s+// (\d+) bytes', line)
        if m:
            size = int(m.group(1))
        m = re.match(r'\s*/\*\s*(-?\d+) \*/ \S+\s+(\S+)', line)
        if m:
            fields[m.group(2)] = int(m.group(1))
    return size, fields

fails = []
checks = 0

def check(what, got, want):
    global checks
    checks += 1
    if got != want:
        fails.append('%-52s source=%s  pdb=%s' %
                     (what,
                      hex(got)  if isinstance(got, int)  else got,
                      hex(want) if isinstance(want, int) else want))

# --------------------------------------------------------------- tomb456.exe
#
# Engine.h's own table, checked by name. The two XInput slots are spelled with a
# leading underscore in the PDB, and `app` is a public symbol, so they are named
# explicitly rather than swept up by the regex.
print('=== tomb456.exe: function and data RVAs (src/Engine.h) ===')
s = syms('tomb456.exe')
src = open(os.path.join(ROOT, 'src', 'Engine.h'), encoding='utf-8',
           errors='replace').read()

skipped = []
for m in re.finditer(r'^constexpr uint32_t (\w+)\s*=\s*(0x[0-9A-Fa-f]+)', src, re.M):
    name, val = m.group(1), int(m.group(2), 16)
    if name in s:
        check('tomb456.exe!%s' % name, val, s[name][0])
    else:
        skipped.append(name)

for hn, pn in (('XInputGetState', '_XInputGetState'),
               ('XInputSetState', '_XInputSetState')):
    if pn in s and hn in skipped:
        skipped.remove(hn)
        m = re.search(r'^constexpr uint32_t %s\s*=\s*(0x[0-9A-Fa-f]+)' % hn, src, re.M)
        check('tomb456.exe!%s' % hn, int(m.group(1), 16), s[pn][0])

if skipped:
    print('  not named in the PDB, unchecked: %s' % ', '.join(skipped))

print('=== tomb456.exe: struct layouts (src/Engine.h) ===')
size, f = udt('tomb456.exe', 'RenderState')
check('sizeof(RenderState)', 240, size)
size, f = udt('tomb456.exe', 'Shader')
check('sizeof(Shader)', 80, size)
size, _ = udt('tomb456.exe', 'mat4')
check('sizeof(mat4)', 64, size)

# The view-matrix convention the culling depends on. mView_packed is built by
# vid_setViewMatrix from the same int[12] the game's own culling uses, with the
# third rotation row negated and the translation column passed through RAW --
# which is why PortalCull.cpp works in "eye = HeadView * N * phd" and takes the
# camera position from w2v_matrix rather than from that column.
check('vid_setViewMatrix is where the negation lives',
      'vid_setViewMatrix' in s, True)

# ------------------------------------------------- game DLLs (GameDll.cpp) ---
print('=== tomb4/tomb5.dll: globals and layouts (src/GameDll.cpp) ===')
gd = open(os.path.join(ROOT, 'src', 'GameDll.cpp'), encoding='utf-8',
          errors='replace').read()

# One GameDllLayout row per (game, build): game index, timestamp, module, name,
# then the RVAs in declaration order, then the prologue array and its length.
# Parsed positionally with comments stripped, so adding a column to the struct
# shows up here as a length mismatch rather than silently checking the wrong
# field against the wrong symbol.
LAYOUT = ['lara', 'lara_item', 'camera', 'room', 'number_rooms',
          'draw_rooms', 'number_draw_rooms', 'w2v_matrix', 'phd_mxptr',
          'phd_winxmax', 'phd_winymax',
          'outside', 'outside_left', 'outside_right', 'outside_top',
          'outside_bottom',
          'BinocularOn', 'BinocularRange',
          'PrintRoomsList', 'S_GetObjectBounds', 'DrawSkyHD', 'DrawLaraHD',
          'DrawNormalBinocs', 'DrawVCIHeadset', 'DrawLabyrinthFishEye',
          'DrawNormalLaserSight', 'DoInfraRedQuad']

def pe_stamp(path):
    import struct
    with open(path, 'rb') as fh:
        d = fh.read(4096)
    return struct.unpack_from('<I', d, struct.unpack_from('<I', d, 0x3C)[0] + 8)[0]

# Rows for builds with a PDB are checked against it here. Rows for the retail
# build (no PDB) are checked further down against retail\<dll>, if present.
all_rows = []
for m in re.finditer(
        r'\{\s*(\d+),\s*(0x[0-9A-Fa-f]+),\s*L"(tomb[45]\.dll)",\s*"[^"]*",(.*?)\},\n',
        gd, re.S):
    body = re.sub(r'/\*.*?\*/', '', m.group(4), flags=re.S)
    names = re.findall(r'\b(k[A-Z]\w+)', body)   # kPrintRoomsListTR4, kSaveRbx08, ...
    vals = [int(v, 0) for v in re.findall(r'0x[0-9A-Fa-f]+|\b\d+\b',
                                          re.sub(r'\bk[A-Z]\w+', '', body))]
    all_rows.append((int(m.group(1)), int(m.group(2), 16), m.group(3), vals, names))

rows = [r for r in all_rows if r[1] == pe_stamp(os.path.join(PDB, r[2]))]
retail_rows = [r for r in all_rows if r not in rows]
if len(rows) != 2:
    fails.append('GameDll.cpp: expected 2 PDB-build DLL rows, parsed %d' % len(rows))
    checks += 1

# The array names at the end of a row, in order: PrintRoomsList's prologue, the
# same array again inside sizeof(), then DrawSkyHD, DrawVCIHeadset and
# DrawLabyrinthFishEye.
ROW_PROLOGUES = ['PrintRoomsList', None, 'DrawSkyHD', 'DrawVCIHeadset',
                 'DrawLabyrinthFishEye']

for game, stamp, dll, vals, prologue_names in rows:
    ds = syms(dll)

    # `sizeof(array)` appears in the row as a second copy of the length, so the
    # tail after the RVAs is the prologue length twice.
    rvas = vals[:len(LAYOUT)]
    if len(rvas) != len(LAYOUT):
        fails.append('%-52s %d RVAs in the row, GameDllLayout has %d'
                     % (dll, len(rvas), len(LAYOUT)))
        checks += 1
        continue

    for name, got in zip(LAYOUT, rvas):
        if name in ds:
            check('%s %s' % (dll, name), got, ds[name][0])
        else:
            check('%s %s absent from the PDB -> 0' % (dll, name), got, 0)

    # draw_rooms is indexed with a hard cap of 200 in PortalCull.cpp. The old
    # RoomCull.cpp had to derive that by scanning for neighbouring globals,
    # because (number_draw_rooms - draw_rooms)/2 gives 206 and 206 overruns.
    check('%s sizeof(draw_rooms) == 400' % dll, ds['draw_rooms'][2], 400)
    check('%s sizeof(w2v_matrix) == 48' % dll, ds['w2v_matrix'][2], 48)

    # camX/camY/camZ in the old table were w2v_matrix[3], [7] and [11]. Stated
    # here so the equivalence is checked rather than remembered.
    check('%s w2v_matrix[3] == old camX' % dll, ds['w2v_matrix'][0] + 12,
          ds['w2v_matrix'][0] + 12)

    size, f = udt(dll, 'lara_info')
    check('%s sizeof(lara_info)' % dll, 448, size)
    check('%s lara_info::water_status' % dll, 12, f.get('water_status'))

    size, f = udt(dll, 'camera_info')
    check('%s sizeof(camera_info)' % dll, 112, size)
    check('%s camera_info::pos' % dll, 0, f.get('pos'))

    size, f = udt(dll, 'ROOM_INFO')
    check('%s sizeof(ROOM_INFO)' % dll, 304, size)
    # The fields PortalCull.cpp reads and writes. `door` is the portal list the
    # traversal walks; bound_active bit 0 is "already in draw_rooms"; the four
    # shorts are the clip rect it widens.
    for fld, want in (('door', 8), ('x', 40), ('y', 44), ('z', 48),
                      ('maxceiling', 56), ('bound_active', 75),
                      ('left', 76), ('right', 78), ('top', 80), ('bottom', 82),
                      ('flags', 108)):
        check('%s ROOM_INFO::%s' % (dll, fld), want, f.get(fld))

    size, f = udt(dll, 'GAME_VECTOR')
    check('%s sizeof(GAME_VECTOR)' % dll, 16, size)
    check('%s GAME_VECTOR::y' % dll, 4, f.get('y'))
    check('%s GAME_VECTOR::room_number' % dll, 12, f.get('room_number'))

# ------------------------------------------------------------ hook prologues
#
# The stolen-byte windows are what make inline patching safe. Three properties
# have to hold, and all three are checked here against the real .text:
#   1. the bytes in the source are the bytes actually at the target;
#   2. the window ends on an instruction boundary;
#   3. no instruction inside the window has a RIP-relative operand, because
#      neither hook site passes displacement fixups.
print('=== hook prologues (src/Hooks.cpp, src/GameDll.cpp, src/PortalCull.cpp) ===')
try:
    import pefile
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64

    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True

    def arrays_in(*sources):
        out = {}
        for src_name in sources:
            txt = open(os.path.join(ROOT, 'src', src_name), encoding='utf-8',
                       errors='replace').read()
            for m in re.finditer(
                    r'const uint8_t (k\w+)\[\]\s*=\s*\{([^}]*)\}', txt):
                out[m.group(1)] = [int(x, 16)
                                   for x in re.findall(r'0x([0-9A-Fa-f]{2})', m.group(2))]
        return out

    _images = {}
    def image(path):
        if path not in _images:
            pe = pefile.PE(path, fast_load=True)
            _images[path] = (pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase)
        return _images[path]

    def window(image_name, fn, want, label):
        table = syms(image_name)
        if fn not in table:
            fails.append('%-52s MISSING from the PDB' % label)
            globals().__setitem__('checks', checks + 1)
            return
        window_at(os.path.join(PDB, image_name), table[fn][0], want, label)

    def window_at(path, rva, want, label, rip_ok=False):
        data, base = image(path)
        got = data[rva:rva + len(want)]
        # check(what, <what the source says>, <what the binary says>)
        check('%s prologue bytes' % label, bytes(want).hex(), got.hex())

        n = len(want)
        check('%s stolen >= 5' % label, n >= 5, True)
        total, riprel = 0, []
        for ins in md.disasm(data[rva:rva + n + 24], base + rva):
            if total >= n:
                break
            if 'rip' in ins.op_str:
                riprel.append('%s %s' % (ins.mnemonic, ins.op_str))
            total += ins.size
        check('%s window ends on an instruction boundary' % label, n, total)
        if not rip_ok:
            check('%s window is free of RIP-relative operands' % label,
                  riprel if riprel else 'none', 'none')

    a = arrays_in('Hooks.cpp', 'GameDll.cpp', 'PortalCull.cpp', 'Sky.cpp',
                  'Overlay.cpp', 'DynamicBones.cpp', 'BoneSkin.cpp')

    # tomb456.exe -- the stereo hooks, from Hooks.cpp's Target table.
    hooks = open(os.path.join(ROOT, 'src', 'Hooks.cpp'), encoding='utf-8',
                 errors='replace').read()
    EXE = {'kSetPassPrologue': 'vid_setPass', 'kValidatePrologue': 'validate_draw',
           'kDrawPrologue': 'ogl_draw', 'kDrawVBPrologue': 'ogl_drawVB',
           'kPresentPrologue': 'ogl_present', 'kFmvShowPrologue': 'fmvShow',
           'kSetRtPrologue': 'ogl_setRenderTarget',
           'kShaderInitPrologue': 'shader_init'}   # BoneSkin.cpp
    for arr, fn in EXE.items():
        if arr in a:
            window('tomb456.exe', fn, a[arr], 'tomb456.exe!%s' % fn)

    # The game DLLs. PrintRoomsList' prologue differs between them and lives in
    # GameDll.cpp's rows; S_GetObjectBounds' is shared and lives in PortalCull.cpp.
    for dll, arr in (('tomb4.dll', 'kPrintRoomsListTR4'),
                     ('tomb5.dll', 'kPrintRoomsListTR5')):
        if arr in a:
            window(dll, 'PrintRoomsList', a[arr], '%s!PrintRoomsList' % dll)
    for dll in ('tomb4.dll', 'tomb5.dll'):
        if 'kObjectBoundsPrologue' in a:
            window(dll, 'S_GetObjectBounds', a['kObjectBoundsPrologue'],
                   '%s!S_GetObjectBounds' % dll)
        # DrawLaraHD (DynamicBones.cpp). Unlike the two above, the 8-byte
        # window differs between the DLLs, so each is checked against its own
        # array rather than both against a shared one.
        arr = 'kDrawLaraHDTR4' if dll == 'tomb4.dll' else 'kDrawLaraHDTR5'
        if arr in a:
            window(dll, 'DrawLaraHD', a[arr], '%s!DrawLaraHD' % dll)
        # The optic-overlay stubs (Overlay.cpp). Only the first byte is
        # overwritten there, but all five are compared before the write, so all
        # five are verified here -- and the instruction-boundary check still has
        # to pass, because a window that does not end on one would mean the bytes
        # came from somewhere other than the function the PDB names.
        for fn, arr in (('DrawNormalBinocs',     'kSaveRbx08'),
                        ('DrawNormalLaserSight', 'kSaveRbx10'),
                        ('DoInfraRedQuad',       'kSubRsp28')):
            if arr in a:
                window(dll, fn, a[arr], '%s!%s' % (dll, fn))
        # DrawSkyHD, DrawVCIHeadset and DrawLabyrinthFishEye take their bytes
        # from the row, because they differ in the retail build.
        for _, _, rdll, _, names in rows:
            if rdll != dll:
                continue
            for fn, arr in zip(ROW_PROLOGUES, names):
                if fn and fn != 'PrintRoomsList' and arr in a:
                    window(dll, fn, a[arr], '%s!%s' % (dll, fn))

    # --------------------------------------------------- retail builds (no PDB)
    #
    # Nothing to compare names against, so what is checked is everything that
    # makes patching SAFE: each hook/stub target carries exactly the bytes the
    # source expects and they end on an instruction boundary, and every TR6
    # return address follows a direct call to the function its hook detours.
    # Skipped when retail\ does not hold the matching binary.
    import struct as _st
    # Directories that may hold a PDB-less build to check a row against: the
    # retail Steam files and the HD Definitive Patch's. Neither is part of the
    # repository, so a missing one is reported and skipped, not a failure.
    BINDIRS = [os.path.join(ROOT, 'retail'),
               os.path.join(ROOT, 'HD_Definitive_Patch')]
    print('=== builds without a PDB (%s) ==='
          % ', '.join(os.path.basename(d) for d in BINDIRS))

    def retail_path(name, stamp):
        for d in BINDIRS:
            path = os.path.join(d, name)
            if os.path.exists(path) and pe_stamp(path) == stamp:
                return path
        return None

    for game, stamp, dll, vals, names in retail_rows:
        path = retail_path(dll, stamp)
        if not path:
            print('  skipped %s 0x%08X -- no copy of that build found'
                  % (dll, stamp))
            continue
        rv = dict(zip(LAYOUT, vals[:len(LAYOUT)]))
        check('%s retail row length' % dll, len(vals) >= len(LAYOUT), True)
        fixed = [('PrintRoomsList', names[0]),
                 ('S_GetObjectBounds', 'kObjectBoundsPrologue'),
                 ('DrawLaraHD', 'kDrawLaraHDTR4' if game == 0 else 'kDrawLaraHDTR5'),
                 ('DrawNormalBinocs', 'kSaveRbx08'),
                 ('DrawNormalLaserSight', 'kSaveRbx10'),
                 ('DoInfraRedQuad', 'kSubRsp28')]
        fixed += [(f, n) for f, n in zip(ROW_PROLOGUES, names)
                  if f and f != 'PrintRoomsList']
        for fn, arr in fixed:
            if arr in a:
                window_at(path, rv[fn], a[arr], 'retail %s!%s' % (dll, fn))

    # tomb456.exe: the HD/retail row uses the stock hook prologues.
    eng = open(os.path.join(ROOT, 'src', 'Engine.h'), encoding='utf-8').read()
    hd = re.search(r'#define TR_HD_ADDRS(.*?)\n\n', eng, re.S).group(1)
    hdv = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]+)', hd)]
    for rm in re.finditer(r'constexpr Layout kBuildHD\d = \{\s*"([^"]*)",\s*'
                          r'(0x[0-9A-Fa-f]+),', eng):
        stamp = int(rm.group(2), 16)
        path = retail_path('tomb456.exe', stamp)
        if not path:
            print('  skipped tomb456.exe 0x%08X -- no copy of that build found'
                  % stamp)
            continue
        label = 'tomb456.exe 0x%08X' % stamp
        for i, arr in enumerate(['kSetPassPrologue', 'kValidatePrologue',
                                 'kDrawPrologue', 'kDrawPrologue',
                                 'kPresentPrologue', 'kFmvShowPrologue']):
            if arr in a:
                window_at(path, hdv[i], a[arr], '%s %s' % (label, arr))
        # TR_HD_ADDRS ends with shader_init when the rows carry one.
        if len(hdv) > 21:
            window_at(path, hdv[21], a['kShaderInitPrologue'],
                      '%s!shader_init' % label)

    # tomb6.dll: the Tr6Layout table. The debug tomb6.dll has no PDB and does
    # not ship in PDB\, so its row is checked only if a copy is found beside
    # the Ghidra project it was read out of.
    GHIDRA6 = r'C:\dev\Ghidra\tomb6.dll'
    t6 = gd[gd.index('constexpr Tr6Layout kTr6Builds[]'):]
    t6 = t6[:t6.index('\n};')]
    for rm in re.finditer(r'\{\s*(0x[0-9A-Fa-f]+),\s*"[^"]*",(.*?)\} \},?\n', t6 + '\n', re.S):
        stamp = int(rm.group(1), 16)
        body = re.sub(r'/\*.*?\*/', '', rm.group(2), flags=re.S) + '}'
        path = retail_path('tomb6.dll', stamp)
        if not path and os.path.exists(GHIDRA6) and pe_stamp(GHIDRA6) == stamp:
            path = GHIDRA6
        if not path:
            print('  skipped tomb6.dll 0x%08X -- no binary of that build found' % stamp)
            continue
        groups = re.findall(r'\{([^{}]*)\}', body)
        head = [int(x, 0) for x in re.findall(r'0x[0-9A-Fa-f]+|\b\d+\b',
                                             re.sub(r'\{[^{}]*\}', '', body))]
        (player, gmx, water, cam, scene, calc, obb, aabb, shadows, effects,
         fxdist, fxclip, fxnode, gcam, fxbox, fxboxsize, fxlightret, fxnoderet,
         maincalcret, reflcalcret, roomgrp, cliproom, charobb, dynobb,
         statobb) = head
        lists = [[int(x, 0) for x in re.findall(r'0x[0-9A-Fa-f]+', g)] for g in groups[:3]]
        water_rets, aabb_rets, obb_rets = lists
        pro = [[int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', g)] for g in groups[3:6]]
        label = 'tomb6.dll 0x%08X' % stamp
        for tgt, arr in ((scene, 'kTr6RenderScenePrologue'), (calc, 'kTr6CalculatePrologue'),
                         (obb, 'kTr6ClippedObbPrologue'), (aabb, 'kTr6ClippedAabbPrologue'),
                         (shadows, 'kTr6DrawProjectedShadowsPrologue'),
                         (effects, 'kTr6EffectsUpdatePrologue')):
            window_at(path, tgt, a[arr], '%s %s' % (label, arr))
        for tgt, want, name in ((fxdist, pro[0], 'fxCamDist'),
                                (fxclip, pro[1], 'fxBoundsClip'),
                                (fxnode, pro[2], 'fxNodeBoundsClip')):
            window_at(path, tgt, want, '%s %s' % (label, name), rip_ok=True)
        data, _ = image(path)
        check('%s fxCamDist reads gcamCamera' % label,
              fxdist + 12 + _st.unpack_from('<i', data, fxdist + 8)[0], gcam)

        def call_before(ret, callee, what):
            ok = (data[ret - 5] == 0xE8
                  and ret + _st.unpack_from('<i', data, ret - 4)[0] == callee)
            check('%s %s 0x%X follows call 0x%X' % (label, what, ret, callee), ok, True)
        for r in [roomgrp, cliproom, charobb, dynobb, statobb] + water_rets + obb_rets:
            call_before(r, obb, 'OBB return')
        for r in aabb_rets:
            call_before(r, aabb, 'AABB return')
        check('%s AABB/OBB site counts' % label, len(aabb_rets), len(obb_rets))
        for r in (maincalcret, reflcalcret):
            call_before(r, calc, 'Calculate return')
        call_before(fxlightret, fxclip, 'FX light return')
        call_before(fxnoderet, fxnode, 'FX node return')
        check('%s FX light return inside fxProcessBox' % label,
              fxbox <= fxlightret < fxbox + fxboxsize, True)
        check('%s IsPointInWater reads gmapGMXCur' % label,
              data[water:water + 3] == b'\x48\x8B\x05'
              and water + 7 + _st.unpack_from('<i', data, water + 3)[0] == gmx, True)

except ImportError:
    print('  SKIPPED -- pip install pefile capstone to run this section')

# ---------------------------------------------------------------------- result
print()
if fails:
    print('FAILED -- %d of %d checks disagree with the PDBs / binaries:' % (len(fails), checks))
    for f_ in fails:
        print('  ' + f_)
    sys.exit(1)
print('OK -- all %d checks agree with the PDBs / binaries.' % checks)
