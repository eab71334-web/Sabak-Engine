#include "renderer.h"
#include "gl.h"
#include <cstdio>
namespace sk {
static bool g_bad=false;
static unsigned compile(unsigned type,const char* src){unsigned s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);int ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
  if(!ok){g_bad=true;char b[2048];glGetShaderInfoLog(s,2048,nullptr,b);std::fprintf(stderr,"shader error: %s\n",b);}return s;}
static unsigned link(const char* vs,const char* fs){unsigned p=glCreateProgram();glAttachShader(p,compile(GL_VERTEX_SHADER,vs));glAttachShader(p,compile(GL_FRAGMENT_SHADER,fs));glLinkProgram(p);int lk=0;glGetProgramiv(p,GL_LINK_STATUS,&lk);if(!lk)g_bad=true;return p;}

static const char* VS=R"(#version 300 es
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec3 aCol;
uniform mat4 uModel,uVP,uLightVP; uniform vec3 uTint;
out vec3 vW; out vec3 vN; out vec3 vC; out vec4 vL;
void main(){ vec4 w=uModel*vec4(aPos,1.0); vW=w.xyz; vN=mat3(uModel)*aNrm; vC=pow(aCol*uTint,vec3(2.2)); vL=uLightVP*w; gl_Position=uVP*w; })";
static const char* FS=R"(#version 300 es
precision highp float; precision highp sampler2DShadow;
in vec3 vW; in vec3 vN; in vec3 vC; in vec4 vL; out vec4 o;
uniform vec3 uSun,uCam; uniform sampler2DShadow uShadow;
float shadow(){ vec3 p=vL.xyz/vL.w*0.5+0.5; if(p.z>1.0||p.x<0.0||p.x>1.0||p.y<0.0||p.y>1.0) return 1.0;
  float s=0.0; float t=1.0/2048.0; for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++) s+=texture(uShadow,vec3(p.xy+vec2(x,y)*t,p.z-0.0012)); return s/9.0; }
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }
void main(){ vec3 N=normalize(vN); vec3 L=normalize(uSun); vec3 V=normalize(uCam-vW);
  float ndl=max(dot(N,L),0.0); float sh=shadow();
  vec3 amb=mix(vec3(0.20,0.18,0.15),vec3(0.38,0.48,0.62),N.y*0.5+0.5);
  vec3 H=normalize(L+V); float spec=pow(max(dot(N,H),0.0),48.0)*0.15*ndl*sh;
  vec3 col=vC*(amb+vec3(1.25,1.12,0.95)*ndl*sh)+spec;
  float d=length(uCam-vW); float fog=1.0-exp(-d*0.0065); col=mix(col,vec3(0.62,0.68,0.76),clamp(fog,0.0,1.0));
  col=aces(col*0.9); o=vec4(pow(col,vec3(1.0/2.2)),1.0); })";
static const char* SVS=R"(#version 300 es
layout(location=0) in vec3 aPos; uniform mat4 uModel,uLightVP; void main(){ gl_Position=uLightVP*uModel*vec4(aPos,1.0);} )";
static const char* SFS=R"(#version 300 es
precision mediump float; void main(){} )";
static const char* SKVS=R"(#version 300 es
out vec2 vUv; void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); vUv=p; gl_Position=vec4(p*2.0-1.0,0.9999,1.0);} )";
static const char* SKFS=R"(#version 300 es
precision highp float; in vec2 vUv; out vec4 o; void main(){ float t=clamp(vUv.y,0.0,1.0); vec3 hor=vec3(0.70,0.76,0.84),top=vec3(0.26,0.38,0.58);
  o=vec4(pow(mix(hor,top,pow(t,0.7)),vec3(1.0/2.2)),1.0);} )";

bool Renderer::init(int w,int h){
  prog_=link(VS,FS);shProg_=link(SVS,SFS);skyProg_=link(SKVS,SKFS);glGenVertexArrays(1,&skyVao_);
  glGenTextures(1,&shTex_);glBindTexture(GL_TEXTURE_2D,shTex_);glTexStorage2D(GL_TEXTURE_2D,1,GL_DEPTH_COMPONENT24,shSize_,shSize_);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_REF_TO_TEXTURE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1,&shFbo_);glBindFramebuffer(GL_FRAMEBUFFER,shFbo_);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shTex_,0);
  GLenum none=GL_NONE;glDrawBuffers(1,&none);glReadBuffer(GL_NONE);
  glGenFramebuffers(1,&fbo_);glGenRenderbuffers(1,&colorRb_);glGenRenderbuffers(1,&depthRb_);resize(w,h);
  shadersOk=!g_bad;glError=glGetError();return shadersOk&&glError==GL_NO_ERROR;}
void Renderer::resize(int w,int h){w_=w;h_=h;glBindRenderbuffer(GL_RENDERBUFFER,colorRb_);glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,w,h);
  glBindRenderbuffer(GL_RENDERBUFFER,depthRb_);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,w,h);
  glBindFramebuffer(GL_FRAMEBUFFER,fbo_);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,colorRb_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depthRb_);}
int Renderer::upload(const MeshData& m){GpuMesh g;g.count=m.i.size();glGenVertexArrays(1,&g.vao);glBindVertexArray(g.vao);
  glGenBuffers(1,&g.vbo);glBindBuffer(GL_ARRAY_BUFFER,g.vbo);glBufferData(GL_ARRAY_BUFFER,m.v.size()*sizeof(Vertex),m.v.data(),GL_STATIC_DRAW);
  glGenBuffers(1,&g.ibo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g.ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,m.i.size()*4,m.i.data(),GL_STATIC_DRAW);
  for(int a=0;a<3;a++){glEnableVertexAttribArray(a);glVertexAttribPointer(a,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(a*12));}
  meshes_.push_back(g);return meshes_.size()-1;}
