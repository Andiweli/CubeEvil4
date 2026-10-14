#include "graphics_options_gl.h"
#include "model_textures_gl.h"
#include <EGL/egl.h>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <stdexcept>
using namespace re4;
namespace {
constexpr int W=128,H=128;
void check(bool ok,const char *m){if(!ok)throw std::runtime_error(m);}
GLuint shader(GLenum kind,const char *src){auto id=glCreateShader(kind);glShaderSource(id,1,&src,nullptr);glCompileShader(id);GLint ok;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);check(ok,"compile shader");return id;}
GLuint program(const char *vert,const char *frag){auto p=glCreateProgram();auto v=shader(GL_VERTEX_SHADER,vert),f=shader(GL_FRAGMENT_SHADER,frag);glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);check(ok,"link shader");glDeleteShader(v);glDeleteShader(f);return p;}
std::vector<unsigned char> pixels(){std::vector<unsigned char> p(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,p.data());check(glGetError()==GL_NO_ERROR,"read pixels");return p;}
unsigned changes(const std::vector<unsigned char> &a,const std::vector<unsigned char> &b){unsigned n=0;for(unsigned i=0;i<a.size();i+=4)if(a[i]!=b[i])++n;return n;}
}
int main(){try {
 check(requestedMsaa==0 && userAf==8 && userMipmapping && userRoomFps.load(),"Requested default options");
 setenv("EGL_PLATFORM","surfaceless",1);auto d=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major,minor,n;
 check(eglInitialize(d,&major,&minor)&&eglBindAPI(EGL_OPENGL_ES_API),"EGL init");
 const EGLint attrs[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};EGLConfig config;
 check(eglChooseConfig(d,attrs,&config,1,&n)&&n,"EGL config");const EGLint sz[]{EGL_WIDTH,W,EGL_HEIGHT,H,EGL_NONE},cx[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
 auto s=eglCreatePbufferSurface(d,config,sz);auto c=eglCreateContext(d,config,EGL_NO_CONTEXT,cx);check(eglMakeCurrent(d,s,s,c),"EGL current");
 glDisable(GL_DITHER);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glViewport(0,0,W,H);GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
 const auto edge=program(R"(#version 300 es
void main(){vec2 p[3]=vec2[3](vec2(-.83,-.77),vec2(.81,-.43),vec2(-.37,.91));gl_Position=vec4(p[gl_VertexID],0,1);})",R"(#version 300 es
precision highp float;out vec4 color;void main(){color=vec4(1);})");
 auto drawEdge=[&](int count){requestedMsaa=count;{MultisampleFrameGl frame(W,H);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glUseProgram(edge);glDrawArrays(GL_TRIANGLES,0,3);}return pixels();};
 const auto no=drawEdge(0),aa=drawEdge(4);const auto msaaDiff=changes(no,aa);check(msaaActual==4 && msaaDiff>50,"4x MSAA must change real rendered edge pixels");
 unsigned partial=0;for(unsigned i=0;i<aa.size();i+=4)if(aa[i]>0&&aa[i]<255)++partial;check(partial>50,"MSAA must produce intermediate coverage");
 requestedMsaa=0;DestroyMsaaGl();
 const auto filter=program(R"(#version 300 es
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,0,1);})",R"(#version 300 es
precision highp float;uniform sampler2D image;out vec4 color;
void main(){vec2 uv=vec2(gl_FragCoord.x/128.0*16.0,gl_FragCoord.y/128.0*.25);color=textureGrad(image,uv,dFdx(uv),dFdy(uv));})");
 Texture texture;texture.width=texture.height=128;texture.rgba.resize(128*128*4);
 for(unsigned y=0;y<128;++y)for(unsigned x=0;x<128;++x)for(unsigned ch=0;ch<4;++ch)texture.rgba[(y*128+x)*4+ch]=ch==3?255:((y/2)%2?255:0);
 GLuint image;glGenTextures(1,&image);glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glBindTexture(GL_TEXTURE_2D,image);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,texture.rgba.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 ModelTextureLodGl lod;lod.append(texture);glUseProgram(filter);glUniform1i(glGetUniformLocation(filter,"image"),0);
 auto drawAf=[&](int value){userAf=value;lod.bind({image});glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);return pixels();};
 const auto base=drawAf(1),af2=drawAf(2),filtered=drawAf(8),again=drawAf(1);check(lod.anisotropySupported,"AF extension available");
 const auto afDiff=changes(base,filtered),afLevelDiff=changes(af2,filtered);check(afLevelDiff>1000,"2x and 8x AF must differ with the same generated mip chain");check(afDiff>1000,"AF must change grazing texture pixels, including originally mipless images");check(base==again,"No AF must restore original no-mipmap pixels");
 userMipmapping=false;const auto noMips=drawAf(8);GLint maximumLevel=-1;glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,&maximumLevel);
 check(maximumLevel==0,"Mipmapping OFF limits sampling to the base level while AF remains selected");
 check(userAf==8,"Mipmapping switch preserves AF selection");
 userMipmapping=true;const auto mipsRestored=drawAf(8);check(mipsRestored==filtered,"Mipmapping ON restores previous filtered pixels");
 const auto mipDiff=changes(noMips,mipsRestored);check(mipDiff>1000,"Mipmapping changes actual minified pixels");
 userRoomFps.store(false);check(!userRoomFps.load(),"Room/FPS OFF");userRoomFps.store(true);
 std::cout<<"Fix10 graphics PASS: MSAA changed "<<msaaDiff<<" pixels / "<<partial<<" intermediate edge pixels; AF changed "<<afDiff<<" pixels (2x -> 8x: "<<afLevelDiff<<"); No AF restores source image; mipmapping changed "<<mipDiff<<" pixels\n";
 glDeleteTextures(1,&image);glDeleteProgram(edge);glDeleteProgram(filter);glDeleteVertexArrays(1,&vao);eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);eglDestroySurface(d,s);eglTerminate(d);return 0;
}catch(const std::exception &e){std::cerr<<"Fix10 graphics FAILED: "<<e.what()<<'\n';return 1;}}
