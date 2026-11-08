#include "MirrorReflection.h"
#include "MirrorReflectionMath.h"
#include "Config.h"
#include "GameDll.h"
#include "GL.h"
#include "VRSystem.h"
#include "StereoRenderer.h"
#include "Log.h"

namespace tr {
namespace {
GLuint color=0,depth=0,fbo=0,program=0,vao=0,vbo=0;
GLint uMvp=-1,uColor=-1,uDepth=-1,uSize=-1;
int width=0,height=0;
bool ready=false,capturing=false,captured=false,failed=false;
unsigned completedPasses=0;
int pendingPass=-1,currentEye=0;
float observers[2][4]{};
void (APIENTRY* BlendFuncSeparate)(GLenum,GLenum,GLenum,GLenum)=nullptr;
void (APIENTRY* BlendEquationSeparate)(GLenum,GLenum)=nullptr;
unsigned captureDraws=0;
bool loggedCapture=false,loggedComposite=false;
Affine reflection{};
mat4 surfaceMvp[2]{};
const mirror::RoomProfile* surfaceProfile=nullptr;
struct Target {
    GLint read=0,draw=0,viewport[4]{},scissor[4]{},front=GL_CCW;
    GLboolean scissorOn=false;
    void Save() {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
        glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_SCISSOR_BOX,scissor);
        glGetIntegerv(GL_FRONT_FACE,&front);scissorOn=glIsEnabled(GL_SCISSOR_TEST);
    }
    void Restore() const {
        gl::BindFramebuffer(GL_READ_FRAMEBUFFER,read);gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glScissor(scissor[0],scissor[1],scissor[2],scissor[3]);glFrontFace(front);
        if(scissorOn) glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
    }
} target;
void Delete() {
    if(vbo) gl::DeleteBuffers(1,&vbo);if(vao) gl::DeleteVertexArrays(1,&vao);
    if(program) gl::DeleteProgram(program);if(fbo) gl::DeleteFramebuffers(1,&fbo);
    if(color) glDeleteTextures(1,&color);if(depth) glDeleteTextures(1,&depth);
    color=depth=fbo=program=vao=vbo=0;width=height=0;surfaceProfile=nullptr;
}
GLuint Compile(GLenum kind,const char* source) {
    const GLuint shader=gl::CreateShader(kind);
    gl::ShaderSource(shader,1,&source,nullptr);gl::CompileShader(shader);
    GLint ok=0;gl::GetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) {
        char error[1024]{};gl::GetShaderInfoLog(shader,sizeof(error)-1,nullptr,error);
        LogF("mirror: shader failed: %s",error);gl::DeleteShader(shader);return 0;
    }
    return shader;
}
void Texture(GLuint& name,GLint format,GLenum external,GLenum type) {
    glGenTextures(1,&name);glBindTexture(GL_TEXTURE_2D,name);
    glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,external,type,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}
