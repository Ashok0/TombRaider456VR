// FirstPerson.h -- first-person camera and controls for Tomb Raider IV/V.
#pragma once
#include <cstdint>

namespace tr {
void FirstPersonUpdate();
void FirstPersonToggle();
void FirstPersonRecenter();
void FirstPersonShutdown();
bool FirstPersonActive();
bool FirstPersonCalibrationKeyReserved(int virtualKey);
// Called after view/graphics chords and physical-pad merge.
void FirstPersonGunTriggers(uint8_t& left, uint8_t& right, bool chordConsumed);
void FirstPersonInput(float& leftX, float& leftY, float& rightX, bool shifted,
                      bool jumpPressed = false);
} // namespace tr
