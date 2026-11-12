// Sabak Core - Sculpt Studio screen: clay brushes on a dense mesh, symmetry, undo, subdivision, GLB export.
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include "app.h"
#include "theme.h"
#include "icons.h"
namespace sk {
using namespace sk::th;
namespace fs=std::filesystem;
static const char* TOOL_N[8]={"Draw","Carve","Smooth","Inflate","Pinch","Grab","Flatten","Orbit"};
static const char* BASE_N[4]={"Sphere","Human","Horse","Car"};
static std::string sdir(const std::string& proj){return proj+"/sculpts";}

Entity* App::scEntity(){for(auto& e:scene_.ents)if(e.prim==P_MODEL&&e.asset.rfind("sculpt:",0)==0)return &e;return nullptr;}

void App::scBuildBase(int b){scBase_=b;
  if(b==0){sc_.fromSphere(40);return;}
  Model tmp;bool ok=false;
  if(b==1){CharDef d;d.outfit=2;d.hair=0;ok=buildHuman(*r_,d,tmp);if(ok)sc_.fromModel(tmp,true);}
  else if(b==2){ok=buildHorse(*r_,"",tmp);if(ok)sc_.fromModel(tmp,false);}
  else {ok=buildCar(*r_,"",tmp);if(ok)sc_.fromModel(tmp,false);}
  if(!ok){sc_.fromSphere(40);return;}
  int n=0;while(sc_.tris()<15000&&n<3){sc_.subdivide();n++;}}

bool App::scModel(Model& m){if(!sc_.valid()){std::string dir=cur_>=0&&cur_<(int)projects_.size()?projects_[cur_].dir:"";if(dir.empty()||!sc_.load(sdir(dir)+"/S1.sculpt"))sc_.fromSphere(40);}
  sc_.buildModel(r_,m);return true;}

void App::scRebuild(){Entity* e=scEntity();if(!e)return;models_.erase(e->asset);
  for(auto i2=minst_.begin();i2!=minst_.end();){if(i2->first.first==&scene_&&i2->first.second==e->id)i2=minst_.erase(i2);else ++i2;}modelFor(e->asset);}

void App::scLive(){Entity* e=scEntity();if(!e)return;Model* m=modelFor(e->asset);if(!m||m->prims.empty())return;r_->updateSkin(m->prims[0].mesh,sc_.v);}

void App::openSculpt(){if(cur_<0||cur_>=(int)projects_.size())return;std::string dir=projects_[cur_].dir;std::error_code ec;fs::create_directories(sdir(dir),ec);
  Entity* e=scEntity();if(!e){Entity& ne=scene_.add(P_MODEL,"Clay");ne.asset="sculpt:S1";ne.pos={0,0.08f,0};e=scene_.get(ne.id);rebuildRows();}
  sc_=Sculpt();if(!sc_.load(sdir(dir)+"/S1.sculpt"))sc_.fromSphere(40);scEd_=true;scDrag_=scStroke_=-1;animEd_=false;crEd_=false;select(e->id);scRebuild();
  Vec3 p=scene_.worldPos(e->id);ecTarget_={p.x,p.y+1.1f,p.z};ecDist_=3.6f;ecPitch_=0.15f;ecYaw_=0.5f;}

void App::closeSculpt(){if(cur_>=0&&cur_<(int)projects_.size())sc_.save(sdir(projects_[cur_].dir)+"/S1.sculpt");scEd_=false;scStroke_=-1;}

bool App::scPointer(int type,int id,float px,float py){if(!scEd_)return false;float s=ui.s(),x=px/s,y=py/s;float H=H_/s;
  auto slider=[&](int w){float t=std::max(0.f,std::min(1.f,(x-18)/176.f));if(w==0)sc_.radius=0.03f+t*0.40f;else sc_.strength=0.05f+t*0.95f;};
  Entity* e=scEntity();
  auto ray=[&](Vec3& o,Vec3& d){r_->ray(px,py,o,d);if(e){Vec3 p=scene_.worldPos(e->id);o=o-p;}};
  if(type==0){
    if(x>=10&&x<=204){for(int w=0;w<2;w++){float ry=192+w*30;if(y>=ry&&y<=ry+28){scDrag_=w;scDragId_=id;slider(w);return true;}}}
    if(x<212||y<42||y>H-24)return false;
    if(sc_.tool==7||!e)return false;
    if(scStroke_>=0){sc_.endStroke();scStroke_=-1;return false;}
    Vec3 o,d,h,n;ray(o,d);if(!sc_.raycast(o,d,h,n))return false;
    sc_.beginStroke(h,o,d);scStroke_=id;sc_.dab(h,o,d);scLive();return true;}
  if(type==1){
    if(id==scDragId_&&scDrag_>=0){slider(scDrag_);return true;}
    if(id==scStroke_){Vec3 o,d,h,n;ray(o,d);if(sc_.tool==5){sc_.dab(h,o,d);scLive();}else if(sc_.raycast(o,d,h,n)){sc_.dab(h,o,d);scLive();}return true;}
    return false;}
  if(type==2){
    if(id==scDragId_){scDrag_=-1;scDragId_=-1;return true;}
    if(id==scStroke_){sc_.endStroke();scStroke_=-1;return true;}}
  return false;}

void App::scTap(int id){Entity* e=scEntity();std::string dir=cur_>=0&&cur_<(int)projects_.size()?projects_[cur_].dir:"";
  if(id>=900&&id<908){sc_.tool=id-900;return;}
  if(id==910){sc_.sym=!sc_.sym;return;}
  if(id==911){if(sc_.undo())scLive();else toast("Nothing to undo");return;}
  if(id==912){if(sc_.subdivide()){scRebuild();toast("Subdivided: "+std::to_string(sc_.tris())+" triangles");}else toast("Max detail reached");return;}
  if(id==913){scBuildBase((scBase_+1)%4);scRebuild();scTap(918);toast(std::string("New base: ")+BASE_N[scBase_]);return;}
  if(id==914){Model m;sc_.buildModel(nullptr,m);std::string err,out=data_+"/Models/Exports/Sculpt.glb";if(exportGlb(m,out,err))toast("Exported: Files > SabakEngine > Models > Exports");else toast("Export failed: "+err);return;}
  if(id==915){if(!dir.empty()){sc_.save(sdir(dir)+"/S1.sculpt");saveScene(true);toast("Sculpt saved");}return;}
  if(id==916){ecDist_=std::max(0.6f,ecDist_*0.8f);return;}
  if(id==917){ecDist_=std::min(30.f,ecDist_*1.25f);return;}
  if(id==918&&e){Vec3 p=scene_.worldPos(e->id);ecTarget_={p.x,p.y+sc_.maxY()*0.55f,p.z};ecDist_=std::max(2.5f,sc_.maxY()*2.4f);return;}
  (void)e;}

void App::drawSculpt(float W,float H){
  ui.rrect(0,0,W,40,0,BG1.with(0.97f),BG1.with(0.97f));ui.rrect(0,39,W,1,0,LINE,LINE);
  fbtn(5,10,7,100,26,"",0);ui.text("Done",60,12,12,Col::hex(0xffffff),F_BOLD,1);ui.text("SCULPT STUDIO",122,8,12,TXT,F_BOLD);ui.text("Drag on the model to sculpt  |  drag empty space to orbit",122,24,10,TXT3);
  float x=6,y=46,w=200,h=H-46-28;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.96f),BG1.with(0.96f));ui.ring(x,y,w,h,5,1,LINE);ui.rrect(x+1,y+1,w-2,22,4,BG2,BG2);
  ui.text("CLAY TOOLS",x+12,y+7,10.5f,TXT2,F_BOLD);
  for(int t=0;t<8;t++){float bx=x+8+(t%2)*96,by=y+28+(t/2)*28;drawToolBtn(900+t,bx,by,92,25,TOOL_N[t],sc_.tool==t);}
  const char* nm[2]={"Brush size","Strength"};float val[2]={(sc_.radius-0.03f)/0.40f,(sc_.strength-0.05f)/0.95f};
  for(int i=0;i<2;i++){float ry=192+i*30;ui.text(nm[i],x+14,ry+1,10.5f,TXT2,F_BOLD);char b[16];std::snprintf(b,sizeof b,"%d%%",(int)(std::max(0.f,std::min(1.f,val[i]))*100));ui.text(b,x+186,ry+1,10.5f,TXT,F_BOLD,2);
    float t=std::max(0.f,std::min(1.f,val[i]));ui.rrect(18,ry+16,176,5,2,BG0,BG0);ui.rrect(18,ry+16,176*t,5,2,ACC,ACC);ui.circle(18+176*t,ry+18.5f,6,Col::hex(0xffffff),Col::hex(0xffffff));}
  drawToolBtn(910,x+8,254,92,24,sc_.sym?"Mirror: ON":"Mirror: OFF",sc_.sym);
  drawToolBtn(911,x+104,254,92,24,"Undo");
  float r2=282;
  drawToolBtn(912,x+8,r2,92,24,"Subdivide");
  { std::string b=std::string("Base: ")+BASE_N[scBase_%4];drawToolBtn(913,x+104,r2,92,24,b); }
  drawToolBtn(916,x+8,r2+28,44,24,"Zoom +");drawToolBtn(917,x+54,r2+28,44,24,"Zoom -");drawToolBtn(918,x+104,r2+28,92,24,"Frame");
  drawToolBtn(914,x+8,r2+56,92,24,"Export GLB");drawToolBtn(915,x+104,r2+56,92,24,"Save");
  char st[48];std::snprintf(st,sizeof st,"%dk triangles",(int)(sc_.tris()/1000));ui.text(st,x+188,y+8,9.5f,TXT3,F_BOLD,2);}
}
