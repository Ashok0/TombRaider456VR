"""Read installed TR5 level data and reproduce the forced-lock holster trap.

Usage: python tools/verify_grapple_animation.py <game-directory>
No game files are modified. Layouts/decisions are from the PDB functions
LoadObjectsAndPartialData_TRX, draw_shotgun, AnimateShotgun, GetChange,
AnimateItem and LaraGun. This is an animation-table regression, not gameplay.
"""
import struct
import sys
from pathlib import Path


def verify(path):
    data = path.read_bytes()
    tables = {}
    cursor = 0
    for name, size in [("anims", 40), ("changes", 6), ("ranges", 8),
                       ("commands", 2), ("bones", 4), ("frames", 2),
                       ("objects", 18)]:
        count, = struct.unpack_from("<I", data, cursor)
        cursor += 4
        end = cursor + count * size
        assert end <= len(data), (path, name, "table bounds")
        tables[name] = data[cursor:end]
        cursor = end

    objects = list(struct.iter_unpack("<IhhIIh", tables["objects"]))
    # draw_shotgun maps TR5 weapon 6 (grapple) to object slot 4.
    base = next(obj[-1] for obj in objects if obj[0] == 4)

    def anim(index):
        record = tables["anims"][index * 40:(index + 1) * 40]
        state, = struct.unpack_from("<h", record, 6)
        return (state, *struct.unpack_from("<8h", record, 24))

    def tick(index, frame, goal):
        frame += 1
        state, first, last, jump, jump_frame, count, change, cmds, cmd = anim(index)
        for n in range(change, change + count):
            target, ranges, start = struct.unpack_from("<3h", tables["changes"], n * 6)
            if target != goal:
                continue
            for r in range(start, start + ranges):
                lo, hi, dest, dest_frame = struct.unpack_from("<4h", tables["ranges"], r * 8)
                if lo <= frame <= hi:
                    index, frame = dest, dest_frame
                    state, first, last, jump, jump_frame, count, change, cmds, cmd = anim(index)
                    break
            break
        if frame > last:
            # The grapple undraw animation's sole command is AC_DEACTIVATE=4.
            if cmds == 1 and struct.unpack_from("<h", tables["commands"], cmd * 2)[0] == 4:
                return index, frame, True
            index, frame = jump, jump_frame
        return index, frame, False

    def cycle(forced_lock):
        index, frame = base, anim(base)[1]
        goal = 0
        for _ in range(120):
            # Native AnimateShotgun state 0, dry ground, no fire input:
            # arm.lock ? goal 2 : goal 4. No synthetic projectile involved.
            if anim(index)[0] == 0:
                goal = 2 if forced_lock else 4
            index, frame, dead = tick(index, frame, goal)
            assert not dead
        ready_state = anim(index)[0]
        for _ in range(240):
            index, frame, dead = tick(index, frame, 3)  # LaraGun holstering
            if dead:
                return ready_state, True
        return ready_state, False

    assert cycle(True) == (2, False), "old forced lock must reproduce holster lockout"
    assert cycle(False) == (0, True), "native unlocked aim must reach normal holster completion"
    print(f"{path.name}: forced lock traps state 2; native lock completes holster without firing")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    verify(Path(sys.argv[1]) / "5" / "DATA" / "RICH3.PDP")
