// Sabak Core - Model Studio screen: Blender-style polygon modelling on touch (select, extrude, inset, loop cut, crease, mirror, subdivision).
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include "app.h"
#include "theme.h"
#include "stb_image.h"
namespace sk {
using namespace sk::th;
namespace fs=std::filesystem;
static std::string pdir(const std::string& proj){return proj+"/pmodels";}
Entity* App::pmEntity(){for(auto& e:scene_.ents)if(e.prim==P_MODEL&&e.asset.rfind("pmodel:",0)==0)return &e;return nullptr;}

void App::pmArrays(std::vector<SkinVertex>& v,std::vector<uint32_t>& idx,bool cutter,int mat) const{
  const PolyMesh& P=(pmObj_==0)!=cutter?pm_:pmOther_;PolyData m=P.evaluate(P.subLevels);v.clear();idx.clear();
  auto matOf=[&](size_t f){return f<m.M.size()?(int)m.M[f]:0;};
  auto push=[&](Vec3 p,Vec3 n){SkinVertex s{};s.p[0]=p.x;s.p[1]=p.y;s.p[2]=p.z;s.n[0]=n.x;s.n[1]=n.y;s.n[2]=n.z;s.uv[0]=p.z*0.5f+p.x*0.3f;s.uv[1]=p.y*0.5f+p.x*0.2f;s.j[0]=0;s.w[0]=1;v.push_back(s);};
  std::vector<Vec3> fnr;std::vector<std::vector<int>> vfa;bool smooth=P.rig&&!cutter&&P.subLevels==0;
  if(smooth){fnr.resize(m.F.size());vfa.resize(m.V.size());for(size_t fi=0;fi<m.F.size();fi++){auto& f=m.F[fi];Vec3 n{0,0,0};for(size_t i=0;i<f.size();i++){Vec3 a=m.V[f[i]],b=m.V[f[(i+1)%f.size()]];n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);}fnr[fi]=n;for(int k:f)vfa[k].push_back((int)fi);}}
  if(P.subLevels==0){for(size_t fi=0;fi<m.F.size();fi++){auto& f=m.F[fi];if(mat>=0&&matOf(fi)!=mat)continue;Vec3 n{0,0,0};for(size_t i=0;i<f.size();i++){Vec3 a=m.V[f[i]],b=m.V[f[(i+1)%f.size()]];n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);}n=norm(n);
      uint32_t b0=(uint32_t)v.size();for(int k:f){Vec3 nn=n;if(smooth){Vec3 s{0,0,0};for(int g:vfa[k])if(dot(norm(fnr[g]),n)>0.82f)s=s+fnr[g];if(dot(s,s)>1e-12f)nn=norm(s);}push(m.V[k],nn);}if(P.rig&&!cutter){Vec3 cc{0,0,0};for(int k:f)cc=cc+m.V[k];cc=cc*(1.f/f.size());int jt=carPartAt(cc,n,matOf(fi));if(jt)for(size_t q=b0;q<v.size();q++)v[q].j[0]=(uint8_t)jt;}for(size_t i=1;i+1<f.size();i++){idx.push_back(b0);idx.push_back(b0+(uint32_t)i);idx.push_back(b0+(uint32_t)i+1);}}}
  else{std::vector<Vec3> PP,N;std::vector<uint32_t> all;PolyMesh::triangulate(m,PP,N,all);
    if(mat<0){idx=all;}else{size_t t=0;for(size_t fi=0;fi<m.F.size();fi++){size_t nt=m.F[fi].size()-2;if(matOf(fi)==mat)for(size_t k=0;k<nt*3;k++)idx.push_back(all[t+k]);t+=nt*3;}}
    for(size_t i=0;i<PP.size();i++)push(PP[i],N[i]);}}

