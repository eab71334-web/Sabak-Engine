// Example gameplay script (C++). Moves the entity with the touch stick / WASD and keeps it on the terrain.
#include <cmath>
#include "scene.h"
#include "script.h"
using namespace sk;
struct PlayerController : Script {
  float speed=5.5f;
  void onUpdate(float dt) override {
    float ax=ctx->input->moveX, ay=ctx->input->moveY, yaw=ctx->input->camYaw;
    float l=std::sqrt(ax*ax+ay*ay); if(l>1){ax/=l;ay/=l;}
    float fx=-std::sin(yaw), fz=-std::cos(yaw), rx=std::cos(yaw), rz=-std::sin(yaw);
    float wx=rx*ax+fx*-ay, wz=rz*ax+fz*-ay;
    self->pos.x+=wx*speed*dt; self->pos.z+=wz*speed*dt;
    self->pos.y=ctx->scene->height(self->pos.x,self->pos.z);
    if(l>0.1f) self->rot.y=std::atan2(wx,wz)*57.2957795f;
  }
};
SABAK_SCRIPT(PlayerController)
