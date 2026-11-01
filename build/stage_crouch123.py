from pathlib import Path
import hashlib
root=Path('C:/dev/TombRaider123VR')
stage=Path('C:/dev/TombRaider456VR/build/crouch123-port')
source=root/'src/FirstPerson.cpp'
data=source.read_bytes()
assert hashlib.sha256(data).hexdigest().upper()=='0C73D71071A361FEEBCD5816E102520A8BADC87058876E2B0628A20A8BA6301C'
text=data.decode().replace('\r\n','\n')
edits=[('''bool CanSteerCrouch(const uint8_t* item) {
    if (!CanUseCrouchCamera(item) ||
        *reinterpret_cast<const int16_t*>(item+off::item_required_state)!=0) return false;
    switch (*reinterpret_cast<const int16_t*>(item+off::item_goal_state)) {
    case 2: case 71: case 80: case 81: case 84: case 85: case 86:
    case 89: case 90: return true;
    default: return false;
    }
}''','''bool CanSteerCrouch(const uint8_t* item) {
    if (!CanUseCrouchCamera(item)) return false;
    const auto posture=[](int state) {
        switch(state) {
        case 2: case 71: case 80: case 81: case 84: case 85: case 86:
        case 89: case 90: return true;
        default: return false;
        }
    };
    // Ordinary queued crouch/crawl postures still belong to player locomotion.
    // Preserve the queue and native ceiling checks; interactions/hangs/rolls
    // retain their authored control. These state IDs are TR3-only above.
    const int required=*reinterpret_cast<const int16_t*>(item+off::item_required_state);
    return (!required || posture(required)) &&
        posture(*reinterpret_cast<const int16_t*>(item+off::item_goal_state));
}'''),('''    if (!item || item!=g_headingItem || LaraWaterStatus()!=0 ||
        *Ptr<int16_t>(g_boundDll->lara+off::lara_gun_status)!=0) return false;
    const int gun=*Ptr<int16_t>(g_boundDll->lara+4);''','''    if (!item || item!=g_headingItem || LaraWaterStatus()!=0) return false;
    const int status=*Ptr<int16_t>(g_boundDll->lara+off::lara_gun_status);
    // Native ALL4S uses hands-busy even without a weapon or interaction.
    // Keep controller hands available without changing that native flag.
    if (status!=0 && !(status==1 && CanSteerCrouch(item))) return false;
    const int gun=*Ptr<int16_t>(g_boundDll->lara+4);'''),('''    if(status==1 && !FullBodyGripState(item)) return false;''','''    if(status==1 && !FullBodyGripState(item) && !CanSteerCrouch(item)) return false;''')]
for old,new in edits:
    assert text.count(old)==1,old
    text=text.replace(old,new)
out=stage/'src/FirstPerson.cpp';out.parent.mkdir(parents=True,exist_ok=True)
out.write_bytes(text.replace('\n','\r\n').encode())
print('Staged TR3-only steering and tracked-hand correction.')
