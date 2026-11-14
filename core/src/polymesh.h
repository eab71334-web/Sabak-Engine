// Sabak Core - polygon (quad) modelling: selection, extrude, inset, bevel(crease), loop cut, mirror, proportional edit, Catmull-Clark.
#pragma once
#include <cstdint>
#include <vector>
#include <map>
#include <set>
#include <string>
#include "vmath.h"
namespace sk {
typedef unsigned long long ull;
struct PolyData { std::vector<Vec3> V; std::vector<std::vector<int>> F; std::map<ull,float> crease; };
struct PolyMesh {
  PolyData d;
  std::set<int> selV, selF; std::set<ull> selE;     // selection (any mode keeps the three sets consistent)
  int mode=2;                                       // 0 vertex, 1 edge, 2 face
  bool mirrorX=true; int subLevels=0;               // modifiers: mirror on X, subdivision levels shown/exported
  static ull key(int a,int b){if(a>b){int t=a;a=b;b=t;}return ((ull)a<<32)|(unsigned)b;}
  // primitives
  void clear(); void cube(Vec3 c,Vec3 h); void cylinder(Vec3 c,float r,float h,int n); void grid(int n,float size);
  // selection
  void selectNone(); void selectAllFaces(); void setMode(int m);
  void toggleVert(int v); void toggleEdge(ull e); void toggleFace(int f);
  void selectRingLoop(ull e);                       // select the whole edge loop through an edge (quad strip)
  // edit ops (return false when nothing to do)
  bool extrude(float dist); bool inset(float amount); bool bevel(float crease);
  bool loopCut(ull edge,float t=0.5f); bool deleteFaces(); bool flip(); bool setCrease(float v);
  void moveSel(Vec3 delta,float radius=0.f); void scaleSel(float f,float radius=0.f);
  Vec3 selCenter() const; Vec3 faceNormal(int f) const; Vec3 faceCenter(int f) const;
  // undo
  void snapshot(); bool undo();
  // evaluation: mirror + subdivision -> triangles with smooth normals
  PolyData evaluate(int levels) const;
  static void triangulate(const PolyData& m,std::vector<Vec3>& P,std::vector<Vec3>& N,std::vector<uint32_t>& I);
  bool save(const std::string& path) const; bool load(const std::string& path);
  std::vector<PolyData> undo_;
 private:
  void syncFromFaces(); void syncFromVerts();
};
}
