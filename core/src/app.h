// Sabak Core - application shell: launcher (home / templates / import) + project editor view.
#pragma once
#include <string>
#include <vector>
#include "game.h"
#include "ui.h"
namespace sk {
struct Project { std::string name,dir; int tmpl=1; long created=0; };
class App {
public:
  bool init(Renderer& r,const std::string& dataDir,int w,int h);
  void resize(int w,int h);
  void update(float dt);
  void render();
  void pointer(int type,int id,float x,float y);   // type 0 down, 1 move, 2 up. x,y in framebuffer pixels
  void key(const std::string& k,bool down);
  const char* screenName() const;
  bool quit=false; Game game;
private:
  enum Screen { HOME,TEMPLATES,IMPORT,EDITOR };
  struct Btn { int id; float x,y,w,h; };
  void go(Screen s); void createProject(int tmpl); void openProject(int i); void rescan(); void rescanImports(); void importFound(int i);
  void toast(const std::string& m){toast_=m;toastT_=2.4f;}
  void drawBackground(); void drawHome(); void drawTemplates(); void drawImport(); void drawHud(); void header(const std::string& title,const std::string& sub);
  bool btn(int id,float x,float y,float w,float h);     // registers a hit area (design units); returns true if held
  float pressOf(int id){return id>=0&&id<64?press_[id]:0.f;}
  void onTap(int id);
  Col C(Col c){c.a*=A_;return c;}
  void R(float x,float y,float w,float h,float r,Col a,Col b){ui.rrect(x,y+oy_,w,h,r,C(a),C(b));}
  void RING(float x,float y,float w,float h,float r,float bw,Col c){ui.ring(x,y+oy_,w,h,r,bw,C(c));}
  void SH(float x,float y,float w,float h,float r,float bl,Col c){ui.shadow(x,y+oy_,w,h,r,bl,C(c));}
  void GL(float x,float y,float rad,Col c){ui.glow(x,y+oy_,rad,C(c));}
  void T(const std::string& t,float x,float y,float sz,Col c,int f=F_REG,int al=0){ui.text(t,x,y+oy_,sz,C(c),f,al);}
  void plusIcon(float cx,float cy,float sz,Col c); void downIcon(float cx,float cy,float sz,Col c); void cubeIcon(float cx,float cy,float sz,float a);
  Renderer* r_=nullptr; UI ui; std::string data_,projDir_,importDir_; int W_=0,H_=0; float t_=0,A_=1,oy_=0,tr_=1,toastT_=0,fps_=60,fpsAcc_=0; int fpsN_=0;
  Screen scr_=HOME; std::string toast_; std::vector<Project> projects_; std::vector<std::string> found_; int cur_=-1;
  std::vector<Btn> btns_,prevBtns_; float press_[64]={0}; int held_=-1,heldId_=-1;
  int stickId_=-1,lookId_=-1; float sx0_=0,sy0_=0,lx_=0,ly_=0;
};
}
