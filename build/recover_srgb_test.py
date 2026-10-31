from pathlib import Path
import json,re
root=Path('C:/dev/TombRaider456VR')
session=Path('C:/Users/Ashok/.codex/sessions/2026/10/25/rollout-2026-10-25T01-07-29-01a17174-e960-7df2-9174-8cdf0eefabbc.jsonl')
entry=json.loads(session.read_text(encoding='utf-8').splitlines()[2440])
code=entry['payload']['input']
patch=json.loads(re.search(r'apply_patch\(("(?:\\.|[^"\\])*")\)',code).group(1))
for name in ['tools/stereo_color_gl_tests.cpp','build/test_stereo_color.cmd']:
 marker='*** Add File: C:/dev/TombRaider456VR/'+name+'\n'
 text=patch.split(marker,1)[1].split('*** ',1)[0]
 content='\n'.join(line[1:] for line in text.splitlines() if line.startswith('+'))+'\n'
 (root/name).write_text(content)
print('Recovered the earlier isolated GPU regression, without running the game self-test.')
