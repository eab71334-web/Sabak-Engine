// Sabak Core - glTF skinned models: loading (cgltf + stb_image), skeletal animation with cross-fade.
#pragma once
#include <string>
#include <vector>
#include <map>
#include "renderer.h"
namespace sk {
extern std::string g_assetDir;                       // folder holding hero.glb, horse.glb and tex/
struct ModelMat { unsigned tex=0; float col[4]={1,1,1,1}; bool mask=false; };
struct ModelPrim { int mesh=-1; int mat=0; };
struct TRS { Vec3 t; float r[4]={0,0,0,1}; Vec3 s{1,1,1}; };
struct AnimChannel { int node=0,path=0; bool step=false; std::vector<float> t,v; };   // path 0 translation, 1 rotation, 2 scale
struct Anim { std::string name; float dur=0; std::vector<AnimChannel> ch; };
struct ClipKey { float t=0; Vec3 rot,pos; };            // rot in degrees (added to the base pose), pos in cm
struct ClipDef { std::string name,base; float dur=1.5f; std::map<int,std::vector<ClipKey>> keys; bool raw=false; };   // raw = baked/retargeted data kept as-is
struct Model {
  std::string name; std::vector<ModelPrim> prims; std::vector<ModelMat> mats;
  std::vector<int> parent; std::vector<std::string> nodeName; std::vector<TRS> rest; std::vector<int> joints; std::vector<Mat4> ibm; std::vector<Anim> anims;
  float fit=1, foot=0, height=1.8f, radius=1.2f;       // fit scales the raw model to `height` metres, foot is the raw min Y
  int animIndex(const std::string& n) const { for(size_t i=0;i<anims.size();i++) if(anims[i].name==n) return (int)i; return -1; }
  void sample(int anim,float t,std::vector<TRS>& out) const;
  std::vector<ClipDef> clips; std::vector<Anim> rawData; int builtin=0;      // anims[builtin+i] is the baked clip i
  void bakeClip(int ci);
  int addClip(const std::string& name,const std::string& base);
  void removeClip(int ci);
  std::string uniqueClipName(const std::string& n) const;
  bool saveClips(const std::string& dir,const std::string& id) const;
  void loadClips(const std::string& dir,const std::string& id);
  void clearClips();
  int retargetFrom(const Model& src,int& mapped);                          // returns number of clips added
};
struct ModelInst {
  const Model* m=nullptr; std::string cur; int a=-1,prev=-1; float t=0,pt=0,blend=1; bool loop=true; std::vector<float> joints;
  void play(const std::string& name,float fade=0.2f);
  void update(float dt,float speed=1.f);
  void evaluate();
};
std::string clipFilePath(const std::string& dir,const std::string& id,const std::string& name);
bool loadModel(Renderer& r,const std::string& path,float targetHeight,Model& out);
}
