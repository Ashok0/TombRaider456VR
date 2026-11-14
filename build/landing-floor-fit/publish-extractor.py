from pathlib import Path
p=Path('build/landing-floor-fit/extract-poses.py');s=p.read_text()
s='''"""Extract native standing/landing skeletons for the first-person renderer tests.
Usage: python tools/extract_landing_poses.py <TR4-6 directory> <TR1-3 directory> <output.inl>
Reads installed PDP assets only. TR1 uses fixed 40-word frames with a joint
count and low/high packed rotations; TR2-5 use variable compact rotations.
No game code is executed and no installed assets are modified.
"""\n'''+s
s=s.replace('import math,struct,json','import math,struct,sys')
s=s.replace("root=Path('C:/Program Files (x86)/Steam/steamapps/common/Tomb Raider IV-VI Remastered')", "if len(sys.argv)!=4:raise SystemExit(__doc__)\nroot=Path(sys.argv[1]);root123=Path(sys.argv[2]);output=Path(sys.argv[3])")
s=s.replace("root.parent/'Tomb Raider I-III Remastered'",'root123')
s=s.replace("        if clip==103:print('standing format',game,rate,size,struct.unpack_from('<12h',data,t[5][0]+a[0]))\n",'')
s=s.replace('            cases.append(dict(', '            if game==1:assert words[9]==15\n            cases.append(dict(')
s=s.replace("Path('build/landing-floor-fit/native-poses.inl').write_text(code)","output.write_text(code,encoding='utf-8')")
s=s.replace("Path('build/landing-floor-fit/native-poses.json').write_text(json.dumps(cases))\n",'')
Path('tools/extract_landing_poses.py').write_text(s)