void Renderer::setCamera(Vec3 pos,Vec3 target,float fovy){camPos_=pos;camTarget_=target;fov_=fovy;view_=Mat4::lookAt(pos,target,{0,1,0});proj_=Mat4::perspective(fovy,(float)w_/h_,0.3f,500.0f);}
bool Renderer::project(Vec3 p,float& sx,float& sy) const{Mat4 vp=proj_*view_;float x=vp.m[0]*p.x+vp.m[4]*p.y+vp.m[8]*p.z+vp.m[12],y=vp.m[1]*p.x+vp.m[5]*p.y+vp.m[9]*p.z+vp.m[13],w=vp.m[3]*p.x+vp.m[7]*p.y+vp.m[11]*p.z+vp.m[15];
  if(w<=0.01f)return false;sx=(x/w*0.5f+0.5f)*w_;sy=(1-(y/w*0.5f+0.5f))*h_;return true;}
void Renderer::ray(float px,float py,Vec3& o,Vec3& d) const{float nx=px/w_*2-1,ny=1-py/h_*2;float th=std::tan(fov_/2);Vec3 f=norm(camTarget_-camPos_),s=norm(cross(f,{0,1,0})),u=cross(s,f);
  o=camPos_;d=norm(s*(nx*th*((float)w_/h_))+u*(ny*th)+f);}
void Renderer::setSun(Vec3 dir,Vec3 focus,float r){sunDir_=norm(dir);Vec3 eye=focus+sunDir_*(r*2.0f);
  // texel-snap the focus so shadows don't shimmer when the camera moves
  float tex=2*r/shSize_;(void)tex;lightVP_=Mat4::ortho(-r,r,-r,r,0.1f,r*4.0f)*Mat4::lookAt(eye,focus,{0,1,0});}
void Renderer::render(const std::vector<DrawItem>& items){
  stats={};
  // 1) shadow pass
  glBindFramebuffer(GL_FRAMEBUFFER,shFbo_);glViewport(0,0,shSize_,shSize_);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
  glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(2.0f,4.0f);glDisable(GL_CULL_FACE);glUseProgram(shProg_);
  glUniformMatrix4fv(glGetUniformLocation(shProg_,"uLightVP"),1,GL_FALSE,lightVP_.m);
  int uM=glGetUniformLocation(shProg_,"uModel");
  for(auto&it:items){if(!it.castShadow)continue;auto&g=meshes_[it.mesh];glBindVertexArray(g.vao);glUniformMatrix4fv(uM,1,GL_FALSE,it.model.m);glDrawElements(GL_TRIANGLES,g.count,GL_UNSIGNED_INT,0);}
  glDisable(GL_POLYGON_OFFSET_FILL);
  // 2) main pass
  glBindFramebuffer(GL_FRAMEBUFFER,useExt_?ext_:fbo_);glViewport(0,0,w_,h_);glClearColor(0.6f,0.75f,0.9f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);glUseProgram(skyProg_);glBindVertexArray(skyVao_);glDrawArrays(GL_TRIANGLES,0,3);glEnable(GL_DEPTH_TEST);
  glUseProgram(prog_);Mat4 vp=proj_*view_;
  glUniformMatrix4fv(glGetUniformLocation(prog_,"uVP"),1,GL_FALSE,vp.m);glUniformMatrix4fv(glGetUniformLocation(prog_,"uLightVP"),1,GL_FALSE,lightVP_.m);
  glUniform3f(glGetUniformLocation(prog_,"uSun"),sunDir_.x,sunDir_.y,sunDir_.z);glUniform3f(glGetUniformLocation(prog_,"uCam"),camPos_.x,camPos_.y,camPos_.z);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,shTex_);glUniform1i(glGetUniformLocation(prog_,"uShadow"),0);
  int uMM=glGetUniformLocation(prog_,"uModel"),uT=glGetUniformLocation(prog_,"uTint");
  for(auto&it:items){auto&g=meshes_[it.mesh];glBindVertexArray(g.vao);glUniformMatrix4fv(uMM,1,GL_FALSE,it.model.m);glUniform3f(uT,it.tint.x,it.tint.y,it.tint.z);
    glDrawElements(GL_TRIANGLES,g.count,GL_UNSIGNED_INT,0);stats.draws++;stats.tris+=g.count/3;}
}
void Renderer::present(unsigned t,int tw,int th){glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo_);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,t);
  glBlitFramebuffer(0,0,w_,h_,0,0,tw,th,GL_COLOR_BUFFER_BIT,GL_LINEAR);glBindFramebuffer(GL_FRAMEBUFFER,t);}
bool Renderer::readPixels(std::vector<unsigned char>& rgba) const{rgba.assign((size_t)w_*h_*4,0);glBindFramebuffer(GL_FRAMEBUFFER,fbo_);
  glReadPixels(0,0,w_,h_,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
  for(int y=0;y<h_/2;y++)for(int x=0;x<w_*4;x++)std::swap(rgba[(size_t)y*w_*4+x],rgba[(size_t)(h_-1-y)*w_*4+x]);return true;}
}
