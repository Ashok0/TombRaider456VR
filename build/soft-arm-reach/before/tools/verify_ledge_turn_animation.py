"""Read-only validation of the pelvis-yaw ledge-camera source in installed PDPs.
Usage: python tools/verify_ledge_turn_animation.py <game-directory>
"""
from pathlib import Path
import math
import struct
import sys


def verify(path):
    data = path.read_bytes()
    cursor, tables = 0, []
    for stride in (40, 6, 8, 2, 4, 2, 18):
        count, = struct.unpack_from('<I', data, cursor)
        cursor += 4
        assert cursor + count * stride <= len(data), (path, 'table bounds')
        tables.append((cursor, count))
        cursor += count * stride
    if tables[0][1] <= 96:
        return False
    if struct.unpack_from('<h', data, tables[0][0] + 48 * 40 + 6)[0] != 139:
        return False
    for clip in (43, 48, 49):
        anim = struct.unpack_from('<Ihhiiii8h', data, tables[0][0] + clip * 40)
        assert anim[2] == 139, (path, clip, 'stop-to-hang state')
        rate, size = anim[1] & 255, (anim[1] & 65535) >> 8
        assert rate > 0 and size >= 11
        angles = []
        for frame in range((anim[8] - anim[7]) // rate + 1):
            offset = tables[5][0] + anim[0] + frame * size * 2
            assert offset + size * 2 <= tables[5][0] + tables[5][1] * 2
            words = struct.unpack_from('<' + str(size) + 'H', data, offset)
            word = words[9]
            axis = word >> 14
            x = y = z = 0
            if axis:
                angle = (word & 16383) * math.tau / 4096
                if axis == 1:
                    x = angle
                elif axis == 2:
                    y = angle
                else:
                    z = angle
            else:
                packed = (word << 16) | words[10]
                x = ((packed >> 20) & 1023) * math.tau / 1024
                y = ((packed >> 10) & 1023) * math.tau / 1024
                z = (packed & 1023) * math.tau / 1024
            # Native YXZ rotation applied to the pelvis right axis.
            right_x = math.cos(y) * math.cos(z) + math.sin(y) * math.sin(x) * math.sin(z)
            right_z = -math.sin(y) * math.cos(z) + math.cos(y) * math.sin(x) * math.sin(z)
            assert math.hypot(right_x, right_z) > .85, (path, clip, 'stable horizontal axis')
            yaw = math.atan2(-right_z, right_x)
            if clip == 49 and yaw < -math.pi / 2:
                yaw += math.tau
            if clip != 49 and yaw > math.pi / 2:
                yaw -= math.tau
            angles.append(yaw * (1 if clip == 49 else -1))
        assert abs(angles[0]) < .1 and abs(angles[-1] - math.pi) < .05, (path, clip, 'half-turn endpoints')
        assert any(.8 < angle < 2.3 for angle in angles), (path, clip, 'turn occurs during animation')
        assert max(abs(b - a) for a, b in zip(angles, angles[1:])) < .35, (path, clip, 'continuous yaw')
    return True


if __name__ == '__main__':
    root = Path(sys.argv[1])
    for game in (4, 5):
        count = sum(verify(path) for path in (root / str(game) / 'DATA').glob('*.PDP'))
        assert count, (game, 'no Lara animation banks')
        print(f'TR{game}: {count} banks verified; clips 43/48 turn left, 49 turns right; stable pelvis axis and continuous half-turns')
