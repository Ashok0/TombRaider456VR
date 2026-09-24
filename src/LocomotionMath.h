// Directional stick locomotion from TombRaider123VR/LocomotionMath.h.
// Horizontal +x is right, +z is forward; positive yaw turns right.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tr::locomotion {
struct Vec { float x = 0, z = 0; };
inline Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.z + b.z}; }
inline Vec operator-(Vec a, Vec b) { return {a.x - b.x, a.z - b.z}; }
inline Vec operator*(Vec a, float s) { return {a.x * s, a.z * s}; }
inline float Length(Vec a) { return std::sqrt(a.x * a.x + a.z * a.z); }
inline Vec Rotate(Vec v, float yaw) {
    const float c = std::cos(yaw), s = std::sin(yaw);
    return {c * v.x + s * v.z, -s * v.x + c * v.z};
}
inline Vec Limit(Vec v) {
    const float n = Length(v);
    return n > 1 ? v * (1 / n) : v;
}
// Same physical neck pivot as TR1-3: Lara's animated head supplies the eye
// arc, so only genuine neck displacement belongs on top of that anchor.
inline Vec NeckToHead(float yaw, float metres) { return Rotate({0, metres}, yaw); }
inline Vec NeckFloorOffset(Vec rawEyeOffset, Vec neckArc) { return rawEyeOffset - neckArc; }
inline Vec DragRequest(Vec pending, float dead) {
    const float n = Length(pending);
    return n > std::max(0.0f, dead) && n > 0.0001f
        ? pending * ((n - std::max(0.0f, dead)) / n) : Vec{};
}
inline Vec PivotFloorOffset(Vec rawEyeOffset, Vec neckArc, float yawDelta) {
    return Rotate(NeckFloorOffset(rawEyeOffset, neckArc), -yawDelta) + neckArc;
}
inline bool IsJumpSteeringState(int state) { return state == 15 || state == 3; }
inline Vec SimulationStick(Vec world, float frameYaw, float magnitude) {
    const float n = Length(world);
    return n > 0.0001f ? Rotate(world, -frameYaw) * (magnitude / n) : Vec{};
}
// TR4/5 retain these classic action bits, but their full input mask is 64-bit.
constexpr uint64_t Forward = 1, Back = 2, Left = 4, Right = 8;
constexpr uint64_t Walk = 0x80, StepLeft = 0x400, StepRight = 0x800;
constexpr uint64_t Directions = Forward | Back | Left | Right | StepLeft | StepRight;
inline Vec CardinalMovement(Vec stick) {
    const float magnitude = Length(stick);
    if (magnitude <= 0.0001f) return {};
    if (std::fabs(stick.x) > std::fabs(stick.z))
        return {std::copysign(magnitude, stick.x), 0};
    return {0, std::copysign(magnitude, stick.z)};
}
inline Vec MovementWorld(Vec stick, float heading) {
    return Rotate(CardinalMovement(stick), heading);
}
inline float MovementYaw(Vec stick, float heading) {
    const Vec world = MovementWorld(stick, heading);
    return std::atan2(world.x, world.z);
}
inline uint64_t MovementAction(Vec stick, bool preparingJump = false) {
    if (Length(stick) <= 0.0001f) return 0;
    if (std::fabs(stick.x) > std::fabs(stick.z)) {
        if (stick.x < 0) return preparingJump ? Left : StepLeft;
        return preparingJump ? Right : StepRight;
    }
    // Back alone is a hop; Walk+Back selects continuous backward walking.
    return stick.z < 0 ? (Back | Walk) : Forward;
}
inline int DirectionalRootScale(uint64_t action, bool preparingJump) {
    if (preparingJump) return 1;
    const uint64_t direction = action & Directions;
    return direction == Back || direction == StepLeft || direction == StepRight ? 3 : 1;
}
} // namespace tr::locomotion