static const struct{int kind;float c[4];} PMM[8]={{5,{0.16f,0.29f,0.55f,1}},{6,{0.05f,0.07f,0.09f,1}},{7,{0.55f,0.56f,0.6f,1}},{9,{0.04f,0.04f,0.045f,1}},{9,{0.02f,0.02f,0.02f,1}},{12,{1.f,0.97f,0.85f,1}},{12,{1.f,0.08f,0.05f,1}},{11,{0.30f,0.16f,0.09f,1}}};
bool App::pmModel(Model& m){
  if(pm_.d.F.empty()){std::string dir=cur_>=0&&cur_<(int)projects_.size()?projects_[cur_].dir:"";if(dir.empty()||!pm_.load(pdir(dir)+"/M1.ply"))pmPrim(0);}
  m=Model();m.name="PolyModel";m.nodeName={"root"};m.parent={-1};m.rest.assign(1,TRS());m.joints={0};m.ibm={Mat4::identity()};
  for(int k=0;k<8;k++){ModelMat mm;mm.kind=PMM[k].kind;for(int c=0;c<4;c++)mm.col[c]=PMM[k].c[c];if(r_){unsigned char wh[4]={255,255,255,255};mm.tex=r_->createTexture(wh,1,1);}m.mats.push_back(mm);}
  if(pm_.rig&&pm_.subLevels==0){const char* jn[8]={"wheel.FL","wheel.FR","wheel.RL","wheel.RR","door.L","door.R","hood","trunk"};Vec3 jp[8]={{0.8f,0.355f,1.48f},{-0.8f,0.355f,1.48f},{0.8f,0.355f,-1.377f},{-0.8f,0.355f,-1.377f},{0.93f,0.7f,0.66f},{-0.93f,0.7f,0.66f},{0.f,1.0f,1.10f},{0.f,1.0f,-1.55f}};
    for(int i=0;i<8;i++){m.nodeName.push_back(jn[i]);m.parent.push_back(0);TRS t;t.t=jp[i];m.rest.push_back(t);m.joints.push_back(i+1);m.ibm.push_back(Mat4::translate(jp[i]*-1.f));}
    Anim sp;sp.name="wheels";sp.dur=0.6f;for(int i=0;i<4;i++){AnimChannel c;c.node=i+1;c.path=1;float sg=1.f;c.t={0,0.15f,0.3f,0.45f,0.6f};for(int q=0;q<5;q++){float a=sg*6.2831853f*q/4.f;c.v.insert(c.v.end(),{std::sin(a*0.5f),0.f,0.f,std::cos(a*0.5f)});}sp.ch.push_back(c);}m.anims.push_back(sp);
    Anim dr;dr.name="doors_open";dr.dur=1.f;for(int i=0;i<2;i++){AnimChannel c;c.node=5+i;c.path=1;float sg=i?0.9f:-0.9f;c.t={0,0.8f};float a=sg;c.v={0,0,0,1,0,std::sin(a*0.5f),0,std::cos(a*0.5f)};dr.ch.push_back(c);}m.anims.push_back(dr);
    Anim dc;dc.name="doors_close";dc.dur=1.f;for(int i=0;i<2;i++){AnimChannel c;c.node=5+i;c.path=1;float sg=i?0.9f:-0.9f;c.t={0,0.8f};float a=sg;c.v={0,std::sin(a*0.5f),0,std::cos(a*0.5f),0,0,0,1};dc.ch.push_back(c);}m.anims.push_back(dc);
    {Anim a;a.name="hood_open";a.dur=1.f;AnimChannel c;c.node=7;c.path=1;c.t={0,0.8f};float an=-0.85f;c.v={0,0,0,1,std::sin(an*0.5f),0,0,std::cos(an*0.5f)};a.ch.push_back(c);m.anims.push_back(a);}
    {Anim a;a.name="trunk_open";a.dur=1.f;AnimChannel c;c.node=8;c.path=1;c.t={0,0.8f};float an=0.75f;c.v={0,0,0,1,std::sin(an*0.5f),0,0,std::cos(an*0.5f)};a.ch.push_back(c);m.anims.push_back(a);}
    m.builtin=(int)m.anims.size();}
  float top=0.5f;
  for(int k=0;k<8;k++){std::vector<SkinVertex> v;std::vector<uint32_t> idx;pmArrays(v,idx,false,k);if(idx.empty())continue;
    ModelPrim mp;mp.mat=k;mp.cv=v;mp.ci=idx;if(r_)mp.mesh=r_->uploadSkin(v,idx);m.prims.push_back(std::move(mp));for(auto& s:v)top=std::max(top,s.p[1]);}
  if(m.prims.empty()){ModelPrim mp;mp.mat=0;m.prims.push_back(std::move(mp));}
  m.fit=1;m.foot=0;m.height=top;m.radius=2;m.autoRig=false;return true;}

bool App::pmCutModel(Model& m){std::vector<SkinVertex> v;std::vector<uint32_t> idx;pmArrays(v,idx,true);
  m=Model();m.name="Cutter";m.nodeName={"root"};m.parent={-1};m.rest.assign(1,TRS());m.joints={0};m.ibm={Mat4::identity()};
  ModelMat mm;mm.kind=13;mm.col[0]=mm.col[1]=mm.col[2]=mm.col[3]=1;if(r_){unsigned char rd[4]={255,70,70,120};mm.tex=r_->createTexture(rd,1,1);}m.mats.push_back(mm);
  ModelPrim mp;mp.mat=0;mp.cv=v;mp.ci=idx;if(r_&&!idx.empty())mp.mesh=r_->uploadSkin(v,idx);m.prims.push_back(std::move(mp));m.fit=1;m.foot=0;m.height=1;m.radius=2;m.autoRig=false;return true;}
