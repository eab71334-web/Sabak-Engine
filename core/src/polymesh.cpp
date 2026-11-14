#include "polymesh.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <array>
#include <memory>
namespace sk {
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
}
static Vec3 newell(const PolyData& m,const std::vector<int>& f){Vec3 n{0,0,0};for(size_t i=0;i<f.size();i++){Vec3 a=m.V[f[i]],b=m.V[f[(i+1)%f.size()]];n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);}return norm(n);}
Vec3 PolyMesh::faceNormal(int f) const {return newell(d,d.F[f]);}
Vec3 PolyMesh::faceCenter(int f) const {Vec3 c{0,0,0};for(int v:d.F[f])c=c+d.V[v];return c*(1.f/d.F[f].size());}
void PolyMesh::clear(){d=PolyData();selectNone();undo_.clear();}
void PolyMesh::cube(Vec3 c,Vec3 h){clear();for(int i=0;i<8;i++)d.V.push_back({c.x+(i&1?h.x:-h.x),c.y+(i&2?h.y:-h.y),c.z+(i&4?h.z:-h.z)});
  static const int Q[6][4]={{0,2,3,1},{4,5,7,6},{0,1,5,4},{2,6,7,3},{0,4,6,2},{1,3,7,5}};for(auto& q:Q)d.F.push_back({q[0],q[1],q[2],q[3]});}
void PolyMesh::cylinder(Vec3 c,float r,float h,int n){clear();for(int k=0;k<2;k++)for(int i=0;i<n;i++){float a=6.2831853f*i/n;d.V.push_back({c.x+r*std::cos(a),c.y+(k?h:-h)*0.5f,c.z+r*std::sin(a)});}
  for(int i=0;i<n;i++){int j=(i+1)%n;d.F.push_back({i,n+i,n+j,j});}std::vector<int> top,bot;for(int i=0;i<n;i++){top.push_back(n+i);bot.push_back(n-1-i);}std::reverse(top.begin(),top.end());d.F.push_back(top);d.F.push_back(bot);
  // fix orientation: make all face normals point away from centre
  for(auto& f:d.F){Vec3 nn=newell(d,f);Vec3 ce{0,0,0};for(int v:f)ce=ce+d.V[v];ce=ce*(1.f/f.size())-c;if(dot(nn,ce)<0)std::reverse(f.begin(),f.end());}}
void PolyMesh::grid(int n,float size){clear();for(int j=0;j<=n;j++)for(int i=0;i<=n;i++)d.V.push_back({(i/(float)n-0.5f)*size,0,(j/(float)n-0.5f)*size});
  for(int j=0;j<n;j++)for(int i=0;i<n;i++){int a=j*(n+1)+i;d.F.push_back({a,a+n+1,a+n+2,a+1});}}
void PolyMesh::selectNone(){selV.clear();selF.clear();selE.clear();}
void PolyMesh::selectAllFaces(){selF.clear();for(size_t i=0;i<d.F.size();i++)selF.insert((int)i);syncFromFaces();}
void PolyMesh::syncFromFaces(){selV.clear();selE.clear();for(int f:selF){auto& F=d.F[f];for(size_t i=0;i<F.size();i++){selV.insert(F[i]);selE.insert(key(F[i],F[(i+1)%F.size()]));}}}
void PolyMesh::syncFromVerts(){selF.clear();for(size_t f=0;f<d.F.size();f++){bool all=true;for(int v:d.F[f])if(!selV.count(v)){all=false;break;}if(all)selF.insert((int)f);}
  selE.clear();for(auto& F:d.F)for(size_t i=0;i<F.size();i++)if(selV.count(F[i])&&selV.count(F[(i+1)%F.size()]))selE.insert(key(F[i],F[(i+1)%F.size()]));}
