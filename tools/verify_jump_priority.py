"""Read-only verification of native TR4/5 immediate jump dispatch windows.
Usage: python tools/verify_jump_priority.py <game-directory>
"""
import struct
import sys
from pathlib import Path


def verify(path):
    data = path.read_bytes()
    tables, cursor = {}, 0
    for name, size in (("anims", 40), ("changes", 6), ("ranges", 8), ("commands", 2)):
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
                if clip == 0:
                    assert dest in (16, 18) and frame == base, (path, "running jump enters full run-up")
                covered.update(range(low, high + 1))
        required = {first + 1} if clip == 11 else set(range(first + 1, last + 2))
        assert required <= covered, (path, clip, "jump dispatch window")
        if clip == 11:
            assert last + 1 not in covered, (path, "final standing frame needs restart")
    for clip in (0, 16, 18):
        speed, acceleration = struct.unpack_from("<2i", tables["anims"], clip * 40 + 8)
        assert (speed, acceleration) == (47 << 16, 0), (path, clip, "constant run-up speed")
        if clip:
            _, first, last, _, _, _, _, count, start = animation(clip)
            assert last - first + 1 == 7 and count == 5, (path, clip, "seven grounded run-up frames")
            commands = struct.unpack_from("<15h", tables["commands"], start * 2)
            assert commands[:3] == (2, -100, 75), (path, clip, "native end-of-animation takeoff")
            assert all(commands[i] in (5, 6) for i in (3, 6, 9, 12)), (path, clip, "remaining commands are sound/effects")
    state, first, last, dest, df, count, start, nc, cmd = animation(73)
    assert (state, last-first, dest) == (15, 10, 91) and nc in (2,4), (path, "compression metadata")
    for index in range(nc):
        opcode, _, _ = struct.unpack_from('<3h', tables['commands'], (cmd+index*3)*2)
        assert opcode in (5,6), (path, "compression only has sound/footstep effects")
    dispatch = {}
    for index in range(start, start+count):
        goal, nr, ri = struct.unpack_from("<3h", tables["changes"], index*6)
        dispatch[goal] = [struct.unpack_from("<4h", tables["ranges"], k*8) for k in range(ri, ri+nr)]
    for goal, clip, next_clip in ((3,76,77), (25,74,75), (27,78,79), (26,80,81), (28,91,28)):
        state, base, end, next_id, next_frame, _, _, nc, cmd = animation(clip)
        assert (state, end-base, next_id, nc) == (goal, 2, next_clip, 2), (path, clip, "takeoff metadata")
        assert animation(next_clip)[0] == goal and animation(next_clip)[1] == next_frame
        commands = struct.unpack_from("<6h", tables["commands"], cmd*2)
        assert commands[:3] == ((2,-110,2) if goal==28 else (2,-100,50)) and commands[3]==5
        if goal!=28:
            assert dispatch[goal] == [(first+8,last+1,clip,base)], (path, goal, "native direction dispatch")
        else:
            assert df==base, (path, "default vertical jump")
    return True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    for game in ("4", "5"):
        paths = sorted((Path(sys.argv[1]) / game / "DATA").glob("*.PDP"))
        assert paths, (game, "missing PDP files")
        count = sum(verify(path) for path in paths)
        assert count, (game, "no Lara tables")
        print(f"TR{game}: {count} tables verified: idle/run dispatch, compression directions, takeoff commands and airborne clips")
