#include "sculpt.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <unordered_map>
namespace sk {
static void setSV(SkinVertex& s,Vec3 p){s.p[0]=p.x;s.p[1]=p.y;s.p[2]=p.z;s.uv[0]=p.x*0.7f+p.z*0.3f;s.uv[1]=p.y*0.7f+p.z*0.2f;s.j[0]=s.j[1]=s.j[2]=s.j[3]=0;s.w[0]=1;s.w[1]=s.w[2]=s.w[3]=0;s.c[0]=205;s.c[1]=163;s.c[2]=133;s.c[3]=255;}
void Sculpt::setMesh(const std::vector<Vec3>& pos,const std::vector<uint32_t>& ind,const std::vector<std::array<unsigned char,4>>* cols){
  std::unordered_map<unsigned long long,uint32_t> mp;std::vector<uint32_t> remap(pos.size());std::vector<SkinVertex> tv;
  for(size_t i=0;i<pos.size();i++){long long a=std::llround(pos[i].x*20000.0),b=std::llround(pos[i].y*20000.0),c=std::llround(pos[i].z*20000.0);
    unsigned long long key=((unsigned long long)(a&0x1fffff)<<42)|((unsigned long long)(b&0x1fffff)<<21)|(unsigned long long)(c&0x1fffff);
    auto it=mp.find(key);if(it==mp.end()){SkinVertex s{};setSV(s,pos[i]);if(cols&&i<cols->size())for(int k=0;k<4;k++)s.c[k]=(*cols)[i][k];mp[key]=(uint32_t)tv.size();remap[i]=(uint32_t)tv.size();tv.push_back(s);}else remap[i]=it->second;}
  std::vector<uint32_t> I;for(size_t i=0;i+2<ind.size();i+=3){uint32_t a=remap[ind[i]],b=remap[ind[i+1]],c=remap[ind[i+2]];if(a==b||b==c||a==c)continue;I.push_back(a);I.push_back(b);I.push_back(c);}
  std::vector<int> nid(tv.size(),-1);v.clear();idx.clear();for(uint32_t i:I){if(nid[i]<0){nid[i]=(int)v.size();v.push_back(tv[i]);}idx.push_back((uint32_t)nid[i]);}
  undo_.clear();topo();}
void Sculpt::topo(){size_t n=v.size();cstamp_.assign(n,0);stamp_.assign(n,0);mark_.assign(n,0);strokeId_=markId_=0;
  std::vector<std::vector<uint32_t>> adj(n);std::vector<uint32_t> cnt(n+1,0);
  for(size_t i=0;i+2<idx.size();i+=3)for(int k=0;k<3;k++){uint32_t a=idx[i+k],b=idx[i+(k+1)%3];adj[a].push_back(b);adj[b].push_back(a);cnt[a+1]++;cnt[b+1]++;}
  // note: cnt uses face-corner counts for vertex faces below
  adjS.assign(n+1,0);adjI.clear();for(size_t i=0;i<n;i++){auto& a=adj[i];std::sort(a.begin(),a.end());a.erase(std::unique(a.begin(),a.end()),a.end());adjS[i]=(uint32_t)adjI.size();adjI.insert(adjI.end(),a.begin(),a.end());}adjS[n]=(uint32_t)adjI.size();
  std::vector<uint32_t> fc(n+1,0);for(size_t i=0;i<idx.size();i++)fc[idx[i]+1]++;for(size_t i=0;i<n;i++)fc[i+1]+=fc[i];vfS=fc;vfI.assign(idx.size(),0);std::vector<uint32_t> fill(fc.begin(),fc.end()-1);
  for(size_t i=0;i<idx.size();i++)vfI[fill[idx[i]]++]=(uint32_t)(i/3);
  {double sum=0;size_t c=0;for(size_t i=0;i<n;i+=std::max<size_t>(1,n/2000))for(uint32_t q=adjS[i];q<adjS[i+1];q++){sum+=len(pos((uint32_t)i)-pos(adjI[q]));c++;}avgEdge_=c?(float)(sum/c):0.01f;}
  std::vector<uint32_t> all(n);for(size_t i=0;i<n;i++)all[i]=(uint32_t)i;renorm(all);}
void Sculpt::renorm(const std::vector<uint32_t>& a){for(uint32_t i:a){Vec3 s{0,0,0};for(uint32_t k=vfS[i];k<vfS[i+1];k++){uint32_t f=vfI[k]*3;Vec3 p0=pos(idx[f]),p1=pos(idx[f+1]),p2=pos(idx[f+2]);s=s+cross(p1-p0,p2-p0);}
    s=norm(s);v[i].n[0]=s.x;v[i].n[1]=s.y;v[i].n[2]=s.z;}}
void Sculpt::fromSphere(int N,float r,Vec3 c){std::vector<Vec3> P;std::vector<uint32_t> I;
  for(int f=0;f<6;f++){uint32_t b=(uint32_t)P.size();for(int j=0;j<=N;j++)for(int i=0;i<=N;i++){float u=2.f*i/N-1,w=2.f*j/N-1;Vec3 q;
      switch(f){case 0:q={1,w,-u};break;case 1:q={-1,w,u};break;case 2:q={u,1,-w};break;case 3:q={u,-1,w};break;case 4:q={u,w,1};break;default:q={-u,w,-1};}
      // equal-area-ish cube->sphere map
      float x2=q.x*q.x,y2=q.y*q.y,z2=q.z*q.z;Vec3 s{q.x*std::sqrt(1-y2*0.5f-z2*0.5f+y2*z2/3),q.y*std::sqrt(1-x2*0.5f-z2*0.5f+x2*z2/3),q.z*std::sqrt(1-x2*0.5f-y2*0.5f+x2*y2/3)};P.push_back(c+s*r);}
    for(int j=0;j<N;j++)for(int i=0;i<N;i++){uint32_t a=b+j*(N+1)+i,bb=a+1,cc=a+N+1,d=cc+1;I.insert(I.end(),{a,bb,cc,bb,d,cc});}}
  // orient triangles outward
  for(size_t i=0;i+2<I.size();i+=3){Vec3 a=P[I[i]],b=P[I[i+1]],cc=P[I[i+2]];Vec3 n=cross(b-a,cc-a);Vec3 m=(a+b+cc)*(1.f/3)-c;if(dot(n,m)<0)std::swap(I[i+1],I[i+2]);}
  setMesh(P,I);}
void Sculpt::fromModel(const Model& m,int mode){std::vector<Vec3> P;std::vector<uint32_t> I;std::vector<std::array<unsigned char,4>> C;float bodyTop=0;
  for(auto& mp:m.prims){const ModelMat& mm=m.mats[std::min<size_t>(mp.mat,m.mats.size()-1)];if(mm.kind==1)for(auto& s:mp.cv)bodyTop=std::max(bodyTop,s.p[1]);}
  float thr=bodyTop*0.835f;
  for(auto& mp:m.prims){const ModelMat& mm=m.mats[std::min<size_t>(mp.mat,m.mats.size()-1)];
    if(mode==1||mode==2){if(mm.kind!=1&&mm.kind!=4)continue;}
    if(mode==3&&mm.kind!=8)continue;
    uint32_t b=(uint32_t)P.size();std::array<unsigned char,4> col{(unsigned char)(std::min(1.f,mm.col[0])*255),(unsigned char)(std::min(1.f,mm.col[1])*255),(unsigned char)(std::min(1.f,mm.col[2])*255),255};
    for(auto& s:mp.cv){P.push_back({s.p[0],s.p[1],s.p[2]});C.push_back(col);}
    for(size_t i=0;i+2<mp.ci.size();i+=3){if(mode==2&&mm.kind==1){float lo=std::min({P[b+mp.ci[i]].y,P[b+mp.ci[i+1]].y,P[b+mp.ci[i+2]].y});if(lo<thr)continue;}
      I.push_back(mp.ci[i]+b);I.push_back(mp.ci[i+1]+b);I.push_back(mp.ci[i+2]+b);}}
  if(mode==2){Vec3 mn{1e9f,1e9f,1e9f},mx{-1e9f,-1e9f,-1e9f};std::vector<char> used(P.size(),0);for(uint32_t i:I)used[i]=1;for(size_t i=0;i<P.size();i++)if(used[i]){mn.x=std::min(mn.x,P[i].x);mn.y=std::min(mn.y,P[i].y);mn.z=std::min(mn.z,P[i].z);mx.x=std::max(mx.x,P[i].x);mx.y=std::max(mx.y,P[i].y);mx.z=std::max(mx.z,P[i].z);}
    float k=3.2f;float cz=(mn.z+mx.z)*0.5f;for(auto& p:P)p={p.x*k,(p.y-mn.y)*k+0.25f,(p.z-cz)*k};}
  setMesh(P,I,&C);}
void Sculpt::fillColor(const unsigned char* c){for(auto& s:v)for(int k=0;k<4;k++)s.c[k]=c[k];}
float Sculpt::maxY() const{float m=0;for(auto& s:v)m=std::max(m,s.p[1]);return m;}
bool Sculpt::subdivide(size_t maxTris){if(tris()*4>maxTris)return false;size_t n=v.size();
  struct EK{uint32_t a,b;bool operator==(const EK& o)const{return a==o.a&&b==o.b;}};
  std::unordered_map<unsigned long long,uint32_t> em;std::vector<Vec3> np;std::vector<std::array<int,2>> opp;std::vector<std::array<uint32_t,2>> ends;
  auto key=[&](uint32_t a,uint32_t b){if(a>b)std::swap(a,b);return ((unsigned long long)a<<32)|b;};
  auto edge=[&](uint32_t a,uint32_t b,uint32_t o)->uint32_t{auto k=key(a,b);auto it=em.find(k);if(it==em.end()){uint32_t id=(uint32_t)(n+ends.size());em[k]=id;ends.push_back({a,b});opp.push_back({(int)o,-1});return id;}
    auto& op=opp[it->second-n];if(op[1]<0)op[1]=(int)o;return it->second;};
  std::vector<uint32_t> I;I.reserve(idx.size()*4);
  for(size_t i=0;i+2<idx.size();i+=3){uint32_t a=idx[i],b=idx[i+1],c=idx[i+2];uint32_t ab=edge(a,b,c),bc=edge(b,c,a),ca=edge(c,a,b);I.insert(I.end(),{a,ab,ca,b,bc,ab,c,ca,bc,ab,bc,ca});}
  std::vector<SkinVertex> nv(n+ends.size());std::vector<char> bnd(n,0);
  for(size_t e=0;e<ends.size();e++){Vec3 a=pos(ends[e][0]),b=pos(ends[e][1]),p=(a+b)*0.5f;if(opp[e][1]>=0)p=(a+b)*0.375f+(pos(opp[e][0])+pos(opp[e][1]))*0.125f;else{bnd[ends[e][0]]=bnd[ends[e][1]]=1;}setSV(nv[n+e],p);for(int k=0;k<4;k++)nv[n+e].c[k]=(unsigned char)((v[ends[e][0]].c[k]+v[ends[e][1]].c[k])/2);}
  for(size_t i=0;i<n;i++){Vec3 p=pos((uint32_t)i);uint32_t k=adjS[i+1]-adjS[i];if(!bnd[i]&&k>=3){float beta=k==3?3.f/16:3.f/(8*k);Vec3 s{0,0,0};for(uint32_t q=adjS[i];q<adjS[i+1];q++)s=s+pos(adjI[q]);p=p*(1-k*beta)+s*beta;}setSV(nv[i],p);for(int c=0;c<4;c++)nv[i].c[c]=v[i].c[c];}
  v=nv;idx=I;undo_.clear();topo();return true;}
bool Sculpt::raycast(Vec3 o,Vec3 d,Vec3& hit,Vec3& n) const{float best=1e30f;int bf=-1;
  for(size_t i=0;i+2<idx.size();i+=3){Vec3 a=pos(idx[i]),b=pos(idx[i+1]),c=pos(idx[i+2]);Vec3 e1=b-a,e2=c-a;Vec3 pv=cross(d,e2);float det=dot(e1,pv);if(std::fabs(det)<1e-12f)continue;float inv=1/det;Vec3 tv=o-a;float u=dot(tv,pv)*inv;if(u<0||u>1)continue;Vec3 qv=cross(tv,e1);float w=dot(d,qv)*inv;if(w<0||u+w>1)continue;float t=dot(e2,qv)*inv;if(t>1e-4f&&t<best){best=t;bf=(int)i;}}
  if(bf<0)return false;hit=o+d*best;Vec3 a=pos(idx[bf]),b=pos(idx[bf+1]),c=pos(idx[bf+2]);n=norm(cross(b-a,c-a));return true;}
void Sculpt::touchC(uint32_t i){if(cstamp_[i]==strokeId_)return;cstamp_[i]=strokeId_;if(!undo_.empty())undo_.back().c.push_back({i,{v[i].c[0],v[i].c[1],v[i].c[2],v[i].c[3]}});}
void Sculpt::touch(uint32_t i){if(stamp_[i]==strokeId_)return;stamp_[i]=strokeId_;if(!undo_.empty())undo_.back().d.push_back({i,pos(i)});}
void Sculpt::setPos(uint32_t i,Vec3 p){touch(i);v[i].p[0]=p.x;v[i].p[1]=p.y;v[i].p[2]=p.z;}
void Sculpt::beginStroke(Vec3 hit,Vec3 o,Vec3 d){stroking_=true;strokeId_++;undo_.emplace_back();if(undo_.size()>40)undo_.erase(undo_.begin());lastDab_=hit;
  if(tool==5){grabStart_=hit;grabDir_=d;(void)o;for(int s=0;s<2;s++){gIdx[s].clear();gW[s].clear();gBase[s].clear();if(s==1&&!sym)break;Vec3 c=s?Vec3{-hit.x,hit.y,hit.z}:hit;
      for(uint32_t i=0;i<v.size();i++){float dd=len(pos(i)-c);if(dd<radius){float t=1-dd*dd/(radius*radius);gIdx[s].push_back(i);gW[s].push_back(t*t);gBase[s].push_back(pos(i));touch(i);}}}}}
void Sculpt::endStroke(){stroking_=false;if(!undo_.empty()&&undo_.back().d.empty()&&undo_.back().c.empty())undo_.pop_back();}
bool Sculpt::undo(){if(undo_.empty())return false;auto u=undo_.back();undo_.pop_back();std::vector<uint32_t> a;markId_++;for(auto& pr:u.c)for(int k=0;k<4;k++)v[pr.first].c[k]=pr.second[k];
  for(auto& pr:u.d){v[pr.first].p[0]=pr.second.x;v[pr.first].p[1]=pr.second.y;v[pr.first].p[2]=pr.second.z;if(mark_[pr.first]!=markId_){mark_[pr.first]=markId_;a.push_back(pr.first);}
    for(uint32_t q=adjS[pr.first];q<adjS[pr.first+1];q++){uint32_t k=adjI[q];if(mark_[k]!=markId_){mark_[k]=markId_;a.push_back(k);}}}
  renorm(a);return true;}
void Sculpt::applyAt(Vec3 c,Vec3 n,int side){std::vector<uint32_t> aff;std::vector<float> wt;float r2=radius*radius;
  for(uint32_t i=0;i<v.size();i++){Vec3 q=pos(i);Vec3 dv=q-c;float d2=dot(dv,dv);if(d2<r2){float t=1-d2/r2;aff.push_back(i);wt.push_back(t*t);}}
  if(aff.empty())return;float k=strength*radius*0.30f;
  // stable average normal around the dab
  Vec3 an{0,0,0};for(size_t j=0;j<aff.size();j++)if(wt[j]>0.25f){an=an+Vec3{v[aff[j]].n[0],v[aff[j]].n[1],v[aff[j]].n[2]}*wt[j];}an=len(an)>1e-6f?norm(an):n;
  (void)side;
  if(tool==9||tool==10){for(size_t j=0;j<aff.size();j++){uint32_t i=aff[j];touchC(i);float t=std::min(1.f,wt[j]*(0.15f+strength*0.85f));const unsigned char* tc=tool==9?color:nullptr;unsigned char clay[4]={205,163,133,255};if(!tc)tc=clay;for(int q=0;q<4;q++)v[i].c[q]=(unsigned char)(v[i].c[q]+(tc[q]-(int)v[i].c[q])*t);}return;}
  if(tool==2){std::vector<Vec3> tg(aff.size());for(size_t j=0;j<aff.size();j++){uint32_t i=aff[j];Vec3 s{0,0,0};uint32_t cnt=adjS[i+1]-adjS[i];for(uint32_t q=adjS[i];q<adjS[i+1];q++)s=s+pos(adjI[q]);Vec3 avg=cnt?s*(1.f/cnt):pos(i);tg[j]=pos(i)+(avg-pos(i))*std::min(1.f,wt[j]*(0.25f+strength*0.7f));}
    for(size_t j=0;j<aff.size();j++)setPos(aff[j],tg[j]);}
  else for(size_t j=0;j<aff.size();j++){uint32_t i=aff[j];Vec3 p=pos(i);float w=wt[j];
    if(tool==0)p=p+an*(k*w);else if(tool==1)p=p-an*(k*w);
    else if(tool==3){Vec3 vn{v[i].n[0],v[i].n[1],v[i].n[2]};p=p+vn*(k*w);}
    else if(tool==4){Vec3 dv=c-p;dv=dv-an*dot(dv,an);p=p+dv*(w*strength*0.18f);}
    else if(tool==6){float h=dot(p-c,an);p=p-an*(h*w*std::min(1.f,strength*0.6f));}
    else if(tool==7){float dd=len(p-c);float rs=radius*0.35f;float w2=1-dd*dd/(rs*rs);if(w2<=0)continue;w2*=w2;p=p-an*(avgEdge_*1.6f*strength*w2);Vec3 dv=c-p;dv=dv-an*dot(dv,an);p=p+dv*(0.25f*w2);}
    else if(tool==8){uint32_t hh=i*2654435761u;hh^=hh>>13;hh*=0x5bd1e995u;hh^=hh>>15;float rnd=(hh&0xffff)/65535.f;if(rnd>0.55f)p=p-an*(avgEdge_*0.30f*strength*w*(rnd-0.55f)/0.45f);else if(rnd<0.15f)p=p+an*(avgEdge_*0.12f*strength*w);}
    setPos(i,p);}
  std::vector<uint32_t> a;markId_++;for(uint32_t i:aff){if(mark_[i]!=markId_){mark_[i]=markId_;a.push_back(i);}for(uint32_t q=adjS[i];q<adjS[i+1];q++){uint32_t kk=adjI[q];if(mark_[kk]!=markId_){mark_[kk]=markId_;a.push_back(kk);}}}renorm(a);}
void Sculpt::dab(Vec3 hit,Vec3 o,Vec3 d){if(!stroking_)return;
  if(tool==5){float den=dot(d,grabDir_);if(std::fabs(den)<1e-5f)return;float t=dot(grabStart_-o,grabDir_)/den;Vec3 delta=o+d*t-grabStart_;std::vector<uint32_t> a;markId_++;
    for(int s=0;s<(sym?2:1);s++){Vec3 dl=s?Vec3{-delta.x,delta.y,delta.z}:delta;for(size_t j=0;j<gIdx[s].size();j++){uint32_t i=gIdx[s][j];Vec3 p=gBase[s][j]+dl*gW[s][j];v[i].p[0]=p.x;v[i].p[1]=p.y;v[i].p[2]=p.z;
        if(mark_[i]!=markId_){mark_[i]=markId_;a.push_back(i);}for(uint32_t q=adjS[i];q<adjS[i+1];q++){uint32_t kk=adjI[q];if(mark_[kk]!=markId_){mark_[kk]=markId_;a.push_back(kk);}}}}
    renorm(a);return;}
  if(len(hit-lastDab_)<radius*(tool==9||tool==10?0.05f:0.12f)&&strokeId_>0&&lastDab_.x!=hit.x)return;lastDab_=hit;
  applyAt(hit,{0,1,0},0);if(sym&&std::fabs(hit.x)>radius*0.25f)applyAt({-hit.x,hit.y,hit.z},{0,1,0},1);}
bool Sculpt::save(const std::string& path) const{std::ofstream f(path,std::ios::binary);if(!f)return false;uint32_t m=0x32435553,nv=(uint32_t)v.size(),ni=(uint32_t)idx.size();f.write((char*)&m,4);f.write((char*)&nv,4);f.write((char*)&ni,4);
  for(auto& s:v)f.write((const char*)s.p,12);f.write((const char*)idx.data(),ni*4);for(auto& s:v)f.write((const char*)s.c,4);return (bool)f;}
bool Sculpt::load(const std::string& path){std::ifstream f(path,std::ios::binary);if(!f)return false;uint32_t m,nv,ni;f.read((char*)&m,4);f.read((char*)&nv,4);f.read((char*)&ni,4);bool hasC=m==0x32435553;if(!f||(m!=0x4c435553&&!hasC)||nv>2000000||ni>6000000)return false;
  std::vector<Vec3> P(nv);for(auto& p:P){float q[3];f.read((char*)q,12);p={q[0],q[1],q[2]};}std::vector<uint32_t> I(ni);f.read((char*)I.data(),ni*4);if(!f)return false;
  v.clear();idx=I;v.resize(nv);for(uint32_t i=0;i<nv;i++){setSV(v[i],P[i]);if(hasC)f.read((char*)v[i].c,4);}undo_.clear();topo();return true;}
void Sculpt::buildModel(Renderer* r,Model& out) const{out=Model();out.name="Sculpt";out.nodeName={"root"};out.parent={-1};out.rest.assign(1,TRS());out.joints={0};out.ibm={Mat4::identity()};
  ModelMat mm;mm.kind=10;mm.col[0]=mm.col[1]=mm.col[2]=1;mm.col[3]=1;if(r){unsigned char wh[4]={255,255,255,255};mm.tex=r->createTexture(wh,1,1);}out.mats.push_back(mm);
  ModelPrim mp;mp.mat=0;mp.cv=v;mp.ci=idx;if(r)mp.mesh=r->uploadSkin(v,idx);out.prims.push_back(std::move(mp));out.fit=1;out.foot=0;out.height=std::max(0.5f,maxY());out.radius=2;out.autoRig=false;}
}
