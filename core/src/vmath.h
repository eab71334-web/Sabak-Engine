// Sabak Core - minimal math (column-major, right-handed, GL clip space)
#pragma once
#include <cmath>
#include <cstring>
namespace sk {
struct Vec3 { float x=0,y=0,z=0;
  Vec3 operator+(Vec3 o) const {return {x+o.x,y+o.y,z+o.z};} Vec3 operator-(Vec3 o) const {return {x-o.x,y-o.y,z-o.z};}
  Vec3 operator*(float s) const {return {x*s,y*s,z*s};} };
inline float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float len(Vec3 a){return std::sqrt(dot(a,a));}
inline Vec3 norm(Vec3 a){float l=len(a);return l>0?a*(1/l):a;}
struct Mat4 { float m[16];
  static Mat4 identity(){Mat4 r{};r.m[0]=r.m[5]=r.m[10]=r.m[15]=1;return r;}
  Mat4 operator*(const Mat4& b) const {Mat4 r{};for(int c=0;c<4;c++)for(int rr=0;rr<4;rr++){float s=0;for(int k=0;k<4;k++)s+=m[k*4+rr]*b.m[c*4+k];r.m[c*4+rr]=s;}return r;}
  static Mat4 translate(Vec3 t){Mat4 r=identity();r.m[12]=t.x;r.m[13]=t.y;r.m[14]=t.z;return r;}
  static Mat4 scale(Vec3 s){Mat4 r=identity();r.m[0]=s.x;r.m[5]=s.y;r.m[10]=s.z;return r;}
  static Mat4 rotX(float a){Mat4 r=identity();float c=std::cos(a),s=std::sin(a);r.m[5]=c;r.m[6]=s;r.m[9]=-s;r.m[10]=c;return r;}
  static Mat4 rotZ(float a){Mat4 r=identity();float c=std::cos(a),s=std::sin(a);r.m[0]=c;r.m[1]=s;r.m[4]=-s;r.m[5]=c;return r;}
  static Mat4 rotY(float a){Mat4 r=identity();float c=std::cos(a),s=std::sin(a);r.m[0]=c;r.m[2]=-s;r.m[8]=s;r.m[10]=c;return r;}
  static Mat4 perspective(float fovy,float asp,float n,float f){Mat4 r{};float t=1/std::tan(fovy/2);r.m[0]=t/asp;r.m[5]=t;r.m[10]=(f+n)/(n-f);r.m[11]=-1;r.m[14]=2*f*n/(n-f);return r;}
  static Mat4 ortho(float l,float rr,float b,float t,float n,float f){Mat4 r=identity();r.m[0]=2/(rr-l);r.m[5]=2/(t-b);r.m[10]=-2/(f-n);r.m[12]=-(rr+l)/(rr-l);r.m[13]=-(t+b)/(t-b);r.m[14]=-(f+n)/(f-n);return r;}
  static Mat4 lookAt(Vec3 e,Vec3 c,Vec3 up){Vec3 f=norm(c-e),s=norm(cross(f,up)),u=cross(s,f);Mat4 r=identity();
    r.m[0]=s.x;r.m[4]=s.y;r.m[8]=s.z;r.m[1]=u.x;r.m[5]=u.y;r.m[9]=u.z;r.m[2]=-f.x;r.m[6]=-f.y;r.m[10]=-f.z;
    r.m[12]=-dot(s,e);r.m[13]=-dot(u,e);r.m[14]=dot(f,e);return r;}
};
inline float clampf(float v,float a,float b){return v<a?a:v>b?b:v;}
}
