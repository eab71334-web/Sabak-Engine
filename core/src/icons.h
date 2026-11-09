// Sabak Core - vector icons drawn with UI primitives (design units, centred at cx,cy, size s)
#pragma once
#include <cmath>
#include "ui.h"
namespace sk { namespace ic {
inline void arrow(UI& u,float x0,float y0,float x1,float y1,float w,float hs,Col c){u.line(x0,y0,x1,y1,w,c);float dx=x1-x0,dy=y1-y0,l=std::sqrt(dx*dx+dy*dy);if(l<0.01f)return;dx/=l;dy/=l;float px=-dy,py=dx;
  u.tri(x1+dx*hs*0.6f,y1+dy*hs*0.6f,x1-dx*hs*0.5f+px*hs*0.6f,y1-dy*hs*0.5f+py*hs*0.6f,x1-dx*hs*0.5f-px*hs*0.6f,y1-dy*hs*0.5f-py*hs*0.6f,c);}
inline void move(UI& u,float cx,float cy,float s,Col c){float r=s*0.46f,w=1.5f;arrow(u,cx,cy,cx,cy-r,w,s*0.3f,c);arrow(u,cx,cy,cx,cy+r,w,s*0.3f,c);arrow(u,cx,cy,cx-r,cy,w,s*0.3f,c);arrow(u,cx,cy,cx+r,cy,w,s*0.3f,c);}
inline void rotate(UI& u,float cx,float cy,float s,Col c){float r=s*0.38f;int n=14;for(int i=0;i<n;i++){float a0=-0.5f+i*(4.6f/n),a1=-0.5f+(i+1)*(4.6f/n);u.line(cx+std::cos(a0)*r,cy+std::sin(a0)*r,cx+std::cos(a1)*r,cy+std::sin(a1)*r,1.6f,c);}
  float a=-0.5f;float ex=cx+std::cos(a)*r,ey=cy+std::sin(a)*r;u.tri(ex+s*0.22f,ey-s*0.02f,ex-s*0.12f,ey-s*0.2f,ex-s*0.12f,ey+s*0.2f,c);}
inline void scale(UI& u,float cx,float cy,float s,Col c){float h=s*0.36f;u.ring(cx-h,cy-h+s*0.1f,h*1.4f,h*1.4f,1.5f,1.5f,c);arrow(u,cx-h*0.1f,cy+h*0.2f,cx+h*1.0f,cy-h*0.9f,1.5f,s*0.3f,c);}
inline void plus(UI& u,float cx,float cy,float s,Col c){u.line(cx-s*0.4f,cy,cx+s*0.4f,cy,1.8f,c);u.line(cx,cy-s*0.4f,cx,cy+s*0.4f,1.8f,c);}
inline void copy(UI& u,float cx,float cy,float s,Col c){float h=s*0.3f;u.ring(cx-h-s*0.1f,cy-h-s*0.1f,h*1.6f,h*1.8f,2,1.4f,c);u.ring(cx-h+s*0.12f,cy-h+s*0.1f,h*1.6f,h*1.8f,2,1.4f,c);}
inline void trash(UI& u,float cx,float cy,float s,Col c){float w=s*0.34f;u.line(cx-w-2,cy-s*0.3f,cx+w+2,cy-s*0.3f,1.5f,c);u.line(cx-s*0.12f,cy-s*0.38f,cx+s*0.12f,cy-s*0.38f,1.5f,c);u.ring(cx-w,cy-s*0.24f,w*2,s*0.62f,2,1.4f,c);u.line(cx-s*0.1f,cy-s*0.1f,cx-s*0.1f,cy+s*0.24f,1.2f,c);u.line(cx+s*0.1f,cy-s*0.1f,cx+s*0.1f,cy+s*0.24f,1.2f,c);}
inline void save(UI& u,float cx,float cy,float s,Col c){float h=s*0.4f;u.ring(cx-h,cy-h,h*2,h*2,2.5f,1.5f,c);u.ring(cx-h*0.5f,cy-h,h*1.0f,h*0.7f,1,1.2f,c);u.rrect(cx-h*0.55f,cy+h*0.1f,h*1.1f,h*0.9f,1.5f,c,c);}
inline void play(UI& u,float cx,float cy,float s,Col c){u.tri(cx-s*0.22f,cy-s*0.32f,cx-s*0.22f,cy+s*0.32f,cx+s*0.34f,cy,c);}
inline void stop(UI& u,float cx,float cy,float s,Col c){u.rrect(cx-s*0.26f,cy-s*0.26f,s*0.52f,s*0.52f,2,c,c);}
inline void back(UI& u,float cx,float cy,float s,Col c){u.line(cx+s*0.14f,cy-s*0.3f,cx-s*0.16f,cy,2.0f,c);u.line(cx-s*0.16f,cy,cx+s*0.14f,cy+s*0.3f,2.0f,c);}
inline void chevronR(UI& u,float cx,float cy,float s,Col c){u.line(cx-s*0.12f,cy-s*0.28f,cx+s*0.16f,cy,1.8f,c);u.line(cx+s*0.16f,cy,cx-s*0.12f,cy+s*0.28f,1.8f,c);}
inline void chevronD(UI& u,float cx,float cy,float s,Col c){u.line(cx-s*0.28f,cy-s*0.12f,cx,cy+s*0.16f,1.8f,c);u.line(cx,cy+s*0.16f,cx+s*0.28f,cy-s*0.12f,1.8f,c);}
inline void chevronU(UI& u,float cx,float cy,float s,Col c){u.line(cx-s*0.28f,cy+s*0.12f,cx,cy-s*0.16f,1.8f,c);u.line(cx,cy-s*0.16f,cx+s*0.28f,cy+s*0.12f,1.8f,c);}
inline void download(UI& u,float cx,float cy,float s,Col c){arrow(u,cx,cy-s*0.42f,cx,cy+s*0.14f,1.8f,s*0.34f,c);u.line(cx-s*0.4f,cy+s*0.42f,cx+s*0.4f,cy+s*0.42f,1.8f,c);u.line(cx-s*0.4f,cy+s*0.2f,cx-s*0.4f,cy+s*0.42f,1.8f,c);u.line(cx+s*0.4f,cy+s*0.2f,cx+s*0.4f,cy+s*0.42f,1.8f,c);}
inline void code(UI& u,float cx,float cy,float s,Col c){u.line(cx-s*0.12f,cy-s*0.26f,cx-s*0.4f,cy,1.8f,c);u.line(cx-s*0.4f,cy,cx-s*0.12f,cy+s*0.26f,1.8f,c);u.line(cx+s*0.12f,cy-s*0.26f,cx+s*0.4f,cy,1.8f,c);u.line(cx+s*0.4f,cy,cx+s*0.12f,cy+s*0.26f,1.8f,c);u.line(cx+s*0.06f,cy-s*0.34f,cx-s*0.06f,cy+s*0.34f,1.4f,c);}
inline void cube(UI& u,float cx,float cy,float s,Col c){float h=s*0.4f;u.line(cx,cy-h,cx+h,cy-h*0.5f,1.5f,c);u.line(cx+h,cy-h*0.5f,cx+h,cy+h*0.5f,1.5f,c);u.line(cx+h,cy+h*0.5f,cx,cy+h,1.5f,c);u.line(cx,cy+h,cx-h,cy+h*0.5f,1.5f,c);u.line(cx-h,cy+h*0.5f,cx-h,cy-h*0.5f,1.5f,c);u.line(cx-h,cy-h*0.5f,cx,cy-h,1.5f,c);u.line(cx,cy,cx,cy+h,1.5f,c);u.line(cx,cy,cx+h,cy-h*0.5f,1.5f,c);u.line(cx,cy,cx-h,cy-h*0.5f,1.5f,c);}
inline void sphere(UI& u,float cx,float cy,float s,Col c){float r=s*0.38f;u.ring(cx-r,cy-r,r*2,r*2,r,1.5f,c);u.line(cx-r,cy,cx+r,cy,1.0f,c.with(c.a*0.6f));}
inline void cylinder(UI& u,float cx,float cy,float s,Col c){float w=s*0.3f,h=s*0.4f;u.line(cx-w,cy-h+3,cx-w,cy+h-3,1.5f,c);u.line(cx+w,cy-h+3,cx+w,cy+h-3,1.5f,c);u.ring(cx-w,cy-h,w*2,6,3,1.4f,c);u.ring(cx-w,cy+h-6,w*2,6,3,1.4f,c);}
inline void cone(UI& u,float cx,float cy,float s,Col c){float w=s*0.34f,h=s*0.4f;u.line(cx,cy-h,cx-w,cy+h-3,1.5f,c);u.line(cx,cy-h,cx+w,cy+h-3,1.5f,c);u.ring(cx-w,cy+h-6,w*2,6,3,1.4f,c);}
inline void plane(UI& u,float cx,float cy,float s,Col c){float w=s*0.42f;u.line(cx-w*0.6f,cy-s*0.16f,cx+w,cy-s*0.16f,1.5f,c);u.line(cx+w,cy-s*0.16f,cx+w*0.6f,cy+s*0.2f,1.5f,c);u.line(cx+w*0.6f,cy+s*0.2f,cx-w,cy+s*0.2f,1.5f,c);u.line(cx-w,cy+s*0.2f,cx-w*0.6f,cy-s*0.16f,1.5f,c);}
inline void empty(UI& u,float cx,float cy,float s,Col c){float r=s*0.14f;u.ring(cx-r,cy-r,r*2,r*2,r,1.5f,c);u.line(cx-s*0.4f,cy,cx-r,cy,1.3f,c);u.line(cx+r,cy,cx+s*0.4f,cy,1.3f,c);u.line(cx,cy-s*0.4f,cx,cy-r,1.3f,c);u.line(cx,cy+r,cx,cy+s*0.4f,1.3f,c);}
}}
