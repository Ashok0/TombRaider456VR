from pathlib import Path
import hashlib,json
root=Path(r'C:\dev\TombRaider123VR');out=Path(__file__).parent
records=json.loads((out/'123-source-manifest.json').read_text())
for row in records:
    path=root/row['path']
    assert (hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None)==row['sha256'],path
for row in records:(root/row['path']).write_bytes((out/'123-staged'/row['path']).read_bytes())
print('Applied backed-up TR1-3 Classic sky hook changes.')
