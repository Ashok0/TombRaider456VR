from pathlib import Path
import shutil
root=Path('C:/dev/TombRaider456VR');stage=root/'build/vr-options123'
for name in ['VRNativeMenu.cpp','VRNativeMenuExtra.inl','VRNativeMenuModel.h']:
 shutil.copy2(root/'src'/name,stage/'src'/name)
shutil.copy2(root/'tools/vr_native_menu_tests.cpp',stage/'tools/vr_native_menu_tests.cpp')
p=stage/'README.md';s=p.read_text(encoding='utf-8')
s=s.replace('Select **VR Options** in the title inventory ring.', 'Open the original **Options** item in the title inventory ring, scroll to the\nbottom of its settings, and select **VR Options**. The ring retains its five\noriginal items; no extra ring object is added.')
p.write_text(s,encoding='utf-8')
print('Synced TR1-3 native Options implementation and regression checks.')
