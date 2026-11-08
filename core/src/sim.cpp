// Sabak Core simulator (headless). Reads commands (stdin or a script file), one per line:
//   size W H | press W|A|S|D | release K | releaseall | stick X Y | look DYAW DPITCH | step N [dt] | shot FILE.png | state
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include "game.h"
#include "platform_egl.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
int main(int argc,char** argv){
  if(!eglHeadlessInit()){std::puts("{\"error\":\"no GL context\"}");return 1;}
  sk::Renderer r;int W=844,H=390;if(!r.init(W,H)){std::puts("{\"error\":\"renderer init failed\"}");return 1;}
  sk::Game g;g.init(r);std::ifstream f;if(argc>1)f.open(argv[1]);std::istream& in=argc>1?f:std::cin;
  std::string line;int frames=0;
  while(std::getline(in,line)){std::istringstream s(line);std::string c;s>>c;if(c.empty()||c[0]=='#')continue;
    if(c=="size"){s>>W>>H;r.resize(W,H);}
    else if(c=="press"){std::string k;s>>k;g.press(k);}else if(c=="release"){std::string k;s>>k;g.release(k);}else if(c=="releaseall")g.releaseAll();
    else if(c=="stick"){float x,y;s>>x>>y;g.stick(x,y);}else if(c=="look"){float a,b;s>>a>>b;g.look(a,b);}
    else if(c=="step"){int n=1;float dt=1/60.f;s>>n;s>>dt;for(int i=0;i<n;i++){g.update(dt);frames++;}}
    else if(c=="shot"){std::string p;s>>p;g.draw(r);std::vector<unsigned char> px;r.readPixels(px);stbi_write_png(p.c_str(),r.width(),r.height(),4,px.data(),r.width()*4);
      std::printf("{\"shot\":\"%s\",\"draws\":%d,\"tris\":%ld}\n",p.c_str(),r.stats.draws,r.stats.tris);}
    else if(c=="state")std::printf("{\"frames\":%d,\"hero\":[%.2f,%.2f,%.2f],\"yaw\":%.2f,\"time\":%.2f}\n",frames,g.hero.x,g.hero.y,g.hero.z,g.heroYaw,g.time);
    else std::printf("{\"error\":\"unknown command %s\"}\n",c.c_str());}
  return 0;}
