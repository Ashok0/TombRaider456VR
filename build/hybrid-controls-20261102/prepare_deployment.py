from pathlib import Path
import hashlib,json,re
root=Path(__file__).resolve().parent
install=Path('C:/Program Files (x86)/Steam/steamapps/common')
records=[]
for collection,folder in [('123','Tomb Raider I-III Remastered'),('456','Tomb Raider IV-VI Remastered')]:
    game=install/folder;path=game/'TombRaiderVR.ini';raw=path.read_bytes()
    if raw.startswith((b'\xff\xfe',b'\xfe\xff')): encoding='utf-16'
    elif raw.startswith(b'\xef\xbb\xbf'): encoding='utf-8-sig'
    else:
        try: raw.decode('utf-8');encoding='utf-8'
        except UnicodeDecodeError: encoding='latin-1'
    text=raw.decode(encoding);newline='\r\n' if '\r\n' in text else '\n'
    section=re.search(r'(?im)^\[VR\][^\r\n]*(?:[\r\n]+|$)',text)
    assert section,f'Missing VR section: {path}'
    next_section=re.search(r'(?m)^\[',text[section.end():])
    end=section.end()+next_section.start() if next_section else len(text)
    body=text[section.end():end]
    pattern=r'(?im)^([ \t]*HybridModernTankControls[ \t]*=)[^\r\n]*'
    matches=list(re.finditer(pattern,body));assert len(matches)<=1,'Duplicate setting'
    if matches: updated=re.sub(pattern,r'\g<1>0',body)
    else:
        comment=(newline+'; Hybrid Modern/Tank Controls: 0 = Off (default), 1 = On.'+newline+
            '; Off uses native Modern walking; On enables the third-person Walk + back hop in TR1-5.'+newline+
            '; First-person movement/crawling and TR6 native controls are unchanged. Restart after editing.'+newline+
            'HybridModernTankControls=0'+newline)
        anchor=re.search(r'(?im)^GamepadEnabled[ \t]*=[^\r\n]*(?:[\r\n]+|$)',body)
        offset=anchor.end() if anchor else 0
        updated=body[:offset]+comment+body[offset:]
        assert updated.replace(comment,'',1)==body,'Unrelated INI content changed'
    updated=text[:section.end()]+updated+text[end:]
    staged=root/collection/'TombRaiderVR.ini';staged.write_bytes(updated.encode(encoding))
    records.append({'collection':collection,'directory':str(game),'ini_before_sha256':hashlib.sha256(raw).hexdigest(),
        'ini_after_sha256':hashlib.sha256(staged.read_bytes()).hexdigest(),
        'dll_sha256':hashlib.sha256((root/collection/'bin/TombRaiderVR.dll').read_bytes()).hexdigest()})
(root/'deployment-manifest.json').write_text(json.dumps(records,indent=2))
print('Prepared both installed INIs with HybridModernTankControls=0; unrelated text retained.')