void PolyMesh::setMode(int m){mode=m;if(m==2){syncFromVerts();syncFromFaces();}}
void PolyMesh::toggleVert(int v){if(!selV.erase(v))selV.insert(v);syncFromVerts();}
void PolyMesh::toggleEdge(ull e){if(!selE.erase(e)){selE.insert(e);}selV.clear();for(ull k:selE){selV.insert((int)(k>>32));selV.insert((int)(k&0xffffffff));}}
void PolyMesh::toggleFace(int f){if(!selF.erase(f))selF.insert(f);syncFromFaces();}
Vec3 PolyMesh::selCenter() const{Vec3 c{0,0,0};if(selV.empty())return c;for(int v:selV)c=c+d.V[v];return c*(1.f/selV.size());}
void PolyMesh::snapshot(){undo_.push_back(d);if(undo_.size()>40)undo_.erase(undo_.begin());}
bool PolyMesh::undo(){if(undo_.empty())return false;d=undo_.back();undo_.pop_back();selectNone();return true;}
// ---- face set for ops: explicit faces or faces whose vertices are all selected
static std::set<int> opFaces(const PolyMesh& p){return p.selF;}
bool PolyMesh::extrude(float dist){auto sf=opFaces(*this);if(sf.empty())return false;snapshot();
  std::map<ull,int> cnt;for(int f:sf){auto& F=d.F[f];for(size_t i=0;i<F.size();i++)cnt[key(F[i],F[(i+1)%F.size()])]++;}
  std::map<int,int> dup;std::map<int,Vec3> nsum;for(int f:sf){Vec3 n=faceNormal(f);for(int v:d.F[f]){if(!dup.count(v)){dup[v]=(int)d.V.size();d.V.push_back(d.V[v]);}nsum[v]=nsum[v]+n;}}
  std::vector<std::vector<int>> side;
  for(int f:sf){auto F=d.F[f];for(size_t i=0;i<F.size();i++){int a=F[i],b=F[(i+1)%F.size()];if(cnt[key(a,b)]==1)side.push_back({a,b,dup[b],dup[a]});}
    for(auto& v:d.F[f])v=dup[v];}
  for(auto& kv:dup)d.V[kv.second]=d.V[kv.first]+norm(nsum[kv.first])*dist;
  for(auto& s:side)d.F.push_back(s);
  selV.clear();for(auto& kv:dup)selV.insert(kv.second);selE.clear();syncFromFaces();return true;}
bool PolyMesh::inset(float amount){auto sf=opFaces(*this);if(sf.empty())return false;snapshot();std::vector<std::vector<int>> side;std::set<int> keep;
  for(int f:sf){auto F=d.F[f];Vec3 c=faceCenter(f);std::vector<int> in;for(int v:F){in.push_back((int)d.V.size());d.V.push_back(d.V[v]*(1-amount)+c*amount);}
    for(size_t i=0;i<F.size();i++){size_t j=(i+1)%F.size();side.push_back({F[i],F[j],in[j],in[i]});}d.F[f]=in;}
  for(auto& s:side)d.F.push_back(s);selV.clear();selE.clear();syncFromFaces();return true;}
