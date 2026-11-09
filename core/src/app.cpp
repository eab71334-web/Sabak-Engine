#include "app.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
namespace fs = std::filesystem;
namespace sk {
static const Col WHITE=Col::hex(0xffffff), MUTED=Col::hex(0xaab4d0), DIM=Col::hex(0x7d88a8);
static const Col VIOLET=Col::hex(0x8b5cf6), BLUE=Col::hex(0x2563eb), CYAN=Col::hex(0x22d3ee), PINK=Col::hex(0xf472b6), GREEN=Col::hex(0x34d399);
static float hash1(float n){float s=std::sin(n*127.1f)*43758.5453f;return s-std::floor(s);}
static void squish(float p,float&x,float&y,float&w,float&h){float k=1-0.045f*p;x+=w*(1-k)/2;y+=h*(1-k)/2;w*=k;h*=k;}
static const char* TNAME[3]={"Empty World","Village Demo","Open Terrain"};
static const char* TDESC[3]={"A clean green field. Start from nothing.","Houses, forest and crystals. A ready scene.","Wide hills and dense forest to explore."};

bool App::init(Renderer& r,const std::string& data,int w,int h){
  r_=&r;data_=data;projDir_=data+"/SabakProjects";importDir_=data+"/Import";std::error_code ec;fs::create_directories(projDir_,ec);fs::create_directories(importDir_,ec);
  W_=w;H_=h;if(!ui.init(h/393.0f))return false;for(int& m:meshIds_)m=-1;sctx_.scene=&scene_;sctx_.input=&in_;bg_.buildTemplate(1);rescan();rescanImports();return true;}
void App::resize(int w,int h){W_=w;H_=h;}
const char* App::screenName() const{return scr_==HOME?"home":scr_==TEMPLATES?"templates":scr_==IMPORT?"import":"editor";}
static std::string field(const std::string& js,const std::string& key){size_t p=js.find("\""+key+"\"");if(p==std::string::npos)return "";p=js.find(':',p);if(p==std::string::npos)return "";p++;
  while(p<js.size()&&(js[p]==' '||js[p]=='"'))p++;size_t e=p;while(e<js.size()&&js[e]!='"'&&js[e]!=','&&js[e]!='}')e++;return js.substr(p,e-p);}
void App::rescan(){projects_.clear();std::error_code ec;for(auto& e:fs::directory_iterator(projDir_,ec)){if(!e.is_directory())continue;std::ifstream f(e.path()/"project.json");if(!f)continue;
    std::stringstream ss;ss<<f.rdbuf();std::string js=ss.str();Project p;p.dir=e.path().string();p.name=field(js,"name");if(p.name.empty())p.name=e.path().filename().string();
    p.tmpl=std::atoi(field(js,"template").c_str());p.created=std::atol(field(js,"created").c_str());projects_.push_back(p);}
  std::sort(projects_.begin(),projects_.end(),[](const Project&a,const Project&b){return a.created>b.created;});}
void App::rescanImports(){found_.clear();std::error_code ec;for(auto& e:fs::directory_iterator(importDir_,ec))if(e.is_directory()&&fs::exists(e.path()/"project.json"))found_.push_back(e.path().filename().string());}
void App::createProject(int tmpl){int n=(int)projects_.size()+1;std::string name;fs::path d;do{name="Project "+std::to_string(n++);d=fs::path(projDir_)/name;}while(fs::exists(d));
  std::error_code ec;fs::create_directories(d/"scenes",ec);fs::create_directories(d/"assets",ec);
  std::ofstream f(d/"project.json");f<<"{\"name\":\""<<name<<"\",\"template\":"<<tmpl<<",\"created\":"<<(long)std::time(nullptr)<<",\"engine\":\"0.1\"}\n";f.close();
  rescan();for(size_t i=0;i<projects_.size();i++)if(projects_[i].name==name)openProject((int)i);toast("Project created");}
void App::openProject(int i){if(i<0||i>=(int)projects_.size())return;cur_=i;play_=false;scripts_.clear();sel_=-1;addMenu_=false;expanded_.clear();hierScroll_=0;
  std::string sp=projects_[i].dir+"/scenes/main.scene";if(!scene_.load(sp)){scene_.buildTemplate(projects_[i].tmpl);std::error_code ec;fs::create_directories(projects_[i].dir+"/scenes",ec);scene_.save(sp);}
  ecYaw_=0.8f;ecPitch_=0.5f;ecDist_=26;ecTarget_={0,3,0};rebuildRows();go(EDITOR);}
void App::importFound(int i){if(i<0||i>=(int)found_.size())return;std::error_code ec;fs::path src=fs::path(importDir_)/found_[i],dst=fs::path(projDir_)/found_[i];
  fs::copy(src,dst,fs::copy_options::recursive|fs::copy_options::overwrite_existing,ec);if(ec){toast("Import failed");return;}
  // refresh created time so it appears first
  rescan();rescanImports();toast("Imported "+found_[i]);for(size_t k=0;k<projects_.size();k++)if(projects_[k].name==found_[i]||fs::path(projects_[k].dir).filename()==found_[i])openProject((int)k);}
void App::go(Screen s){scr_=s;tr_=0;held_=-1;heldId_=-1;if(s==IMPORT)rescanImports();if(s==HOME)rescan();}
void App::onTap(int id){
  if(id==1)go(TEMPLATES);else if(id==2)go(IMPORT);else if(id==3)go(HOME);else if(id==4){rescanImports();toast("Refreshed");}
  else if(id==5){if(play_)stopPlay();saveScene(true);go(HOME);}
  else if(id>=100&&id<500)editorTap(id);
  else if(id>=10&&id<20)openProject(id-10);else if(id>=20&&id<23)createProject(id-20);else if(id>=30&&id<40)importFound(id-30);}
void App::key(const std::string& k,bool d){int i=k=="W"?0:k=="A"?1:k=="S"?2:3;keyDown_[i]=d;}
bool App::btn(int id,float x,float y,float w,float h){btns_.push_back({id,x,y+0,w,h});return heldId_==id;}

void App::update(float dt){t_+=dt;tr_=std::min(1.0f,tr_+dt*3.2f);float e=1-std::pow(1-tr_,3);A_=e;oy_=(1-e)*16;
  for(int i=0;i<512;i++){float tg=(heldId_==i)?1.f:0.f;press_[i]+=(tg-press_[i])*std::min(1.0f,dt*20);}
  if(toastT_>0)toastT_-=dt;fpsAcc_+=dt;fpsN_++;if(fpsAcc_>0.5f){fps_=fpsN_/fpsAcc_;fpsAcc_=0;fpsN_=0;}
  if(scr_==EDITOR)editorUpdate(dt);}

void App::pointer(int type,int id,float px,float py){float s=ui.s();float x=px/s,y=py/s;
  if(type==0){
    for(int i=(int)prevBtns_.size()-1;i>=0;i--){auto&b=prevBtns_[i];if(x>=b.x&&x<=b.x+b.w&&y>=b.y&&y<=b.y+b.h){heldId_=b.id;held_=id;return;}}
    if(scr_==EDITOR){editorPointer(0,id,px,py);return;}
  }else if(type==1){
    if(scr_==EDITOR)editorPointer(1,id,px,py);
    if(held_==id&&heldId_>=0){bool over=false;for(auto&b:prevBtns_)if(b.id==heldId_&&x>=b.x&&x<=b.x+b.w&&y>=b.y&&y<=b.y+b.h)over=true;if(!over)heldId_=-1;}
  }else{
    if(held_==id&&heldId_>=0){int h=heldId_;heldId_=-1;held_=-1;onTap(h);return;}
    if(scr_==EDITOR)editorPointer(2,id,px,py);}
}

// ---------- drawing helpers ----------
void App::plusIcon(float cx,float cy,float sz,Col c){R(cx-sz/2,cy-sz*0.09f,sz,sz*0.18f,sz*0.09f,c,c);R(cx-sz*0.09f,cy-sz/2,sz*0.18f,sz,sz*0.09f,c,c);}
void App::downIcon(float cx,float cy,float sz,Col c){R(cx-sz*0.08f,cy-sz*0.5f,sz*0.16f,sz*0.55f,sz*0.08f,c,c);
  for(int i=0;i<5;i++){float w=sz*(0.62f-i*0.12f);R(cx-w/2,cy-sz*0.02f+i*sz*0.075f,w,sz*0.09f,sz*0.04f,c,c);}R(cx-sz*0.5f,cy+sz*0.42f,sz,sz*0.12f,sz*0.06f,c,c);}
void App::cubeIcon(float cx,float cy,float sz,float a){float h=sz/2;SH(cx-h,cy-h+6,sz,sz,sz*0.26f,12,VIOLET.with(0.55f*a));
  R(cx-h,cy-h,sz,sz,sz*0.26f,Col::hex(0xa78bfa,a),Col::hex(0x6d28d9,a));RING(cx-h,cy-h,sz,sz,sz*0.26f,1.2f,WHITE.with(0.35f*a));
  R(cx-h*0.62f,cy-h*0.62f,sz*0.62f,sz*0.62f,sz*0.14f,WHITE.with(0.22f*a),WHITE.with(0.05f*a));R(cx-h*0.2f,cy-h*0.2f,sz*0.52f,sz*0.52f,sz*0.14f,Col::hex(0x4c1d95,0.55f*a),Col::hex(0x4c1d95,0.2f*a));}

void App::drawBackground(){float W=W_/ui.s();float H=393;
  
  { float a=t_*0.1f+0.6f;Vec3 tg{0,bg_.height(0,0)+2,0};Vec3 c{std::sin(a)*24,bg_.height(0,0)+9+std::sin(t_*0.2f)*1.5f,std::cos(a)*24};float gh=bg_.height(c.x,c.z)+1;if(c.y<gh)c.y=gh;renderScene(bg_,c,tg,0.95f); }
  ui.begin(W_,H_);
  // dark gradient wash so the UI stays readable, then drifting colour glows and sparkles
  ui.rrect(0,0,W,H,0,Col::hex(0x090b1f,0.55f),Col::hex(0x090b1f,0.84f));
  ui.glow(160+std::sin(t_*0.3f)*40,110+std::cos(t_*0.25f)*20,300,VIOLET.with(0.42f));
  ui.glow(W-120+std::cos(t_*0.2f)*50,H-40+std::sin(t_*0.3f)*20,340,CYAN.with(0.30f));
  ui.glow(W*0.55f+std::sin(t_*0.22f)*60,30,230,PINK.with(0.20f));
  for(int i=0;i<34;i++){float sp=6+hash1(i+3)*14;float x=hash1(i*7.3f)*W+std::sin(t_*0.5f+i)*6;float y=H-std::fmod(t_*sp+hash1(i*3.1f)*H,H);float sz=0.8f+hash1(i*5.7f)*1.8f;
    float tw=0.35f+0.65f*std::fabs(std::sin(t_*1.2f+i*2.1f));ui.circle(x,y,sz,WHITE.with(0.55f*tw),WHITE.with(0.55f*tw));}
}

void App::header(const std::string& title,const std::string& sub){
  { float x=22,y=18,w=40,h=40;bool p=btn(3,x,y,w,h);float k=pressOf(3);squish(k,x,y,w,h);(void)p;
    SH(x,y,w,h,20,10,Col::hex(0x000000,0.35f));R(x,y,w,h,20,WHITE.with(0.16f),WHITE.with(0.07f));RING(x,y,w,h,20,1,WHITE.with(0.25f));T("<",x+w/2,y+5,24,WHITE,F_BOLD,1);}
  T(title,76,14,26,WHITE,F_BOLD);T(sub,77,48,13,MUTED);}

void App::drawHome(){float W=W_/ui.s();float H=393;
  // top bar: logo + status chip
  SH(22,16,38,38,11,10,VIOLET.with(0.6f));R(22,16,38,38,11,Col::hex(0xa78bfa),Col::hex(0x2563eb));RING(22,16,38,38,11,1,WHITE.with(0.4f));T("S",41,19,24,WHITE,F_BOLD,1);
  T("SABAK ENGINE",70,16,15,WHITE,F_BOLD);T("v0.1  |  native C++ core",70,35,11,DIM);
  float cw=150;R(W-cw-24,20,cw,28,14,WHITE.with(0.10f),WHITE.with(0.04f));RING(W-cw-24,20,cw,28,14,1,WHITE.with(0.18f));
  ui.circle(W-cw-8,34+oy_,4,C(GREEN),C(GREEN));GL(W-cw-8,34,14,GREEN.with(0.5f));T("OpenGL ES 3  |  "+std::to_string((int)(fps_+0.5f))+" fps",W-cw+4,25,11,MUTED);
  // headline
  T("Build your",38,78,40,WHITE,F_BOLD);T("next game.",38,122,40,CYAN,F_BOLD);
  T("Create a project or bring one in. Everything runs",40,182,13,MUTED);T("natively on your iPhone.",40,201,13,MUTED);
  // primary button
  { float x=38,y=236,w=232,h=64;btn(1,x,y,w,h);float k=pressOf(1);squish(k,x,y,w,h);
    SH(x,y+6,w,h,20,18,VIOLET.with(0.65f));R(x,y,w,h,20,Col::hex(0x9b6cff),Col::hex(0x2f5fe8));RING(x,y,w,h,20,1.2f,WHITE.with(0.45f));
    R(x+2,y+2,w-4,h*0.45f,18,WHITE.with(0.20f),WHITE.with(0.0f));
    ui.circle(x+34,y+h/2+oy_,19,C(WHITE.with(0.22f)),C(WHITE.with(0.10f)));plusIcon(x+34,y+h/2,19,WHITE);
    T("New Project",x+64,y+13,19,WHITE,F_BOLD);T("Start from a template",x+64,y+38,12,WHITE.with(0.8f)); }
  // secondary button
  { float x=286,y=236,w=210,h=64;btn(2,x,y,w,h);float k=pressOf(2);squish(k,x,y,w,h);
    SH(x,y+4,w,h,20,14,Col::hex(0x000000,0.35f));R(x,y,w,h,20,WHITE.with(0.13f),WHITE.with(0.05f));RING(x,y,w,h,20,1.3f,WHITE.with(0.30f));
    ui.circle(x+34,y+h/2+oy_,19,C(CYAN.with(0.25f)),C(CYAN.with(0.10f)));downIcon(x+34,y+h/2-1,17,CYAN);
    T("Import Project",x+64,y+13,19,WHITE,F_BOLD);T("From the Files app",x+64,y+38,12,MUTED); }
  // right panel: recent projects
  float px=W-322,py=72,pw=298,ph=290;SH(px,py,pw,ph,24,24,Col::hex(0x000000,0.45f));R(px,py,pw,ph,24,WHITE.with(0.11f),WHITE.with(0.04f));RING(px,py,pw,ph,24,1.2f,WHITE.with(0.20f));
  R(px+1,py+1,pw-2,50,23,WHITE.with(0.07f),WHITE.with(0.0f));
  T("Recent projects",px+20,py+16,15,WHITE,F_BOLD);
  { std::string n=std::to_string(projects_.size());float cwid=22+n.size()*7;R(px+pw-cwid-18,py+17,cwid,22,11,VIOLET.with(0.35f),VIOLET.with(0.2f));T(n,px+pw-cwid/2-18,py+20,12,WHITE,F_BOLD,1);}
  if(projects_.empty()){cubeIcon(px+pw/2,py+128,64,A_);T("No projects yet",px+pw/2,py+176,16,WHITE,F_BOLD,1);T("Tap New Project to create",px+pw/2,py+202,12,MUTED,F_REG,1);T("your first game.",px+pw/2,py+220,12,MUTED,F_REG,1);}
  else for(int i=0;i<(int)projects_.size()&&i<3;i++){auto&p=projects_[i];float x=px+14,y=py+58+i*74,w=pw-28,h=66;btn(10+i,x,y,w,h);float k=pressOf(10+i);squish(k,x,y,w,h);
    R(x,y,w,h,18,WHITE.with(0.09f+0.06f*k),WHITE.with(0.04f));RING(x,y,w,h,18,1,WHITE.with(0.14f));
    unsigned c1s[3]={0xf472b6,0x22d3ee,0xfbbf24},c2s[3]={0x8b5cf6,0x2563eb,0xf97316};int ci=(p.name.size()+p.tmpl)%3;
    SH(x+8,y+8,50,50,15,8,Col::hex(c2s[ci],0.5f));R(x+8,y+8,50,50,15,Col::hex(c1s[ci]),Col::hex(c2s[ci]));RING(x+8,y+8,50,50,15,1,WHITE.with(0.4f));
    T(p.name.substr(p.name.size()>8?8:0,1).empty()?"?":p.name.substr(p.name.size()>8?p.name.size()-1:0,1),x+33,y+14,24,WHITE,F_BOLD,1);
    T(p.name.size()>16?p.name.substr(0,15)+"..":p.name,x+70,y+12,14,WHITE,F_BOLD);
    long days=(long)((std::time(nullptr)-p.created)/86400);std::string when=days<=0?"Today":std::to_string(days)+"d ago";
    T(std::string(TNAME[std::min(2,std::max(0,p.tmpl))])+"  |  "+when,x+70,y+37,11,MUTED);T(">",x+w-18,y+20,17,MUTED,F_BOLD,1);}
  T("Tip: drag the right half of the screen to look around inside a project.",40,H-24,11,DIM);
}


void App::drawTemplates(){float W=W_/ui.s();header("Choose a template","Pick a starting point. You can change everything later.");
  float cw=(W-36*2-18*2)/3,y0=96,ch=266;
  for(int i=0;i<3;i++){float x=36+i*(cw+18),y=y0,w=cw,h=ch;btn(20+i,x,y,w,h);float k=pressOf(20+i);squish(k,x,y,w,h);
    SH(x,y+6,w,h,22,22,Col::hex(0x000000,0.5f));R(x,y,w,h,22,Col::hex(0x1a1f3d,0.92f),Col::hex(0x12162e,0.92f));RING(x,y,w,h,22,1.2f,WHITE.with(0.18f+0.15f*k));
    float ax=x+8,ay=y+8,aw=w-16,ah=128;
    if(i==0){R(ax,ay,aw,ah,16,Col::hex(0x60a5fa),Col::hex(0xdbeafe));ui.circle(ax+aw*0.72f,ay+34+oy_,18,C(Col::hex(0xfef08a)),C(Col::hex(0xfde047)));GL(ax+aw*0.72f,ay+34,46,Col::hex(0xfde047,0.5f));
      R(ax,ay+ah*0.62f,aw,ah*0.38f,16,Col::hex(0x4ade80),Col::hex(0x15803d));}
    else if(i==1){R(ax,ay,aw,ah,16,Col::hex(0xfb923c),Col::hex(0xfde68a));ui.circle(ax+aw*0.5f,ay+62+oy_,22,C(Col::hex(0xfff7ed,0.9f)),C(Col::hex(0xfde68a,0.9f)));
      R(ax,ay+ah*0.62f,aw,ah*0.38f,16,Col::hex(0x84cc16),Col::hex(0x3f6212));
      for(int h2=0;h2<3;h2++){float hx=ax+22+h2*(aw-70)/2.0f,hy=ay+ah*0.52f,hw=30;R(hx,hy+10,hw,20,3,Col::hex(0xfde8c8),Col::hex(0xd6b78e));R(hx-4,hy,hw+8,12,5,Col::hex(0xdc2626),Col::hex(0x991b1b));}
      for(int t2=0;t2<4;t2++){float tx=ax+10+t2*(aw-30)/3.0f;ui.circle(tx+6,ay+ah*0.84f+oy_,7,C(Col::hex(0x166534)),C(Col::hex(0x14532d)));}}
    else {R(ax,ay,aw,ah,16,Col::hex(0xa78bfa),Col::hex(0xf0abfc));ui.circle(ax+aw*0.25f,ay+36+oy_,15,C(Col::hex(0xffffff,0.85f)),C(Col::hex(0xfce7f3,0.85f)));
      R(ax,ay+ah*0.50f,aw*0.60f,ah*0.50f,26,Col::hex(0x22c55e),Col::hex(0x15803d));R(ax+aw*0.38f,ay+ah*0.42f,aw*0.62f,ah*0.58f,30,Col::hex(0x16a34a),Col::hex(0x14532d));
      R(ax+aw*0.12f,ay+ah*0.74f,aw*0.76f,ah*0.26f,14,Col::hex(0x166534),Col::hex(0x052e16));}
    RING(ax,ay,aw,ah,16,1,WHITE.with(0.35f));
    T(TNAME[i],x+18,y+148,17,WHITE,F_BOLD);std::string d=TDESC[i];size_t cut=d.find(". ");
    T(d,x+18,y+174,11,MUTED);
    float bx=x+16,by=y+h-52,bw=w-32,bh=36;unsigned c1[3]={0x34d399,0xfb923c,0xa78bfa},c2[3]={0x059669,0xea580c,0x7c3aed};
    R(bx,by,bw,bh,18,Col::hex(c1[i]),Col::hex(c2[i]));RING(bx,by,bw,bh,18,1,WHITE.with(0.4f));R(bx+2,by+2,bw-4,bh*0.45f,16,WHITE.with(0.22f),WHITE.with(0.0f));
    T("Create project",bx+bw/2,by+8,14,WHITE,F_BOLD,1);(void)cut;}
}

void App::drawImport(){float W=W_/ui.s();header("Import a project","Bring a project folder in from the Files app.");
  float lx=36,ly=96,lw=360,lh=266;SH(lx,ly,lw,lh,22,22,Col::hex(0x000000,0.5f));R(lx,ly,lw,lh,22,Col::hex(0x1a1f3d,0.92f),Col::hex(0x12162e,0.92f));RING(lx,ly,lw,lh,22,1.2f,WHITE.with(0.18f));
  T("How it works",lx+20,ly+16,16,WHITE,F_BOLD);
  const char* st[4]={"Open the Files app","Go to On My iPhone > Sabak Engine","Put your project folder in Import","Come back here and tap Refresh"};
  for(int i=0;i<4;i++){float y=ly+54+i*50;unsigned cs[4]={0x8b5cf6,0x2563eb,0x06b6d4,0x10b981};ui.circle(lx+36,y+14+oy_,14,C(Col::hex(cs[i])),C(Col::hex(cs[i]).mul(0.7f)));
    T(std::to_string(i+1),lx+36,y+5,15,WHITE,F_BOLD,1);T(st[i],lx+62,y+6,12.5f>13?13:13,WHITE.with(0.92f),F_REG);}
  float rx=412,rw=W-412-36;SH(rx,ly,rw,lh,22,22,Col::hex(0x000000,0.5f));R(rx,ly,rw,lh,22,WHITE.with(0.10f),WHITE.with(0.04f));RING(rx,ly,rw,lh,22,1.2f,WHITE.with(0.2f));
  T("Found in Import",rx+20,ly+16,16,WHITE,F_BOLD);
  { float x=rx+rw-112,y=ly+12,w=94,h=30;btn(4,x,y,w,h);float k=pressOf(4);squish(k,x,y,w,h);R(x,y,w,h,15,CYAN.with(0.28f),CYAN.with(0.12f));RING(x,y,w,h,15,1,CYAN.with(0.6f));T("Refresh",x+w/2,y+5,13,WHITE,F_BOLD,1);}
  if(found_.empty()){cubeIcon(rx+rw/2,ly+120,58,A_);T("No projects found",rx+rw/2,ly+166,15,WHITE,F_BOLD,1);T("Put a folder with a project.json",rx+rw/2,ly+192,12,MUTED,F_REG,1);T("inside Import, then Refresh.",rx+rw/2,ly+210,12,MUTED,F_REG,1);}
  else for(int i=0;i<(int)found_.size()&&i<3;i++){float x=rx+14,y=ly+56+i*66,w=rw-28,h=58;btn(30+i,x,y,w,h);float k=pressOf(30+i);squish(k,x,y,w,h);
    R(x,y,w,h,16,WHITE.with(0.10f+0.06f*k),WHITE.with(0.04f));RING(x,y,w,h,16,1,WHITE.with(0.15f));R(x+8,y+8,42,42,12,Col::hex(0x22d3ee),Col::hex(0x2563eb));
    T(found_[i].substr(0,1),x+29,y+12,20,WHITE,F_BOLD,1);T(found_[i].size()>18?found_[i].substr(0,17)+"..":found_[i],x+62,y+10,14,WHITE,F_BOLD);T("Tap to import",x+62,y+32,11,MUTED);
    R(x+w-74,y+14,60,30,15,GREEN.with(0.9f),Col::hex(0x059669));T("Import",x+w-44,y+19,12,WHITE,F_BOLD,1);}
}

void App::render(){
  btns_.clear();
  if(scr_==EDITOR){drawEditor();}
  else{drawBackground();if(scr_==HOME)drawHome();else if(scr_==TEMPLATES)drawTemplates();else drawImport();}
  if(toastT_>0){float W=W_/ui.s();float a=std::min(1.0f,toastT_*3);float w=ui.measure(toast_,14,F_BOLD)+48;float x=(W-w)/2,y=332;
    ui.shadow(x,y,w,38,19,14,Col::hex(0,0.5f*a));ui.rrect(x,y,w,38,19,Col::hex(0x10b981,a),Col::hex(0x047857,a));ui.text(toast_,W/2,y+9,14,WHITE.with(a),F_BOLD,1);}
  ui.end();prevBtns_=btns_;
}
}
