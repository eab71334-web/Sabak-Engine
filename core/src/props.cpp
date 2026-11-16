// Sabak Core - procedural props: furniture, books, bag, stationery, clothes.
// Built from a small toolbox (rounded boxes with true fillets, lathes, parametric surfaces, strips)
// plus procedurally painted textures (wood grain, paper, denim, book covers).
#include "model.h"
#include "renderer.h"
#include <cmath>
#include <functional>
#include <map>
#include <algorithm>
#include <cstdlib>
namespace sk {
namespace {
const float PI=3.14159265f;
inline float hsh(int x,int y,int s){unsigned h=(unsigned)x*374761393u+(unsigned)y*668265263u+(unsigned)s*2147483647u;h=(h^(h>>13))*1274126177u;return ((h^(h>>16))&0xffff)/65535.f;}
float vnoise(float x,float y,int s){int xi=(int)std::floor(x),yi=(int)std::floor(y);float fx=x-xi,fy=y-yi;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
  float a=hsh(xi,yi,s),b=hsh(xi+1,yi,s),c=hsh(xi,yi+1,s),d=hsh(xi+1,yi+1,s);return a+(b-a)*fx+(c-a)*fy+(a-b-c+d)*fx*fy;}
float fbm(float x,float y,int s){float v=0,a=0.5f;for(int o=0;o<5;o++){v+=a*vnoise(x,y,s+o);x*=2;y*=2;a*=0.5f;}return v;}
typedef std::vector<unsigned char> Img;
const int TN=512;
Img mkTex(const std::function<void(float,float,float*)>& f){Img im((size_t)TN*TN*4);for(int y=0;y<TN;y++)for(int x=0;x<TN;x++){float c[4]={1,1,1,1};f((x+0.5f)/TN,(y+0.5f)/TN,c);for(int k=0;k<4;k++)im[((size_t)y*TN+x)*4+k]=(unsigned char)std::max(0.f,std::min(255.f,c[k]*255.f));}return im;}
// wood: long grain with growth rings, pores and a darker figure
Img woodTex(float r,float g,float b,int seed){return mkTex([=](float u,float v,float* c){float w=fbm(u*2.f,v*5.f,seed)*2.2f;float ring=std::sin((v*34.f+w)*PI)*0.5f+0.5f;ring=std::pow(ring,1.6f);float pore=vnoise(u*260.f,v*14.f,seed+9);float fig=fbm(u*1.5f,v*1.2f,seed+4);
  float t=0.86f+0.14f*ring+0.07f*(pore-0.5f)+0.10f*(fig-0.5f);c[0]=r*t;c[1]=g*t;c[2]=b*t;});}
Img paperTex(){return mkTex([](float u,float v,float* c){float n=vnoise(u*400.f,v*400.f,3)*0.04f;float line=(std::fmod(v*TN,5.f)<1.f)?0.10f:0.f;float t=0.93f-n-line;c[0]=t;c[1]=t*0.985f;c[2]=t*0.94f;});}
Img coverTex(float r,float g,float b,float ar,float ag,float ab,int seed){return mkTex([=](float u,float v,float* c){float n=vnoise(u*300.f,v*300.f,seed)*0.10f;float t=0.88f+n;c[0]=r*t;c[1]=g*t;c[2]=b*t;
  bool frame=(std::fabs(u-0.5f)<0.40f&&std::fabs(v-0.5f)<0.42f)&&!(std::fabs(u-0.5f)<0.385f&&std::fabs(v-0.5f)<0.405f);bool bar=std::fabs(u-0.5f)<0.28f&&((std::fabs(v-0.34f)<0.030f)||(std::fabs(v-0.44f)<0.012f)||(std::fabs(v-0.62f)<0.008f));
  if(frame||bar){c[0]=ar;c[1]=ag;c[2]=ab;}});}
Img denimTex(){return mkTex([](float u,float v,float* c){float tw=std::fmod((u+v)*TN*0.5f,3.f)<1.5f?1.f:0.82f;float slub=vnoise(u*60.f,v*500.f,2)*0.35f;float fade=fbm(u*4.f,v*4.f,7);float t=(0.62f+0.30f*tw*0.5f+slub*0.25f)*(0.75f+0.45f*fade);c[0]=0.16f*t*1.5f;c[1]=0.26f*t*1.5f;c[2]=0.45f*t*1.5f;});}
Img knitTex(float r,float g,float b){return mkTex([=](float u,float v,float* c){float rib=std::sin(u*TN*0.5f*PI/2.f)*0.5f+0.5f;float n=vnoise(u*200.f,v*200.f,5)*0.1f;float t=0.70f+0.30f*rib*std::fabs(std::sin(v*TN*0.25f*PI))*0.9f+n;c[0]=r*t;c[1]=g*t;c[2]=b*t;});}

struct MB{std::vector<SkinVertex> v;std::vector<uint32_t> i;};
struct MatKey{int kind;float c[4];unsigned tex;bool operator<(const MatKey& o)const{if(kind!=o.kind)return kind<o.kind;if(tex!=o.tex)return tex<o.tex;for(int k=0;k<4;k++)if(c[k]!=o.c[k])return c[k]<o.c[k];return false;}};
struct PB{
  Renderer* R; std::vector<MB> mb; std::vector<ModelMat> mats; std::map<MatKey,int> idx; Mat4 X=Mat4::identity(); std::vector<Mat4> stk; unsigned white=0;
  explicit PB(Renderer* r):R(r){unsigned char w[4]={255,255,255,255};white=r?r->createTexture(w,1,1):0;}
  unsigned tex(const Img& im){return R?R->createTexture(im.data(),TN,TN):0;}
  int mat(int kind,float r,float g,float b,unsigned t=0){MatKey k{kind,{r,g,b,1},t?t:white};auto it=idx.find(k);if(it!=idx.end())return it->second;ModelMat m;m.kind=kind;m.col[0]=r;m.col[1]=g;m.col[2]=b;m.col[3]=1;m.tex=k.tex;mats.push_back(m);mb.emplace_back();idx[k]=(int)mats.size()-1;return (int)mats.size()-1;}
  void push(const Mat4& m){stk.push_back(X);X=X*m;} void pop(){X=stk.back();stk.pop_back();}
  Vec3 tp(Vec3 p)const{return {X.m[0]*p.x+X.m[4]*p.y+X.m[8]*p.z+X.m[12],X.m[1]*p.x+X.m[5]*p.y+X.m[9]*p.z+X.m[13],X.m[2]*p.x+X.m[6]*p.y+X.m[10]*p.z+X.m[14]};}
  Vec3 tn(Vec3 n)const{return norm(Vec3{X.m[0]*n.x+X.m[4]*n.y+X.m[8]*n.z,X.m[1]*n.x+X.m[5]*n.y+X.m[9]*n.z,X.m[2]*n.x+X.m[6]*n.y+X.m[10]*n.z});}
  uint32_t vtx(int m,Vec3 p,Vec3 n,float u,float v){SkinVertex s{};Vec3 q=tp(p),nn=tn(n);s.p[0]=q.x;s.p[1]=q.y;s.p[2]=q.z;s.n[0]=nn.x;s.n[1]=nn.y;s.n[2]=nn.z;s.uv[0]=u;s.uv[1]=v;s.j[0]=0;s.w[0]=1;s.c[0]=s.c[1]=s.c[2]=s.c[3]=255;mb[m].v.push_back(s);return (uint32_t)mb[m].v.size()-1;}
  // quad grid -> triangles, flipped when needed so that the geometric normal agrees with the shading normal
  void grid(int m,const std::vector<uint32_t>& ids,int nu,int nv,bool wrapU){auto& I=mb[m].i;int cu=wrapU?nu:nu-1;
    for(int j=0;j+1<nv;j++)for(int i=0;i<cu;i++){uint32_t a=ids[j*nu+i],b=ids[j*nu+(i+1)%nu],c=ids[(j+1)*nu+(i+1)%nu],d=ids[(j+1)*nu+i];
      const SkinVertex& A=mb[m].v[a];const SkinVertex& B=mb[m].v[b];const SkinVertex& C=mb[m].v[c];Vec3 e1{B.p[0]-A.p[0],B.p[1]-A.p[1],B.p[2]-A.p[2]},e2{C.p[0]-A.p[0],C.p[1]-A.p[1],C.p[2]-A.p[2]};Vec3 g=cross(e1,e2);Vec3 sn{A.n[0],A.n[1],A.n[2]};
      if(dot(g,sn)<0){I.insert(I.end(),{a,c,b,a,d,c});}else I.insert(I.end(),{a,b,c,a,c,d});}}
  // rounded box with true circular fillets (smooth analytic normals)
  void rbox(int m,Vec3 c,Vec3 h,float r,int k=3,float us=1.f){r=std::max(1e-4f,std::min(r,std::min(h.x,std::min(h.y,h.z))*0.98f));float hh[3]={h.x,h.y,h.z};
    auto samp=[&](float H){std::vector<float> s;for(int j=k;j>=1;j--){float th=0.7853982f*j/k;s.push_back(-((H-r)+r*std::tan(th)));}s.push_back(-(H-r));s.push_back(H-r);for(int j=1;j<=k;j++){float th=0.7853982f*j/k;s.push_back((H-r)+r*std::tan(th));}return s;};
    for(int ax=0;ax<3;ax++)for(int sg=-1;sg<=1;sg+=2){int a1=(ax+1)%3,a2=(ax+2)%3;auto s1=samp(hh[a1]),s2=samp(hh[a2]);int nu=(int)s1.size(),nv=(int)s2.size();std::vector<uint32_t> ids;
      for(int j=0;j<nv;j++)for(int i=0;i<nu;i++){float p[3];p[ax]=sg*hh[ax];p[a1]=s1[i];p[a2]=s2[j];float in[3],d[3];for(int q=0;q<3;q++){float lim=hh[q]-r;in[q]=std::max(-lim,std::min(lim,p[q]));d[q]=p[q]-in[q];}
        Vec3 n=norm(Vec3{d[0],d[1],d[2]});Vec3 pos{in[0]+n.x*r+c.x,in[1]+n.y*r+c.y,in[2]+n.z*r+c.z};ids.push_back(vtx(m,pos,n,(s1[i]+hh[a1])*us*(ax==1?1.f:1.f)+(sg>0?0.37f:0.f),(s2[j]+hh[a2])*us));}
      grid(m,ids,nu,nv,false);}}
  // lathe around Y: profile of (radius,y) pairs, smooth normals from the profile tangent
  void lathe(int m,const std::vector<std::pair<float,float>>& pr,int seg,float us=1.f,bool flat=false){int np=(int)pr.size();std::vector<uint32_t> ids;int nu=seg+1;
    for(int j=0;j<np;j++){float dr,dy;if(j==0){dr=pr[1].first-pr[0].first;dy=pr[1].second-pr[0].second;}else if(j==np-1){dr=pr[j].first-pr[j-1].first;dy=pr[j].second-pr[j-1].second;}else{dr=pr[j+1].first-pr[j-1].first;dy=pr[j+1].second-pr[j-1].second;}
      float l=std::sqrt(dr*dr+dy*dy)+1e-9f;float nr=dy/l,ny=-dr/l;
      for(int i=0;i<nu;i++){float a=2*PI*i/seg;float ca=std::cos(a),sa=std::sin(a);Vec3 n{nr*ca,ny,nr*sa};if(flat){float am=2*PI*(i+0.5f)/seg;n={nr*std::cos(am),ny,nr*std::sin(am)};}
        ids.push_back(vtx(m,{pr[j].first*ca,pr[j].second,pr[j].first*sa},n,(float)i/seg*us,pr[j].second*us*4));}}
    grid(m,ids,nu,np,false);}
  // parametric surface p(u,v), u,v in [0,1]; normals by finite differences
  void surf(int m,int nu,int nv,bool wrapU,const std::function<Vec3(float,float)>& f,float us=1.f){std::vector<uint32_t> ids;int uu=wrapU?nu+1:nu;
    for(int j=0;j<nv;j++)for(int i=0;i<uu;i++){float u=(float)i/(wrapU?nu:nu-1),v=(float)j/(nv-1);float e=1e-3f;Vec3 p=f(u,v),pu=f(u+e,v)-f(u-e,v),pv=f(u,std::min(1.f,v+e))-f(u,std::max(0.f,v-e));Vec3 n=norm(cross(pu,pv));ids.push_back(vtx(m,p,n,u*us,v*us));}
    grid(m,ids,uu,nv,false);}
  // beam between two points (straps, handles, hinges)
  void beam(int m,Vec3 a,Vec3 b,float w,float t,Vec3 up,float r){Vec3 z=norm(b-a);Vec3 x=norm(cross(up,z));Vec3 y=cross(z,x);float len=std::sqrt(dot(b-a,b-a));Mat4 M=Mat4::identity();
    M.m[0]=x.x;M.m[1]=x.y;M.m[2]=x.z;M.m[4]=y.x;M.m[5]=y.y;M.m[6]=y.z;M.m[8]=z.x;M.m[9]=z.y;M.m[10]=z.z;Vec3 c=(a+b)*0.5f;M.m[12]=c.x;M.m[13]=c.y;M.m[14]=c.z;push(M);rbox(m,{0,0,0},{w*0.5f,t*0.5f,len*0.5f+t*0.3f},r,2);pop();}
  void cyl(int m,float r,float len,int seg,bool flat=false){lathe(m,{{0,0},{r,0},{r,len},{0,len}},seg,1.f,flat);}
  void finish(Model& out,float height,float radius){out=Model();out.name="Prop";out.nodeName={"root"};out.parent={-1};out.rest.assign(1,TRS());out.joints={0};out.ibm={Mat4::identity()};
    for(auto& m:mats)out.mats.push_back(m);for(size_t k=0;k<mb.size();k++){if(mb[k].i.empty())continue;ModelPrim mp;mp.mat=(int)k;mp.cv=mb[k].v;mp.ci=mb[k].i;if(R)mp.mesh=R->uploadSkin(mb[k].v,mb[k].i);out.prims.push_back(std::move(mp));}
    out.fit=1;out.foot=0;out.height=height;out.radius=radius;out.autoRig=false;}
};
Mat4 T(float x,float y,float z){return Mat4::translate({x,y,z});}

// ------------------------------------------------------------------ desk
void desk(PB& P){unsigned oak=P.tex(woodTex(0.78f,0.58f,0.36f,11)),wal=P.tex(woodTex(0.46f,0.30f,0.19f,23));
  int top=P.mat(0,1,1,1,oak),leg=P.mat(0,1,1,1,wal),chrome=P.mat(7,0.8f,0.8f,0.82f),dark=P.mat(9,0.05f,0.05f,0.05f);
  P.rbox(top,{0,0.735f,0},{0.70f,0.0225f,0.35f},0.010f,3,1.1f);
  for(int sx=-1;sx<=1;sx+=2)for(int sz=-1;sz<=1;sz+=2){P.rbox(leg,{sx*0.640f,0.355f,sz*0.295f},{0.028f,0.355f,0.028f},0.008f,2,3.f);P.rbox(leg,{sx*0.640f,0.040f,sz*0.295f},{0.031f,0.040f,0.031f},0.008f,2,3.f);}
  P.rbox(leg,{0,0.672f,-0.300f},{0.60f,0.040f,0.011f},0.004f,2,2.f);P.rbox(leg,{0,0.672f,0.300f},{0.60f,0.040f,0.011f},0.004f,2,2.f);
  P.rbox(leg,{-0.640f,0.672f,0},{0.011f,0.040f,0.26f},0.004f,2,2.f);P.rbox(leg,{0.640f,0.672f,0},{0.011f,0.040f,0.26f},0.004f,2,2.f);
  P.rbox(leg,{-0.24f,0.17f,0},{0.38f,0.012f,0.27f},0.004f,2,2.f);                                   // lower shelf
  P.rbox(leg,{0.0f,0.42f,-0.30f},{0.60f,0.22f,0.008f},0.003f,2,2.f);                              // back panel
  // drawer pedestal (right)
  P.rbox(leg,{0.40f,0.38f,0.0f},{0.225f,0.33f,0.285f},0.006f,2,2.f);
  for(int k=0;k<3;k++){float y=0.62f-k*0.175f;P.rbox(top,{0.40f,y,0.292f},{0.205f,0.077f,0.010f},0.005f,2,1.4f);
    P.rbox(dark,{0.40f,y+0.074f,0.2845f},{0.208f,0.003f,0.004f},0.001f,1);
    P.push(T(0.40f,y+0.005f,0.312f)*Mat4::rotZ(PI/2));P.cyl(chrome,0.0065f,0.075f,18);P.pop();P.push(T(0.365f,y+0.005f,0.312f)*Mat4::rotX(PI/2));P.cyl(chrome,0.005f,0.02f,14);P.pop();P.push(T(0.435f,y+0.005f,0.312f)*Mat4::rotX(PI/2));P.cyl(chrome,0.005f,0.02f,14);P.pop();}
  P.rbox(dark,{0.40f,0.056f,0.0f},{0.215f,0.012f,0.275f},0.004f,1);                                // plinth shadow gap
}
// ------------------------------------------------------------------ book (lying, spine on -x)
void book(PB& P,float w,float t,float d,float cr,float cg,float cb,float ar,float ag,float ab,int seed){
  static unsigned paper=0;(void)paper;int pg=P.mat(0,1,1,1,P.tex(paperTex()));int cv=P.mat(0,1,1,1,P.tex(coverTex(cr,cg,cb,ar,ag,ab,seed))),sp=P.mat(11,cr*0.9f,cg*0.9f,cb*0.9f),gold=P.mat(7,0.9f,0.72f,0.25f);
  P.rbox(pg,{0.003f,0,0},{w*0.5f-0.004f,t*0.5f-0.003f,d*0.5f-0.005f},0.0012f,2,3.f);
  P.rbox(cv,{0,t*0.5f-0.0017f,0},{w*0.5f,0.0017f,d*0.5f},0.0012f,2,1.f);P.rbox(cv,{0,-t*0.5f+0.0017f,0},{w*0.5f,0.0017f,d*0.5f},0.0012f,2,1.f);
  P.rbox(sp,{-w*0.5f+0.0012f,0,0},{0.0035f,t*0.5f,d*0.5f},0.0030f,3,1.f);
  P.rbox(gold,{-w*0.5f-0.0006f,0,0},{0.0008f,t*0.30f,d*0.12f},0.0006f,1);P.rbox(gold,{-w*0.5f-0.0006f,0,d*0.30f},{0.0008f,t*0.10f,d*0.02f},0.0006f,1);
  P.rbox(P.mat(9,0.55f,0.1f,0.1f),{0.01f,-t*0.5f-0.0006f,d*0.5f-0.0015f},{0.004f,0.0007f,0.014f},0.0005f,1);}   // bookmark ribbon
void books(PB& P){struct B{float w,t,d,r,g,b,ar,ag,ab;float yaw,dx;};B bs[]={{0.215f,0.045f,0.285f,0.45f,0.08f,0.10f,0.88f,0.72f,0.32f,0.0f,0.0f},{0.19f,0.032f,0.262f,0.10f,0.22f,0.38f,0.85f,0.85f,0.80f,0.07f,0.012f},{0.205f,0.038f,0.275f,0.14f,0.32f,0.20f,0.90f,0.78f,0.35f,-0.05f,-0.010f},{0.175f,0.026f,0.245f,0.62f,0.50f,0.22f,0.25f,0.18f,0.12f,0.09f,0.015f},{0.20f,0.034f,0.27f,0.22f,0.12f,0.30f,0.85f,0.80f,0.55f,-0.03f,-0.006f},{0.185f,0.022f,0.25f,0.55f,0.55f,0.52f,0.20f,0.20f,0.20f,0.06f,0.010f}};
  float y=0;int s=3;for(auto& b:bs){P.push(T(b.dx,y+b.t*0.5f,0)*Mat4::rotY(b.yaw));book(P,b.w,b.t,b.d,b.r,b.g,b.b,b.ar,b.ag,b.ab,s++);P.pop();y+=b.t;}}
// ------------------------------------------------------------------ backpack
void bag(PB& P){int fab=P.mat(2,0.10f,0.27f,0.33f),fab2=P.mat(2,0.08f,0.21f,0.27f),strap=P.mat(9,0.05f,0.06f,0.07f),chrome=P.mat(7,0.78f,0.78f,0.80f),lea=P.mat(11,0.42f,0.26f,0.14f),blk=P.mat(9,0.03f,0.03f,0.035f);
  P.rbox(fab,{0,0.26f,0},{0.150f,0.230f,0.082f},0.060f,5,5.f);                       // main body
  P.rbox(fab2,{0,0.40f,0.040f},{0.145f,0.085f,0.058f},0.045f,4,5.f);                  // top flap
  P.rbox(fab,{0,0.17f,0.102f},{0.122f,0.115f,0.030f},0.026f,4,5.f);                   // front pocket
  for(int k=0;k<20;k++)P.rbox(chrome,{-0.100f+k*0.0105f,0.285f,0.124f},{0.0030f,0.0030f,0.0022f},0.001f,1);   // zip teeth
  P.rbox(blk,{0,0.285f,0.1225f},{0.108f,0.0034f,0.0026f},0.001f,1);
  P.rbox(chrome,{0.108f,0.280f,0.128f},{0.011f,0.007f,0.003f},0.002f,2);P.rbox(strap,{0.108f,0.255f,0.126f},{0.003f,0.020f,0.002f},0.001f,1);   // zip pull
  P.rbox(lea,{0,0.115f,0.1335f},{0.040f,0.022f,0.0038f},0.003f,2);                      // logo patch
  for(int s=-1;s<=1;s+=2){P.rbox(fab2,{s*0.158f,0.135f,0.020f},{0.026f,0.105f,0.056f},0.022f,4,5.f);                    // side pockets
    P.beam(strap,{s*0.085f,0.470f,-0.075f},{s*0.115f,0.390f,-0.185f},0.050f,0.016f,{0,0,1},0.007f);P.beam(strap,{s*0.115f,0.390f,-0.185f},{s*0.115f,0.230f,-0.190f},0.050f,0.016f,{0,0,1},0.007f);P.beam(strap,{s*0.115f,0.230f,-0.190f},{s*0.100f,0.070f,-0.100f},0.050f,0.016f,{0,0,1},0.007f);
    P.rbox(chrome,{s*0.113f,0.300f,-0.188f},{0.026f,0.010f,0.0060f},0.004f,2);                                         // strap slider
    P.rbox(blk,{s*0.108f,0.20f,0.134f},{0.012f,0.026f,0.002f},0.001f,1);}
  P.beam(strap,{-0.050f,0.478f,0.0f},{-0.040f,0.525f,0.0f},0.034f,0.012f,{1,0,0},0.005f);P.beam(strap,{-0.040f,0.525f,0.0f},{0.040f,0.525f,0.0f},0.034f,0.012f,{0,0,1},0.005f);P.beam(strap,{0.040f,0.525f,0.0f},{0.050f,0.478f,0.0f},0.034f,0.012f,{1,0,0},0.005f);   // carry handle
  P.rbox(blk,{0,0.035f,0},{0.150f,0.034f,0.082f},0.030f,4);                              // moulded base
}
// ------------------------------------------------------------------ stationery
void pencil(PB& P,float len,float r,int ci){static const float C[][3]={{0.95f,0.78f,0.10f},{0.80f,0.12f,0.10f},{0.12f,0.35f,0.80f},{0.15f,0.60f,0.25f}};int body=P.mat(5,C[ci%4][0],C[ci%4][1],C[ci%4][2]),wood=P.mat(9,0.86f,0.68f,0.46f),lead=P.mat(9,0.10f,0.10f,0.11f),fer=P.mat(7,0.80f,0.80f,0.82f),era=P.mat(9,0.88f,0.45f,0.50f);
  P.cyl(body,r,len,6,true);P.push(T(0,len,0));P.lathe(fer,{{r*1.03f,0},{r*1.03f,0.007f},{r*0.98f,0.0075f}},24);P.lathe(era,{{r*0.9f,0.0075f},{r*0.9f,0.0125f},{r*0.7f,0.0135f},{0,0.0138f}},20);P.pop();
  P.push(T(0,-0.016f,0));P.lathe(wood,{{0,0},{r*0.25f,0},{r*1.0f,0.016f},{r*0.88f,0.016f}},6,1.f,true);P.lathe(lead,{{0,-0.004f},{r*0.25f,0}},12);P.pop();}
void pen(PB& P,float cr,float cg,float cb){int pl=P.mat(5,cr,cg,cb),chrome=P.mat(7,0.8f,0.8f,0.82f),blk=P.mat(5,0.02f,0.02f,0.025f);
  P.lathe(pl,{{0,0.0f},{0.0035f,0.0f},{0.0050f,0.006f},{0.0055f,0.020f},{0.0058f,0.110f},{0.0056f,0.150f},{0.0050f,0.152f},{0,0.152f}},28);
  P.lathe(chrome,{{0.0013f,-0.010f},{0.0035f,0.0f},{0.0030f,0.001f},{0.0008f,-0.012f}},16);P.lathe(blk,{{0.0034f,0.0f},{0.0050f,0.006f},{0.0051f,0.021f},{0.0036f,0.0f}},16);
  P.beam(chrome,{0.0f,0.150f,0.0057f},{0.0f,0.092f,0.0072f},0.0035f,0.0016f,{1,0,0},0.0006f);}
void cup(PB& P,bool withItems){int met=P.mat(5,0.05f,0.05f,0.06f);P.lathe(met,{{0,0},{0.036f,0},{0.040f,0.004f},{0.043f,0.090f},{0.045f,0.096f},{0.0435f,0.098f},{0.040f,0.094f},{0.0385f,0.092f},{0.0355f,0.006f},{0,0.006f}},40);
  if(!withItems)return;struct I{float x,z,ax,az;int t;float h;};I it[]={{0.012f,0.008f,0.10f,-0.06f,0,0.17f},{-0.014f,0.010f,-0.15f,0.05f,1,0.165f},{0.004f,-0.016f,0.04f,0.22f,2,0.18f},{-0.010f,-0.008f,-0.20f,-0.12f,3,0.16f},{0.018f,-0.010f,0.20f,0.12f,0,0.175f},{-0.003f,0.020f,-0.02f,-0.20f,1,0.17f}};
  for(auto& p:it){P.push(T(p.x,0.012f,p.z)*Mat4::rotZ(p.ax)*Mat4::rotX(p.az));pencil(P,p.h,0.0037f,p.t);P.pop();}
  P.push(T(-0.022f,0.012f,-0.006f)*Mat4::rotZ(0.20f)*Mat4::rotX(0.08f));pen(P,0.08f,0.12f,0.45f);P.pop();P.push(T(0.025f,0.012f,0.008f)*Mat4::rotZ(-0.18f)*Mat4::rotX(-0.10f));pen(P,0.55f,0.08f,0.10f);P.pop();}
// ------------------------------------------------------------------ clothes
void tshirt(PB& P){int fab=P.mat(2,0.42f,0.09f,0.11f),rib=P.mat(2,0.33f,0.07f,0.09f);
  auto fold=[](float x,float y,float s){return 0.010f*std::sin(x*26.f+y*8.f+s)*std::exp(-y*2.f)+0.014f*(fbm(x*5.f+s,y*5.f,s>1?4:9)-0.5f)+0.006f*std::sin(y*34.f+x*3.f);};
  auto panel=[&](float sgn){return [=](float u,float v)->Vec3{float x=(u*2-1);float y=v*0.66f;float w=0.245f+0.012f*std::sin(PI*std::min(1.f,v*1.3f));float sh=std::max(0.f,(v-0.80f)/0.20f);w*=1.f-0.50f*sh*sh;float xx=x*w;float yy=y-0.045f*sh*sh*std::fabs(x)*std::fabs(x)*0;
      float neck=std::fabs(xx)<0.105f&&v>0.9f?(0.105f-std::fabs(xx))/0.105f:0.f;float dip=neck*(sgn>0?0.085f:0.03f)*std::pow(std::min(1.f,(v-0.9f)/0.1f),1.f);yy-=dip*(1.f-(xx/0.105f)*(xx/0.105f))*0.0f+dip*0.55f;
      float bulge=0.055f+0.040f*(1-x*x)*(1-0.6f*v);float z=sgn*(bulge+fold(xx,yy,sgn*2.f));return Vec3{xx,yy,z};};};
  P.surf(fab,64,80,false,panel(1.f),9.f);P.surf(fab,64,80,false,panel(-1.f),9.f);
  for(int s=-1;s<=1;s+=2){auto sl=[=](float u,float v)->Vec3{float a=2*PI*u;Vec3 c0{s*0.20f,0.60f,0},c1{s*0.37f,0.45f,0};Vec3 c=c0+(c1-c0)*v;Vec3 ax=norm(c1-c0);Vec3 pa=norm(cross(ax,Vec3{0,0,1}));float r=0.060f-0.012f*v;float rz=0.052f-0.010f*v;return c+pa*(std::cos(a)*r)+Vec3{0,0,1}*(std::sin(a)*rz)+Vec3{0,0,1}*(0.006f*std::sin(v*14.f+a*2.f+s)*(1-v));};
    P.surf(fab,32,20,true,sl,9.f);
    auto cuff=[=](float u,float v)->Vec3{float a=2*PI*u;Vec3 c0{s*0.355f,0.465f,0},c1{s*0.37f,0.45f,0};Vec3 c=c0+(c1-c0)*v;Vec3 ax=norm(c1-c0);Vec3 pa=norm(cross(ax,Vec3{0,0,1}));return c+pa*(std::cos(a)*0.049f)+Vec3{0,0,1}*(std::sin(a)*0.043f);};P.surf(rib,24,3,true,cuff,9.f);}
  auto collar=[](float u,float v)->Vec3{float a=2*PI*u;float rx=0.105f+0.004f*v,rz=0.060f;float y=0.655f-0.0f*v;return Vec3{std::cos(a)*rx,y+0.02f*(std::sin(a)<0?-0.9f*std::fabs(std::sin(a)):0.f)*0+((std::sin(a)>0)?-0.07f*std::sin(a)*std::sin(a)*0.f:0.f),std::sin(a)*rz*(std::sin(a)>0?1.f:0.6f)+0.0f};};
  P.surf(rib,36,3,true,[&](float u,float v){Vec3 p=collar(u,v);p.y+=0.012f*v;p.y-=(std::sin(2*PI*u)>0?0.065f*std::pow(std::sin(2*PI*u),2.f):0.f);return p;},1.f);
}
void folded(PB& P){int den=P.mat(9,1,1,1,P.tex(denimTex())),chrome=P.mat(7,0.78f,0.62f,0.40f),lea=P.mat(11,0.40f,0.25f,0.14f),thread=P.mat(9,0.80f,0.62f,0.20f),wh=P.mat(2,0.88f,0.86f,0.80f),kn=P.mat(0,1,1,1,P.tex(knitTex(0.62f,0.50f,0.40f))),tee=P.mat(2,0.18f,0.38f,0.55f);
  P.rbox(den,{0,0.030f,0},{0.190f,0.030f,0.150f},0.026f,5,1.2f);                                    // jeans
  P.rbox(den,{0,0.062f,-0.120f},{0.186f,0.012f,0.020f},0.006f,2,1.f);                              // waistband
  for(int s=-1;s<=1;s+=2){P.cyl(chrome,0.0045f,0.004f,12);P.push(T(s*0.07f,0.0595f,0.115f));P.cyl(chrome,0.0045f,0.004f,12);P.pop();P.rbox(thread,{s*0.085f,0.0612f,0.015f},{0.0012f,0.0006f,0.060f},0.0004f,1);}
  P.rbox(lea,{0.115f,0.0615f,-0.121f},{0.026f,0.0022f,0.0120f},0.0014f,1);
  P.rbox(kn,{0.006f,0.098f,0.004f},{0.175f,0.036f,0.140f},0.034f,5,1.6f);                          // sweater
  P.rbox(kn,{0.006f,0.126f,0.118f},{0.082f,0.012f,0.018f},0.010f,3,1.6f);
  P.rbox(tee,{-0.004f,0.150f,0.002f},{0.150f,0.015f,0.120f},0.014f,4,1.f);P.rbox(tee,{-0.004f,0.156f,0.100f},{0.060f,0.005f,0.020f},0.005f,2,1.f);   // t-shirt on top
  (void)wh;}
}   // namespace

// study scene: desk with books and stationery, backpack and clothes on the floor
void study(PB& P){desk(P);P.push(T(-0.40f,0.7575f,0.05f)*Mat4::rotY(0.12f));books(P);P.pop();P.push(T(0.12f,0.7575f,-0.20f));cup(P,true);P.pop();
  P.push(T(-0.95f,0,-0.05f)*Mat4::rotY(0.55f));bag(P);P.pop();P.push(T(0.95f,0,0.55f)*Mat4::rotY(-0.40f));folded(P);P.pop();
  P.push(T(0.22f,0.7575f,0.20f)*Mat4::rotY(0.4f)*Mat4::rotX(PI/2));pen(P,0.10f,0.30f,0.15f);P.pop();}

bool buildProp(Renderer& r,const std::string& kind,Model& out){PB P(&r);float h=1,rad=1;
  if(kind=="desk"){desk(P);h=0.76f;rad=1.0f;}
  else if(kind=="books"){books(P);h=0.20f;rad=0.25f;}
  else if(kind=="bag"){bag(P);h=0.55f;rad=0.35f;}
  else if(kind=="pens"){cup(P,true);h=0.28f;rad=0.15f;}
  else if(kind=="tshirt"){tshirt(P);h=0.7f;rad=0.45f;}
  else if(kind=="clothes"){folded(P);h=0.18f;rad=0.3f;}
  else {study(P);h=1.1f;rad=1.6f;}
  P.finish(out,h,rad);out.name="Prop "+kind;if(const char* ps=getenv("SABAK_PSCALE")){float s=(float)atof(ps);if(s>0)out.fit=s;}return true;}
}   // namespace sk