void App::pmRebuild(){Entity* e=pmEntity();if(!e)return;models_.erase(e->asset);models_.erase("pmcut:M2");if(const char* an=getenv("SABAK_PMANIM"))e->anim=an;
  for(auto i2=minst_.begin();i2!=minst_.end();){const Entity* en=scene_.get(i2->first.second);if(i2->first.first==&scene_&&en&&(en->asset==e->asset||en->asset=="pmcut:M2"))i2=minst_.erase(i2);else ++i2;}modelFor(e->asset);}
void App::pmLive(){Entity* e=pmEntity();if(!e)return;Model* m=modelFor(e->asset);if(!m||m->prims.empty())return;
  std::vector<std::vector<SkinVertex>> vs;bool ok=true;int present=0;
  for(int k=0;k<8;k++){std::vector<SkinVertex> v;std::vector<uint32_t> idx;pmArrays(v,idx,false,k);if(idx.empty())continue;present++;bool f=false;
    for(auto& p:m->prims)if(p.mat==k){f=true;if(v.size()!=p.cv.size()||idx.size()!=p.ci.size())ok=false;}if(!f)ok=false;vs.push_back(v);}
  if(!ok||present!=(int)m->prims.size()){pmRebuild();return;}
  size_t i=0;for(int k=0;k<8;k++){for(auto& p:m->prims)if(p.mat==k&&i<vs.size()){p.cv=vs[i];r_->updateSkin(p.mesh,vs[i]);i++;break;}}}

void App::pmPrim(int k){bool half=pm_.mirrorX;float cx=half?0.6f:0.f;
  if(k==0)pm_.cube({cx,0.8f,0},{0.6f,0.6f,0.6f});else if(k==1){pm_.cylinder({0,0.8f,0},0.6f,1.2f,12);if(pmObj_==1){for(auto& p:pm_.d.V){float yy=p.y-0.8f;p={yy,0.8f+p.x*0+0,p.z};}}}else{pm_.grid(8,2.f);for(auto& p:pm_.d.V){p.y+=0.1f;if(half)p.x+=1.f;}
    if(half){std::vector<std::vector<int>> nf;for(auto& f:pm_.d.F){bool ok=true;for(int v:f)if(pm_.d.V[v].x<-1e-4f)ok=false;if(ok)nf.push_back(f);}pm_.d.F=nf;}}
  pm_.selectNone();pm_.mode=2;}

void App::openModel(){if(cur_<0||cur_>=(int)projects_.size())return;std::string dir=projects_[cur_].dir;std::error_code ec;fs::create_directories(pdir(dir),ec);
  Entity* e=pmEntity();if(!e){Entity& ne=scene_.add(P_MODEL,"PolyModel");ne.asset="pmodel:M1";ne.pos={0,0.08f,0};e=scene_.get(ne.id);rebuildRows();}
  pmObj_=0;pm_=PolyMesh();pmOther_=PolyMesh();if(!pm_.load(pdir(dir)+"/M1.ply"))pmPrim(0);
  if(!pmOther_.load(pdir(dir)+"/M2.ply")){pmOther_.mirrorX=false;pmOther_.cube({0,0.8f,0},{0.35f,0.35f,0.35f});pmOther_.mirrorX=false;}
  {bool has=false;for(auto& q:scene_.ents)if(q.asset=="pmcut:M2")has=true;if(!has){Entity& ce=scene_.add(P_MODEL,"Cutter");ce.asset="pmcut:M2";ce.pos={0,0.08f,0};}}pm_.subLevels=std::max(pm_.subLevels,0);pmEd_=true;pmDrag_=-1;scEd_=false;animEd_=false;crEd_=false;select(e->id);pmRebuild();
  Vec3 p=scene_.worldPos(e->id);ecTarget_={p.x,p.y+0.9f,p.z};ecDist_=4.2f;ecPitch_=0.35f;ecYaw_=0.7f;}
void App::closeModel(){if(pmObj_==1){std::swap(pm_,pmOther_);pmObj_=0;}if(cur_>=0&&cur_<(int)projects_.size()){pm_.save(pdir(projects_[cur_].dir)+"/M1.ply");pmOther_.save(pdir(projects_[cur_].dir)+"/M2.ply");}pmRefClear();pmEd_=false;}


