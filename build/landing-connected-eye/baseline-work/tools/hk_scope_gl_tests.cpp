// Hidden-window OpenGL smoke test; no game process or VR runtime required.
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include "../src/HKScope.cpp"

namespace {
tr::Config cfg;
tr::RenderState state{},previous{};
tr::mat4 projections[2]{};
bool available=true,handPass=false;
float scopeTestScale=1000.f,scopeTestLensZ=120.f;
int checks=0;
void Check(bool pass,const char* label) {
    ++checks;if (!pass) { std::printf("FAIL: %s\n",label);std::exit(1); }
}
GLint Get(GLenum key) { GLint value=0;glGetIntegerv(key,&value);return value; }
}
void Log(const char* text) { std::puts(text); }
void LogF(const char* fmt,...) { va_list args;va_start(args,fmt);std::vprintf(fmt,args);va_end(args);std::puts(""); }
namespace tr {
const Config& Cfg() { return cfg; }
float LiveWorldUnitsPerMetre() { return scopeTestScale; }
RenderState& VidState() { return state; }
RenderState& VidStatePrev() { return previous; }
mat4* Proj() { return projections; }
VRSystem& VR() { static VRSystem vr;return vr; }
Affine VRSystem::EyeView(Eye eye) const {
    Affine view=Affine::Identity();view.r[0][3]=eye==Eye::Left ? 0.f : -.064f*scopeTestScale;return view;
}
void VRSystem::EyeProjection(Eye,float n,float f,mat4& out) const { BuildEyeProjection(out,-1,1,-1,1,n,f,true); }
bool CameraViewFrame(float rot[3][3],float pos[3]) {
    std::memset(rot,0,9*sizeof(float));std::memset(pos,0,3*sizeof(float));
    rot[0][0]=rot[1][1]=rot[2][2]=1;return true;
}
bool FirstPersonDrawingTrackedHands() { return handPass; }
bool FirstPersonHKScopePose(motiongun::Frame& lens,motiongun::Frame& camera,float* radiusMetres) {
    const motiongun::Basis identity{{{1,0,0},{0,1,0},{0,0,1}}};
    lens={motiongun::GunBasis(identity),{0,0,scopeTestLensZ}};camera={lens.basis,{0,0,200}};
    if (radiusMetres) *radiusMetres=.03f;return available;
}
}

