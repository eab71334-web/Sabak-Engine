// Sabak Core - in-app scene editor: hierarchy, inspector, gizmo, add\/delete\/duplicate, save, and a Play mode that runs C++ scripts.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "app.h"
#include "theme.h"
#include "icons.h"
namespace sk {
static const Col WHITE=Col::hex(0xffffff);
using namespace sk::th;
static const unsigned AXC[3]={0xe5534b,0x5fb35f,0x4b92e6};
static const unsigned SWATCH[8]={0xcfc6b4,0x7a2f2f,0xd0a07c,0x3f6b3a,0x5fb8cc,0x4b6fa8,0xc9a23a,0x8c7fb0};
static void squishE(float p,float&x,float&y,float&w,float&h){float k=1-0.05f*p;x+=w*(1-k)/2;y+=h*(1-k)/2;w*=k;h*=k;}

int App::meshFor(int prim){int& m=meshIds_[prim];if(m>=0)return m;Vec3 w{1,1,1};MeshData d;
  switch(prim){case P_BOX:d=makeBox(1,1,1,w);break;case P_SPHERE:d=makeSphere(0.5f,20,14,w);break;case P_CYL:d=makeCylinder(0.5f,0.5f,1,20,w);break;case P_CONE:d=makeCone(0.5f,1,16,w);break;
    case P_PLANE:d=makeBox(1,0.02f,1,w);break;case P_PYRAMID:d=makeCylinder(0.7071f,0.01f,1,4,w);break;
    case P_TERRAIN:d=makeTerrain(260,200,[&](float x,float z){return scene_.height(x,z);},[&](float x,float y,float z){
      float n=scene_.noise(x*0.07f+11,z*0.07f+5),n2=scene_.noise(x*0.31f,z*0.31f);float slope=std::fabs(scene_.height(x+1.6f,z)-y)+std::fabs(scene_.height(x,z+1.6f)-y);
      Vec3 grassA{0.24f,0.40f,0.16f},grassB{0.36f,0.52f,0.20f},dirt{0.52f,0.45f,0.33f},sand{0.64f,0.59f,0.46f},rock{0.46f,0.45f,0.43f};
      Vec3 g=grassA*(1-n)+grassB*n;g=g*(0.9f+0.2f*n2);float d=std::sqrt(x*x+z*z);float flat=clampf(1-(d-14)/16,0,1);g=g*(1-flat*0.35f)+dirt*(flat*0.35f);
      if(y<-3.0f)g=sand;float rk=clampf((slope-1.5f)/2.0f,0,1);if(y>9)rk=std::max(rk,clampf((y-9)/3,0,1));return g*(1-rk)+rock*rk;});break;
    default:d=makeBox(0.01f,0.01f,0.01f,w);}
  m=r_->upload(d);return m;}

Model* App::modelFor(const std::string& name){auto it=models_.find(name);if(it!=models_.end())return it->second.get();
  float h=name=="horse"?2.1f:1.8f;std::string path=name.rfind("models/",0)==0&&cur_>=0&&cur_<(int)projects_.size()?projects_[cur_].dir+"/"+name:g_assetDir+"/"+name+".glb";auto m=std::make_unique<Model>();bool okLoad;if(name.rfind("gen:car",0)==0){okLoad=buildCar(*r_,name.size()>8?name.substr(8):"",*m);}else if(name.rfind("gen:horse",0)==0){okLoad=buildHorse(*r_,name.size()>10?name.substr(10):"",*m);}else if(name.rfind("sculpt:",0)==0){okLoad=scModel(*m);}else if(name.rfind("char:",0)==0){CharDef cd;cd.name=name.substr(5);if(cur_>=0&&cur_<(int)projects_.size())cd.load(projects_[cur_].dir+"/characters/"+cd.name+".char");okLoad=buildCharacter(*r_,cd,*m);}else okLoad=loadModel(*r_,path,h,*m);if(!okLoad)m.reset();else{if(m->autoRig&&name!="hero"){Model* hero=modelFor("hero");int mp=0;if(hero)m->retargetFrom(*hero,mp,true);}if(cur_>=0&&cur_<(int)projects_.size()){m->loadClips(projects_[cur_].dir+"/anims",name);m->loadSM(projects_[cur_].dir+"/anims",name);}}Model* p=m.get();models_[name]=std::move(m);return p;}

void App::renderScene(const Scene& sc,Vec3 cam,Vec3 target,float fov){std::vector<DrawItem> items;items.reserve(sc.ents.size());std::vector<SkinDraw> skin;bool studio=false;for(auto& e:sc.ents)if(e.name=="StudioFloor")studio=true;r_->setStudio(studio);
  for(auto& e:sc.ents){if(e.prim==P_MODEL){if(((animEd_&&e.id!=sel_)||(crEd_&&e.asset.rfind("char:",0)!=0)||(scEd_&&e.asset.rfind("sculpt:",0)!=0))&&&sc==&scene_)continue;Model* m=modelFor(e.asset);if(!m)continue;ModelInst& mi=minst_[{&sc,e.id}];if(mi.m!=m){mi=ModelInst();mi.m=m;}
      mi.param=e.animParam;mi.play(e.anim.empty()?"idle":e.anim);if(mi.a<0)mi.play("idle");mi.evaluate();Mat4 w=sc.world(e.id)*Mat4::scale({m->fit,m->fit,m->fit})*Mat4::translate({0,-m->foot,0});
      for(auto& mp:m->prims){SkinDraw sd;sd.mesh=mp.mesh;sd.model=w;const ModelMat& mm=m->mats[std::min<size_t>(mp.mat,m->mats.size()-1)];sd.tex=mm.tex;for(int k=0;k<4;k++)sd.col[k]=mm.col[k];sd.mask=mm.mask;sd.kind=mm.kind;sd.tint={e.color.r,e.color.g,e.color.b};sd.joints=mi.joints.data();sd.nj=(int)m->joints.size();skin.push_back(sd);}
      continue;}
    if(e.prim==P_EMPTY)continue;DrawItem it;it.mesh=meshFor(e.prim);it.model=sc.world(e.id);it.tint={e.color.r,e.color.g,e.color.b};it.castShadow=e.prim!=P_TERRAIN&&e.prim!=P_PLANE;items.push_back(it);}
  r_->setCamera(cam,target,fov);r_->setSun({0.55f,0.52f,0.42f},{target.x,target.y,target.z},55);r_->render(items,skin);}

void App::saveScene(bool quiet){if(cur_<0||cur_>=(int)projects_.size())return;bool ok=scene_.save(projects_[cur_].dir+"/scenes/main.scene");for(auto& kv:models_)if(kv.second&&!kv.second->clips.empty()){kv.second->saveClips(projects_[cur_].dir+"/anims",kv.first);kv.second->saveSM(projects_[cur_].dir+"/anims",kv.first);}thumbDirty_=true;if(!quiet)toast(ok?"Scene saved":"Save failed");}
void App::select(int id){sel_=id;}
void App::rebuildRows(){rows_.clear();std::vector<int> roots;for(auto&e:scene_.ents)if(e.parent<0||!scene_.get(e.parent))roots.push_back(e.id);
  std::function<void(int,int)> walk=[&](int id,int d){rows_.push_back({id,d});if(expanded_.count(id))for(int c:scene_.childrenOf(id))walk(c,d+1);};for(int r:roots)walk(r,0);}

void App::startPlay(){if(play_)return;scene_.buildSolids();snap_=scene_;play_=true;addMenu_=false;scripts_.clear();playT_=0;sctx_.time=0;in_=Input();
  for(auto& e:scene_.ents){if(e.script.empty())continue;auto it=scriptRegistry().find(e.script);if(it==scriptRegistry().end())continue;Script* s=it->second();s->self=&e;s->ctx=&sctx_;scripts_[e.id].reset(s);}
  // scripts hold pointers into the entity vector, so start them only after it is final
  for(auto& kv:scripts_){kv.second->self=scene_.get(kv.first);kv.second->onStart();}
  pYaw_=0;pPitch_=0.3f;stickId_=lookId_=-1;}
void App::stopPlay(){if(!play_)return;scripts_.clear();scene_=snap_;play_=false;in_=Input();rebuildRows();}

void App::editorUpdate(float dt){animUpdate(dt);crUpdate(dt);cbT_+=((cbOpen_?1.0f:0.0f)-cbT_)*std::min(1.0f,dt*12);
  if(play_){playT_+=dt;sctx_.time=playT_;float kx=(keyDown_[3]?1:0)-(keyDown_[1]?1:0),ky=(keyDown_[2]?1:0)-(keyDown_[0]?1:0);
    in_.moveX=std::max(-1.f,std::min(1.f,stickX_+kx));in_.moveY=std::max(-1.f,std::min(1.f,stickY_+ky));in_.camYaw=pYaw_;
    for(auto& kv:scripts_){kv.second->self=scene_.get(kv.first);if(kv.second->self)kv.second->onUpdate(dt);}}
  // hold-to-repeat on inspector steppers
  if((heldId_>=300&&heldId_<318)||(heldId_>=320&&heldId_<330)){repeatT_+=dt;if(repeatT_>0.45f){repeatT_-=0.07f;editorTap(heldId_);}}else repeatT_=0;}

bool App::nearPanel(float x,float y) const{float W=W_/ui.s();if(bhOpen_&&x>=196&&x<=W-246&&y>=44&&y<=330)return true;if(scEd_)return y<42||x<212||y>H_/ui.s()-24||((sc_.tool==9||sc_.tool==10)&&x>W_/ui.s()-132&&y<270);if(crEd_)return y<42||x<246||x>W-172||y>H_/ui.s()-24;if(animEd_){if(aeCam_)return y<36||y>H_/ui.s()-36;return y<42||x>W-240||(x<244&&y<218)||y>H_/ui.s()-182||(aeTab_>0&&x>250&&x<W-244&&y<212);}float ph=24+108*cbT_;return y<42||x<190||y>H_/ui.s()-24||(sel_>=0&&x>W-210)||y>=H_/ui.s()-22-ph;}

void App::gizmoUpdate(){gOk_=false;const Entity* e=scene_.get(sel_);if(!e||e->prim==P_TERRAIN||play_||animEd_)return;Vec3 o=scene_.worldPos(sel_);Vec3 c=r_->cameraPos();gL_=len(o-c)*0.14f;
  float sx,sy;if(!r_->project(o,sx,sy))return;gO_[0]=sx;gO_[1]=sy;Vec3 ax[3]={{1,0,0},{0,1,0},{0,0,1}};
  for(int a=0;a<3;a++){float tx,ty;if(!r_->project(o+ax[a]*gL_,tx,ty))return;gT_[a][0]=tx;gT_[a][1]=ty;}gOk_=true;}

bool App::pickAt(float px,float py){Vec3 o,d;r_->ray(px,py,o,d);int best=-1;float bt=1e9f;
  for(auto& e:scene_.ents){if(e.prim==P_TERRAIN)continue;Mat4 m=scene_.world(e.id);Vec3 c{m.m[12],m.m[13],m.m[14]};float sx=len({m.m[0],m.m[1],m.m[2]}),sy=len({m.m[4],m.m[5],m.m[6]}),sz=len({m.m[8],m.m[9],m.m[10]});
    float rad;if(e.prim==P_EMPTY){c=c+Vec3{0,1,0}*sy;rad=1.3f*std::max(sx,std::max(sy,sz));}
    else if(e.prim==P_MODEL){Model* mm=modelFor(e.asset);float h=mm?mm->height:1.8f,r=mm?mm->radius:1;c=c+Vec3{m.m[4],m.m[5],m.m[6]}*(h*0.5f);rad=std::max(h*0.55f,r*0.7f)*std::max(sx,std::max(sy,sz));}
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
  if(crEd_&&crPointer(type,id,px,py))return;
  if(scEd_&&scPointer(type,id,px,py))return;
  if(animEd_&&aePointer(type,id,px,py))return;
  if(type==0){
    if(addMenu_){addMenu_=false;}
    int sl=slot(id,true);if(sl>=0){ptX_[sl]=px;ptY_[sl]=py;}
    // gizmo handle?
    if(gOk_&&sel_>=0){for(int a=0;a<3;a++){float dx=px-gT_[a][0],dy=py-gT_[a][1];if(dx*dx+dy*dy<26*26*s*s){dragAxis_=a;dragId_=id;lastX_=px;lastY_=py;return;}}}
    if(!animEd_&&!crEd_&&!scEd_&&x>=6&&x<=184&&y>=46&&y<=H_/s-26){hierId_=id;lastY_=py;moved_=false;downX_=px;downY_=py;return;}
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
  else if(id==119)openAnimator();
  else if((id>=120&&id<150)||(id>=320&&id<330)||(id>=600&&id<660)||(id>=200&&id<300))animTap(id);
  else if(id>=700&&id<800)crTap(id);
  else if(id>=900&&id<940)scTap(id);
  else if(id>=800&&id<900)bhTap(id);
  else if(id==114){if(e&&e->prim!=P_TERRAIN){scene_.remove(sel_);sel_=-1;rebuildRows();toast("Deleted");}}
  else if(id==115){if(e&&e->prim!=P_TERRAIN){int n=scene_.duplicate(sel_);select(n);rebuildRows();toast("Duplicated");}}
  else if(id==116)saveScene();
  else if(id==117)startPlay();
  else if(id==118)stopPlay();
  else if(id>=150&&id<158){if(e)e->color=Col::hex(SWATCH[id-150]);}
  else if(id==161||id==162){if(e&&e->prim==P_MODEL){Model* m=modelFor(e->asset);if(m&&!m->anims.empty()){std::vector<std::string> nm;for(auto& a:m->anims)nm.push_back(a.name);if(!m->smStates.empty())nm.push_back("StateMachine");int n=(int)nm.size(),i=0;for(int k=0;k<n;k++)if(nm[k]==e->anim)i=k;i=(i+(id==162?1:n-1))%n;e->anim=nm[i];}}}
  else if(id==160){if(e){std::vector<std::string> names{""};for(auto&kv:scriptRegistry())names.push_back(kv.first);size_t i=0;for(;i<names.size();i++)if(names[i]==e->script)break;e->script=names[(i+1)%names.size()];}}
  else if(id>=300&&id<318){if(e){int k=id-300,idx=k/2,sign=(k%2)?1:-1,grp=idx/3,ax=idx%3;Vec3* v=grp==0?&e->pos:grp==1?&e->rot:&e->scale;float st=grp==0?0.5f:grp==1?5.0f:0.1f;
      float* f=ax==0?&v->x:ax==1?&v->y:&v->z;*f+=sign*st;if(grp==2&&*f<0.05f)*f=0.05f;}}
  else if(id>=400&&id<408){int prim=id-400;if(prim!=P_TERRAIN){Entity& n=scene_.add(prim,std::string(PRIM_NAME[prim]));n.pos=ecTarget_;n.pos.y=scene_.height(n.pos.x,n.pos.z)+(prim==P_BOX?0.5f:prim==P_SPHERE?0.5f:0.0f);n.color=Col::hex(0xcfc6b4);select(n.id);rebuildRows();addMenu_=false;}}
  else if(id>=450&&id<480)cbTap(id);
}

void App::refreshModels(){mdlList_={"hero","horse","gen:car","gen:horse"};namespace fs=std::filesystem;std::error_code ec;
  if(cur_>=0&&cur_<(int)projects_.size()){std::string d=projects_[cur_].dir+"/models";if(fs::is_directory(d,ec))for(auto& f:fs::directory_iterator(d,ec)){std::string x=f.path().extension().string();for(char& c:x)c=(char)tolower(c);if(x==".glb"||x==".gltf"||x==".fbx")mdlList_.push_back("models/"+f.path().filename().string());}}
  std::string d=data_+"/Models";if(fs::is_directory(d,ec))for(auto& f:fs::directory_iterator(d,ec)){std::string x=f.path().extension().string();for(char& c:x)c=(char)tolower(c);if(x==".glb"||x==".gltf"||x==".fbx")mdlList_.push_back("ext:"+f.path().filename().string());}}
void App::placeModel(const std::string& id){namespace fs=std::filesystem;std::error_code ec;std::string asset=id,name;
  if(id.rfind("ext:",0)==0){if(cur_<0||cur_>=(int)projects_.size())return;std::string fn=id.substr(4);std::string src=data_+"/Models/"+fn,dst=projects_[cur_].dir+"/models/"+fn;fs::create_directories(projects_[cur_].dir+"/models",ec);fs::copy_file(src,dst,fs::copy_options::overwrite_existing,ec);
    if(fn.size()>5&&(fn.substr(fn.size()-5)==".gltf")){std::ifstream f(src);std::string js((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());size_t p=0;while((p=js.find("\"uri\"",p))!=std::string::npos){p=js.find('"',js.find(':',p)+1);size_t q=js.find('"',p+1);if(q==std::string::npos)break;std::string u=js.substr(p+1,q-p-1);p=q;if(u.rfind("data:",0)==0||u.empty())continue;
        fs::create_directories(fs::path(projects_[cur_].dir+"/models/"+u).parent_path(),ec);fs::copy_file(data_+"/Models/"+u,projects_[cur_].dir+"/models/"+u,fs::copy_options::overwrite_existing,ec);}}
    asset="models/"+fn;}
  if(!modelFor(asset)){toast("Could not load model");return;}
  name=asset=="hero"?"Hero":asset=="horse"?"Horse":asset=="gen:car"?"Car (generated)":asset=="gen:horse"?"Horse (generated)":fs::path(asset).stem().string();
  Entity& n=scene_.add(P_MODEL,name);n.asset=asset;n.color=Col{1,1,1,1};n.pos=ecTarget_;n.pos.y=scene_.height(n.pos.x,n.pos.z);select(n.id);rebuildRows();toast("Added "+name);}
void App::cbTap(int id){
  if(id==450)cbOpen_=!cbOpen_;else if(id>=451&&id<=453){cbTab_=id-451;if(cbTab_==1)refreshModels();}
  else if(id>=460){int i=id-460;if(cbTab_==0){int order[7]={P_EMPTY,P_BOX,P_SPHERE,P_CYL,P_CONE,P_PYRAMID,P_PLANE};if(i<7)editorTap(400+order[i]);}
    else if(cbTab_==1){if(i<(int)mdlList_.size())placeModel(mdlList_[i]);}
    else{std::vector<std::string> names;for(auto&kv:scriptRegistry())names.push_back(kv.first);Entity* e=scene_.get(sel_);if(e&&i<(int)names.size()){e->script=names[i];toast("Attached "+names[i]);}else if(!e)toast("Select an object first");}}}

void App::drawContent(float x0,float x1){float H=H_/ui.s();float ph=24+108*cbT_,y=H-22-ph,w=x1-x0;
  ui.shadow(x0,y+2,w,ph,5,10,Col::hex(0,0.35f));ui.rrect(x0,y,w,ph,5,BG1.with(0.96f),BG1.with(0.96f));ui.ring(x0,y,w,ph,5,1,LINE);
  ui.rrect(x0+1,y+1,w-2,22,4,BG2,BG2);ui.rrect(x0+1,y+12,w-2,11,0,BG2,BG2);
  fbtn(450,x0+w-30,y+2,26,20,"",3);if(cbOpen_)ic::chevronD(ui,x0+w-17,y+12,12,TXT2);else ic::chevronU(ui,x0+w-17,y+12,12,TXT2);
  ui.text("CONTENT BROWSER",x0+10,y+7,10,TXT2,F_BOLD);
  const char* tn[3]={"Place","Models","C++ Scripts"};float tx=x0+128;for(int i=0;i<3;i++){float tw=i==0?54:i==1?60:92;btn(451+i,tx,y+2,tw,20);bool on=cbTab_==i&&cbOpen_;if(on)ui.rrect(tx,y+2,tw,20,3,ACC.with(0.35f),ACC.with(0.30f));ui.text(tn[i],tx+tw/2,y+6,10.5f,on?TXT:TXT3,F_BOLD,1);tx+=tw+4;}
  if(cbT_<0.6f)return;float a=std::min(1.0f,(cbT_-0.6f)*2.5f);
  if(cbTab_==0){int order[7]={P_EMPTY,P_BOX,P_SPHERE,P_CYL,P_CONE,P_PYRAMID,P_PLANE};
    for(int i=0;i<7;i++){float cx=x0+10+i*62,cy=y+30,cw=56,chh=70;btn(460+i,cx,cy,cw,chh);float k=pressOf(460+i);ui.rrect(cx,cy,cw,chh,4,BG3.with(a*(0.8f+0.2f*k)),BG2.with(a));ui.ring(cx,cy,cw,chh,4,1,k>0.05f?ACC.with(a):LINE.with(a));
      Col c=TXT2.with(a);float ix=cx+cw/2,iy=cy+27;switch(order[i]){case P_EMPTY:ic::empty(ui,ix,iy,30,c);break;case P_BOX:ic::cube(ui,ix,iy,30,c);break;case P_SPHERE:ic::sphere(ui,ix,iy,30,c);break;case P_CYL:ic::cylinder(ui,ix,iy,30,c);break;case P_CONE:case P_PYRAMID:ic::cone(ui,ix,iy,30,c);break;default:ic::plane(ui,ix,iy,30,c);}
      ui.text(PRIM_NAME[order[i]],ix,cy+52,9.5f,TXT.with(a),F_BOLD,1);}}
  else if(cbTab_==1){if(mdlList_.empty())refreshModels();int cnt=std::min<int>((int)mdlList_.size(),(int)((w-10)/76));
    for(int i=0;i<cnt;i++){const std::string& n=mdlList_[i];bool ext=n.rfind("ext:",0)==0;float cx=x0+10+i*76,cy=y+30,cw=70,chh=70;btn(460+i,cx,cy,cw,chh);float k=pressOf(460+i);
      ui.rrect(cx,cy,cw,chh,4,BG3.with(a*(0.8f+0.2f*k)),BG2.with(a));ui.ring(cx,cy,cw,chh,4,1,k>0.05f?ACC.with(a):LINE.with(a));
      auto th=mthumbs_.find(n);if(!ext&&th!=mthumbs_.end()&&th->second.fbo)ui.image(th->second.tex,cx+3,cy+3,cw-6,44,4,WHITE.with(a));else{ui.circle(cx+cw/2,cy+17,6,TXT2.with(a),TXT2.with(a));ui.rrect(cx+cw/2-9,cy+26,18,18,6,TXT2.with(a),TXT2.with(a));}
      std::string nm=n;size_t sl=nm.find_last_of("/:");if(sl!=std::string::npos)nm=nm.substr(sl+1);size_t dt=nm.find_last_of('.');if(dt!=std::string::npos)nm=nm.substr(0,dt);if(nm.size()>10)nm=nm.substr(0,9)+"..";if(nm.size()&&nm[0]>='a'&&nm[0]<='z')nm[0]-=32;
      ui.text(nm,cx+cw/2,cy+50,10,TXT.with(a),F_BOLD,1);ui.text(ext?"IMPORT":"MODEL",cx+cw/2,cy+60,7.5f,ext?ORG.with(a):TXT3.with(a),F_BOLD,1);}
    if(mdlList_.size()<=2)ui.text("Put .glb / .gltf files in Files > SabakEngine > Models",x0+170,y+60,10,TXT3.with(a));}
  else{int i=0;for(auto& kv:scriptRegistry()){float cx=x0+10+i*98,cy=y+30,cw=92,chh=70;btn(460+i,cx,cy,cw,chh);float k=pressOf(460+i);const Entity* se=scene_.get(sel_);bool on=se&&se->script==kv.first;
      ui.rrect(cx,cy,cw,chh,4,BG3.with(a*(0.8f+0.2f*k)),BG2.with(a));ui.ring(cx,cy,cw,chh,4,1,on?ACC.with(a):LINE.with(a));ic::code(ui,cx+cw/2,cy+24,26,(on?ACC:TXT2).with(a));
      std::string nm=kv.first;if(nm.size()>13)nm=nm.substr(0,12)+"..";ui.text(nm,cx+cw/2,cy+44,10,TXT.with(a),F_BOLD,1);ui.text("C++",cx+cw/2,cy+57,8.5f,TXT3.with(a),F_BOLD,1);i++;}
    if(scriptRegistry().empty())ui.text("No scripts. Add .cpp files to game/scripts/",x0+16,y+60,11,TXT3.with(a));}
}

void App::drawSelection(Vec3 cam,Vec3 tg){(void)cam;(void)tg;const Entity* se=scene_.get(sel_);if(!se||se->prim==P_TERRAIN)return;float S=ui.s();
  std::vector<int> q{sel_};float mnx=1e9f,mny=1e9f,mxx=-1e9f,mxy=-1e9f;bool any=false;
  for(size_t qi=0;qi<q.size();qi++){int id=q[qi];for(int c:scene_.childrenOf(id))q.push_back(c);const Entity* e=scene_.get(id);if(!e||e->prim==P_EMPTY||e->prim==P_TERRAIN)continue;Mat4 m=scene_.world(id);
    float y0=(e->prim==P_CYL||e->prim==P_CONE||e->prim==P_PYRAMID||e->prim==P_MODEL)?0.0f:-0.5f,y1=y0+1.0f;float bw=0.5f,bh=1.0f;if(e->prim==P_MODEL){Model* mm=modelFor(e->asset);if(mm){bw=mm->radius*0.7f;bh=mm->height;}y1=bh;}
    for(int k=0;k<8;k++){float lx=(k&1)?bw:-bw,ly=(k&2)?y1:y0,lz=(k&4)?bw:-bw;Vec3 p{m.m[0]*lx+m.m[4]*ly+m.m[8]*lz+m.m[12],m.m[1]*lx+m.m[5]*ly+m.m[9]*lz+m.m[13],m.m[2]*lx+m.m[6]*ly+m.m[10]*lz+m.m[14]};float sx,sy;
      if(!r_->project(p,sx,sy))continue;any=true;mnx=std::min(mnx,sx);mxx=std::max(mxx,sx);mny=std::min(mny,sy);mxy=std::max(mxy,sy);}}
  if(!any)return;float x0=mnx/S-4,y0=mny/S-4,x1=mxx/S+4,y1=mxy/S+4;float L=std::min(12.0f,std::min(x1-x0,y1-y0)*0.35f);
  ui.rrect(x0,y0,x1-x0,y1-y0,2,ORG.with(0.05f),ORG.with(0.05f));ui.ring(x0,y0,x1-x0,y1-y0,2,1,ORG.with(0.45f));
  float xs[2]={x0,x1},ys[2]={y0,y1};for(int a=0;a<2;a++)for(int b=0;b<2;b++){float cx=xs[a],cy=ys[b],dx=a?-L:L,dy=b?-L:L;ui.line(cx,cy,cx+dx,cy,2,ORG);ui.line(cx,cy,cx,cy+dy,2,ORG);}
  std::string nm=se->name;float tw=ui.measure(nm,10,F_BOLD)+14;float lx=std::max(190.0f,x0),ly=std::max(46.0f,y0-20);ui.rrect(lx,ly,tw,16,3,ORG,ORG.mul(0.85f));ui.text(nm,lx+tw/2,ly+3,10,Col::hex(0x1a1206),F_BOLD,1);}

void App::drawEditor(){float W=W_/ui.s();float H=H_/ui.s();A_=1;oy_=0;
  Vec3 cam,tg;
  if(play_){const Entity* h=nullptr;for(auto&kv:scripts_){const Entity* en=scene_.get(kv.first);if(en&&(en->script=="PlayerController"||(en->script=="Behavior"&&!en->blocks.empty()&&en->blocks[0].type==0))){h=en;break;}}if(!h)h=scene_.find("Hero");
    Vec3 p=h?scene_.worldPos(h->id):Vec3{0,2,0};tg={p.x,p.y+1.4f,p.z};cam=tg+Vec3{std::sin(pYaw_)*std::cos(pPitch_)*7,std::sin(pPitch_)*7,std::cos(pYaw_)*std::cos(pPitch_)*7};
    float gh=scene_.height(cam.x,cam.z)+0.6f;if(cam.y<gh)cam.y=gh;}
  else{tg=ecTarget_;cam=tg+Vec3{std::sin(ecYaw_)*std::cos(ecPitch_)*ecDist_,std::sin(ecPitch_)*ecDist_,std::cos(ecYaw_)*std::cos(ecPitch_)*ecDist_};
    float gh=scene_.height(cam.x,cam.z)+1.0f;if(cam.y<gh)cam.y=gh;}
  renderScene(scene_,cam,tg,0.95f);gizmoUpdate();ui.begin(W_,H_);float S=ui.s();
  if(crEd_){drawCreator(W,H);return;}
  if(scEd_){drawSculpt(W,H);return;}
  if(animEd_){drawAnimator(W,H);return;}
  if(play_){
    R(0,0,W,0,0,BG1,BG1);
    fbtn(118,12,10,92,28,"",2);ic::stop(ui,28,24,16,WHITE);ui.text("Stop",64,17,12,WHITE,F_BOLD,1);
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
  drawSelection(cam,tg);
  { Vec3 f=norm(tg-cam),sv=norm(cross(f,{0,1,0})),uv=cross(sv,f);float cx=214,cy=H-62;ui.circle(cx,cy,30,BG1.with(0.55f),BG1.with(0.55f));ui.ring(cx-30,cy-30,60,60,30,1,LINE.with(0.7f));
    Vec3 ax[3]={{1,0,0},{0,1,0},{0,0,1}};const char* an[3]={"X","Y","Z"};int ord[3]={0,1,2};std::sort(ord,ord+3,[&](int a,int b){return dot(ax[a],f)>dot(ax[b],f);});
    for(int k=0;k<3;k++){int a=ord[k];float dx=dot(ax[a],sv)*20,dy=-dot(ax[a],uv)*20;Col c=Col::hex(AXC[a]);ui.line(cx,cy,cx+dx,cy+dy,2,c);ui.circle(cx+dx,cy+dy,6,c,c);ui.text(an[a],cx+dx,cy+dy-5.5f,8.5f,WHITE,F_BOLD,1);}}
  drawContent(190,sel_>=0?W-212:W-8);
  // ---- top bar
  ui.rrect(0,0,W,40,0,BG1.with(0.96f),BG1.with(0.96f));ui.rrect(0,39,W,1,0,LINE,LINE);
  fbtn(5,8,6,28,28,"",1);ic::back(ui,22,20,16,TXT);
  { std::string nm=cur_>=0&&cur_<(int)projects_.size()?projects_[cur_].name:"Project";if(nm.size()>12)nm=nm.substr(0,11)+"..";T(nm,46,5,12,TXT,F_BOLD);T("main  /  Scene",46,22,9.5f,TXT3);}
  auto ib=[&](int id,float x,float w,int icon,const char* label,int kind,bool active){fbtn(id,x,6,w,28,"",kind,active);Col c=(active||kind==2)?WHITE:TXT;float cx=x+15,cy=20;
    switch(icon){case 0:ic::move(ui,cx,cy,15,c);break;case 1:ic::rotate(ui,cx,cy,15,c);break;case 2:ic::scale(ui,cx,cy,15,c);break;case 3:ic::plus(ui,cx,cy,14,c);break;case 4:ic::copy(ui,cx,cy,15,c);break;case 5:ic::trash(ui,cx,cy,15,c);break;case 6:ic::save(ui,cx,cy,15,c);break;default:ic::play(ui,cx,cy,16,c);}
    ui.text(label,x+28,13,11.5f,c,F_BOLD);};
  ib(110,150,68,0,"Move",1,mode_==0);ib(111,220,72,1,"Rotate",1,mode_==1);ib(112,294,66,2,"Scale",1,mode_==2);
  ui.rrect(364,7,1,26,0,LINE,LINE);
  fbtn(119,656,6,92,28,"",1);ui.text("Animate",702,13,11.5f,TXT,F_BOLD,1);
  ib(113,372,64,3,"Add",1,false);ib(115,440,66,4,"Copy",1,false);ib(114,510,72,5,"Delete",2,false);ib(116,586,64,6,"Save",1,false);
  fbtn(117,W-96,6,86,28,"",1);ui.rrect(W-96,6,86,28,4,OK,OK2);ui.ring(W-96,6,86,28,4,1,OK.mul(1.3f));ic::play(ui,W-80,20,16,WHITE);ui.text("Play",W-46,13,12,WHITE,F_BOLD,1);
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
    if(e->prim==P_MODEL){ui.text("Anim",x+12,y+71,10,TXT2,F_BOLD);float bx=x+52,bw=w-64;ui.rrect(bx,y+66,bw,20,3,BG3,BG3);ui.ring(bx,y+66,bw,20,3,1,LINE);btn(161,bx,y+66,22,20);btn(162,bx+bw-22,y+66,22,20);
      ui.text("<",bx+11,y+68,12,pressOf(161)>0.1f?ACC:TXT2,F_BOLD,1);ui.text(">",bx+bw-11,y+68,12,pressOf(162)>0.1f?ACC:TXT2,F_BOLD,1);ui.text(e->anim,bx+bw/2,y+71,10.5f,TXT,F_BOLD,1);}
    else{ui.text("Color",x+12,y+71,10,TXT2,F_BOLD);}for(int i=0;e->prim!=P_MODEL&&i<8;i++){float cx=x+56+i*17,cy=y+68;btn(150+i,cx,cy,15,15);Col c=Col::hex(SWATCH[i]);ui.rrect(cx,cy,15,15,3,c,c);bool same=std::fabs(e->color.r-c.r)<0.01f&&std::fabs(e->color.g-c.g)<0.01f&&std::fabs(e->color.b-c.b)<0.01f;ui.ring(cx-1.5f,cy-1.5f,18,18,4,same?1.5f:1,same?ACC:LINE);}
    ui.rrect(x+1,y+92,w-2,18,0,BG2,BG2);ui.text("TRANSFORM",x+12,y+96,9.5f,TXT3,F_BOLD);
    if(e->prim!=P_TERRAIN){const char* gl[3]={"Location","Rotation","Scale"};const Vec3* vs[3]={&e->pos,&e->rot,&e->scale};
      for(int g=0;g<3;g++){float gy=y+116+g*55;ui.text(gl[g],x+12,gy+16,10.5f,TXT2,F_BOLD);
        for(int a=0;a<3;a++){float ry=gy+a*18;float v=a==0?vs[g]->x:a==1?vs[g]->y:vs[g]->z;char b[24];std::snprintf(b,sizeof b,g==1?"%.0f":"%.2f",v);
          Col ac=Col::hex(AXC[a]);int idx=g*3+a;float mx=x+74,mw=w-84;ui.rrect(mx,ry,mw,16,3,BG3,BG3);ui.ring(mx,ry,mw,16,3,1,LINE);ui.rrect(mx,ry,3,16,0,ac,ac);
          btn(300+idx*2,mx+3,ry,26,16);btn(301+idx*2,mx+mw-26,ry,26,16);ui.text("-",mx+16,ry-1,13,pressOf(300+idx*2)>0.1f?ACC:TXT2,F_BOLD,1);ui.text("+",mx+mw-13,ry-1,13,pressOf(301+idx*2)>0.1f?ACC:TXT2,F_BOLD,1);ui.text(b,mx+mw/2+1,ry+2,10.5f,TXT,F_BOLD,1);}}
      float sy=y+h-32;float sw=e->script=="Behavior"?w-72-64:w-72;ui.text("Script",x+12,sy+5,10.5f,TXT2,F_BOLD);btn(160,x+62,sy,sw,22);ui.rrect(x+62,sy,sw,22,3,BG3,BG3);ui.ring(x+62,sy,sw,22,3,1,e->script.empty()?LINE:ACC);
      ui.text(e->script.empty()?"None (tap)":e->script,x+62+sw/2,sy+5,10.5f,e->script.empty()?TXT3:TXT,F_BOLD,1);if(e->script=="Behavior"){drawToolBtn(806,x+62+sw+4,sy,56,22,"Blocks",bhOpen_);}}}
  else{ui.text("Select an object to see its details",W-110,H-44,10,TXT3,F_REG,1);}
  // ---- add menu
  drawBehavior(W,H);
  if(addMenu_){float x=346,y=42,w=170,h=30+7*26+6;ui.shadow(x,y+2,w,h,5,14,Col::hex(0,0.5f));ui.rrect(x,y,w,h,5,BG1.with(0.98f),BG1.with(0.98f));ui.ring(x,y,w,h,5,1,LINE2);
    ui.rrect(x+1,y+1,w-2,24,4,BG2,BG2);ui.rrect(x+1,y+12,w-2,12,0,BG2,BG2);ui.text("ADD OBJECT",x+12,y+7,10,TXT2,F_BOLD);
    int order[7]={P_EMPTY,P_BOX,P_SPHERE,P_CYL,P_CONE,P_PYRAMID,P_PLANE};for(int i=0;i<7;i++){float ry=y+30+i*26;btn(400+order[i],x+4,ry,w-8,24);float k=pressOf(400+order[i]);if(k>0.1f)ui.rrect(x+4,ry,w-8,24,3,ACC.with(0.35f),ACC.with(0.35f));
      ui.rrect(x+14,ry+8,8,8,order[i]==P_SPHERE?4:2,TXT3,TXT3);ui.text(PRIM_NAME[order[i]],x+32,ry+5,11.5f,TXT);}}
}
}
