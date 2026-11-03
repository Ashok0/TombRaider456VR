from pathlib import Path
import hashlib,json
root=Path('C:/dev/TombRaider123VR')
stage=Path(__file__).resolve().parent/'stage'
records=[]
def change(relative,old,new):
    source=root/relative;data=source.read_bytes();text=data.decode('utf-8')
    newline='\r\n' if '\r\n' in text else '\n'
    old=old.replace('\n',newline);new=new.replace('\n',newline)
    assert text.count(old)==1,relative
    out=stage/relative;out.parent.mkdir(parents=True,exist_ok=True)
    out.write_bytes(text.replace(old,new).encode('utf-8'))
    records.append({'Relative':relative,'Before':hashlib.sha256(data).hexdigest()})
change('src/FirstPersonVisibility.h','inline bool UseHeadCamera(int hitPoints) { return hitPoints > 0; }',
'''inline bool UseHeadCamera(int hitPoints) { return hitPoints > 0; }

inline bool QuestItemUsesThirdPerson(int game,int state) {
    // Native USEKEY/USEPUZZLE share these state IDs in TR1, TR2 and TR3.
    // TR4/5's custom socket state 89 is a crawling state in TR3.
    return game>=1 && game<=3 && (state==42 || state==43);
}''')
change('src/FirstPerson.cpp',
'''    if (locomotion::IsSwitchAnimation(*reinterpret_cast<const int16_t*>(item+off::item_anim_state))) return false;''',
'''    // Keep native camera, complete body and input through the actual use
    // animation, even when its goal has already returned to STOP. The normal
    // camera handoff resumes FP without toggling the player's preference.
    const int state=*reinterpret_cast<const int16_t*>(item+off::item_anim_state);
    if (locomotion::IsSwitchAnimation(state) ||
        firstperson::QuestItemUsesThirdPerson(BoundGame(),state)) return false;''')
change('README.md','### TR2 Manor piano collision (2026-11-02)',
'''### Key and puzzle-item camera (2026-11-03)

In all three games, using a key or inserting a puzzle item temporarily shows
Lara in third person for the complete native use animation, then automatically
resumes first person. This works with Classic and Remastered graphics. Selecting
third person manually during the animation keeps that view afterward. Inventory,
item consumption and puzzle completion remain controlled by the game.

### TR2 Manor piano collision (2026-11-02)''')
(stage.parent/'source_hashes.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
print('Staged camera gate, state predicate and README changes for TR1-3.')
