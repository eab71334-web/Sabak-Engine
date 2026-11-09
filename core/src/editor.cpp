// Sabak Core - in-app scene editor: hierarchy, inspector, gizmo, add\/delete\/duplicate, save, and a Play mode that runs C++ scripts.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include "app.h"
#include "theme.h"
namespace sk {
static const Col WHITE=Col::hex(0xffffff);
using namespace sk::th;
static const unsigned AXC[3]={0xe5534b,0x5fb35f,0x4b92e6};
static const unsigned SWATCH[8]={0xcfc6b4,0x7a2f2f,0xd0a07c,0x3f6b3a,0x5fb8cc,0x4b6fa8,0xc9a23a,0x8c7fb0};
static void squishE(float p,float&x,float&y,float&w,float&h){float k=1-0.05f*p;x+=w*(1-k)/2;y+=h*(1-k)/2;w*=k;h*=k;}

int App::meshFor(int prim){int& m=meshIds_[prim];if(m>=0)return m;Vec3 w{1,1,1};MeshData d;
  switch(prim){case P_BOX:d=makeBox(1,1,1,w);break;case P_SPHERE:d=makeSphere(0.5f,20,14,w);break;case P_CYL:d=makeCylinder(0.5f,0.5f,1,20,w);break;case P_CONE:d=makeCone(0.5f,1,16,w);break;
    case P_PLANE:d=makeBox(1,0.02f,1,w);break;case P_PYRAMID:d=makeCylinder(0.7071f,0.01f,1,4,w);break;
    case P_TERRAIN:d=makeTerrain(260,180,[&](float x,float z){return scene_.height(x,z);},[&](float,float y,float){
      if(y<0.2f)return Vec3{0.66f,0.62f,0.50f};if(y<2.0f)return Vec3{0.36f,0.48f,0.27f};if(y<9)return Vec3{0.28f,0.40f,0.22f};return Vec3{0.45f,0.45f,0.43f};});break;
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

bool App::nearPanel(float x,float y) const{float W=W_/ui.s();return y<42||x<190||y>H_/ui.s()-24||(sel_>=0&&x>W-210);}

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
    if(x>=6&&x<=184&&y>=46&&y<=H_/s-26){hierId_=id;lastY_=py;moved_=false;downX_=px;downY_=py;return;}
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
    if(id==hierId_){float d=py-lastY_;if(std::fabs(py-downY_)>8*s)moved_=true;if(moved_){hierScroll_=std::max(0.0f,hierScroll_-d/(22*s));}lastY_=py;return;}
    if(pinchA_>=0&&(id==pinchA_||id==pinchB_)){int a=slot(pinchA_,false),b=slot(pinchB_,false);if(a>=0&&b>=0){float nd=std::hypot(ptX_[a]-ptX_[b],ptY_[a]-ptY_[b]);if(pinchD_>1&&nd>1)ecDist_=clampf(ecDist_*pinchD_/nd,4,120);pinchD_=nd;}return;}
    if(id==orbitId_&&pinchA_<0){float dx=px-lastX_,dy=py-lastY_;if(std::hypot(px-downX_,py-downY_)>8*s)moved_=true;if(moved_){ecYaw_-=dx*0.008f/s;ecPitch_=clampf(ecPitch_+dy*0.006f/s,0.05f,1.5f);}lastX_=px;lastY_=py;}
  }else{
    int sl=slot(id,false);if(sl>=0)ptId_[sl]=-1;
    if(id==dragId_){dragId_=-1;dragAxis_=-1;return;}
    if(id==hierId_){hierId_=-1;if(!moved_){rebuildRows();int row=(int)((y-(46+30))/22+hierScroll_);float rx=x-6;if(row>=0&&row<(int)rows_.size()){int eid=rows_[row].first;int d=rows_[row].second;
          if(rx<18+d*12+12&&!scene_.childrenOf(eid).empty()&&rx>d*12){if(expanded_.count(eid))expanded_.erase(eid);else expanded_.insert(eid);rebuildRows();}
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
  else if(id>=400&&id<408){int prim=id-400;if(prim!=P_TERRAIN){Entity& n=scene_.add(prim,std::string(PRIM_NAME[prim]));n.pos=ecTarget_;n.pos.y=scene_.height(n.pos.x,n.pos.z)+(prim==P_BOX?0.5f:prim==P_SPHERE?0.5f:0.0f);n.color=Col::hex(0xcfc6b4);select(n.id);rebuildRows();addMenu_=false;}}
  else if(id==119){/* reserved */}
}

void App::drawEditor(){float W=W_/ui.s();float H=H_/ui.s();A_=1;oy_=0;
  Vec3 cam,tg;
  if(play_){const Entity* h=nullptr;for(auto&kv:scripts_){const Entity* en=scene_.get(kv.first);if(en&&en->script=="PlayerController"){h=en;break;}}if(!h)h=scene_.find("Hero");
    Vec3 p=h?scene_.worldPos(h->id):Vec3{0,2,0};tg={p.x,p.y+1.4f,p.z};cam=tg+Vec3{std::sin(pYaw_)*std::cos(pPitch_)*7,std::sin(pPitch_)*7,std::cos(pYaw_)*std::cos(pPitch_)*7};
    float gh=scene_.height(cam.x,cam.z)+0.6f;if(cam.y<gh)cam.y=gh;}
  else{tg=ecTarget_;cam=tg+Vec3{std::sin(ecYaw_)*std::cos(ecPitch_)*ecDist_,std::sin(ecPitch_)*ecDist_,std::cos(ecYaw_)*std::cos(ecPitch_)*ecDist_};
    float gh=scene_.height(cam.x,cam.z)+1.0f;if(cam.y<gh)cam.y=gh;}
  renderScene(scene_,cam,tg,0.95f);gizmoUpdate();ui.begin(W_,H_);float S=ui.s();
  if(play_){
    R(0,0,W,0,0,BG1,BG1);
    fbtn(118,12,10,92,28,"Stop",2);R(26,19,10,10,2,WHITE,WHITE);
    ui.rrect(W-96,10,84,26,4,BG1.with(0.85f),BG1.with(0.85f));ui.ring(W-96,10,84,26,4,1,LINE2);ui.text(std::to_string((int)(fps_+0.5f))+" fps",W-54,16,11,TXT,F_BOLD,1);
    float jx=96,jy=H-92,kx=stickX_*30,ky=stickY_*30;ui.circle(jx,jy,50,WHITE.with(0.06f),WHITE.with(0.06f));ui.ring(jx-50,jy-50,100,100,50,1.2f,WHITE.with(0.28f));ui.circle(jx+kx,jy+ky,22,WHITE.with(0.35f),WHITE.with(0.25f));ui.ring(jx+kx-22,jy+ky-22,44,44,22,1,WHITE.with(0.5f));
    return;}
  // ---- gizmo
  if(gOk_){float ox=gO_[0]/S,oy=gO_[1]/S;for(int a=0;a<3;a++){float tx=gT_[a][0]/S,ty=gT_[a][1]/S;Col c=Col::hex(AXC[a]);float dx=tx-ox,dy=ty-oy,l=std::sqrt(dx*dx+dy*dy);int n=std::max(2,(int)(l/2));
      for(int i=1;i<n;i++)ui.circle(ox+dx*i/n,oy+dy*i/n,1.1f,c,c);
      bool act=dragAxis_==a;float rr=act?8:6;
      if(mode_==2)ui.rrect(tx-rr,ty-rr,rr*2,rr*2,2,c,c);else if(mode_==1){ui.circle(tx,ty,rr,c.with(0.2f),c.with(0.2f));ui.ring(tx-rr,ty-rr,rr*2,rr*2,rr,2,c);}else ui.circle(tx,ty,rr,c,c);
      ui.ring(tx-rr,ty-rr,rr*2,rr*2,mode_==2?2:rr,1,WHITE.with(act?1.0f:0.6f));}
    ui.ring(ox-9,oy-9,18,18,9,1.5f,ORG);ui.circle(ox,oy,2.5f,ORG,ORG);}
  // ---- top bar
  ui.rrect(0,0,W,40,0,BG1.with(0.96f),BG1.with(0.96f));ui.rrect(0,39,W,1,0,LINE,LINE);
  fbtn(5,8,6,28,28,"",1);T("<",22,8,17,TXT,F_BOLD,1);
  { std::string nm=cur_>=0&&cur_<(int)projects_.size()?projects_[cur_].name:"Project";if(nm.size()>12)nm=nm.substr(0,11)+"..";T(nm,46,5,12,TXT,F_BOLD);T("main  /  Scene",46,22,9.5f,TXT3);}
  ui.rrect(142,7,1,26,0,LINE,LINE);
  const char* ml[3]={"Move","Rotate","Scale"};for(int i=0;i<3;i++)fbtn(110+i,152+i*60,6,56,28,ml[i],1,mode_==i);
  ui.rrect(340,7,1,26,0,LINE,LINE);
  fbtn(113,350,6,60,28,"+ Add",1);fbtn(115,414,6,48,28,"Copy",1);fbtn(114,466,6,56,28,"Delete",2);fbtn(116,526,6,52,28,"Save",1);
  fbtn(117,W-92,6,82,28,"Play",1);ui.rrect(W-92,6,82,28,4,OK,OK2);ui.ring(W-92,6,82,28,4,1,OK.mul(1.3f));
  ui.rrect(W-79,14,0,0,0,WHITE,WHITE);{float tx=W-76,ty=20;ui.rrect(tx,ty-2,2,12,1,WHITE,WHITE);ui.rrect(tx+3,ty,2,8,1,WHITE,WHITE);ui.rrect(tx+6,ty+2,2,4,1,WHITE,WHITE);}
  ui.text("Play",W-38,13,12,WHITE,F_BOLD,1);
  // ---- viewport chips + status bar
  { float cy=H_/S;(void)cy;ui.rrect(194,46,84,20,3,BG1.with(0.8f),BG1.with(0.8f));ui.ring(194,46,84,20,3,1,LINE);ui.text("Perspective",236,51,10,TXT2,F_BOLD,1);
    ui.rrect(282,46,40,20,3,BG1.with(0.8f),BG1.with(0.8f));ui.ring(282,46,40,20,3,1,LINE);ui.text("Lit",302,51,10,TXT2,F_BOLD,1);}
  ui.rrect(0,H-22,W,22,0,BG1.with(0.97f),BG1.with(0.97f));ui.rrect(0,H-22,W,1,0,LINE,LINE);
  { const Entity* se=scene_.get(sel_);std::string st=std::to_string(scene_.ents.size())+" objects";if(se)st+="   |   Selected: "+se->name;st+=std::string("   |   Mode: ")+(mode_==0?"Move":mode_==1?"Rotate":"Scale");ui.text(st,12,H-17,10,TXT3);
    ui.text("Drag: orbit   Pinch: zoom   Tap: select",W-12,H-17,10,TXT3,F_REG,2);}
  // ---- outliner
  rebuildRows();{float x=6,y=46,w=178,h=H-46-28;sect(0);A_=1;oy_=0;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.95f),BG1.with(0.95f));ui.ring(x,y,w,h,5,1,LINE);
    ui.rrect(x+1,y+1,w-2,25,4,BG2,BG2);ui.rrect(x+1,y+14,w-2,12,0,BG2,BG2);ui.rrect(x+1,y+26,w-2,1,0,LINE,LINE);ui.text("OUTLINER",x+12,y+8,10.5f,TXT2,F_BOLD);ui.text(std::to_string(scene_.ents.size()),x+w-12,y+8,10.5f,TXT3,F_BOLD,2);
    int vis=(int)((h-34)/22);int maxScroll=std::max(0,(int)rows_.size()-vis);hierScroll_=std::min(hierScroll_,(float)maxScroll);int first=(int)hierScroll_;
    for(int i=0;i<vis&&first+i<(int)rows_.size();i++){int eid=rows_[first+i].first,dep=rows_[first+i].second;const Entity* e=scene_.get(eid);if(!e)continue;float ry=y+30+i*22;bool on=eid==sel_;
      if(on){ui.rrect(x+3,ry,w-6,21,3,ACC.with(0.30f),ACC.with(0.24f));ui.rrect(x+3,ry,2,21,0,ACC,ACC);}else if(i%2)ui.rrect(x+3,ry,w-6,21,0,WHITE.with(0.025f),WHITE.with(0.025f));
      float ix=x+10+dep*12;bool hasKids=!scene_.childrenOf(eid).empty();if(hasKids)ui.text(expanded_.count(eid)?"v":">",ix,ry+4,10,TXT2,F_BOLD);
      Col dot=e->prim==P_EMPTY?TXT3:e->prim==P_TERRAIN?Col::hex(0x6b8f5a):e->color;
      if(e->prim==P_EMPTY)ui.ring(ix+12,ry+6,8,8,2,1.2f,dot);else ui.rrect(ix+12,ry+6,8,8,2,dot,dot);
      std::string nm=e->name;if(nm.size()>15)nm=nm.substr(0,14)+"..";ui.text(nm,ix+26,ry+4,11,on?TXT:TXT2,on?F_BOLD:F_REG);}
    if(maxScroll>0){float th=(h-36)*vis/(float)rows_.size();float ty=y+32+(h-36-th)*(hierScroll_/maxScroll);ui.rrect(x+w-5,ty,3,th,1.5f,LINE2,LINE2);}}
  // ---- details
  if(const Entity* e=scene_.get(sel_)){float x=W-206,y=46,w=200,h=H-46-28;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.95f),BG1.with(0.95f));ui.ring(x,y,w,h,5,1,LINE);
    ui.rrect(x+1,y+1,w-2,25,4,BG2,BG2);ui.rrect(x+1,y+14,w-2,12,0,BG2,BG2);ui.rrect(x+1,y+26,w-2,1,0,LINE,LINE);ui.text("DETAILS",x+12,y+8,10.5f,TXT2,F_BOLD);
    std::string nm=e->name;if(nm.size()>18)nm=nm.substr(0,17)+"..";ui.text(nm,x+12,y+34,13,TXT,F_BOLD);ui.text(std::string(PRIM_NAME[e->prim])+"   #"+std::to_string(e->id),x+12,y+51,10,TXT3);
    ui.text("Color",x+12,y+71,10,TXT2,F_BOLD);for(int i=0;i<8;i++){float cx=x+56+i*17,cy=y+68;btn(150+i,cx,cy,15,15);Col c=Col::hex(SWATCH[i]);ui.rrect(cx,cy,15,15,3,c,c);bool same=std::fabs(e->color.r-c.r)<0.01f&&std::fabs(e->color.g-c.g)<0.01f&&std::fabs(e->color.b-c.b)<0.01f;ui.ring(cx-1.5f,cy-1.5f,18,18,4,same?1.5f:1,same?ACC:LINE);}
    ui.rrect(x+1,y+92,w-2,18,0,BG2,BG2);ui.text("TRANSFORM",x+12,y+96,9.5f,TXT3,F_BOLD);
    if(e->prim!=P_TERRAIN){const char* gl[3]={"Location","Rotation","Scale"};const Vec3* vs[3]={&e->pos,&e->rot,&e->scale};
      for(int g=0;g<3;g++){float gy=y+116+g*55;ui.text(gl[g],x+12,gy+16,10.5f,TXT2,F_BOLD);
        for(int a=0;a<3;a++){float ry=gy+a*18;float v=a==0?vs[g]->x:a==1?vs[g]->y:vs[g]->z;char b[24];std::snprintf(b,sizeof b,g==1?"%.0f":"%.2f",v);
          Col ac=Col::hex(AXC[a]);int idx=g*3+a;float mx=x+74,mw=w-84;ui.rrect(mx,ry,mw,16,3,BG3,BG3);ui.ring(mx,ry,mw,16,3,1,LINE);ui.rrect(mx,ry,3,16,0,ac,ac);
          btn(300+idx*2,mx+3,ry,26,16);btn(301+idx*2,mx+mw-26,ry,26,16);ui.text("-",mx+16,ry-1,13,pressOf(300+idx*2)>0.1f?ACC:TXT2,F_BOLD,1);ui.text("+",mx+mw-13,ry-1,13,pressOf(301+idx*2)>0.1f?ACC:TXT2,F_BOLD,1);ui.text(b,mx+mw/2+1,ry+2,10.5f,TXT,F_BOLD,1);}}
      float sy=y+h-32;ui.text("Script",x+12,sy+5,10.5f,TXT2,F_BOLD);btn(160,x+62,sy,w-72,22);ui.rrect(x+62,sy,w-72,22,3,BG3,BG3);ui.ring(x+62,sy,w-72,22,3,1,e->script.empty()?LINE:ACC);
      ui.text(e->script.empty()?"None (tap)":e->script,x+62+(w-72)/2,sy+5,10.5f,e->script.empty()?TXT3:TXT,F_BOLD,1);}}
  else{ui.text("Select an object to see its details",W-110,H-44,10,TXT3,F_REG,1);}
  // ---- add menu
  if(addMenu_){float x=346,y=42,w=170,h=30+7*26+6;ui.shadow(x,y+2,w,h,5,14,Col::hex(0,0.5f));ui.rrect(x,y,w,h,5,BG1.with(0.98f),BG1.with(0.98f));ui.ring(x,y,w,h,5,1,LINE2);
    ui.rrect(x+1,y+1,w-2,24,4,BG2,BG2);ui.rrect(x+1,y+12,w-2,12,0,BG2,BG2);ui.text("ADD OBJECT",x+12,y+7,10,TXT2,F_BOLD);
    int order[7]={P_EMPTY,P_BOX,P_SPHERE,P_CYL,P_CONE,P_PYRAMID,P_PLANE};for(int i=0;i<7;i++){float ry=y+30+i*26;btn(400+order[i],x+4,ry,w-8,24);float k=pressOf(400+order[i]);if(k>0.1f)ui.rrect(x+4,ry,w-8,24,3,ACC.with(0.35f),ACC.with(0.35f));
      ui.rrect(x+14,ry+8,8,8,order[i]==P_SPHERE?4:2,TXT3,TXT3);ui.text(PRIM_NAME[order[i]],x+32,ry+5,11.5f,TXT);}}
}
}