// ---- reference (blueprint) planes: real-scale drawings shown behind/around the model
bool App::pmRefModel(const std::string& nm,Model& m){
  std::string path=g_assetDir+"/reference/g82_"+nm+".png";if(cur_>=0&&cur_<(int)projects_.size()){std::string pp=projects_[cur_].dir+"/pmodels/ref_"+nm+".png";if(fs::exists(pp))path=pp;}
  int w=0,h=0,c=0;unsigned char* px=stbi_load(path.c_str(),&w,&h,&c,4);if(!px)return false;
  m=Model();m.name="Ref";m.nodeName={"root"};m.parent={-1};m.rest.assign(1,TRS());m.joints={0};m.ibm={Mat4::identity()};
  ModelMat mm;mm.kind=13;mm.col[0]=mm.col[1]=mm.col[2]=mm.col[3]=1;if(r_)mm.tex=r_->createTexture(px,w,h);stbi_image_free(px);m.mats.push_back(mm);
  std::vector<SkinVertex> v;std::vector<uint32_t> idx;
  auto V=[&](Vec3 p,float u,float t){SkinVertex s{};s.p[0]=p.x;s.p[1]=p.y;s.p[2]=p.z;s.n[1]=1;s.uv[0]=u;s.uv[1]=t;s.j[0]=0;s.w[0]=1;v.push_back(s);};
  Vec3 a,b,cc,d; // corners: top-left, top-right, bottom-right, bottom-left as seen from the front face
  if(nm=="side"){float L=2.46f,H=1.48f;a={0,H,L};b={0,H,-L};cc={0,0,-L};d={0,0,L};}
  else if(nm=="front"){float Wd=1.065f,H=1.481f;a={-Wd,H,2.5f};b={Wd,H,2.5f};cc={Wd,0,2.5f};d={-Wd,0,2.5f};}
  else{float L=2.46f,Wd=0.985f;float y=0.25f;a={-Wd,y,L};b={-Wd,y,-L};cc={Wd,y,-L};d={Wd,y,L};}
  V(a,0,0);V(b,1,0);V(cc,1,1);V(d,0,1);V(b,0,0);V(a,1,0);V(d,1,1);V(cc,0,1);
  for(uint32_t q:{0u,1u,2u,0u,2u,3u,4u,5u,6u,4u,6u,7u})idx.push_back(q);
  // order: front-face winding differs per plane; draw both orientations so it is visible from either side
  for(uint32_t q:{0u,2u,1u,0u,3u,2u,4u,6u,5u,4u,7u,6u})idx.push_back(q);
  ModelPrim mp;mp.mat=0;mp.cv=v;mp.ci=idx;if(r_)mp.mesh=r_->uploadSkin(v,idx);m.prims.push_back(std::move(mp));m.fit=1;m.foot=0;m.height=1.5f;m.radius=3;m.autoRig=false;return true;}
void App::pmRefToggle(int i){const char* nm[3]={"side","front","top"};std::string as=std::string("pmref:")+nm[i];Entity* ex=nullptr;for(auto& e:scene_.ents)if(e.asset==as)ex=&e;
  if(ex){int id=ex->id;scene_.remove(id);rebuildRows();return;}
  Entity& ne=scene_.add(P_MODEL,std::string("Ref_")+nm[i]);ne.asset=as;ne.pos={0,0.08f,0};}
void App::pmRefClear(){std::vector<int> ids;for(auto& e:scene_.ents)if(e.asset.rfind("pmref:",0)==0||e.asset=="pmcut:M2")ids.push_back(e.id);for(int id:ids)scene_.remove(id);}
// ---- picking in screen space (framebuffer pixels)
bool App::pmProj(Vec3 p,float& x,float& y) const{Entity* e=const_cast<App*>(this)->pmEntity();Vec3 o=e?scene_.worldPos(e->id):Vec3{0,0,0};return r_->project(p+o,x,y);}
int App::pmPickVert(float px,float py) const{int best=-1;float bd=26*ui.s();bd*=bd;for(size_t i=0;i<pm_.d.V.size();i++){float x,y;if(!pmProj(pm_.d.V[i],x,y))continue;float dd=(x-px)*(x-px)+(y-py)*(y-py);if(dd<bd){bd=dd;best=(int)i;}}return best;}
ull App::pmPickEdge(float px,float py) const{ull best=0;float bd=22*ui.s();std::set<ull> seen;for(auto& f:pm_.d.F)for(size_t i=0;i<f.size();i++){ull k=PolyMesh::key(f[i],f[(i+1)%f.size()]);if(!seen.insert(k).second)continue;float x0,y0,x1,y1;if(!pmProj(pm_.d.V[f[i]],x0,y0)||!pmProj(pm_.d.V[f[(i+1)%f.size()]],x1,y1))continue;
      float dx=x1-x0,dy=y1-y0;float l2=dx*dx+dy*dy;float t=l2>0?std::max(0.f,std::min(1.f,((px-x0)*dx+(py-y0)*dy)/l2)):0;float cx=x0+dx*t,cy=y0+dy*t;float dd=std::sqrt((px-cx)*(px-cx)+(py-cy)*(py-cy));if(dd<bd){bd=dd;best=k;}}return best;}
