#include "renderer.h"
#include "gl.h"
#include <cstdio>
#include <string>
namespace sk {
static bool g_bad=false;
static unsigned compile(unsigned type,const char* src){unsigned s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);int ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
  if(!ok){g_bad=true;char b[2048];glGetShaderInfoLog(s,2048,nullptr,b);std::fprintf(stderr,"shader error: %s\n",b);}return s;}
static unsigned link(const char* vs,const char* fs){unsigned p=glCreateProgram();glAttachShader(p,compile(GL_VERTEX_SHADER,vs));glAttachShader(p,compile(GL_FRAGMENT_SHADER,fs));glLinkProgram(p);int lk=0;glGetProgramiv(p,GL_LINK_STATUS,&lk);if(!lk)g_bad=true;return p;}

static const char* VS=R"(#version 300 es
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm;
#ifdef SKIN
layout(location=2) in vec2 aUV; layout(location=3) in uvec4 aJ; layout(location=4) in vec4 aW; uniform mat4 uJ[80];
#else
layout(location=2) in vec3 aCol;
#endif
uniform mat4 uModel,uVP,uLightVP; uniform vec3 uTint;
out vec3 vW; out vec3 vN; out vec3 vC; out vec4 vL; out vec2 vUV;
void main(){
#ifdef SKIN
  mat4 sm=uJ[aJ.x]*aW.x+uJ[aJ.y]*aW.y+uJ[aJ.z]*aW.z+uJ[aJ.w]*aW.w; vec4 lp=sm*vec4(aPos,1.0); vec3 ln=mat3(sm)*aNrm; vC=uTint; vUV=aUV;
#else
  vec4 lp=vec4(aPos,1.0); vec3 ln=aNrm; vC=pow(aCol*uTint,vec3(2.2)); vUV=vec2(0.0);
#endif
  vec4 w=uModel*lp; vW=w.xyz; vN=mat3(uModel)*ln; vL=uLightVP*w; gl_Position=uVP*w; })";
static const char* FS=R"(#version 300 es
precision highp float; precision highp sampler2DShadow;
in vec3 vW; in vec3 vN; in vec3 vC; in vec4 vL; in vec2 vUV; out vec4 o;
uniform vec3 uSun,uCam,uFog; uniform sampler2DShadow uShadow;
#ifdef SKIN
uniform sampler2D uTex; uniform vec4 uBase; uniform float uMask;
#endif
float shadow(){ vec3 p=vL.xyz/vL.w*0.5+0.5; if(p.z>1.0||p.x<0.0||p.x>1.0||p.y<0.0||p.y>1.0) return 1.0;
  float s=0.0; float t=1.0/2048.0; for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++) s+=texture(uShadow,vec3(p.xy+vec2(x,y)*t,p.z-0.0012)); return s/9.0; }
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }
void main(){ vec3 albedo=vC;
#ifdef SKIN
  vec4 tc=texture(uTex,vUV)*uBase; if(uMask>0.5&&tc.a<0.5) discard; albedo=pow(tc.rgb,vec3(2.2))*vC;
#endif
  vec3 N=normalize(vN); if(!gl_FrontFacing) N=-N; vec3 L=normalize(uSun); vec3 V=normalize(uCam-vW);
  float ndl=max(dot(N,L),0.0); float sh=shadow();
  vec3 amb=mix(vec3(0.30,0.25,0.19),vec3(0.36,0.47,0.66),N.y*0.5+0.5);
  vec3 H=normalize(L+V); float spec=pow(max(dot(N,H),0.0),48.0)*0.15*ndl*sh;
  float rim=pow(1.0-max(dot(N,V),0.0),3.0)*0.10; vec3 col=albedo*(amb+vec3(1.45,1.22,0.96)*ndl*sh)+spec+vec3(0.55,0.68,0.9)*rim*(N.y*0.5+0.5);
  float d=length(uCam-vW); float fog=1.0-exp(-d*0.0030); col=mix(col,uFog,clamp(fog,0.0,1.0));
  col=aces(col*1.0); col=mix(vec3(dot(col,vec3(0.299,0.587,0.114))),col,1.08); o=vec4(pow(col,vec3(1.0/2.2)),1.0); })";
