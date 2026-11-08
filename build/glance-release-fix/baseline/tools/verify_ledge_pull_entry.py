"""Read-only validation of ledge gesture animation assumptions in installed PDPs.
Usage: python tools/verify_ledge_pull_entry.py <game-directory>
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
    state, = struct.unpack_from("<h", tables["anims"], 96 * 40 + 6)
    first, last, _, _, count, start, _, _ = struct.unpack_from("<8h", tables["anims"], 96 * 40 + 24)
    assert state == 10, (path, "ledge hang state")
    assert 0 < (last - first + 1) / 30 < .9, (path, "request must cover hang loop")
    found = False
    for index in range(start, start + count):
        goal, ranges, offset = struct.unpack_from("<3h", tables["changes"], index * 6)
        if goal != 19:
            continue
        for index in range(offset, offset + ranges):
            low, high, dest, frame = struct.unpack_from("<4h", tables["ranges"], index * 8)
            dest_state, = struct.unpack_from("<h", tables["anims"], dest * 40 + 6)
            base, end = struct.unpack_from("<2h", tables["anims"], dest * 40 + 24)
            assert first <= low <= high <= last + 1 and base <= frame <= end, (path, "pull-up range")
            assert dest_state == 19, (path, "native pull-up destination")
            found = True
    assert found, (path, "no native ledge mount transition")
    return True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    for game in ("4", "5"):
        paths = sorted((Path(sys.argv[1]) / game / "DATA").glob("*.PDP"))
        assert paths, (game, "missing PDP files")
        count = sum(verify(path) for path in paths)
        assert count, (game, "no Lara tables")
        print(f"TR{game}: {count} tables verified: ledge hang 10/96, native mount 19, bounded request covers loop")