bool PolyMesh::setCrease(float v){if(selE.empty())return false;snapshot();for(ull e:selE){if(v<=0)d.crease.erase(e);else d.crease[e]=v;}return true;}
bool PolyMesh::bevel(float c){return setCrease(c);}   // rounded edge: semi-sharp crease, resolved by subdivision
bool PolyMesh::deleteFaces(){if(selF.empty())return false;snapshot();std::vector<std::vector<int>> nf;for(size_t f=0;f<d.F.size();f++)if(!selF.count((int)f))nf.push_back(d.F[f]);d.F=nf;selectNone();return true;}
bool PolyMesh::flip(){if(selF.empty())return false;snapshot();for(int f:selF)std::reverse(d.F[f].begin(),d.F[f].end());return true;}
// ---- loop cut: walks the quad ring across `edge`, inserts a vertex on every ring edge and splits the quads
bool PolyMesh::loopCut(ull edge,float t){
  std::map<ull,std::vector<int>> ef;for(size_t f=0;f<d.F.size();f++){auto& F=d.F[f];for(size_t i=0;i<F.size();i++)ef[key(F[i],F[(i+1)%F.size()])].push_back((int)f);}
  auto it=ef.find(edge);if(it==ef.end())return false;
  struct Step{int f;int i;};std::vector<Step> ring;std::map<ull,int> side0;std::set<int> seen;
  auto walk=[&](int f0,ull e0,int a0,bool forward){int f=f0;ull e=e0;int a=a0;while(true){if(seen.count(f))break;auto& F=d.F[f];if(F.size()!=4)break;int ix=-1;for(int i=0;i<4;i++)if(key(F[i],F[(i+1)%4])==e){ix=i;break;}if(ix<0)break;
      seen.insert(f);ring.push_back({f,ix});if(!side0.count(e))side0[e]=a;
      int o=(ix+2)%4;ull oe=key(F[o],F[(o+1)%4]);int oa=(F[ix]==a)?F[(ix+3)%4]:F[(ix+2)%4];  // vertex on the same side
      if(!side0.count(oe))side0[oe]=oa;auto& nb=ef[oe];int nf=-1;for(int g:nb)if(g!=f)nf=g;if(nf<0)break;f=nf;e=oe;a=oa;(void)forward;}};
  int f0=it->second[0];int a0=(int)(edge>>32);walk(f0,edge,a0,true);
  if(it->second.size()>1){int g=it->second[1];if(!seen.count(g))walk(g,edge,a0,false);}
  if(ring.empty())return false;snapshot();
  std::map<ull,int> mid;for(auto& kv:side0){int a=kv.second;int b=(int)(kv.first>>32)==a?(int)(kv.first&0xffffffff):(int)(kv.first>>32);mid[kv.first]=(int)d.V.size();d.V.push_back(d.V[a]*(1-t)+d.V[b]*t);
    auto c=d.crease.find(kv.first);if(c!=d.crease.end()){d.crease[key(a,mid[kv.first])]=c->second;d.crease[key(b,mid[kv.first])]=c->second;}}
  std::vector<std::vector<int>> nf;std::set<int> ringF;std::vector<int> ringSel;for(auto& s:ring)ringF.insert(s.f);
  for(size_t f=0;f<d.F.size();f++){auto F=d.F[f];if(ringF.count((int)f)){int ix=-1;for(auto& s:ring)if(s.f==(int)f){ix=s.i;break;}
      int v0=F[ix],v1=F[(ix+1)%4],v2=F[(ix+2)%4],v3=F[(ix+3)%4];int m1=mid[key(v0,v1)],m2=mid[key(v2,v3)];
      nf.push_back({v0,m1,m2,v3});nf.push_back({m1,v1,v2,m2});}
    else{std::vector<int> g;for(size_t i=0;i<F.size();i++){g.push_back(F[i]);ull k=key(F[i],F[(i+1)%F.size()]);auto m=mid.find(k);if(m!=mid.end())g.push_back(m->second);}nf.push_back(g);}}
  d.F=nf;selectNone();for(auto& kv:mid){selV.insert(kv.second);}syncFromVerts();return true;}
void PolyMesh::selectRingLoop(ull e){selE.clear();selV.clear();selE.insert(e);selV.insert((int)(e>>32));selV.insert((int)(e&0xffffffff));}
// ---- proportional move / scale (radius 0 = selection only)
void PolyMesh::moveSel(Vec3 dl,float radius){if(selV.empty())return;std::vector<Vec3> src;for(int v:selV)src.push_back(d.V[v]);
  for(size_t i=0;i<d.V.size();i++){float w=0;if(selV.count((int)i))w=1;else if(radius>0){float m=1e30f;for(auto& s:src){Vec3 q=d.V[i]-s;m=std::min(m,dot(q,q));}float dd=std::sqrt(m)/radius;if(dd<1){float u=1-dd;w=u*u*(3-2*u);}}
    if(w>0)d.V[i]=d.V[i]+dl*w;}}