bool Create(const mirror::RoomProfile& profile) {
    const int w=int(Stereo().eyeWidth())*2,h=int(Stereo().eyeHeight());
    if(fbo && width==w && height==h && surfaceProfile==&profile) return true;
    if(failed || !gl::Loaded() || !gl::LoadedShaderApi() || w<=0 || h<=0) return false;
    BlendFuncSeparate=reinterpret_cast<decltype(BlendFuncSeparate)>(wglGetProcAddress("glBlendFuncSeparate"));
    BlendEquationSeparate=reinterpret_cast<decltype(BlendEquationSeparate)>(wglGetProcAddress("glBlendEquationSeparate"));
    if(!BlendFuncSeparate || !BlendEquationSeparate) { failed=true;return false; }
    // A resolution change invalidates all previously captured pixels.
    Delete();width=w;height=h;
    Target old;old.Save();
    GLint oldTexture=0,oldVao=0,oldBuffer=0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTexture);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldBuffer);
    Texture(color,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE);
    Texture(depth,GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT);
    gl::GenFramebuffers(1,&fbo);gl::BindFramebuffer(GL_FRAMEBUFFER,fbo);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
    bool ok=gl::CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    const char* vertex="#version 150\nin vec3 position;uniform mat4 mvp;void main(){gl_Position=mvp*vec4(position,1);}";
    const char* fragment="#version 150\nout vec4 result;uniform sampler2D bodyImage;uniform sampler2D bodyDepth;uniform vec4 inverseSize;"
        "void main(){vec2 uv=gl_FragCoord.xy*inverseSize.xy;"
        "vec4 body=texture(bodyImage,uv);bool solid=texture(bodyDepth,uv).r<1.0;"
        "if(!solid && body.a<=0.0)discard;result=vec4(body.rgb,solid ? 1.0 : clamp(body.a,0.0,1.0));}";
    const GLuint vs=ok ? Compile(GL_VERTEX_SHADER,vertex) : 0;
    const GLuint fs=vs ? Compile(GL_FRAGMENT_SHADER,fragment) : 0;
    if(vs && fs) {
        program=gl::CreateProgram();gl::AttachShader(program,vs);gl::AttachShader(program,fs);gl::LinkProgram(program);
        GLint linked=0;gl::GetProgramiv(program,GL_LINK_STATUS,&linked);ok=linked!=0;
        if(!ok) { char error[1024]{};gl::GetProgramInfoLog(program,sizeof(error)-1,nullptr,error);LogF("mirror: link failed: %s",error); }
    } else ok=false;
    if(vs) gl::DeleteShader(vs);if(fs) gl::DeleteShader(fs);
    if(ok) {
        uMvp=gl::GetUniformLocation(program,"mvp");uColor=gl::GetUniformLocation(program,"bodyImage");
        uDepth=gl::GetUniformLocation(program,"bodyDepth");uSize=gl::GetUniformLocation(program,"inverseSize");
        float vertices[54]{};int at=0;
        for(int pane=0;pane<3;++pane) for(int corner:{0,1,2,2,1,3}) {
            vertices[at++]=profile.planeX+.5f; // glass depth, in front of its opaque backing
            vertices[at++]=(corner&1) ? profile.bottom : profile.top;
            vertices[at++]=profile.glassZ[pane][corner>>1];
        }
        gl::GenVertexArrays(1,&vao);gl::BindVertexArray(vao);gl::GenBuffers(1,&vbo);gl::BindBuffer(GL_ARRAY_BUFFER,vbo);
        gl::BufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
        const GLint attr=gl::GetAttribLocation(program,"position");
        ok=attr>=0 && uMvp>=0 && uColor>=0 && uDepth>=0 && uSize>=0;
        if(ok) { gl::EnableVertexAttribArray(attr);gl::VertexAttribPointer(attr,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr); }
    }
    gl::BindVertexArray(oldVao);gl::BindBuffer(GL_ARRAY_BUFFER,oldBuffer);glBindTexture(GL_TEXTURE_2D,oldTexture);old.Restore();
    if(!ok) { Delete();failed=true;Log("mirror: unavailable; retaining normal first-person rendering"); }
    else { surfaceProfile=&profile;LogF("mirror: TR5 RICH2 room %d, three verified panes, %dx%d stereo body target",profile.room,width,height); }
    return ok;
}
} // namespace

