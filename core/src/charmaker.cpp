// Sabak Core - parametric character generator: builds a humanoid from sliders/colors, then auto-rigs it.
#include <cmath>
#include <array>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include "model.h"
namespace sk {
namespace {
const float PI=3.14159265f;
struct MB{   // mesh builder for one material
  std::vector<SkinVertex> v;std::vector<uint32_t> idx;std::vector<Vec3> n;
  uint32_t add(Vec3 p,float u=0,float w=0){SkinVertex s{};s.p[0]=p.x;s.p[1]=p.y;s.p[2]=p.z;s.uv[0]=u;s.uv[1]=w;v.push_back(s);return (uint32_t)v.size()-1;}
  void tri(uint32_t a,uint32_t b,uint32_t c){idx.push_back(a);idx.push_back(b);idx.push_back(c);}
  void finish(){std::vector<Vec3> nn(v.size(),Vec3{0,0,0});for(size_t i=0;i+2<idx.size();i+=3){Vec3 a{v[idx[i]].p[0],v[idx[i]].p[1],v[idx[i]].p[2]},b{v[idx[i+1]].p[0],v[idx[i+1]].p[1],v[idx[i+1]].p[2]},c{v[idx[i+2]].p[0],v[idx[i+2]].p[1],v[idx[i+2]].p[2]};
      Vec3 e1=b-a,e2=c-a;Vec3 f{e1.y*e2.z-e1.z*e2.y,e1.z*e2.x-e1.x*e2.z,e1.x*e2.y-e1.y*e2.x};for(int k=0;k<3;k++){nn[idx[i+k]]=nn[idx[i+k]]+f;}}
    for(size_t i=0;i<v.size();i++){float l=std::sqrt(nn[i].x*nn[i].x+nn[i].y*nn[i].y+nn[i].z*nn[i].z);if(l<1e-9f){v[i].n[0]=0;v[i].n[1]=1;v[i].n[2]=0;}else{v[i].n[0]=nn[i].x/l;v[i].n[1]=nn[i].y/l;v[i].n[2]=nn[i].z/l;}}}
  // ellipsoid (optionally only the part above latMax*PI from the top pole)
  void ellipsoid(Vec3 c,Vec3 r,float latMax=1.f,int seg=18,int rings=12){uint32_t base=(uint32_t)v.size();
    for(int j=0;j<=rings;j++){float ph=PI*latMax*j/rings;for(int i=0;i<=seg;i++){float th=2*PI*i/seg;add({c.x+r.x*std::sin(ph)*std::cos(th),c.y+r.y*std::cos(ph),c.z+r.z*std::sin(ph)*std::sin(th)},(float)i/seg,(float)j/rings);}}
    for(int j=0;j<rings;j++)for(int i=0;i<seg;i++){uint32_t a=base+j*(seg+1)+i,b=a+1,c2=a+seg+1,d=c2+1;tri(a,c2,b);tri(b,c2,d);}}
  // tube between two points with circular rings of radius ra..rb
  void tube(Vec3 a,Vec3 b,float ra,float rb,int seg=14,int rings=4){Vec3 d=b-a;float L=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);if(L<1e-6f)return;d=d*(1.f/L);Vec3 up=std::fabs(d.y)<0.9f?Vec3{0,1,0}:Vec3{1,0,0};
    Vec3 u{d.y*up.z-d.z*up.y,d.z*up.x-d.x*up.z,d.x*up.y-d.y*up.x};float ul=std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z);u=u*(1.f/ul);Vec3 w{d.y*u.z-d.z*u.y,d.z*u.x-d.x*u.z,d.x*u.y-d.y*u.x};
    uint32_t base=(uint32_t)v.size();for(int j=0;j<=rings;j++){float t=(float)j/rings;Vec3 c=a+(b-a)*t;float r=ra+(rb-ra)*t;for(int i=0;i<=seg;i++){float th=2*PI*i/seg;add(c+u*(r*std::cos(th))+w*(r*std::sin(th)),(float)i/seg,t);}}
    for(int j=0;j<rings;j++)for(int i=0;i<seg;i++){uint32_t p=base+j*(seg+1)+i,q=p+1,r=p+seg+1,s=r+1;tri(p,q,r);tri(q,s,r);}
    ellipsoid(a,{ra,ra,ra},1.f,12,8);ellipsoid(b,{rb,rb,rb},1.f,12,8);}
  // loft of elliptical rings stacked along Y: {y,rx,rz,cz}
  void loft(const std::vector<std::array<float,4>>& rs,int seg=20){uint32_t base=(uint32_t)v.size();int R=(int)rs.size();
    for(int j=0;j<R;j++)for(int i=0;i<=seg;i++){float th=2*PI*i/seg;add({rs[j][1]*std::cos(th),rs[j][0],rs[j][3]+rs[j][2]*std::sin(th)},(float)i/seg,(float)j/(R-1));}
    for(int j=0;j<R-1;j++)for(int i=0;i<seg;i++){uint32_t p=base+j*(seg+1)+i,q=p+1,r=p+seg+1,s=r+1;tri(p,r,q);tri(q,r,s);}
    for(int cap=0;cap<2;cap++){int j=cap?R-1:0;uint32_t c=add({0,rs[j][0],rs[j][3]});for(int i=0;i<seg;i++){uint32_t a=base+j*(seg+1)+i,b=a+1;if(cap)tri(c,b,a);else tri(c,a,b);}}}
};
}
bool CharDef::save(const std::string& path) const{std::error_code ec;std::filesystem::create_directories(std::filesystem::path(path).parent_path(),ec);std::ofstream f(path);if(!f)return false;
  f<<"SABAKCHAR 1\n"<<height<<' '<<build<<' '<<muscle<<' '<<head<<' '<<shoulder<<' '<<hair<<' '<<outfit<<'\n';for(int i=0;i<5;i++)f<<col[i]<<(i<4?' ':'\n');return true;}
