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
  if(caps){for(int e:{0,S-1}){std::vector<int> q;for(int j=0;j<nr;j++)q.push_back(id(e,j));float zz=st[e];d.F.push_back(outward(q,{0,0.7f,zz>0?zz-1.f:zz+1.f}));}}
}
void buildCarPoly(PolyMesh& pm){
  pm.clear();pm.mirrorX=true;pm.subLevels=2;
  std::vector<float> stB={2.397f,2.36f,2.30f,2.20f,2.05f,1.85f,1.65f,1.45f,1.25f,1.05f,.85f,.6f,.3f,0.f,-.3f,-.6f,-.9f,-1.2f,-1.5f,-1.8f,-2.0f,-2.15f,-2.28f,-2.397f};
  const float BELT=0.985f;
  addLoft(pm,stB,10,[&](float f,std::vector<Vec3>& p){float hw=halfW(f),deck=std::min(topH(f),BELT);float zb=0.16f+(std::fabs(f)<2.0f?0.f:0.10f*(std::fabs(f)-2.0f)/0.4f);
      float zs[10]={zb,zb,zb+0.06f,0.40f,0.68f,deck-0.12f,deck-0.03f,deck-0.006f,deck-0.004f,deck};float us[10]={0,0.80f,0.96f,1.0f,1.0f,0.985f,0.92f,deck>0.95f?0.865f:0.6f,0.45f,0};
      for(int k=0;k<10;k++)p.push_back({us[k]*hw,zs[k],f});},{2,6},1.f,true);
  std::vector<float> stC={1.0f,.9f,.75f,.55f,.35f,.1f,-.15f,-.4f,-.65f,-.9f,-1.1f,-1.3f,-1.5f,-1.65f,-1.78f};
  addLoft(pm,stC,6,[&](float f,std::vector<Vec3>& p){auto tp=[](float t){t=std::max(0.f,std::min(1.f,t));return t*t*(3-2*t);};float hw=halfW(f)*(0.45f+0.55f*tp((f+1.8f)/0.8f)*tp((1.04f-f)/0.55f));float roof=std::max(topH(f),BELT+0.02f);float base=BELT-0.01f;
      float us[6]={0,0.86f,0.80f,0.66f,0.40f,0};float zs[6]={base,base,base+0.10f*(roof-base)/0.4f+0.12f,roof-0.07f,roof-0.015f,roof};for(int k=0;k<6;k++)p.push_back({us[k]*hw,zs[k],f});},{1,3},1.f,true);
  pm.selectNone();}
void bakeWheelArches(PolyMesh& pm){
  PolyData A=pm.evaluate(pm.subLevels);PolyMesh cu;cu.mirrorX=false;
  for(float zf:{1.48f,-1.377f})for(int s=-1;s<=1;s+=2){PolyMesh c;c.cylinder({0,0,0},0.43f,0.45f,40);PolyData cd=c.d; // axis Y -> rotate to X
    for(auto& v:cd.V){float x=v.y,y=v.x,z=v.z;v={s*0.88f+x,0.35f+y,zf+z};}for(auto& f:cd.F)std::reverse(f.begin(),f.end());A=PolyMesh::boolean(A,cd,0);}
  pm.d=A;pm.mirrorX=false;pm.subLevels=0;pm.selectNone();}
}
