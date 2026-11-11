// Sabak Core - Animator: touch-driven keyframe animation for skinned models.
// Direct bone manipulation (FK aim / 2-bone IK), pin (lock) limbs, auto-key, layers, events, mirror/copy/paste, undo/redo, FBX/GLB retarget.
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <array>
#include <functional>
#include <filesystem>
#include "app.h"
#include "theme.h"
#include "icons.h"
namespace sk {
using namespace sk::th;
static const Col WHITE=Col::hex(0xffffff);
static const unsigned AXC[3]={0xe5534b,0x5fb35f,0x4b92e6};
static const float TL_X0=150,TL_PAD=18,TOOLS_W=244,TL_H=148;
static const char* EASE_N[6]={"Linear","Smooth","Ease in","Ease out","Step","Bezier"};
static const char* EV_N[5]={"Footstep","Hit","Sound","Spawn","Custom"};
static const char* MASK_N[3]={"Upper body","Lower body","Full body"};

static Mat4 inv4(const Mat4& a){double m[4][8];for(int r=0;r<4;r++){for(int c=0;c<4;c++){m[r][c]=a.m[c*4+r];m[r][4+c]=r==c;}}
  for(int i=0;i<4;i++){int p=i;for(int r=i+1;r<4;r++)if(std::fabs(m[r][i])>std::fabs(m[p][i]))p=r;if(std::fabs(m[p][i])<1e-12)return Mat4::identity();if(p!=i)for(int c=0;c<8;c++)std::swap(m[i][c],m[p][c]);
    double d=m[i][i];for(int c=0;c<8;c++)m[i][c]/=d;for(int r=0;r<4;r++)if(r!=i){double f=m[r][i];if(f!=0)for(int c=0;c<8;c++)m[r][c]-=f*m[i][c];}}
  Mat4 o=Mat4::identity();for(int r=0;r<4;r++)for(int c=0;c<4;c++)o.m[c*4+r]=(float)m[r][4+c];return o;}
static Vec3 xf(const Mat4& m,Vec3 p){return {m.m[0]*p.x+m.m[4]*p.y+m.m[8]*p.z+m.m[12],m.m[1]*p.x+m.m[5]*p.y+m.m[9]*p.z+m.m[13],m.m[2]*p.x+m.m[6]*p.y+m.m[10]*p.z+m.m[14]};}
static Vec3 xd(const Mat4& m,Vec3 d){return {m.m[0]*d.x+m.m[4]*d.y+m.m[8]*d.z,m.m[1]*d.x+m.m[5]*d.y+m.m[9]*d.z,m.m[2]*d.x+m.m[6]*d.y+m.m[10]*d.z};}
static Vec3 nrm(Vec3 v){float l=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return l>1e-9f?v*(1/l):v;}
static float dot3(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static void qmulA(const float* a,const float* b,float* o){o[0]=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];o[1]=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];o[2]=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];o[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];}
static void qarcA(Vec3 u,Vec3 v,float* q){u=nrm(u);v=nrm(v);float d=dot3(u,v);if(d<-0.99999f){Vec3 ax=std::fabs(u.x)<0.9f?Vec3{1,0,0}:Vec3{0,1,0};Vec3 c{u.y*ax.z-u.z*ax.y,u.z*ax.x-u.x*ax.z,u.x*ax.y-u.y*ax.x};c=nrm(c);q[0]=c.x;q[1]=c.y;q[2]=c.z;q[3]=0;return;}
  q[0]=u.y*v.z-u.z*v.y;q[1]=u.z*v.x-u.x*v.z;q[2]=u.x*v.y-u.y*v.x;q[3]=1+d;float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);for(int i=0;i<4;i++)q[i]/=l;}

Model* App::aeModel(){const Entity* e=scene_.get(sel_);if(!e||e->prim!=P_MODEL)return nullptr;return modelFor(e->asset);}
std::string App::aeId(){const Entity* e=scene_.get(sel_);return e?e->asset:"";}
void App::aePush(){Model* m=aeModel();if(!m)return;aeUndo_.push_back(m->clips);if(aeUndo_.size()>40)aeUndo_.erase(aeUndo_.begin());aeRedo_.clear();}

void App::openAnimator(){Entity* e=scene_.get(sel_);if(!e||e->prim!=P_MODEL){toast("Select a model first");return;}Model* m=modelFor(e->asset);if(!m){toast("Model not loaded");return;}
  animEd_=true;aeBone_=0;aeScroll_=0;aeT_=0;aePlay_=true;aeImp_=false;aePrevAnim_=e->anim;aeClip_=m->clips.empty()?-1:0;aeUndo_.clear();aeRedo_.clear();aeDragId_=-1;aeTab_=0;aeCam_=false;aeLook_=false;aeFxPrev_=false;aeKeySel_=-1;aeGMode_=0;aePoses_.clear();
  Vec3 p=scene_.worldPos(e->id);ecTarget_={p.x,p.y-m->height*0.05f,p.z};ecDist_=std::max(5.2f,m->height*2.9f);ecPitch_=0.12f;
  if(aeClip_>=0)e->anim=m->clips[aeClip_].name;}
void App::closeAnimator(){Entity* e=scene_.get(sel_);Model* m=aeModel();if(m&&cur_>=0&&cur_<(int)projects_.size()){m->saveClips(projects_[cur_].dir+"/anims",aeId());m->saveSM(projects_[cur_].dir+"/anims",aeId());}if(e&&!aePrevAnim_.empty())e->anim=aePrevAnim_;if(e)e->animSpeed=1;{auto it=minst_.find({&scene_,sel_});if(it!=minst_.end()){it->second.fxOff=false;it->second.lookOn=false;}}animEd_=false;aeImp_=false;aeDragId_=-1;aeTab_=0;aeCam_=false;aeLook_=false;aeLookDrag_=false;}

