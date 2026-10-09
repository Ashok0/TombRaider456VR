"""Read-only verification of native TR4/5 immediate jump dispatch windows.
Usage: python tools/verify_jump_priority.py <game-directory>
"""
import struct
import sys
from pathlib import Path


def verify(path):
    data = path.read_bytes()
    tables, cursor = {}, 0
    for name, size in (("anims", 40), ("changes", 6), ("ranges", 8)):
        count, = struct.unpack_from("<I", data, cursor)
        cursor += 4
        end = cursor + count * size
        assert end <= len(data), (path, name, "bounds")
        tables[name] = data[cursor:end]
        cursor = end
    if len(tables["anims"]) < 104 * 40:
        return False

    def animation(index):
        return (struct.unpack_from("<h", tables["anims"], index * 40 + 6)[0],
                *struct.unpack_from("<8h", tables["anims"], index * 40 + 24))

    for clip, target in ((11, 15), (103, 15), (0, 3)):
        state, first, last, _, _, count, start, commands, _ = animation(clip)
        assert state == (1 if clip == 0 else 2), (path, clip, "source state")
        if clip == 11:
            assert first < last and commands == 0, (path, "safe standing entry")
        covered = set()
        for index in range(start, start + count):
            goal, ranges, offset = struct.unpack_from("<3h", tables["changes"], index * 6)
            if goal != target:
                continue
            for index in range(offset, offset + ranges):
                low, high, dest, frame = struct.unpack_from("<4h", tables["ranges"], index * 8)
                dest_state, base, end, *_ = animation(dest)
                assert dest_state == target and base <= frame <= end, (path, clip, "jump destination")
                covered.update(range(low, high + 1))
        required = {first + 1} if clip == 11 else set(range(first + 1, last + 2))
        assert required <= covered, (path, clip, "jump dispatch window")
        if clip == 11:
            assert last + 1 not in covered, (path, "final standing frame needs restart")
    return True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    for game in ("4", "5"):
        paths = sorted((Path(sys.argv[1]) / game / "DATA").glob("*.PDP"))
        assert paths, (game, "missing PDP files")
        count = sum(verify(path) for path in paths)
        assert count, (game, "no Lara tables")
        print(f"TR{game}: {count} tables verified: idle entry compression, final-frame hazard, complete idle/run jump windows")
