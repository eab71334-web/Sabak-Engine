// Sabak Core simulator (headless). Reads commands (stdin or a script file), one per line:
//   size W H | tap X Y (pixels) | down X Y | move X Y | up X Y | key W|A|S|D down|up | step N [dt] | shot FILE.png | state | reset
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "app.h"
#include "model.h"
#include "platform_egl.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
int main(int argc,char** argv){
  if(!eglHeadlessInit()){std::puts("{\"error\":\"no GL context\"}");return 1;}
  int W=852,H=393;const char* dd=getenv("SABAK_DATA");std::string data=dd?dd:"./simdata";
  {const char* ad=getenv("SABAK_ASSETS");sk::g_assetDir=ad?ad:"../../assets";}
  sk::Renderer r;if(!r.init(W,H)){std::puts("{\"error\":\"renderer init failed\"}");return 1;}
  auto* app=new sk::App();if(!app->init(r,data,W,H)){std::puts("{\"error\":\"app init failed\"}");return 1;}
  std::ifstream f;if(argc>1)f.open(argv[1]);std::istream& in=argc>1?f:std::cin;std::string line;int frames=0;
  while(std::getline(in,line)){std::istringstream s(line);std::string c;s>>c;if(c.empty()||c[0]=='#')continue;
    if(c=="size"){s>>W>>H;r.resize(W,H);app->resize(W,H);}
    else if(c=="tap"||c=="down"||c=="move"||c=="up"){float x,y;s>>x>>y;if(c=="tap"||c=="down"){app->update(0.0f);app->render();}
      if(c=="tap"){app->pointer(0,1,x,y);app->update(0.05f);app->pointer(2,1,x,y);app->update(0.5f);}else app->pointer(c=="down"?0:c=="move"?1:2,1,x,y);}
    else if(c=="key"){std::string k,d;s>>k>>d;app->key(k,d=="down");}
    else if(c=="step"){int n=1;float dt=1/60.f;s>>n;s>>dt;for(int i=0;i<n;i++){app->update(dt);frames++;}}
    else if(c=="reset"){std::filesystem::remove_all(data);std::puts("{\"reset\":true}");}
    else if(c=="shot"){std::string p;s>>p;app->render();std::vector<unsigned char> px;r.readPixels(px);stbi_write_png(p.c_str(),r.width(),r.height(),4,px.data(),r.width()*4);
      std::printf("{\"shot\":\"%s\",\"screen\":\"%s\",\"draws\":%d}\n",p.c_str(),app->screenName(),r.stats.draws);}
    else if(c=="cam"){float a[7];for(float& q:a)s>>q;app->setDbgCam(true,{a[0],a[1],a[2]},{a[3],a[4],a[5]},a[6]);}
    else if(c=="camoff"){app->setDbgCam(false,{0,0,0},{0,0,0},0.9f);}
    else if(c=="cinebegin"){int w,h;s>>w>>h;W=w;H=h;r.resize(W,H);app->resize(W,H);app->cineBegin(W,H);}
    else if(c=="cinejump"){float t;s>>t;app->cineJump(t);}
    else if(c=="cine"){int fps=24;std::string out;float t0=0,t1=-1;s>>fps>>out>>t0>>t1;if(t1<0)t1=app->cineDuration();
      char cmd[1024];std::snprintf(cmd,sizeof cmd,"ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgba -s %dx%d -r %d -i - -vf format=yuv420p -c:v libx264 -preset medium -crf 18 %s",r.width(),r.height(),fps,out.c_str());
      FILE* pf=popen(cmd,"w");int n=0;float dtf=1.f/fps;if(t0>0)app->cineJump(t0);
      for(float t=t0;t<t1;t+=dtf){app->update(dtf);app->render();std::vector<unsigned char> px;r.readPixels(px);fwrite(px.data(),1,px.size(),pf);if((n++%24)==0){std::fprintf(stderr,"cine t=%.1f\n",t);}}
      pclose(pf);std::printf("{\"cine\":\"%s\",\"frames\":%d}\n",out.c_str(),n);}
    else if(c=="cineaudio"){std::string p;s>>p;std::vector<float> a;int rate=24000;if(app->cineAudio(a,rate)){FILE* f2=fopen(p.c_str(),"wb");std::vector<short> pcm(a.size());for(size_t i=0;i<a.size();i++)pcm[i]=(short)std::max(-32767.f,std::min(32767.f,a[i]*32767.f));
      unsigned dl=(unsigned)(pcm.size()*2),sr=rate,br=rate*2;short fmt=1,ch=1,ba=2,bits=16;unsigned fl=16,rl=36+dl;fwrite("RIFF",1,4,f2);fwrite(&rl,4,1,f2);fwrite("WAVEfmt ",1,8,f2);fwrite(&fl,4,1,f2);fwrite(&fmt,2,1,f2);fwrite(&ch,2,1,f2);fwrite(&sr,4,1,f2);fwrite(&br,4,1,f2);fwrite(&ba,2,1,f2);fwrite(&bits,2,1,f2);fwrite("data",1,4,f2);fwrite(&dl,4,1,f2);fwrite(pcm.data(),2,pcm.size(),f2);fclose(f2);std::printf("{\"audio\":\"%s\"}\n",p.c_str());}}
    else if(c=="state")std::printf("{\"frames\":%d,\"screen\":\"%s\",\"entities\":%d,\"selected\":%d,\"playing\":%d}\n",frames,app->screenName(),app->entityCount(),app->selected(),app->playing()?1:0);
    else std::printf("{\"error\":\"unknown command %s\"}\n",c.c_str());}
  return 0;}
