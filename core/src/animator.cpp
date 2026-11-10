// Sabak Core - Animator: easy keyframe animation for skinned models (auto-key), clip library, import/retarget from FBX/GLB.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include "app.h"
#include "theme.h"
#include "icons.h"
namespace sk {
using namespace sk::th;
static const Col WHITE=Col::hex(0xffffff);
static const unsigned AXC[3]={0xe5534b,0x5fb35f,0x4b92e6};
static const float TL_X0=150,TL_PAD=18;

Model* App::aeModel(){const Entity* e=scene_.get(sel_);if(!e||e->prim!=P_MODEL)return nullptr;return modelFor(e->asset);}
std::string App::aeId(){const Entity* e=scene_.get(sel_);return e?e->asset:"";}

void App::openAnimator(){Entity* e=scene_.get(sel_);if(!e||e->prim!=P_MODEL){toast("Select a model first");return;}Model* m=modelFor(e->asset);if(!m){toast("Model not loaded");return;}
  animEd_=true;aeBone_=0;aeScroll_=0;aeT_=0;aePlay_=true;aeImp_=false;aePrevAnim_=e->anim;aeClip_=m->clips.empty()?-1:0;
  Vec3 p=scene_.worldPos(e->id);ecTarget_={p.x,p.y+m->height*0.30f,p.z};ecDist_=std::max(3.6f,m->height*2.5f);ecPitch_=0.25f;
  if(aeClip_>=0)e->anim=m->clips[aeClip_].name;}
void App::closeAnimator(){Entity* e=scene_.get(sel_);Model* m=aeModel();if(m&&cur_>=0&&cur_<(int)projects_.size())m->saveClips(projects_[cur_].dir+"/anims",aeId());if(e&&!aePrevAnim_.empty())e->anim=aePrevAnim_;animEd_=false;aeImp_=false;}

void App::aeSeek(float x){Model* m=aeModel();if(!m)return;ClipDef* c=aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;float W=W_/ui.s();float X0=6+TL_X0,X1=6+(W-248)-TL_PAD;float dur=c?c->dur:1.f;aeT_=clampf((x-X0)/(X1-X0),0,1)*dur;aePlay_=false;}
static int keyIndexNear(const std::vector<ClipKey>& ks,float t){for(size_t i=0;i<ks.size();i++)if(std::fabs(ks[i].t-t)<0.04f)return (int)i;return -1;}
static void valueAt(const std::vector<ClipKey>& ks,float t,Vec3& rot,Vec3& pos){rot={};pos={};if(ks.empty())return;
  if(t<=ks.front().t){rot=ks.front().rot;pos=ks.front().pos;return;}if(t>=ks.back().t){rot=ks.back().rot;pos=ks.back().pos;return;}
  for(size_t i=1;i<ks.size();i++)if(t<=ks[i].t){float f=(t-ks[i-1].t)/std::max(1e-5f,ks[i].t-ks[i-1].t);f=f*f*(3-2*f);rot=ks[i-1].rot+(ks[i].rot-ks[i-1].rot)*f;pos=ks[i-1].pos+(ks[i].pos-ks[i-1].pos)*f;return;}}

void App::animUpdate(float dt){(void)dt;if(!animEd_)return;Entity* e=scene_.get(sel_);Model* m=aeModel();if(!e||!m){animEd_=false;return;}
  auto it=minst_.find({&scene_,sel_});if(it==minst_.end())return;ModelInst& mi=it->second;
  if(aeClip_>=0&&aeClip_<(int)m->clips.size()){e->anim=m->clips[aeClip_].name;float d=m->clips[aeClip_].dur;
    if(aePlay_){aeT_=std::min(mi.t,d);e->animSpeed=1;}else{e->animSpeed=0;mi.t=aeT_;}}
  else{e->animSpeed=aePlay_?1.f:0.f;}}

void App::animTap(int id){Entity* e=scene_.get(sel_);Model* m=aeModel();if(!e||!m){animEd_=false;return;}
  auto curClip=[&]()->ClipDef*{return aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;};
  auto selClip=[&](int c){int n=(int)m->clips.size();if(n==0){aeClip_=-1;return;}aeClip_=(c%n+n)%n;aeT_=0;};
  auto boneNode=[&](){return m->joints.empty()?0:m->joints[std::min<int>(aeBone_,(int)m->joints.size()-1)];};
  if(id==120){closeAnimator();return;}
  else if(id==121)selClip(aeClip_-1);else if(id==122)selClip(aeClip_+1);
  else if(id==123){std::string base=m->animIndex("idle")>=0?"idle":"";int c=m->addClip("My Animation",base);selClip(c);aePlay_=true;toast("New animation created");}
  else if(id==124){if(curClip()){if(cur_>=0&&cur_<(int)projects_.size()){std::error_code ec;std::filesystem::remove(clipFilePath(projects_[cur_].dir+"/anims",aeId(),curClip()->name),ec);}m->removeClip(aeClip_);selClip(std::min(aeClip_,(int)m->clips.size()-1));toast("Animation deleted");}}
  else if(id==125||id==126){ClipDef* c=curClip();if(c){std::vector<std::string> names{""};for(int i=0;i<m->builtin;i++)names.push_back(m->anims[i].name);size_t i=0;for(;i<names.size();i++)if(names[i]==c->base)break;if(i==names.size())i=0;i=(i+(id==126?1:names.size()-1))%names.size();c->base=names[i];m->bakeClip(aeClip_);}}
  else if(id==127||id==128){if(ClipDef* c=curClip()){c->dur=clampf(c->dur+(id==128?0.25f:-0.25f),0.5f,10.0f);m->bakeClip(aeClip_);}}
  else if(id==129)aePlay_=!aePlay_;
  else if(id==130)aeScroll_=std::max(0.f,aeScroll_-3);else if(id==131)aeScroll_=std::min((float)std::max(0,(int)m->joints.size()-10),aeScroll_+3);
  else if(id==132){if(ClipDef* c=curClip()){auto& ks=c->keys[boneNode()];int k=keyIndexNear(ks,aeT_);if(k>=0){ks.erase(ks.begin()+k);if(ks.empty())c->keys.erase(boneNode());m->bakeClip(aeClip_);toast("Key deleted");}else toast("No key at this time");}}
  else if(id==133){if(ClipDef* c=curClip()){c->keys.erase(boneNode());m->bakeClip(aeClip_);toast("Bone reset");}}
  else if(id==134){aeImp_=!aeImp_;if(aeImp_){aeFiles_.clear();namespace fs=std::filesystem;std::error_code ec;std::string d=data_+"/Models";if(fs::is_directory(d,ec))for(auto& f:fs::directory_iterator(d,ec)){std::string x=f.path().extension().string();for(char& ch:x)ch=(char)tolower(ch);if(x==".glb"||x==".gltf"||x==".fbx")aeFiles_.push_back(f.path().string());}std::sort(aeFiles_.begin(),aeFiles_.end());}}
  else if(id==135){if(cur_>=0&&cur_<(int)projects_.size()){m->saveClips(projects_[cur_].dir+"/anims",aeId());toast("Animations saved");}}
  else if(id==149){}
  else if(id>=320&&id<326){ClipDef* c=curClip();if(!c){toast("Tap New to create an animation");return;}if(c->raw){toast("Imported clips cannot be edited. Duplicate by New");return;}
    int k=id-320,ax=k/2,sg=(k%2)?1:-1;auto& ks=c->keys[boneNode()];int ki=keyIndexNear(ks,aeT_);if(ki<0){ClipKey nk;nk.t=aeT_;valueAt(ks,aeT_,nk.rot,nk.pos);ks.push_back(nk);std::sort(ks.begin(),ks.end(),[](const ClipKey& a,const ClipKey& b){return a.t<b.t;});ki=keyIndexNear(ks,aeT_);}
    float* f=ax==0?&ks[ki].rot.x:ax==1?&ks[ki].rot.y:&ks[ki].rot.z;*f=clampf(*f+sg*5.0f,-180,180);m->bakeClip(aeClip_);aePlay_=false;}
  else if(id==326||id==327||id==328||id==329){ClipDef* c=curClip();if(!c||c->raw)return;auto& ks=c->keys[boneNode()];int ki=keyIndexNear(ks,aeT_);if(ki<0){ClipKey nk;nk.t=aeT_;valueAt(ks,aeT_,nk.rot,nk.pos);ks.push_back(nk);std::sort(ks.begin(),ks.end(),[](const ClipKey& a,const ClipKey& b){return a.t<b.t;});ki=keyIndexNear(ks,aeT_);}
    float sg=(id%2)?1.f:-1.f;float* f=id<328?&ks[ki].pos.y:&ks[ki].pos.z;*f=clampf(*f+sg*2.0f,-100,100);m->bakeClip(aeClip_);aePlay_=false;}
  else if(id>=600&&id<606){int i=id-600;if(i<(int)aeFiles_.size()){Model src;float h=m->height;if(loadModel(*r_,aeFiles_[i],h,src)){int mapped=0;int n=m->retargetFrom(src,mapped);
        if(n>0){selClip((int)m->clips.size()-n);toast("Imported "+std::to_string(n)+" animations ("+std::to_string(mapped)+" bones matched)");if(cur_>=0&&cur_<(int)projects_.size())m->saveClips(projects_[cur_].dir+"/anims",aeId());}
        else toast(src.anims.empty()?"That file has no animations":"Skeleton does not match this character");}else toast("Could not read that file");aeImp_=false;}}
  else if(id>=640&&id<660){aeBone_=(int)aeScroll_+(id-640);}
}

void App::drawAnimator(float W,float H){Model* m=aeModel();Entity* e=scene_.get(sel_);if(!m||!e)return;ClipDef* c=aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;
  int bone=m->joints.empty()?0:m->joints[std::min<int>(aeBone_,(int)m->joints.size()-1)];
  // top bar
  ui.rrect(0,0,W,40,0,BG1.with(0.96f),BG1.with(0.96f));ui.rrect(0,39,W,1,0,LINE,LINE);
  fbtn(120,8,6,86,28,"",0);ic::back(ui,22,20,16,WHITE);ui.text("Done",58,13,12,WHITE,F_BOLD,1);
  ui.text("ANIMATOR",108,6,12,TXT,F_BOLD);{std::string t=e->name+"   |   "+std::to_string(m->joints.size())+" bones   |   "+std::to_string(m->anims.size())+" clips";ui.text(t,108,23,10,TXT3);}
  ui.text("Pick a bone, tap + / - : keys are added automatically",W-14,14,10.5f,TXT3,F_REG,2);
  // ---- bones panel
  { float x=W-236,y=46,w=230,h=H-46-28;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.95f),BG1.with(0.95f));ui.ring(x,y,w,h,5,1,LINE);
    ui.rrect(x+1,y+1,w-2,25,4,BG2,BG2);ui.rrect(x+1,y+14,w-2,12,0,BG2,BG2);ui.text("BONES",x+12,y+8,10.5f,TXT2,F_BOLD);
    fbtn(130,x+w-62,y+3,26,20,"",3);ic::chevronU(ui,x+w-49,y+13,12,TXT2);fbtn(131,x+w-32,y+3,26,20,"",3);ic::chevronD(ui,x+w-19,y+13,12,TXT2);
    int vis=10;for(int i=0;i<vis;i++){int bi=(int)aeScroll_+i;if(bi>=(int)m->joints.size())break;float ry=y+30+i*19;bool on=bi==aeBone_;int node=m->joints[bi];bool hasKey=c&&c->keys.count(node);
      btn(640+i,x+4,ry,w-8,18);if(on){ui.rrect(x+4,ry,w-8,18,3,ACC.with(0.30f),ACC.with(0.24f));ui.rrect(x+4,ry,2,18,0,ACC,ACC);}
      std::string nm=m->nodeName[node];size_t cp=nm.find(':');if(cp!=std::string::npos)nm=nm.substr(cp+1);if(nm.size()>22)nm=nm.substr(0,21)+"..";ui.text(nm,x+14,ry+3,11,on?TXT:TXT2,on?F_BOLD:F_REG);
      if(hasKey){ui.circle(x+w-14,ry+9,3.5f,ORG,ORG);}}
    float sy=y+30+10*19+6;ui.rrect(x+1,sy,w-2,1,0,LINE,LINE);
    Vec3 rot,pos;if(c)valueAt(c->keys.count(bone)?c->keys[bone]:std::vector<ClipKey>(),aeT_,rot,pos);
    const char* an[3]={"Bend X","Turn Y","Roll Z"};for(int a=0;a<3;a++){float ry=sy+8+a*20;float v=a==0?rot.x:a==1?rot.y:rot.z;char b[24];std::snprintf(b,sizeof b,"%.0f",v);Col ac=Col::hex(AXC[a]);
      ui.text(an[a],x+10,ry+3,10.5f,TXT2,F_BOLD);float mx=x+74,mw=w-86;ui.rrect(mx,ry,mw,18,3,BG3,BG3);ui.ring(mx,ry,mw,18,3,1,LINE);ui.rrect(mx,ry,3,18,0,ac,ac);
      btn(320+a*2,mx+3,ry,30,18);btn(321+a*2,mx+mw-30,ry,30,18);ui.text("-",mx+17,ry,14,pressOf(320+a*2)>0.1f?ACC:TXT2,F_BOLD,1);ui.text("+",mx+mw-15,ry,14,pressOf(321+a*2)>0.1f?ACC:TXT2,F_BOLD,1);ui.text(b,mx+mw/2+1,ry+3,11,TXT,F_BOLD,1);}
    float py=sy+8+60+2;{float mx=x+74,mw=w-86;ui.text("Move Y",x+10,py+3,10.5f,TXT2,F_BOLD);ui.rrect(mx,py,mw,18,3,BG3,BG3);ui.ring(mx,py,mw,18,3,1,LINE);btn(326,mx+3,py,30,18);btn(327,mx+mw-30,py,30,18);
      ui.text("-",mx+17,py,14,TXT2,F_BOLD,1);ui.text("+",mx+mw-15,py,14,TXT2,F_BOLD,1);char b[24];std::snprintf(b,sizeof b,"%.0f cm",pos.y);ui.text(b,mx+mw/2+1,py+3,11,TXT,F_BOLD,1);}
    float by=y+h-26;fbtn(132,x+8,by,102,22,"",1);ui.text("Delete key",x+59,by+4,11,TXT,F_BOLD,1);fbtn(133,x+w-110,by,102,22,"",1);ui.text("Reset bone",x+w-59,by+4,11,TXT,F_BOLD,1);}
  // ---- timeline panel
  { float x=6,y=H-28-132,w=W-248,h=132;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.96f),BG1.with(0.96f));ui.ring(x,y,w,h,5,1,LINE);
    ui.rrect(x+1,y+1,w-2,24,4,BG2,BG2);ui.rrect(x+1,y+13,w-2,12,0,BG2,BG2);ui.text("TIMELINE",x+12,y+7,10.5f,TXT2,F_BOLD);
    // clip selector
    float cx=x+84;ui.rrect(cx,y+3,200,20,3,BG3,BG3);ui.ring(cx,y+3,200,20,3,1,LINE);btn(121,cx,y+3,24,20);btn(122,cx+176,y+3,24,20);ui.text("<",cx+12,y+5,12,TXT2,F_BOLD,1);ui.text(">",cx+188,y+5,12,TXT2,F_BOLD,1);
    { std::string nm=c?c->name:"(no animation yet)";if(nm.size()>22)nm=nm.substr(0,21)+"..";ui.text(nm,cx+100,y+7,11,c?TXT:TXT3,F_BOLD,1);}
    fbtn(123,cx+206,y+3,58,20,"",0);ui.text("+ New",cx+235,y+7,11,WHITE,F_BOLD,1);fbtn(124,cx+268,y+3,58,20,"",2);ui.text("Delete",cx+297,y+7,11,WHITE,F_BOLD,1);
    // row 2
    float ry=y+32;fbtn(129,x+10,ry,70,24,"",aePlay_?1:0);if(aePlay_){ui.rrect(x+27,ry+7,4,10,1,TXT,TXT);ui.rrect(x+35,ry+7,4,10,1,TXT,TXT);}else ic::play(ui,x+30,ry+12,12,WHITE);ui.text(aePlay_?"Pause":"Play",x+58,ry+5,11,aePlay_?TXT:WHITE,F_BOLD,1);
    float bx=x+90;ui.text("Base",bx,ry+6,10.5f,TXT2,F_BOLD);ui.rrect(bx+32,ry,130,24,3,BG3,BG3);ui.ring(bx+32,ry,130,24,3,1,LINE);btn(125,bx+32,ry,22,24);btn(126,bx+140,ry,22,24);ui.text("<",bx+43,ry+5,12,TXT2,F_BOLD,1);ui.text(">",bx+151,ry+5,12,TXT2,F_BOLD,1);
    ui.text(c?(c->base.empty()?"Rest pose":c->base):"-",bx+97,ry+6,10.5f,TXT,F_BOLD,1);
    float dx=bx+176;ui.text("Length",dx,ry+6,10.5f,TXT2,F_BOLD);ui.rrect(dx+44,ry,86,24,3,BG3,BG3);ui.ring(dx+44,ry,86,24,3,1,LINE);btn(127,dx+44,ry,26,24);btn(128,dx+104,ry,26,24);ui.text("-",dx+57,ry+3,14,TXT2,F_BOLD,1);ui.text("+",dx+117,ry+3,14,TXT2,F_BOLD,1);
    { char b[24];std::snprintf(b,sizeof b,"%.2fs",c?c->dur:0.f);ui.text(b,dx+87,ry+6,11,TXT,F_BOLD,1);}
    fbtn(134,dx+140,ry,86,24,"",aeImp_?0:1);ui.text("Import FBX",dx+183,ry+5,10.5f,aeImp_?WHITE:TXT,F_BOLD,1);
    fbtn(135,dx+232,ry,50,24,"",1);ui.text("Save",dx+257,ry+5,11,TXT,F_BOLD,1);
    // track
    float ty=y+68,tx0=TL_X0,tx1=w-TL_PAD;float dur=c?c->dur:(m->anims.empty()||e->anim.empty()?1.f:std::max(0.1f,m->anims[std::max(0,m->animIndex(e->anim))].dur));
    ui.rrect(x+10,ty,tx1+TL_PAD-4-x+x-10,56,4,BG0,BG0);ui.ring(x+10,ty,w-20,56,4,1,LINE);
    ui.text("Selected bone",x+18,ty+8,10,TXT3,F_BOLD);{std::string nm=m->nodeName[bone];size_t cp=nm.find(':');if(cp!=std::string::npos)nm=nm.substr(cp+1);if(nm.size()>16)nm=nm.substr(0,15)+"..";ui.text(nm,x+18,ty+22,11.5f,ORG,F_BOLD);}
    btn(149,x+tx0,ty,tx1-tx0,56);float X0=x+tx0,X1=x+tx1;
    int ticks=(int)std::ceil(dur/0.25f);for(int i=0;i<=ticks;i++){float t=std::min(dur,i*0.25f);float px=X0+(X1-X0)*t/dur;bool big=i%4==0;ui.rrect(px,ty+2,1,big?9:5,0,LINE2,LINE2);if(big){char b[12];std::snprintf(b,sizeof b,"%.0fs",t);ui.text(b,px+3,ty+1,9,TXT3);}}
    ui.rrect(X0,ty+30,X1-X0,1,0,LINE,LINE);
    if(c){for(auto& kv:c->keys){bool selb=kv.first==bone;for(auto& k:kv.second){float px=X0+(X1-X0)*k.t/dur,py=ty+(selb?31:46);float r=selb?6.f:3.f;Col kc=selb?ORG:TXT3;
          if(selb){ui.line(px-r,py,px,py-r,2,kc);ui.line(px,py-r,px+r,py,2,kc);ui.line(px+r,py,px,py+r,2,kc);ui.line(px,py+r,px-r,py,2,kc);ui.circle(px,py,3.2f,kc,kc);}else ui.circle(px,py,r,kc.with(0.7f),kc.with(0.7f));}}}
    float hx=X0+(X1-X0)*clampf(aeT_/dur,0,1);ui.rrect(hx-0.75f,ty+2,1.5f,52,0,ACC,ACC);ui.rrect(hx-5,ty+2,10,8,2,ACC,ACC);}
  // import popup
  if(aeImp_){float x=W-236-260,y=H-28-132-12-24-24*6;float w=250,h=24+24*6+8;ui.shadow(x,y+2,w,h,5,14,Col::hex(0,0.5f));ui.rrect(x,y,w,h,5,BG1.with(0.98f),BG1.with(0.98f));ui.ring(x,y,w,h,5,1,LINE2);
    ui.rrect(x+1,y+1,w-2,22,4,BG2,BG2);ui.text("IMPORT ANIMATIONS FROM FILE",x+10,y+6,10,TXT2,F_BOLD);
    if(aeFiles_.empty()){ui.text("Put .fbx / .glb files in",x+12,y+36,11,TXT3);ui.text("Files > SabakEngine > Models",x+12,y+54,11,TXT3);}
    for(int i=0;i<6&&i<(int)aeFiles_.size();i++){float ry=y+26+i*24;btn(600+i,x+4,ry,w-8,22);if(pressOf(600+i)>0.1f)ui.rrect(x+4,ry,w-8,22,3,ACC.with(0.35f),ACC.with(0.35f));std::string nm=aeFiles_[i];size_t sl=nm.find_last_of('/');if(sl!=std::string::npos)nm=nm.substr(sl+1);if(nm.size()>28)nm=nm.substr(0,27)+"..";ui.text(nm,x+12,ry+5,11.5f,TXT);}}
  // status bar
  ui.rrect(0,H-22,W,22,0,BG1.with(0.97f),BG1.with(0.97f));ui.rrect(0,H-22,W,1,0,LINE,LINE);
  { char b[96];std::snprintf(b,sizeof b,"Time %.2fs   |   Bone %s   |   %s",aeT_,m->nodeName[bone].c_str(),c?c->name.c_str():"no clip");ui.text(b,12,H-17,10,TXT3);ui.text("Drag the timeline to scrub   Drag the view to orbit",W-12,H-17,10,TXT3,F_REG,2);}
}
}
