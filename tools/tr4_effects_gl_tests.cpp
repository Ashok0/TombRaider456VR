// Hidden GL-context regression of the production TR4 BC7 upload transform.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <cstdarg>
#include <chrono>
#include "../src/TR4EffectsGL.h"
void Log(const char* s) { std::puts(s); }
void LogF(const char* f,...) { va_list a;va_start(a,f);std::vprintf(f,a);va_end(a);std::puts(""); }
int checks=0;
void Check(bool ok,const char* label) { ++checks;if(!ok) {std::printf("FAIL: %s\n",label);std::exit(1);} }
using Image3=void(APIENTRY*)(GLenum,GLint,GLenum,GLsizei,GLsizei,GLsizei,GLint,GLsizei,const void*);
using Sub3=void(APIENTRY*)(GLenum,GLint,GLint,GLint,GLint,GLsizei,GLsizei,GLsizei,GLenum,GLsizei,const void*);
int main(int argc,char** argv) {
    using namespace tr::effects;
    Check(argc==2,"pass installed game directory (read-only)");
    Check(Identify("4\\TEX\\3115.DDS")==Texture::Sunrays,"TR4 ray path");
    Check(Identify("4\\tex\\effect.dds")==Texture::Atlas,"TR4 atlas case insensitive");
    for(auto path:{"5\\TEX\\3115.DDS","6\\TEX\\EFFECT.DDS","4\\TEX\\13115.DDS","4\\TEX\\3115.DDS.bak","TEX\\3115.DDS",""})
        Check(Identify(path)==Texture::None,"other game/unresolved/partial filename excluded");
    Check(Identify(nullptr)==Texture::None,"null path excluded");
    Check(Strength(-1)==0 && Strength(3)==2 && Strength(NAN)==1,"strength sanitization");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"TR4EffectsTest";wc.style=CS_OWNDC;
    Check(RegisterClassW(&wc)!=0,"register hidden window");
    HWND window=CreateWindowW(wc.lpszClassName,L"Effects test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
    HDC dc=GetDC(window);PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;
    Check(SetPixelFormat(dc,ChoosePixelFormat(dc,&pfd),&pfd)!=0,"set pixel format");
    HGLRC legacy=wglCreateContext(dc);Check(legacy && wglMakeCurrent(dc,legacy),"create GL context");
    using Create=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
    auto create=reinterpret_cast<Create>(wglGetProcAddress("wglCreateContextAttribsARB"));Check(create!=nullptr,"core context available");
    const int attribs[]={0x2091,3,0x2092,2,0x9126,1,0};HGLRC core=create(dc,nullptr,attribs);
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(legacy);Check(core && wglMakeCurrent(dc,core),"activate GL 3.2");
    Check(gl::Load(),"load GL API");std::printf("GL: %s\n",glGetString(GL_VERSION));
    auto image3=gpu::Proc<Image3>("glCompressedTexImage3D");auto sub3=gpu::Proc<Sub3>("glCompressedTexSubImage3D");
    auto get=gpu::Proc<gpu::GetCompressed>("glGetCompressedTexImage");
    Check(image3 && sub3 && get,"compressed array API");
    const auto started=std::chrono::steady_clock::now();
    for(auto kind:{Texture::Sunrays,Texture::Atlas}) {
        const int size=kind==Texture::Sunrays?512:2048;
        const std::string path=std::string(argv[1])+"\\4\\TEX\\"+(kind==Texture::Sunrays?"3115.DDS":"EFFECT.DDS");
        std::ifstream file(path,std::ios::binary);Check(bool(file),"stock DDS opens read-only");
        std::vector<uint8_t> dds((std::istreambuf_iterator<char>(file)),{});
        Check(dds.size()>148 && !std::memcmp(dds.data(),"DDS ",4),"DDS header");
        auto u32=[&](size_t p){uint32_t n;std::memcpy(&n,dds.data()+p,4);return n;};
        Check(u32(12)==size && u32(16)==size && u32(28)==6 && u32(128)==98,"expected six-mip BC7 resource");
        GLuint array=0,other=0,tex2=0,pbos[2]{};glGenTextures(1,&array);glGenTextures(1,&other);glGenTextures(1,&tex2);gl::GenBuffers(2,pbos);
        size_t offset=148;double sumBefore=0,sumAfter=0;
        for(int mip=0;mip<6;++mip) {
            const int w=size>>mip;const size_t bytes=size_t(w)*w;
            Check(offset+bytes<=dds.size(),"mip bounded by DDS");
            const uint8_t* native=dds.data()+offset;offset+=bytes;
            std::vector<uint8_t> layers(bytes*3);for(int z=0;z<3;++z) std::memcpy(layers.data()+z*bytes,native,bytes);
            glBindTexture(GL_TEXTURE_2D_ARRAY,array);image3(GL_TEXTURE_2D_ARRAY,mip,gpu::BC7,w,w,3,0,GLsizei(layers.size()),layers.data());
            Check(glGetError()==0,"create native BC7 array mip");
            std::vector<uint8_t> before(size_t(w)*w*4*3);glGetTexImage(GL_TEXTURE_2D_ARRAY,mip,GL_RGBA,GL_UNSIGNED_BYTE,before.data());
            gl::ActiveTexture(GL_TEXTURE0+3);glBindTexture(GL_TEXTURE_2D_ARRAY,other);glBindTexture(GL_TEXTURE_2D,tex2);
            gl::BindBuffer(gpu::PackBuffer,pbos[0]);gl::BindBuffer(gpu::UnpackBuffer,pbos[1]);
            for(int i=0;i<12;++i) glPixelStorei(gpu::TransferState::fields[i],i<2?8:7);
            std::vector<uint8_t> enhanced;
            Check(PrepareUpload(array,1,mip,kind,1,native,enhanced),"native mip enhanced");
            GLint n=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&n);Check(n==GL_TEXTURE0+3,"active unit preserved");
            glGetIntegerv(gpu::ArrayBinding,&n);Check(n==GLint(other),"array binding preserved");
            glGetIntegerv(GL_TEXTURE_BINDING_2D,&n);Check(n==GLint(tex2),"2D binding preserved");
            glGetIntegerv(gpu::PackBinding,&n);Check(n==GLint(pbos[0]),"pack PBO preserved");
            glGetIntegerv(gpu::UnpackBinding,&n);Check(n==GLint(pbos[1]),"unpack PBO preserved");
            for(int i=0;i<12;++i) {glGetIntegerv(gpu::TransferState::fields[i],&n);Check(n==(i<2?8:7),"pixel store preserved");}
            gl::BindBuffer(gpu::PackBuffer,0);gl::BindBuffer(gpu::UnpackBuffer,0);
            for(int i=0;i<12;++i) glPixelStorei(gpu::TransferState::fields[i],i<2?4:0);
            glBindTexture(GL_TEXTURE_2D_ARRAY,array);
            std::vector<uint8_t> actual(layers.size());get(GL_TEXTURE_2D_ARRAY,mip,actual.data());
            Check(actual==layers,"staging never mutates original array");
            sub3(GL_TEXTURE_2D_ARRAY,mip,0,0,1,w,w,1,gpu::BC7,GLsizei(bytes),enhanced.data());
            get(GL_TEXTURE_2D_ARRAY,mip,actual.data());
            Check(!std::memcmp(actual.data(),native,bytes) && !std::memcmp(actual.data()+2*bytes,native,bytes),"neighboring layers remain exact");
            std::vector<uint8_t> after(before.size());glGetTexImage(GL_TEXTURE_2D_ARRAY,mip,GL_RGBA,GL_UNSIGNED_BYTE,after.data());
            for(int y=0;y<w;++y) for(int x=0;x<w;++x) {
                const auto i=(size_t(w)*w+size_t(y)*w+x)*4;
                Check(std::abs(int(before[i+3])-int(after[i+3]))<=2,"alpha quantization bounded to two levels");
                if(before[i+3]==0) Check(after[i+3]==0,"fully transparent stays transparent");
                if(!(before[i]|before[i+1]|before[i+2])) Check(!(after[i]|after[i+1]|after[i+2]),"additive black remains exact");
                if(kind==Texture::Atlas && AtlasGain((2*x+1)*1024/w,(2*y+1)*1024/w)==1.f)
                    Check(!std::memcmp(before.data()+i,after.data()+i,4),"unselected atlas sprite remains exact");
                for(int c=0;c<3;++c) {sumBefore+=before[i+c];sumAfter+=after[i+c];}
            }
            std::vector<uint8_t> repeat;
            Check(PrepareUpload(array,1,mip,kind,1,native,repeat) && repeat==enhanced,"reload is deterministic, no cumulative brightening");
            Check(!PrepareUpload(array,1,mip,kind,0,native,repeat),"zero strength passes native");
            Check(!PrepareUpload(array,3,mip,kind,1,native,repeat),"out-of-range layer rejected");
            Check(!PrepareUpload(array,1,6,kind,1,native,repeat),"unexpected mip rejected");
            Check(!PrepareUpload(array,1,mip,kind==Texture::Atlas?Texture::Sunrays:Texture::Atlas,1,native,repeat),"wrong dimensions rejected");
            Check(glGetError()==0,"no GL errors");gl::ActiveTexture(GL_TEXTURE0);
        }
        std::printf("%s total RGB gain %.3fx across six mips\n",kind==Texture::Sunrays?"Sunrays":"Atlas",sumAfter/sumBefore);
        Check(sumAfter>sumBefore*1.01,"effects measurably brighter");
        glDeleteTextures(1,&array);glDeleteTextures(1,&other);glDeleteTextures(1,&tex2);gl::DeleteBuffers(2,pbos);
    }
    std::printf("PASS: %d checks, %.2fs including repeated encoding\n",checks,std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count());
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(core);ReleaseDC(window,dc);DestroyWindow(window);return 0;
}
