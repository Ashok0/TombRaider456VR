#pragma once
#include "MotionGunMath.h"
#include <algorithm>

namespace tr::firstperson {
// MONITOR_SCREEN's inset glass, measured in canonical TRM coordinates.
// The screen is slightly curved; a small contact allowance covers its surface.
inline bool MonitorScreenTouch(motiongun::Vec previous,motiongun::Vec current,float scale) {
    const auto finite=[](motiongun::Vec p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); };
    if(!finite(previous) || !finite(current) || !std::isfinite(scale) || scale<=1) return false;
    const auto delta=motiongun::Sub(current,previous);
    if(motiongun::Dot(delta,delta)>scale*scale*.4f*.4f) return false;
    const float plane=-390.f+scale*.035f;
    // Enter from the glass side, not through the back of the television.
    // Resting on the screen cannot generate another touch until withdrawing.
    if(previous.z<=plane || current.z>plane || current.z<plane-scale*.12f) return false;
    const float t=(previous.z-plane)/(previous.z-current.z);
    const float x=previous.x+delta.x*t,y=previous.y+delta.y*t;
    return x>=-146 && x<=144 && y>=-642 && y<=-354;
}
}
