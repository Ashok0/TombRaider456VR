// Hidden-window real-driver test. Does not load a game DLL or the VR runtime.
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#define private public
#include "../src/VRSystem.h"
#undef private
#include "../src/MirrorReflection.cpp"
#include "../src/GL.cpp"
#include "../src/StereoRenderer.cpp"
#include "../src/MirrorReflectionShader.h"
#include "../src/MotionHandSkin.h"
#include "../src/KeypadLightShader.h"
#include "../build/mirror/native-shaders.h"
namespace {
tr::Config cfg;
tr::RenderState state{};
tr::OglRenderTarget renderTarget{};
tr::mat4 projection{};
float cameraX=34256;
float cameraY=-24744,cameraZ=77308;
int checks=0;
void Check(bool ok,const char* why) { ++checks;if(!ok) { std::printf("FAIL: %s\n",why);std::exit(1); } }
GLint Get(GLenum key) { GLint value=0;glGetIntegerv(key,&value);return value; }
}
void Log(const char* s) { std::puts(s); }
void LogF(const char* fmt,...) { va_list a;va_start(a,fmt);std::vprintf(fmt,a);va_end(a);std::puts(""); }
namespace tr {
const Config& Cfg() { return cfg; }
RenderState& VidState() { return state; }
OglRenderTarget& Rt() { return renderTarget; }
VRSystem& VR() { static VRSystem vr;return vr; }
Affine VRSystem::EyeView(Eye eye) const {
    Affine v=Affine::Identity();v.r[0][3]=eye==Eye::Left ? 32.f : -32.f;return v;
}
void VRSystem::EyeProjection(Eye eye,float n,float f,mat4& out) const {
    const float shift=eye==Eye::Left ? .13f : -.13f;
    BuildEyeProjection(out,-1+shift,1+shift,-1,1,n,f,true);
}
bool CameraViewFrame(float rot[3][3],float pos[3]) {
    const float r[3][3]={{0,0,1},{0,1,0},{-1,0,0}};
    std::memcpy(rot,r,sizeof(r));pos[0]=cameraX;pos[1]=cameraY;pos[2]=cameraZ;return true;
}
}
int main() {
    using namespace tr;
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"MirrorFixture";wc.style=CS_OWNDC;
    Check(RegisterClassW(&wc)!=0,"register hidden window");
    HWND window=CreateWindowW(wc.lpszClassName,L"Mirror fixture",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
    Check(window!=nullptr,"create hidden window");HDC dc=GetDC(window);
    PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cDepthBits=24;
    Check(SetPixelFormat(dc,ChoosePixelFormat(dc,&pfd),&pfd)!=0,"pixel format");
    HGLRC legacy=wglCreateContext(dc);Check(legacy && wglMakeCurrent(dc,legacy),"temporary GL context");
    using CreateContext=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
    const auto create=reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB"));
    Check(create!=nullptr,"core context support");const int attributes[]={0x2091,3,0x2092,2,0x9126,1,0};
    HGLRC context=create(dc,nullptr,attributes);Check(context!=nullptr,"GL 3.2 core context");
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(legacy);Check(wglMakeCurrent(dc,context)!=0,"activate core context");
    Check(gl::Load(),"load GL functions");std::printf("GL: %s\n",glGetString(GL_VERSION));
    cfg.eyeOffsetMode=3;cfg.duplicateDraws=cfg.perEyeView=cfg.firstPersonMirrorReflections=true;
    cfg.perEyeProjection=1;cfg.nearClip=16;cfg.farClip=32768;VR().m_poseValid=true;
    BuildEyeProjection(projection,-1,1,-1,1,16,32768,true);state.proj=&projection;
    Check(Stereo().Create(256,256),"create stereo target");
    auto pixel=[](int x,int y,int channel) { unsigned char p[4]{};glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);return p[channel]; };
    auto clearScene=[]() {
        Stereo().BeginFrame();glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glDepthMask(1);
        glClearColor(0,0,1,1);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    };
    auto seedCapture=[](bool empty) {
        Check(MirrorReflectionPrepare() && MirrorReflectionBegin(),"verified mirror capture starts");
        Check(MirrorReflectionCapturing(),"capture scope active");
        for(int eye=0;eye<2;++eye) {
            Stereo().SetEyeViewport(eye);glDepthMask(1);glClearDepth(empty ? 1 : .5);
            glClearColor(empty ? 0.f : (eye ? 0.f : 1.f),empty ? 0.f : (eye ? 1.f : 0.f),0,empty ? 0.f : 1.f);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        }
        MirrorReflectionEnd();
    };
    clearScene();
    glViewport(7,8,200,190);glScissor(10,11,180,170);glEnable(GL_SCISSOR_TEST);glFrontFace(GL_CCW);
    const GLint oldFbo=Get(GL_DRAW_FRAMEBUFFER_BINDING);
    Check(MirrorReflectionPrepare() && MirrorReflectionBegin(),"first capture initializes resources");
    Check(Get(GL_FRONT_FACE)==GL_CW,"reflection reverses front face winding");
    MirrorReflectionEnd();
    Check(Get(GL_DRAW_FRAMEBUFFER_BINDING)==oldFbo && Get(GL_FRONT_FACE)==GL_CCW,"capture restores FBO and winding");
    GLint viewport[4]{},scissor[4]{};glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_SCISSOR_BOX,scissor);
    Check(viewport[0]==7 && viewport[3]==190 && scissor[0]==10 && glIsEnabled(GL_SCISSOR_TEST),"capture restores viewport/scissor");
    Check(!MirrorReflectionPrepare(),"one capture per frame");MirrorReflectionPresent();
    for(int mode=0;mode<3;++mode) {
        clearScene();seedCapture(mode==1);
        if(mode==2) { // Occluding real geometry is closer than the mirror.
            gl::BindFramebuffer(GL_FRAMEBUFFER,Stereo().fbo());glDisable(GL_SCISSOR_TEST);
            glDepthMask(1);glClearDepth(.2);glClear(GL_DEPTH_BUFFER_BIT);
        }
        glDisable(GL_DEPTH_TEST);glDepthFunc(GL_GREATER);glDepthMask(1);glEnable(GL_BLEND);glEnable(GL_CULL_FACE);
        gl::ActiveTexture(GL_TEXTURE0+3);
        MirrorReflectionPresent();
        Check(Get(GL_ACTIVE_TEXTURE)==GL_TEXTURE0+3 && Get(GL_DRAW_FRAMEBUFFER_BINDING)==oldFbo,"composite restores FBO/texture unit");
        Check(!glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE) && Get(GL_DEPTH_FUNC)==GL_GREATER,"composite restores depth/cull/blend");
        gl::BindFramebuffer(GL_READ_FRAMEBUFFER,Stereo().fbo());
        for(int eye=0;eye<2;++eye) {
            Check(pixel(eye*256+128,128,mode==0 ? eye : 2)>220,"each eye samples its own image; empty/occluded glass keeps the world");
            Check(pixel(eye*256+20,128,2)>220,"reflection clipped outside mirror width");
            Check(pixel(eye*256+128,20,2)>220,"reflection clipped outside mirror height");
        }
        Check(glGetError()==GL_NO_ERROR,"capture/composite produces no GL errors");
    }
    // Material passes append to one image. Alpha-only hair edges must survive
    // even with no opaque depth beneath them, and composite over the glass.
    cfg.vrBrighterColors=false;glDisable(GL_FRAMEBUFFER_SRGB);clearScene();
    Check(!MirrorReflectionPrepare(1) && !MirrorReflectionPrepare(4),"transparent/shadow passes cannot start a capture");
    Check(MirrorReflectionPrepare(0) && MirrorReflectionBegin(),"opaque pass starts multi-pass image");
    for(int eye=0;eye<2;++eye) {
        glScissor(eye*256+125,124,8,8);glClearColor(.1f,.2f,.3f,1);glClearDepth(.5);glDepthMask(1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    }
    MirrorReflectionEnd();
    Check(!MirrorReflectionPrepare(0) && !MirrorReflectionPrepare(2),"duplicate or out-of-order passes do not replace the image");
    Check(MirrorReflectionPrepare(1) && MirrorReflectionBegin(),"transparent pass appends without clearing opaque pixels");
    for(int eye=0;eye<2;++eye) {
        Check(std::abs(pixel(eye*256+128,128,0)-26)<2,"opaque detail retained before transparent pass");
        glScissor(eye*256+141,124,5,8);glClearColor(.2f,.1f,0,.5f);glClear(GL_COLOR_BUFFER_BIT);
    }
    MirrorReflectionEnd();
    Check(MirrorReflectionPrepare(2) && MirrorReflectionBegin(),"additive pass appends without clearing transparent hair");
    MirrorReflectionEnd();
    BlendFuncSeparate(GL_SRC_ALPHA,GL_ONE,GL_ZERO,GL_ONE);
    BlendEquationSeparate(0x800A,0x800B);glEnable(GL_BLEND);
    MirrorReflectionPresent();
    Check(Get(0x80C9)==GL_SRC_ALPHA && Get(0x80C8)==GL_ONE && Get(0x80CB)==GL_ZERO && Get(0x80CA)==GL_ONE &&
          Get(0x8009)==0x800A && Get(0x883D)==0x800B && glIsEnabled(GL_BLEND),"composite restores separate native blend factors and equations");
    gl::BindFramebuffer(GL_READ_FRAMEBUFFER,Stereo().fbo());
    for(int eye=0;eye<2;++eye) {
        Check(std::abs(pixel(eye*256+128,128,0)-26)<2,"opaque material detail survives all three passes");
        Check(std::abs(pixel(eye*256+143,128,0)-51)<3 && std::abs(pixel(eye*256+143,128,2)-128)<3,
              "transparent hair with no depth blends over original blue glass instead of disappearing");
    }
    BlendEquationSeparate(0x8006,0x8006);glDisable(GL_BLEND);
    // Draw real triangles through the production reflection projection. The
    // unreflected fixture is inside the near plane; only its mirror image can
    // produce these pixels. This also checks the two supported color formats.
    const GLuint testVs=Compile(GL_VERTEX_SHADER,"#version 150\nin vec3 p;uniform mat4 transform;void main(){gl_Position=transform*vec4(p,1);}");
    const GLuint testFs=Compile(GL_FRAGMENT_SHADER,"#version 150\nout vec4 c;void main(){c=vec4(.25,.6,.1,1);}");
    GLuint drawProgram=gl::CreateProgram();gl::AttachShader(drawProgram,testVs);gl::AttachShader(drawProgram,testFs);gl::LinkProgram(drawProgram);
    gl::DeleteShader(testVs);gl::DeleteShader(testFs);
    GLint linked=0;gl::GetProgramiv(drawProgram,GL_LINK_STATUS,&linked);Check(linked!=0,"native-like mesh shader links");
    GLuint meshVao=0,meshVbo=0;gl::GenVertexArrays(1,&meshVao);gl::BindVertexArray(meshVao);gl::GenBuffers(1,&meshVbo);gl::BindBuffer(GL_ARRAY_BUFFER,meshVbo);
    const float mesh[]={34250,-24820,77180,34250,-24668,77180,34250,-24820,77436,
                        34250,-24820,77436,34250,-24668,77180,34250,-24668,77436};
    gl::BufferData(GL_ARRAY_BUFFER,sizeof(mesh),mesh,GL_STATIC_DRAW);
    const int attribute=gl::GetAttribLocation(drawProgram,"p"),matrix=gl::GetUniformLocation(drawProgram,"transform");
    gl::EnableVertexAttribArray(attribute);gl::VertexAttribPointer(attribute,3,GL_FLOAT,GL_FALSE,12,nullptr);
    for(bool brighter:{false,true}) {
        cfg.vrBrighterColors=brighter;glEnable(GL_FRAMEBUFFER_SRGB);clearScene();
        Check(MirrorReflectionPrepare() && MirrorReflectionBegin(),"mesh capture begins");
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(1);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
        gl::UseProgram(drawProgram);gl::BindVertexArray(meshVao);
        int centers[2]{};
        for(int eye=0;eye<2;++eye) {
            Stereo().SetEyeViewport(eye);
            float r[3][3]{},p[3]{};CameraViewFrame(r,p);
            mat4 transform{};VR().EyeProjection(Eye(eye),16,32768,transform);
            transform=Mul4(transform,AffineToMat4(VR().EyeView(Eye(eye))));
            MirrorReflectionProjection(transform,eye);
            transform=Mul4(transform,AffineToMat4(mirror::WorldView(r,p)));
            const float point[]={34250,-24744,77308,1};float clip[4]{};
            for(int row=0;row<4;++row) for(int col=0;col<4;++col) clip[row]+=transform.m[col*4+row]*point[col];
            Check(clip[3]>900 && clip[3]<950,"reflected body is behind the mirror at the physical reflected distance");
            centers[eye]=eye*256+int((clip[0]/clip[3]*.5f+.5f)*256);
            gl::UniformMatrix4fv(matrix,1,GL_FALSE,transform.m);glDrawArrays(GL_TRIANGLES,0,6);
        }
        MirrorReflectionEnd();MirrorReflectionPresent();gl::BindFramebuffer(GL_READ_FRAMEBUFFER,Stereo().fbo());
        for(int eye=0;eye<2;++eye) {
            const int red=pixel(centers[eye],128,0);
            Check(std::abs(red-(brighter ? 137 : 64))<3,"reflected mesh color matches the selected native color scheme");
            Check(pixel(centers[eye],128,1)>145,"reflected mesh appears at its per-eye projected position");
        }
        Check(centers[0]!=(centers[1]-256),"asymmetric frusta and IPD retain independent parallax");
        Check(glGetError()==GL_NO_ERROR,"actual reflected geometry renders without GL errors");
    }
    gl::BindVertexArray(0);gl::UseProgram(0);gl::DeleteBuffers(1,&meshVbo);gl::DeleteVertexArrays(1,&meshVao);gl::DeleteProgram(drawProgram);
    auto link=[](const std::string& vs,const std::string& fs) {
        const GLuint v=Compile(GL_VERTEX_SHADER,vs.c_str()),f=Compile(GL_FRAGMENT_SHADER,fs.c_str());
        Check(v && f,"native material shader compiles with reflected observer");
        const GLuint p=gl::CreateProgram();gl::AttachShader(p,v);gl::AttachShader(p,f);gl::LinkProgram(p);
        GLint ok=0;gl::GetProgramiv(p,GL_LINK_STATUS,&ok);Check(ok!=0,"native material shader pair links with visibility and mirror patches");
        gl::DeleteShader(v);gl::DeleteShader(f);return p;
    };
    for(bool env:{false,true}) {
        std::string v=env ? NativeEnvVertex : NativeCreatureVertex,f=env ? NativeEnvFragment : NativeCreatureFragment;
        Check(mirror::PatchObserver(v,f),"recognize audited native material camera convention");
        Check(!mirror::PatchObserver(v,f),"shader patch rejects duplicate observer injection");
        Check(motionhandskin::Patch(v,f),"mirror and first-person skin visibility patches coexist");
        Check(keypad::PatchLight(v,f),"keypad lighting coexists with hand and reflection shader patches");
        const auto p=link(v,f);gl::DeleteProgram(p);
    }
    // Run the shipped catsuit fragment shader on controlled textured pixels.
    // Its uncorrected Fresnel sees the back-facing normal from the real chase
    // camera and saturates. The reflected observer must retain dark detail.
    const std::string materialVertex="#version 150\nuniform vec4 testPosition;out vec3 vTexCoord;out float vLayer;out float vFog;out vec3 vNormal;out vec3 vPos;out vec3 vWorldPos;"
        "void main(){vec2 p=vec2((gl_VertexID==1)?3:-1,(gl_VertexID==2)?3:-1);gl_Position=vec4(p,0,1);"
        "vTexCoord=vec3(p*.5+.5,0);vLayer=0;vFog=1;vNormal=vec3(-1,0,0);vPos=testPosition.xyz;vWorldPos=vec3(0);}";
    std::string envFragment=NativeEnvFragment;Check(mirror::PatchObserver(NativeEnvVertex,envFragment),"patch native catsuit shader for pixel test");
    const GLuint material=link(materialVertex,envFragment);
    gl::UseProgram(material);GLuint materialVao=0;gl::GenVertexArrays(1,&materialVao);gl::BindVertexArray(materialVao);
    using Image3D=void(APIENTRY*)(GLenum,GLint,GLint,GLsizei,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
    const auto texImage3D=reinterpret_cast<Image3D>(wglGetProcAddress("glTexImage3D"));Check(texImage3D!=nullptr,"array texture upload available");
    GLuint materialTextures[2]{};glGenTextures(2,materialTextures);
    gl::ActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D_ARRAY,materialTextures[0]);
    const unsigned char cloth[]={13,13,13,255,38,38,38,255,128,128,128,255,128,128,128,255};
    texImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA8,2,1,2,0,GL_RGBA,GL_UNSIGNED_BYTE,cloth);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    gl::ActiveTexture(GL_TEXTURE0+1);glBindTexture(GL_TEXTURE_2D_ARRAY,materialTextures[1]);
    const float shadowDepth=1;texImage3D(GL_TEXTURE_2D_ARRAY,0,GL_DEPTH_COMPONENT24,1,1,1,0,GL_DEPTH_COMPONENT,GL_FLOAT,&shadowDepth);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,0x884C,0x884E);glTexParameteri(GL_TEXTURE_2D_ARRAY,0x884D,GL_LEQUAL);
    gl::Uniform1i(gl::GetUniformLocation(material,"sShadow"),1);gl::Uniform1i(gl::GetUniformLocation(material,"sNoise"),2);
    auto uniform=[&](const char* name,const float* value,int count=1) { gl::Uniform4fv(gl::GetUniformLocation(material,name),count,value); };
    const float white[]={1,1,1,1},params[]={1,0,0,0},light[]={0,1,0,0};
    float ambient[24]{};for(int i=0;i<6;++i) ambient[i*4]=ambient[i*4+1]=ambient[i*4+2]=.2f;
    uniform("uColor",white);uniform("uParams",params);uniform("uFogPlane",white);uniform("uLightPos",light);uniform("uAmbient",ambient,6);
    const auto identity=AffineToMat4(Affine::Identity());gl::UniformMatrix4fv(gl::GetUniformLocation(material,"uShadowMatrix"),1,GL_FALSE,identity.m);
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_FRAMEBUFFER_SRGB);
    gl::BindFramebuffer(GL_FRAMEBUFFER,Stereo().fbo());glViewport(0,0,256,256);
    for(float distance:{464.f,2600.f}) {
        cameraX=mirror::Profiles[0].planeX+distance;
        Check(MirrorReflectionPrepare() && MirrorReflectionBegin(),"material test captures first-person and chase-camera positions");
        for(int eye=0;eye<2;++eye) {
            mat4 dummy=identity;MirrorReflectionProjection(dummy,eye);
            float observer[4]{};MirrorReflectionObserver(observer);
            Check(std::fabs(observer[0]+2*distance)<.1f && std::fabs(std::fabs(observer[2])-32.f)<.1f,"highlight observer is reflected per eye in native world-relative coordinates");
            // Render into the capture with the shipped material, without the
            // geometry transform: two screen samples expose texture contrast.
            glDisable(GL_SCISSOR_TEST);glViewport(0,0,256,256);
            const float position[]={34250-cameraX,0,0,0},zero[4]{};uniform("testPosition",position);
            uniform("uMirrorObserver",zero);glDrawArrays(GL_TRIANGLES,0,3);
            Check(pixel(64,128,0)>245 && pixel(192,128,0)>245,"regression reproduces blown-out third-person catsuit using real native Fresnel");
            uniform("uMirrorObserver",observer);glDrawArrays(GL_TRIANGLES,0,3);
            const int dark=pixel(64,128,0),detail=pixel(192,128,0);
            std::printf("Catsuit observer: distance=%.0f eye=%d dark=%d detail=%d\n",distance,eye,dark,detail);
            Check(std::abs(dark-6)<3 && std::abs(detail-16)<3 && detail-dark>8,"reflected observer preserves dark catsuit color and texture contrast in both views");
        }
        MirrorReflectionEnd();float outside[4]{};MirrorReflectionObserver(outside);
        Check(outside[0]==0 && outside[1]==0 && outside[2]==0,"ordinary scene uses unchanged native material observer");
        MirrorReflectionPresent();gl::UseProgram(material);gl::BindVertexArray(materialVao);
    }
    Check(glGetError()==GL_NO_ERROR,"native shader material regression renders without GL errors");
    gl::UseProgram(0);gl::BindVertexArray(0);glDeleteTextures(2,materialTextures);gl::DeleteVertexArrays(1,&materialVao);gl::DeleteProgram(material);
    // Moving between the restrooms rebuilds the glass mesh for the selected
    // wall. Exercise both directions and reject mixed-room material passes.
    for(int roomIndex:{1,0,1,0}) {
        const auto& profile=mirror::Profiles[roomIndex];
        cameraX=profile.planeX+464;cameraY=(profile.top+profile.bottom)*.5f;
        cameraZ=(profile.glassZ[1][0]+profile.glassZ[1][1])*.5f;
        clearScene();Check(MirrorReflectionPrepare(0,&profile) && MirrorReflectionBegin(),"each audited restroom initializes its own mirror surface");
        for(int eye=0;eye<2;++eye) {
            Stereo().SetEyeViewport(eye);glClearColor(1,0,0,1);glClearDepth(.5);glDepthMask(1);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        }
        MirrorReflectionEnd();
        Check(!MirrorReflectionPrepare(1,&mirror::Profiles[1-roomIndex]),"room transition cannot reuse previous room's reflection passes");
        MirrorReflectionPresent();gl::BindFramebuffer(GL_READ_FRAMEBUFFER,Stereo().fbo());
        for(int eye=0;eye<2;++eye) {
            Check(pixel(eye*256+128,128,0)>220,"new restroom reflection covers the exact center pane");
            Check(pixel(eye*256+20,128,2)>220,"new restroom reflection remains clipped away from the wall and sinks");
        }
    }
    cameraY=-24744;cameraZ=77308;
    cameraX=mirror::Profiles[0].planeX+2600;
    Check(MirrorReflectionPrepare() && MirrorReflectionBegin(),"third-person chase camera can stand farther behind Lara");
    MirrorReflectionEnd();MirrorReflectionPresent();cameraX=34256;
    for(int gate=0;gate<8;++gate) {
        cfg.firstPersonMirrorReflections=gate!=0;cfg.monoTracking=gate==1;cfg.duplicateDraws=gate!=2;
        cfg.perEyeView=gate!=3;cfg.perEyeProjection=gate==4 ? 0 : 1;cfg.eyeOffsetMode=gate==5 ? 2 : 3;
        renderTarget.color_id=gate==6 ? 1 : 0;cameraX=gate==7 ? mirror::Profiles[0].planeX-10 : 34256;
        Check(!MirrorReflectionPrepare(),"unsupported rendering modes or back of mirror fail closed");MirrorReflectionPresent();
    }
    cfg=Config{};cfg.eyeOffsetMode=3;cfg.perEyeView=cfg.duplicateDraws=true;cfg.perEyeProjection=1;renderTarget={};cameraX=34256;
    Check(Stereo().Create(192,192),"resize eye targets");Check(MirrorReflectionPrepare(),"resize reflection target");
    Check(width==384 && height==192,"capture dimensions follow stereo resolution");
    MirrorReflectionShutdown();Check(!MirrorReflectionCapturing() && !fbo,"shutdown releases capture");
    Stereo().Destroy();wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);ReleaseDC(window,dc);DestroyWindow(window);
    std::printf("Mirror real-GL checks passed: %d\n",checks);return 0;
}
