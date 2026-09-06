// StereoRenderer.h -- one double-wide eye target and the viewport bookkeeping
// around it.
//
// Layout is a single 2W x H RGBA8 texture: left eye occupies [0,W), right eye
// [W,2W). Submitting is then two Submit() calls differing only in UV bounds,
// with no second FBO and no attachment switch between eyes.
//
// This deliberately does NOT reuse the engine's FBO_custom. That is a single
// global (created in init_ogl, destroyed in ogl_release) shared by every
// offscreen pass in the game, and ogl_setRenderTarget re-attaches colour and
// depth and calls glCheckFramebufferStatus on every change. Borrowing it would
// mean fighting the engine for attachments twice per frame.
#pragma once

#include "GL.h"
#include <cstdint>

namespace tr {

class StereoRenderer {
public:
    // Requires a current GL context. Idempotent; recreates if the size changed.
    bool Create(uint32_t eyeWidth, uint32_t eyeHeight);
    void Destroy();

    bool valid() const { return m_fbo != 0; }

    GLuint texture() const { return m_tex; }
    GLuint fbo()     const { return m_fbo; }
    uint32_t eyeWidth()  const { return m_eyeW; }
    uint32_t eyeHeight() const { return m_eyeH; }

    // Bind the eye target and clear it. Called once per frame.
    void BeginFrame();

    // Set viewport + scissor to one half of the double-wide target.
    void SetEyeViewport(int eye);

    // Blit the left half back to the game's window framebuffer so the flat
    // screen still shows something sensible.
    void MirrorToWindow(int windowWidth, int windowHeight, GLuint windowFbo);

private:
    GLuint   m_fbo   = 0;
    GLuint   m_tex   = 0;
    GLuint   m_depth = 0;
    uint32_t m_eyeW  = 0;
    uint32_t m_eyeH  = 0;
};

StereoRenderer& Stereo();

} // namespace tr