static int keyIndexNear(const std::vector<ClipKey>& ks,float t){for(size_t i=0;i<ks.size();i++)if(std::fabs(ks[i].t-t)<0.04f)return (int)i;return -1;}
static ClipKey& keyAtTime(std::vector<ClipKey>& ks,float t){int ki=keyIndexNear(ks,t);if(ki<0){ClipKey nk;nk.t=t;clipEval(ks,t,nk.rot,nk.pos);ks.push_back(nk);std::sort(ks.begin(),ks.end(),[](const ClipKey& a,const ClipKey& b){return a.t<b.t;});ki=keyIndexNear(ks,t);}return ks[ki];}

void App::aeSeek(float x){Model* m=aeModel();if(!m)return;ClipDef* c=aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;float W=W_/ui.s();float X0=6+TL_X0,X1=6+(W-248)-TL_PAD;float dur=c?c->dur:1.f;aeT_=clampf((x-X0)/(X1-X0),0,1)*dur;aePlay_=false;}

void App::animUpdate(float dt){if(!animEd_)return;Entity* e=scene_.get(sel_);Model* m=aeModel();if(!e||!m){animEd_=false;return;}
  auto it=minst_.find({&scene_,sel_});if(it==minst_.end())return;ModelInst& mi=it->second;mi.fxOff=!(aeFxPrev_||aeLook_);if(!aeLook_)mi.lookOn=false;if(aeCam_){ecYaw_+=dt*0.35f;ecPitch_=0.14f+0.06f*std::sin(t_*0.5f);}
  if(aeClip_>=0&&aeClip_<(int)m->clips.size()){e->anim=m->clips[aeClip_].name;float d=m->clips[aeClip_].dur;
    if(aePlay_){aeT_=std::min(mi.t,d);e->animSpeed=1;}else{e->animSpeed=0;mi.t=aeT_;}}
  else{e->animSpeed=aePlay_?1.f:0.f;}}

// ---- screen positions of the bones (for drawing and touch hit-testing)
void App::aeProject(){aeJ_.clear();Entity* e=scene_.get(sel_);Model* m=aeModel();if(!e||!m)return;auto it=minst_.find({&scene_,sel_});if(it==minst_.end()||it->second.world.empty())return;
  Mat4 wm=scene_.world(e->id)*Mat4::scale({m->fit,m->fit,m->fit})*Mat4::translate({0,-m->foot,0});float s=ui.s();
  for(size_t j=0;j<m->joints.size();j++){const Mat4& w=it->second.world[m->joints[j]];Vec3 p=xf(wm,{w.m[12],w.m[13],w.m[14]});float sx,sy;if(!r_->project(p,sx,sy))continue;aeJ_.push_back({(int)j,sx/s,sy/s});}}

