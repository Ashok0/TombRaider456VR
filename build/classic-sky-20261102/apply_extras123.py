from pathlib import Path
import hashlib,json
root=Path(r'C:\dev\TombRaider123VR');out=Path(__file__).parent
records=json.loads((out/'123-extra-manifest.json').read_text())
for row in records:
    p=root/row['path'];assert (hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None)==row['sha256'],p
for row in records:(root/row['path']).write_bytes((out/'123-extra-staged'/row['path']).read_bytes())
print('Updated TR1-3 documentation and focused sky regression.')