void PolyMesh::scaleSel(float f,float radius){if(selV.empty())return;Vec3 c=selCenter();std::vector<Vec3> src;for(int v:selV)src.push_back(d.V[v]);
  for(size_t i=0;i<d.V.size();i++){float w=0;if(selV.count((int)i))w=1;else if(radius>0){float m=1e30f;for(auto& s:src){Vec3 q=d.V[i]-s;m=std::min(m,dot(q,q));}float dd=std::sqrt(m)/radius;if(dd<1){float u=1-dd;w=u*u*(3-2*u);}}
    if(w>0){Vec3 p=d.V[i]-c;d.V[i]=c+p*(1+(f-1)*w);}}}
// ---- evaluate: mirror modifier, then Catmull-Clark with creases
PolyData PolyMesh::evaluate(int levels) const{
  SubMesh m;m.V=d.V;m.F=d.F;m.crease=d.crease;
  if(mirrorX){std::map<int,int> mv;size_t n=m.V.size();for(size_t i=0;i<n;i++){if(std::fabs(d.V[i].x)<1e-4f)mv[(int)i]=(int)i;else{mv[(int)i]=(int)m.V.size();m.V.push_back({-d.V[i].x,d.V[i].y,d.V[i].z});}}
    {std::vector<std::vector<int>> keep;for(auto& f:m.F){bool onPlane=true;for(int v:f)if(std::fabs(m.V[v].x)>1e-4f){onPlane=false;break;}if(!onPlane)keep.push_back(f);}m.F=keep;}  // faces on the mirror plane are internal
    for(auto& f:d.F){bool onPlane=true;for(int v:f)if(std::fabs(d.V[v].x)>1e-4f){onPlane=false;break;}if(onPlane)continue;std::vector<int> g;for(int k=(int)f.size()-1;k>=0;k--)g.push_back(mv[f[k]]);m.F.push_back(g);}
    for(auto& kv:d.crease)m.crease[ekey(mv[(int)(kv.first>>32)],mv[(int)(kv.first&0xffffffff)])]=kv.second;}
  if(levels>0)m=ccSub(m,levels);
  PolyData o;o.V=m.V;o.F=m.F;o.crease=m.crease;return o;}
void PolyMesh::triangulate(const PolyData& m,std::vector<Vec3>& P,std::vector<Vec3>& N,std::vector<uint32_t>& I){
  P=m.V;N.assign(P.size(),Vec3{0,0,0});I.clear();
  for(auto& f:m.F){Vec3 fn=newell(m,f);for(int v:f)N[v]=N[v]+fn;for(size_t i=1;i+1<f.size();i++){I.push_back(f[0]);I.push_back(f[i]);I.push_back(f[i+1]);}}
  for(auto& n:N)n=norm(n);}

