from pathlib import Path
import hashlib
root=Path('C:/dev/TombRaider456VR');stage=root/'build/vr-options123'
s=(root/'build/deploy_srgb_option.ps1').read_text()
s=s.replace("$newFiles=@('tools/stereo_color_gl_tests.cpp','build/test_stereo_color.cmd')",'$newFiles=@()')
a=s.index('$targets=@(');b=s.index('\nforeach($target in $targets)',a)
new=hashlib.sha256((stage/'bin/TombRaiderVR.dll').read_bytes()).hexdigest()
s=s[:a]+'''$targets=@(
    @{repo=$repo123;game='C:\\Program Files (x86)\\Steam\\steamapps\\common\\Tomb Raider I-III Remastered';process='tomb123';source=(Join-Path $stage 'bin\\TombRaiderVR.dll');old='136741705f0ac1468b5d46ee30aa5b0591ca731b8c83c96073ec912bb789ec52';new='''+"'"+new+"'"+'''}
)
'''+s[b:]
s=s.replace('build/srgb-option/','build/options123-nested/').replace('before-srgb-option-','before-nested-options-').replace('srgb-option-deployment.json','nested-options-deployment.json')
s=s.replace("stereo_color_gpu_checks=872;color_scheme='optional native sRGB targets; Dark unchanged'", "native_options_checks=85;settings_checks=55;verified_game_layouts=9;change='Original Options > VR Options; no ring hooks or additional objects'")
(root/'build/deploy_options123_nested.ps1').write_text(s)
print('Prepared guarded TR1-3-only source port and DLL installation.')
