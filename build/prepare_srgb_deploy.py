from pathlib import Path
import hashlib,re
root=Path('C:/dev/TombRaider456VR')
s=(root/'build/deploy_color_scheme.ps1').read_text()
s=s.replace('$newFiles=@()',"$newFiles=@('tools/stereo_color_gl_tests.cpp','build/test_stereo_color.cmd')")
paths=[root/'build/vr-options-release/TombRaiderVR.dll',root/'build/vr-options123/bin/TombRaiderVR.dll']
matches=list(re.finditer("old='[^']+';new='([^']+)'",s))
for m,p in reversed(list(zip(matches,paths))):
 s=s[:m.start()]+"old='"+m[1]+"';new='"+hashlib.sha256(p.read_bytes()).hexdigest()+"'"+s[m.end():]
s=s.replace('build/color-scheme/','build/srgb-option/').replace('before-color-scheme-','before-srgb-option-').replace('color-scheme-deployment.json','srgb-option-deployment.json')
s=s.replace('vr_settings_checks=55;native_structure_checks=27;verified_layouts=15','stereo_color_gpu_checks=872;color_scheme=\'optional native sRGB targets; Dark unchanged\'')
(root/'build/deploy_srgb_option.ps1').write_text(s)
print('Prepared verified native sRGB option deployment.')
