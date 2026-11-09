// Sabak Core - OpenGL ES 3.0 renderer: directional shadow map, lit shading with fog, sky gradient, ACES tone map.
#pragma once
#include <vector>
#include "mesh.h"
namespace sk {
struct GpuMesh { unsigned vao=0,vbo=0,ibo=0; int count=0; };
struct DrawItem { int mesh; Mat4 model; Vec3 tint{1,1,1}; bool castShadow=true; };
struct Stats { int draws=0; long tris=0; };
class Renderer {
public:
  bool init(int w,int h);                 // creates offscreen target + shaders (a GL context must be current)
  int upload(const MeshData& m);          // returns mesh id
  void resize(int w,int h);
  void setCamera(Vec3 pos,Vec3 target,float fovyRad);
  void setSun(Vec3 dirToSun,Vec3 focus,float radius);
  void render(const std::vector<DrawItem>& items);
  void present(unsigned targetFbo,int tw,int th);   // blit the rendered frame to a window framebuffer (0 = default)
  bool readPixels(std::vector<unsigned char>& rgba) const;  // top-down RGBA8
  int width() const {return w_;} int height() const {return h_;}
  void setExternalTarget(unsigned fbo){ext_=fbo;useExt_=true;}   // draw straight into a window framebuffer (iOS)
  bool shadersOk=true; unsigned glError=0;
  Stats stats;
private:
  unsigned prog_=0,shProg_=0,skyProg_=0,fbo_=0,colorRb_=0,depthRb_=0,shFbo_=0,shTex_=0,skyVao_=0;
  unsigned ext_=0; bool useExt_=false;
  int w_=0,h_=0,shSize_=2048; std::vector<GpuMesh> meshes_;
  Mat4 view_,proj_,lightVP_; Vec3 camPos_,sunDir_{0.5f,0.8f,0.3f};
};
}
