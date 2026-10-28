"""Read-only audit of costume wrist weights affected by armed body masking.

Usage: python tools/verify_costume_wrists.py <game-directory> [weight-output]
The optional output supplies real costume weights to motion_hand_gl_tests.
"""
from pathlib import Path
import sys
from verify_revolver_laser_mesh import vertices

root = Path(sys.argv[1]) / '4' / 'ITEM'
weights = set()
costumes = sorted(root.glob('OUTFIT_*.TRM'))
assert costumes, 'no installed costumes found'
for path in costumes:
    mixed = removed = 0
    for point, (joints, skin) in vertices(path).items():
        wrist = sum(skin[i] for i in range(3) if joints[i] in (10, 13))
        forearm = sum(skin[i] for i in range(3) if joints[i] in (9, 12))
        visible = sum(skin[i] for i in range(3) if joints[i] not in (10, 13, 14))
        if wrist and forearm:
            assert 0 < visible <= 255
            mixed += 1
            removed += visible < 128
            weights.add(visible)
    assert mixed, f'{path.name}: no weighted wrist seams found'
    print(f'{path.name}: {mixed} mixed wrist vertices; {removed} cut by old 50% mask')
if len(sys.argv) > 2:
    Path(sys.argv[2]).write_text(''.join(f'{value / 255:.9f}\n' for value in sorted(weights)))
print(f'OK: {len(costumes)} costumes, {len(weights)} distinct wrist blend weights')
