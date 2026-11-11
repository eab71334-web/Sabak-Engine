// Sabak Core - Behavior blocks editor: build gameplay on the device with no compile step.
#include <algorithm>
#include <cstdio>
#include "app.h"
#include "theme.h"
namespace sk {
using namespace sk::th;
extern const char* kBlockNames[12]; extern const char* kBlockA[12]; extern const char* kBlockB[12]; int blockUsesName(int t);
static const float DEFA[12]={2.4f,2.4f,0,3,45,0.15f,0,0,4,3.5f,0,2.0f},DEFB[12]={6,1.5f,0,1.5f,0,1,0,0,1.5f,6,0,1.2f};
static const int MAXB=5;
static std::vector<std::string> entNames(const Scene& sc,const Entity* self){std::vector<std::string> v;for(auto& e:sc.ents)if(&e!=self&&e.prim!=P_TERRAIN)v.push_back(e.name);return v;}
static std::vector<std::string> animNames(App* a,Model* m){std::vector<std::string> v;if(m){for(auto& an:m->anims)v.push_back(an.name);if(!m->smStates.empty())v.push_back("StateMachine");}(void)a;return v;}

void App::drawBehavior(float W,float H){(void)H;Entity* e=scene_.get(sel_);if(!bhOpen_||!e||e->script!="Behavior")return;
  float x=198,y=44,w=W-198-246,h=286;ui.shadow(x,y+2,w,h,5,14,Col::hex(0,0.5f));ui.rrect(x,y,w,h,5,BG1.with(0.98f),BG1.with(0.98f));ui.ring(x,y,w,h,5,1,LINE2);ui.rrect(x+1,y+1,w-2,24,4,BG2,BG2);
  ui.text("BEHAVIOR BLOCKS",x+12,y+8,10.5f,TXT2,F_BOLD);ui.text(e->name,x+w/2,y+8,10.5f,ORG,F_BOLD,1);drawToolBtn(801,x+w-62,y+4,56,18,"Close");
  Model* m=e->prim==P_MODEL?modelFor(e->asset):nullptr;
  for(int r=0;r<MAXB;r++){float ry=y+32+r*44;ui.rrect(x+6,ry,w-12,40,4,BG0,BG0);ui.ring(x+6,ry,w-12,40,4,1,LINE);
    if(r>=(int)e->blocks.size()){ui.text("(empty slot)",x+16,ry+14,10.5f,TXT3);continue;}
    Block& b=e->blocks[r];int t=std::max(0,std::min(11,b.type));
    drawToolBtn(810+r*10,x+10,ry+4,18,20,"<");ui.rrect(x+30,ry+4,128,20,3,BG3,BG3);ui.text(kBlockNames[t],x+94,ry+9,10.5f,TXT,F_BOLD,1);drawToolBtn(811+r*10,x+160,ry+4,18,20,">");
    int nu=blockUsesName(t);if(nu){drawToolBtn(816+r*10,x+186,ry+4,18,20,"<");ui.rrect(x+206,ry+4,w-206-44,20,3,BG3,BG3);std::string s=b.s.empty()?(nu==1?"(pick object)":"(pick animation)"):b.s;if(s.size()>18)s=s.substr(0,17)+"..";ui.text(s,x+206+(w-206-44)/2,ry+9,10.5f,b.s.empty()?TXT3:ORG,F_BOLD,1);drawToolBtn(817+r*10,x+w-42,ry+4,18,20,">");}
    drawToolBtn(818+r*10,x+w-20-6,ry+16,20,18,"x");
    char buf[32];if(*kBlockA[t]){ui.text(kBlockA[t],x+14,ry+27,10,TXT2,F_BOLD);drawToolBtn(812+r*10,x+60,ry+25,18,15,"-");std::snprintf(buf,sizeof buf,"%.2f",b.a);ui.text(buf,x+98,ry+27,10.5f,TXT,F_REG,1);drawToolBtn(813+r*10,x+118,ry+25,18,15,"+");}
    if(*kBlockB[t]){ui.text(kBlockB[t],x+160,ry+27,10,TXT2,F_BOLD);drawToolBtn(814+r*10,x+214,ry+25,18,15,"-");std::snprintf(buf,sizeof buf,"%.2f",b.b);ui.text(buf,x+252,ry+27,10.5f,TXT,F_REG,1);drawToolBtn(815+r*10,x+272,ry+25,18,15,"+");}}
  float by=y+32+MAXB*44+2;drawToolBtn(800,x+8,by,70,22,"+ Block");drawToolBtn(802,x+82,by,74,22,"Player");drawToolBtn(803,x+160,by,74,22,"Follower");drawToolBtn(804,x+238,by,74,22,"Patrol");drawToolBtn(805,x+316,by,56,22,"Wander");
}

void App::bhTap(int id){Entity* e=scene_.get(sel_);if(!e)return;
  if(id==806){bhOpen_=!bhOpen_;return;}if(id==801){bhOpen_=false;return;}
  auto setDef=[&](Block& b){b.a=DEFA[b.type];b.b=DEFB[b.type];if(!blockUsesName(b.type))b.s.clear();};
  if(id==800){if((int)e->blocks.size()<MAXB){Block b;b.type=0;setDef(b);e->blocks.push_back(b);}else toast("Max 5 blocks");return;}
  if(id>=802&&id<=805){e->blocks.clear();auto add=[&](int t,float a,float b2,const std::string& s=""){Block b;b.type=t;b.a=a;b.b=b2;b.s=s;e->blocks.push_back(b);};
    if(id==802){add(0,2.4f,6);add(7,0,0);}else if(id==803){auto n=entNames(scene_,e);std::string tgt="Hero";for(auto& s:n)if(s=="Hero")tgt=s;add(1,2.4f,1.5f,tgt);add(7,0,0);}else if(id==804){add(3,3,1.5f);add(7,0,0);}else{add(8,4,1.5f);add(7,0,0);}
    toast("Blocks ready. Press Play");return;}
  if(id<810||id>=810+MAXB*10)return;int r=(id-810)/10,k=(id-810)%10;if(r>=(int)e->blocks.size())return;Block& b=e->blocks[r];
  if(k==0||k==1){b.type=(b.type+(k==1?1:11))%12;setDef(b);return;}
  if(k==8){e->blocks.erase(e->blocks.begin()+r);return;}
  float st=b.type==4?15.f:(b.type==5&&k<=3)?0.1f:0.5f;
  if(k==2){b.a=std::max(0.f,b.a-st);return;}if(k==3){b.a+=st;return;}
  float stb=b.type==5?0.25f:0.5f;if(k==4){b.b=std::max(0.f,b.b-stb);return;}if(k==5){b.b+=stb;return;}
  if(k==6||k==7){int nu=blockUsesName(b.type);std::vector<std::string> list=nu==1?entNames(scene_,e):animNames(this,e->prim==P_MODEL?modelFor(e->asset):nullptr);if(list.empty())return;int ci=-1;for(size_t i=0;i<list.size();i++)if(list[i]==b.s)ci=(int)i;ci=(ci+(k==7?1:(int)list.size()-1)+(ci<0?1:0))%(int)list.size();b.s=list[ci];return;}
}
}
