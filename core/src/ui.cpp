#include "ui.h"
#include "gl.h"
#include <cstdio>
#include <cmath>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
extern const unsigned char Inter_SemiBold_ttf[]; extern const unsigned char Inter_Medium_ttf[];
namespace sk {
static const char* VS=R"(#version 300 es
layout(location=0) in vec2 aPos; layout(location=1) in vec2 aLocal; layout(location=2) in vec2 aHalf; layout(location=3) in vec4 aPar;
layout(location=4) in vec4 aC1; layout(location=5) in vec4 aC2; layout(location=6) in vec2 aUv;
uniform vec2 uRes; out vec2 vL; out vec2 vH; out vec4 vP; out vec4 vC1; out vec4 vC2; out vec2 vUv;
void main(){ vL=aLocal; vH=aHalf; vP=aPar; vC1=aC1; vC2=aC2; vUv=aUv; vec2 n=aPos/uRes*2.0-1.0; gl_Position=vec4(n.x,-n.y,0.0,1.0); })";
static const char* FS=R"(#version 300 es
precision highp float; in vec2 vL; in vec2 vH; in vec4 vP; in vec4 vC1; in vec4 vC2; in vec2 vUv; out vec4 o; uniform sampler2D uTex;
float sdr(vec2 p,vec2 h,float r){ vec2 q=abs(p)-(h-vec2(r)); return length(max(q,0.0))+min(max(q.x,q.y),0.0)-r; }
void main(){ float mode=vP.z; float a=1.0; vec4 col=vC1;
  if(mode<0.5){ float d=sdr(vL,vH,vP.x); a=clamp(0.5-d,0.0,1.0); col=mix(vC1,vC2,clamp(vL.y/vH.y*0.5+0.5,0.0,1.0)); }
  else if(mode<1.5){ float d=sdr(vL,vH,vP.x); a=clamp(0.5-abs(d+vP.y*0.5)+vP.y*0.5,0.0,1.0)*step(d,0.5); a=clamp(vP.y*0.5-abs(d+vP.y*0.5)+0.5,0.0,1.0); }
  else if(mode<2.5){ float d=sdr(vL,vH-vec2(vP.y),vP.x); a=1.0-smoothstep(-vP.y,vP.y,d); a*=a; }
  else if(mode<3.5){ a=texture(uTex,vUv).r; }
  else { float t=1.0-clamp(length(vL)/vH.x,0.0,1.0); a=t*t; }
  o=vec4(col.rgb,col.a*a); })";
static unsigned mk(unsigned t,const char* s){unsigned h=glCreateShader(t);glShaderSource(h,1,&s,nullptr);glCompileShader(h);int ok=0;glGetShaderiv(h,GL_COMPILE_STATUS,&ok);
  if(!ok){char b[1024];glGetShaderInfoLog(h,1024,nullptr,b);std::fprintf(stderr,"ui shader: %s\n",b);}return h;}
bool UI::init(float scale){s_=scale;prog_=glCreateProgram();glAttachShader(prog_,mk(GL_VERTEX_SHADER,VS));glAttachShader(prog_,mk(GL_FRAGMENT_SHADER,FS));glLinkProgram(prog_);
  int lk=0;glGetProgramiv(prog_,GL_LINK_STATUS,&lk);glGenVertexArrays(1,&vao_);glBindVertexArray(vao_);glGenBuffers(1,&vbo_);glBindBuffer(GL_ARRAY_BUFFER,vbo_);
  int st=20*4,off=0;int cnt[7]={2,2,2,4,4,4,2};for(int a=0;a<7;a++){glEnableVertexAttribArray(a);glVertexAttribPointer(a,cnt[a],GL_FLOAT,GL_FALSE,st,(void*)(size_t)off);off+=cnt[a]*4;}
  ok=lk&&glGetError()==GL_NO_ERROR;return ok;}
