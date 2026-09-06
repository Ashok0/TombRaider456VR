#include "GL.h"
#include "Log.h"

namespace gl {

void (APIENTRY* GenFramebuffers)(GLsizei, GLuint*) = nullptr;
void (APIENTRY* DeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
void (APIENTRY* BindFramebuffer)(GLenum, GLuint) = nullptr;
void (APIENTRY* FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
void (APIENTRY* GenRenderbuffers)(GLsizei, GLuint*) = nullptr;
void (APIENTRY* DeleteRenderbuffers)(GLsizei, const GLuint*) = nullptr;
void (APIENTRY* BindRenderbuffer)(GLenum, GLuint) = nullptr;
void (APIENTRY* RenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei) = nullptr;
void (APIENTRY* FramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint) = nullptr;
GLenum (APIENTRY* CheckFramebufferStatus)(GLenum) = nullptr;
void (APIENTRY* BlitFramebuffer)(GLint, GLint, GLint, GLint,
                                 GLint, GLint, GLint, GLint,
                                 GLbitfield, GLenum) = nullptr;
void (APIENTRY* GetUniformfv)(GLuint, GLint, GLfloat*) = nullptr;

namespace {

bool g_loaded = false;

template <typename T>
bool Grab(T& fn, const char* name) {
    fn = reinterpret_cast<T>(wglGetProcAddress(name));
    if (!fn) {
        // A few of these live in opengl32.dll itself on some drivers.
        static HMODULE ogl = GetModuleHandleW(L"opengl32.dll");
        if (ogl) fn = reinterpret_cast<T>(GetProcAddress(ogl, name));
    }
    if (!fn) LogF("gl: failed to resolve %s", name);
    return fn != nullptr;
}

} // namespace

bool Loaded() { return g_loaded; }

bool Load() {
    if (g_loaded) return true;
    if (!wglGetCurrentContext()) {
        Log("gl: no current context, deferring loader");
        return false;
    }

    bool ok = true;
    ok &= Grab(GenFramebuffers,        "glGenFramebuffers");
    ok &= Grab(DeleteFramebuffers,     "glDeleteFramebuffers");
    ok &= Grab(BindFramebuffer,        "glBindFramebuffer");
    ok &= Grab(FramebufferTexture2D,   "glFramebufferTexture2D");
    ok &= Grab(GenRenderbuffers,       "glGenRenderbuffers");
    ok &= Grab(DeleteRenderbuffers,    "glDeleteRenderbuffers");
    ok &= Grab(BindRenderbuffer,       "glBindRenderbuffer");
    ok &= Grab(RenderbufferStorage,    "glRenderbufferStorage");
    ok &= Grab(FramebufferRenderbuffer,"glFramebufferRenderbuffer");
    ok &= Grab(CheckFramebufferStatus, "glCheckFramebufferStatus");
    ok &= Grab(BlitFramebuffer,        "glBlitFramebuffer");

    // Diagnostics only. Deliberately not folded into `ok`: a driver that does
    // not hand this out must not disable stereo, it just loses the GPU-side
    // verification line in the log.
    Grab(GetUniformfv, "glGetUniformfv");

    g_loaded = ok;
    LogF("gl: loader %s", ok ? "ready" : "INCOMPLETE");
    return ok;
}

void CheckErrors(const char* where) {
    for (int i = 0; i < 8; ++i) {
        GLenum e = glGetError();
        if (e == GL_NO_ERROR) return;
        LogF("gl: error 0x%04X at %s", e, where);
    }
}

} // namespace gl