int App::pmPickFace(float px,float py) const{int best=-1;float bz=1e30f;Entity* e=const_cast<App*>(this)->pmEntity();Vec3 o=e?scene_.worldPos(e->id):Vec3{0,0,0};Vec3 cam=r_->cameraPos();
  for(size_t f=0;f<pm_.d.F.size();f++){auto& F=pm_.d.F[f];std::vector<float> X,Y;bool ok=true;for(int v:F){float x,y;if(!pmProj(pm_.d.V[v],x,y)){ok=false;break;}X.push_back(x);Y.push_back(y);}if(!ok)continue;
    bool in=false;for(size_t i=0,j=F.size()-1;i<F.size();j=i++)if(((Y[i]>py)!=(Y[j]>py))&&(px<(X[j]-X[i])*(py-Y[i])/(Y[j]-Y[i])+X[i]))in=!in;if(!in)continue;
    Vec3 c=pm_.faceCenter((int)f)+o;Vec3 dv=c-cam;float z=dot(dv,dv);if(z<bz){bz=z;best=(int)f;}}return best;}

bool App::pmPointer(int type,int id,float px,float py){if(!pmEd_)return false;float s=ui.s(),x=px/s,y=py/s;float H=H_/s;
  auto slider=[&](int w){float t=std::max(0.f,std::min(1.f,(x-18)/176.f));if(w==0)pmAmt_=-0.5f+t*1.0f;else pmProp_=t*1.5f;};
  Entity* e=pmEntity();(void)e;
  if(type==0){
    if(x>=10&&x<=204){for(int w=0;w<2;w++){float ry=268+w*24;if(y>=ry&&y<=ry+28){pmDrag_=w;pmDragId_=id;slider(w);return true;}}}
    if(x<212||y<42||y>H-24)return false;
    if(pmTool_==2){ull k=pmPickEdge(px,py);if(k){pm_.loopCut(k,0.5f);pmRebuild();toast("Loop cut");return true;}return false;}
    if(pmTool_==1&&!pm_.selV.empty()){pmMove_=id;pmLX_=px;pmLY_=py;pm_.snapshot();return true;}
    pmTapId_=id;pmTX_=px;pmTY_=py;pmTapMoved_=false;return false;}
  if(type==1){
    if(id==pmDragId_&&pmDrag_>=0){slider(pmDrag_);return true;}
    if(id==pmMove_){Entity* en=pmEntity();Vec3 o=en?scene_.worldPos(en->id):Vec3{0,0,0};Vec3 c=pm_.selCenter()+o;Vec3 o0,d0,o1,d1;r_->ray(pmLX_,pmLY_,o0,d0);r_->ray(px,py,o1,d1);
      Vec3 cp=r_->cameraPos();Vec3 nrmv=norm(c-cp);auto hit=[&](Vec3 ro,Vec3 rd){float dn=dot(rd,nrmv);if(std::fabs(dn)<1e-5f)return c;float t=dot(c-ro,nrmv)/dn;return ro+rd*t;};
      Vec3 dl=hit(o1,d1)-hit(o0,d0);
      if(pmAxis_<3){Vec3 ax=pmAxis_==0?Vec3{1,0,0}:pmAxis_==1?Vec3{0,1,0}:Vec3{0,0,1};dl=ax*dot(dl,ax);}
      pm_.moveSel(dl,pmProp_);pmLX_=px;pmLY_=py;pmLive();return true;}
    if(id==pmTapId_&&!pmTapMoved_){float dx=px-pmTX_,dy=py-pmTY_;if(dx*dx+dy*dy>64*s*s)pmTapMoved_=true;}
    return false;}
  if(type==2){
    if(id==pmDragId_){pmDrag_=-1;pmDragId_=-1;return true;}
    if(id==pmMove_){pmMove_=-1;pmRebuild();return true;}
    if(id==pmTapId_){pmTapId_=-1;if(!pmTapMoved_){bool had=!pmMulti_?true:false;
        if(pm_.mode==0){int v=pmPickVert(px,py);if(had)pm_.selectNone();if(v>=0)pm_.toggleVert(v);}
        else if(pm_.mode==1){ull k=pmPickEdge(px,py);if(had)pm_.selectNone();if(k)pm_.toggleEdge(k);}
        else{int f=pmPickFace(px,py);if(had)pm_.selectNone();if(f>=0)pm_.toggleFace(f);}
        return true;}}}
  return false;}

