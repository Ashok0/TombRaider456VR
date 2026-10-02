// FirstPerson.h -- first-person camera and controls for Tomb Raider IV/V.
#pragma once
#include <cstdint>
#include "MotionGunMath.h"

namespace tr {
void FirstPersonUpdate();
void FirstPersonToggle();
void FirstPersonRecenter();
void FirstPersonShutdown();
bool FirstPersonActive();
// Unmasked palette only within Lara's first-person body draw.
const float* FirstPersonBodyPalette(uint64_t& mask,int& count);
// Render-only HK optic pose; no native zoom, weapon or camera state mutations.
bool FirstPersonHKScopePose(motiongun::Frame& lens, motiongun::Frame& camera,float* radiusMetres=nullptr);
bool FirstPersonDrawingTrackedHands();
// Palette index only while a successfully corrected tracked-hand draw is active.
int FirstPersonTrackedHandJoint();
bool FirstPersonHKScopeAiming();
bool FirstPersonCalibrationKeyReserved(int virtualKey);
// Called after view/graphics chords and physical-pad merge.
void FirstPersonGunTriggers(uint8_t& left, uint8_t& right, bool chordConsumed);
void FirstPersonInput(float& leftX, float& leftY, float& rightX, bool shifted,
                      bool jumpPressed = false);
} // namespace tr
