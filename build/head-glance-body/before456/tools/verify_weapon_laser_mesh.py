"""Check every production laser mesh calibration against installed assets.

Usage: python tools/verify_weapon_laser_mesh.py <game-directory>
Read only; no game assets are changed or distributed.
"""
import re
import sys
import math
from pathlib import Path
from verify_revolver_laser_mesh import vertices, centre

root = Path(sys.argv[1]) / '4' / 'ITEM'
header = (Path(__file__).resolve().parents[1] / 'src' / 'WeaponLaserMesh.h').read_text()
rows = re.findall(r'\{(\d+),(\d+),(-?\d+),\{([^}]+)\},\{([^}]+)\}\}', header)
assert len(rows) == 7
regions = {
    10: ('REVOLVER', (82.5, 83), (40, 44.5), (32, 32)),
    8: ('DEAGLE', (92, 95), (34, 41), (32, 32)),
    14: ('CROSSBOW', (229, 234), (173, 178), (32, 32)),
    15: ('HK', (245, 247), (241, 245), (24, 12)),
    9: ('REVOLVER', None, None, (12, 1)),
    7: ('DEAGLE', None, None, (36, 18)),
    13: ('CROSSBOW', None, None, (24, 8)),
}
def bare_rings(mesh, hand):
    if hand == 9:
        # The inner muzzle ring and the bottom centre of the modelled bore.
        return ([p for p in mesh if abs(math.dist(p, (56.518993,131.135376,52.376949))-3.10894)<.002],
                [p for p in mesh if 118.9<p[1]<119.1 and 49.6<p[2]<49.9])
    if hand == 7:
        # Two bevelled cap rings: use their centres, not the cap normal.
        return tuple([p for p in mesh if p[1]>110 and round(p[1]-.15966*p[2],2)==plane]
                     for plane in (123.74,122.4))
    return tuple([p for p in mesh if p[1]>220 and 28<p[2]<38 and round(p[1]-.04447*p[0],2)==plane]
                 for plane in (243.01,227.75))
for hand, weapon, game, front, rear in rows:
    name, front_range, rear_range, counts = regions[int(hand)]
    expected = [tuple(float(v.rstrip('f')) for v in row.split(',')) for row in (front, rear)]
    for variant in ('BARE', 'GLOVES', 'XRAY'):
        base = vertices(root / f'HAND_{variant}_{name}.TRM')
        bare = int(hand) in (7,9,13)
        if name == 'HK' or bare:
            mesh = base
        else:
            attached = vertices(root / f'HAND_{variant}_{name}_LASER.TRM')
            mesh = {p: skin for p, skin in attached.items() if p not in base}
        rings = bare_rings(mesh,int(hand)) if bare else tuple(
            [p for p in mesh if limits[0]<p[1]<limits[1]] for limits in (front_range,rear_range))
        for ring, count, target in zip(rings, counts, expected):
            assert len(ring) == count, (name, variant, len(ring), count)
            for p in ring:
                joints, weights = mesh[p]
                assert joints[0] == 10 and weights[:3] == bytes((255, 0, 0))
            measured = centre(ring)
            assert all(abs(a-b) < .0001 for a, b in zip(measured, target)), (name, variant, measured, target)
        print(f'OK: {variant} {name} {"bare" if bare else "sight"}: production origin/axis match mesh; full wrist weighting')
