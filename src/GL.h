// GL.h -- the small slice of OpenGL this project needs.
//
// The game creates a 3.2 Core context (init_ogl asks for CTX_MAJOR 3 /
// CTX_MINOR 2 / PROFILE_CORE via wglCreateContextAttribsARB). opengl32.lib only
// exports GL 1.1, so everything framebuffer-related has to come through
// wglGetProcAddress once a context is current.
#pragma once

#include <windows.h>
#include <gl/GL.h>
#include <cstdint>

// --- enums the Windows GL header predates ----------------------------------
#define GL_FRAMEBUFFER            0x8D40
#define GL_READ_FRAMEBUFFER       0x8CA8
#define GL_DRAW_FRAMEBUFFER       0x8CA9
#define GL_RENDERBUFFER           0x8D41
#define GL_COLOR_ATTACHMENT0      0x8CE0
#define GL_DEPTH_ATTACHMENT       0x8D00
#define GL_DEPTH_COMPONENT24      0x81A6
#define GL_FRAMEBUFFER_COMPLETE   0x8CD5
#define GL_FRAMEBUFFER_BINDING    0x8CA6
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define GL_TEXTURE_2D_ARRAY       0x8C1A
#define GL_CLAMP_TO_EDGE          0x812F
#define GL_SRGB8_ALPHA8           0x8C43
#define GL_RGBA8                  0x8058
#define GL_FRAMEBUFFER_SRGB       0x8DB9
#define GL_CURRENT_PROGRAM        0x8B8D

typedef ptrdiff_t GLsizeiptr_t;

namespace gl {

// GL 3.0+ framebuffer / renderbuffer entry points.
extern void (APIENTRY* GenFramebuffers)(GLsizei, GLuint*);
extern void (APIENTRY* DeleteFramebuffers)(GLsizei, const GLuint*);
extern void (APIENTRY* BindFramebuffer)(GLenum, GLuint);
extern void (APIENTRY* FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
extern void (APIENTRY* GenRenderbuffers)(GLsizei, GLuint*);
extern void (APIENTRY* DeleteRenderbuffers)(GLsizei, const GLuint*);
extern void (APIENTRY* BindRenderbuffer)(GLenum, GLuint);
extern void (APIENTRY* RenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
extern void (APIENTRY* FramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
extern GLenum (APIENTRY* CheckFramebufferStatus)(GLenum);
extern void (APIENTRY* BlitFramebuffer)(GLint, GLint, GLint, GLint,
                                        GLint, GLint, GLint, GLint,
                                        GLbitfield, GLenum);

// GL 2.0. Used only by the per-eye verification in Hooks.cpp, so its absence is
// reported but not fatal -- a driver without it loses the diagnostic, not stereo.
extern void (APIENTRY* GetUniformfv)(GLuint, GLint, GLfloat*);

// Resolve everything. Requires a current context. Safe to call repeatedly.
bool Load();
bool Loaded();

// Drains and logs any pending GL errors, tagged with `where`.
void CheckErrors(const char* where);

} // namespace gl