void App::pmTap(int id){std::string dir=cur_>=0&&cur_<(int)projects_.size()?projects_[cur_].dir:"";
  if(id>=940&&id<943){pm_.setMode(id-940);if(id==942)pm_.setMode(2);return;}
  if(id==943){pmTool_=0;return;} if(id==944){pmTool_=1;return;} if(id==945){pmTool_=2;return;}
  if(id==946){if(pm_.extrude(pmAmt_*2.f)){pmRebuild();}else toast("Select a face first");return;}
  if(id==947){if(pm_.inset(std::max(0.05f,std::min(0.9f,std::fabs(pmAmt_)*1.8f)))){pmRebuild();}else toast("Select a face first");return;}
  if(id==948){float c=std::max(0.f,std::min(3.f,std::fabs(pmAmt_)*6.f));if(pm_.setCrease(c)){toast(c<=0?"Crease removed":"Edge crease set (use Subd to see rounding)");pmRebuild();}else toast("Select edges first (Edge mode)");return;}
  if(id==949){if(pm_.deleteFaces())pmRebuild();else toast("Select faces first");return;}
  if(id==950){if(pm_.flip())pmRebuild();return;}
  if(id==951){pm_.snapshot();pm_.scaleSel(pmAmt_>=0?1.1f:0.9f,pmProp_);pmRebuild();return;}
  if(id==952){pm_.snapshot();pm_.scaleSel(0.9f,pmProp_);pmRebuild();return;}
  if(id==953){pmMulti_=!pmMulti_;return;}
  if(id==954){pm_.mirrorX=!pm_.mirrorX;pmRebuild();return;}
  if(id==955){pm_.subLevels=(pm_.subLevels+1)%4;pmRebuild();return;}
  if(id==956){pmWire_=!pmWire_;return;}
  if(id==988){PolyData T=pmOther_.evaluate(pmOther_.subLevels);if(pm_.shrinkwrap(T,0.f))pmRebuild();else toast("Shrinkwrap needs a cutter/target mesh");return;}
  if(id==989){if(pm_.snapBlueprint())pmRebuild();else toast("Select vertices near the blueprint outline first");return;}
  if(id>=980&&id<=987){if(pm_.setMat(id-980))pmRebuild();else toast("Select faces first (Face mode)");return;}
  if(id==979){pm_.snapshot();buildCarPoly(pm_);pm_.snapshot();bakeWheelArches(pm_);if(pmOther_.d.V.size()&&pmOther_.d.V[0].y>-20)for(auto& v:pmOther_.d.V)v.y-=60.f;pmRebuild();toast("G82 car built (arches cut)");return;}
  if(id==957||id==958||id==959){pm_.snapshot();pmPrim(id-957);pmRebuild();return;}
  if(id>=961&&id<=964){pmAxis_=id-961;return;}
  if(id==960){if(pm_.undo())pmRebuild();else toast("Nothing to undo");return;}
  if(id==965){Model m;pmModel(m);std::string err,out=data_+"/Models/Exports/PolyModel.glb";if(exportGlb(m,out,err))toast("Exported: Files > SabakEngine > Models > Exports");else toast("Export failed: "+err);return;}
  if(id==966){if(!dir.empty()){(pmObj_==0?pm_:pmOther_).save(pdir(dir)+"/M1.ply");(pmObj_==0?pmOther_:pm_).save(pdir(dir)+"/M2.ply");saveScene(true);toast("Model saved");}return;}
  if(id==975){std::swap(pm_,pmOther_);pmObj_^=1;pm_.selectNone();toast(pmObj_?"Editing the CUTTER (red)":"Editing the MODEL");return;}
  if(id>=976&&id<=978){if(pmObj_==1){std::swap(pm_,pmOther_);pmObj_=0;}pm_.snapshot();PolyData A=pm_.evaluate(pm_.subLevels),B=pmOther_.evaluate(pmOther_.subLevels);PolyData R=PolyMesh::boolean(A,B,id-976);
    if(R.F.empty()||R.V.empty()){toast("Boolean gave an empty result");return;}pm_.d=R;pm_.mirrorX=false;pm_.subLevels=0;pm_.selectNone();pmRebuild();toast(id==976?"Subtracted":id==977?"Union done":"Intersect done");return;}
  if(id>=972&&id<=974){float yw[3]={1.5708f,0.f,0.f},pt[3]={0.f,0.f,1.5f};ecYaw_=yw[id-972];ecPitch_=pt[id-972];ecDist_=std::max(ecDist_,id==972?6.f:5.f);return;}
  if(id>=969&&id<=971){pmRefToggle(id-969);return;}
  if(id==967){ecDist_=std::max(0.6f,ecDist_*0.8f);return;} if(id==968){ecDist_=std::min(30.f,ecDist_*1.25f);return;}
}

