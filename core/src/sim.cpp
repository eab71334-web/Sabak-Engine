// Sabak Core simulator (headless). Reads commands (stdin or a script file), one per line:
//   size W H | tap X Y (pixels) | down X Y | move X Y | up X Y | key W|A|S|D down|up | step N [dt] | shot FILE.png | state | reset
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "app.h"
#include "platform_egl.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
int main(int argc,char** argv){
  if(!eglHeadlessInit()){std::puts("{\"error\":\"no GL context\"}");return 1;}
  int W=852,H=393;const char* dd=getenv("SABAK_DATA");std::string data=dd?dd:"./simdata";
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
    else if(c=="state")std::printf("{\"frames\":%d,\"screen\":\"%s\",\"hero\":[%.2f,%.2f,%.2f]}\n",frames,app->screenName(),app->game.hero.x,app->game.hero.y,app->game.hero.z);
    else std::printf("{\"error\":\"unknown command %s\"}\n",c.c_str());}
  return 0;}
