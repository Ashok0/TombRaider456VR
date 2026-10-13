"""Check the landing camera whitelist against installed TR4/5 animation tables.

Usage: python tools/verify_landing_camera.py <game-directory>
Reads PDP files only; covers grounded clip states, native jump/fall exits and
the level-dependent variation in animation 99 (landing versus death).
"""
import struct
import sys
from pathlib import Path


def verify(path, game):
    data = path.read_bytes()
    tables, cursor = {}, 0
    for name, size in [("anims", 40), ("changes", 6), ("ranges", 8)]:
        count, = struct.unpack_from("<I", data, cursor)
        cursor += 4
        end = cursor + count * size
        assert end <= len(data), (path, name, "table bounds")
        tables[name] = data[cursor:end]
        cursor = end
    if len(tables["anims"]) < 104 * 40:
        return False

    def anim(index):
        record = tables["anims"][index * 40:(index + 1) * 40]
        return struct.unpack_from("<h", record, 6)[0], *struct.unpack_from("<8h", record, 24)

    expected = {24: 2, 31: 2, 82: 2, 92: 1}
    for index, state in expected.items():
        assert anim(index)[0] == state, (path, index, "landing/death state")
    state99 = anim(99)[0]
    assert state99 in ((2,) if game == "4" else (2, 8)), (path, 99, "landing/death variant")
    assert anim(99)[3] == (82 if state99 == 2 else 99), (path, 99, "variant exit")
    for index, target in [(24, 11), (31, 11), (82, 11), (92, 0)]:
        assert anim(index)[3] == target, (path, index, "return to normal gait")

    def destinations(source, goal_state):
        _, _, _, _, _, count, start, _, _ = anim(source)
        result = set()
        for index in range(start, start + count):
            goal, ranges, start_range = struct.unpack_from("<3h", tables["changes"], index * 6)
            if goal != goal_state:
                continue
            for index in range(start_range, start_range + ranges):
                _, _, dest, _ = struct.unpack_from("<4h", tables["ranges"], index * 8)
                result.add(dest)
        return result

    # Native lara_control_routines[6/7] are lara_as_turn_r/l. Merely checking
    # clip 13's state (7) previously rubber-stamped it as a landing. Verify its
    # actual entry, self-loop and fast-turn exit instead: no airborne transition.
    for clip, state in ((12, 6), (13, 7), (44, 20), (69, 20)):
        assert anim(clip)[0] == state and anim(clip)[3] == clip, (path, clip, "turn loop")
    for clip, state, fast in ((12, 6, 44), (13, 7, 69)):
        assert clip in destinations(11, state), (path, clip, "idle to turn")
        assert fast in destinations(clip, 20), (path, clip, "turn to fast turn")

    # These airborne clips choose their grounded destinations through native
    # goal-state ranges. Inspect the transition tables, not guessed names.
    for source, target in [(23, 24), (28, 31), (75, 82), (77, 92)]:
        _, _, _, _, _, count, start, _, _ = anim(source)
        destinations = set()
        for index in range(start, start + count):
            goal, count, start_range = struct.unpack_from("<3h", tables["changes"], index * 6)
            if goal not in (1, 2):
                continue
            for index in range(start_range, start_range + count):
                _, _, dest, _ = struct.unpack_from("<4h", tables["ranges"], index * 8)
                destinations.add(dest)
        assert target in destinations, (path, source, target, "native landing transition")
    return True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    root = Path(sys.argv[1])
    for game in ("4", "5"):
        paths = sorted((root / game / "DATA").glob("*.PDP"))
        assert paths, (game, "no PDP files")
        count = sum(verify(path, game) for path in paths)
        assert count, (game, "no Lara animation tables")
        print(f"TR{game}: {count} landing animation tables verified")
