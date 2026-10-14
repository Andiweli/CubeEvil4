#include "radio_term.h"
#include "radio_term_gl.h"
#include "room_lighting_gl.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks{};
void require(bool value,const char *description){++checks;if(!value)throw std::runtime_error(description);}
struct Context{
 EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLConfig config{};EGLContext context{};EGLSurface surface{};
 Context(){require(eglInitialize(display,nullptr,nullptr),"EGL initialize");require(eglBindAPI(EGL_OPENGL_ES_API),"GLES API");EGLint n{};
 const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
 require(eglChooseConfig(display,attributes,&config,1,&n)&&n,"RGBA8/depth24 config");const EGLint size[]{EGL_WIDTH,960,EGL_HEIGHT,540,EGL_NONE};surface=eglCreatePbufferSurface(display,config,size);const EGLint version[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(display,config,EGL_NO_CONTEXT,version);require(eglMakeCurrent(display,surface,surface,context),"GLES current");}
 ~Context(){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);}
};
std::vector<unsigned char> render(const re4::RadioTermRuntime &runtime,int width=960,int height=540){glViewport(0,0,width,height);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(.01f,.02f,.03f,0);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);re4::DrawRadioTermGl(width,height,runtime);
 std::vector<unsigned char> pixels(width*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());require(glGetError()==GL_NO_ERROR,"Radio original GPU pixels");return pixels;}
void save(const std::string &path,const std::vector<unsigned char> &pixels){std::ofstream out(path,std::ios::binary);out<<"P6\n960 540\n255\n";for(int y=539;y>=0;--y)for(int x=0;x<960;++x)out.write(reinterpret_cast<const char*>(pixels.data()+(y*960+x)*4),3);require(bool(out),"Radio export");}
void call(const std::string &files,unsigned number,const std::string &output){auto data=std::make_shared<re4::RadioTermData>(files,number,1);require(data->sequence().size()==(number?7u:10u),"Original call0/1 OpeSeq entry counts");require(data->sequence().back().frame==(number?430:810),"Original codec frame end");require(data->voice()->block==1 && data->voice()->number==0x8c+number && !data->voice()->left.empty(),"Original radio DSP stream");
 re4::RadioTermRuntime runtime(data);require(runtime.actors().size()==11,"Original Leon five and Hunnigan six model layers");auto audio=runtime.takeAudio();require(audio.size()==1 && audio[0].kind==re4::RadioTermAudio::Sample && audio[0].number==0x14,"Source open radio sample14");
 require(runtime.fadeAlpha()==255,"Original codec entry frame0 black fade");
 for(unsigned i=0;i<5;++i)runtime.tick30Hz();
 require(runtime.fadeAlpha()==0,"Source FadeSetW5 black-in ends in5ticks");
 const auto initial=render(runtime);require(std::count_if(initial.begin(),initial.end(),[](unsigned char v){return v>20;})>10000,"Original radio models and authored ID art visible");
 for(unsigned i=5;i<30;++i)runtime.tick30Hz();
 require(runtime.sequenceFrame()==0 && runtime.takeAudio().empty(),"Source30tick wait before stream request");runtime.tick30Hz(false,false,false,true,false);audio=runtime.takeAudio();require(audio.size()==1 && audio[0].kind==re4::RadioTermAudio::StreamRequest,"Source stream request after30tickwait");
 for(unsigned i=0;i<8;++i)runtime.tick30Hz(false,false,false,true,false);
 require(runtime.sequenceFrame()==0,"Original stream-ready wait preserves OpeSeq clock");runtime.tick30Hz();audio=runtime.takeAudio();require(audio.size()==1 && audio[0].kind==re4::RadioTermAudio::StreamStart,"Original stream ready starts talking");
 unsigned morph{},pages{},previous=~0u;while(!runtime.finished()){if(runtime.sequenceFrame()%37==0){const auto pixels=render(runtime);require(pixels!=initial,"Source talking radio frame changes original pixels");}
  for(unsigned i:{3u,8u})morph+=runtime.actors()[i]->meshRevision()>0;
  const auto &m=runtime.message();if(!m.words.empty() && m.number!=previous){++pages;previous=m.number;}runtime.tick30Hz();
 require(runtime.frame()<2000,"Radio original sequence terminates");}
 require(runtime.sequenceFrame()==unsigned(data->sequence().back().frame),"Source final OpeMesMove returns at sentinel before seqCnt increment");require(morph>100 && pages>2,"Actual mouth shapes and MDT dialogue pages");
 re4::RadioTermRuntime manual(data);for(unsigned i=0;i<40;++i)manual.tick30Hz();manual.tick30Hz(false,true);require(manual.manual(),"B stops stream and enters source manual mode");const unsigned frozen=manual.sequenceFrame();for(unsigned i=0;i<3;++i)manual.tick30Hz();require(manual.sequenceFrame()==frozen,"Manual dialogue stops OpeSeq clock");for(unsigned i=0;i<16 && !manual.finished();++i)manual.tick30Hz(true);require(manual.finished(),"A steps original OpeSeq to sentinel");
 re4::RadioTermRuntime close(data);for(unsigned i=0;i<40;++i)close.tick30Hz();close.tick30Hz(false,false,true);require(close.finished(),"START closes original radio call");
 re4::RadioTermRuntime waitingClose(data);for(unsigned i=0;i<30;++i)waitingClose.tick30Hz(false,false,true);
 require(!waitingClose.finished(),"START remains gated during initial30tick ope.wait");
 waitingClose.tick30Hz(false,false,true,true,false);require(waitingClose.finished(),"Source SsTermMain START closes even when OpeMesMove waits for stream-ready");
 re4::RadioTermRuntime paused(data);for(unsigned i=0;i<90;++i)paused.tick30Hz();const auto a=render(paused);
 {auto ukData=std::make_shared<re4::RadioTermData>(files,number,2);re4::RadioTermRuntime uk(ukData);for(unsigned i=0;i<90;++i)uk.tick30Hz();
  require(uk.data().language()==2 && uk.message().words==paused.message().words && render(uk)==a,"Source US/UK English uses shared Eng art and MDT language1");}
 const auto frame=paused.frame();for(unsigned i=0;i<30;++i)paused.tick30Hz(true,true,true,false);require(paused.frame()==frame && render(paused)==a,"Paused codec changes no clocks/pixels/input");
 re4::DestroyRadioTermGl();re4::CreateRadioTermGl(paused);require(render(paused)==a,"Codec GPU destroy/recreate retains exact CPU frame");render(paused,540,540);render(paused,960,360);save(output+"-call"+std::to_string(number)+".ppm",a);re4::DestroyRadioTermGl();
 std::cout<<"Radio call"<<number<<" PASS:11 source models, "<<data->sequence().size()<<" OpeSeq entries, end"<<data->sequence().back().frame<<", "<<pages<<" original messages, "<<morph<<" morph observations; DSP/skip/pause/fullscreen/context lifecycle\n";
}
}
int main(int argc,char **argv){try{if(argc!=3)throw std::runtime_error("Usage: m37_radio_gl_check DISC_FILES OUTPUT_PREFIX");Context context;call(argv[1],0,argv[2]);call(argv[1],1,argv[2]);std::cout<<checks<<" radio checks PASS\n";return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