void App::drawModelStudio(float W,float H){
  ui.rrect(0,0,W,40,0,BG1.with(0.97f),BG1.with(0.97f));ui.rrect(0,39,W,1,0,LINE,LINE);
  fbtn(5,10,7,100,26,"",0);ui.text("Done",60,12,12,Col::hex(0xffffff),F_BOLD,1);ui.text("MODEL STUDIO",122,14,12,TXT,F_BOLD);
  drawToolBtn(975,232,8,92,24,pmObj_?"Obj: Cutter":"Obj: Model",pmObj_==1);drawToolBtn(976,328,8,64,24,"Subtract");drawToolBtn(977,396,8,50,24,"Union");drawToolBtn(978,450,8,64,24,"Intersect");
  drawToolBtn(972,W-322,8,52,24,"Side");drawToolBtn(973,W-266,8,52,24,"Front");drawToolBtn(974,W-210,8,52,24,"Top");
  drawToolBtn(967,W-136,8,56,24,"Zoom +");drawToolBtn(968,W-76,8,66,24,"Zoom -");
  float x=6,y=46,w=200,h=H-46-28;ui.shadow(x,y+2,w,h,5,10,Col::hex(0,0.35f));ui.rrect(x,y,w,h,5,BG1.with(0.96f),BG1.with(0.96f));ui.ring(x,y,w,h,5,1,LINE);ui.rrect(x+1,y+1,w-2,22,4,BG2,BG2);
  ui.text("POLY TOOLS",x+12,y+7,10.5f,TXT2,F_BOLD);
  char st[64];std::snprintf(st,sizeof st,"%dV %dF",(int)pm_.d.V.size(),(int)pm_.d.F.size());ui.text(st,x+188,y+8,9.5f,TXT3,F_BOLD,2);
  float cw=60,gx=x+6;auto B=[&](int id,int col,int row,const char* l,bool on=false,int span=1){float bw=cw*span+(span-1)*4;drawToolBtn(id,gx+col*(cw+4),y+27+row*24,bw,21,l,on);};
  B(940,0,0,"Vertex",pm_.mode==0);B(941,1,0,"Edge",pm_.mode==1);B(942,2,0,"Face",pm_.mode==2);
  B(943,0,1,"Select",pmTool_==0);B(944,1,1,"Move",pmTool_==1);B(945,2,1,"Loop cut",pmTool_==2);
  B(946,0,2,"Extrude");B(947,1,2,"Inset");B(948,2,2,"Crease");
  B(949,0,3,"Delete");B(950,1,3,"Flip");B(960,2,3,"Undo");
  B(951,0,4,"Scale +");B(952,1,4,"Scale -");B(953,2,4,pmMulti_?"Multi ON":"Multi",pmMulti_);
  B(954,0,5,pm_.mirrorX?"Mirror ON":"Mirror",pm_.mirrorX);{char b[16];std::snprintf(b,sizeof b,"Subd %d",pm_.subLevels);B(955,1,5,b,pm_.subLevels>0);}B(956,2,5,pmWire_?"Wire ON":"Wire",pmWire_);
  {float bw4=44;const char* pn[4]={"Cube","Cyl","Grid","Car"};int pid[4]={957,958,959,979};for(int i=0;i<4;i++)drawToolBtn(pid[i],gx+i*(bw4+4),y+27+6*24,bw4,21,pn[i]);}
  {float aw=44;const char* an[4]={"X","Y","Z","Free"};for(int i=0;i<4;i++)drawToolBtn(961+i,gx+i*(aw+4),y+27+7*24,aw,21,an[i],pmAxis_==i);}
  const char* nm[2]={"Amount (extrude/inset/crease)","Soft radius (proportional)"};float val[2]={(pmAmt_+0.5f),pmProp_/1.5f};
  for(int i=0;i<2;i++){float ry=268+i*24;ui.text(nm[i],x+14,ry+1,10,TXT2,F_BOLD);float t=std::max(0.f,std::min(1.f,val[i]));ui.rrect(18,ry+14,176,5,2,BG0,BG0);ui.rrect(18,ry+14,176*t,5,2,ACC,ACC);ui.circle(18+176*t,ry+16.5f,6,Col::hex(0xffffff),Col::hex(0xffffff));}
  {bool hs=false,hf=false,ht=false;for(auto& e:scene_.ents){if(e.asset=="pmref:side")hs=true;if(e.asset=="pmref:front")hf=true;if(e.asset=="pmref:top")ht=true;}drawToolBtn(969,gx,321,60,19,"Ref side",hs);drawToolBtn(970,gx+64,321,60,19,"Ref front",hf);drawToolBtn(971,gx+128,321,60,19,"Ref top",ht);}
  {const char* ml[8]={"Paint","Glass","Chrome","Black","Rubber","Lamp","Tail","Seat"};int cur=-1;for(int f:pm_.selF){int m2=f<(int)pm_.d.M.size()?pm_.d.M[f]:0;if(cur==-1)cur=m2;else if(cur!=m2){cur=-2;break;}}
    ui.text("MATERIAL",W-52,48,8.5f,TXT3,F_BOLD);for(int i=0;i<8;i++)drawToolBtn(980+i,W-52,60+i*25,48,21,ml[i],cur==i);
    drawToolBtn(988,W-52,266,48,21,"Wrap");drawToolBtn(989,W-52,291,48,21,"Snap BP");}
  drawToolBtn(965,gx,343,92,19,"Export GLB");drawToolBtn(966,gx+96,343,92,19,"Save");
  // ---- cutter wire (red) when editing the model
  if(pmWire_&&pmObj_==0){float S=ui.s();std::set<ull> seen;for(auto& f:pmOther_.d.F)for(size_t i=0;i<f.size();i++){int a=f[i],b=f[(i+1)%f.size()];if(!seen.insert(PolyMesh::key(a,b)).second)continue;float x0,y0,x1,y1;if(!pmProj(pmOther_.d.V[a],x0,y0)||!pmProj(pmOther_.d.V[b],x1,y1))continue;ui.line(x0/S,y0/S,x1/S,y1/S,1.1f,Col::hex(0xff5a5a,0.9f));}}
  // ---- wire / vertex overlay
  if(pmWire_&&pm_.d.F.size()<=2500){float S=ui.s();Col wc=Col::hex(0xdfe6ff,0.55f),sc=Col::hex(0xffa23a,0.95f);std::set<ull> seen;
    for(auto& f:pm_.d.F)for(size_t i=0;i<f.size();i++){int a=f[i],b=f[(i+1)%f.size()];ull k=PolyMesh::key(a,b);if(!seen.insert(k).second)continue;float x0,y0,x1,y1;if(!pmProj(pm_.d.V[a],x0,y0)||!pmProj(pm_.d.V[b],x1,y1))continue;
      bool se=pm_.selE.count(k)>0;auto cr=pm_.d.crease.find(k);Col c=se?sc:(cr!=pm_.d.crease.end()?Col::hex(0xff5a8a,0.9f):wc);ui.line(x0/S,y0/S,x1/S,y1/S,se?1.6f:0.9f,c);}
    for(int f:pm_.selF){auto& F=pm_.d.F[f];for(size_t i=1;i+1<F.size();i++){float a[3],b[3];int ids[3]={F[0],F[i],F[i+1]};bool ok=true;for(int k=0;k<3;k++)if(!pmProj(pm_.d.V[ids[k]],a[k],b[k]))ok=false;if(ok)ui.tri(a[0]/S,b[0]/S,a[1]/S,b[1]/S,a[2]/S,b[2]/S,Col::hex(0xffa23a,0.35f));}}
    if(pm_.mode==0||pm_.mode==1)for(size_t v=0;v<pm_.d.V.size();v++){float vx,vy;if(!pmProj(pm_.d.V[v],vx,vy))continue;bool se=pm_.selV.count((int)v)>0;if(pm_.mode==0||se)ui.circle(vx/S,vy/S,se?3.2f:2.2f,se?sc:Col::hex(0xffffff,0.8f),se?sc:Col::hex(0xffffff,0.8f));}}
}
}