static const char* SVS=R"(#version 300 es
layout(location=0) in vec3 aPos;
#ifdef SKIN
layout(location=2) in vec2 aUV; layout(location=3) in uvec4 aJ; layout(location=4) in vec4 aW; uniform mat4 uJ[80]; out vec2 vUV;
#endif
uniform mat4 uModel,uLightVP;
void main(){
#ifdef SKIN
  mat4 sm=uJ[aJ.x]*aW.x+uJ[aJ.y]*aW.y+uJ[aJ.z]*aW.z+uJ[aJ.w]*aW.w; vUV=aUV; gl_Position=uLightVP*uModel*(sm*vec4(aPos,1.0));
#else
  gl_Position=uLightVP*uModel*vec4(aPos,1.0);
#endif
} )";
static const char* SFS=R"(#version 300 es
precision mediump float;
#ifdef SKIN
in vec2 vUV; uniform sampler2D uTex; uniform float uMask;
#endif
void main(){
#ifdef SKIN
  if(uMask>0.5&&texture(uTex,vUV).a<0.5) discard;
#endif
} )";
static std::string withSkin(const char* src){std::string s=src;size_t p=s.find('\n');return s.substr(0,p+1)+"#define SKIN 1\n"+s.substr(p+1);}
static const char* SKVS=R"(#version 300 es
out vec2 vN; void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); vN=p*2.0-1.0; gl_Position=vec4(vN,0.9999,1.0);} )";
static const char* SKFS=R"(#version 300 es
precision highp float; in vec2 vN; out vec4 o; uniform vec3 uF,uS,uU,uSun; uniform vec2 uT; uniform float uStudio;
void main(){ vec3 r=normalize(uF+uS*(vN.x*uT.x)+uU*(vN.y*uT.y)); float h=r.y;
  vec3 hor=vec3(0.74,0.78,0.84),mid=vec3(0.40,0.54,0.74),top=vec3(0.14,0.27,0.54);
  vec3 c=mix(hor,mid,smoothstep(0.0,0.22,h)); c=mix(c,top,smoothstep(0.18,0.85,h));
  float sd=max(dot(r,normalize(uSun)),0.0); c+=vec3(1.0,0.80,0.52)*(pow(sd,6.0)*0.16+pow(sd,60.0)*0.35+pow(sd,900.0)*2.0);
  c=mix(c,hor*0.85,smoothstep(0.0,-0.12,h));
  if(uStudio>0.5){ vec3 s0=vec3(0.020,0.022,0.028), s1=vec3(0.075,0.085,0.105); c=mix(s1,s0,smoothstep(-0.25,0.7,h)); c+=vec3(0.03,0.04,0.06)*pow(max(0.0,1.0-abs(h)*3.0),2.0); }
  o=vec4(pow(c,vec3(1.0/2.2)),1.0);} )";

bool Renderer::init(int w,int h){
  prog_=link(VS,FS);shProg_=link(SVS,SFS);{std::string a=withSkin(VS),b=withSkin(FS),c=withSkin(SVS),d=withSkin(SFS);skinProg_=link(a.c_str(),b.c_str());skinShProg_=link(c.c_str(),d.c_str());}skyProg_=link(SKVS,SKFS);glGenVertexArrays(1,&skyVao_);
  glGenTextures(1,&shTex_);glBindTexture(GL_TEXTURE_2D,shTex_);glTexStorage2D(GL_TEXTURE_2D,1,GL_DEPTH_COMPONENT24,shSize_,shSize_);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_REF_TO_TEXTURE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1,&shFbo_);glBindFramebuffer(GL_FRAMEBUFFER,shFbo_);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shTex_,0);
  GLenum none=GL_NONE;glDrawBuffers(1,&none);glReadBuffer(GL_NONE);
  glGenFramebuffers(1,&fbo_);glGenRenderbuffers(1,&colorRb_);glGenRenderbuffers(1,&depthRb_);resize(w,h);
  shadersOk=!g_bad;glError=glGetError();return shadersOk&&glError==GL_NO_ERROR;}
