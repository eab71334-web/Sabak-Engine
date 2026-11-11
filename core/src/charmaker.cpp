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
  // sculpted head: jaw narrows, chin pushes forward (all in absolute units)
  void head(Vec3 c,Vec3 r,int seg,int rings){uint32_t base=(uint32_t)v.size();auto sm=[](float t){t=t<0?0:t>1?1:t;return t*t*(3-2*t);};
    for(int j=0;j<=rings;j++){float ph=PI*j/rings,vv=std::cos(ph);for(int i=0;i<=seg;i++){float th=2*PI*i/seg;float w=1.f-0.30f*sm((-vv-0.15f)/0.85f),zo=r.z*0.17f*sm((-vv-0.30f)/0.70f)*std::max(0.f,std::sin(th));
      add({c.x+r.x*w*std::sin(ph)*std::cos(th),c.y+r.y*vv,c.z+r.z*std::sin(ph)*std::sin(th)+zo},(float)i/seg,(float)j/rings);}}
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
  f<<"SABAKCHAR 1\n"<<height<<' '<<build<<' '<<muscle<<' '<<head<<' '<<shoulder<<' '<<hair<<' '<<outfit<<'\n';for(int i=0;i<5;i++)f<<col[i]<<(i<4?' ':'\n');for(int i=0;i<6;i++)f<<ex[i]<<(i<5?' ':'\n');return true;}
bool CharDef::load(const std::string& path){std::ifstream f(path);if(!f)return false;std::string h;std::getline(f,h);if(h.rfind("SABAKCHAR",0)!=0)return false;
  f>>height>>build>>muscle>>head>>shoulder>>hair>>outfit;for(int i=0;i<5;i++)f>>col[i];if(!f)return false;for(int i=0;i<6;i++){float v=0;if(f>>v)ex[i]=v;}return true;}
