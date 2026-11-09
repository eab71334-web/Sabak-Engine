// Sabak Core - demo world (terrain, village, trees, hero) driven by simulated or real input
#pragma once
#include <vector>
#include <string>
#include "renderer.h"
namespace sk {
class Game {
public:
  void init(Renderer& r);
  void press(const std::string& k){keys_[idx(k)]=true;} void release(const std::string& k){keys_[idx(k)]=false;} void releaseAll(){for(bool&b:keys_)b=false;}
  void stick(float x,float y){sx_=x;sy_=y;} void look(float dyaw,float dpitch){yaw_+=dyaw;pitch_=clampf(pitch_+dpitch,-0.2f,1.2f);}
  void setTemplate(Renderer& r,int t){tmpl_=t;items_.clear();init(r);}   // 0 empty world, 1 village, 2 open terrain
  void drawBackdrop(Renderer& r,float t);                                // slow orbit camera for menu backgrounds
  float stickX() const {return sx_;} float stickY() const {return sy_;}
  void update(float dt);
  void draw(Renderer& r);
  Vec3 hero{0,0,4}; float heroYaw=0,time=0; float yaw_=0,pitch_=0.3f,dist_=7;
  float height(float x,float z) const;
private:
  static int idx(const std::string& k){return k=="W"?0:k=="A"?1:k=="S"?2:3;}
  bool keys_[4]={false,false,false,false}; float sx_=0,sy_=0;
  int tmpl_=1; std::vector<DrawItem> items_; int heroMesh_=0,headMesh_=0; Vec3 vel_{0,0,0};
};
}