bool Renderer::createTarget(Target& t,int w,int h){t.w=w;t.h=h;glGenTextures(1,&t.tex);glBindTexture(GL_TEXTURE_2D,t.tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glGenRenderbuffers(1,&t.depth);glBindRenderbuffer(GL_RENDERBUFFER,t.depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,w,h);
  glGenFramebuffers(1,&t.fbo);glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t.tex,0);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,t.depth);
  bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;glBindFramebuffer(GL_FRAMEBUFFER,useExt_?ext_:fbo_);return ok;}
void Renderer::pushTarget(const Target& t){sExt_=ext_;sUse_=useExt_;sW_=w_;sH_=h_;ext_=t.fbo;useExt_=true;w_=t.w;h_=t.h;}
void Renderer::popTarget(){ext_=sExt_;useExt_=sUse_;w_=sW_;h_=sH_;}
void Renderer::resize(int w,int h){w_=w;h_=h;glBindRenderbuffer(GL_RENDERBUFFER,colorRb_);glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,w,h);
  glBindRenderbuffer(GL_RENDERBUFFER,depthRb_);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,w,h);
  glBindFramebuffer(GL_FRAMEBUFFER,fbo_);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,colorRb_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depthRb_);}
int Renderer::upload(const MeshData& m){GpuMesh g;g.count=m.i.size();glGenVertexArrays(1,&g.vao);glBindVertexArray(g.vao);
  glGenBuffers(1,&g.vbo);glBindBuffer(GL_ARRAY_BUFFER,g.vbo);glBufferData(GL_ARRAY_BUFFER,m.v.size()*sizeof(Vertex),m.v.data(),GL_STATIC_DRAW);
  glGenBuffers(1,&g.ibo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g.ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,m.i.size()*4,m.i.data(),GL_STATIC_DRAW);
  for(int a=0;a<3;a++){glEnableVertexAttribArray(a);glVertexAttribPointer(a,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(a*12));}
  meshes_.push_back(g);return meshes_.size()-1;}
void Renderer::updateSkin(int id,const std::vector<SkinVertex>& v){if(id<0||id>=(int)skinMeshes_.size())return;glBindBuffer(GL_ARRAY_BUFFER,skinMeshes_[id].vbo);glBufferSubData(GL_ARRAY_BUFFER,0,v.size()*sizeof(SkinVertex),v.data());}
int Renderer::uploadSkin(const std::vector<SkinVertex>& v,const std::vector<uint32_t>& ix){GpuMesh g;g.count=(int)ix.size();glGenVertexArrays(1,&g.vao);glBindVertexArray(g.vao);
  glGenBuffers(1,&g.vbo);glBindBuffer(GL_ARRAY_BUFFER,g.vbo);glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(SkinVertex),v.data(),GL_STATIC_DRAW);
  glGenBuffers(1,&g.ibo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g.ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,ix.size()*4,ix.data(),GL_STATIC_DRAW);
  const int S=sizeof(SkinVertex);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,S,(void*)0);glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,S,(void*)12);
  glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,S,(void*)24);glEnableVertexAttribArray(3);glVertexAttribIPointer(3,4,GL_UNSIGNED_BYTE,S,(void*)32);
  glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,S,(void*)36);skinMeshes_.push_back(g);return (int)skinMeshes_.size()-1;}
unsigned Renderer::createTexture(const unsigned char* px,int w,int h){unsigned t=0;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
  glGenerateMipmap(GL_TEXTURE_2D);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);return t;}
void Renderer::setCamera(Vec3 pos,Vec3 target,float fovy){camPos_=pos;camTarget_=target;fov_=fovy;view_=Mat4::lookAt(pos,target,{0,1,0});proj_=Mat4::perspective(fovy,(float)w_/h_,0.3f,500.0f);}
bool Renderer::project(Vec3 p,float& sx,float& sy) const{Mat4 vp=proj_*view_;float x=vp.m[0]*p.x+vp.m[4]*p.y+vp.m[8]*p.z+vp.m[12],y=vp.m[1]*p.x+vp.m[5]*p.y+vp.m[9]*p.z+vp.m[13],w=vp.m[3]*p.x+vp.m[7]*p.y+vp.m[11]*p.z+vp.m[15];
  if(w<=0.01f)return false;sx=(x/w*0.5f+0.5f)*w_;sy=(1-(y/w*0.5f+0.5f))*h_;return true;}
