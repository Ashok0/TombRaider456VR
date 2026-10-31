from pathlib import Path
import hashlib
root=Path('C:/dev/TombRaider456VR')
s=(root/'build/deploy_native_vr_options.ps1').read_text()
start=s.index('$newFiles=');end=s.index('\n$targets=',start)
s=s[:start]+"$newFiles=@('src/VRNativeMenuExtra.inl')"+s[end:]
s=s.replace("old='E0F264E80E087EDFC4EADB9F500A97D042A77750965A81FF386DD75F439EC15A';new='8E5C7310648F7605238E788A4E1E94A8389532374496A3363DD54CD7485E3F5D'", "old='8E5C7310648F7605238E788A4E1E94A8389532374496A3363DD54CD7485E3F5D';new='"+hashlib.sha256((root/'build/vr-options-release/TombRaiderVR.dll').read_bytes()).hexdigest()+"'")
s=s.replace("old='F6196FE57CCC064AC7302B701AEDA21490E783D937B11C3E3D6D7B4DBB6C7CAC';new='267FDB8002E3FEA1BBBCD531E73ECB64A0D708A189715E0B01A81D4DDFC0CBC1'", "old='267FDB8002E3FEA1BBBCD531E73ECB64A0D708A189715E0B01A81D4DDFC0CBC1';new='"+hashlib.sha256((root/'build/vr-options123/bin/TombRaiderVR.dll').read_bytes()).hexdigest()+"'")
s=s.replace("(Join-Path $stage 'native-baseline.json')", "(Join-Path $root 'build/controller-type-only/baseline123.json')")
s=s.replace("(Join-Path $stage 'native-source-baseline.json')", "(Join-Path $root 'build/controller-type-only/sources123.json')")
s=s.replace('before-native-', 'before-controller-type-').replace('native-deployment.json','controller-type-deployment.json').replace('controller_menu_checks=72;native_structure_checks=16','controller_menu_checks=43;native_structure_checks=21').replace('native VR OPTIONS','native VR Options')
(root/'build/deploy_controller_type_only.ps1').write_text(s)
print('Prepared guarded source port and installation script.')
