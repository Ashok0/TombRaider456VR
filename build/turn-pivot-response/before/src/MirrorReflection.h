#pragma once
#include "Engine.h"
namespace tr {
namespace mirror { struct RoomProfile; }
// Caller verifies the exact level/room/mesh profile and VR gameplay context.
bool MirrorReflectionPrepare(int nativePass=0,const mirror::RoomProfile* profile=nullptr);
bool MirrorReflectionBegin();
void MirrorReflectionEnd();
bool MirrorReflectionCapturing();
void MirrorReflectionProjection(mat4& projection,int eye=0);
void MirrorReflectionObserver(float out[4]);
void MirrorReflectionPresent();
void MirrorReflectionShutdown();
} // namespace tr
