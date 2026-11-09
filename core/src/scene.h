// Sabak Core - scene: entities with transform, primitive renderer and an optional C++ script name. Saved as text.
#pragma once
#include <string>
#include <vector>
#include "ui.h"      // Col
#include "vmath.h"
namespace sk {
enum Prim { P_EMPTY=0,P_BOX,P_SPHERE,P_CYL,P_CONE,P_PLANE,P_TERRAIN,P_PYRAMID,P_COUNT };
extern const char* PRIM_NAME[P_COUNT];
struct Entity { int id=0,parent=-1; std::string name; int prim=P_BOX; Col color; Vec3 pos,rot,scale{1,1,1}; std::string script; };
class Scene {
public:
  std::vector<Entity> ents; int nextId=1;
  Entity& add(int prim,const std::string& name,int parent=-1);
  Entity* get(int id); const Entity* get(int id) const; Entity* find(const std::string& name);
  void remove(int id);                               // removes the entity and all its children
  int duplicate(int id);
  Mat4 local(const Entity& e) const; Mat4 world(int id) const; Vec3 worldPos(int id) const;
  bool save(const std::string& path) const; bool load(const std::string& path);
  void buildTemplate(int t);                         // 0 empty world, 1 village, 2 open terrain
  float height(float x,float z) const;
  std::vector<int> childrenOf(int id) const;
  int depthOf(int id) const;
};
}
