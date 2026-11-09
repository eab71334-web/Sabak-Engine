#include "app.h"
#include "theme.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
namespace fs = std::filesystem;
namespace sk {
static const Col WHITE=Col::hex(0xffffff);
using namespace sk::th;
static float hash1(float n){float s=std::sin(n*127.1f)*43758.5453f;return s-std::floor(s);}
static void squish(float p,float&x,float&y,float&w,float&h){float k=1-0.045f*p;x+=w*(1-k)/2;y+=h*(1-k)/2;w*=k;h*=k;}
static const char* TNAME[3]={"Empty World","Village Demo","Open Terrain"};
static const char* TDESC[3]={"Flat terrain with a hero. A clean start.","Village, forest and props. A ready scene.","Open hills and dense forest. No buildings."};

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
void App::go(Screen s){scr_=s;tr_=0;trT_=0;held_=-1;heldId_=-1;if(s==IMPORT)rescanImports();if(s==HOME)rescan();}
void App::onTap(int id){
  if(id==1)go(TEMPLATES);else if(id==2)go(IMPORT);else if(id==3)go(HOME);else if(id==4){rescanImports();toast("Refreshed");}
  else if(id==5){if(play_)stopPlay();saveScene(true);go(HOME);}
  else if(id>=100&&id<500)editorTap(id);
  else if(id>=10&&id<20)openProject(id-10);else if(id>=20&&id<23)createProject(id-20);else if(id>=30&&id<40)importFound(id-30);}
void App::key(const std::string& k,bool d){int i=k=="W"?0:k=="A"?1:k=="S"?2:3;keyDown_[i]=d;}
bool App::btn(int id,float x,float y,float w,float h){btns_.push_back({id,x,y+0,w,h});return heldId_==id;}

void App::update(float dt){t_+=dt;trT_+=dt;A_=1;oy_=0;
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
void App::sect(float k){float e=std::min(1.0f,std::max(0.0f,trT_*2.2f-k*0.16f));e=1-std::pow(1-e,3);A_=e;oy_=(1-e)*12;}
void App::panel(float x,float y,float w,float h,const std::string& title){SH(x,y+2,w,h,5,10,Col::hex(0,0.35f));R(x,y,w,h,5,BG1.with(0.95f),BG1.with(0.95f));RING(x,y,w,h,5,1,LINE);
  R(x+1,y+1,w-2,25,4,BG2,BG2);R(x+1,y+14,w-2,12,0,BG2,BG2);R(x+1,y+26,w-2,1,0,LINE,LINE);T(title,x+12,y+8,10.5f,TXT2,F_BOLD);}
void App::fbtn(int id,float x,float y,float w,float h,const std::string& label,int kind,bool active){btn(id,x,y,w,h);float k=pressOf(id);Col a,b,ln,tx=TXT;
  if(kind==0){a=ACC.mul(1+0.12f*k);b=ACC2.mul(1+0.12f*k);ln=ACC.mul(1.25f);}else if(kind==2){a=BAD.with(0.9f);b=BAD2.with(0.9f);ln=BAD.mul(1.2f);}
  else if(kind==3){a=BG3.with(0.0f+0.9f*k);b=a;ln=Col{0,0,0,0};tx=TXT2;}else{a=BG3.mul(1+0.25f*k);b=BG2.mul(1+0.25f*k);ln=LINE2;}
  if(active){a=ACC.with(0.9f);b=ACC2.with(0.9f);ln=ACC.mul(1.25f);tx=WHITE;}
  R(x,y,w,h,4,a,b);if(ln.a>0)RING(x,y,w,h,4,1,ln);if(!label.empty())T(label,x+w/2,y+h/2-7.0f,12,tx,F_BOLD,1);}
void App::plusIcon(float cx,float cy,float sz,Col c){R(cx-sz/2,cy-1,sz,2,1,c,c);R(cx-1,cy-sz/2,2,sz,1,c,c);}
void App::downIcon(float cx,float cy,float sz,Col c){R(cx-1,cy-sz*0.5f,2,sz*0.62f,1,c,c);for(int i=0;i<4;i++){float w=sz*(0.56f-i*0.14f);R(cx-w/2,cy+sz*0.02f+i*sz*0.07f,w,2,1,c,c);}R(cx-sz*0.5f,cy+sz*0.44f,sz,2,1,c,c);}
void App::cubeIcon(float cx,float cy,float sz,float a){float h=sz/2;Col c=TXT3.with(0.8f*a);RING(cx-h,cy-h,sz,sz,6,1.5f,c);RING(cx-h*0.55f,cy-h*0.55f,sz*0.55f,sz*0.55f,3,1.5f,c);R(cx-h*0.55f+sz*0.55f-2,cy-h*0.55f+sz*0.55f-2,0,0,0,c,c);}

void App::drawBackground(){float W=W_/ui.s();float H=393;A_=1;oy_=0;
  { float a=t_*0.045f+0.6f;Vec3 tg{0,bg_.height(0,0)+2,0};Vec3 c{std::sin(a)*26,bg_.height(0,0)+8+std::sin(t_*0.15f)*1.0f,std::cos(a)*26};float gh=bg_.height(c.x,c.z)+1;if(c.y<gh)c.y=gh;renderScene(bg_,c,tg,0.92f); }
  ui.begin(W_,H_);
  ui.rrect(0,0,W,H,0,BG0.with(0.72f),BG0.with(0.92f));
  // very soft moving light + a few slow dust motes (kept subtle)
  ui.glow(W*0.30f+std::sin(t_*0.12f)*80,H*0.55f+std::cos(t_*0.10f)*30,360,ACC.with(0.10f));
  ui.glow(W*0.82f+std::cos(t_*0.09f)*40,H*0.25f,260,Col::hex(0x6a7a92,0.08f));
  for(int i=0;i<14;i++){float sp=3+hash1(i+3)*5;float x=hash1(i*7.3f)*W+std::sin(t_*0.2f+i)*10;float y=H-std::fmod(t_*sp+hash1(i*3.1f)*H,H);float tw=0.25f+0.25f*std::sin(t_*0.6f+i*2.0f);ui.circle(x,y,0.9f,WHITE.with(tw*0.6f),WHITE.with(tw*0.6f));}
  // faint vignette bars
  ui.rrect(0,0,W,60,0,BG0.with(0.55f),BG0.with(0.0f));ui.rrect(0,H-60,W,60,0,BG0.with(0.0f),BG0.with(0.6f));
}

void App::header(const std::string& title,const std::string& sub){sect(0);
  R(0,0,W_/ui.s(),50,0,BG1.with(0.94f),BG1.with(0.94f));R(0,49,W_/ui.s(),1,0,LINE,LINE);
  fbtn(3,12,9,32,32,"",1);T("<",28,13,19,TXT,F_BOLD,1);
  T(title,58,9,15,TXT,F_BOLD);T(sub,58,29,10.5f,TXT3);}

void App::drawHome(){float W=W_/ui.s();float H=393;
  sect(0);R(0,0,W,46,0,BG1.with(0.94f),BG1.with(0.94f));R(0,45,W,1,0,LINE,LINE);
  R(14,11,24,24,5,ACC,ACC2);T("S",26,13,14,WHITE,F_BOLD,1);T("SABAK ENGINE",48,10,13,TXT,F_BOLD);T("Version 0.1",48,27,10,TXT3);
  { std::string st="C++ native   |   OpenGL ES 3   |   "+std::to_string((int)(fps_+0.5f))+" fps";T(st,W-16,17,10.5f,TXT3,F_REG,2);}
  sect(1);T("Sabak Engine",36,76,38,TXT,F_BOLD);T("Real-time 3D engine for iPhone",38,124,14,TXT2);
  R(38,152,36,2,1,ACC,ACC);
  sect(2);
  { float x=38,y=176,w=236,h=76;btn(1,x,y,w,h);float k=pressOf(1);SH(x,y+3,w,h,6,12,Col::hex(0,0.45f));R(x,y,w,h,6,ACC.mul(1+0.1f*k),ACC2.mul(1+0.1f*k));RING(x,y,w,h,6,1,ACC.mul(1.3f));
    R(x+14,y+20,36,36,5,Col::hex(0,0.22f),Col::hex(0,0.22f));plusIcon(x+32,y+38,16,WHITE);T("New Project",x+62,y+20,15,WHITE,F_BOLD);T("Create from a template",x+62,y+42,11,WHITE.with(0.78f)); }
  { float x=288,y=176,w=210,h=76;btn(2,x,y,w,h);float k=pressOf(2);SH(x,y+3,w,h,6,12,Col::hex(0,0.35f));R(x,y,w,h,6,BG2.mul(1+0.3f*k),BG1.mul(1+0.3f*k));RING(x,y,w,h,6,1,LINE2);
    R(x+14,y+20,36,36,5,BG3,BG3);downIcon(x+32,y+37,15,TXT);T("Import Project",x+62,y+20,15,TXT,F_BOLD);T("From the Files app",x+62,y+42,11,TXT2); }
  sect(3);
  { const char* lb[3]={"ENGINE","RENDERER","SCRIPTING"};const char* vl[3]={"0.1 preview","OpenGL ES 3","C++17"};float cw=(460-16)/3.0f;
    for(int i=0;i<3;i++){float x=38+i*(cw+8),y=268,w=cw,h=44;R(x,y,w,h,5,BG1.with(0.9f),BG1.with(0.9f));RING(x,y,w,h,5,1,LINE);T(lb[i],x+12,y+8,9,TXT3,F_BOLD);T(vl[i],x+12,y+22,12,TXT,F_BOLD);} }
  sect(4);
  float px=W-322,py=62,pw=298,ph=H-62-34;panel(px,py,pw,ph,"RECENT PROJECTS");
  { std::string n=std::to_string(projects_.size());T(n,px+pw-14,py+8,10.5f,TXT3,F_BOLD,2);}
  if(projects_.empty()){cubeIcon(px+pw/2,py+110,46,A_);T("No projects yet",px+pw/2,py+150,13,TXT,F_BOLD,1);T("Create a new project or import one.",px+pw/2,py+170,11,TXT3,F_REG,1);}
  else for(int i=0;i<(int)projects_.size()&&i<4;i++){auto&p=projects_[i];float x=px+8,y=py+34+i*60,w=pw-16,h=54;btn(10+i,x,y,w,h);float k=pressOf(10+i);
    R(x,y,w,h,4,BG3.with(0.0f+0.9f*k),BG3.with(0.0f+0.9f*k));R(x,y+h-1,w,1,0,LINE.with(0.6f),LINE.with(0.6f));
    unsigned cs[4]={0x3d7dd8,0x3aa08a,0xc08a3e,0x8a6fc0};Col cc=Col::hex(cs[(p.name.size()+p.tmpl)%4]);R(x+4,y+8,38,38,4,BG2,BG2);RING(x+4,y+8,38,38,4,1,LINE2);R(x+4,y+8,3,38,0,cc,cc);
    T(p.name.empty()?"?":p.name.substr(p.name.size()-1,1),x+25,y+17,16,cc,F_BOLD,1);
    T(p.name.size()>20?p.name.substr(0,19)+"..":p.name,x+54,y+11,12.5f,TXT,F_BOLD);
    long days=(long)((std::time(nullptr)-p.created)/86400);std::string when=days<=0?"Today":std::to_string(days)+"d ago";
    T(std::string(TNAME[std::min(2,std::max(0,p.tmpl))])+"  |  "+when,x+54,y+30,10.5f,TXT3);T(">",x+w-14,y+16,14,TXT3,F_BOLD,1);}
  sect(5);T("Tip: pinch to zoom, drag to orbit, tap an object to select it.",38,H-26,10.5f,TXT3);
}


void App::drawTemplates(){float W=W_/ui.s();header("New project","Select a template to start from.");
  float cw=(W-36*2-16*2)/3,y0=70,ch=H_/ui.s()-y0-26;
  for(int i=0;i<3;i++){sect(1+i);float x=36+i*(cw+16),y=y0,w=cw,h=ch;btn(20+i,x,y,w,h);float k=pressOf(20+i);
    SH(x,y+3,w,h,6,12,Col::hex(0,0.4f));R(x,y,w,h,6,BG1.with(0.96f),BG1.with(0.96f));RING(x,y,w,h,6,1,k>0.05f?ACC:LINE2);
    float ax=x+8,ay=y+8,aw=w-16,ah=122;
    if(i==0){R(ax,ay,aw,ah,4,Col::hex(0x3d4c5e),Col::hex(0x93a5b8));ui.circle(ax+aw*0.74f,ay+32+oy_,13,C(Col::hex(0xe8e2cf,0.9f)),C(Col::hex(0xe8e2cf,0.9f)));R(ax,ay+ah*0.64f,aw,ah*0.36f,4,Col::hex(0x59713f),Col::hex(0x37492a));}
    else if(i==1){R(ax,ay,aw,ah,4,Col::hex(0x4a4150),Col::hex(0xc29a7a));R(ax,ay+ah*0.64f,aw,ah*0.36f,4,Col::hex(0x5d6b3a),Col::hex(0x2f3a22));
      for(int h2=0;h2<3;h2++){float hx=ax+22+h2*(aw-70)/2.0f,hy=ay+ah*0.50f;R(hx,hy+10,30,20,2,Col::hex(0xcfc6b4),Col::hex(0x9a927f));R(hx-4,hy,38,12,3,Col::hex(0x7a3e36),Col::hex(0x55292a));}
      for(int t2=0;t2<4;t2++){float tx=ax+12+t2*(aw-34)/3.0f;ui.circle(tx+6,ay+ah*0.87f+oy_,6,C(Col::hex(0x2f5a35)),C(Col::hex(0x244a2b)));}}
    else {R(ax,ay,aw,ah,4,Col::hex(0x343c4a),Col::hex(0x7d8aa0));R(ax,ay+ah*0.50f,aw*0.62f,ah*0.50f,24,Col::hex(0x4a6b3a),Col::hex(0x2f4a28));R(ax+aw*0.36f,ay+ah*0.42f,aw*0.64f,ah*0.58f,28,Col::hex(0x3e5d33),Col::hex(0x243b20));R(ax+aw*0.1f,ay+ah*0.76f,aw*0.8f,ah*0.24f,10,Col::hex(0x2a4325),Col::hex(0x182a16));}
    RING(ax,ay,aw,ah,4,1,LINE);
    T(TNAME[i],x+14,y+142,14,TXT,F_BOLD);T(TDESC[i],x+14,y+164,10.5f,TXT2);
    const char* tag[3]={"BLANK","SCENE","TERRAIN"};R(x+14,y+h-72,52,16,3,BG3,BG3);T(tag[i],x+40,y+h-70,8.5f,TXT2,F_BOLD,1);
    fbtn(20+i,x+14,y+h-44,w-28,30,"Create",0);}
}

void App::drawImport(){float W=W_/ui.s();header("Import project","Bring a project folder in from the Files app.");
  sect(1);panel(36,70,360,H_/ui.s()-70-26,"HOW IT WORKS");float lh=H_/ui.s()-70-26;(void)lh;
  const char* st[4]={"Open the Files app","Go to On My iPhone > Sabak Engine","Put your project folder in Import","Come back here and tap Refresh"};
  for(int i=0;i<4;i++){float y=70+44+i*46;R(54,y,22,22,4,BG3,BG3);RING(54,y,22,22,4,1,LINE2);T(std::to_string(i+1),65,y+4,11,TXT,F_BOLD,1);T(st[i],88,y+4,12,TXT);R(64,y+26,1,16,0,LINE,LINE);}
  sect(2);float rx=412,rw=W-412-36;panel(rx,70,rw,H_/ui.s()-70-26,"FOUND IN IMPORT");fbtn(4,rx+rw-86,70+38+0,72,24,"Refresh",1);
  if(found_.empty()){cubeIcon(rx+rw/2,70+110,46,A_);T("Nothing found",rx+rw/2,70+150,13,TXT,F_BOLD,1);T("Put a folder with project.json inside Import.",rx+rw/2,70+170,11,TXT3,F_REG,1);}
  else for(int i=0;i<(int)found_.size()&&i<3;i++){float x=rx+8,y=70+70+i*58,w=rw-16,h=52;R(x,y,w,h,4,BG2,BG2);RING(x,y,w,h,4,1,LINE);R(x+8,y+8,36,36,4,BG3,BG3);T(found_[i].substr(0,1),x+26,y+16,15,ACC,F_BOLD,1);
    T(found_[i].size()>18?found_[i].substr(0,17)+"..":found_[i],x+56,y+10,12.5f,TXT,F_BOLD);T("Ready to import",x+56,y+28,10.5f,TXT3);fbtn(30+i,x+w-72,y+13,62,26,"Import",0);}
}

void App::render(){
  btns_.clear();
  if(scr_==EDITOR){drawEditor();}
  else{drawBackground();if(scr_==HOME)drawHome();else if(scr_==TEMPLATES)drawTemplates();else drawImport();}
  A_=1;oy_=0;
  if(toastT_>0){float W=W_/ui.s();float a=std::min(1.0f,toastT_*3);float w=ui.measure(toast_,12,F_BOLD)+44;float x=(W-w)/2,y=H_/ui.s()-60;
    ui.shadow(x,y+2,w,32,4,12,Col::hex(0,0.5f*a));ui.rrect(x,y,w,32,4,BG2.with(a),BG2.with(a));ui.ring(x,y,w,32,4,1,LINE2.with(a));ui.rrect(x,y,3,32,0,OK.with(a),OK.with(a));ui.text(toast_,x+w/2+1,y+9,12,TXT.with(a),F_BOLD,1);}
  ui.end();prevBtns_=btns_;
}
}