// ---- direct manipulation: drag a joint. Hips translate, FK aims the parent bone at the finger, IK solves the 2-bone chain, pinned joints move their pin.
bool App::aePointer(int type,int id,float px,float py){if(!animEd_)return false;float s=ui.s();float x=px/s,y=py/s;Model* m=aeModel();Entity* e=scene_.get(sel_);if(!m||!e)return false;
  if(aeCam_)return false;if(aeStudioPointer(type,id,x,y))return true;
  auto lookSet=[&](float lx,float ly){auto it2=minst_.find({&scene_,sel_});if(it2==minst_.end())return;float hx=lx,hy=ly;for(auto& q:aeJ_)if(m->boneRole(m->joints[q.joint])=="head"){hx=q.x;hy=q.y;}float W=W_/s,H=H_/s;it2->second.lookOn=true;it2->second.lookYaw=(lx-hx)/(W*0.2f)*70.f;it2->second.lookPitch=-(ly-hy)/(H*0.2f)*35.f;};
  ClipDef* c=aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;
  if(type==0){if(nearPanel(x,y))return false;int best=-1;float bd=26*26;for(size_t i=0;i<aeJ_.size();i++){float dx=x-aeJ_[i].x,dy=y-aeJ_[i].y;if(dx*dx+dy*dy<bd){bd=dx*dx+dy*dy;best=(int)i;}}
    if(best<0){if(aeLook_){aeDragId_=id;aeLookDrag_=true;lookSet(x,y);return true;}return false;}aeBone_=aeJ_[best].joint;aeDragId_=id;aeLx_=px;aeLy_=py;aeDragChanged_=false;aePlay_=false;if(!c)toast("Tap + New first, then drag the bones");return true;}
  if(id!=aeDragId_)return false;
  if(type==2){aeDragId_=-1;aeLookDrag_=false;return true;}
  if(aeLookDrag_){lookSet(x,y);return true;}
  if(!c||c->raw){return true;}
  auto it=minst_.find({&scene_,sel_});if(it==minst_.end())return true;
  int node=m->joints[std::min<int>(aeBone_,(int)m->joints.size()-1)];
  // finger -> point on a camera-facing plane through the joint, in raw model space
  Mat4 wm=scene_.world(e->id)*Mat4::scale({m->fit,m->fit,m->fit})*Mat4::translate({0,-m->foot,0});Mat4 iw=inv4(wm);
  Vec3 ro,rd;r_->ray(px,py,ro,rd);Vec3 cam=r_->cameraPos();Vec3 fwd=nrm(ecTarget_-cam);
  Vec3 jr{it->second.world[node].m[12],it->second.world[node].m[13],it->second.world[node].m[14]};Vec3 jw=xf(wm,jr);
  float den=dot3(rd,fwd);if(std::fabs(den)<1e-5f)return true;float tt=dot3(jw-ro,fwd)/den;Vec3 hitW=ro+rd*tt;Vec3 T=xf(iw,hitW);
  // current pose (with keys) and base pose (without keys)
  std::vector<TRS> cur,base;float t=aeT_;int ai=m->builtin+aeClip_;if(ai>=(int)m->anims.size())return true;m->sample(ai,t,cur);m->basePose(aeClip_,t,base);
  std::vector<TRS> np=cur;int pn=m->parent[node];int gp=pn>=0?m->parent[pn]:-1;bool isJointParent=pn>=0&&std::find(m->joints.begin(),m->joints.end(),pn)!=m->joints.end();
  // pinned? move the pin target
  for(auto& pin:c->pins)if(pin.first==node){if(!aeDragChanged_){aePush();aeDragChanged_=true;}pin.second=T;m->bakeClip(aeClip_);return true;}
  if(!aeDragChanged_){aePush();aeDragChanged_=true;}
  std::vector<int> changed;
  if(!isJointParent){ // root: translate in raw model space
    Vec3 cp=jr;Vec3 d=T-cp;auto& ks=c->keys[node];ClipKey& k=keyAtTime(ks,t);k.pos=k.pos+d*100.f;k.pos.x=clampf(k.pos.x,-300,300);k.pos.y=clampf(k.pos.y,-300,300);k.pos.z=clampf(k.pos.z,-300,300);m->bakeClip(aeClip_);return true;}
  if(aeIK_&&gp>=0){ik2(*m,np,node,T);changed={pn,gp};}
  else{ // FK: swing the parent bone so that the joint follows the finger
    std::vector<Mat4> w;w.assign(np.size(),Mat4::identity());std::vector<std::array<float,4>> wq(np.size(),{0,0,0,1});std::vector<char> dn(np.size(),0);
    std::function<void(int)> go=[&](int i){if(dn[i])return;int pa=m->parent[i];Mat4 l=Mat4::identity();{float* r=np[i].r;float xx=r[0],yy=r[1],zz=r[2],ww=r[3];l.m[0]=1-2*(yy*yy+zz*zz);l.m[1]=2*(xx*yy+zz*ww);l.m[2]=2*(xx*zz-yy*ww);l.m[4]=2*(xx*yy-zz*ww);l.m[5]=1-2*(xx*xx+zz*zz);l.m[6]=2*(yy*zz+xx*ww);l.m[8]=2*(xx*zz+yy*ww);l.m[9]=2*(yy*zz-xx*ww);l.m[10]=1-2*(xx*xx+yy*yy);
        for(int q=0;q<3;q++){l.m[q]*=np[i].s.x;l.m[4+q]*=np[i].s.y;l.m[8+q]*=np[i].s.z;}l.m[12]=np[i].t.x;l.m[13]=np[i].t.y;l.m[14]=np[i].t.z;}
      float lq[4]={np[i].r[0],np[i].r[1],np[i].r[2],np[i].r[3]};if(pa>=0){go(pa);w[i]=w[pa]*l;qmulA(wq[pa].data(),lq,wq[i].data());}else{w[i]=l;for(int q=0;q<4;q++)wq[i][q]=lq[q];}dn[i]=1;};
    for(size_t i=0;i<np.size();i++)go((int)i);
    Vec3 B=xf(Mat4::identity(),{w[pn].m[12],w[pn].m[13],w[pn].m[14]}),C={w[node].m[12],w[node].m[13],w[node].m[14]};float q[4];qarcA(C-B,T-B,q);
    int pp=m->parent[pn];float pq[4]={0,0,0,1};if(pp>=0)for(int i=0;i<4;i++)pq[i]=wq[pp][i];float nw[4],ip[4]={-pq[0],-pq[1],-pq[2],pq[3]},loc[4];qmulA(q,wq[pn].data(),nw);qmulA(ip,nw,loc);for(int i=0;i<4;i++)np[pn].r[i]=loc[i];changed={pn};}
  for(int b:changed){float ib[4]={-base[b].r[0],-base[b].r[1],-base[b].r[2],base[b].r[3]},dq[4];qmulA(ib,np[b].r,dq);Vec3 eu;quatToEuler(dq,eu);auto& ks=c->keys[b];ClipKey& k=keyAtTime(ks,t);k.rot={clampf(eu.x,-180,180),clampf(eu.y,-180,180),clampf(eu.z,-180,180)};}
  m->bakeClip(aeClip_);return true;}

