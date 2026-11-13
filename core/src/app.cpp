#include "app.h"
#include "theme.h"
#include "icons.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
namespace fs = std::filesystem;
namespace sk {
void registerBuiltinScripts();
static const Col WHITE=Col::hex(0xffffff);
using namespace sk::th;
static float hash1(float n){float s=std::sin(n*127.1f)*43758.5453f;return s-std::floor(s);}
static void squish(float p,float&x,float&y,float&w,float&h){float k=1-0.045f*p;x+=w*(1-k)/2;y+=h*(1-k)/2;w*=k;h*=k;}
static const char* TNAME[6]={"Empty World","Village Demo","Open Terrain","Animation Studio","Character Creator","Sculpt Studio"};
static const char* TDESC[3]={"Flat terrain with a hero. A clean start.","Village, forest and props. A ready scene.","Open hills and dense forest. No buildings."};

bool App::init(Renderer& r,const std::string& data,int w,int h){registerBuiltinScripts();
  r_=&r;data_=data;projDir_=data+"/SabakProjects";importDir_=data+"/Import";std::error_code ec;fs::create_directories(projDir_,ec);fs::create_directories(importDir_,ec);fs::create_directories(data+"/Models",ec);
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
void App::openProject(int i){if(i<0||i>=(int)projects_.size())return;cur_=i;play_=false;animEd_=false;crEd_=false;scEd_=false;models_.clear();minst_.clear();mthumbs_.clear();scripts_.clear();sel_=-1;addMenu_=false;expanded_.clear();hierScroll_=0;
  std::string sp=projects_[i].dir+"/scenes/main.scene";if(!scene_.load(sp)){scene_.buildTemplate(projects_[i].tmpl);std::error_code ec;fs::create_directories(projects_[i].dir+"/scenes",ec);scene_.save(sp);}
  ecYaw_=0.8f;ecPitch_=0.5f;ecDist_=26;ecTarget_={0,3,0};if(const char* e=getenv("SABAK_CAM")){float a[6]={0};sscanf(e,"%f,%f,%f,%f,%f,%f",&a[0],&a[1],&a[2],&a[3],&a[4],&a[5]);ecDist_=a[0];ecTarget_={a[1],a[2],a[3]};ecYaw_=a[4];ecPitch_=a[5];}rebuildRows();go(EDITOR);
  if(projects_[i].tmpl==3){for(auto& e:scene_.ents)if(e.prim==P_MODEL){select(e.id);openAnimator();break;}}
  if(projects_[i].tmpl==4)openCreator();if(projects_[i].tmpl==5)openSculpt();}
void App::importFound(int i){if(i<0||i>=(int)found_.size())return;std::error_code ec;fs::path src=fs::path(importDir_)/found_[i],dst=fs::path(projDir_)/found_[i];
  fs::copy(src,dst,fs::copy_options::recursive|fs::copy_options::overwrite_existing,ec);if(ec){toast("Import failed");return;}
  // refresh created time so it appears first
  rescan();rescanImports();toast("Imported "+found_[i]);for(size_t k=0;k<projects_.size();k++)if(projects_[k].name==found_[i]||fs::path(projects_[k].dir).filename()==found_[i])openProject((int)k);}
void App::go(Screen s){scr_=s;tr_=0;trT_=0;held_=-1;heldId_=-1;if(s==IMPORT)rescanImports();if(s==HOME)rescan();}
void App::onTap(int id){
  if(id==1)go(TEMPLATES);else if(id==2)go(IMPORT);else if(id==3)go(HOME);else if(id==4){rescanImports();toast("Refreshed");}
  else if(id==5){if(crEd_)closeCreator();if(scEd_)closeSculpt();if(play_)stopPlay();saveScene(true);go(HOME);}
  else if(id>=100&&id<940)editorTap(id);
  else if(id>=10&&id<20)openProject(id-10);else if(id>=20&&id<26)createProject(id-20);else if(id>=30&&id<40)importFound(id-30);}
void App::key(const std::string& k,bool d){int i=k=="W"?0:k=="A"?1:k=="S"?2:3;keyDown_[i]=d;}
bool App::btn(int id,float x,float y,float w,float h){btns_.push_back({id,x,y+0,w,h});return heldId_==id;}

void App::update(float dt){for(auto& kv:minst_){const Entity* e=nullptr;if(kv.first.first==&scene_)e=scene_.get(kv.first.second);if(e)kv.second.param=e->animParam;if(e&&!(animEd_&&e->id==sel_)){kv.second.lookOn=e->lookOn;kv.second.lookYaw=e->lookYaw;kv.second.lookPitch=e->lookPitch;}if(e&&e->anim=="StateMachine"){Entity* me=scene_.get(kv.first.second);if(me&&!me->trigger.empty()){kv.second.trigger=me->trigger;me->trigger.clear();}kv.second.smTick(e->animParam);}kv.second.update(dt,e?e->animSpeed:1.f);if(!kv.second.fired.empty()){if(e){Entity* me=scene_.get(kv.first.second);if(me)me->event=kv.second.fired.back();}kv.second.fired.clear();}}t_+=dt;trT_+=dt;A_=1;oy_=0;
  for(int i=0;i<1024;i++){float tg=(heldId_==i)?1.f:0.f;press_[i]+=(tg-press_[i])*std::min(1.0f,dt*20);}
  if(toastT_>0)toastT_-=dt;fpsAcc_+=dt;fpsN_++;if(fpsAcc_>0.5f){fps_=fpsN_/fpsAcc_;fpsAcc_=0;fpsN_=0;}
  if(scr_==EDITOR)editorUpdate(dt);}

void App::pointer(int type,int id,float px,float py){float s=ui.s();float x=px/s,y=py/s;
  if(type==0){
    for(int i=(int)prevBtns_.size()-1;i>=0;i--){auto&b=prevBtns_[i];if(x>=b.x&&x<=b.x+b.w&&y>=b.y&&y<=b.y+b.h){heldId_=b.id;held_=id;if(animEd_&&b.id==149)aeSeek(x);return;}}
    if(scr_==EDITOR){editorPointer(0,id,px,py);return;}
  }else if(type==1){
    if(animEd_&&held_==id&&heldId_==149)aeSeek(x);
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

void App::renderThumb(Target& t,const Scene& sc,Vec3 cam,Vec3 tg,float fov){r_->pushTarget(t);renderScene(sc,cam,tg,fov);r_->popTarget();}
void App::ensureThumbs(){
  if(scr_==EDITOR&&cbOpen_&&cbTab_==1){for(auto& n:mdlList_){if(n.rfind("ext:",0)==0)continue;Target& t=mthumbs_[n];if(t.fbo)continue;if(!r_->createTarget(t,128,128))return;Scene tmp;auto& en=tmp.add(P_MODEL,"m");en.asset=n;en.color=Col{1,1,1,1};
      bool horse=n=="horse"||n.rfind("gen:horse",0)==0;bool car=n.rfind("gen:car",0)==0;renderThumb(t,tmp,car?Vec3{5.2f,2.2f,5.6f}:horse?Vec3{3.6f,1.7f,3.8f}:Vec3{2.1f,1.3f,3.3f},{0,car?0.7f:horse?1.0f:0.85f,0},0.8f);return;}}
  Vec3 cams[3]={{9,4.5f,14},{15,6.5f,16},{-18,8,18}},tgs[3]={{0,1.5f,0},{-1,2.5f,-5},{0,3,-3}};
  for(int i=0;i<3;i++)if(!tmplOk_[i]){Scene tmp;tmp.buildTemplate(i);if(tmplT_[i].fbo==0&&!r_->createTarget(tmplT_[i],320,180))return;renderThumb(tmplT_[i],tmp,cams[i],tgs[i]);tmplOk_[i]=true;return;}
  if(!bannerOk_){Scene tmp;tmp.buildTemplate(1);if(banner_.fbo==0&&!r_->createTarget(banner_,640,252))return;renderThumb(banner_,tmp,{19,7.5f,23},{-2,2.5f,-6},0.8f);bannerOk_=true;return;}
  if(thumbDirty_&&cur_>=0&&cur_<(int)projects_.size()){Target& t=thumbs_[projects_[cur_].dir];if(t.fbo==0)r_->createTarget(t,192,108);Vec3 tg=ecTarget_,cam=tg+Vec3{std::sin(ecYaw_)*std::cos(ecPitch_)*ecDist_,std::sin(ecPitch_)*ecDist_,std::cos(ecYaw_)*std::cos(ecPitch_)*ecDist_};renderThumb(t,scene_,cam,tg);thumbDirty_=false;return;}
  if(scr_==HOME)for(auto& p:projects_){if(thumbs_.count(p.dir)&&thumbs_[p.dir].fbo)continue;Scene tmp;if(!tmp.load(p.dir+"/scenes/main.scene"))tmp.buildTemplate(p.tmpl);Target& t=thumbs_[p.dir];if(t.fbo==0)r_->createTarget(t,192,108);
    Vec3 tg{0,3,0},cam=tg+Vec3{std::sin(0.8f)*std::cos(0.5f)*26,std::sin(0.5f)*26,std::cos(0.8f)*std::cos(0.5f)*26};renderThumb(t,tmp,cam,tg);return;}
}

void App::drawHome(){float W=W_/ui.s();float H=H_/ui.s();
  sect(0);R(0,0,W,46,0,BG1.with(0.94f),BG1.with(0.94f));R(0,45,W,1,0,LINE,LINE);
  R(14,11,24,24,5,ACC,ACC2);ic::cube(ui,26,23+oy_,15,WHITE.with(A_));T("SABAK ENGINE",48,10,13,TXT,F_BOLD);T("Version 0.1",48,27,10,TXT3);
  { std::string st="C++ native   |   OpenGL ES 3   |   "+std::to_string((int)(fps_+0.5f))+" fps";T(st,W-16,17,10.5f,TXT3,F_REG,2);}
  // ---- hero banner (real render of the Village template)
  sect(1);{ float x=24,y=58,w=500,h=196;SH(x,y+3,w,h,8,14,Col::hex(0,0.5f));
    if(bannerOk_)ui.image(banner_.tex,x,y+oy_,w,h,8,Col{1,1,1,A_});else R(x,y,w,h,8,BG2,BG2);
    R(x,y,w,h,8,BG0.with(0.05f),BG0.with(0.80f));for(int i=0;i<25;i++){float aa=0.78f*std::pow(1-i/25.0f,1.6f);R(x+i*14,y,15,h,0,BG0.with(aa),BG0.with(aa));}RING(x,y,w,h,8,1,LINE2);
    R(x+24,y+26,3,50,0,ACC,ACC);T("SABAK ENGINE",x+36,y+24,10.5f,ACC,F_BOLD);T("Create your game",x+36,y+40,30,WHITE,F_BOLD);T("Design scenes, place objects and write C++ gameplay.",x+36,y+84,12,TXT2);
    { float bx=x+24,by=y+h-60,bw=170,bh=40;btn(1,bx,by,bw,bh);float k=pressOf(1);R(bx,by,bw,bh,5,ACC.mul(1+0.12f*k),ACC2.mul(1+0.12f*k));RING(bx,by,bw,bh,5,1,ACC.mul(1.3f));ic::plus(ui,bx+24,by+bh/2+oy_,15,WHITE.with(A_));T("New Project",bx+44,by+11,13.5f,WHITE,F_BOLD);}
    { float bx=x+204,by=y+h-60,bw=156,bh=40;btn(2,bx,by,bw,bh);float k=pressOf(2);R(bx,by,bw,bh,5,BG3.with(0.9f).mul(1+0.25f*k),BG2.with(0.9f).mul(1+0.25f*k));RING(bx,by,bw,bh,5,1,LINE2);ic::download(ui,bx+24,by+bh/2+oy_,15,TXT.with(A_));T("Import",bx+44,by+11,13.5f,TXT,F_BOLD);} }
  { float x=24,y=58,w=500,h=196;{float bx=x+370,by=y+h-108,bw=106,bh=40;btn(24,bx,by,bw,bh);float k=pressOf(24);R(bx,by,bw,bh,5,Col::hex(0x2a3a22).mul(1+0.2f*k),Col::hex(0x1f2b19).mul(1+0.2f*k));RING(bx,by,bw,bh,5,1,Col::hex(0x6fa85a));T("Character",bx+bw/2,by+7,11,Col::hex(0x9bd67f),F_BOLD,1);T("Creator",bx+bw/2,by+22,11,Col::hex(0x9bd67f),F_BOLD,1);}
  {float bx=x+256,by=y+h-108,bw=106,bh=40;btn(25,bx,by,bw,bh);float k=pressOf(25);R(bx,by,bw,bh,5,Col::hex(0x2a2b3d).mul(1+0.2f*k),Col::hex(0x1c1d2b).mul(1+0.2f*k));RING(bx,by,bw,bh,5,1,Col::hex(0x7a7ee0));T("Sculpt",bx+bw/2,by+7,11,Col::hex(0xa9acff),F_BOLD,1);T("Studio",bx+bw/2,by+22,11,Col::hex(0xa9acff),F_BOLD,1);}
  float bx=x+370,by=y+h-60,bw=106,bh=40;btn(23,bx,by,bw,bh);float k=pressOf(23);R(bx,by,bw,bh,5,Col::hex(0x3d2b1e).mul(1+0.2f*k),Col::hex(0x2a1f18).mul(1+0.2f*k));RING(bx,by,bw,bh,5,1,Col::hex(0xb87a45));T("Animation",bx+bw/2,by+7,11,Col::hex(0xe0aa70),F_BOLD,1);T("Studio",bx+bw/2,by+22,11,Col::hex(0xe0aa70),F_BOLD,1); }
  // ---- quick start templates
  sect(2);T("QUICK START",26,266,10,TXT3,F_BOLD);R(100,272,424,1,0,LINE,LINE);
  for(int i=0;i<3;i++){float x=24+i*(160+10),y=284,w=160,h=94;btn(20+i,x,y,w,h);float k=pressOf(20+i);SH(x,y+2,w,h,6,10,Col::hex(0,0.4f));
    if(tmplOk_[i])ui.image(tmplT_[i].tex,x,y+oy_,w,h,6,Col{1,1,1,A_});else R(x,y,w,h,6,BG2,BG2);
    R(x,y+h*0.45f,w,h*0.55f,6,BG0.with(0.0f),BG0.with(0.88f));RING(x,y,w,h,6,k>0.05f?1.5f:1,k>0.05f?ACC:LINE2);
    T(TNAME[i],x+10,y+h-34,12,WHITE,F_BOLD);const char* tg[3]={"BLANK","SCENE","TERRAIN"};T(tg[i],x+10,y+h-18,9,TXT2,F_BOLD);R(x+w-30,y+h-30,20,20,10,ACC.with(0.9f),ACC2.with(0.9f));ic::plus(ui,x+w-20,y+h-20+oy_,10,WHITE.with(A_));}
  // ---- recent projects
  sect(3);float px=W-24-288,py=58,pw=288,ph=H-58-14;panel(px,py,pw,ph,"RECENT PROJECTS");T(std::to_string(projects_.size()),px+pw-14,py+8,10.5f,TXT3,F_BOLD,2);
  if(projects_.empty()){ic::cube(ui,px+pw/2,py+ph/2-24+oy_,44,TXT3.with(A_*0.8f));T("No projects yet",px+pw/2,py+ph/2+10,13,TXT,F_BOLD,1);T("Create one or import an existing project.",px+pw/2,py+ph/2+30,10.5f,TXT3,F_REG,1);}
  else for(int i=0;i<(int)projects_.size()&&i<4;i++){auto&p=projects_[i];float x=px+8,y=py+34+i*70,w=pw-16,h=64;btn(10+i,x,y,w,h);float k=pressOf(10+i);
    R(x,y,w,h,5,BG3.with(0.0f+0.9f*k),BG3.with(0.0f+0.9f*k));R(x,y+h+2,w,1,0,LINE.with(0.5f),LINE.with(0.5f));
    auto it=thumbs_.find(p.dir);if(it!=thumbs_.end()&&it->second.fbo)ui.image(it->second.tex,x+4,y+5+oy_,96,54,4,Col{1,1,1,A_});else{R(x+4,y+5,96,54,4,BG2,BG2);ic::cube(ui,x+52,y+32+oy_,22,TXT3.with(A_));}
    RING(x+4,y+5,96,54,4,1,LINE2);
    T(p.name.size()>16?p.name.substr(0,15)+"..":p.name,x+110,y+12,12.5f,TXT,F_BOLD);
    long days=(long)((std::time(nullptr)-p.created)/86400);std::string when=days<=0?"Today":std::to_string(days)+"d ago";
    T(std::string(TNAME[std::min(5,std::max(0,p.tmpl))]),x+110,y+30,10.5f,TXT2);T(when,x+110,y+44,10,TXT3);ic::chevronR(ui,x+w-12,y+32+oy_,14,TXT3.with(A_));}
}

void App::drawTemplates(){float W=W_/ui.s();header("New project","Select a template to start from.");
  float cw=(W-36*2-16*2)/3,y0=66,ch=H_/ui.s()-y0-20;
  for(int i=0;i<3;i++){sect(1+i);float x=36+i*(cw+16),y=y0,w=cw,h=ch;btn(20+i,x,y,w,h);float k=pressOf(20+i);
    SH(x,y+3,w,h,6,12,Col::hex(0,0.4f));R(x,y,w,h,6,BG1.with(0.96f),BG1.with(0.96f));RING(x,y,w,h,6,1,k>0.05f?ACC:LINE2);
    float ax=x+8,ay=y+8,aw=w-16,ah=aw*0.5625f;
    if(tmplOk_[i])ui.image(tmplT_[i].tex,ax,ay+oy_,aw,ah,4,Col{1,1,1,A_});else R(ax,ay,aw,ah,4,BG2,BG2);
    RING(ax,ay,aw,ah,4,1,LINE);
    T(TNAME[i],x+14,ay+ah+12,14,TXT,F_BOLD);T(TDESC[i],x+14,ay+ah+34,10.5f,TXT2);
    const char* tag[3]={"BLANK","SCENE","TERRAIN"};R(x+14,y+h-76,52,16,3,BG3,BG3);T(tag[i],x+40,y+h-74,8.5f,TXT2,F_BOLD,1);
    fbtn(20+i,x+14,y+h-48,w-28,34,"Create project",0);}
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
  btns_.clear();if(scr_!=EDITOR||thumbDirty_||(cbOpen_&&cbTab_==1))ensureThumbs();
  if(scr_==EDITOR){drawEditor();}
  else{drawBackground();if(scr_==HOME)drawHome();else if(scr_==TEMPLATES)drawTemplates();else drawImport();}
  A_=1;oy_=0;
  if(toastT_>0){float W=W_/ui.s();float a=std::min(1.0f,toastT_*3);float w=ui.measure(toast_,12,F_BOLD)+44;float x=(W-w)/2,y=H_/ui.s()-60;
    ui.shadow(x,y+2,w,32,4,12,Col::hex(0,0.5f*a));ui.rrect(x,y,w,32,4,BG2.with(a),BG2.with(a));ui.ring(x,y,w,32,4,1,LINE2.with(a));ui.rrect(x,y,3,32,0,OK.with(a),OK.with(a));ui.text(toast_,x+w/2+1,y+9,12,TXT.with(a),F_BOLD,1);}
  ui.end();prevBtns_=btns_;
}
}