// ================= BSP CSG (polygon booleans) =================
namespace {
struct CPlane{Vec3 n;float w;};
struct CPoly{std::vector<Vec3> v;CPlane p;};
static const float EPS=1e-5f;
static CPlane planeOf(const std::vector<Vec3>& v){Vec3 n=norm(cross(v[1]-v[0],v[2]-v[0]));return {n,dot(n,v[0])};}
static void splitPoly(const CPlane& pl,const CPoly& po,std::vector<CPoly>& cf,std::vector<CPoly>& cb,std::vector<CPoly>& f,std::vector<CPoly>& b){
  int ty=0;std::vector<int> t;for(auto& v:po.v){float d=dot(pl.n,v)-pl.w;int k=d<-EPS?2:d>EPS?1:0;ty|=k;t.push_back(k);}
  if(ty==0){(dot(pl.n,po.p.n)>0?cf:cb).push_back(po);}
  else if(ty==1)f.push_back(po);else if(ty==2)b.push_back(po);
  else{CPoly pf,pb;pf.p=pb.p=po.p;size_t n=po.v.size();for(size_t i=0;i<n;i++){size_t j=(i+1)%n;int ti=t[i],tj=t[j];Vec3 vi=po.v[i],vj=po.v[j];
      if(ti!=2)pf.v.push_back(vi);if(ti!=1)pb.v.push_back(vi);
      if((ti|tj)==3){float tt=(pl.w-dot(pl.n,vi))/dot(pl.n,vj-vi);Vec3 vv=vi+(vj-vi)*tt;pf.v.push_back(vv);pb.v.push_back(vv);}}
    if(pf.v.size()>=3)f.push_back(pf);if(pb.v.size()>=3)b.push_back(pb);}}
struct Node{bool has=false;CPlane pl;std::vector<CPoly> polys;std::unique_ptr<Node> front,back;
  void build(const std::vector<CPoly>& ps){if(ps.empty())return;if(!has){pl=ps[0].p;has=true;}std::vector<CPoly> f,b;for(auto& p:ps)splitPoly(pl,p,polys,polys,f,b);
    if(!f.empty()){if(!front)front.reset(new Node);front->build(f);}if(!b.empty()){if(!back)back.reset(new Node);back->build(b);}}
  void invert(){for(auto& p:polys){std::reverse(p.v.begin(),p.v.end());p.p.n=p.p.n*-1.f;p.p.w=-p.p.w;}pl.n=pl.n*-1.f;pl.w=-pl.w;if(front)front->invert();if(back)back->invert();std::swap(front,back);}
  std::vector<CPoly> clip(const std::vector<CPoly>& ps)const{if(!has)return ps;std::vector<CPoly> f,b;for(auto& p:ps)splitPoly(pl,p,f,b,f,b);
    if(front)f=front->clip(f);if(back)b=back->clip(b);else b.clear();f.insert(f.end(),b.begin(),b.end());return f;}
  void clipTo(const Node& o){polys=o.clip(polys);if(front)front->clipTo(o);if(back)back->clipTo(o);}
  void all(std::vector<CPoly>& out)const{out.insert(out.end(),polys.begin(),polys.end());if(front)front->all(out);if(back)back->all(out);}
};
static std::vector<CPoly> toPolys(const PolyData& m){std::vector<CPoly> o;for(auto& f:m.F)for(size_t i=1;i+1<f.size();i++){CPoly p;p.v={m.V[f[0]],m.V[f[i]],m.V[f[i+1]]};Vec3 n=cross(p.v[1]-p.v[0],p.v[2]-p.v[0]);if(dot(n,n)<1e-14f)continue;p.p=planeOf(p.v);o.push_back(p);}return o;}
}
PolyData PolyMesh::boolean(const PolyData& A0,const PolyData& B,int op){
  // locality: only faces of A that touch B's bounding box go through the BSP (subtract/intersect keep the rest untouched / drop it)
  PolyData A=A0;std::vector<std::vector<int>> farF;
  if(!B.V.empty()&&op==0){Vec3 lo=B.V[0],hi=B.V[0];for(auto& v:B.V){lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z);}
    float m=0.03f;lo=lo-Vec3{m,m,m};hi=hi+Vec3{m,m,m};std::vector<std::vector<int>> nearF;
    for(auto& f:A0.F){Vec3 fl=A0.V[f[0]],fh=fl;for(int k:f){Vec3 v=A0.V[k];fl.x=std::min(fl.x,v.x);fl.y=std::min(fl.y,v.y);fl.z=std::min(fl.z,v.z);fh.x=std::max(fh.x,v.x);fh.y=std::max(fh.y,v.y);fh.z=std::max(fh.z,v.z);}
      bool hit=!(fh.x<lo.x||fl.x>hi.x||fh.y<lo.y||fl.y>hi.y||fh.z<lo.z||fl.z>hi.z);(hit?nearF:farF).push_back(f);}
    A.F=nearF;}
  Node a,b;a.build(toPolys(A));b.build(toPolys(B));
  if(op==0){a.invert();a.clipTo(b);b.clipTo(a);b.invert();b.clipTo(a);b.invert();std::vector<CPoly> bp;b.all(bp);a.build(bp);a.invert();}
  else if(op==1){a.clipTo(b);b.clipTo(a);b.invert();b.clipTo(a);b.invert();std::vector<CPoly> bp;b.all(bp);a.build(bp);}
  else{a.invert();b.clipTo(a);b.invert();a.clipTo(b);b.clipTo(a);std::vector<CPoly> bp;b.all(bp);a.build(bp);a.invert();}
  std::vector<CPoly> res;a.all(res);PolyData o;std::map<std::array<long long,3>,int> wm;auto W=[&](Vec3 p){std::array<long long,3> k{(long long)std::llround(p.x*20000),(long long)std::llround(p.y*20000),(long long)std::llround(p.z*20000)};auto it=wm.find(k);if(it!=wm.end())return it->second;int i=(int)o.V.size();o.V.push_back(p);wm[k]=i;return i;};
  for(auto& p:res){std::vector<int> f;for(auto& v:p.v){int i=W(v);if(f.empty()||f.back()!=i)f.push_back(i);}while(f.size()>1&&f.front()==f.back())f.pop_back();if(f.size()>=3)o.F.push_back(f);}
  for(auto& f:farF){std::vector<int> g;for(int k:f)g.push_back(W(A0.V[k]));o.F.push_back(g);}
  return o;}