void App::animTap(int id){Entity* e=scene_.get(sel_);Model* m=aeModel();if(!e||!m){animEd_=false;return;}
  if(id>=200&&id<300){aeStudioTap(id);return;}
  auto curClip=[&]()->ClipDef*{return aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;};
  auto selClip=[&](int c){int n=(int)m->clips.size();if(n==0){aeClip_=-1;return;}aeClip_=(c%n+n)%n;aeT_=0;aeUndo_.clear();aeRedo_.clear();};
  auto boneNode=[&](){return m->joints.empty()?0:m->joints[std::min<int>(aeBone_,(int)m->joints.size()-1)];};
  auto editable=[&]()->ClipDef*{ClipDef* c=curClip();if(!c){toast("Tap + New to create an animation");return nullptr;}if(c->raw){toast("Imported clips are locked. Tap + New and use it as Base");return nullptr;}return c;};
  if(id==120){closeAnimator();return;}
  else if(id==121)selClip(aeClip_-1);else if(id==122)selClip(aeClip_+1);
  else if(id==123){std::string base=m->animIndex("idle")>=0?"idle":"";int c=m->addClip("My Animation",base);selClip(c);aePlay_=true;toast("New animation created");}
  else if(id==124){if(curClip()){if(cur_>=0&&cur_<(int)projects_.size()){std::error_code ec;std::filesystem::remove(clipFilePath(projects_[cur_].dir+"/anims",aeId(),curClip()->name),ec);}m->removeClip(aeClip_);selClip(std::min(aeClip_,(int)m->clips.size()-1));toast("Animation deleted");}}
  else if(id==125||id==126){ClipDef* c=editable();if(c){aePush();std::vector<std::string> names{""};for(int i=0;i<m->builtin;i++)names.push_back(m->anims[i].name);size_t i=0;for(;i<names.size();i++)if(names[i]==c->base)break;if(i==names.size())i=0;i=(i+(id==126?1:names.size()-1))%names.size();c->base=names[i];m->bakeClip(aeClip_);}}
  else if(id==127||id==128){if(ClipDef* c=editable()){aePush();c->dur=clampf(c->dur+(id==128?0.25f:-0.25f),0.5f,10.0f);m->bakeClip(aeClip_);}}
  else if(id==129)aePlay_=!aePlay_;
  else if(id==130)aeScroll_=std::max(0.f,aeScroll_-3);else if(id==131)aeScroll_=std::min((float)std::max(0,(int)m->joints.size()-10),aeScroll_+3);
  else if(id==132){if(ClipDef* c=editable()){auto& ks=c->keys[boneNode()];int k=keyIndexNear(ks,aeT_);if(k>=0){aePush();ks.erase(ks.begin()+k);if(ks.empty())c->keys.erase(boneNode());m->bakeClip(aeClip_);toast("Key deleted");}else toast("No key at this time");}}
  else if(id==133){if(ClipDef* c=editable()){aePush();c->keys.erase(boneNode());m->bakeClip(aeClip_);toast("Bone reset");}}
  else if(id==134){aeImp_=!aeImp_;if(aeImp_){aeFiles_.clear();namespace fs=std::filesystem;std::error_code ec;std::string d=data_+"/Models";if(fs::is_directory(d,ec))for(auto& f:fs::directory_iterator(d,ec)){std::string x=f.path().extension().string();for(char& ch:x)ch=(char)tolower(ch);if(x==".glb"||x==".gltf"||x==".fbx")aeFiles_.push_back(f.path().string());}std::sort(aeFiles_.begin(),aeFiles_.end());}}
  else if(id==135){if(cur_>=0&&cur_<(int)projects_.size()){m->saveClips(projects_[cur_].dir+"/anims",aeId());m->saveSM(projects_[cur_].dir+"/anims",aeId());toast("Animations saved");}}
  else if(id==149){}
  // ---- tools
  else if(id==170||id==171){auto& from=id==170?aeUndo_:aeRedo_;auto& to=id==170?aeRedo_:aeUndo_;if(from.empty()){toast(id==170?"Nothing to undo":"Nothing to redo");return;}to.push_back(m->clips);m->clips=from.back();from.pop_back();m->rawData.resize(m->clips.size());for(size_t i=0;i<m->clips.size();i++)m->bakeClip((int)i);if(aeClip_>=(int)m->clips.size())aeClip_=(int)m->clips.size()-1;}
  else if(id==172){if(ClipDef* c=curClip()){aeCopy_.clear();for(auto& kv:c->keys){Vec3 r,p;clipEval(kv.second,aeT_,r,p);aeCopy_[kv.first]={r,p};}toast(aeCopy_.empty()?"No keys to copy":"Pose copied");}}
  else if(id==173){if(ClipDef* c=editable()){if(aeCopy_.empty()){toast("Copy a pose first");return;}aePush();for(auto& kv:aeCopy_){ClipKey& k=keyAtTime(c->keys[kv.first],aeT_);k.rot=kv.second.first;k.pos=kv.second.second;}m->bakeClip(aeClip_);toast("Pose pasted");}}
  else if(id==174){if(ClipDef* c=editable()){aePush();std::map<int,std::pair<Vec3,Vec3>> src;for(int node:m->joints){Vec3 r,p;auto f=c->keys.find(node);if(f!=c->keys.end())clipEval(f->second,aeT_,r,p);src[node]={r,p};}
      for(int node:m->joints){if(m->boneRole(node).empty())continue;int mn=m->mirrorNode(node);Vec3 r=src[mn].first,p=src[mn].second;ClipKey& k=keyAtTime(c->keys[node],aeT_);k.rot={r.x,-r.y,-r.z};k.pos={-p.x,p.y,p.z};}
      m->bakeClip(aeClip_);toast("Pose mirrored");}}
  else if(id==175){if(ClipDef* c=editable()){auto& ks=c->keys[boneNode()];int k=keyIndexNear(ks,aeT_);if(k<0){toast("Select a bone with a key at the playhead");return;}aePush();ks[k].ease=(ks[k].ease+1)%5;m->bakeClip(aeClip_);toast(std::string("Ease: ")+EASE_N[ks[k].ease]);}}
  else if(id==176){aeIK_=!aeIK_;toast(aeIK_?"IK: drag a hand or foot, the arm or leg follows":"FK: drag a joint to swing its parent bone");}
  else if(id==188){if(ClipDef* c=editable()){int node=boneNode();auto it=minst_.find({&scene_,sel_});if(it==minst_.end())return;aePush();bool had=false;for(size_t i=0;i<c->pins.size();i++)if(c->pins[i].first==node){c->pins.erase(c->pins.begin()+i);had=true;break;}
      if(!had){const Mat4& w=it->second.world[node];c->pins.push_back({node,Vec3{w.m[12],w.m[13],w.m[14]}});}m->bakeClip(aeClip_);toast(had?"Pin removed":"Pinned: this joint stays in place while the rest moves");}}
  else if(id==189){aeEvName_=(aeEvName_+1)%5;toast(std::string("Event type: ")+EV_N[aeEvName_]);}
  else if(id==177){if(ClipDef* c=editable()){aePush();c->events.push_back({aeT_,EV_N[aeEvName_]});std::sort(c->events.begin(),c->events.end(),[](auto& a,auto& b){return a.first<b.first;});toast(std::string("Event added: ")+EV_N[aeEvName_]);}}
  else if(id==190){if(ClipDef* c=editable()){size_t bi=c->events.size();float bd=0.15f;for(size_t i=0;i<c->events.size();i++)if(std::fabs(c->events[i].first-aeT_)<bd){bd=std::fabs(c->events[i].first-aeT_);bi=i;}if(bi<c->events.size()){aePush();c->events.erase(c->events.begin()+bi);toast("Event removed");}else toast("No event near the playhead");}}
  else if(id==181){if(ClipDef* c=editable()){aePush();for(int node:m->joints){Vec3 r,p;auto f=c->keys.find(node);if(f!=c->keys.end())clipEval(f->second,aeT_,r,p);ClipKey& k=keyAtTime(c->keys[node],aeT_);k.rot=r;k.pos=p;}m->bakeClip(aeClip_);toast("Pose keyed on all bones");}}
  else if(id==179){if(ClipDef* c=editable()){aePush();c->loop=!c->loop;m->bakeClip(aeClip_);}}
  else if(id==180){if(ClipDef* c=editable()){aePush();c->inPlace=!c->inPlace;m->bakeClip(aeClip_);}}
  else if(id==182||id==183){if(ClipDef* c=editable()){aePush();std::vector<std::string> names{""};for(auto& a:m->anims)if(a.name!=c->name&&a.name!="Locomotion")names.push_back(a.name);size_t i=0;for(;i<names.size();i++)if(names[i]==c->overlay)break;if(i==names.size())i=0;i=(i+(id==183?1:names.size()-1))%names.size();c->overlay=names[i];m->bakeClip(aeClip_);}}
  else if(id==184||id==185){if(ClipDef* c=editable()){aePush();c->mask=(c->mask+(id==185?1:2))%3;m->bakeClip(aeClip_);}}
  else if(id==186||id==187){if(ClipDef* c=editable()){aePush();c->weight=clampf(c->weight+(id==187?0.1f:-0.1f),0,1);m->bakeClip(aeClip_);}}
  else if(id>=600&&id<606){int i=id-600;if(i<(int)aeFiles_.size()){Model src;float h=m->height;if(loadModel(*r_,aeFiles_[i],h,src)){int mapped=0;int n=m->retargetFrom(src,mapped);
        if(n>0){selClip((int)m->clips.size()-n);toast("Imported "+std::to_string(n)+" animations ("+std::to_string(mapped)+" bones matched)");if(cur_>=0&&cur_<(int)projects_.size())m->saveClips(projects_[cur_].dir+"/anims",aeId());}
        else toast(src.anims.empty()?"That file has no animations":"Skeleton does not match this character");}else toast("Could not read that file");aeImp_=false;}}
  else if(id>=640&&id<660){aeBone_=(int)aeScroll_+(id-640);}
  else if(id>=320&&id<326){ClipDef* c=editable();if(!c)return;aePush();
    int k=id-320,ax=k/2,sg=(k%2)?1:-1;ClipKey& key=keyAtTime(c->keys[boneNode()],aeT_);float* f=ax==0?&key.rot.x:ax==1?&key.rot.y:&key.rot.z;*f=clampf(*f+sg*5.0f,-180,180);m->bakeClip(aeClip_);aePlay_=false;}
  else if(id>=326&&id<330){ClipDef* c=editable();if(!c)return;aePush();ClipKey& key=keyAtTime(c->keys[boneNode()],aeT_);float sg=(id%2)?1.f:-1.f;float* f=id<328?&key.pos.y:&key.pos.z;*f=clampf(*f+sg*2.0f,-100,100);m->bakeClip(aeClip_);aePlay_=false;}
}