bool buildCharacter(Renderer& r,const CharDef& cd,Model& out){
  const float H=1.8f;float b=0.8f+0.5f*cd.build,mu=0.85f+0.5f*cd.muscle,hs=0.85f+0.4f*cd.head,sh=0.85f+0.35f*cd.shoulder;
  MB skin,hair,shirt,pants,shoes;bool sleeveless=cd.outfit==1,naked=cd.outfit==2;MB& top=naked?skin:shirt;MB& bot=naked?skin:pants;MB& foot=naked?skin:shoes;
  auto P=[&](float x,float y,float z){return Vec3{x*H,y*H,z*H};};
  // torso: hips to neck base
  top.loft({{.455f*H,.092f*H*b,.070f*H*b,0},{.50f*H,.104f*H*b,.078f*H*b,0},{.58f*H,.090f*H*b,.068f*H*b,0},{.68f*H,.100f*H*b,.072f*H*b,.004f*H},{.76f*H,.122f*H*sh*b,.068f*H*b,.002f*H},{.805f*H,.100f*H*sh,.055f*H,0},{.822f*H,.040f*H,.040f*H,0}});
  skin.tube(P(0,.815f,0),P(0,.865f,0.004f),.034f*H,.030f*H);
  skin.head(P(0,.925f,.006f),{.062f*H*hs,.076f*H*hs,.068f*H*hs},28,20);
  // arms (A-pose, matching the auto-rig joints)
  for(int s=-1;s<=1;s+=2){float x=(float)s;
    MB& ua=sleeveless?skin:shirt;ua.ellipsoid(P(x*.125f,.785f,0),{.046f*H*mu,.042f*H*mu,.043f*H*mu},1.f,12,8);ua.tube(P(x*.12f,.79f,0),P(x*.20f,.61f,0),.040f*H*mu,.032f*H*mu);
    skin.tube(P(x*.20f,.61f,0),P(x*.27f,.46f,.005f),.031f*H*mu,.022f*H*mu);
    skin.ellipsoid(P(x*.282f,.428f,.010f),{.019f*H,.026f*H,.010f*H},1.f,10,8);   // palm
    {const float fl[4]={.034f,.038f,.036f,.029f};for(int k=0;k<4;k++){float px=x*.282f+(k-1.5f)*.0072f;skin.tube(P(px,.410f,.010f),P(px,.410f-fl[k],.016f),.0050f*H,.0042f*H,8,3);}
      skin.tube(P(x*.270f,.425f,.016f),P(x*.262f,.392f,.026f),.0058f*H,.0046f*H,8,3);}
    bot.tube(P(x*.06f,.50f,0),P(x*.065f,.27f,.004f),.066f*H*mu*b,.046f*H*mu);
    MB& shin=(sleeveless&&!naked)?skin:bot;shin.tube(P(x*.065f,.27f,.004f),P(x*.065f,.05f,0),.044f*H*mu,.032f*H);
    foot.ellipsoid(P(x*.065f,.03f,.045f),{.036f*H,.030f*H,.078f*H},1.f,12,8);}
  // hair
  if(cd.hair==1||cd.hair==2||cd.hair==4){hair.ellipsoid(P(0,.932f,-.004f),{.067f*H*hs,.078f*H*hs,.072f*H*hs},0.62f,22,12);}
  if(cd.hair==2){hair.tube(P(0,.90f,-.062f*hs),P(0,.77f,-.07f),.034f*H,.026f*H);}
  if(cd.hair==4){hair.ellipsoid(P(0,.995f,-.02f),{.032f*H,.032f*H,.032f*H},1.f,12,8);}
  if(cd.hair==3){hair.ellipsoid(P(0,.934f,-.006f),{.068f*H*hs,.080f*H*hs,.073f*H*hs},0.48f,22,12);hair.tube(P(-.058f*hs,.92f,0),P(-.052f*hs,.82f,-.02f),.012f*H,.010f*H);hair.tube(P(.058f*hs,.92f,0),P(.052f*hs,.82f,-.02f),.012f*H,.010f*H);}
  // ---- face: eyes, brows, lips (separate parts so expression morphs can move them)
  MB eyeW,iris,brow,lipU,lipL,mouthIn;
  auto sm3=[](float t){t=t<0?0:t>1?1:t;return t*t*(3-2*t);};
  auto surf=[&](float x,float y){float vv=(y-.925f)/(.076f*hs);if(vv>1)vv=1;if(vv<-1)vv=-1;float w=1.f-0.30f*sm3((-vv-0.15f)/0.85f);float a=x/(.062f*hs*w),q=1.f-a*a-vv*vv;if(q<.05f)q=.05f;return .006f+.068f*hs*(std::sqrt(q)+0.17f*sm3((-vv-0.30f)/0.70f));};
  skin.tube(P(0,.940f,surf(0,.940f)),P(0,.908f,surf(0,.908f)+.008f),.0050f*H,.0088f*H,10,4);
  for(int s2=-1;s2<=1;s2+=2){skin.ellipsoid(P(s2*.011f,.898f,surf(0,.898f)+.004f),{.0075f*H,.0060f*H,.0070f*H},1.f,8,6);skin.ellipsoid(P(s2*.063f*hs,.922f,.004f),{.0075f*H,.0175f*H,.0120f*H},1.f,10,8);}

  for(int s2=-1;s2<=1;s2+=2){float x=s2*.022f*hs;
    eyeW.ellipsoid(P(x,.934f,surf(x,.934f)-.002f),{.0125f*H,.0105f*H,.010f*H},1.f,14,10);
    iris.ellipsoid(P(x,.934f,surf(x,.934f)+.0055f),{.0065f*H,.0065f*H,.004f*H},1.f,12,8);
    float x1=s2*.010f*hs,x2=s2*.038f*hs;brow.tube(P(x1,.962f,surf(x1,.962f)+.001f),P(x2,.965f,surf(x2,.965f)+.001f),.0035f*H,.0030f*H,8,3);}
  for(int i=0;i<4;i++){float x1=(-.022f+.011f*i)*hs,x2=x1+.011f*hs;
    lipU.tube(P(x1,.8875f,surf(x1,.8875f)+.0015f),P(x2,.8875f,surf(x2,.8875f)+.0015f),.0042f*H,.0042f*H,8,2);
    lipL.tube(P(x1,.8795f,surf(x1,.8795f)+.0015f),P(x2,.8795f,surf(x2,.8795f)+.0015f),.0046f*H,.0046f*H,8,2);}
  mouthIn.ellipsoid(P(0,.8835f,surf(0,.8835f)-.0010f),{.017f*H,.0055f*H,.003f*H},1.f,12,8);
  std::vector<MB*> parts={&skin,&hair,&shirt,&pants,&shoes,&eyeW,&iris,&brow,&lipU,&lipL,&mouthIn};std::vector<RigPrim> ps;std::vector<int> roles;out=Model();out.name=cd.name;
  std::vector<unsigned char> white{255,255,255,255};unsigned wt=r.createTexture(white.data(),1,1);
  auto mix=[](unsigned a,unsigned b,float t){auto ch=[&](int sh){return (unsigned)((1-t)*((a>>sh)&255)+t*((b>>sh)&255));};return (ch(16)<<16)|(ch(8)<<8)|ch(0);};
  unsigned lip=mix(cd.col[0],0xa03838,0.35f);unsigned cols[11]={cd.col[0],cd.col[1],cd.col[2],cd.col[3],cd.col[4],0xf2f2ee,0x3a2616,cd.col[1],lip,lip,0x2a0f10};
  for(int i=0;i<11;i++){MB& m=*parts[i];if(m.idx.empty())continue;m.finish();ModelMat mm;mm.tex=wt;unsigned c=cols[i];mm.col[0]=((c>>16)&255)/255.f;mm.col[1]=((c>>8)&255)/255.f;mm.col[2]=(c&255)/255.f;mm.col[3]=1;int mi=(int)out.mats.size();out.mats.push_back(mm);
    RigPrim p;p.v=std::move(m.v);p.idx=std::move(m.idx);p.mat=mi;ps.push_back(std::move(p));roles.push_back(i);}
  if(!rigStatic(r,ps,out,cd.height))return false;
  // ---- expression morph targets (deltas in raw model space)
  out.morphNames={"Smile","MouthOpen","Blink","BrowUp","Angry","Sad"};out.morphW.assign(6,0.f);
  auto cl=[](float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;};
  for(size_t pi=0;pi<out.prims.size()&&pi<roles.size();pi++){int ro=roles[pi];if(ro<5)continue;ModelPrim& mp=out.prims[pi];size_t vc=mp.cv.size();mp.md.assign(6,std::vector<float>(vc*3,0.f));
    for(size_t i=0;i<vc;i++){float qx=mp.cv[i].p[0]/H,qy=mp.cv[i].p[1]/H;float ax=std::fabs(qx),sg=qx<0?-1.f:1.f;
      auto D=[&](int k,float dx,float dy,float dz){mp.md[k][i*3]+=dx*H;mp.md[k][i*3+1]+=dy*H;mp.md[k][i*3+2]+=dz*H;};
      if(ro==5||ro==6){D(2,0,-(qy-.934f)*0.92f,0);D(4,0,-(qy-.934f)*0.18f,0);D(5,0,0,0);}
      else if(ro==7){float u=cl((ax-.010f)/.028f,0,1);D(3,0,.012f,0);D(4,-sg*.003f*(1-u),-.010f+.006f*u,0);D(5,0,.010f-.008f*u,0);}
      else{float c=cl(ax/.022f,0,1);bool lower=ro==9,inner=ro==10;
        D(0,sg*.004f*c,.011f*c*c-.002f,0);
        D(1,0,inner?(qy-.8835f)*3.f-.005f:lower?-.014f:.002f,0);
        D(4,0,-.004f*c,0);D(5,0,-.008f*c*c,0);}}}
  for(int k=0;k<6;k++)out.morphW[k]=cd.ex[k];out.applyMorphs(r);
  return true;
}
}