bool PolyMesh::save(const std::string& path) const{FILE* f=fopen(path.c_str(),"wb");if(!f)return false;int n=(int)d.V.size(),nf=(int)d.F.size(),nc=(int)d.crease.size();fwrite("PLY1",1,4,f);fwrite(&n,4,1,f);fwrite(d.V.data(),sizeof(Vec3),n,f);fwrite(&nf,4,1,f);
  for(auto& F:d.F){int k=(int)F.size();fwrite(&k,4,1,f);fwrite(F.data(),4,k,f);}fwrite(&nc,4,1,f);for(auto& kv:d.crease){fwrite(&kv.first,8,1,f);fwrite(&kv.second,4,1,f);}fwrite(&mirrorX,1,1,f);fwrite(&subLevels,4,1,f);fclose(f);return true;}
bool PolyMesh::load(const std::string& path){FILE* f=fopen(path.c_str(),"rb");if(!f)return false;char h[4];if(fread(h,1,4,f)!=4||std::string(h,4)!="PLY1"){fclose(f);return false;}PolyData t;int n=0;if(fread(&n,4,1,f)!=1||n<0||n>5000000){fclose(f);return false;}
  t.V.resize(n);if(fread(t.V.data(),sizeof(Vec3),n,f)!=(size_t)n){fclose(f);return false;}int nf=0;if(fread(&nf,4,1,f)!=1){fclose(f);return false;}
  for(int i=0;i<nf;i++){int k=0;if(fread(&k,4,1,f)!=1||k<3||k>64){fclose(f);return false;}std::vector<int> F(k);if(fread(F.data(),4,k,f)!=(size_t)k){fclose(f);return false;}t.F.push_back(F);}
  int nc=0;if(fread(&nc,4,1,f)==1)for(int i=0;i<nc;i++){ull kk;float v;if(fread(&kk,8,1,f)!=1||fread(&v,4,1,f)!=1)break;t.crease[kk]=v;}bool mx=true;int sl=0;if(fread(&mx,1,1,f)==1)fread(&sl,4,1,f);fclose(f);d=t;mirrorX=mx;subLevels=sl;selectNone();undo_.clear();return true;}
}