void App::drawAnimator(float W,float H){Model* m=aeModel();Entity* e=scene_.get(sel_);if(!m||!e)return;ClipDef* c=aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;
  int bone=m->joints.empty()?0:m->joints[std::min<int>(aeBone_,(int)m->joints.size()-1)];
  if(aeCam_){aeCamDraw(W,H);return;}
  aeProject();aeOnionDraw();
  // ---- bone overlay on the character (touch targets)
  { std::vector<int> jidx(m->nodeName.size(),-1);for(size_t j=0;j<m->joints.size();j++)jidx[m->joints[j]]=(int)j;std::vector<const AeJ*> at(m->joints.size(),nullptr);for(auto& q:aeJ_)at[q.joint]=&q;
    for(size_t j=0;j<m->joints.size();j++){int pn=m->parent[m->joints[j]];if(pn<0||jidx[pn]<0||!at[j]||!at[jidx[pn]])continue;const AeJ* a=at[jidx[pn]];const AeJ* b=at[j];ui.line(a->x,a->y,b->x,b->y,3.4f,Col::hex(0x0b0f14,0.55f));ui.line(a->x,a->y,b->x,b->y,1.8f,Col::hex(0x6fb4ff,0.85f));}
    for(auto& q:aeJ_){int node=m->joints[q.joint];bool sel=q.joint==aeBone_;bool pin=false;if(c)for(auto& pn:c->pins)if(pn.first==node)pin=true;bool keyed=c&&c->keys.count(node);float r=sel?7.f:4.6f;
      ui.circle(q.x,q.y,r+1.6f,Col::hex(0x0b0f14,0.7f),Col::hex(0x0b0f14,0.7f));ui.circle(q.x,q.y,r,pin?Col::hex(0xe5534b):sel?ORG:Col::hex(0x9fd0ff),pin?Col::hex(0xe5534b):sel?ORG:Col::hex(0x9fd0ff));if(keyed)ui.circle(q.x,q.y,2.4f,Col::hex(0xffe08a),Col::hex(0xffe08a));
      if(sel)ui.ring(q.x-r-4,q.y-r-4,(r+4)*2,(r+4)*2,r+4,1.5f,ORG);if(pin)ui.ring(q.x-r-3,q.y-r-3,(r+3)*2,(r+3)*2,r+3,1.5f,WHITE);}}
  // ---- top bar
  ui.rrect(0,0,W,40,0,BG1.with(0.96f),BG1.with(0.96f));ui.rrect(0,39,W,1,0,LINE,LINE);
  fbtn(120,8,6,86,28,"",0);ic::back(ui,22,20,16,WHITE);ui.text("Done",58,13,12,WHITE,F_BOLD,1);
  ui.text("ANIMATOR",108,6,12,TXT,F_BOLD);{std::string t=e->name+"   |   "+std::to_string(m->joints.size())+" bones   |   "+std::to_string(m->anims.size())+" clips";ui.text(t,108,23,10,TXT3);}
  ui.text(aeIK_?"Mode: IK  (drag a hand / foot)":"Mode: FK  (drag a joint)",W-14,14,11,aeIK_?ORG:ACC,F_BOLD,2);
  aeStudioDraw(W,H);
  // ---- tools panel (left)
  { float x=6,y=44,w=TOOLS_W-8,h=TL_H+26+4;(void)h;float ph=22+7*22+4;ui.shadow(x,y+2,w,ph,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,ph,5,BG1.with(0.95f),BG1.with(0.95f));ui.ring(x,y,w,ph,5,1,LINE);
    ui.rrect(x+1,y+1,w-2,18,4,BG2,BG2);ui.text("TOOLS",x+10,y+4,10,TXT2,F_BOLD);float bw=(w-18)/2,bx0=x+6,bx1=x+12+bw;float ry=y+22;
    bool pinned=false;if(c)for(auto& pn:c->pins)if(pn.first==bone)pinned=true;
    drawToolBtn(170,bx0,ry,bw,20,"Undo");drawToolBtn(171,bx1,ry,bw,20,"Redo");ry+=22;
    drawToolBtn(172,bx0,ry,bw,20,"Copy pose");drawToolBtn(173,bx1,ry,bw,20,"Paste pose");ry+=22;
    drawToolBtn(174,bx0,ry,bw,20,"Mirror L/R");{int ke=-1;int ei=1;if(c){auto f=c->keys.find(bone);if(f!=c->keys.end()){ke=keyIndexNear(f->second,aeT_);if(ke>=0)ei=f->second[ke].ease;}}drawToolBtn(175,bx1,ry,bw,20,std::string(ke>=0?EASE_N[ei]:"Ease"));}ry+=22;
    drawToolBtn(176,bx0,ry,bw,20,aeIK_?"IK mode":"FK mode",true);drawToolBtn(188,bx1,ry,bw,20,pinned?"Unpin joint":"Pin joint",pinned);ry+=22;
    drawToolBtn(189,bx0,ry,bw,20,std::string("Event: ")+EV_N[aeEvName_]);drawToolBtn(177,bx1,ry,bw,20,"+ Add event");ry+=22;
    drawToolBtn(190,bx0,ry,bw,20,"- Del event");drawToolBtn(181,bx1,ry,bw,20,"Key all bones");ry+=22;
    drawToolBtn(179,bx0,ry,bw,20,std::string("Loop: ")+(c&&!c->loop?"Off":"On"),c&&c->loop);drawToolBtn(180,bx1,ry,bw,20,std::string("In place: ")+(c&&c->inPlace?"On":"Off"),c&&c->inPlace);}
  // ---- bones panel (right)
  { float x=W-236,y=46,w=230,h=H-46-28;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.95f),BG1.with(0.95f));ui.ring(x,y,w,h,5,1,LINE);
    ui.rrect(x+1,y+1,w-2,25,4,BG2,BG2);ui.rrect(x+1,y+14,w-2,12,0,BG2,BG2);ui.text("BONES",x+12,y+8,10.5f,TXT2,F_BOLD);
    fbtn(130,x+w-62,y+3,26,20,"",3);ic::chevronU(ui,x+w-49,y+13,12,TXT2);fbtn(131,x+w-32,y+3,26,20,"",3);ic::chevronD(ui,x+w-19,y+13,12,TXT2);
    int vis=10;for(int i=0;i<vis;i++){int bi=(int)aeScroll_+i;if(bi>=(int)m->joints.size())break;float ry=y+30+i*19;bool on=bi==aeBone_;int node=m->joints[bi];bool hasKey=c&&c->keys.count(node);
      btn(640+i,x+4,ry,w-8,18);if(on){ui.rrect(x+4,ry,w-8,18,3,ACC.with(0.30f),ACC.with(0.24f));ui.rrect(x+4,ry,2,18,0,ACC,ACC);}
      std::string nm=m->nodeName[node];size_t cp=nm.find(':');if(cp!=std::string::npos)nm=nm.substr(cp+1);if(nm.size()>22)nm=nm.substr(0,21)+"..";ui.text(nm,x+14,ry+3,11,on?TXT:TXT2,on?F_BOLD:F_REG);
      if(hasKey){ui.circle(x+w-14,ry+9,3.5f,ORG,ORG);}}
    float sy=y+30+10*19+6;ui.rrect(x+1,sy,w-2,1,0,LINE,LINE);
    Vec3 rot,pos;if(c){auto f=c->keys.find(bone);if(f!=c->keys.end())clipEval(f->second,aeT_,rot,pos);}
    const char* an[3]={"Bend X","Turn Y","Roll Z"};for(int a=0;a<3;a++){float ry=sy+8+a*20;float v=a==0?rot.x:a==1?rot.y:rot.z;char b[24];std::snprintf(b,sizeof b,"%.0f",v);Col ac=Col::hex(AXC[a]);
      ui.text(an[a],x+10,ry+3,10.5f,TXT2,F_BOLD);float mx=x+74,mw=w-86;ui.rrect(mx,ry,mw,18,3,BG3,BG3);ui.ring(mx,ry,mw,18,3,1,LINE);ui.rrect(mx,ry,3,18,0,ac,ac);
      btn(320+a*2,mx+3,ry,30,18);btn(321+a*2,mx+mw-30,ry,30,18);ui.text("-",mx+17,ry,14,pressOf(320+a*2)>0.1f?ACC:TXT2,F_BOLD,1);ui.text("+",mx+mw-15,ry,14,pressOf(321+a*2)>0.1f?ACC:TXT2,F_BOLD,1);ui.text(b,mx+mw/2+1,ry+3,11,TXT,F_BOLD,1);}
    float py=sy+8+60+2;{float mx=x+74,mw=w-86;ui.text("Move Y",x+10,py+3,10.5f,TXT2,F_BOLD);ui.rrect(mx,py,mw,18,3,BG3,BG3);ui.ring(mx,py,mw,18,3,1,LINE);btn(326,mx+3,py,30,18);btn(327,mx+mw-30,py,30,18);
      ui.text("-",mx+17,py,14,TXT2,F_BOLD,1);ui.text("+",mx+mw-15,py,14,TXT2,F_BOLD,1);char b[24];std::snprintf(b,sizeof b,"%.0f cm",pos.y);ui.text(b,mx+mw/2+1,py+3,11,TXT,F_BOLD,1);}
    float by=y+h-26;fbtn(132,x+8,by,102,22,"",1);ui.text("Delete key",x+59,by+4,11,TXT,F_BOLD,1);fbtn(133,x+w-110,by,102,22,"",1);ui.text("Reset bone",x+w-59,by+4,11,TXT,F_BOLD,1);}
  // ---- timeline panel
  if(aeTab_==0){ float x=6,y=H-28-TL_H,w=W-248,h=TL_H;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.96f),BG1.with(0.96f));ui.ring(x,y,w,h,5,1,LINE);
    ui.rrect(x+1,y+1,w-2,24,4,BG2,BG2);ui.rrect(x+1,y+13,w-2,12,0,BG2,BG2);ui.text("TIMELINE",x+12,y+7,10.5f,TXT2,F_BOLD);
    float cx=x+84;ui.rrect(cx,y+3,200,20,3,BG3,BG3);ui.ring(cx,y+3,200,20,3,1,LINE);btn(121,cx,y+3,24,20);btn(122,cx+176,y+3,24,20);ui.text("<",cx+12,y+5,12,TXT2,F_BOLD,1);ui.text(">",cx+188,y+5,12,TXT2,F_BOLD,1);
    { std::string nm=c?c->name:"(no animation yet)";if(nm.size()>22)nm=nm.substr(0,21)+"..";ui.text(nm,cx+100,y+7,11,c?TXT:TXT3,F_BOLD,1);}
    fbtn(123,cx+206,y+3,58,20,"",0);ui.text("+ New",cx+235,y+7,11,WHITE,F_BOLD,1);fbtn(124,cx+268,y+3,58,20,"",2);ui.text("Delete",cx+297,y+7,11,WHITE,F_BOLD,1);
    float ry=y+30;fbtn(129,x+10,ry,70,24,"",aePlay_?1:0);if(aePlay_){ui.rrect(x+27,ry+7,4,10,1,TXT,TXT);ui.rrect(x+35,ry+7,4,10,1,TXT,TXT);}else ic::play(ui,x+30,ry+12,12,WHITE);ui.text(aePlay_?"Pause":"Play",x+58,ry+5,11,aePlay_?TXT:WHITE,F_BOLD,1);
    float bx=x+90;ui.text("Base",bx,ry+6,10.5f,TXT2,F_BOLD);ui.rrect(bx+32,ry,130,24,3,BG3,BG3);ui.ring(bx+32,ry,130,24,3,1,LINE);btn(125,bx+32,ry,22,24);btn(126,bx+140,ry,22,24);ui.text("<",bx+43,ry+5,12,TXT2,F_BOLD,1);ui.text(">",bx+151,ry+5,12,TXT2,F_BOLD,1);
    ui.text(c?(c->base.empty()?"Rest pose":c->base):"-",bx+97,ry+6,10.5f,TXT,F_BOLD,1);
    float dx=bx+176;ui.text("Length",dx,ry+6,10.5f,TXT2,F_BOLD);ui.rrect(dx+44,ry,86,24,3,BG3,BG3);ui.ring(dx+44,ry,86,24,3,1,LINE);btn(127,dx+44,ry,26,24);btn(128,dx+104,ry,26,24);ui.text("-",dx+57,ry+3,14,TXT2,F_BOLD,1);ui.text("+",dx+117,ry+3,14,TXT2,F_BOLD,1);
    { char b[24];std::snprintf(b,sizeof b,"%.2fs",c?c->dur:0.f);ui.text(b,dx+87,ry+6,11,TXT,F_BOLD,1);}
    fbtn(134,dx+140,ry,86,24,"",aeImp_?0:1);ui.text("Import FBX",dx+183,ry+5,10.5f,aeImp_?WHITE:TXT,F_BOLD,1);fbtn(135,dx+232,ry,50,24,"",1);ui.text("Save",dx+257,ry+5,11,TXT,F_BOLD,1);
    // layer row
    float ly=y+58;ui.text("Layer",x+14,ly+4,10.5f,TXT2,F_BOLD);ui.rrect(x+58,ly,150,20,3,BG3,BG3);ui.ring(x+58,ly,150,20,3,1,LINE);btn(182,x+58,ly,22,20);btn(183,x+186,ly,22,20);ui.text("<",x+69,ly+3,12,TXT2,F_BOLD,1);ui.text(">",x+197,ly+3,12,TXT2,F_BOLD,1);
    ui.text(c?(c->overlay.empty()?"none":c->overlay):"-",x+133,ly+4,10.5f,c&&!c->overlay.empty()?ORG:TXT3,F_BOLD,1);
    ui.text("Mask",x+222,ly+4,10.5f,TXT2,F_BOLD);ui.rrect(x+256,ly,120,20,3,BG3,BG3);ui.ring(x+256,ly,120,20,3,1,LINE);btn(184,x+256,ly,22,20);btn(185,x+354,ly,22,20);ui.text("<",x+267,ly+3,12,TXT2,F_BOLD,1);ui.text(">",x+365,ly+3,12,TXT2,F_BOLD,1);ui.text(MASK_N[c?c->mask:0],x+316,ly+4,10.5f,TXT,F_BOLD,1);
    ui.text("Mix",x+388,ly+4,10.5f,TXT2,F_BOLD);ui.rrect(x+414,ly,78,20,3,BG3,BG3);ui.ring(x+414,ly,78,20,3,1,LINE);btn(186,x+414,ly,22,20);btn(187,x+470,ly,22,20);ui.text("-",x+425,ly+1,14,TXT2,F_BOLD,1);ui.text("+",x+481,ly+1,14,TXT2,F_BOLD,1);{char b[16];std::snprintf(b,sizeof b,"%.0f%%",(c?c->weight:1.f)*100);ui.text(b,x+453,ly+4,10.5f,TXT,F_BOLD,1);}
    // track
    float ty=y+84,tx0=TL_X0,tx1=w-TL_PAD;float dur=c?c->dur:(m->anims.empty()||e->anim.empty()?1.f:std::max(0.1f,m->anims[std::max(0,m->animIndex(e->anim))].dur));
    ui.rrect(x+10,ty,w-20,56,4,BG0,BG0);ui.ring(x+10,ty,w-20,56,4,1,LINE);
    ui.text("Selected bone",x+18,ty+8,10,TXT3,F_BOLD);{std::string nm=m->nodeName[bone];size_t cp=nm.find(':');if(cp!=std::string::npos)nm=nm.substr(cp+1);if(nm.size()>16)nm=nm.substr(0,15)+"..";ui.text(nm,x+18,ty+22,11.5f,ORG,F_BOLD);}
    btn(149,x+tx0,ty,tx1-tx0,56);float X0=x+tx0,X1=x+tx1;
    int ticks=(int)std::ceil(dur/0.25f);for(int i=0;i<=ticks;i++){float t=std::min(dur,i*0.25f);float px=X0+(X1-X0)*t/dur;bool big=i%4==0;ui.rrect(px,ty+2,1,big?9:5,0,LINE2,LINE2);if(big){char b[12];std::snprintf(b,sizeof b,"%.0fs",t);ui.text(b,px+3,ty+1,9,TXT3);}}
    ui.rrect(X0,ty+32,X1-X0,1,0,LINE,LINE);
    if(c){for(auto& kv:c->keys){bool selb=kv.first==bone;for(auto& k:kv.second){float px=X0+(X1-X0)*k.t/dur,py=ty+(selb?33:47);float r=selb?6.f:3.f;Col kc=selb?ORG:TXT3;
          if(selb){ui.line(px-r,py,px,py-r,2,kc);ui.line(px,py-r,px+r,py,2,kc);ui.line(px+r,py,px,py+r,2,kc);ui.line(px,py+r,px-r,py,2,kc);ui.circle(px,py,3.2f,kc,kc);}else ui.circle(px,py,r,kc.with(0.7f),kc.with(0.7f));}}
      for(auto& ev:c->events){float px=X0+(X1-X0)*ev.first/dur;ui.line(px,ty+12,px,ty+30,1.6f,Col::hex(0x5fb35f));ui.tri(px,ty+12,px+8,ty+16,px,ty+20,Col::hex(0x5fb35f));ui.text(ev.second,px+9,ty+14,8.5f,Col::hex(0x8fe08f));}}
    float hx=X0+(X1-X0)*clampf(aeT_/dur,0,1);ui.rrect(hx-0.75f,ty+2,1.5f,52,0,ACC,ACC);ui.rrect(hx-5,ty+2,10,8,2,ACC,ACC);}
  // import popup
  if(aeImp_){float x=W-236-260,y=H-28-TL_H-12-24-24*6;float w=250,h=24+24*6+8;ui.shadow(x,y+2,w,h,5,14,Col::hex(0,0.5f));ui.rrect(x,y,w,h,5,BG1.with(0.98f),BG1.with(0.98f));ui.ring(x,y,w,h,5,1,LINE2);
    ui.rrect(x+1,y+1,w-2,22,4,BG2,BG2);ui.text("IMPORT ANIMATIONS FROM FILE",x+10,y+6,10,TXT2,F_BOLD);
    if(aeFiles_.empty()){ui.text("Put .fbx / .glb files in",x+12,y+36,11,TXT3);ui.text("Files > SabakEngine > Models",x+12,y+54,11,TXT3);}
    for(int i=0;i<6&&i<(int)aeFiles_.size();i++){float ry=y+26+i*24;btn(600+i,x+4,ry,w-8,22);if(pressOf(600+i)>0.1f)ui.rrect(x+4,ry,w-8,22,3,ACC.with(0.35f),ACC.with(0.35f));std::string nm=aeFiles_[i];size_t sl=nm.find_last_of('/');if(sl!=std::string::npos)nm=nm.substr(sl+1);if(nm.size()>28)nm=nm.substr(0,27)+"..";ui.text(nm,x+12,ry+5,11.5f,TXT);}}
  // status bar
  ui.rrect(0,H-22,W,22,0,BG1.with(0.97f),BG1.with(0.97f));ui.rrect(0,H-22,W,1,0,LINE,LINE);
  { char b[96];std::snprintf(b,sizeof b,"Time %.2fs   |   Bone %s   |   %s",aeT_,m->nodeName[bone].c_str(),c?c->name.c_str():"no clip");ui.text(b,12,H-17,10,TXT3);ui.text("Drag a joint on the character   Drag empty space to orbit   Drag the timeline to scrub",W-12,H-17,10,TXT3,F_REG,2);}
}
void App::drawToolBtn(int id,float x,float y,float w,float h,const std::string& label,bool on){btn(id,x,y,w,h);float k=pressOf(id);Col c1=on?ACC.with(0.30f):BG3,c2=on?ACC.with(0.22f):BG2;if(k>0.05f){c1=ACC.with(0.5f);c2=ACC.with(0.4f);}
  ui.rrect(x,y,w,h,3,c1,c2);ui.ring(x,y,w,h,3,1,on?ACC:LINE);std::string t=label;if(t.size()>13)t=t.substr(0,12)+"..";ui.text(t,x+w/2,y+4,10.5f,on?TXT:TXT2,F_BOLD,1);}
}
