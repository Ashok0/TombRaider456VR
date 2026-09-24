// FirstPerson.h -- first-person camera and controls for Tomb Raider IV/V.
#pragma once

namespace tr {
void FirstPersonUpdate();
void FirstPersonToggle();
void FirstPersonRecenter();
void FirstPersonShutdown();
bool FirstPersonActive();
void FirstPersonInput(float& leftX, float& leftY, float& rightX, bool shifted,
                      bool jumpPressed = false);
} // namespace tr
