#include "mesh.h"
namespace sk {
static void quad(MeshData& m,Vec3 a,Vec3 b,Vec3 c,Vec3 d,Vec3 n,Vec3 col){uint32_t o=m.v.size();Vec3 ps[4]={a,b,c,d};
  for(auto&p:ps)m.v.push_back({{p.x,p.y,p.z},{n.x,n.y,n.z},{col.x,col.y,col.z}});
  uint32_t ix[6]={0,1,2,0,2,3};for(auto k:ix)m.i.push_back(o+k);}
MeshData makeBox(float w,float h,float d,Vec3 c){MeshData m;float x=w/2,y=h/2,z=d/2;
  quad(m,{-x,-y,z},{x,-y,z},{x,y,z},{-x,y,z},{0,0,1},c);quad(m,{x,-y,-z},{-x,-y,-z},{-x,y,-z},{x,y,-z},{0,0,-1},c);
  quad(m,{x,-y,z},{x,-y,-z},{x,y,-z},{x,y,z},{1,0,0},c);quad(m,{-x,-y,-z},{-x,-y,z},{-x,y,z},{-x,y,-z},{-1,0,0},c);
  quad(m,{-x,y,z},{x,y,z},{x,y,-z},{-x,y,-z},{0,1,0},c);quad(m,{-x,-y,-z},{x,-y,-z},{x,-y,z},{-x,-y,z},{0,-1,0},c);return m;}
MeshData makeSphere(float r,int seg,int rings,Vec3 c){MeshData m;
  for(int j=0;j<=rings;j++){float th=3.14159265f*j/rings;for(int i=0;i<=seg;i++){float ph=6.2831853f*i/seg;
    Vec3 n{std::sin(th)*std::cos(ph),std::cos(th),std::sin(th)*std::sin(ph)};m.v.push_back({{n.x*r,n.y*r,n.z*r},{n.x,n.y,n.z},{c.x,c.y,c.z}});}}
  for(int j=0;j<rings;j++)for(int i=0;i<seg;i++){uint32_t a=j*(seg+1)+i,b=a+seg+1;uint32_t t[6]={a,b,a+1,a+1,b,b+1};for(auto k:t)m.i.push_back(k);}return m;}
MeshData makeCylinder(float r0,float r1,float h,int seg,Vec3 c){MeshData m;float sl=(r0-r1)/h;
  for(int i=0;i<=seg;i++){float a=6.2831853f*i/seg,ca=std::cos(a),sa=std::sin(a);Vec3 n=norm({ca,sl,sa});
    m.v.push_back({{ca*r0,0,sa*r0},{n.x,n.y,n.z},{c.x,c.y,c.z}});m.v.push_back({{ca*r1,h,sa*r1},{n.x,n.y,n.z},{c.x,c.y,c.z}});}
  for(int i=0;i<seg;i++){uint32_t a=i*2;uint32_t t[6]={a,a+2,a+1,a+1,a+2,a+3};for(auto k:t)m.i.push_back(k);}
  uint32_t o=m.v.size();m.v.push_back({{0,h,0},{0,1,0},{c.x,c.y,c.z}});
  for(int i=0;i<=seg;i++){float a=6.2831853f*i/seg;m.v.push_back({{std::cos(a)*r1,h,std::sin(a)*r1},{0,1,0},{c.x,c.y,c.z}});}
  for(int i=0;i<seg;i++){m.i.push_back(o);m.i.push_back(o+2+i);m.i.push_back(o+1+i);}return m;}
MeshData makeCone(float r,float h,int seg,Vec3 c){return makeCylinder(r,0.0f,h,seg,c);}
MeshData makeTerrain(float size,int res,const std::function<float(float,float)>& H,const std::function<Vec3(float,float,float)>& C){MeshData m;float s=size/res,o=-size/2;
  for(int j=0;j<=res;j++)for(int i=0;i<=res;i++){float x=o+i*s,z=o+j*s,y=H(x,z);
    Vec3 n=norm({H(x-s,z)-H(x+s,z),2*s,H(x,z-s)-H(x,z+s)});Vec3 c=C(x,y,z);m.v.push_back({{x,y,z},{n.x,n.y,n.z},{c.x,c.y,c.z}});}
  for(int j=0;j<res;j++)for(int i=0;i<res;i++){uint32_t a=j*(res+1)+i,b=a+res+1;uint32_t t[6]={a,b,a+1,a+1,b,b+1};for(auto k:t)m.i.push_back(k);}return m;}
}
