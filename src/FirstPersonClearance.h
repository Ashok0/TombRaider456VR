#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tr::firstperson {

// Sweep the rendered eye from Lara's collision origin in short horizontal
// steps. The caller supplies the engine's collision query for each candidate.
// Keep the last clear point; do not move the eye through a blocked wall.
template <typename Blocked>
void ClampEyeToWall(const int32_t body[3], int32_t eye[3], Blocked blocked) {
    const int64_t dx = int64_t(eye[0]) - body[0];
    const int64_t dz = int64_t(eye[2]) - body[2];
    const double distance = std::hypot(double(dx), double(dz));
    if (distance < 1.0) return;
    if (distance > 512.0) {
        eye[0] = body[0];
        eye[2] = body[2];
        return;
    }

    const int steps = std::clamp(int(std::ceil(distance / 16.0)), 1, 32);
    int32_t clearX = body[0], clearZ = body[2];
    for (int i = 1; i <= steps; ++i) {
        const int32_t x = body[0] + int32_t(dx * i / steps);
        const int32_t z = body[2] + int32_t(dz * i / steps);
        if (blocked(x, z, clearX, clearZ)) {
            eye[0] = clearX;
            eye[2] = clearZ;
            return;
        }
        clearX = x;
        clearZ = z;
    }
}

} // namespace tr::firstperson