UI::Face& UI::face(float size,int font){int key=(int)(size*10)*2+font;for(size_t i=0;i<keys_.size();i++)if(keys_[i]==key)return faces_[i];
  Face f;f.px=size*s_;f.cd=new stbtt_bakedchar[96];const unsigned char* ttf=font==F_BOLD?Inter_SemiBold_ttf:Inter_Medium_ttf;
  int aw=256;std::vector<unsigned char> bm;while(true){bm.assign((size_t)aw*aw,0);int r=stbtt_BakeFontBitmap(ttf,0,f.px,bm.data(),aw,aw,32,96,(stbtt_bakedchar*)f.cd);if(r>0||aw>=4096)break;aw*=2;}
  f.aw=aw;glGenTextures(1,&f.tex);glBindTexture(GL_TEXTURE_2D,f.tex);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_R8,aw,aw,0,GL_RED,GL_UNSIGNED_BYTE,bm.data());
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  keys_.push_back(key);faces_.push_back(f);return faces_.back();}
void UI::begin(int w,int h){W_=w;H_=h;buf_.clear();curTex_=0;glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(prog_);glUniform2f(glGetUniformLocation(prog_,"uRes"),(float)w,(float)h);glUniform1i(glGetUniformLocation(prog_,"uTex"),0);glActiveTexture(GL_TEXTURE0);glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,vbo_);}
void UI::flush(){if(buf_.empty())return;glBindTexture(GL_TEXTURE_2D,curTex_);glBufferData(GL_ARRAY_BUFFER,buf_.size()*4,buf_.data(),GL_STREAM_DRAW);glDrawArrays(GL_TRIANGLES,0,buf_.size()/20);buf_.clear();}
void UI::end(){flush();glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);}
void UI::quad(float x0,float y0,float x1,float y1,float u0,float v0,float u1,float v1,float hx,float hy,float rad,float bw,float mode,Col a,Col b){
  float cx=(x0+x1)/2,cy=(y0+y1)/2;float P[4][4]={{x0,y0,u0,v0},{x1,y0,u1,v0},{x1,y1,u1,v1},{x0,y1,u0,v1}};int ix[6]={0,1,2,0,2,3};
  for(int k:ix){float* p=P[k];float v[20]={p[0],p[1],p[0]-cx,p[1]-cy,hx,hy,rad,bw,mode,0,a.r,a.g,a.b,a.a,b.r,b.g,b.b,b.a,p[2],p[3]};buf_.insert(buf_.end(),v,v+20);}}
// NOTE: vertex = 20 floats (pos2 local2 half2 par4 c1 4 c2 4 uv2)
void UI::rrect(float x,float y,float w,float h,float r,Col t,Col b){float S=s_;
  quad(x*S,y*S,(x+w)*S,(y+h)*S,0,0,0,0,w*S/2,h*S/2,r*S,0,0,t,b);}
void UI::ring(float x,float y,float w,float h,float r,float bw,Col c){float S=s_;float p=bw*S;quad(x*S-1,y*S-1,(x+w)*S+1,(y+h)*S+1,0,0,0,0,w*S/2+1,h*S/2+1,r*S,p,1,c,c);}
void UI::shadow(float x,float y,float w,float h,float r,float blur,Col c){float S=s_;float bl=blur*S;quad(x*S-bl,y*S-bl,(x+w)*S+bl,(y+h)*S+bl,0,0,0,0,w*S/2+bl,h*S/2+bl,r*S,bl,2,c,c);}
void UI::glow(float cx,float cy,float radius,Col c){float S=s_;float r=radius*S;quad(cx*S-r,cy*S-r,cx*S+r,cy*S+r,0,0,0,0,r,r,0,0,4,c,c);}
float UI::measure(const std::string& t,float size,int font){Face& f=face(size,font);float x=0,y=0;stbtt_aligned_quad q;
  for(unsigned char ch:t){if(ch<32||ch>126)ch='?';stbtt_GetBakedQuad((stbtt_bakedchar*)f.cd,f.aw,f.aw,ch-32,&x,&y,&q,1);}return x/s_;}
void UI::text(const std::string& t,float x,float y,float size,Col c,int font,int align){Face& f=face(size,font);
  if(curTex_!=f.tex){flush();curTex_=f.tex;}
  float w=measure(t,size,font);if(align==1)x-=w/2;else if(align==2)x-=w;float px=x*s_,py=y*s_+f.px*0.8f;
  for(unsigned char ch:t){if(ch<32||ch>126)ch='?';stbtt_aligned_quad q;stbtt_GetBakedQuad((stbtt_bakedchar*)f.cd,f.aw,f.aw,ch-32,&px,&py,&q,1);
    quad(q.x0,q.y0,q.x1,q.y1,q.s0,q.t0,q.s1,q.t1,1,1,0,0,3,c,c);}}
}
