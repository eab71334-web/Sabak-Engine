// Sabak Core - immediate-mode 2D UI renderer: SDF rounded rects, shadows, glows, rings, antialiased text (Poppins, OFL).
#pragma once
#include <string>
#include <vector>
namespace sk {
struct Col { float r=1,g=1,b=1,a=1;
  Col with(float na) const {return {r,g,b,na};}
  Col mul(float k) const {return {r*k,g*k,b*k,a};}
  static Col hex(unsigned h,float a=1){return {((h>>16)&255)/255.f,((h>>8)&255)/255.f,(h&255)/255.f,a};} };
enum FontId { F_REG=0, F_BOLD=1 };
class UI {
public:
  bool init(float scale);                    // scale = framebuffer height / 393 (design height); call with a GL context current
  void begin(int w,int h);                   // start a UI frame (framebuffer pixels)
  void end();
  float s() const {return s_;}               // design-unit -> pixel multiplier. All draw calls below take DESIGN units.
  void rrect(float x,float y,float w,float h,float r,Col top,Col bottom);
  void ring(float x,float y,float w,float h,float r,float bw,Col c);
  void shadow(float x,float y,float w,float h,float r,float blur,Col c);
  void glow(float cx,float cy,float radius,Col c);
  void circle(float cx,float cy,float r,Col top,Col bottom){rrect(cx-r,cy-r,2*r,2*r,r,top,bottom);}
  // text: size in design units (one of 11,13,15,19,26,40,56). align: 0 left, 1 center, 2 right
  void text(const std::string& t,float x,float y,float size,Col c,int font=F_REG,int align=0);
  float measure(const std::string& t,float size,int font=F_REG);
  bool ok=false;
private:
  struct Face { unsigned tex=0; int aw=0; float px=0; void* cd=nullptr; };
  Face& face(float size,int font);
  void quad(float x0,float y0,float x1,float y1,float u0,float v0,float u1,float v1,float hx,float hy,float rad,float bw,float mode,Col a,Col b);
  void flush();
  unsigned prog_=0,vao_=0,vbo_=0; float s_=1; int W_=0,H_=0; std::vector<float> buf_; unsigned curTex_=0;
  std::vector<Face> faces_; std::vector<int> keys_;
};
}
