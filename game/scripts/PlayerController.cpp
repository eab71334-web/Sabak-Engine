// Example gameplay script (C++). Moves the entity with the touch stick / WASD, keeps it on the terrain and picks the animation (idle / walk / run).
#include <cmath>
#include "scene.h"
#include "script.h"
using namespace sk;
struct PlayerController : Script {
  float walk=2.4f, run=6.0f;
  void onUpdate(float dt) override {
    float ax=ctx->input->moveX, ay=ctx->input->moveY, yaw=ctx->input->camYaw;
    float l=std::sqrt(ax*ax+ay*ay); if(l>1){ax/=l;ay/=l;l=1;}
    float fx=-std::sin(yaw), fz=-std::cos(yaw), rx=std::cos(yaw), rz=-std::sin(yaw);
    float wx=rx*ax+fx*-ay, wz=rz*ax+fz*-ay;
    float sp=l>0.65f?run:walk;
    if(l>0.1f){ self->pos.x+=wx*sp*dt*(l>0.65f?1.f:l/0.65f); self->pos.z+=wz*sp*dt*(l>0.65f?1.f:l/0.65f);
      float want=std::atan2(wx,wz)*57.2957795f, d=want-self->rot.y; while(d>180)d-=360; while(d<-180)d+=360; self->rot.y+=d*std::min(1.f,dt*12.f);
      self->anim=l>0.65f?"run":"walk"; }
    else self->anim="idle";
    self->pos.y=ctx->scene->height(self->pos.x,self->pos.z);
  }
};
SABAK_SCRIPT(PlayerController)