int main() {
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"HKScopeTest";wc.style=CS_OWNDC;
    Check(RegisterClassW(&wc)!=0,"register hidden window");
    HWND window=CreateWindowW(wc.lpszClassName,L"Scope test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
    Check(window!=nullptr,"create hidden window");HDC dc=GetDC(window);
    PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cDepthBits=24;
    Check(SetPixelFormat(dc,ChoosePixelFormat(dc,&pfd),&pfd)!=0,"set pixel format");
    HGLRC context=wglCreateContext(dc);Check(context && wglMakeCurrent(dc,context),"create GL context");
    using CreateContext=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
    auto create=reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB"));
    Check(create!=nullptr,"GL 3.2 context creation available");
    const int attribs[]={0x2091,3,0x2092,2,0x9126,1,0};
    HGLRC core=create(dc,nullptr,attribs);Check(core!=nullptr,"create GL 3.2 core");
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);Check(wglMakeCurrent(dc,core)!=0,"activate core context");
    std::printf("GL: %s\n",glGetString(GL_VERSION));Check(gl::Load(),"load GL entry points");
    cfg.eyeOffsetMode=3;cfg.perEyeProjection=1;cfg.perEyeView=true;cfg.duplicateDraws=true;
    tr::BuildEyeProjection(projections[1],-1,1,-1,1,1,32768,true);
    Check(tr::Stereo().Create(256,256),"create test stereo target");
    tr::Stereo().BeginFrame();glClearColor(0,0,1,1);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glViewport(7,8,200,190);glScissor(10,11,180,170);glEnable(GL_SCISSOR_TEST);
    const GLint oldFbo=Get(GL_DRAW_FRAMEBUFFER_BINDING);
    handPass=true;Check(!tr::HKScopeBeginCapture(),"tracked hands excluded without capturing");handPass=false;
    Check(tr::HKScopeBeginCapture(),"aligned HK starts capture");
    Check(tr::HKScopeCapturing() && Get(GL_DRAW_FRAMEBUFFER_BINDING)!=oldFbo,"capture owns independent FBO");
    auto projection=projections[1];tr::HKScopeProjection(projection,false);
    Check(std::fabs(projection.m[0]-12.f)<0.001f,"independent narrow projection at reference 3x");
    glClearColor(0,1,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glScissor(0,256,256,256);glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    glScissor(0,0,256,256);glClearColor(1,1,0,1);glClear(GL_COLOR_BUFFER_BIT);
    tr::HKScopeEndCapture();
    Check(!tr::HKScopeCapturing() && Get(GL_DRAW_FRAMEBUFFER_BINDING)==oldFbo,"capture restores framebuffer");
    GLint vp[4]{},box[4]{};glGetIntegerv(GL_VIEWPORT,vp);glGetIntegerv(GL_SCISSOR_BOX,box);
    Check(vp[0]==7 && vp[1]==8 && vp[2]==200 && vp[3]==190 && box[0]==10 && box[1]==11,"capture restores viewport/scissor");
    Check((state.consts&(tr::kProj|tr::kView))==(tr::kProj|tr::kView),"eye uniforms dirtied after capture");
    glEnable(GL_BLEND);glEnable(GL_CULL_FACE);glDisable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_GREATER);
    gl::ActiveTexture(GL_TEXTURE0+2);
    tr::HKScopePresent();
    Check(Get(GL_DRAW_FRAMEBUFFER_BINDING)==oldFbo && Get(GL_ACTIVE_TEXTURE)==GL_TEXTURE0+2,"composite restores target/texture unit");
    Check(glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE) && !glIsEnabled(GL_DEPTH_TEST) && Get(GL_DEPTH_FUNC)==GL_GREATER,"composite restores depth/blend/cull");
    glGetIntegerv(GL_VIEWPORT,vp);glGetIntegerv(GL_SCISSOR_BOX,box);
    Check(vp[0]==7 && vp[1]==8 && vp[2]==200 && box[0]==10 && glIsEnabled(GL_SCISSOR_TEST),"composite restores viewport/scissor");
    gl::BindFramebuffer(GL_READ_FRAMEBUFFER,tr::Stereo().fbo());
    auto pixel=[](int x,int y) { static unsigned char p[4];glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);return p; };
    auto p=pixel(137,137);Check(p[1]>200 && p[2]<20,"aligned lens samples captured green view");
    p=pixel(119,137);Check(p[0]>200 && p[1]<20,"lens preserves left/right and upper image orientation");
    p=pixel(119,119);Check(p[0]>200 && p[1]>200 && p[2]<20,"lens preserves lower image orientation");
    p=pixel(128,128);Check(p[0]>100 && p[1]<30,"center reticle rendered");
    p=pixel(40,40);Check(p[2]>200 && p[1]<20,"normal world outside lens unchanged");
    p=pixel(384,128);Check(p[2]>200 && p[1]<20,"unaligned eye retains normal view");
    Check(glGetError()==GL_NO_ERROR,"capture/composite has no GL errors");
    // Render actual view-space geometry, rather than only clearing the target
    // to colors. Measure image zoom independently of the physical lens MVP.
    // This exercises scope optics/compositing, NOT the native game draw hook.
    GLuint vs=tr::Compile(GL_VERTEX_SHADER,"#version 150\nin vec3 point;uniform mat4 projection;void main(){gl_Position=projection*vec4(point,1);}");
    GLuint fs=tr::Compile(GL_FRAGMENT_SHADER,"#version 150\nout vec4 color;void main(){color=vec4(0,1,0,1);}");
    GLuint geometry=gl::CreateProgram();gl::AttachShader(geometry,vs);gl::AttachShader(geometry,fs);gl::LinkProgram(geometry);
    GLint linked=0;gl::GetProgramiv(geometry,GL_LINK_STATUS,&linked);Check(linked!=0,"geometry zoom test shader links");
    GLuint geometryVao=0,geometryBuffer=0;gl::GenVertexArrays(1,&geometryVao);gl::BindVertexArray(geometryVao);
    gl::GenBuffers(1,&geometryBuffer);gl::BindBuffer(GL_ARRAY_BUFFER,geometryBuffer);
    const float points[]={-80,-80,-4200,80,-80,-4200,-80,80,-4200,80,-80,-4200,80,80,-4200,-80,80,-4200};
    gl::BufferData(GL_ARRAY_BUFFER,sizeof(points),points,GL_STATIC_DRAW);
    GLint point=gl::GetAttribLocation(geometry,"point");gl::EnableVertexAttribArray(point);gl::VertexAttribPointer(point,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
    tr::mat4 fixedLens[2]{};
    auto measuredWidth=[&](float factor) {
        Check(tr::HKScopeBeginCapture(),"geometry capture starts");
        if (factor==1.f) std::memcpy(fixedLens,tr::lensMvp,sizeof(fixedLens));
        else Check(!std::memcmp(fixedLens,tr::lensMvp,sizeof(fixedLens)),"magnification leaves physical lens matrices unchanged");
        auto pr=projections[1];tr::HKScopeProjection(pr,false);
        // Scale both complete clip-space X/Y rows, including camera translation.
        for (int col=0;col<4;++col) { pr.m[col*4]*=factor;pr.m[col*4+1]*=factor; }
        gl::UseProgram(geometry);gl::BindVertexArray(geometryVao);
        gl::UniformMatrix4fv(gl::GetUniformLocation(geometry,"projection"),1,GL_FALSE,pr.m);
        glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glDrawArrays(GL_TRIANGLES,0,6);
        unsigned char row[512*4]{};glReadPixels(0,256,512,1,GL_RGBA,GL_UNSIGNED_BYTE,row);
        int width=0;for (int x=0;x<512;++x) if(row[x*4+1]>200) ++width;
        tr::HKScopeEndCapture();tr::HKScopePresent();return width;
    };
    const int width3=measuredWidth(1.f),width1=measuredWidth(1.f/3.f),width6=measuredWidth(2.f);
    Check(width1>0 && std::abs(width3-width1*3)<=4,"3x enlarges rendered world geometry threefold inside capture");
    Check(std::abs(width6-width3*2)<=2,"6x enlarges world geometry twice versus 3x without enlarging lens");
    std::printf("Geometry widths: 1x=%d 3x=%d 6x=%d pixels; lens matrices unchanged.\n",width1,width3,width6);
    gl::UseProgram(0);gl::BindVertexArray(0);gl::DeleteBuffers(1,&geometryBuffer);gl::DeleteVertexArrays(1,&geometryVao);
    gl::DeleteProgram(geometry);gl::DeleteShader(vs);gl::DeleteShader(fs);
    // The rear lens must pass in front of the tube/glass, but still lose to
    // nearer world geometry. An empty depth buffer alone cannot verify this.
    auto lensOverDepth=[&](float sceneDistance) {
        gl::BindFramebuffer(GL_FRAMEBUFFER,tr::Stereo().fbo());glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
        const auto& p=projections[1];
        glClearDepth(.5*(-p.m[10]+p.m[14]/sceneDistance)+.5);
        glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glClearDepth(1);
        glEnable(GL_SCISSOR_TEST);
        Check(tr::HKScopeBeginCapture(),"depth fixture starts scope capture");
        glClearColor(0,1,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        tr::HKScopeEndCapture();tr::HKScopePresent();
        gl::BindFramebuffer(GL_READ_FRAMEBUFFER,tr::Stereo().fbo());
        const auto sample=pixel(128,128);
        return sample[0]>100 && sample[1]<30;
    };
    Check(lensOverDepth(122.f),"reticle survives opaque tube/glass behind the rear lens");
    Check(!lensOverDepth(110.f),"nearer world geometry still occludes the lens");
    for (float scale:{423.f,1000.f}) for (float relief:{.12f,.039f,.02f,.0011f}) {
        scopeTestScale=scale;scopeTestLensZ=scale*relief;
        tr::BuildEyeProjection(projections[1],-1,1,-1,1,16,32768,true);
        Check(lensOverDepth(300.f),"close eye retains visible reticle without shifting lens");
        // At very short relief the crosshair covers more pixels, so sample
        // farther from the center while staying inside the physical aperture.
        const int offset=relief<.01f ? 50 : 9;
        p=pixel(128+offset,128+offset);Check(p[1]>200 && p[2]<20,"close-eye lens retains the captured world image");
        p=pixel(384,128);Check(p[2]>200 && p[0]<20,"close-eye comfort does not cover the unaligned eye");
    }
    for (float relief:{0.f,-.01f,-.03f}) {
        scopeTestLensZ=relief*scopeTestScale;
        Check(!tr::HKScopeBeginCapture(),"crossing the physical lens disables it without floating overlay");tr::HKScopePresent();
    }
    scopeTestScale=1000;scopeTestLensZ=120;
    available=false;Check(!tr::HKScopeBeginCapture(),"disabled scope cannot reuse stale capture");tr::HKScopePresent();
    available=true;Check(tr::HKScopeBeginCapture(),"scope reentry works");tr::HKScopeEndCapture();tr::HKScopePresent();
    tr::HKScopeShutdown();tr::Stereo().Destroy();
    Check(glGetError()==GL_NO_ERROR,"GL resource cleanup succeeds");
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(core);ReleaseDC(window,dc);DestroyWindow(window);
    std::printf("OK: %d HK scope GL checks passed.\n",checks);
}
