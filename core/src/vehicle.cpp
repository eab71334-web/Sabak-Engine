// Sabak Core - procedural realistic car and horse generators (lofted superellipse sections, rigged, with built-in animations).
#include <cmath>
#include <algorithm>
#include <functional>
#include <map>
#include <array>
#include "model.h"
namespace sk {
namespace {
struct VB{std::vector<SkinVertex> v;std::vector<uint32_t> idx;};
struct Ring{Vec3 c;float rx,ry;};
static Vec3 nrm(Vec3 a){float l=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);return l<1e-9f?Vec3{0,1,0}:Vec3{a.x/l,a.y/l,a.z/l};}
static Vec3 crs(Vec3 a,Vec3 b){return Vec3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static float sstep(float x){x=std::min(1.f,std::max(0.f,x));return x*x*(3-2*x);}
using SkinFn=std::function<void(SkinVertex&,int ring,int rings)>;
static void setW(SkinVertex& s,int b0,float w0,int b1=0,float w1=0){s.j[0]=(unsigned char)b0;s.j[1]=(unsigned char)b1;s.j[2]=s.j[3]=0;s.w[0]=w0;s.w[1]=w1;s.w[2]=s.w[3]=0;}
// Lofts a closed superellipse tube through rings; side = fixed in-plane axis, the other axis is perpendicular to the path tangent.
static void loft(VB& m,const std::vector<Ring>& rs,Vec3 side,float expo,const SkinFn& fn,int S=22,bool caps=true){
  int K=(int)rs.size();if(K<2)return;uint32_t base=(uint32_t)m.v.size();
  for(int k=0;k<K;k++){Vec3 T=nrm(k==0?rs[1].c-rs[0].c:k==K-1?rs[K-1].c-rs[K-2].c:rs[k+1].c-rs[k-1].c);Vec3 U=nrm(crs(T,side));Vec3 sd=nrm(crs(U,T));
    for(int i=0;i<S;i++){float th=6.2831853f*i/S;float c=std::cos(th),s=std::sin(th);float cx=(c<0?-1.f:1.f)*std::pow(std::fabs(c),2.f/expo),cy=(s<0?-1.f:1.f)*std::pow(std::fabs(s),2.f/expo);
      SkinVertex v{};Vec3 p=rs[k].c+sd*(rs[k].rx*cx)+U*(rs[k].ry*cy);v.p[0]=p.x;v.p[1]=p.y;v.p[2]=p.z;v.uv[0]=p.z*0.5f+p.x*0.3f;v.uv[1]=p.y*0.5f+p.x*0.2f;setW(v,0,1);fn(v,k,K);m.v.push_back(v);}}
  auto P=[&](uint32_t i){return Vec3{m.v[i].p[0],m.v[i].p[1],m.v[i].p[2]};};
  auto addTri=[&](uint32_t a,uint32_t b,uint32_t c,Vec3 ref){Vec3 n=crs(P(b)-P(a),P(c)-P(a));Vec3 ce=(P(a)+P(b)+P(c))*(1.f/3);Vec3 r=ce-ref;if(n.x*r.x+n.y*r.y+n.z*r.z<0)std::swap(b,c);m.idx.push_back(a);m.idx.push_back(b);m.idx.push_back(c);};
  for(int k=0;k+1<K;k++)for(int i=0;i<S;i++){uint32_t a=base+k*S+i,b=base+k*S+(i+1)%S,c=base+(k+1)*S+i,d=base+(k+1)*S+(i+1)%S;Vec3 ref=(rs[k].c+rs[k+1].c)*0.5f;addTri(a,c,b,ref);addTri(b,c,d,ref);}
  if(caps)for(int e=0;e<2;e++){int k=e?K-1:0;uint32_t cv=(uint32_t)m.v.size();SkinVertex v{};v.p[0]=rs[k].c.x;v.p[1]=rs[k].c.y;v.p[2]=rs[k].c.z;setW(v,0,1);fn(v,k,K);m.v.push_back(v);
    Vec3 ref=e?rs[K-2].c:rs[1].c;for(int i=0;i<S;i++)addTri(cv,base+k*S+i,base+k*S+(i+1)%S,ref);}}
static void box(VB& m,Vec3 c,Vec3 h,float rotX,const SkinFn& fn){float cs=std::cos(rotX),sn=std::sin(rotX);
  Vec3 corner[8];for(int i=0;i<8;i++){Vec3 q{(i&1?1.f:-1.f)*h.x,(i&2?1.f:-1.f)*h.y,(i&4?1.f:-1.f)*h.z};corner[i]=c+Vec3{q.x,q.y*cs-q.z*sn,q.y*sn+q.z*cs};}
  static const int F[6][4]={{0,1,3,2},{4,6,7,5},{0,2,6,4},{1,5,7,3},{0,4,5,1},{2,3,7,6}};
  for(int f=0;f<6;f++){uint32_t b=(uint32_t)m.v.size();for(int k=0;k<4;k++){SkinVertex v{};v.p[0]=corner[F[f][k]].x;v.p[1]=corner[F[f][k]].y;v.p[2]=corner[F[f][k]].z;v.uv[0]=(float)(k&1);v.uv[1]=(float)(k>>1);setW(v,0,1);fn(v,0,1);m.v.push_back(v);}
    Vec3 n=crs(Vec3{m.v[b+1].p[0]-m.v[b].p[0],m.v[b+1].p[1]-m.v[b].p[1],m.v[b+1].p[2]-m.v[b].p[2]},Vec3{m.v[b+2].p[0]-m.v[b].p[0],m.v[b+2].p[1]-m.v[b].p[1],m.v[b+2].p[2]-m.v[b].p[2]});
    Vec3 fc=(corner[F[f][0]]+corner[F[f][2]])*0.5f-c;bool fl=n.x*fc.x+n.y*fc.y+n.z*fc.z<0;
    uint32_t t[6]={b,b+1,b+2,b,b+2,b+3};if(fl)for(int k=0;k<6;k+=3)std::swap(t[k+1],t[k+2]);for(int k=0;k<6;k++)m.idx.push_back(t[k]);}}
// ribbon (hair strand / mane flap): polyline with half-width, double sided
static void ribbon(VB& m,const std::vector<Vec3>& pts,const std::vector<float>& wd,Vec3 side,const SkinFn& fn){uint32_t b=(uint32_t)m.v.size();
  for(size_t i=0;i<pts.size();i++)for(int s=-1;s<=1;s+=2){SkinVertex v{};Vec3 p=pts[i]+side*(wd[i]*s);v.p[0]=p.x;v.p[1]=p.y;v.p[2]=p.z;v.uv[0]=(float)i/pts.size();v.uv[1]=s>0;setW(v,0,1);fn(v,(int)i,(int)pts.size());m.v.push_back(v);}
  for(size_t i=0;i+1<pts.size();i++){uint32_t a=b+i*2;m.idx.insert(m.idx.end(),{a,a+1,a+2,a+1,a+3,a+2});}}
static void normalsOf(VB& m){std::vector<Vec3> nn(m.v.size(),Vec3{0,0,0});auto P=[&](uint32_t k){return Vec3{m.v[k].p[0],m.v[k].p[1],m.v[k].p[2]};};
  for(size_t i=0;i+2<m.idx.size();i+=3){Vec3 a=P(m.idx[i]),b=P(m.idx[i+1]),c=P(m.idx[i+2]);Vec3 f=crs(b-a,c-a);for(int k=0;k<3;k++)nn[m.idx[i+k]]=nn[m.idx[i+k]]+f;}
  for(size_t i=0;i<m.v.size();i++){Vec3 n=nrm(nn[i]);m.v[i].n[0]=n.x;m.v[i].n[1]=n.y;m.v[i].n[2]=n.z;}}
struct Asm{Model* out;Renderer* r;unsigned wt;std::function<void(SkinVertex&)> post;
  void add(VB& m,unsigned col,int kind){if(m.idx.empty())return;if(post)for(auto& v:m.v)post(v);normalsOf(m);ModelMat mm;mm.tex=wt;mm.kind=kind;mm.col[0]=((col>>16)&255)/255.f;mm.col[1]=((col>>8)&255)/255.f;mm.col[2]=(col&255)/255.f;mm.col[3]=1;int mi=(int)out->mats.size();out->mats.push_back(mm);
    ModelPrim mp;mp.mesh=r->uploadSkin(m.v,m.idx);mp.mat=mi;mp.cv=m.v;mp.ci=m.idx;out->prims.push_back(std::move(mp));}};
static void quatX(float a,float* q){q[0]=std::sin(a*0.5f);q[1]=0;q[2]=0;q[3]=std::cos(a*0.5f);}
static unsigned parseHex(const std::string& s,unsigned def){if(s.size()<6)return def;try{return (unsigned)std::stoul(s.substr(0,6),nullptr,16);}catch(...){return def;}}
static unsigned shade(unsigned c,float k){auto ch=[&](int sh){return (unsigned)std::min(255.f,((c>>sh)&255)*k);};return (ch(16)<<16)|(ch(8)<<8)|ch(0);}
static void skeleton(Model& o,const std::vector<std::string>& names,const std::vector<int>& par,const std::vector<Vec3>& head){size_t n=names.size();o.nodeName=names;o.parent=par;o.rest.resize(n);
  for(size_t i=0;i<n;i++){TRS t;int p=par[i];t.t=head[i]-(p>=0?head[p]:Vec3{0,0,0});o.rest[i]=t;o.joints.push_back((int)i);o.ibm.push_back(Mat4::translate({-head[i].x,-head[i].y,-head[i].z}));}}
}


// ---------------------------------------------------------------- Catmull-Clark subdivision with creases (hard-surface modelling)
namespace {
typedef unsigned long long ull;
static ull ekey(int a,int b){if(a>b)std::swap(a,b);return ((ull)a<<32)|(unsigned)b;}
struct SubMesh{std::vector<Vec3> V;std::vector<std::vector<int>> F;std::map<ull,float> crease;};
static SubMesh ccOnce(const SubMesh& m){
  size_t nv=m.V.size(),nf=m.F.size();SubMesh o;o.V=m.V;
  std::vector<Vec3> fp(nf);for(size_t f=0;f<nf;f++){Vec3 s{0,0,0};for(int v:m.F[f])s=s+m.V[v];fp[f]=s*(1.f/m.F[f].size());}
  struct E{int a,b;int f[2]={-1,-1};int id=-1;};std::map<ull,E> em;
  for(size_t f=0;f<nf;f++){auto& F=m.F[f];for(size_t i=0;i<F.size();i++){int a=F[i],b=F[(i+1)%F.size()];ull k=ekey(a,b);auto it=em.find(k);if(it==em.end()){E e;e.a=a;e.b=b;e.f[0]=(int)f;em[k]=e;}else it->second.f[1]=(int)f;}}
  for(size_t f=0;f<nf;f++)o.V.push_back(fp[f]);                 // face points at nv..nv+nf-1
  std::vector<Vec3> ep;int id=0;
  for(auto& kv:em){E& e=kv.second;e.id=(int)(nv+nf)+id++;Vec3 mid=(m.V[e.a]+m.V[e.b])*0.5f;float sh=0;auto ci=m.crease.find(kv.first);if(ci!=m.crease.end())sh=ci->second;bool bd=e.f[1]<0;Vec3 sm=bd?mid:(m.V[e.a]+m.V[e.b]+fp[e.f[0]]+fp[e.f[1]])*0.25f;
    float t=bd?1.f:std::min(1.f,sh);o.V.push_back(sm*(1-t)+mid*t);}
  // vertex points
  std::vector<std::vector<ull>> inc(nv);for(auto& kv:em){inc[kv.second.a].push_back(kv.first);inc[kv.second.b].push_back(kv.first);}
  std::vector<std::vector<int>> vf(nv);for(size_t f=0;f<nf;f++)for(int v:m.F[f])vf[v].push_back((int)f);
  for(size_t v=0;v<nv;v++){Vec3 P=m.V[v];int n=(int)inc[v].size();std::vector<int> sharp;float shMax=0;
    for(ull k:inc[v]){E& e=em[k];auto ci=m.crease.find(k);float sh=ci!=m.crease.end()?ci->second:0;if(e.f[1]<0)sh=1e9f;if(sh>0){sharp.push_back(e.a==(int)v?e.b:e.a);shMax=std::max(shMax,sh);}}
    Vec3 np=P;
    if(sharp.size()>=3)np=P;
    else if(sharp.size()==2){Vec3 sm=(P*6+m.V[sharp[0]]+m.V[sharp[1]])*(1.f/8);float t=std::min(1.f,shMax);Vec3 F{0,0,0},R{0,0,0};for(int f:vf[v])F=F+fp[f];F=F*(1.f/vf[v].size());for(ull k:inc[v]){E& e=em[k];R=R+(m.V[e.a]+m.V[e.b])*0.5f;}R=R*(1.f/n);Vec3 sm2=(F+R*2+P*(float)(n-3))*(1.f/n);np=sm2*(1-t)+sm*t;}
    else{Vec3 F{0,0,0},R{0,0,0};for(int f:vf[v])F=F+fp[f];F=F*(1.f/vf[v].size());for(ull k:inc[v]){E& e=em[k];R=R+(m.V[e.a]+m.V[e.b])*0.5f;}R=R*(1.f/n);np=(F+R*2+P*(float)(n-3))*(1.f/n);}
    o.V[v]=np;}
  for(size_t f=0;f<nf;f++){auto& F=m.F[f];size_t n=F.size();for(size_t i=0;i<n;i++){int a=F[i],b=F[(i+1)%n],z=F[(i+n-1)%n];o.F.push_back({a,em[ekey(a,b)].id,(int)(nv+f),em[ekey(z,a)].id});}}
  for(auto& kv:m.crease){if(kv.second<=1.f)continue;E& e=em[kv.first];o.crease[ekey(e.a,e.id)]=kv.second-1;o.crease[ekey(e.b,e.id)]=kv.second-1;}
  return o;}
static SubMesh ccSub(SubMesh m,int levels){for(int i=0;i<levels;i++)m=ccOnce(m);return m;}
// triangulate into a VB with outward-facing winding relative to an axis (x=0,y=ay,z=same)
static void ccToVB(VB& vb,const SubMesh& m,float ay,float insetGap=0.f,Vec3 shift={0,0,0},float normalOff=0.f,const SkinFn* fn=nullptr,VB* seam=nullptr,float seamW=0.004f){
  uint32_t b=(uint32_t)vb.v.size();std::vector<Vec3> P=m.V;std::vector<uint32_t> I;
  for(auto& f:m.F){for(size_t i=1;i+1<f.size();i++){int a=f[0],c=f[i],d=f[i+1];Vec3 n=crs(P[c]-P[a],P[d]-P[a]);Vec3 ce=(P[a]+P[c]+P[d])*(1.f/3);Vec3 r=ce-Vec3{0,ay,ce.z};bool fl=dot(n,r)<0;I.push_back(a);I.push_back(fl?d:c);I.push_back(fl?c:d);}}
  if(insetGap>0||normalOff!=0||seam){ // vertex normals, boundary inset toward centroid, optional normal offset
    std::vector<Vec3> nn(P.size(),Vec3{0,0,0});std::map<ull,int> ec;
    for(size_t i=0;i+2<I.size();i+=3){Vec3 f=crs(P[I[i+1]]-P[I[i]],P[I[i+2]]-P[I[i]]);for(int k=0;k<3;k++){nn[I[i+k]]=nn[I[i+k]]+f;ec[ekey(I[i+k],I[i+(k+1)%3])]++;}}
    std::vector<char> bd(P.size(),0);for(auto& kv:ec)if(kv.second==1){bd[kv.first>>32]=1;bd[kv.first&0xffffffff]=1;}
    Vec3 cen{0,0,0};std::vector<char> used(P.size(),0);int cn=0;for(uint32_t i:I)if(!used[i]){used[i]=1;cen=cen+P[i];cn++;}cen=cen*(1.f/std::max(1,cn));
    if(seam){for(auto& kv:ec)if(kv.second==1){int a=(int)(kv.first>>32),b=(int)(kv.first&0xffffffff);Vec3 na=nrm(nn[a]),nb=nrm(nn[b]);Vec3 d=nrm(P[b]-P[a]);Vec3 sa=nrm(crs(d,na))*seamW,sb=nrm(crs(d,nb))*seamW;Vec3 pa=P[a]+na*(normalOff+0.0012f),pb=P[b]+nb*(normalOff+0.0012f);uint32_t sbase=(uint32_t)seam->v.size();Vec3 q[4]={pa-sa,pa+sa,pb+sb,pb-sb};Vec3 nq[4]={na,na,nb,nb};for(int k=0;k<4;k++){SkinVertex v{};v.p[0]=q[k].x+shift.x;v.p[1]=q[k].y+shift.y;v.p[2]=q[k].z+shift.z;v.n[0]=nq[k].x;v.n[1]=nq[k].y;v.n[2]=nq[k].z;v.uv[0]=v.p[2]*0.5f;v.uv[1]=v.p[1]*0.5f;setW(v,0,1);seam->v.push_back(v);}seam->idx.insert(seam->idx.end(),{sbase,sbase+1,sbase+2,sbase,sbase+2,sbase+3});}}
    std::vector<Vec3> NP=P;for(size_t i=0;i<P.size();i++){if(!used[i])continue;Vec3 n=nrm(nn[i]);Vec3 p=P[i]+n*normalOff;if(bd[i]&&insetGap>0){Vec3 d=cen-P[i];d=d-n*dot(d,n);p=p+nrm(d)*insetGap;}NP[i]=p;}P=NP;}
  for(size_t i=0;i<P.size();i++){SkinVertex v{};Vec3 p=P[i]+shift;v.p[0]=p.x;v.p[1]=p.y;v.p[2]=p.z;v.uv[0]=p.z*0.5f+p.x*0.3f;v.uv[1]=p.y*0.5f+p.x*0.2f;setW(v,0,1);if(fn)(*fn)(v,0,1);vb.v.push_back(v);}
  for(uint32_t i:I)vb.idx.push_back(i+b);}
// car body cage: ring stations along z, profile around the cross-section, wheel arches cut by lifting the lower profile, panels split by region
struct CarPanels{VB panels,under,seams,cabin,glass,liner;};
static void carBody(CarPanels& cp,float WYc,float WR,float zF,float zR,const SkinFn& fn,int levels){
  struct St{float z,yb,yBelt,yDeck,hw;};
  std::vector<float> zs={-2.25f,-2.20f,-2.10f,-2.02f,-1.85f,-1.72f,-1.60f,-1.45f,-1.30f,-1.15f,-1.02f,-0.88f,-0.60f,-0.32f,0.10f,0.40f,0.68f,0.85f,0.98f,1.08f,1.25f,1.38f,1.50f,1.62f,1.75f,2.00f,2.10f,2.19f,2.25f};
  auto sect=[&](float z)->St{ // interpolate design curves
    auto L=[&](std::vector<std::pair<float,float>> k){if(z<=k.front().first)return k.front().second;for(size_t i=0;i+1<k.size();i++)if(z<=k[i+1].first){float t=(z-k[i].first)/(k[i+1].first-k[i].first);t=t*t*(3-2*t);return k[i].second*(1-t)+k[i+1].second*t;}return k.back().second;};
    St s;s.z=z;
    s.yb=L({{-2.25f,.50f},{-2.2f,.40f},{-2.0f,.33f},{-1.6f,.30f},{1.7f,.30f},{2.1f,.34f},{2.25f,.46f}});
    s.yBelt=L({{-2.25f,.66f},{-2.2f,.74f},{-2.0f,.88f},{-1.2f,.94f},{0.7f,.95f},{1.4f,.92f},{2.0f,.80f},{2.25f,.64f}});
    s.yDeck=L({{-2.25f,.72f},{-2.2f,.82f},{-2.0f,.95f},{-1.3f,.99f},{-0.5f,1.0f},{0.9f,1.0f},{1.5f,.97f},{2.0f,.84f},{2.25f,.66f}});
    s.hw=L({{-2.25f,.50f},{-2.2f,.76f},{-2.05f,.88f},{-1.6f,.935f},{1.5f,.935f},{2.0f,.90f},{2.2f,.78f},{2.25f,.52f}});return s;};
  const int K=20;SubMesh m;std::vector<std::array<int,K>> idx;
  auto archY=[&](float z){float y=0;for(float zw:{zF,zR}){float d=z-zw;float R=WR+0.07f;if(std::fabs(d)<R)y=std::max(y,WYc+std::sqrt(R*R-d*d));}return y;};
  for(float z:zs){St s=sect(z);float ay=archY(z);float ymid=(s.yb+s.yBelt)*0.5f;
    struct Pt{float u,y;};float yb=s.yb,ya=std::max(0.f,ay);
    auto lift=[&](float y,float minY){return ya>0?std::max(y,minY):y;};
    Pt P[11]={{0,yb},{0.78f,yb},{0.96f,lift(yb+0.03f,ya-0.02f)},{1.0f,lift(yb+0.12f,ya)},{1.012f,lift(ymid+0.02f,ya+0.04f)},{1.0f,std::max(s.yBelt-0.13f,ya+0.08f)},{0.975f,s.yBelt-0.035f},{0.90f,s.yBelt+0.005f},{0.60f,s.yDeck-0.005f},{0.30f,s.yDeck+0.012f},{0,s.yDeck+0.018f}};
    if(ya>0){P[1].u=0.70f;P[1].y=std::max(yb,ya-0.18f);}
    std::array<int,K> row;int n=0;
    for(int i=0;i<=10;i++){row[n++]=(int)m.V.size();m.V.push_back({P[i].u*s.hw,P[i].y,z});}
    for(int i=9;i>=1;i--){row[n++]=(int)m.V.size();m.V.push_back({-P[i].u*s.hw,P[i].y,z});}
    idx.push_back(row);}
  std::vector<int> panelOf;
  auto classify=[&](float z,int j,float y)->int{
    if(z>2.0f||z<-2.02f)return 6;
    bool top=(j>=7&&j<=12);if(top&&j>=8&&j<=11&&z>-1.12f&&z<0.98f)return -1;if(top){if(z>0.98f)return 1;if(z<-1.12f)return 2;return 0;}
    bool dR=(j>=3&&j<=6),dL=(j>=13&&j<=16);if((dR||dL)&&z>-0.32f&&z<0.68f)return dR?3:4;
    if(j>=2&&j<=6)return 5;if(j>=13&&j<=17)return 5;(void)y;return 0;};
  std::vector<std::vector<int>> faceIdx;
  for(size_t s=0;s+1<zs.size();s++)for(int j=0;j<K;j++){int j2=(j+1)%K;m.F.push_back({idx[s][j],idx[s][j2],idx[s+1][j2],idx[s+1][j]});float zc=(zs[s]+zs[s+1])*0.5f;int pc=classify(zc,j,0);if(pc<0){m.F.pop_back();continue;}panelOf.push_back(pc);}
  {std::vector<int> a(idx.front().begin(),idx.front().end()),b(idx.back().rbegin(),idx.back().rend());m.F.push_back(a);panelOf.push_back(6);m.F.push_back(b);panelOf.push_back(6);}
  // creases: shoulder and sill lines
  for(size_t s=0;s+1<zs.size();s++)for(int j:{2,6,17}){int a=idx[s][j],b=idx[s+1][j];m.crease[ekey(a,b)]=1.f;}
  // whole body underlay (dark) and separate panels with gaps
  SubMesh full=ccSub(m,levels);ccToVB(cp.under,full,0.7f,0,{0,0,0},-0.016f);
  for(int pid=0;pid<=6;pid++){SubMesh pm;std::map<int,int> remap;for(size_t f=0;f<m.F.size();f++)if(panelOf[f]==pid){std::vector<int> nf;for(int v:m.F[f]){auto it=remap.find(v);if(it==remap.end()){remap[v]=(int)pm.V.size();pm.V.push_back(m.V[v]);it=remap.find(v);}nf.push_back(it->second);}pm.F.push_back(nf);}
    if(pm.F.empty())continue;for(auto& kv:m.crease){int a=kv.first>>32,b=kv.first&0xffffffff;auto ia=remap.find(a),ib=remap.find(b);if(ia!=remap.end()&&ib!=remap.end())pm.crease[ekey(ia->second,ib->second)]=kv.second;}
    SubMesh sm=ccSub(pm,levels);ccToVB(cp.panels,sm,0.7f,0.f,{0,0,0},0.f,nullptr,&cp.seams,0.0025f);}
  (void)fn;}

static void carInterior(Asm& A,const SkinFn& root){
  VB seat,trimL,dash,glow,wheel,metal,floorB,carpet;
  auto cushion=[&](float x,float zc,float rx){loft(seat,{{{x,0.545f,zc-0.28f},rx*0.7f,0.03f},{{x,0.57f,zc-0.24f},rx,0.075f},{{x,0.585f,zc+0.0f},rx,0.085f},{{x,0.57f,zc+0.22f},rx*0.95f,0.07f},{{x,0.54f,zc+0.30f},rx*0.7f,0.035f}},{1,0,0},2.6f,root,22);};
  auto back=[&](float x,float zc,float rx,float top){loft(seat,{{{x,0.60f,zc-0.27f},rx*0.95f,0.07f},{{x,0.80f,zc-0.31f},rx,0.075f},{{x,top-0.06f,zc-0.36f},rx*0.98f,0.065f},{{x,top,zc-0.375f},rx*0.8f,0.04f}},{1,0,0},2.6f,root,22);
    loft(seat,{{{x,top+0.03f,zc-0.385f},0.075f,0.035f},{{x,top+0.12f,zc-0.395f},0.095f,0.055f},{{x,top+0.22f,zc-0.40f},0.085f,0.045f}},{1,0,0},2.4f,root,18);
    loft(trimL,{{{x-rx*0.0f,0.63f,zc-0.285f},rx*0.62f,0.012f},{{x,0.86f,zc-0.325f},rx*0.62f,0.012f},{{x,top-0.10f,zc-0.355f},rx*0.58f,0.012f}},{1,0,0},2.0f,root,14);};
  for(int sd=-1;sd<=1;sd+=2){cushion(sd*0.36f,-0.12f,0.225f);back(sd*0.36f,-0.12f,0.215f,1.03f);}
  cushion(0,-0.80f,0.56f);back(0,-0.80f,0.55f,1.0f);
  A.add(seat,0x4a3226,11);A.add(trimL,0x2a1c15,11);
  // dashboard + instrument hood + centre console
  loft(dash,{{{-0.72f,0.84f,0.80f},0.15f,0.07f},{{-0.60f,0.90f,0.79f},0.18f,0.10f},{{0,0.93f,0.78f},0.19f,0.11f},{{0.60f,0.90f,0.79f},0.18f,0.10f},{{0.72f,0.84f,0.80f},0.15f,0.07f}},{0,0,1},2.4f,root,22);
  loft(dash,{{{0.36f-0.17f,1.00f,0.73f},0.03f,0.03f},{{0.36f,1.02f,0.73f},0.18f,0.045f},{{0.36f+0.17f,1.00f,0.73f},0.03f,0.03f}},{0,0,1},2.0f,root,14);
  loft(dash,{{{0,0.60f,-0.30f},0.10f,0.06f},{{0,0.68f,0.15f},0.12f,0.075f},{{0,0.72f,0.60f},0.13f,0.08f}},{1,0,0},2.4f,root,18);
  A.add(dash,0x1c1a19,11);
  // gear stick
  VB gs;loft(gs,{{{0,0.72f,0.12f},0.012f,0.012f},{{0,0.86f,0.14f},0.012f,0.012f}},{1,0,0},2.0f,root,10);loft(gs,{{{0,0.86f,0.14f},0.001f,0.001f},{{0,0.875f,0.14f},0.03f,0.03f},{{0,0.915f,0.14f},0.028f,0.028f},{{0,0.93f,0.14f},0.001f,0.001f}},{1,0,0},2.0f,root,14);A.add(gs,0x9a9ea6,7);
  // steering wheel (torus), spokes, hub and column
  Vec3 hub{0.36f,0.92f,0.40f},n=nrm(Vec3{0,0.55f,-0.83f}),e1{1,0,0},e2=nrm(crs(n,e1));float R=0.185f;
  {std::vector<Ring> rs;for(int k=0;k<=28;k++){float th=6.2831853f*k/28;Vec3 c=hub+e1*(std::cos(th)*R)+e2*(std::sin(th)*R);rs.push_back({c,0.016f,0.016f});}loft(wheel,rs,n,2.0f,root,12);}
  for(int sp=0;sp<3;sp++){float th=sp==0?3.1416f:sp==1?0.f:-1.5708f;Vec3 tip=hub+e1*(std::cos(th)*R*0.96f)+e2*(std::sin(th)*R*0.96f);loft(wheel,{{hub,0.016f,0.012f},{(hub+tip)*0.5f,0.014f,0.010f},{tip,0.016f,0.012f}},n,2.0f,root,10);}
  loft(wheel,{{hub-n*0.015f,0.001f,0.001f},{hub-n*0.012f,0.05f,0.05f},{hub+n*0.02f,0.05f,0.05f},{hub+n*0.026f,0.001f,0.001f}},n,2.0f,root,18);
  loft(wheel,{{hub-n*0.02f,0.03f,0.03f},{{0.36f,0.84f,0.62f},0.035f,0.035f}},{1,0,0},2.0f,root,12);
  A.add(wheel,0x141414,11);
  // screens and ambient light strips (emissive)
  box(glow,{0.36f,1.005f,0.71f},{0.115f,0.035f,0.003f},-0.35f,root);
  VB scr;box(scr,{0.0f,0.99f,0.80f},{0.13f,0.07f,0.004f},-0.5f,root);A.add(scr,0x2b8fd6,12);A.add(glow,0xffb04a,12);
  VB amb;box(amb,{0,0.835f,0.66f},{0.62f,0.004f,0.004f},0,root);box(amb,{0.66f,0.55f,0.0f},{0.004f,0.004f,0.85f},0,root);box(amb,{-0.66f,0.55f,0.0f},{0.004f,0.004f,0.85f},0,root);A.add(amb,0x5b8cff,12);
  // floor carpet
  box(carpet,{0,0.40f,-0.10f},{0.66f,0.01f,1.05f},0,root);A.add(carpet,0x1a1a1c,2);
}

static void carCabin(CarPanels& cp,int levels){
  const int K=13;SubMesh m;std::vector<std::array<int,K>> idx;
  std::vector<float> zs={-1.20f,-1.08f,-0.95f,-0.80f,-0.55f,-0.32f,-0.10f,0.15f,0.32f,0.45f,0.62f,0.80f,0.95f,1.05f};
  auto roofY=[&](float z){std::vector<std::pair<float,float>> k={{-1.20f,1.00f},{-1.05f,1.20f},{-0.90f,1.34f},{-0.70f,1.425f},{-0.4f,1.455f},{0.2f,1.46f},{0.45f,1.42f},{0.62f,1.30f},{0.80f,1.15f},{0.95f,1.04f},{1.05f,1.0f}};if(z<=k.front().first)return k.front().second;for(size_t i=0;i+1<k.size();i++)if(z<=k[i+1].first){float t=(z-k[i].first)/(k[i+1].first-k[i].first);t=t*t*(3-2*t);return k[i].second*(1-t)+k[i+1].second*t;}return k.back().second;};
  auto hwAt=[&](float z){float e=std::min(z+1.20f,1.05f-z);float t=std::max(0.f,std::min(1.f,e/0.25f));return 0.40f+0.40f*t;};
  const float T[7]={0,0.10f,0.36f,0.76f,0.965f,0.995f,1.0f},U[7]={.90f,.87f,.82f,.74f,.55f,.28f,0.f};
  for(float z:zs){float y0=0.985f,yr=roofY(z),hw=hwAt(z);float X[7],Y[7];for(int i=0;i<7;i++){X[i]=U[i]*hw;Y[i]=y0+(yr-y0)*T[i];}
    std::array<int,K> row;int n=0;for(int i=0;i<7;i++){row[n++]=(int)m.V.size();m.V.push_back({X[i],Y[i],z});}for(int i=5;i>=0;i--){row[n++]=(int)m.V.size();m.V.push_back({-X[i],Y[i],z});}idx.push_back(row);}
  std::vector<int> panelOf;
  auto cls=[&](float z,int j)->int{bool win=(j==1||j==2||j==9||j==10);bool top=(j>=4&&j<=7);
    if(win&&z>-0.80f&&z<0.32f&&!(z>-0.32f&&z<-0.10f))return 1;if(top&&z>0.45f)return 1;if(top&&z<-0.78f)return 1;return 0;};
  for(size_t s=0;s+1<zs.size();s++)for(int j=0;j<K;j++){int j2=(j+1)%K;m.F.push_back({idx[s][j],idx[s][j2],idx[s+1][j2],idx[s+1][j]});panelOf.push_back(cls((zs[s]+zs[s+1])*0.5f,j));}
  {std::vector<int> a(idx.front().begin(),idx.front().end()),b(idx.back().rbegin(),idx.back().rend());m.F.push_back(a);panelOf.push_back(0);m.F.push_back(b);panelOf.push_back(0);}
  for(int pid=0;pid<2;pid++){SubMesh pm;std::map<int,int> remap;for(size_t f=0;f<m.F.size();f++)if(panelOf[f]==pid){std::vector<int> nf;for(int v:m.F[f]){auto it=remap.find(v);if(it==remap.end()){remap[v]=(int)pm.V.size();pm.V.push_back(m.V[v]);it=remap.find(v);}nf.push_back(it->second);}pm.F.push_back(nf);}
    if(pm.F.empty())continue;SubMesh sm=ccSub(pm,levels);if(pid==0){ccToVB(cp.cabin,sm,1.2f,0.f,{0,0,0},0.f,nullptr,&cp.seams,0.0022f);ccToVB(cp.liner,sm,1.2f,0.f,{0,0,0},-0.014f);}else ccToVB(cp.glass,sm,1.2f,0.f,{0,0,0},-0.006f,nullptr,&cp.seams,0.0022f);}
}
}
// ---------------------------------------------------------------- CAR
bool buildCar(Renderer& r,const std::string& spec,Model& out){
  bool m4=spec.rfind("m4",0)==0;std::string cs=m4?(spec.size()>3?spec.substr(3):std::string()):spec;unsigned paint=parseHex(cs,m4?0x2a4a8c:0xb3202a);std::vector<unsigned char> white{255,255,255,255};
  out=Model();out.name=m4?"M4 Coupe":"Car";Asm A{&out,&r,r.createTexture(white.data(),1,1),nullptr};
  const float sx=m4?1.03f:1.f,sz=m4?1.064f:1.f,sy=m4?0.86f:1.f;
  const float WX=m4?0.905f:0.88f,WZF=m4?1.48f:1.38f,WZR=m4?-1.38f:-1.30f,WR=0.35f;
  if(m4)A.post=[=](SkinVertex& v){v.p[0]*=sx;v.p[2]*=sz;if(v.p[1]>0.98f)v.p[1]=0.98f+(v.p[1]-0.98f)*sy;};
  std::vector<std::string> nm={"root","wheel.FL","wheel.FR","wheel.RL","wheel.RR"};std::vector<int> pa={-1,0,0,0,0};
  std::vector<Vec3> hd={{0,0,0},{WX,WR,WZF},{-WX,WR,WZF},{WX,WR,WZR},{-WX,WR,WZR}};skeleton(out,nm,pa,hd);
  SkinFn root=[](SkinVertex& s,int,int){setW(s,0,1);};
  // lower body
  CarPanels cp;carBody(cp,WR,WR,WZF/sz,WZR/sz,root,2);A.add(cp.panels,paint,5);A.add(cp.under,0x050506,9);
  carCabin(cp,2);A.add(cp.cabin,paint,5);A.add(cp.glass,0x0b0f14,6);A.add(cp.seams,0x050506,9);A.add(cp.liner,0xb8b1a3,2);
  carInterior(A,root);
  // door lines, trim, lights, grille, plate
  VB trim;for(int sd=-1;sd<=1;sd+=2){float x=sd*0.941f;box(trim,{x,0.37f,0.0f},{.012f,.025f,1.5f},0,root);
      box(trim,{sd*0.78f,0.85f,0.67f},{.02f,.02f,.03f},0,root);}
  if(!m4)box(trim,{0,0.53f,2.262f},{.27f,.05f,.012f},0,root);box(trim,{0,0.37f,2.24f},{.50f,.022f,.02f},0,root);box(trim,{0,0.40f,-2.20f},{.45f,.03f,.02f},0,root);
  A.add(trim,0x0c0c0e,9);
  VB chrome;for(int sd=-1;sd<=1;sd+=2){loft(chrome,{{{sd*0.56f,0.70f,2.19f},.001f,.001f},{{sd*0.56f,0.70f,2.185f},.17f,.075f},{{sd*0.56f,0.70f,2.14f},.17f,.075f},{{sd*0.56f,0.70f,2.12f},.001f,.001f}},{1,0,0},2.2f,root,18);
      if(!m4)box(chrome,{sd*0.96f,0.99f,0.67f},{.05f,.05f,.075f},0,root);}
  box(chrome,{0,0.60f,2.265f},{.29f,.008f,.006f},0,root);box(chrome,{0,0.46f,2.255f},{.29f,.008f,.006f},0,root);box(chrome,{0,0.53f,2.272f},{.006f,.05f,.004f},0,root);
  A.add(chrome,0xe8e8ee,7);
  { VB hlg,hlr,hle,bez;for(int sd=-1;sd<=1;sd+=2){float x=sd*0.62f;
      loft(bez,{{{x,0.72f,2.150f},.001f,.001f},{{x,0.72f,2.168f},.158f,.066f},{{x,0.72f,2.200f},.158f,.066f},{{x,0.72f,2.204f},.001f,.001f}},{1,0,0},2.4f,root,24);   // chrome bezel
      loft(hlr,{{{x,0.72f,2.170f},.001f,.001f},{{x,0.72f,2.178f},.145f,.056f},{{x,0.72f,2.196f},.145f,.056f},{{x,0.72f,2.198f},.001f,.001f}},{1,0,0},2.4f,root,24);   // reflector bowl
      box(hle,{x,0.745f,2.201f},{.105f,.007f,.004f},0,root);box(hle,{x,0.700f,2.201f},{.08f,.005f,.004f},0,root);                                                      // LED strips
      loft(hle,{{{x-sd*0.06f,0.72f,2.190f},.001f,.001f},{{x-sd*0.06f,0.72f,2.200f},.032f,.032f},{{x-sd*0.06f,0.72f,2.204f},.001f,.001f}},{1,0,0},2.0f,root,14);          // projector lens
      loft(hlg,{{{x,0.72f,2.205f},.001f,.001f},{{x,0.72f,2.207f},.152f,.060f},{{x,0.72f,2.209f},.001f,.001f}},{1,0,0},2.4f,root,24);}                                    // clear lens cover
    A.add(bez,0xd9dce2,7);A.add(hlr,0x6d7076,7);A.add(hle,0xeaf6ff,12);A.add(hlg,0x10151a,6);}
  { VB tlr,tle,tlg,tlc;for(int sd=-1;sd<=1;sd+=2){float x=sd*0.68f;box(tlc,{x,0.82f,-2.145f},{.205f,.058f,.012f},0,root);box(tlr,{x,0.82f,-2.157f},{.19f,.045f,.006f},0,root);box(tle,{x,0.835f,-2.162f},{.15f,.008f,.004f},0,root);box(tle,{x,0.805f,-2.162f},{.15f,.006f,.004f},0,root);}
    box(tlc,{0,0.82f,-2.15f},{.45f,.006f,.006f},0,root);A.add(tlc,0xd9dce2,7);A.add(tlr,0x500a0e,5);A.add(tle,0xff2a2a,12);}
  VB plate;box(plate,{0,0.56f,-2.215f},{.26f,.07f,.006f},0,root);A.add(plate,0xe9e9df,9);
  // wheels
  if(m4){VB mir;for(int sd=-1;sd<=1;sd+=2)loft(mir,{{{sd*0.90f,0.97f,0.66f},.001f,.001f},{{sd*0.94f,1.0f,0.66f},.03f,.040f},{{sd*1.03f,1.02f,0.66f},.05f,.043f},{{sd*1.065f,1.02f,0.66f},.001f,.001f}},{0,0,1},2.2f,root,16);A.add(mir,0x0e0e10,5);}
  if(m4){ // coupe specifics: twin kidney grille, hood ridge, fender gills, quad exhaust, diffuser, lip spoiler, black mirrors
    VB kf,kd,ks,ridge,gill,exh,exi,dif;
    for(int sd=-1;sd<=1;sd+=2){float x=sd*0.112f;
      loft(kf,{{{x,0.60f,2.246f},.099f,.128f},{{x,0.60f,2.272f},.099f,.128f}},{1,0,0},2.4f,root,24,false);
      loft(kd,{{{x,0.60f,2.258f},.001f,.001f},{{x,0.60f,2.262f},.088f,.117f},{{x,0.60f,2.266f},.001f,.001f}},{1,0,0},2.4f,root,24);
      for(int k=-3;k<=3;k++)box(ks,{x+k*0.0235f,0.60f,2.272f},{.0045f,.108f,.005f},0,root);}
    A.add(kf,0xdadde3,7);A.add(kd,0x070708,9);A.add(ks,0x2b2c30,7);
    loft(ridge,{{{0,0.985f,0.95f},.15f,.012f},{{0,0.975f,1.35f},.20f,.020f},{{0,0.925f,1.85f},.15f,.016f},{{0,0.87f,2.10f},.08f,.008f}},{1,0,0},2.6f,root,18);A.add(ridge,paint,5);
    for(int sd=-1;sd<=1;sd+=2){box(gill,{sd*0.958f,0.70f,1.20f},{.004f,.030f,.085f},0,root);for(int k=-2;k<=2;k++)box(gill,{sd*0.962f,0.70f,1.20f+k*0.03f},{.002f,.026f,.006f},0,root);}A.add(gill,0x101012,7);
    for(int sd=-1;sd<=1;sd+=2)for(int q=0;q<2;q++){float x=sd*(0.36f+q*0.20f);loft(exh,{{{x,0.37f,-2.23f},.048f,.048f},{{x,0.37f,-2.30f},.052f,.052f},{{x,0.37f,-2.31f},.050f,.050f}},{1,0,0},2.0f,root,20,false);loft(exi,{{{x,0.37f,-2.22f},.040f,.040f},{{x,0.37f,-2.31f},.040f,.040f}},{1,0,0},2.0f,root,16);}
    A.add(exh,0xc9ccd3,7);A.add(exi,0x040404,9);
    box(dif,{0,0.37f,-2.20f},{.62f,.045f,.05f},0,root);A.add(dif,0x0e0e10,9);
    VB lip;box(lip,{0,0.985f,-1.98f},{.60f,.012f,.06f},-0.12f,root);A.add(lip,0x0d0d0f,5);}
  struct Wh{int bone;float x;};Wh whs[4]={{1,WX},{2,-WX},{3,WX},{4,-WX}};
  VB tire,rim,arch,disc,caliper;
  for(auto& w:whs){int b=w.bone;Vec3 c=hd[b];SkinFn fn=[b](SkinVertex& s,int,int){setW(s,b,1);};float sg=w.x>0?1.f:-1.f;
    // tyre profile (rings along x)
    std::vector<Ring> tr;float prof[][2]={{-.13f,.22f},{-.126f,.30f},{-.108f,.343f},{-.098f,.356f},{-.070f,.356f},{-.064f,.341f},{-.046f,.341f},{-.040f,.356f},{-.012f,.356f},{-.006f,.341f},{.012f,.341f},{.018f,.356f},{.046f,.356f},{.052f,.341f},{.070f,.341f},{.076f,.356f},{.098f,.356f},{.108f,.343f},{.126f,.30f},{.13f,.22f}};for(auto& p:prof)tr.push_back({{c.x+p[0],c.y,c.z},p[1],p[1]});loft(tire,tr,{0,0,1},2.0f,fn,40,false);
    std::vector<Ring> rr={{{c.x+sg*0.118f,c.y,c.z},.001f,.001f},{{c.x+sg*0.122f,c.y,c.z},.235f,.235f},{{c.x+sg*0.132f,c.y,c.z},.225f,.225f},{{c.x+sg*0.118f,c.y,c.z},.001f,.001f}};
    loft(rim,{{{c.x+sg*0.060f,c.y,c.z},.226f,.226f},{{c.x+sg*0.125f,c.y,c.z},.232f,.232f},{{c.x+sg*0.135f,c.y,c.z},.245f,.245f},{{c.x+sg*0.139f,c.y,c.z},.236f,.236f}},{0,0,1},2.0f,fn,40,false);
    for(int k=0;k<7;k++){float a=k*6.2831853f/7;box(rim,{c.x+sg*0.118f,c.y,c.z},{.010f,.205f,.020f},a,fn);box(rim,{c.x+sg*0.118f,c.y,c.z},{.010f,.205f,.020f},a+0.22f,fn);}
    loft(rim,{{{c.x+sg*0.105f,c.y,c.z},.075f,.075f},{{c.x+sg*0.140f,c.y,c.z},.065f,.065f},{{c.x+sg*0.146f,c.y,c.z},.001f,.001f}},{0,0,1},2.0f,fn,24);
    loft(disc,{{{c.x-sg*0.078f,c.y,c.z},.205f,.205f},{{c.x-sg*0.050f,c.y,c.z},.205f,.205f}},{0,0,1},2.0f,fn,40);
    box(caliper,{c.x-sg*0.064f,c.y+0.185f,c.z+0.01f},{.036f,.058f,.075f},0,root);
    // dark wheel-arch disc on the body side
    SkinFn rfn=root;loft(arch,{{{sg*0.915f,c.y+0.02f,c.z},.46f,.46f},{{sg*0.925f,c.y+0.02f,c.z},.46f,.46f}},{0,0,1},2.0f,rfn,32);(void)rr;}
  A.add(tire,0x151517,9);A.add(rim,0x9a9ea6,7);A.add(disc,0x74777c,7);A.add(caliper,0xc0161e,5);
  // chassis underside
  VB under;loft(under,{{{0,0.30f,-2.0f},.62f,.05f},{{0,0.30f,2.0f},.62f,.05f}},{1,0,0},2.0f,root,12);A.add(under,0x060606,9);
  // animation: wheels spinning
  Anim dr;dr.name="Drive";dr.dur=0.6f;for(int b=1;b<=4;b++){AnimChannel ch;ch.node=b;ch.path=1;int N=8;for(int i=0;i<=N;i++){ch.t.push_back(dr.dur*i/N);float q[4];quatX(6.2831853f*i/N*(-1.f),q);for(int k=0;k<4;k++)ch.v.push_back(q[k]);}dr.ch.push_back(ch);}
  out.anims.push_back(dr);out.builtin=(int)out.anims.size();
  out.fit=1;out.foot=0;out.height=1.47f;out.radius=2.6f;out.autoRig=false;return true;}

// ---------------------------------------------------------------- HORSE
bool buildHorse(Renderer& r,const std::string& spec,Model& out){
  unsigned coat=parseHex(spec,0x7a4a2b);unsigned mane=spec.size()>=13?parseHex(spec.substr(7),0x1b120c):shade(coat,0.35f);
  std::vector<unsigned char> white{255,255,255,255};out=Model();out.name="Horse";Asm A{&out,&r,r.createTexture(white.data(),1,1)};
  enum{ROOT,SPINE,NECK1,NECK2,HEAD,TAIL1,TAIL2,FLu,FLl,FLf,FRu,FRl,FRf,HLu,HLl,HLf,HRu,HRl,HRf};
  std::vector<std::string> nm={"root","spine","neck01","neck02","head","tail01","tail02","foreleg_upper.L","foreleg_lower.L","foreleg_foot.L","foreleg_upper.R","foreleg_lower.R","foreleg_foot.R","hindleg_upper.L","hindleg_lower.L","hindleg_foot.L","hindleg_upper.R","hindleg_lower.R","hindleg_foot.R"};
  std::vector<int> pa={-1,0,1,2,3,0,5,1,7,8,1,10,11,0,13,14,0,16,17};
  float fx=0.17f,hx=0.19f;
  std::vector<Vec3> hd={{0,1.0f,0},{0,1.28f,-0.1f},{0,1.50f,0.62f},{0,1.72f,0.92f},{0,1.95f,1.14f},{0,1.50f,-1.03f},{0,1.22f,-1.22f},
    {fx,1.22f,0.58f},{fx,0.60f,0.62f},{fx,0.17f,0.62f},{-fx,1.22f,0.58f},{-fx,0.60f,0.62f},{-fx,0.17f,0.62f},
    {hx,1.18f,-0.80f},{hx,0.55f,-0.99f},{hx,0.17f,-0.90f},{-hx,1.18f,-0.80f},{-hx,0.55f,-0.99f},{-hx,0.17f,-0.90f}};
  // reparent: legs hang off spine/root
  skeleton(out,nm,pa,hd);
  SkinFn spine=[](SkinVertex& s,int,int){setW(s,SPINE,1);};
  // body barrel
  VB body;{struct S{float z,top,bot,rx;};std::vector<S> ss={{-1.08f,1.47f,1.20f,.12f},{-1.0f,1.55f,1.10f,.24f},{-0.80f,1.58f,1.04f,.31f},{-0.45f,1.53f,0.96f,.31f},{-0.05f,1.50f,0.90f,.31f},{0.32f,1.57f,0.91f,.28f},{0.66f,1.55f,0.97f,.25f},{0.88f,1.47f,1.08f,.18f},{0.98f,1.40f,1.18f,.09f}};
    std::vector<Ring> rs;for(auto& s:ss)rs.push_back({{0,(s.top+s.bot-0.07f)*0.5f,s.z},s.rx*1.18f,(s.top-s.bot+0.07f)*0.5f});loft(body,rs,{1,0,0},2.3f,spine,26);}
  // neck
  VB neck;{std::vector<Ring> rs={{{0,1.34f,0.50f},.24f,.32f},{{0,1.50f,0.70f},.20f,.29f},{{0,1.68f,0.90f},.16f,.23f},{{0,1.86f,1.08f},.125f,.18f},{{0,1.95f,1.14f},.11f,.15f}};
    loft(neck,rs,{1,0,0},2.3f,[&](SkinVertex& s,int k,int K){float t=(float)k/(K-1);if(t<0.4f)setW(s,SPINE,1-t/0.4f,NECK1,t/0.4f);else if(t<0.8f)setW(s,NECK1,1-(t-0.4f)/0.4f,NECK2,(t-0.4f)/0.4f);else setW(s,NECK2,1-(t-0.8f)/0.2f,HEAD,(t-0.8f)/0.2f);},24);}
  VB all;{ // merge body+neck+head+legs into one hide mesh
    all=body;auto app=[&](VB& m){uint32_t b=(uint32_t)all.v.size();all.v.insert(all.v.end(),m.v.begin(),m.v.end());for(uint32_t i:m.idx)all.idx.push_back(i+b);};app(neck);
    SkinFn hf=[](SkinVertex& s,int,int){setW(s,HEAD,1);};
    VB head;std::vector<Ring> hr={{{0,1.99f,1.10f},.09f,.13f},{{0,1.92f,1.17f},.115f,.165f},{{0,1.80f,1.29f},.11f,.16f},{{0,1.70f,1.38f},.085f,.115f},{{0,1.61f,1.47f},.072f,.092f},{{0,1.53f,1.545f},.068f,.080f},{{0,1.495f,1.575f},.058f,.060f},{{0,1.485f,1.582f},.02f,.02f}};
    loft(head,hr,{1,0,0},2.4f,hf,24);app(head);
    VB ears;for(int sd=-1;sd<=1;sd+=2)loft(ears,{{{sd*0.07f,2.02f,1.06f},.03f,.045f},{{sd*0.085f,2.12f,1.045f},.032f,.05f},{{sd*0.095f,2.22f,1.03f},.018f,.03f},{{sd*0.10f,2.27f,1.03f},.004f,.004f}},{1,0,0},2.2f,hf,12);app(ears);
    // legs: smooth skin weights across knee and fetlock
    auto leg=[&](float x,std::vector<Vec3> p,std::vector<float> rx,std::vector<float> ry,int bu,float yK,float yF,float yTop){std::vector<Ring> rs;for(size_t i=0;i<p.size();i++)rs.push_back({{p[i].x+x,p[i].y,p[i].z},rx[i]*1.25f,ry[i]*1.2f});
      VB lg;loft(lg,rs,{1,0,0},2.4f,[&](SkinVertex& s,int,int){float y=s.p[1];float a=sstep((yK+0.06f-y)/0.12f);float b=sstep((yF+0.05f-y)/0.10f);
        if(y>yK+0.06f)setW(s,bu,1);else if(y>yK-0.06f)setW(s,bu,1-a,bu+1,a);else if(y>yF+0.05f)setW(s,bu+1,1);else if(y>yF-0.05f)setW(s,bu+1,1-b,bu+2,b);else setW(s,bu+2,1);},20);(void)yTop;app(lg);};
    for(int sd=-1;sd<=1;sd+=2){int bu=sd>0?FLu:FRu;
      leg(sd*fx,{{0,1.30f,.56f},{0,1.05f,.60f},{0,.82f,.62f},{0,.62f,.625f},{0,.50f,.62f},{0,.30f,.62f},{0,.18f,.625f},{0,.105f,.64f}},{.125f,.105f,.085f,.074f,.050f,.042f,.050f,.043f},{.16f,.135f,.105f,.085f,.060f,.052f,.060f,.050f},bu,.60f,.17f,1.3f);
      int bh=sd>0?HLu:HRu;
      leg(sd*hx,{{0,1.30f,-.80f},{0,1.06f,-.88f},{0,.80f,-.93f},{0,.58f,-.99f},{0,.45f,-.95f},{0,.30f,-.92f},{0,.18f,-.90f},{0,.105f,-.90f}},{.155f,.135f,.095f,.075f,.052f,.042f,.050f,.043f},{.21f,.185f,.12f,.095f,.062f,.050f,.060f,.050f},bh,.55f,.17f,1.3f);}
  }
  A.add(all,coat,8);
  // hooves
  VB hoof;for(int sd=-1;sd<=1;sd+=2){auto h=[&](float x,float z,int b){loft(hoof,{{{x,0.105f,z},.043f,.05f},{{x,0.085f,z+.004f},.056f,.066f},{{x,0.0f,z+.012f},.062f,.074f}},{1,0,0},2.3f,[b](SkinVertex& s,int,int){setW(s,b,1);},16);};
    h(sd*fx,.64f,sd>0?FLf:FRf);h(sd*hx,-.90f,sd>0?HLf:HRf);}
  A.add(hoof,0x1a1613,9);
  // eyes, nostrils
  VB eye,dark;SkinFn hf=[](SkinVertex& s,int,int){setW(s,HEAD,1);};
  for(int sd=-1;sd<=1;sd+=2){loft(eye,{{{sd*0.108f,1.915f,1.19f},.001f,.001f},{{sd*0.114f,1.915f,1.19f},.024f,.020f},{{sd*0.126f,1.915f,1.19f},.012f,.01f},{{sd*0.129f,1.915f,1.19f},.001f,.001f}},{0,0,1},2.0f,hf,14);
    loft(dark,{{{sd*0.04f,1.545f,1.572f},.001f,.001f},{{sd*0.046f,1.545f,1.575f},.014f,.020f},{{sd*0.052f,1.545f,1.577f},.001f,.001f}},{0,1,0},2.0f,hf,10);}
  A.add(eye,0x120b07,4);A.add(dark,0x0b0605,9);
  // mane & tail (ribbons)
  VB hair;SkinFn nf=[&](SkinVertex& s,int,int){float y=s.p[1];if(y>1.85f)setW(s,HEAD,1);else if(y>1.7f)setW(s,NECK2,1);else setW(s,NECK1,1);};
  for(int i=0;i<26;i++){float t=i/25.f;Vec3 base{0.012f,1.70f+(t*0.32f)-0.20f+0.0f,0.0f};float z=0.52f+t*0.60f,y=1.60f+t*0.43f;if(t>.96f)y+=0.02f;
    float len=0.20f+0.06f*std::sin(t*9.f)+(1-t)*0.05f;Vec3 p0{0.02f,y,z-0.02f},p1{0.065f,y-len*0.45f,z-0.05f},p2{0.085f,y-len,z-0.07f};
    ribbon(hair,{p0,p1,p2},{.03f,.03f,.012f},{0,0,1},nf);}
  SkinFn tf=[&](SkinVertex& s,int,int){float y=s.p[1];if(y>1.35f)setW(s,TAIL1,1);else setW(s,TAIL1,0.5f,TAIL2,0.5f);};
  for(int i=0;i<14;i++){float a=(i-6.5f)*0.032f;float lean=std::sin(i*1.7f)*0.03f;float len=0.75f+0.12f*std::sin(i*2.3f);std::vector<Vec3> ps;std::vector<float> ws;
    for(int k=0;k<6;k++){float t=k/5.f;ps.push_back({a+lean*t,1.50f-t*len,-1.04f-0.14f*std::sin(t*2.2f)-t*0.04f});ws.push_back(0.026f*(1-0.7f*t));}ribbon(hair,ps,ws,{1,0,0},tf);}
  A.add(hair,mane,9);
  // idle + walk + trot animations
  auto gait=[&](const char* name,float dur,float amp,float knee,bool trot){Anim an;an.name=name;an.dur=dur;int N=24;
    int legs[4][3]={{FLu,FLl,FLf},{FRu,FRl,FRf},{HLu,HLl,HLf},{HRu,HRl,HRf}};float ph[4]={0,trot?3.14159f:1.5708f*3,trot?3.14159f:1.5708f,trot?0.f:1.5708f*2};
    for(int l=0;l<4;l++){bool hind=l>=2;AnimChannel cu,cl,cf;cu.node=legs[l][0];cl.node=legs[l][1];cf.node=legs[l][2];cu.path=cl.path=cf.path=1;
      for(int i=0;i<=N;i++){float t=dur*i/N;float f=6.2831853f*i/N+ph[l];float sw=std::sin(f);float lift=std::max(0.f,std::cos(f));float au=-amp*sw*(hind?-1.f:1.f)*-1.f;float al=(hind?-1.f:1.f)*knee*lift;float af=-0.35f*al;
        float q[4];for(auto* c:{&cu,&cl,&cf}){c->t.push_back(t);}quatX(au,q);for(int k=0;k<4;k++)cu.v.push_back(q[k]);quatX(al,q);for(int k=0;k<4;k++)cl.v.push_back(q[k]);quatX(af,q);for(int k=0;k<4;k++)cf.v.push_back(q[k]);}
      an.ch.push_back(cu);an.ch.push_back(cl);an.ch.push_back(cf);}
    AnimChannel hn;hn.node=NECK1;hn.path=1;AnimChannel hh;hh.node=HEAD;hh.path=1;for(int i=0;i<=N;i++){float t=dur*i/N;float q[4];hn.t.push_back(t);quatX(0.06f*std::sin(6.2831853f*i/N*2),q);for(int k=0;k<4;k++)hn.v.push_back(q[k]);hh.t.push_back(t);quatX(-0.05f*std::sin(6.2831853f*i/N*2),q);for(int k=0;k<4;k++)hh.v.push_back(q[k]);}
    an.ch.push_back(hn);an.ch.push_back(hh);
    AnimChannel tb;tb.node=TAIL1;tb.path=1;for(int i=0;i<=N;i++){tb.t.push_back(dur*i/N);float q[4];quatX(0.18f*std::sin(6.2831853f*i/N+1.0f),q);for(int k=0;k<4;k++)tb.v.push_back(q[k]);}an.ch.push_back(tb);
    out.anims.push_back(an);};
  gait("Walk",1.2f,0.38f,0.55f,false);gait("Trot",0.8f,0.52f,0.8f,true);
  {Anim an;an.name="Idle";an.dur=3.f;int N=12;AnimChannel hh;hh.node=NECK1;hh.path=1;AnimChannel tb;tb.node=TAIL1;tb.path=1;for(int i=0;i<=N;i++){float t=an.dur*i/N;float q[4];hh.t.push_back(t);quatX(0.03f*std::sin(6.2831853f*i/N),q);for(int k=0;k<4;k++)hh.v.push_back(q[k]);tb.t.push_back(t);quatX(0.12f*std::sin(6.2831853f*i/N*2),q);for(int k=0;k<4;k++)tb.v.push_back(q[k]);}an.ch.push_back(hh);an.ch.push_back(tb);out.anims.push_back(an);}
  out.builtin=(int)out.anims.size();out.fit=1;out.foot=0;out.height=2.27f;out.radius=1.7f;out.autoRig=false;return true;}
}