void Renderer::ray(float px,float py,Vec3& o,Vec3& d) const{float nx=px/w_*2-1,ny=1-py/h_*2;float th=std::tan(fov_/2);Vec3 f=norm(camTarget_-camPos_),s=norm(cross(f,{0,1,0})),u=cross(s,f);
  o=camPos_;d=norm(s*(nx*th*((float)w_/h_))+u*(ny*th)+f);}
void Renderer::setSun(Vec3 dir,Vec3 focus,float r){sunDir_=norm(dir);Vec3 eye=focus+sunDir_*(r*2.0f);
  // texel-snap the focus so shadows don't shimmer when the camera moves
  float tex=2*r/shSize_;(void)tex;lightVP_=Mat4::ortho(-r,r,-r,r,0.1f,r*4.0f)*Mat4::lookAt(eye,focus,{0,1,0});}
void Renderer::render(const std::vector<DrawItem>& items,const std::vector<SkinDraw>& skin){
  stats={};
  // 1) shadow pass
  glBindFramebuffer(GL_FRAMEBUFFER,shFbo_);glViewport(0,0,shSize_,shSize_);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
  glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(2.0f,4.0f);glDisable(GL_CULL_FACE);glUseProgram(shProg_);
  glUniformMatrix4fv(glGetUniformLocation(shProg_,"uLightVP"),1,GL_FALSE,lightVP_.m);
  int uM=glGetUniformLocation(shProg_,"uModel");
  for(auto&it:items){if(!it.castShadow)continue;auto&g=meshes_[it.mesh];glBindVertexArray(g.vao);glUniformMatrix4fv(uM,1,GL_FALSE,it.model.m);glDrawElements(GL_TRIANGLES,g.count,GL_UNSIGNED_INT,0);}
  if(!skin.empty()){glUseProgram(skinShProg_);glUniformMatrix4fv(glGetUniformLocation(skinShProg_,"uLightVP"),1,GL_FALSE,lightVP_.m);int sM=glGetUniformLocation(skinShProg_,"uModel"),sJ=glGetUniformLocation(skinShProg_,"uJ"),sMk=glGetUniformLocation(skinShProg_,"uMask");
    glActiveTexture(GL_TEXTURE1);glUniform1i(glGetUniformLocation(skinShProg_,"uTex"),1);
    for(auto&s:skin){auto&g=skinMeshes_[s.mesh];glBindVertexArray(g.vao);glUniformMatrix4fv(sM,1,GL_FALSE,s.model.m);glUniformMatrix4fv(sJ,s.nj,GL_FALSE,s.joints);glUniform1f(sMk,s.mask?1.f:0.f);glBindTexture(GL_TEXTURE_2D,s.tex);glDrawElements(GL_TRIANGLES,g.count,GL_UNSIGNED_INT,0);}
    glActiveTexture(GL_TEXTURE0);}
  glDisable(GL_POLYGON_OFFSET_FILL);
  // 2) main pass
  glBindFramebuffer(GL_FRAMEBUFFER,useExt_?ext_:fbo_);glViewport(0,0,w_,h_);glClearColor(0.6f,0.75f,0.9f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);glUseProgram(skyProg_);{Vec3 f=norm(camTarget_-camPos_),sx=norm(cross(f,{0,1,0})),su=cross(sx,f);float th=std::tan(fov_/2);
    glUniform3f(glGetUniformLocation(skyProg_,"uF"),f.x,f.y,f.z);glUniform3f(glGetUniformLocation(skyProg_,"uS"),sx.x,sx.y,sx.z);glUniform3f(glGetUniformLocation(skyProg_,"uU"),su.x,su.y,su.z);
    glUniform2f(glGetUniformLocation(skyProg_,"uT"),th*((float)w_/h_),th);glUniform3f(glGetUniformLocation(skyProg_,"uSun"),sunDir_.x,sunDir_.y,sunDir_.z);glUniform1f(glGetUniformLocation(skyProg_,"uStudio"),studio_?1.f:0.f);}glBindVertexArray(skyVao_);glDrawArrays(GL_TRIANGLES,0,3);glEnable(GL_DEPTH_TEST);
  glUseProgram(prog_);Mat4 vp=proj_*view_;
  glUniformMatrix4fv(glGetUniformLocation(prog_,"uVP"),1,GL_FALSE,vp.m);glUniformMatrix4fv(glGetUniformLocation(prog_,"uLightVP"),1,GL_FALSE,lightVP_.m);
  glUniform3f(glGetUniformLocation(prog_,"uFog"),studio_?0.035f:0.74f,studio_?0.040f:0.78f,studio_?0.050f:0.84f);glUniform3f(glGetUniformLocation(prog_,"uSun"),sunDir_.x,sunDir_.y,sunDir_.z);glUniform3f(glGetUniformLocation(prog_,"uCam"),camPos_.x,camPos_.y,camPos_.z);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,shTex_);glUniform1i(glGetUniformLocation(prog_,"uShadow"),0);
  int uMM=glGetUniformLocation(prog_,"uModel"),uT=glGetUniformLocation(prog_,"uTint");
  for(auto&it:items){auto&g=meshes_[it.mesh];glBindVertexArray(g.vao);glUniformMatrix4fv(uMM,1,GL_FALSE,it.model.m);glUniform3f(uT,it.tint.x,it.tint.y,it.tint.z);
    glDrawElements(GL_TRIANGLES,g.count,GL_UNSIGNED_INT,0);stats.draws++;stats.tris+=g.count/3;}
  if(!skin.empty()){glUseProgram(skinProg_);glUniformMatrix4fv(glGetUniformLocation(skinProg_,"uVP"),1,GL_FALSE,vp.m);glUniformMatrix4fv(glGetUniformLocation(skinProg_,"uLightVP"),1,GL_FALSE,lightVP_.m);
    glUniform3f(glGetUniformLocation(skinProg_,"uFog"),studio_?0.035f:0.74f,studio_?0.040f:0.78f,studio_?0.050f:0.84f);glUniform3f(glGetUniformLocation(skinProg_,"uSun"),sunDir_.x,sunDir_.y,sunDir_.z);glUniform3f(glGetUniformLocation(skinProg_,"uCam"),camPos_.x,camPos_.y,camPos_.z);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,shTex_);glUniform1i(glGetUniformLocation(skinProg_,"uShadow"),0);glUniform1i(glGetUniformLocation(skinProg_,"uTex"),1);
    int kM=glGetUniformLocation(skinProg_,"uModel"),kJ=glGetUniformLocation(skinProg_,"uJ"),kT=glGetUniformLocation(skinProg_,"uTint"),kB=glGetUniformLocation(skinProg_,"uBase"),kMk=glGetUniformLocation(skinProg_,"uMask");
    for(auto&s:skin){auto&g=skinMeshes_[s.mesh];glBindVertexArray(g.vao);glUniformMatrix4fv(kM,1,GL_FALSE,s.model.m);glUniformMatrix4fv(kJ,s.nj,GL_FALSE,s.joints);glUniform3f(kT,s.tint.x,s.tint.y,s.tint.z);glUniform4fv(kB,1,s.col);glUniform1f(kMk,s.mask?1.f:0.f);
      glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,s.tex);glDrawElements(GL_TRIANGLES,g.count,GL_UNSIGNED_INT,0);stats.draws++;stats.tris+=g.count/3;}
    glActiveTexture(GL_TEXTURE0);}
}
void Renderer::present(unsigned t,int tw,int th){glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo_);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,t);
  glBlitFramebuffer(0,0,w_,h_,0,0,tw,th,GL_COLOR_BUFFER_BIT,GL_LINEAR);glBindFramebuffer(GL_FRAMEBUFFER,t);}
bool Renderer::readPixels(std::vector<unsigned char>& rgba) const{rgba.assign((size_t)w_*h_*4,0);glBindFramebuffer(GL_FRAMEBUFFER,fbo_);
  glReadPixels(0,0,w_,h_,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
  for(int y=0;y<h_/2;y++)for(int x=0;x<w_*4;x++)std::swap(rgba[(size_t)y*w_*4+x],rgba[(size_t)(h_-1-y)*w_*4+x]);return true;}
}
