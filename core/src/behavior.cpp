// Sabak Core - "Behavior" script: gameplay built from blocks edited on the device (no C++ compile needed).
#include <cmath>
#include <algorithm>
#include "scene.h"
#include "script.h"
namespace sk {
const char* kBlockNames[12]={"Move (touch stick)","Follow entity","Face entity","Patrol","Spin","Bob up/down","Play animation","Snap to ground","Wander","Run away from","Look at (head)","Attack when near"};
const char* kBlockA[12]={"Walk","Speed","","Radius","Deg/s","Height","","","Radius","Speed","","Range"};
const char* kBlockB[12]={"Run","Stop dist","","Speed","","Rate","","","Speed","Safe dist","","Cooldown"};
int blockUsesName(int t){return t==1||t==2||t==9||t==10||t==11?1:t==6?2:0;}   // 1 = entity name, 2 = animation name
namespace {
struct Behavior : Script {
  float speed=0,t=0,base_y=0,sx=0,sz=0,tx=0,tz=0,wt=0;bool inited=false;float cool=0;
  void walkTo(float x,float z,float spd,float dt){float dx=x-self->pos.x,dz=z-self->pos.z;float d=std::sqrt(dx*dx+dz*dz);if(d<1e-4f)return;self->pos.x+=dx/d*spd*dt;self->pos.z+=dz/d*spd*dt;
    float a=std::atan2(dx,dz)*57.2957795f,df=a-self->rot.y;while(df>180)df-=360;while(df<-180)df+=360;self->rot.y+=df*std::min(1.f,dt*10.f);}
  void onUpdate(float dt) override {
    if(!inited){inited=true;base_y=self->pos.y;sx=tx=self->pos.x;sz=tz=self->pos.z;}t+=dt;bool moved=false;float want=0;
    for(const Block& b:self->blocks){
      switch(b.type){
        case 0:{float ax=ctx->input->moveX,ay=ctx->input->moveY,yaw=ctx->input->camYaw;float l=std::sqrt(ax*ax+ay*ay);if(l>1){ax/=l;ay/=l;l=1;}
          float fx=-std::sin(yaw),fz=-std::cos(yaw),rx=std::cos(yaw),rz=-std::sin(yaw);float wx=rx*ax+fx*-ay,wz=rz*ax+fz*-ay;float w=std::max(0.5f,b.a),r=std::max(w,b.b);
          want=l>0.1f?(l>0.65f?r:w*l/0.65f):0.f;speed+=(want-speed)*std::min(1.f,dt*8.f);
          if(l>0.1f){self->pos.x+=wx/l*speed*dt;self->pos.z+=wz/l*speed*dt;float a=std::atan2(wx,wz)*57.2957795f,d=a-self->rot.y;while(d>180)d-=360;while(d<-180)d+=360;self->rot.y+=d*std::min(1.f,dt*12.f);}
          moved=true;self->anim="Locomotion";self->animParam=speed;break;}
        case 1:{Entity* o=ctx->scene->find(b.s);if(!o||o==self)break;float dx=o->pos.x-self->pos.x,dz=o->pos.z-self->pos.z;float d=std::sqrt(dx*dx+dz*dz);float sp=d>std::max(0.3f,b.b)?std::max(0.5f,b.a):0.f;speed+=(sp-speed)*std::min(1.f,dt*6.f);
          if(sp>0)walkTo(o->pos.x,o->pos.z,speed,dt);moved=true;self->anim="Locomotion";self->animParam=speed;break;}
        case 2:{Entity* o=ctx->scene->find(b.s);if(!o||o==self)break;float a=std::atan2(o->pos.x-self->pos.x,o->pos.z-self->pos.z)*57.2957795f,d=a-self->rot.y;while(d>180)d-=360;while(d<-180)d+=360;self->rot.y+=d*std::min(1.f,dt*8.f);break;}
        case 3:{float R=std::max(0.5f,b.a),sp=std::max(0.5f,b.b);float ph=std::fmod(t*sp/R*0.5f,2.f);float k=ph<1?ph:2-ph;float gx=sx-R+2*R*k;float dx=gx-self->pos.x;speed+=(sp-speed)*std::min(1.f,dt*6.f);walkTo(gx,sz,speed,dt);
          if(std::fabs(dx)<0.02f)speed*=0.5f;moved=true;self->anim="Locomotion";self->animParam=speed;break;}
        case 4:self->rot.y=std::fmod(self->rot.y+b.a*dt,360.f);break;
        case 5:self->pos.y=base_y+std::sin(t*b.b*6.2831853f)*b.a;break;
        case 6:if(!moved&&!b.s.empty())self->anim=b.s;break;
        case 7:self->pos.y=ctx->scene->height(self->pos.x,self->pos.z);break;
        case 8:{float R=std::max(0.5f,b.a),sp=std::max(0.5f,b.b);wt-=dt;float dx=tx-self->pos.x,dz=tz-self->pos.z;if(wt<=0||dx*dx+dz*dz<0.2f){float s1=std::sin(t*12.9898f+self->id*78.233f)*43758.5453f,s2=std::sin(t*39.3468f+self->id*11.135f)*24634.6345f;tx=sx+(s1-std::floor(s1)-0.5f)*2*R;tz=sz+(s2-std::floor(s2)-0.5f)*2*R;wt=3+(s1-std::floor(s1))*3;}
          float want2=(dx*dx+dz*dz>0.2f)?sp:0.f;speed+=(want2-speed)*std::min(1.f,dt*5.f);if(want2>0)walkTo(tx,tz,speed,dt);moved=true;self->anim="Locomotion";self->animParam=speed;break;}
        case 10:{Entity* o=ctx->scene->find(b.s);if(!o||o==self){self->lookOn=false;break;}float dx=o->pos.x-self->pos.x,dz=o->pos.z-self->pos.z,dy=(o->pos.y+1.5f)-(self->pos.y+1.6f);float yaw=std::atan2(dx,dz)*57.2957795f-self->rot.y;while(yaw>180)yaw-=360;while(yaw<-180)yaw+=360;
          float d=std::sqrt(dx*dx+dz*dz);self->lookOn=d<12.f&&std::fabs(yaw)<110.f;self->lookYaw=yaw;self->lookPitch=std::atan2(dy,std::max(0.1f,d))*57.2957795f;break;}
        case 11:{Entity* o=ctx->scene->find(b.s);if(!o||o==self)break;cool-=dt;float dx=o->pos.x-self->pos.x,dz=o->pos.z-self->pos.z;if(dx*dx+dz*dz<b.a*b.a&&cool<=0){self->trigger="attack";cool=std::max(0.3f,b.b);}break;}
        case 9:{Entity* o=ctx->scene->find(b.s);if(!o||o==self)break;float dx=self->pos.x-o->pos.x,dz=self->pos.z-o->pos.z;float d=std::sqrt(dx*dx+dz*dz);float sp=d<std::max(1.f,b.b)?std::max(0.5f,b.a):0.f;speed+=(sp-speed)*std::min(1.f,dt*6.f);
          if(sp>0&&d>1e-3f)walkTo(self->pos.x+dx/d*5,self->pos.z+dz/d*5,speed,dt);moved=true;self->anim="Locomotion";self->animParam=speed;break;}
      }}
    if(moved){Vec3 p=self->pos;ctx->scene->resolve(p,0.45f,1.8f);self->pos.x=p.x;self->pos.z=p.z;}}
};
}
void registerBuiltinScripts(){scriptRegistry()["Behavior"]=[]()->Script*{return new Behavior();};}
}
