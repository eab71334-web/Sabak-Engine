// Sabak Core - in-app scene editor: hierarchy, inspector, gizmo, add\/delete\/duplicate, save, and a Play mode that runs C++ scripts.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include "app.h"
namespace sk {
static const Col WHITE=Col::hex(0xffffff), MUTED=Col::hex(0xaab4d0), DIM=Col::hex(0x7d88a8), GREEN=Col::hex(0x34d399), CYAN=Col::hex(0x22d3ee), VIOLET=Col::hex(0x8b5cf6);
static const unsigned AXC[3]={0xf87171,0x4ade80,0x60a5fa};
static const unsigned SWATCH[8]={0xd8c4a0,0xb32428,0xe8b894,0x2a6b2d,0x4dd9ff,0x60a5fa,0xfbbf24,0xa78bfa};
static void squishE(float p,float&x,float&y,float&w,float&h){float k=1-0.05f*p;x+=w*(1-k)/2;y+=h*(1-k)/2;w*=k;h*=k;}

int App::meshFor(int prim){int& m=meshIds_[prim];if(m>=0)return m;Vec3 w{1,1,1};MeshData d;
  switch(prim){case P_BOX:d=makeBox(1,1,1,w);break;case P_SPHERE:d=makeSphere(0.5f,20,14,w);break;case P_CYL:d=makeCylinder(0.5f,0.5f,1,20,w);break;case P_CONE:d=makeCone(0.5f,1,16,w);break;
    case P_PLANE:d=makeBox(1,0.02f,1,w);break;case P_PYRAMID:d=makeCylinder(0.7071f,0.01f,1,4,w);break;
    case P_TERRAIN:d=makeTerrain(260,180,[&](float x,float z){return scene_.height(x,z);},[&](float,float y,float){
      if(y<0.2f)return Vec3{0.78f,0.72f,0.5f};if(y<2.0f)return Vec3{0.42f,0.62f,0.28f};if(y<9)return Vec3{0.30f,0.52f,0.22f};return Vec3{0.5f,0.5f,0.48f};});break;
    default:d=makeBox(0.01f,0.01f,0.01f,w);}
  m=r_->upload(d);return m;}

void App::renderScene(const Scene& sc,Vec3 cam,Vec3 target,float fov){std::vector<DrawItem> items;items.reserve(sc.ents.size());
  for(auto& e:sc.ents){if(e.prim==P_EMPTY)continue;DrawItem it;it.mesh=meshFor(e.prim);it.model=sc.world(e.id);it.tint={e.color.r,e.color.g,e.color.b};it.castShadow=e.prim!=P_TERRAIN&&e.prim!=P_PLANE;items.push_back(it);}
  r_->setCamera(cam,target,fov);r_->setSun({0.5f,0.8f,0.35f},{target.x,target.y,target.z},55);r_->render(items);}

void App::saveScene(bool quiet){if(cur_<0||cur_>=(int)projects_.size())return;bool ok=scene_.save(projects_[cur_].dir+"/scenes/main.scene");if(!quiet)toast(ok?"Scene saved":"Save failed");}
void App::select(int id){sel_=id;}
void App::rebuildRows(){rows_.clear();std::vector<int> roots;for(auto&e:scene_.ents)if(e.parent<0||!scene_.get(e.parent))roots.push_back(e.id);
  std::function<void(int,int)> walk=[&](int id,int d){rows_.push_back({id,d});if(expanded_.count(id))for(int c:scene_.childrenOf(id))walk(c,d+1);};for(int r:roots)walk(r,0);}

void App::startPlay(){if(play_)return;snap_=scene_;play_=true;addMenu_=false;scripts_.clear();playT_=0;sctx_.time=0;in_=Input();
  for(auto& e:scene_.ents){if(e.script.empty())continue;auto it=scriptRegistry().find(e.script);if(it==scriptRegistry().end())continue;Script* s=it->second();s->self=&e;s->ctx=&sctx_;scripts_[e.id].reset(s);}
  // scripts hold pointers into the entity vector, so start them only after it is final
  for(auto& kv:scripts_){kv.second->self=scene_.get(kv.first);kv.second->onStart();}
  pYaw_=0;pPitch_=0.3f;stickId_=lookId_=-1;}
void App::stopPlay(){if(!play_)return;scripts_.clear();scene_=snap_;play_=false;in_=Input();rebuildRows();}

void App::editorUpdate(float dt){
  if(play_){playT_+=dt;sctx_.time=playT_;float kx=(keyDown_[3]?1:0)-(keyDown_[1]?1:0),ky=(keyDown_[2]?1:0)-(keyDown_[0]?1:0);
    in_.moveX=std::max(-1.f,std::min(1.f,stickX_+kx));in_.moveY=std::max(-1.f,std::min(1.f,stickY_+ky));in_.camYaw=pYaw_;
    for(auto& kv:scripts_){kv.second->self=scene_.get(kv.first);if(kv.second->self)kv.second->onUpdate(dt);}}
  // hold-to-repeat on inspector steppers
  if(heldId_>=300&&heldId_<318){repeatT_+=dt;if(repeatT_>0.45f){repeatT_-=0.07f;editorTap(heldId_);}}else repeatT_=0;}

bool App::nearPanel(float x,float y) const{float W=W_/ui.s();return y<50||x<192||(sel_>=0&&x>W-214);}

void App::gizmoUpdate(){gOk_=false;const Entity* e=scene_.get(sel_);if(!e||e->prim==P_TERRAIN||play_)return;Vec3 o=scene_.worldPos(sel_);Vec3 c=r_->cameraPos();gL_=len(o-c)*0.14f;
  float sx,sy;if(!r_->project(o,sx,sy))return;gO_[0]=sx;gO_[1]=sy;Vec3 ax[3]={{1,0,0},{0,1,0},{0,0,1}};
  for(int a=0;a<3;a++){float tx,ty;if(!r_->project(o+ax[a]*gL_,tx,ty))return;gT_[a][0]=tx;gT_[a][1]=ty;}gOk_=true;}

bool App::pickAt(float px,float py){Vec3 o,d;r_->ray(px,py,o,d);int best=-1;float bt=1e9f;
  for(auto& e:scene_.ents){if(e.prim==P_TERRAIN)continue;Mat4 m=scene_.world(e.id);Vec3 c{m.m[12],m.m[13],m.m[14]};float sx=len({m.m[0],m.m[1],m.m[2]}),sy=len({m.m[4],m.m[5],m.m[6]}),sz=len({m.m[8],m.m[9],m.m[10]});
    float rad;if(e.prim==P_EMPTY){c=c+Vec3{0,1,0}*sy;rad=1.3f*std::max(sx,std::max(sy,sz));}
    else{if(e.prim==P_CYL||e.prim==P_CONE||e.prim==P_PYRAMID)c=c+Vec3{m.m[4],m.m[5],m.m[6]}*0.5f;rad=0.5f*std::max(sx,std::max(sy,sz))*1.05f;}
    Vec3 oc=c-o;float tca=dot(oc,d);if(tca<0)continue;float d2=dot(oc,oc)-tca*tca;if(d2>rad*rad)continue;float t=tca-std::sqrt(rad*rad-d2);if(e.prim==P_EMPTY)t+=2.0f;   // prefer real meshes over group handles
    if(t<bt){bt=t;best=e.id;}}
  if(best<0)return false;
  // select the group root (the top-most Empty ancestor), so a whole tree or house moves together
  int sel=best;const Entity* e=scene_.get(sel);while(e&&e->parent>=0){const Entity* p=scene_.get(e->parent);if(!p||p->prim!=P_EMPTY)break;sel=p->id;e=p;}
  select(sel);return true;}

void App::editorPointer(int type,int id,float px,float py){float s=ui.s();float x=px/s,y=py/s;
  auto slot=[&](int pid,bool create)->int{for(int i=0;i<8;i++)if(ptId_[i]==pid)return i;if(create)for(int i=0;i<8;i++)if(ptId_[i]<0){ptId_[i]=pid;return i;}return -1;};
  if(play_){
    if(type==0){float W=W_/s;if(x<W*0.5f&&stickId_<0){stickId_=id;sx0_=px;sy0_=py;}else if(lookId_<0){lookId_=id;lx_=px;ly_=py;}}
    else if(type==1){if(id==stickId_){float dx=(px-sx0_)/(48*s),dy=(py-sy0_)/(48*s);float l=std::sqrt(dx*dx+dy*dy);if(l>1){dx/=l;dy/=l;}stickX_=dx;stickY_=dy;}
      else if(id==lookId_){pYaw_-=(px-lx_)*0.006f/s;pPitch_=clampf(pPitch_+(py-ly_)*0.005f/s,-0.2f,1.2f);lx_=px;ly_=py;}}
    else{if(id==stickId_){stickId_=-1;stickX_=stickY_=0;}if(id==lookId_)lookId_=-1;}
    return;}
  if(type==0){
    if(addMenu_){addMenu_=false;}
    int sl=slot(id,true);if(sl>=0){ptX_[sl]=px;ptY_[sl]=py;}
    // gizmo handle?
    if(gOk_&&sel_>=0){for(int a=0;a<3;a++){float dx=px-gT_[a][0],dy=py-gT_[a][1];if(dx*dx+dy*dy<26*26*s*s){dragAxis_=a;dragId_=id;lastX_=px;lastY_=py;return;}}}
    if(x>=8&&x<=184&&y>=54){hierId_=id;lastY_=py;moved_=false;downX_=px;downY_=py;return;}
    if(nearPanel(x,y))return;
    // viewport: orbit / pinch / tap-to-pick
    if(orbitId_<0){orbitId_=id;lastX_=px;lastY_=py;downX_=px;downY_=py;moved_=false;}
    else if(pinchA_<0){pinchA_=orbitId_;pinchB_=id;int a=slot(pinchA_,false),b=slot(pinchB_,false);if(a>=0&&b>=0)pinchD_=std::hypot(ptX_[a]-ptX_[b],ptY_[a]-ptY_[b]);moved_=true;}
  }else if(type==1){
    int sl=slot(id,false);if(sl>=0){ptX_[sl]=px;ptY_[sl]=py;}
    if(id==dragId_&&dragAxis_>=0){Entity* e=scene_.get(sel_);if(e){float dx=px-lastX_,dy=py-lastY_;int a=dragAxis_;float vx=gT_[a][0]-gO_[0],vy=gT_[a][1]-gO_[1];float vl=std::sqrt(vx*vx+vy*vy);
        if(vl>4){float proj=(dx*vx+dy*vy)/vl;float amt=proj/vl*gL_;float* P=a==0?&e->pos.x:a==1?&e->pos.y:&e->pos.z;float* R=a==0?&e->rot.x:a==1?&e->rot.y:&e->rot.z;float* S=a==0?&e->scale.x:a==1?&e->scale.y:&e->scale.z;
          if(mode_==0)*P+=amt;else if(mode_==1)*R+=proj*0.6f;else *S=std::max(0.05f,*S+amt*0.5f);}}
      lastX_=px;lastY_=py;return;}
    if(id==hierId_){float d=py-lastY_;if(std::fabs(py-downY_)>8*s)moved_=true;if(moved_){hierScroll_=std::max(0.0f,hierScroll_-d/(24*s));}lastY_=py;return;}
    if(pinchA_>=0&&(id==pinchA_||id==pinchB_)){int a=slot(pinchA_,false),b=slot(pinchB_,false);if(a>=0&&b>=0){float nd=std::hypot(ptX_[a]-ptX_[b],ptY_[a]-ptY_[b]);if(pinchD_>1&&nd>1)ecDist_=clampf(ecDist_*pinchD_/nd,4,120);pinchD_=nd;}return;}
    if(id==orbitId_&&pinchA_<0){float dx=px-lastX_,dy=py-lastY_;if(std::hypot(px-downX_,py-downY_)>8*s)moved_=true;if(moved_){ecYaw_-=dx*0.008f/s;ecPitch_=clampf(ecPitch_+dy*0.006f/s,0.05f,1.5f);}lastX_=px;lastY_=py;}
  }else{
    int sl=slot(id,false);if(sl>=0)ptId_[sl]=-1;
    if(id==dragId_){dragId_=-1;dragAxis_=-1;return;}
    if(id==hierId_){hierId_=-1;if(!moved_){rebuildRows();int row=(int)((y-54-34)/24+hierScroll_);float rx=x-8;if(row>=0&&row<(int)rows_.size()){int eid=rows_[row].first;int d=rows_[row].second;
          if(rx<14+d*12+12&&!scene_.childrenOf(eid).empty()&&rx>d*12){if(expanded_.count(eid))expanded_.erase(eid);else expanded_.insert(eid);rebuildRows();}
          else{select(eid);if(const Entity* e=scene_.get(eid))if(e->prim!=P_TERRAIN)ecTarget_=scene_.worldPos(eid);}}}return;}
    if(pinchA_>=0&&(id==pinchA_||id==pinchB_)){pinchA_=pinchB_=-1;orbitId_=-1;return;}
    if(id==orbitId_){orbitId_=-1;if(!moved_){if(!pickAt(px,py))select(-1);}}
  }
}

void App::editorTap(int id){
  Entity* e=scene_.get(sel_);
  if(id>=110&&id<=112)mode_=id-110;
  else if(id==113)addMenu_=!addMenu_;
  else if(id==114){if(e&&e->prim!=P_TERRAIN){scene_.remove(sel_);sel_=-1;rebuildRows();toast("Deleted");}}
  else if(id==115){if(e&&e->prim!=P_TERRAIN){int n=scene_.duplicate(sel_);select(n);rebuildRows();toast("Duplicated");}}
  else if(id==116)saveScene();
  else if(id==117)startPlay();
  else if(id==118)stopPlay();
  else if(id>=150&&id<158){if(e)e->color=Col::hex(SWATCH[id-150]);}
  else if(id==160){if(e){std::vector<std::string> names{""};for(auto&kv:scriptRegistry())names.push_back(kv.first);size_t i=0;for(;i<names.size();i++)if(names[i]==e->script)break;e->script=names[(i+1)%names.size()];}}
  else if(id>=300&&id<318){if(e){int k=id-300,idx=k/2,sign=(k%2)?1:-1,grp=idx/3,ax=idx%3;Vec3* v=grp==0?&e->pos:grp==1?&e->rot:&e->scale;float st=grp==0?0.5f:grp==1?5.0f:0.1f;
      float* f=ax==0?&v->x:ax==1?&v->y:&v->z;*f+=sign*st;if(grp==2&&*f<0.05f)*f=0.05f;}}
  else if(id>=400&&id<408){int prim=id-400;if(prim!=P_TERRAIN){Entity& n=scene_.add(prim,std::string(PRIM_NAME[prim]));n.pos=ecTarget_;n.pos.y=scene_.height(n.pos.x,n.pos.z)+(prim==P_BOX?0.5f:prim==P_SPHERE?0.5f:0.0f);n.color=Col::hex(0xd8c4a0);select(n.id);rebuildRows();addMenu_=false;}}
  else if(id==119){/* reserved */}
}

void App::drawEditor(){float W=W_/ui.s();float H=393;A_=1;oy_=0;
  Vec3 cam,tg;
  if(play_){const Entity* h=nullptr;for(auto&kv:scripts_){const Entity* en=scene_.get(kv.first);if(en&&en->script=="PlayerController"){h=en;break;}}if(!h)h=scene_.find("Hero");
    Vec3 p=h?scene_.worldPos(h->id):Vec3{0,2,0};tg={p.x,p.y+1.4f,p.z};cam=tg+Vec3{std::sin(pYaw_)*std::cos(pPitch_)*7,std::sin(pPitch_)*7,std::cos(pYaw_)*std::cos(pPitch_)*7};
    float gh=scene_.height(cam.x,cam.z)+0.6f;if(cam.y<gh)cam.y=gh;}
  else{tg=ecTarget_;cam=tg+Vec3{std::sin(ecYaw_)*std::cos(ecPitch_)*ecDist_,std::sin(ecPitch_)*ecDist_,std::cos(ecYaw_)*std::cos(ecPitch_)*ecDist_};
    float gh=scene_.height(cam.x,cam.z)+1.0f;if(cam.y<gh)cam.y=gh;}
  renderScene(scene_,cam,tg,0.95f);gizmoUpdate();ui.begin(W_,H_);
  float S=ui.s();
  if(play_){
    { float x=14,y=14,w=104,h=36;btn(118,x,y,w,h);float k=pressOf(118);squishE(k,x,y,w,h);ui.shadow(x,y+3,w,h,18,10,Col::hex(0,0.4f));ui.rrect(x,y,w,h,18,Col::hex(0xef4444),Col::hex(0xb91c1c));ui.ring(x,y,w,h,18,1,WHITE.with(0.4f));
      ui.rrect(x+14,y+12,12,12,3,WHITE,WHITE);ui.text("Stop",x+66,y+9,13,WHITE,F_BOLD,1);}
    ui.rrect(W-92,14,78,30,15,Col::hex(0x0b0f24,0.55f),Col::hex(0x0b0f24,0.55f));ui.text(std::to_string((int)(fps_+0.5f))+" fps",W-53,20,12,WHITE,F_BOLD,1);
    float jx=96,jy=H-92,kx=stickX_*30,ky=stickY_*30;ui.circle(jx,jy,52,WHITE.with(0.10f),WHITE.with(0.04f));ui.ring(jx-52,jy-52,104,104,52,1.5f,WHITE.with(0.3f));ui.circle(jx+kx,jy+ky,24,WHITE.with(0.55f),WHITE.with(0.3f));
    return;}
  // ---- gizmo
  if(gOk_){float ox=gO_[0]/S,oy=gO_[1]/S;for(int a=0;a<3;a++){float tx=gT_[a][0]/S,ty=gT_[a][1]/S;Col c=Col::hex(AXC[a]);float dx=tx-ox,dy=ty-oy,l=std::sqrt(dx*dx+dy*dy);int n=std::max(2,(int)(l/3));
      for(int i=1;i<n;i++)ui.circle(ox+dx*i/n,oy+dy*i/n,1.4f,c,c);
      bool act=dragAxis_==a;float rr=act?11:8;
      if(mode_==2)ui.rrect(tx-rr,ty-rr,rr*2,rr*2,3,c,c);else if(mode_==1){ui.circle(tx,ty,rr,c.with(0.25f),c.with(0.25f));ui.ring(tx-rr,ty-rr,rr*2,rr*2,rr,2.5f,c);}else ui.circle(tx,ty,rr,c,c);
      ui.ring(tx-rr,ty-rr,rr*2,rr*2,mode_==2?3:rr,1,WHITE.with(0.8f));}
    ui.circle(ox,oy,3.5f,WHITE,WHITE);}
  // ---- toolbar
  ui.rrect(0,0,W,48,0,Col::hex(0x0b0f24,0.78f),Col::hex(0x0b0f24,0.70f));ui.rrect(0,47,W,1,0,WHITE.with(0.12f),WHITE.with(0.12f));
  { float x=10,y=7,w=34,h=34;btn(5,x,y,w,h);float k=pressOf(5);squishE(k,x,y,w,h);ui.rrect(x,y,w,h,17,WHITE.with(0.16f),WHITE.with(0.07f));ui.ring(x,y,w,h,17,1,WHITE.with(0.25f));ui.text("<",x+w/2,y+3,22,WHITE,F_BOLD,1);}
  { std::string nm=cur_>=0&&cur_<(int)projects_.size()?projects_[cur_].name:"Project";if(nm.size()>11)nm=nm.substr(0,10)+"..";ui.text(nm,54,8,13,WHITE,F_BOLD);ui.text("Scene: main",54,26,10.5f,DIM);}
  const char* ml[3]={"Move","Rotate","Scale"};float sx0=146;
  ui.rrect(sx0-3,7,3*66+6,34,17,WHITE.with(0.10f),WHITE.with(0.05f));
  for(int i=0;i<3;i++){float x=sx0+i*66,y=9,w=64,h=30;btn(110+i,x,y,w,h);if(mode_==i){ui.rrect(x,y,w,h,15,Col::hex(0x9b6cff),Col::hex(0x2f5fe8));ui.ring(x,y,w,h,15,1,WHITE.with(0.4f));}ui.text(ml[i],x+w/2,y+6,12,WHITE,mode_==i?F_BOLD:F_REG,1);}
  auto tb=[&](int id,float x,float w,const char* t,Col a,Col b){float y=9,h=30;btn(id,x,y,w,h);float k=pressOf(id);float xx=x,yy=y,ww=w,hh=h;squishE(k,xx,yy,ww,hh);ui.rrect(xx,yy,ww,hh,15,a,b);ui.ring(xx,yy,ww,hh,15,1,WHITE.with(0.3f));ui.text(t,xx+ww/2,yy+6,12,WHITE,F_BOLD,1);};
  float bx=sx0+3*66+14;tb(113,bx,64,"+ Add",CYAN.with(0.45f),CYAN.with(0.2f));tb(115,bx+70,52,"Copy",WHITE.with(0.14f),WHITE.with(0.06f));tb(114,bx+128,56,"Delete",Col::hex(0xef4444,0.55f),Col::hex(0xb91c1c,0.4f));
  tb(116,bx+190,54,"Save",WHITE.with(0.14f),WHITE.with(0.06f));
  { float x=W-100,y=7,w=90,h=34;btn(117,x,y,w,h);float k=pressOf(117);squishE(k,x,y,w,h);ui.shadow(x,y+3,w,h,17,10,GREEN.with(0.5f));ui.rrect(x,y,w,h,17,Col::hex(0x34d399),Col::hex(0x059669));ui.ring(x,y,w,h,17,1,WHITE.with(0.45f));
    float tx=x+22,ty=y+17;ui.rrect(tx-4,ty-7,3,14,1,WHITE,WHITE);ui.rrect(tx,ty-5,3,10,1,WHITE,WHITE);ui.rrect(tx+3,ty-3,3,6,1,WHITE,WHITE);ui.text("Play",x+58,y+8,13,WHITE,F_BOLD,1);}
  // ---- hierarchy
  rebuildRows();{float x=8,y=54,w=176,h=H-62;ui.shadow(x,y,w,h,16,14,Col::hex(0,0.4f));ui.rrect(x,y,w,h,16,Col::hex(0x0b0f24,0.78f),Col::hex(0x0b0f24,0.70f));ui.ring(x,y,w,h,16,1,WHITE.with(0.16f));
    ui.text("Hierarchy",x+14,y+9,13,WHITE,F_BOLD);ui.text(std::to_string(scene_.ents.size()),x+w-14,y+11,11,DIM,F_REG,2);
    int vis=(int)((h-38)/24);int maxScroll=std::max(0,(int)rows_.size()-vis);hierScroll_=std::min(hierScroll_,(float)maxScroll);int first=(int)hierScroll_;
    for(int i=0;i<vis&&first+i<(int)rows_.size();i++){int eid=rows_[first+i].first,dep=rows_[first+i].second;const Entity* e=scene_.get(eid);if(!e)continue;float ry=y+34+i*24;bool on=eid==sel_;
      if(on){ui.rrect(x+6,ry,w-12,22,8,Col::hex(0x8b5cf6,0.55f),Col::hex(0x2563eb,0.45f));ui.ring(x+6,ry,w-12,22,8,1,WHITE.with(0.35f));}
      float ix=x+12+dep*12;bool hasKids=!scene_.childrenOf(eid).empty();if(hasKids)ui.text(expanded_.count(eid)?"v":">",ix,ry+3,11,MUTED,F_BOLD);
      Col dot=e->prim==P_EMPTY?Col::hex(0xa78bfa):e->prim==P_TERRAIN?Col::hex(0x4ade80):e->color;ui.circle(ix+16,ry+11,3.5f,dot,dot);
      std::string nm=e->name;if(nm.size()>15)nm=nm.substr(0,14)+"..";ui.text(nm,ix+24,ry+4,11.5f,on?WHITE:Col::hex(0xd5dbee),on?F_BOLD:F_REG);}}
  // ---- inspector
  if(const Entity* e=scene_.get(sel_)){float x=W-206,y=54,w=198,h=H-62;ui.shadow(x,y,w,h,16,14,Col::hex(0,0.4f));ui.rrect(x,y,w,h,16,Col::hex(0x0b0f24,0.80f),Col::hex(0x0b0f24,0.72f));ui.ring(x,y,w,h,16,1,WHITE.with(0.16f));
    std::string nm=e->name;if(nm.size()>16)nm=nm.substr(0,15)+"..";ui.text(nm,x+14,y+9,14,WHITE,F_BOLD);ui.text(std::string(PRIM_NAME[e->prim])+"  #"+std::to_string(e->id),x+14,y+28,10.5f,DIM);
    for(int i=0;i<8;i++){float cx=x+14+i*22,cy=y+48;btn(150+i,cx,cy,18,18);Col c=Col::hex(SWATCH[i]);ui.circle(cx+9,cy+9,8,c,c);bool same=std::fabs(e->color.r-c.r)<0.01f&&std::fabs(e->color.g-c.g)<0.01f&&std::fabs(e->color.b-c.b)<0.01f;if(same)ui.ring(cx-2,cy-2,22,22,11,1.5f,WHITE);}
    if(e->prim!=P_TERRAIN){const char* gl[3]={"Position","Rotation","Scale"};const Vec3* vs[3]={&e->pos,&e->rot,&e->scale};
      for(int g=0;g<3;g++){float gy=y+76+g*72;ui.text(gl[g],x+14,gy,10.5f,MUTED,F_BOLD);
        for(int a=0;a<3;a++){float ry=gy+14+a*19;float v=a==0?vs[g]->x:a==1?vs[g]->y:vs[g]->z;char b[24];std::snprintf(b,sizeof b,g==1?"%.0f":"%.2f",v);
          Col ac=Col::hex(AXC[a]);ui.rrect(x+14,ry+1,16,16,5,ac.with(0.85f),ac.with(0.6f));ui.text(a==0?"X":a==1?"Y":"Z",x+22,ry+2,10.5f,WHITE,F_BOLD,1);
          int idx=g*3+a;float mx=x+38,mw=w-52;ui.rrect(mx,ry,mw,18,9,WHITE.with(0.08f),WHITE.with(0.04f));
          btn(300+idx*2,mx,ry,28,18);btn(301+idx*2,mx+mw-28,ry,28,18);ui.text("-",mx+14,ry+0,14,pressOf(300+idx*2)>0.1f?CYAN:WHITE,F_BOLD,1);ui.text("+",mx+mw-14,ry+0,14,pressOf(301+idx*2)>0.1f?CYAN:WHITE,F_BOLD,1);ui.text(b,mx+mw/2,ry+3,11.5f,WHITE,F_REG,1);}}
      float sy=y+h-34;btn(160,x+12,sy,w-24,24);ui.rrect(x+12,sy,w-24,24,12,VIOLET.with(0.35f),VIOLET.with(0.18f));ui.ring(x+12,sy,w-24,24,12,1,VIOLET.with(0.7f));
      ui.text(e->script.empty()?"Script: none (tap)":"Script: "+e->script,x+w/2,sy+5,11,WHITE,F_BOLD,1);}}
  else{ui.text("Tap an object to edit it",W/2+30,H-28,11,Col::hex(0xffffff,0.55f),F_REG,1);}
  // ---- add menu
  if(addMenu_){float x=bx-4,y=52,w=176,h=8+7*28;ui.shadow(x,y,w,h,16,16,Col::hex(0,0.5f));ui.rrect(x,y,w,h,16,Col::hex(0x10142e,0.97f),Col::hex(0x0b0f24,0.97f));ui.ring(x,y,w,h,16,1,WHITE.with(0.2f));
    int order[7]={P_EMPTY,P_BOX,P_SPHERE,P_CYL,P_CONE,P_PYRAMID,P_PLANE};for(int i=0;i<7;i++){float ry=y+8+i*28;btn(400+order[i],x+6,ry,w-12,26);float k=pressOf(400+order[i]);if(k>0.1f)ui.rrect(x+6,ry,w-12,26,10,VIOLET.with(0.4f),VIOLET.with(0.25f));
      ui.text(PRIM_NAME[order[i]],x+22,ry+6,12.5f,WHITE,F_REG);}}
}
}