bool MirrorReflectionCapturing() { return capturing; }
bool MirrorReflectionPrepare(int nativePass,const mirror::RoomProfile* profile) {
    ready=false;
    if(!profile) profile=&mirror::Profiles[0];
    // Accept only audited profiles and never combine material passes from
    // different rooms into the same reflection image.
    if(profile!=&mirror::Profiles[0] && profile!=&mirror::Profiles[1]) return false;
    if(completedPasses && surfaceProfile!=profile) return false;
    if(capturing || !mirror::CapturePass(nativePass,completedPasses) || !Cfg().firstPersonMirrorReflections || !VR().poseValid() ||
       !Stereo().valid() || Cfg().monoTracking || !Cfg().duplicateDraws ||
       Cfg().eyeOffsetMode!=3 || !Cfg().perEyeView || Cfg().perEyeProjection!=1 ||
       Rt().color_id || Rt().depth_id || !VidState().proj || IsOrthoProjection(*VidState().proj)) return false;
    float rot[3][3]{},pos[3]{};
    if(!CameraViewFrame(rot,pos)) return false;
    const Affine world=mirror::WorldView(rot,pos);
    reflection=mirror::CameraReflection(world,*profile);
    float nearPlane=16,farPlane=32768;ExtractNearFar(*VidState().proj,nearPlane,farPlane);
    if(Cfg().nearClip>0) nearPlane=Cfg().nearClip;if(Cfg().farClip>0) farPlane=Cfg().farClip;
    for(int eye=0;eye<2;++eye) {
        const Affine view=Mul(VR().EyeView(Eye(eye)),world);
        // Both eyes must remain on the front side of the glass.
        if(!mirror::InFront(view,*profile)) return false;
        mirror::ObserverOffset(view,pos,observers[eye],*profile);
        mat4 projection{};VR().EyeProjection(Eye(eye),nearPlane,farPlane,projection);
        surfaceMvp[eye]=Mul4(projection,AffineToMat4(view));
    }
    ready=Create(*profile);pendingPass=ready ? nativePass : -1;return ready;
}
bool MirrorReflectionBegin() {
    if(!ready || capturing || !mirror::CapturePass(pendingPass,completedPasses)) return false;
    target.Save();gl::BindFramebuffer(GL_FRAMEBUFFER,fbo);glDisable(GL_SCISSOR_TEST);
    if(!completedPasses) {
        GLfloat clearColor[4]{};GLdouble clearDepth=1;GLboolean mask[4]{},write=false;
        glGetFloatv(GL_COLOR_CLEAR_VALUE,clearColor);glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clearDepth);
        glGetBooleanv(GL_COLOR_WRITEMASK,mask);glGetBooleanv(GL_DEPTH_WRITEMASK,&write);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(0,0,0,0);glClearDepth(1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glColorMask(mask[0],mask[1],mask[2],mask[3]);glDepthMask(write);
        glClearColor(clearColor[0],clearColor[1],clearColor[2],clearColor[3]);glClearDepth(clearDepth);
        captureDraws=0;
    }
    glEnable(GL_SCISSOR_TEST);
    // Reflection reverses winding. validate_draw changes CullFace, but never
    // FrontFace; retaining its material culling avoids inside-out faces.
    glFrontFace(target.front==GL_CCW ? GL_CW : GL_CCW);
    capturing=true;return true;
}
void MirrorReflectionProjection(mat4& projection,int eye) {
    if(capturing) {
        currentEye=eye==1 ? 1 : 0;
        projection=Mul4(projection,AffineToMat4(reflection));
        ++captureDraws;
    }
}
void MirrorReflectionObserver(float out[4]) {
    for(int i=0;i<4;++i) out[i]=capturing ? observers[currentEye][i] : 0.f;
}
void MirrorReflectionEnd() {
    if(!capturing) return;
    completedPasses|=1u<<pendingPass;ready=false;
    capturing=false;captured=true;target.Restore();
    VidState().consts|=kProj|kView;
    if(!loggedCapture && captureDraws) {
        LogF("mirror: first native Lara capture complete (%u per-eye draws)",captureDraws);
        loggedCapture=true;
    }
}
void MirrorReflectionPresent() {
    if(captured && Stereo().valid() && width==int(Stereo().eyeWidth())*2 && height==int(Stereo().eyeHeight())) {
        Target old;old.Save();
        GLint oldProgram=0,oldVao=0,oldActive=0,oldTexture[2]{},oldSampler[2]{},oldDepthFunc=0;
        GLint blendSrcRGB=0,blendDstRGB=0,blendSrcAlpha=0,blendDstAlpha=0,blendEqRGB=0,blendEqAlpha=0;
        glGetIntegerv(0x80C9,&blendSrcRGB);glGetIntegerv(0x80C8,&blendDstRGB);
        glGetIntegerv(0x80CB,&blendSrcAlpha);glGetIntegerv(0x80CA,&blendDstAlpha);
        glGetIntegerv(0x8009,&blendEqRGB);glGetIntegerv(0x883D,&blendEqAlpha);
        glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&oldActive);glGetIntegerv(GL_DEPTH_FUNC,&oldDepthFunc);
        for(int unit=0;unit<2;++unit) {
            gl::ActiveTexture(GL_TEXTURE0+unit);glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTexture[unit]);
            if(gl::BindSampler) { glGetIntegerv(0x8919,&oldSampler[unit]);gl::BindSampler(unit,0); }
            glBindTexture(GL_TEXTURE_2D,unit ? depth : color);
        }
        const GLboolean wasDepth=glIsEnabled(GL_DEPTH_TEST),wasBlend=glIsEnabled(GL_BLEND),wasCull=glIsEnabled(GL_CULL_FACE);
        GLboolean write=false,mask[4]{};GLfloat range[2]{};
        glGetBooleanv(GL_DEPTH_WRITEMASK,&write);glGetBooleanv(GL_COLOR_WRITEMASK,mask);glGetFloatv(GL_DEPTH_RANGE,range);
        gl::BindFramebuffer(GL_FRAMEBUFFER,Stereo().fbo());glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);glDepthRange(0,1);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        // Native pass 1 uses premultiplied transparency. Preserve hair fringe
        // coverage even where it writes no depth, over the original glass.
        glEnable(GL_BLEND);BlendFuncSeparate(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        BlendEquationSeparate(0x8006,0x8006);
        glDisable(GL_CULL_FACE);gl::UseProgram(program);gl::BindVertexArray(vao);
        gl::Uniform1i(uColor,0);gl::Uniform1i(uDepth,1);const float size[]={1.f/width,1.f/height,0,0};gl::Uniform4fv(uSize,1,size);
        for(int eye=0;eye<2;++eye) {
            Stereo().SetEyeViewport(eye);gl::UniformMatrix4fv(uMvp,1,GL_FALSE,surfaceMvp[eye].m);
            glDrawArrays(GL_TRIANGLES,0,18);
        }
        gl::UseProgram(oldProgram);gl::BindVertexArray(oldVao);
        for(int unit=0;unit<2;++unit) {
            gl::ActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,oldTexture[unit]);
            if(gl::BindSampler) gl::BindSampler(unit,oldSampler[unit]);
        }
        gl::ActiveTexture(oldActive);glDepthFunc(oldDepthFunc);glDepthMask(write);glDepthRange(range[0],range[1]);
        glColorMask(mask[0],mask[1],mask[2],mask[3]);
        BlendFuncSeparate(blendSrcRGB,blendDstRGB,blendSrcAlpha,blendDstAlpha);
        BlendEquationSeparate(blendEqRGB,blendEqAlpha);
        if(!wasDepth) glDisable(GL_DEPTH_TEST);if(!wasBlend) glDisable(GL_BLEND);if(wasCull) glEnable(GL_CULL_FACE);
        old.Restore();
        if(!loggedComposite && captureDraws) {
            Log("mirror: first stereo glass composite complete");loggedComposite=true;
        }
    }
    ready=captured=false;completedPasses=0;pendingPass=-1;
}
void MirrorReflectionShutdown() {
    if(wglGetCurrentContext() && gl::Loaded()) Delete();
    ready=captured=capturing=failed=false;
    completedPasses=0;pendingPass=-1;
    captureDraws=0;loggedCapture=loggedComposite=false;
}
} // namespace tr
