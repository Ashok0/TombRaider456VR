from pathlib import Path
import hashlib
root = Path(r'C:\dev\TombRaider123VR')
stage = Path(__file__).parent
target = root / 'README.md'
original = target.read_bytes()
old = b'with opaque black wrist caps. The arms remain hidden during post-climb body and\r\ncamera recovery, then return when recovery finishes. Normal hanging, shimmying,\r\nand TR3 monkey-bar traversal retain their arms.'
new = b'with opaque black wrist caps. Complete arms return on the first standing or\r\nground-movement frame, even while body, camera and wrist recovery continue.\r\nThe torso retains its recovery protection. Normal hanging, shimmying,\r\nand TR3 monkey-bar traversal retain their arms.'
assert original.count(old) == 1
(stage / 'README123-before-final-doc-update.md').write_bytes(original)
result = original.replace(old, new)
(stage / 'README123-final.md').write_bytes(result)
(stage / 'README123-final.sha256').write_text(hashlib.sha256(original).hexdigest())
print('Final TR1-3 documentation update staged.')
