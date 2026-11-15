// Sabak Core - builds a G82-style coupe with the PolyMesh tools (ring cage from blueprint data, creases, mirror, subdivision, boolean arches).
#include "polymesh.h"
#include <cmath>
#include <functional>
#include <algorithm>
namespace sk {
static const float TOPT[][2]={{2.397f,.713f},{2.305f,.747f},{2.212f,.793f},{2.12f,.828f},{2.027f,.851f},{1.935f,.885f},{1.843f,.908f},{1.75f,.931f},{1.658f,.943f},{1.565f,.955f},{1.473f,.955f},{1.381f,.966f},{1.288f,.978f},{1.196f,.978f},{1.103f,.989f},{1.011f,.989f},{.919f,1.0f},{.826f,1.058f},{.734f,1.104f},{.641f,1.161f},{.549f,1.208f},{.457f,1.242f},{.364f,1.299f},{.272f,1.334f},{.179f,1.369f},{.087f,1.38f},{-.005f,1.403f},{-.098f,1.403f},{-.19f,1.38f},{-.283f,1.38f},{-.375f,1.38f},{-.467f,1.403f},{-.56f,1.403f},{-.652f,1.391f},{-.745f,1.38f},{-.837f,1.369f},{-.929f,1.38f},{-1.022f,1.391f},{-1.114f,1.311f},{-1.207f,1.288f},{-1.299f,1.265f},{-1.391f,1.23f},{-1.484f,1.208f},{-1.576f,1.173f},{-1.669f,1.15f},{-1.761f,1.104f},{-1.853f,1.081f},{-1.946f,1.069f},{-2.038f,1.046f},{-2.131f,1.035f},{-2.223f,1.035f},{-2.315f,1.012f}};
static const float HWT[][2]={{2.40f,.50f},{2.33f,.66f},{2.26f,.76f},{2.12f,.87f},{1.98f,.925f},{1.84f,.941f},{-1.50f,.941f},{-1.62f,.93f},{-1.76f,.895f},{-1.90f,.86f},{-2.04f,.825f},{-2.18f,.72f},{-2.32f,.53f},{-2.40f,.42f}};
template<size_t N> static float lerpTab(const float (&t)[N][2],float f){
  if(f>=t[0][0]&&t[0][0]>t[N-1][0])return t[0][1];if(f<=t[N-1][0]&&t[0][0]>t[N-1][0])return t[N-1][1];
  for(size_t i=0;i+1<N;i++)if(f<=t[i][0]&&f>=t[i+1][0]){float u=(t[i][0]-f)/(t[i][0]-t[i+1][0]);u=u*u*(3-2*u);return t[i][1]*(1-u)+t[i+1][1]*u;}return t[N-1][1];}
// HWT is ascending-first-to-descending? it lists front (2.4) down to -2.4 with 1.84 -> -1.5 flat: treat as descending order
static float topH(float f){return lerpTab(TOPT,f);} static float halfW(float f){return lerpTab(HWT,f);}

struct Ring{std::vector<Vec3> p;};
static void addLoft(PolyMesh& pm,const std::vector<float>& st,int nr,const std::function<void(float,std::vector<Vec3>&)>& fn,const std::vector<int>& creaseRings,float cr,bool caps){
  auto& d=pm.d;int base=(int)d.V.size();std::vector<Ring> rows;
  for(float f:st){std::vector<Vec3> pts;fn(f,pts);rows.push_back({pts});for(auto& v:pts)d.V.push_back(v);}
  int S=(int)st.size();auto id=[&](int i,int j){return base+i*nr+j;};
  auto outward=[&](std::vector<int> q,Vec3 away){Vec3 c{0,0,0};for(int v:q)c=c+d.V[v];c=c*(1.f/q.size());Vec3 n=norm(cross(d.V[q[1]]-d.V[q[0]],d.V[q[2]]-d.V[q[0]]));if(dot(n,c-away)<0)std::reverse(q.begin(),q.end());return q;};
  for(int i=0;i+1<S;i++)for(int j=0;j+1<nr;j++){Vec3 zc{0,0.75f,(st[i]+st[i+1])*0.5f*-1.f};zc.z=-zc.z;d.F.push_back(outward({id(i,j),id(i,j+1),id(i+1,j+1),id(i+1,j)},{0,0.7f,(st[i]+st[i+1])*0.5f}));}
  for(int i=0;i+1<S;i++)for(int j:creaseRings)if(j+1<nr){d.crease[PolyMesh::key(id(i,j),id(i+1,j))]=cr;}
  if(caps){for(int e:{0,S-1})for(int j=0;j+1<nr;j++)d.crease[PolyMesh::key(id(e,j),id(e,j+1))]=2.f;
    for(int e:{0,S-1}){std::vector<int> q;for(int j=0;j<nr;j++)q.push_back(id(e,j));float zz=st[e];d.F.push_back(outward(q,{0,0.7f,zz>0?zz-1.f:zz+1.f}));}}
}
// cross-section half-width vs height, measured from the blueprint FRONT view silhouette (metres)
static const float WFT[][2]={{0.0f,.91f},{.05f,.93f},{.12f,.937f},{.40f,.937f},{.70f,.937f},{.80f,.93f},{.90f,.895f},{.99f,.87f},{1.06f,.82f},{1.13f,.74f},{1.20f,.68f},{1.27f,.65f},{1.34f,.60f},{1.38f,.54f},{1.403f,.40f}};
static float wf(float h){const int N=sizeof(WFT)/sizeof(WFT[0]);if(h<=WFT[0][0])return WFT[0][1];for(int i=0;i+1<N;i++)if(h<=WFT[i+1][0]){float u=(h-WFT[i][0])/(WFT[i+1][0]-WFT[i][0]);return WFT[i][1]*(1-u)+WFT[i+1][1]*u;}return WFT[N-1][1];}
// resample a polyline to n points evenly by arc length (first and last kept)
static std::vector<Vec3> resample(const std::vector<Vec3>& p,int n){std::vector<float> L(1,0.f);for(size_t i=1;i<p.size();i++)L.push_back(L.back()+std::sqrt(dot(p[i]-p[i-1],p[i]-p[i-1]))+1e-7f);std::vector<Vec3> o;float tot=L.back();size_t k=0;
  for(int i=0;i<n;i++){float t=tot*i/(n-1);while(k+2<p.size()&&L[k+1]<t)k++;float u=(t-L[k])/(L[k+1]-L[k]);u=std::max(0.f,std::min(1.f,u));o.push_back(p[k]*(1-u)+p[k+1]*u);}return o;}
static std::vector<float> carStations(){std::vector<float> st;for(float f:{2.397f,2.36f,2.31f,2.24f})st.push_back(f);for(float f=2.10f;f>-2.0f;f-=0.18f)st.push_back(f);for(float f:{-2.10f,-2.19f,-2.26f,-2.32f,-2.365f,-2.397f})st.push_back(f);return st;}
static void buildCarOnce(PolyMesh& pm,const std::vector<float>& corr){
  pm.clear();pm.mirrorX=true;pm.subLevels=3;
  const float BELT=0.985f;const int N1=10,N2=9;const int NR=N1+N2-1; // lower piece N1 points (last = shoulder), upper piece N2 points (first = shoulder, last = centre)
  std::vector<float> st=carStations();
  auto cf=[&](float f){if(corr.empty())return 1.f;size_t bi=0;float bd=1e9f;for(size_t i=0;i<st.size();i++){float d=std::fabs(st[i]-f);if(d<bd){bd=d;bi=i;}}return corr[bi];};
  addLoft(pm,st,NR,[&](float f,std::vector<Vec3>& out){
      float hw=halfW(f),H=topH(f);if(H>BELT+0.03f)H=BELT+(H-BELT)*cf(f);else H*=cf(f);float zb=0.16f+(std::fabs(f)<2.0f?0.f:0.10f*(std::fabs(f)-2.0f)/0.4f);bool green=H>BELT+0.03f;
      float hb=green?BELT:std::max(zb+0.2f,H-0.07f);
      std::vector<Vec3> lo;lo.push_back({0,zb,f});lo.push_back({0.80f*hw,zb,f});lo.push_back({0.955f*hw,zb+0.05f,f});
      for(int k=0;k<=8;k++){float h=zb+0.10f+(hb-zb-0.10f)*k/8.f;lo.push_back({wf(h)/0.937f*hw,h,f});}
      std::vector<Vec3> up;
      if(green){auto tp=[](float t){t=std::max(0.f,std::min(1.f,t));return t*t*(3-2*t);};float gk=0.55f+0.45f*tp((f+1.8f)/0.7f)*tp((1.15f-f)/0.5f);
        up.push_back(lo.back());for(int k=1;k<=10;k++){float h=hb+(H-hb)*k/10.f;float u=wf(h)/0.937f*hw*(h<hb+0.01f?1.f:gk);if(k==10)u*=0.55f;up.push_back({u,h,f});}up.push_back({0,H,f});}
      else{up.push_back(lo.back());up.push_back({0.975f*hw,H-0.035f,f});up.push_back({0.90f*hw,H-0.008f,f});up.push_back({0.55f*hw,H-0.002f,f});up.push_back({0,H,f});}
      std::vector<Vec3> a=resample(lo,N1),b=resample(up,N2);for(auto& v:a)out.push_back(v);for(size_t i=1;i<b.size();i++)out.push_back(b[i]);},{2,N1-1,N1},1.f,true);
  pm.selectNone();}
// build, then compare the subdivided top silhouette with the blueprint and re-fit station heights (2 passes)
void buildCarPoly(PolyMesh& pm){
  std::vector<float> st=carStations();std::vector<float> corr(st.size(),1.f);
  for(int it=0;it<3;it++){buildCarOnce(pm,corr);PolyData e=pm.evaluate(pm.subLevels);
    for(size_t i=0;i<st.size();i++){float best=0;for(auto& v:e.V)if(std::fabs(v.z-st[i])<0.075f&&v.y>best)best=v.y;if(best<0.3f)continue;float want=topH(st[i]);corr[i]=std::max(0.85f,std::min(1.35f,corr[i]*want/best));}}
  buildCarOnce(pm,corr);}

// ---- materials: 0 paint 1 glass 2 chrome 3 black 4 rubber 5 lamp 6 tail-lamp 7 leather

// split every polygon (on a car side) that crosses the plane axis=val, so door borders become exact edges
static void splitPlane(PolyData& d,int axis,float val,float minAx){
  auto g=[&](Vec3 p){return axis==0?p.x:axis==1?p.y:p.z;};
  size_t nf=d.F.size();
  for(size_t i=0;i<nf;i++){auto F=d.F[i];Vec3 c{0,0,0};for(int v:F)c=c+d.V[v];c=c*(1.f/F.size());if(std::fabs(c.x)<minAx)continue;
    bool neg=false,pos=false;for(int v:F){float t=g(d.V[v])-val;if(t<-1e-5f)neg=true;if(t>1e-5f)pos=true;}if(!(neg&&pos))continue;
    std::vector<int> A,B;size_t n=F.size();
    for(size_t k=0;k<n;k++){int a=F[k],b=F[(k+1)%n];float ta=g(d.V[a])-val,tb=g(d.V[b])-val;
      if(ta<=1e-5f)A.push_back(a);if(ta>=-1e-5f)B.push_back(a);
      if((ta<-1e-5f&&tb>1e-5f)||(ta>1e-5f&&tb<-1e-5f)){float u=ta/(ta-tb);Vec3 p=d.V[a]+(d.V[b]-d.V[a])*u;int id=(int)d.V.size();d.V.push_back(p);A.push_back(id);B.push_back(id);}}
    unsigned char mt=i<d.M.size()?d.M[i]:0;
    if(A.size()<3||B.size()<3)continue;d.F[i]=A;d.F.push_back(B);d.M.resize(d.F.size()-1,0);d.M.push_back(mt);}}
static void paintCar(PolyMesh& pm){
  pm.d.M.resize(pm.d.F.size(),0);splitPlane(pm.d,2,0.70f,0.7f);splitPlane(pm.d,2,-0.75f,0.7f);splitPlane(pm.d,1,0.30f,0.7f);splitPlane(pm.d,1,1.40f,0.7f);splitPlane(pm.d,1,0.99f,0.f);splitPlane(pm.d,2,0.30f,0.f);splitPlane(pm.d,2,1.10f,0.f);splitPlane(pm.d,2,-0.95f,0.f);splitPlane(pm.d,2,-1.55f,0.f);splitPlane(pm.d,2,-0.30f,0.5f);splitPlane(pm.d,2,-0.05f,0.5f);splitPlane(pm.d,2,-1.15f,0.5f);splitPlane(pm.d,2,0.80f,0.5f);
  PolyData& d=pm.d;d.M.assign(d.F.size(),0);
  for(size_t i=0;i<d.F.size();i++){auto& f=d.F[i];Vec3 c{0,0,0};for(int v:f)c=c+d.V[v];c=c*(1.f/f.size());Vec3 n=pm.faceNormal((int)i);float ax=std::fabs(c.x);int m=0;
    float dF=std::hypot(c.y-0.35f,c.z-1.48f),dR=std::hypot(c.y-0.35f,c.z+1.377f);
    if(ax>0.62f&&(dF<0.47f||dR<0.47f)&&c.y<0.85f)m=3;                       // inside of wheel arch
    else if(c.y<0.27f)m=3;                                                   // under-body / splitter / diffuser
    else if(c.y>0.99f&&c.y<1.40f){
      bool wind=c.z>0.30f&&c.z<1.10f&&n.z>0.35f&&n.y<0.8f;
      bool rear=c.z<-0.95f&&c.z>-1.55f&&n.z<-0.35f&&n.y<0.8f;
      bool side=std::fabs(n.x)>0.45f&&c.z>-1.15f&&c.z<0.80f&&!(c.z>-0.30f&&c.z<-0.05f);
      if(wind||rear||side)m=1;}
    d.M[i]=(unsigned char)m;}}
static void addPart(PolyMesh& pm,float cx,float cz,float r,float w,int n,int mat){
  PolyMesh c;c.cylinder({0,0,0},r,w,n);PolyData cd=c.d;size_t b=pm.d.V.size();
  for(auto& v:cd.V){float x=v.y,y=v.x,z=v.z;pm.d.V.push_back({cx+x,0.355f+y,cz+z});}
  for(auto f:cd.F){std::reverse(f.begin(),f.end());for(int& k:f)k+=(int)b;pm.d.F.push_back(f);pm.d.M.push_back((unsigned char)mat);}}
static void addLathe(PolyMesh& pm,float cx,float cz,int s,const std::vector<std::pair<float,float>>& prof,int segs,int mat){
  size_t b=pm.d.V.size();size_t np=prof.size();
  for(int k=0;k<segs;k++){float a=6.2831853f*k/segs;for(auto& p:prof)pm.d.V.push_back({cx+s*p.first,0.355f+p.second*std::cos(a),cz+p.second*std::sin(a)});}
  std::vector<std::vector<int>> fs;
  for(int k=0;k<segs;k++){int k2=(k+1)%segs;for(size_t i=0;i<np;i++){size_t j=(i+1)%np;fs.push_back({(int)(b+k*np+i),(int)(b+k*np+j),(int)(b+k2*np+j),(int)(b+k2*np+i)});}}
  double vol=0;for(auto& f:fs){Vec3 p0=pm.d.V[f[0]];for(size_t i=1;i+1<f.size();i++){Vec3 p1=pm.d.V[f[i]],p2=pm.d.V[f[i+1]];vol+=dot(p0,cross(p1,p2));}}
  if(vol<0)for(auto& f:fs)std::reverse(f.begin(),f.end());
  for(auto& f:fs){pm.d.F.push_back(f);pm.d.M.push_back((unsigned char)mat);}}
static void addSpoke(PolyMesh& pm,float cx,float cz,int s,float ang,float r0,float r1,float hw,float xo,float ht,int mat){
  Vec3 P[8];int q=0;for(int xs=0;xs<2;xs++)for(int ys=0;ys<2;ys++)for(int zs=0;zs<2;zs++){float rr=ys?r1:r0,w=zs?hw:-hw,xx=xo+(xs?ht:-ht);
    float y=rr*std::cos(ang)-w*std::sin(ang),z=rr*std::sin(ang)+w*std::cos(ang);P[q++]={cx+s*xx,0.355f+y,cz+z};}
  size_t b=pm.d.V.size();for(auto& p:P)pm.d.V.push_back(p);
  static const int Q[6][4]={{0,2,3,1},{4,5,7,6},{0,1,5,4},{2,6,7,3},{0,4,6,2},{1,3,7,5}};
  std::vector<std::vector<int>> fs;for(auto& qq:Q)fs.push_back({(int)b+qq[0],(int)b+qq[1],(int)b+qq[2],(int)b+qq[3]});
  double vol=0;for(auto& f:fs){Vec3 p0=pm.d.V[f[0]];for(size_t i=1;i+1<f.size();i++)vol+=dot(p0,cross(pm.d.V[f[i]],pm.d.V[f[i+1]]));}
  if(vol<0)for(auto& f:fs)std::reverse(f.begin(),f.end());for(auto& f:fs){pm.d.F.push_back(f);pm.d.M.push_back((unsigned char)mat);}}
static void addWheels(PolyMesh& pm){
  const std::vector<std::pair<float,float>> tyre={{-0.11f,0.27f},{-0.125f,0.30f},{-0.12f,0.335f},{-0.09f,0.352f},{-0.04f,0.358f},{0.04f,0.358f},{0.09f,0.352f},{0.12f,0.335f},{0.125f,0.30f},{0.11f,0.27f},{0.09f,0.20f},{-0.09f,0.20f}};
  const std::vector<std::pair<float,float>> rim={{0.112f,0.268f},{0.124f,0.258f},{0.122f,0.235f},{0.108f,0.215f},{0.110f,0.075f},{0.135f,0.062f},{0.135f,0.0f},{-0.10f,0.0f},{-0.10f,0.262f}};
  const std::vector<std::pair<float,float>> dish={{0.100f,0.236f},{0.100f,0.0f},{0.088f,0.0f},{0.088f,0.236f}};
  for(float zf:{1.48f,-1.377f})for(int s=-1;s<=1;s+=2){float x=s*0.80f;
    addLathe(pm,x,zf,s,tyre,72,4);addLathe(pm,x,zf,s,dish,48,3);addLathe(pm,x,zf,s,rim,48,2);
    for(int k=0;k<10;k++)addSpoke(pm,x,zf,s,6.2831853f*k/10.f,0.07f,0.245f,0.017f,0.122f,0.007f,2);
    for(int k=0;k<5;k++)addSpoke(pm,x,zf,s,6.2831853f*(k+0.5f)/5.f,0.02f,0.07f,0.03f,0.118f,0.005f,3);}}

// ---- decals: grid patches projected (along z) onto the body so lamps / grilles / intakes follow the real surface
struct Tri{Vec3 a,b,c;};
struct TGrid{std::vector<Tri> t;std::vector<std::vector<int>> cell;float x0=-1.2f,y0=0.f,cs=0.04f;int nx=61,ny=40;
  void build(){cell.assign((size_t)nx*ny,{});for(size_t i=0;i<t.size();i++){auto& a=t[i];float mnx=std::min(a.a.x,std::min(a.b.x,a.c.x)),mxx=std::max(a.a.x,std::max(a.b.x,a.c.x)),mny=std::min(a.a.y,std::min(a.b.y,a.c.y)),mxy=std::max(a.a.y,std::max(a.b.y,a.c.y));
      int i0=std::max(0,(int)((mnx-x0)/cs)),i1=std::min(nx-1,(int)((mxx-x0)/cs)),j0=std::max(0,(int)((mny-y0)/cs)),j1=std::min(ny-1,(int)((mxy-y0)/cs));for(int j=j0;j<=j1;j++)for(int k=i0;k<=i1;k++)cell[(size_t)j*nx+k].push_back((int)i);}}};
static bool zHit(const TGrid& G,float x,float y,int side,float& zo){
  bool got=false;float best=side>0?-1e9f:1e9f;int ci=(int)((x-G.x0)/G.cs),cj=(int)((y-G.y0)/G.cs);if(ci<0||cj<0||ci>=G.nx||cj>=G.ny)return false;
  for(int id:G.cell[(size_t)cj*G.nx+ci]){auto& t=G.t[id];
    float d=(t.b.y-t.c.y)*(t.a.x-t.c.x)+(t.c.x-t.b.x)*(t.a.y-t.c.y);if(std::fabs(d)<1e-9f)continue;
    float l1=((t.b.y-t.c.y)*(x-t.c.x)+(t.c.x-t.b.x)*(y-t.c.y))/d,l2=((t.c.y-t.a.y)*(x-t.c.x)+(t.a.x-t.c.x)*(y-t.c.y))/d,l3=1-l1-l2;
    if(l1<-1e-4f||l2<-1e-4f||l3<-1e-4f)continue;float z=l1*t.a.z+l2*t.b.z+l3*t.c.z;
    if(side>0?z>best:z<best){best=z;got=true;}}
  zo=best;return got;}
static void addDecal(PolyMesh& pm,const TGrid& T,int side,float x0,float x1,float y0,float y1,float step,float off,int mat,const std::function<bool(float,float)>& in){
  int nx=(int)std::ceil((x1-x0)/step),ny=(int)std::ceil((y1-y0)/step);float sx=(x1-x0)/nx,sy=(y1-y0)/ny;
  std::vector<float> Z((nx+1)*(ny+1));std::vector<char> ok((nx+1)*(ny+1),0);
  for(int j=0;j<=ny;j++)for(int i=0;i<=nx;i++){float z;if(zHit(T,x0+i*sx,y0+j*sy,side,z)){Z[j*(nx+1)+i]=z+side*off;ok[j*(nx+1)+i]=1;}}
  for(int j=0;j<ny;j++)for(int i=0;i<nx;i++){float cx=x0+(i+0.5f)*sx,cy=y0+(j+0.5f)*sy;if(!in(cx,cy))continue;
    int id[4]={j*(nx+1)+i,j*(nx+1)+i+1,(j+1)*(nx+1)+i+1,(j+1)*(nx+1)+i};if(!(ok[id[0]]&&ok[id[1]]&&ok[id[2]]&&ok[id[3]]))continue;
    float px[4]={x0+i*sx,x0+(i+1)*sx,x0+(i+1)*sx,x0+i*sx},py[4]={y0+j*sy,y0+j*sy,y0+(j+1)*sy,y0+(j+1)*sy};
    int b=(int)pm.d.V.size();std::vector<int> f;for(int k=0;k<4;k++){pm.d.V.push_back({px[k],py[k],Z[id[k]]});f.push_back(b+k);}
    if(side<0)std::reverse(f.begin(),f.end());pm.d.F.push_back(f);pm.d.M.push_back((unsigned char)mat);}}
static void addDecals(PolyMesh& pm){
  TGrid T;for(size_t i=0;i<pm.d.F.size();i++){unsigned char m=i<pm.d.M.size()?pm.d.M[i]:0;if(m!=0&&m!=1&&m!=3)continue;auto& f=pm.d.F[i];if(f.size()<3)continue;Vec3 n=pm.faceNormal((int)i);if(std::fabs(n.z)<0.15f)continue;
    for(size_t k=1;k+1<f.size();k++)T.t.push_back({pm.d.V[f[0]],pm.d.V[f[k]],pm.d.V[f[k+1]]});}
  T.build();const float st=0.010f;
  // kidney grilles: chrome frame + black inside, wider at the bottom
  for(int s=-1;s<=1;s+=2){
    auto fr=[s](float x,float y){float ax=std::fabs(x);return ax>0.035f&&ax<0.43f-(y-0.36f)*0.10f&&y>0.36f&&y<0.72f;};
    auto in=[s](float x,float y){float ax=std::fabs(x);return ax>0.060f&&ax<0.405f-(y-0.36f)*0.10f&&y>0.385f&&y<0.695f;};
    auto bar=[s](float x,float y){return std::fmod(y*40.f,2.f)<0.55f;};
    float xa=s>0?0.02f:-0.45f,xb=s>0?0.45f:-0.02f;
    addDecal(pm,T,1,xa,xb,0.35f,0.73f,st,0.006f,2,fr);
    addDecal(pm,T,1,xa,xb,0.35f,0.73f,st,0.010f,3,in);
    addDecal(pm,T,1,xa,xb,0.35f,0.73f,st,0.014f,2,[&](float x,float y){return in(x,y)&&bar(x,y);});}
  // slim angled headlights
  for(int s=-1;s<=1;s+=2){float xa=s>0?0.40f:-0.92f,xb=s>0?0.92f:-0.40f;
    auto in=[](float x,float y){float ax=std::fabs(x);float lo=0.62f+0.30f*(ax-0.46f);return ax>0.46f&&ax<0.88f&&y>lo&&y<lo+0.075f-(ax-0.46f)*0.05f;};
    addDecal(pm,T,1,xa,xb,0.55f,0.86f,0.009f,0.008f,5,in);
    auto lo2=[](float x,float y){float ax=std::fabs(x);float lo=0.62f+0.30f*(ax-0.46f);return ax>0.46f&&ax<0.88f&&y>lo-0.012f&&y<lo;};
    addDecal(pm,T,1,xa,xb,0.55f,0.86f,0.009f,0.010f,3,lo2);}
  // lower intakes
  for(int s=-1;s<=1;s+=2){float xa=s>0?0.30f:-0.90f,xb=s>0?0.90f:-0.30f;
    addDecal(pm,T,1,xa,xb,0.20f,0.44f,0.008f,0.008f,3,[](float x,float y){float ax=std::fabs(x);return ax>0.40f&&ax<0.85f&&y>0.24f+0.18f*(ax-0.40f)*0&&y<0.40f-(ax-0.40f)*0.12f;});}
  addDecal(pm,T,1,-0.42f,0.42f,0.20f,0.36f,0.008f,0.008f,3,[](float x,float y){return std::fabs(x)<0.40f&&y>0.24f&&y<0.33f;});
  // rear: L-shaped tail-lights + diffuser bars
  for(int s=-1;s<=1;s+=2){float xa=s>0?0.35f:-0.92f,xb=s>0?0.92f:-0.35f;
    addDecal(pm,T,-1,xa,xb,0.66f,1.02f,0.007f,0.008f,6,[](float x,float y){float ax=std::fabs(x);return ax>0.42f&&ax<0.88f&&((y>0.82f&&y<0.90f)||(ax>0.74f&&y>0.72f&&y<0.92f));});}
  addDecal(pm,T,-1,-0.85f,0.85f,0.22f,0.45f,0.008f,0.008f,3,[](float x,float y){return std::fabs(x)<0.82f&&y>0.26f&&y<0.38f;});
}

// joints: 0 body, 1..4 wheels (+x front, -x front, +x rear, -x rear), 5 door +x, 6 door -x
int carPartAt(Vec3 c,Vec3 n,int mat){
  float ax=std::fabs(c.x);float zs[2]={1.48f,-1.377f};
  if(ax>0.62f&&ax<0.99f&&(mat==2||mat==3||mat==4))for(int k=0;k<2;k++)if(std::hypot(c.y-0.355f,c.z-zs[k])<0.372f)return 1+k*2+(c.x<0?1:0);
  if((mat==0||mat==1)&&ax>0.70f&&std::fabs(n.x)>0.4f&&c.y>0.30f&&c.y<1.40f&&c.z>-0.75f&&c.z<0.70f)return c.x>0?5:6;
  return 0;}
static void addBox(PolyMesh& pm,Vec3 c,Vec3 h,int mat){PolyMesh b;b.cube(c,h);size_t o=pm.d.V.size();for(auto v:b.d.V)pm.d.V.push_back(v);for(auto f:b.d.F){for(int& k:f)k+=(int)o;pm.d.F.push_back(f);pm.d.M.push_back((unsigned char)mat);}}
static void addExtras(PolyMesh& pm){
  for(int s=-1;s<=1;s+=2){float x=s*0.95f;
    addBox(pm,{x+s*0.07f,1.02f,0.62f},{0.08f,0.045f,0.07f},0);       // mirror housing
    addBox(pm,{x+s*0.02f,0.93f,0.62f},{0.015f,0.04f,0.06f},3);       // mirror stalk
    addBox(pm,{s*0.945f,0.86f,-0.28f},{0.012f,0.012f,0.07f},3);      // door handle
    addBox(pm,{s*0.940f,0.60f,0.70f},{0.004f,0.30f,0.004f},3);       // door gap front
    addBox(pm,{s*0.940f,0.60f,-0.78f},{0.004f,0.30f,0.004f},3);}     // door gap rear
  addBox(pm,{0,0.58f,2.39f},{0.14f,0.006f,0.01f},3);
  // interior: floor, dashboard, wheel, front seats, rear bench
  addBox(pm,{0,0.30f,-0.20f},{0.80f,0.02f,1.0f},3);
  addBox(pm,{0,0.80f,0.52f},{0.78f,0.09f,0.13f},3);
  addBox(pm,{-0.38f,0.84f,0.36f},{0.16f,0.16f,0.012f},3);
  for(int s=-1;s<=1;s+=2){addBox(pm,{s*0.38f,0.44f,-0.10f},{0.21f,0.07f,0.24f},7);addBox(pm,{s*0.38f,0.70f,-0.36f},{0.21f,0.27f,0.06f},7);addBox(pm,{s*0.38f,0.99f,-0.38f},{0.12f,0.07f,0.04f},7);}
  addBox(pm,{0,0.44f,-0.88f},{0.62f,0.07f,0.20f},7);addBox(pm,{0,0.68f,-1.06f},{0.62f,0.24f,0.05f},7);}
void bakeWheelArches(PolyMesh& pm){
  PolyData A=pm.evaluate(pm.subLevels);PolyMesh cu;cu.mirrorX=false;
  for(float zf:{1.48f,-1.377f})for(int s=-1;s<=1;s+=2){PolyMesh c;c.cylinder({0,0,0},0.43f,0.45f,40);PolyData cd=c.d; // axis Y -> rotate to X
    for(auto& v:cd.V){float x=v.y,y=v.x,z=v.z;v={s*0.88f+x,0.35f+y,zf+z};}for(auto& f:cd.F)std::reverse(f.begin(),f.end());A=PolyMesh::boolean(A,cd,0);}
  pm.d=A;pm.mirrorX=false;pm.subLevels=0;pm.selectNone();paintCar(pm);addWheels(pm);addExtras(pm);addDecals(pm);pm.rig=true;}
}
