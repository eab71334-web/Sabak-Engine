// Example gameplay script (C++). Moves the entity with the touch stick / WASD, keeps it on the terrain,
// and drives the "Locomotion" blend (idle -> walk -> run) with the real movement speed.
#include <cmath>
#include "scene.h"
#include "script.h"
using namespace sk;
struct PlayerController : Script {
  float walk=2.4f, run=6.0f, speed=0;
  void onUpdate(float dt) override {
    float ax=ctx->input->moveX, ay=ctx->input->moveY, yaw=ctx->input->camYaw;
    float l=std::sqrt(ax*ax+ay*ay); if(l>1){ax/=l;ay/=l;l=1;}
    float fx=-std::sin(yaw), fz=-std::cos(yaw), rx=std::cos(yaw), rz=-std::sin(yaw);
    float wx=rx*ax+fx*-ay, wz=rz*ax+fz*-ay;
    float want=l>0.1f ? (l>0.65f ? run : walk*l/0.65f) : 0.f;
    speed+=(want-speed)*std::min(1.f,dt*8.f);
    if(l>0.1f){ self->pos.x+=wx/l*speed*dt; self->pos.z+=wz/l*speed*dt;
      float a=std::atan2(wx,wz)*57.2957795f, d=a-self->rot.y; while(d>180)d-=360; while(d<-180)d+=360; self->rot.y+=d*std::min(1.f,dt*12.f); }
    self->anim="Locomotion"; self->animParam=speed;
    self->pos.y=ctx->scene->height(self->pos.x,self->pos.z);
  }
};
SABAK_SCRIPT(PlayerController)
