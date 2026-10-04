"""Verify native midair half-turn animations in installed TR4/5 PDP files.

Usage: python tools/verify_jump_roll.py <game-directory>
Reads game data only; checks the animation whitelist used by NativeRollTurn.
"""
import struct
import sys
from pathlib import Path


def verify(path):
    data = path.read_bytes()
    tables, cursor = {}, 0
    for name, size in [("anims", 40), ("changes", 6), ("ranges", 8), ("commands", 2)]:
        count, = struct.unpack_from("<I", data, cursor)
        cursor += 4
        end = cursor + count * size
        assert end <= len(data), (path, name, "table bounds")
        tables[name] = data[cursor:end]
        cursor = end
    if len(tables["anims"]) < 214 * 40:
        return False
    for index, state, exit_state in [(207, 3, 25), (210, 3, 25), (212, 25, 3)]:
        record = tables["anims"][index * 40:(index + 1) * 40]
        assert struct.unpack_from("<h", record, 6)[0] == state, (path, index, "state")
        first, last, dest, _, _, _, count, command = struct.unpack_from("<8h", record, 24)
        assert struct.unpack_from("<h", tables["anims"], dest * 40 + 6)[0] == exit_state
        offset, turns = command * 2, []
        for _ in range(count):
            opcode, = struct.unpack_from("<h", tables["commands"], offset)
            offset += 2
            argc = {1: 3, 2: 2, 3: 0, 4: 0, 5: 2, 6: 2}[opcode]
            args = struct.unpack_from("<" + "h" * argc, tables["commands"], offset)
            offset += argc * 2
            if opcode == 6 and args[1] & 0x3fff == 0:
                turns.append(args[0])
        assert len(turns) == 1 and first <= turns[0] <= last, (path, index, "native half-turn")
    return True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    root = Path(sys.argv[1])
    for game in ("4", "5"):
        paths = sorted((root / game / "DATA").glob("*.PDP"))
        assert paths, (game, "no PDP files")
        count = sum(verify(path) for path in paths)
        assert count, (game, "no Lara animations")
        print(f"TR{game}: {count} tables verified: forward/back jump flips each turn once")