bool CharDef::load(const std::string& path){std::ifstream f(path);if(!f)return false;std::string h;std::getline(f,h);if(h.rfind("SABAKCHAR",0)!=0)return false;
  f>>height>>build>>muscle>>head>>shoulder>>hair>>outfit;for(int i=0;i<5;i++)f>>col[i];return (bool)f;}
bool buildCharacter(Renderer& r,const CharDef& cd,Model& out){
  const float H=1.8f;float b=0.8f+0.5f*cd.build,mu=0.85f+0.5f*cd.muscle,hs=0.85f+0.4f*cd.head,sh=0.85f+0.35f*cd.shoulder;
  MB skin,hair,shirt,pants,shoes;bool sleeveless=cd.outfit==1,naked=cd.outfit==2;MB& top=naked?skin:shirt;MB& bot=naked?skin:pants;MB& foot=naked?skin:shoes;
  auto P=[&](float x,float y,float z){return Vec3{x*H,y*H,z*H};};
  // torso: hips to neck base
  top.loft({{.455f*H,.092f*H*b,.070f*H*b,0},{.50f*H,.104f*H*b,.078f*H*b,0},{.58f*H,.090f*H*b,.068f*H*b,0},{.68f*H,.100f*H*b,.072f*H*b,.004f*H},{.76f*H,.122f*H*sh*b,.068f*H*b,.002f*H},{.805f*H,.100f*H*sh,.055f*H,0},{.822f*H,.040f*H,.040f*H,0}});
  skin.tube(P(0,.815f,0),P(0,.865f,0.004f),.034f*H,.030f*H);
  skin.ellipsoid(P(0,.925f,.006f),{.062f*H*hs,.076f*H*hs,.068f*H*hs},1.f,22,14);
  skin.ellipsoid(P(0,.905f,.050f*hs),{.012f*H,.012f*H,.014f*H},1.f,8,6);   // nose
  // arms (A-pose, matching the auto-rig joints)
  for(int s=-1;s<=1;s+=2){float x=(float)s;
    MB& ua=sleeveless?skin:shirt;ua.tube(P(x*.12f,.79f,0),P(x*.20f,.61f,0),.040f*H*mu,.032f*H*mu);
    skin.tube(P(x*.20f,.61f,0),P(x*.27f,.46f,.005f),.031f*H*mu,.022f*H*mu);
    skin.ellipsoid(P(x*.285f,.425f,.01f),{.020f*H,.040f*H,.016f*H},1.f,10,8);
    bot.tube(P(x*.06f,.50f,0),P(x*.065f,.27f,.004f),.066f*H*mu*b,.046f*H*mu);
    MB& shin=(sleeveless&&!naked)?skin:bot;shin.tube(P(x*.065f,.27f,.004f),P(x*.065f,.05f,0),.044f*H*mu,.032f*H);
    foot.ellipsoid(P(x*.065f,.03f,.045f),{.036f*H,.030f*H,.078f*H},1.f,12,8);}
  // hair
  if(cd.hair==1||cd.hair==2||cd.hair==4){hair.ellipsoid(P(0,.932f,-.004f),{.067f*H*hs,.078f*H*hs,.072f*H*hs},0.62f,22,12);}
  if(cd.hair==2){hair.tube(P(0,.90f,-.062f*hs),P(0,.77f,-.07f),.034f*H,.026f*H);}
  if(cd.hair==4){hair.ellipsoid(P(0,.995f,-.02f),{.032f*H,.032f*H,.032f*H},1.f,12,8);}
  if(cd.hair==3){hair.ellipsoid(P(0,.934f,-.006f),{.068f*H*hs,.080f*H*hs,.073f*H*hs},0.48f,22,12);hair.tube(P(-.058f*hs,.92f,0),P(-.052f*hs,.82f,-.02f),.012f*H,.010f*H);hair.tube(P(.058f*hs,.92f,0),P(.052f*hs,.82f,-.02f),.012f*H,.010f*H);}
  std::vector<MB*> parts={&skin,&hair,&shirt,&pants,&shoes};std::vector<RigPrim> ps;out=Model();out.name=cd.name;
  std::vector<unsigned char> white{255,255,255,255};unsigned wt=r.createTexture(white.data(),1,1);
  for(int i=0;i<5;i++){MB& m=*parts[i];if(m.idx.empty())continue;m.finish();ModelMat mm;mm.tex=wt;unsigned c=cd.col[i];mm.col[0]=((c>>16)&255)/255.f;mm.col[1]=((c>>8)&255)/255.f;mm.col[2]=(c&255)/255.f;mm.col[3]=1;int mi=(int)out.mats.size();out.mats.push_back(mm);
    RigPrim p;p.v=std::move(m.v);p.idx=std::move(m.idx);p.mat=mi;ps.push_back(std::move(p));}
  return rigStatic(r,ps,out,cd.height);}
}
