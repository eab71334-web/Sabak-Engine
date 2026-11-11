// Sabak Core - Character Creator screen: sliders, hair/outfit, colors, then animate or export as GLB.
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <filesystem>
#include "app.h"
#include "theme.h"
#include "icons.h"
namespace sk {
using namespace sk::th;
namespace fs=std::filesystem;
static const Col WHITE=Col::hex(0xffffff);
static const unsigned SKIN_P[12]={0xf1c8a5,0xe0ac82,0xc68863,0xa8714c,0x8a5a3a,0x6b4128,0x4a2c1b,0xffe0cc,0xd9a07a,0xb07a52,0x7a4a2e,0x3b2417};
static const unsigned HAIR_P[12]={0x1c1410,0x3a2418,0x6b4423,0xa0682d,0xd2a05a,0xf0d9a0,0x8e8e93,0xffffff,0xc0392b,0x3498db,0x9b59b6,0xe84393};
static const unsigned GEN_P[12]={0xffffff,0x1c1c1c,0x8e8e93,0xc0392b,0xe67e22,0xf1c40f,0x2ecc71,0x1abc9c,0x3498db,0x3b6fb6,0x9b59b6,0xe84393};
static const char* HAIR_N[5]={"Bald","Short","Ponytail","Side locks","Bun"};
static const char* OUT_N[3]={"Shirt + pants","Tank + shorts","No clothes"};
static const char* PART_N[5]={"Skin","Hair","Shirt","Pants","Shoes"};
struct Slider{const char* name;float CharDef::*p;float lo,hi;};
static const Slider SL[5]={{"Height",&CharDef::height,1.5f,2.1f},{"Build",&CharDef::build,0,1},{"Muscle",&CharDef::muscle,0,1},{"Head size",&CharDef::head,0,1},{"Shoulders",&CharDef::shoulder,0,1}};
static const float SLY0=102,SLH=29;
static const char* EX_N[6]={"Smile","Mouth open","Blink","Brows up","Angry","Sad"};

static std::string charDir(const std::string& proj){return proj+"/characters";}
static std::vector<std::string> charNames(const std::string& proj){std::vector<std::string> v;std::error_code ec;if(!fs::is_directory(charDir(proj),ec))return v;for(auto& e:fs::directory_iterator(charDir(proj),ec))if(e.path().extension()==".char")v.push_back(e.path().stem().string());std::sort(v.begin(),v.end());return v;}

Entity* App::crEntity(){for(auto& e:scene_.ents)if(e.prim==P_MODEL&&e.asset.rfind("char:",0)==0)return &e;return nullptr;}

void App::openCreator(){if(cur_<0||cur_>=(int)projects_.size())return;Entity* e=crEntity();std::string dir=projects_[cur_].dir;
  if(!e){CharDef d;d.name="C1";d.save(charDir(dir)+"/C1.char");Entity& ne=scene_.add(P_MODEL,"Character");ne.asset="char:C1";ne.pos={0,0.08f,0};e=scene_.get(ne.id);rebuildRows();}
  select(e->id);cr_=CharDef();cr_.name=e->asset.substr(5);cr_.load(charDir(dir)+"/"+cr_.name+".char");crEd_=true;crSel_=0;crDrag_=-1;crDirty_=false;crT_=0;animEd_=false;
  Model* m=modelFor(e->asset);Vec3 p=scene_.worldPos(e->id);float h=m?m->height:1.8f;ecTarget_={p.x,p.y+h*0.5f,p.z};ecDist_=std::max(5.0f,h*2.8f);ecPitch_=0.1f;ecYaw_=0.4f;}

void App::closeCreator(){if(cur_>=0&&cur_<(int)projects_.size())cr_.save(charDir(projects_[cur_].dir)+"/"+cr_.name+".char");crEd_=false;crDrag_=-1;}

void App::crRebuild(){Entity* e=crEntity();if(!e||cur_<0||cur_>=(int)projects_.size())return;std::string dir=projects_[cur_].dir;cr_.save(charDir(dir)+"/"+cr_.name+".char");
  auto it=models_.find(e->asset);if(it!=models_.end()){if(it->second&&!it->second->clips.empty())it->second->saveClips(dir+"/anims",e->asset);models_.erase(it);}
  for(auto i2=minst_.begin();i2!=minst_.end();){if(i2->first.first==&scene_&&i2->first.second==e->id)i2=minst_.erase(i2);else ++i2;}
  Model* m=modelFor(e->asset);if(m){Vec3 p=scene_.worldPos(e->id);ecTarget_={p.x,p.y+m->height*0.5f,p.z};}crDirty_=false;}

void App::crSwitch(const std::string& name){Entity* e=crEntity();if(!e||cur_<0)return;std::string dir=projects_[cur_].dir;cr_.save(charDir(dir)+"/"+cr_.name+".char");
  auto it=models_.find(e->asset);if(it!=models_.end()){if(it->second&&!it->second->clips.empty())it->second->saveClips(dir+"/anims",e->asset);models_.erase(it);}
  for(auto i2=minst_.begin();i2!=minst_.end();){if(i2->first.first==&scene_&&i2->first.second==e->id)i2=minst_.erase(i2);else ++i2;}
  e->asset="char:"+name;cr_=CharDef();cr_.name=name;cr_.load(charDir(dir)+"/"+name+".char");crDirty_=false;modelFor(e->asset);}

void App::crUpdate(float dt){if(!crEd_)return;crT_+=dt;if(crDirty_&&crT_>0.12f){crT_=0;crRebuild();}}

bool App::crPointer(int type,int id,float px,float py){if(!crEd_)return false;float s=ui.s();float x=px/s,y=py/s;
  auto setFrom=[&](int i){const Slider& sl=SL[i];float t=std::max(0.f,std::min(1.f,(x-18)/208.f));cr_.*(sl.p)=sl.lo+(sl.hi-sl.lo)*t;crDirty_=true;};
  float ex0=W_-92.f,exw=74.f;
  auto setEx=[&](int i){float t=std::max(0.f,std::min(1.f,(x-ex0)/exw));cr_.ex[i]=t;if(Entity* e=crEntity()){if(Model* m=modelFor(e->asset)){m->morphW.assign(cr_.ex,cr_.ex+6);m->applyMorphs(*r_);}}};
  if(type==0){for(int i=0;i<6;i++){float ry=243+i*20;if(x>=ex0-72&&x<=ex0+exw+8&&y>=ry&&y<=ry+20){crDrag_=10+i;crDragId_=id;setEx(i);return true;}}}
  if(type==1&&crDrag_>=10&&id==crDragId_){setEx(crDrag_-10);return true;}
  if(type==2&&crDrag_>=10&&id==crDragId_){crDrag_=-1;crDragId_=-1;if(cur_>=0&&cur_<(int)projects_.size())cr_.save(charDir(projects_[cur_].dir)+"/"+cr_.name+".char");return true;}
  if(type==0){for(int i=0;i<5;i++){float ry=SLY0+i*SLH;if(x>=10&&x<=238&&y>=ry&&y<=ry+SLH){crDrag_=i;crDragId_=id;setFrom(i);return true;}}return false;}
  if(id!=crDragId_||crDrag_<0)return false;
  if(type==1){setFrom(crDrag_);return true;}
  if(type==2){crDrag_=-1;crDragId_=-1;crT_=1;return true;}return false;}

void App::crTap(int id){Entity* e=crEntity();if(!e)return;std::string dir=cur_>=0&&cur_<(int)projects_.size()?projects_[cur_].dir:"";
  auto names=charNames(dir);int ci=0;for(size_t i=0;i<names.size();i++)if(names[i]==cr_.name)ci=(int)i;
  if(id==702||id==703){if(names.size()>1){ci=(ci+(id==703?1:(int)names.size()-1))%(int)names.size();crSwitch(names[ci]);}return;}
  if(id==704){cr_.save(charDir(dir)+"/"+cr_.name+".char");int n=1;while(fs::exists(charDir(dir)+"/C"+std::to_string(n)+".char"))n++;CharDef d;d.name="C"+std::to_string(n);d.save(charDir(dir)+"/"+d.name+".char");crSwitch(d.name);toast("New character created");return;}
  if(id==705){if(names.size()>1){std::error_code ec;fs::remove(charDir(dir)+"/"+cr_.name+".char",ec);std::string old=cr_.name;names=charNames(dir);crSwitch(names[0]);toast("Deleted "+old);}else toast("Keep at least one character");return;}
  if(id==710||id==711){cr_.hair=(cr_.hair+(id==711?1:4))%5;crDirty_=true;crT_=1;return;}
  if(id==712||id==713){cr_.outfit=(cr_.outfit+(id==713?1:2))%3;crDirty_=true;crT_=1;return;}
  if(id>=720&&id<725){crSel_=id-720;return;}
  if(id>=730&&id<742){const unsigned* pal=crSel_==0?SKIN_P:crSel_==1?HAIR_P:GEN_P;cr_.col[crSel_]=pal[id-730];crDirty_=true;crT_=1;return;}
  if(id==754){Model* m=modelFor(e->asset);Vec3 p=scene_.worldPos(e->id);float h=m?m->height:1.8f;if(ecDist_<2.f){ecTarget_={p.x,p.y+h*0.5f,p.z};ecDist_=std::max(5.0f,h*2.8f);}else{ecTarget_={p.x,p.y+h*0.925f,p.z};ecDist_=0.75f;ecYaw_=0.f;ecPitch_=0.02f;}return;}
  if(id==750){closeCreator();select(e->id);openAnimator();return;}
  if(id==751){Model* m=modelFor(e->asset);if(!m){toast("No model");return;}std::string err,out=data_+"/Models/Exports/"+cr_.name+".glb";if(exportGlb(*m,out,err))toast("Exported: Files > SabakEngine > Models > Exports");else toast("Export failed: "+err);return;}
  if(id==752){cr_.save(charDir(dir)+"/"+cr_.name+".char");saveScene(true);toast("Character saved");return;}
  if(id==753){CharDef d=cr_;d.name=cr_.name;unsigned seed=(unsigned)(t_*1000)+d.hair*7;auto rnd=[&](){seed=seed*1664525u+1013904223u;return (seed>>8&0xffff)/65535.f;};
    d.height=1.55f+rnd()*0.5f;d.build=rnd();d.muscle=rnd();d.head=rnd();d.shoulder=rnd();d.hair=(int)(rnd()*4.99f);d.outfit=(int)(rnd()*1.99f);d.col[0]=SKIN_P[(int)(rnd()*11.99f)];d.col[1]=HAIR_P[(int)(rnd()*11.99f)];d.col[2]=GEN_P[(int)(rnd()*11.99f)];d.col[3]=GEN_P[(int)(rnd()*11.99f)];d.col[4]=GEN_P[(int)(rnd()*11.99f)];cr_=d;crDirty_=true;crT_=1;return;}
}

void App::drawCreator(float W,float H){
  ui.rrect(0,0,W,40,0,BG1.with(0.97f),BG1.with(0.97f));ui.rrect(0,39,W,1,0,LINE,LINE);
  fbtn(5,10,7,100,26,"",0);ui.text("Done",60,12,12,WHITE,F_BOLD,1);ui.text("CHARACTER CREATOR",122,8,12,TXT,F_BOLD);ui.text("Drag to orbit  |  pinch to zoom",122,24,10,TXT3);
  // ---- left: character + sliders
  { float x=6,y=46,w=238,h=H-46-28;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.96f),BG1.with(0.96f));ui.ring(x,y,w,h,5,1,LINE);ui.rrect(x+1,y+1,w-2,22,4,BG2,BG2);ui.text("BODY",x+12,y+7,10.5f,TXT2,F_BOLD);
    auto names=charNames(cur_>=0&&cur_<(int)projects_.size()?projects_[cur_].dir:"");
    drawToolBtn(702,x+8,y+28,20,22,"<");ui.rrect(x+30,y+28,98,22,3,BG3,BG3);ui.text(cr_.name,x+79,y+33,11,ORG,F_BOLD,1);drawToolBtn(703,x+130,y+28,20,22,">");drawToolBtn(704,x+154,y+28,40,22,"+ New");drawToolBtn(705,x+196,y+28,34,22,"Del");
    for(int i=0;i<5;i++){const Slider& sl=SL[i];float ry=SLY0+i*SLH;float v=cr_.*(sl.p);float t=(v-sl.lo)/(sl.hi-sl.lo);ui.text(sl.name,x+14,ry+1,10.5f,TXT2,F_BOLD);char b[24];if(i==0)std::snprintf(b,sizeof b,"%.2f m",v);else std::snprintf(b,sizeof b,"%d%%",(int)(t*100));ui.text(b,x+w-12,ry+1,10.5f,TXT,F_REG,2);
      ui.rrect(18,ry+16,208,6,3,BG0,BG0);ui.rrect(18,ry+16,208*t,6,3,ACC,ACC);ui.circle(18+208*t,ry+19,7,WHITE,WHITE);}
    float y2=SLY0+5*SLH+6;ui.text("Hair",x+14,y2+5,10.5f,TXT2,F_BOLD);drawToolBtn(710,x+58,y2,20,20,"<");ui.rrect(x+80,y2,100,20,3,BG3,BG3);ui.text(HAIR_N[cr_.hair%5],x+130,y2+4,10.5f,TXT,F_BOLD,1);drawToolBtn(711,x+182,y2,20,20,">");
    y2+=26;ui.text("Outfit",x+14,y2+5,10.5f,TXT2,F_BOLD);drawToolBtn(712,x+58,y2,20,20,"<");ui.rrect(x+80,y2,100,20,3,BG3,BG3);ui.text(OUT_N[cr_.outfit%3],x+130,y2+4,10.5f,TXT,F_BOLD,1);drawToolBtn(713,x+182,y2,20,20,">");
    y2+=28;for(int i=0;i<5;i++)drawToolBtn(720+i,x+8+i*45,y2,43,20,PART_N[i],crSel_==i);
    y2+=26;const unsigned* pal=crSel_==0?SKIN_P:crSel_==1?HAIR_P:GEN_P;for(int i=0;i<12;i++){float sx=x+8+i*18.5f,sy=y2;btn(730+i,sx,sy,17,30);bool on=cr_.col[crSel_]==pal[i];ui.rrect(sx,sy,17,30,3,Col::hex(pal[i]),Col::hex(pal[i]));ui.ring(sx,sy,17,30,3,on?2.f:1.f,on?WHITE:LINE);}}
  // ---- right: actions
  { float w=164,x=W-w-6,y=46,h=170;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.96f),BG1.with(0.96f));ui.ring(x,y,w,h,5,1,LINE);ui.rrect(x+1,y+1,w-2,22,4,BG2,BG2);ui.text("CHARACTER",x+12,y+7,10.5f,TXT2,F_BOLD);
    fbtn(750,x+10,y+30,w-20,30,"",0);ui.text("Animate this character",x+w/2,y+39,11.5f,WHITE,F_BOLD,1);
    drawToolBtn(751,x+10,y+68,w-20,26,"Export GLB");drawToolBtn(752,x+10,y+98,w-20,26,"Save");drawToolBtn(753,x+10,y+128,w-20,26,"Randomize");}
  { float w=164,x=W-w-6,y=222,h=H-222-28;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.96f),BG1.with(0.96f));ui.ring(x,y,w,h,5,1,LINE);ui.rrect(x+1,y+1,w-2,18,4,BG2,BG2);ui.text("EXPRESSION",x+12,y+5,10,TXT2,F_BOLD);drawToolBtn(754,x+w-58,y+2,54,16,ecDist_<2.f?"Body cam":"Face cam");
    for(int i=0;i<6;i++){float ry=243+i*20;ui.text(EX_N[i],x+10,ry+2,9.5f,TXT2,F_BOLD);float t=cr_.ex[i];ui.rrect(x+78,ry+7,w-90,5,2,BG0,BG0);ui.rrect(x+78,ry+7,(w-90)*t,5,2,ACC,ACC);ui.circle(x+78+(w-90)*t,ry+9.5f,6,WHITE,WHITE);}}
  ui.rrect(0,H-22,W,22,0,BG1.with(0.97f),BG1.with(0.97f));ui.rrect(0,H-22,W,1,0,LINE,LINE);ui.text("Auto-rigged on every change. Animate uses the hero library; export keeps all animations.",10,H-16,10,TXT3);
}
}
