from pathlib import Path
import hashlib,re
root=Path('C:/dev/TombRaider456VR')
s=(root/'build/deploy_controller_type_only.ps1').read_text()
s=s.replace("$newFiles=@('src/VRNativeMenuExtra.inl')",'$newFiles=@()')
builds=[root/'build/vr-options-release/TombRaiderVR.dll',root/'build/vr-options123/bin/TombRaiderVR.dll']
matches=list(re.finditer("old='[^']+';new='([^']+)'",s))
for match,path in reversed(list(zip(matches,builds))):
 replacement="old='"+match[1]+"';new='"+hashlib.sha256(path.read_bytes()).hexdigest()+"'"
 s=s[:match.start()]+replacement+s[match.end():]
s=s.replace('build/controller-type-only/','build/color-scheme/').replace('before-controller-type-','before-color-scheme-').replace('controller-type-deployment.json','color-scheme-deployment.json')
s=s.replace('controller_menu_checks=43;native_structure_checks=21','vr_settings_checks=55;native_structure_checks=27')
(root/'build/deploy_color_scheme.ps1').write_text(s)
print('Prepared guarded color-scheme installation for both mods.')
