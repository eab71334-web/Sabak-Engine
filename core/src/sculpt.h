// Sabak Core - digital clay sculpting (dense mesh, brushes, symmetry, undo, subdivision).
#pragma once
#include <string>
#include <array>
#include <vector>
#include "model.h"
namespace sk {
struct Sculpt {
  std::vector<SkinVertex> v; std::vector<uint32_t> idx;
  std::vector<uint32_t> adjS,adjI,vfS,vfI;           // CSR: vertex neighbours / vertex faces
  int tool=0; float radius=0.15f,strength=0.5f; bool sym=true; unsigned char color[4]={200,60,50,255};   // tools: 0 draw 1 carve 2 smooth 3 inflate 4 pinch 5 grab 6 flatten 7 wrinkle 8 pores 9 paint 10 erase-paint 11 orbit
  bool valid() const {return !v.empty();}
  size_t tris() const {return idx.size()/3;}
  void setMesh(const std::vector<Vec3>& pos,const std::vector<uint32_t>& ind,const std::vector<std::array<unsigned char,4>>* cols=nullptr);   // welds, builds topology
  void fromSphere(int n=36,float r=0.55f,Vec3 c={0,1.1f,0});
  void fromModel(const Model& m,int mode);   // 0 all parts, 1 human body+eyes, 2 human head bust, 3 horse body only
  void fillColor(const unsigned char* c);
  bool subdivide(size_t maxTris=420000);
  bool raycast(Vec3 o,Vec3 d,Vec3& hit,Vec3& n) const;
  void beginStroke(Vec3 hit,Vec3 o,Vec3 d);
  void dab(Vec3 hit,Vec3 o,Vec3 d);               // returns after updating positions and normals
  void endStroke();
  bool undo();
  bool save(const std::string& path) const; bool load(const std::string& path);
  Vec3 pos(uint32_t i) const {return {v[i].p[0],v[i].p[1],v[i].p[2]};}
  float maxY() const;
  void buildModel(Renderer* r,Model& out) const;      // r may be null (export only)
 private:
  struct Undo{std::vector<std::pair<uint32_t,Vec3>> d; std::vector<std::pair<uint32_t,std::array<unsigned char,4>>> c;}; float avgEdge_=0.01f; std::vector<uint32_t> cstamp_; void touchC(uint32_t i);
  std::vector<Undo> undo_; std::vector<uint32_t> stamp_,mark_; uint32_t strokeId_=0,markId_=0; bool stroking_=false;
  Vec3 lastDab_,grabStart_,grabDir_; std::vector<uint32_t> gIdx[2]; std::vector<float> gW[2]; std::vector<Vec3> gBase[2];
  void topo(); void setPos(uint32_t i,Vec3 p); void touch(uint32_t i); void renorm(const std::vector<uint32_t>& a);
  void applyAt(Vec3 c,Vec3 n,int side);
};
}
