#include "game.h"
namespace sk {
static float hash(int a,int b){unsigned h=a*374761393u+b*668265263u;h=(h^(h>>13))*1274126177u;return ((h^(h>>16))&0xffffff)/16777215.0f;}
static float vnoise(float x,float z){int xi=std::floor(x),zi=std::floor(z);float fx=x-xi,fz=z-zi;fx=fx*fx*(3-2*fx);fz=fz*fz*(3-2*fz);
  float a=hash(xi,zi),b=hash(xi+1,zi),c=hash(xi,zi+1),d=hash(xi+1,zi+1);return (a+(b-a)*fx)+((c+(d-c)*fx)-(a+(b-a)*fx))*fz;}
static float fbm(float x,float z){float s=0,a=0.5f;for(int i=0;i<5;i++){s+=a*vnoise(x,z);x*=2;z*=2;a*=0.5f;}return s;}
float Game::height(float x,float z) const{float d=std::sqrt(x*x+z*z),flat=clampf(1-(d-18)/30,0,1);float h=fbm(x*0.011f,z*0.011f)*22-5;return h*(1-flat*0.9f)+0.4f*flat;}
void Game::init(Renderer& r){
  auto terr=makeTerrain(260,180,[&](float x,float z){return height(x,z);},[&](float x,float y,float z){(void)x;(void)z;
    if(y<0.2f)return Vec3{0.78f,0.72f,0.5f};if(y<2.0f)return Vec3{0.42f,0.62f,0.28f};if(y<9)return Vec3{0.30f,0.52f,0.22f};return Vec3{0.5f,0.5f,0.48f};});
  int tm=r.upload(terr);items_.push_back({tm,Mat4::identity(),{1,1,1},false});
  int box=r.upload(makeBox(1,1,1,{0.84f,0.76f,0.6f})),roof=r.upload(makeCylinder(1,0.01f,1,4,{0.58f,0.22f,0.16f}));
  int trunk=r.upload(makeCylinder(0.3f,0.2f,2.2f,8,{0.36f,0.24f,0.14f})),crown=r.upload(makeCone(1.5f,4.2f,10,{0.16f,0.4f,0.17f}));
  int crystal=r.upload(makeSphere(0.5f,14,10,{0.3f,0.85f,1.0f}));
  float hs[5][2]={{-9,-8},{8,-10},{12,6},{-12,8},{0,-16}};
  for(int i=0;i<5;i++){float x=hs[i][0],z=hs[i][1],y=height(x,z),w=5+(i%2)*1.5f;
    items_.push_back({box,Mat4::translate({x,y+1.5f,z})*Mat4::scale({w,3,4.5f})});
    items_.push_back({roof,Mat4::translate({x,y+3,z})*Mat4::rotY(0.785f)*Mat4::scale({w*0.78f,2.2f,w*0.78f})});}
  for(int i=0;i<420;i++){float x=(hash(i,1)-0.5f)*240,z=(hash(i,2)-0.5f)*240,y=height(x,z);if(y<0.3f||std::sqrt(x*x+z*z)<22||y>13)continue;float s=0.8f+hash(i,3)*0.9f;
    items_.push_back({trunk,Mat4::translate({x,y,z})*Mat4::scale({s,s,s})});items_.push_back({crown,Mat4::translate({x,y+2.2f*s,z})*Mat4::scale({s,s,s})});}
  for(int i=0;i<5;i++){float a=i/5.0f*6.28f,x=std::cos(a)*7,z=std::sin(a)*7;items_.push_back({crystal,Mat4::translate({x,height(x,z)+1.2f,z})*Mat4::scale({0.7f,1.4f,0.7f}),{1.0f,1.0f,1.0f},true});}
  heroMesh_=r.upload(makeCylinder(0.38f,0.3f,1.5f,10,{0.6f,0.12f,0.12f}));headMesh_=r.upload(makeSphere(0.26f,12,8,{0.9f,0.72f,0.58f}));
  hero.y=height(hero.x,hero.z);}
void Game::update(float dt){time+=dt;float ax=sx_,ay=sy_;if(keys_[1])ax-=1;if(keys_[3])ax+=1;if(keys_[0])ay-=1;if(keys_[2])ay+=1;
  float l=std::sqrt(ax*ax+ay*ay);if(l>1){ax/=l;ay/=l;}
  float fx=-std::sin(yaw_),fz=-std::cos(yaw_),rx=std::cos(yaw_),rz=-std::sin(yaw_);float wx=rx*ax+fx*-ay,wz=rz*ax+fz*-ay;
  float sp=5.5f;hero.x+=wx*sp*dt;hero.z+=wz*sp*dt;hero.y=height(hero.x,hero.z);if(l>0.1f)heroYaw=std::atan2(wx,wz);}
void Game::draw(Renderer& r){
  std::vector<DrawItem> all=items_;
  all.push_back({heroMesh_,Mat4::translate({hero.x,hero.y,hero.z})*Mat4::rotY(heroYaw)});
  all.push_back({headMesh_,Mat4::translate({hero.x,hero.y+1.75f,hero.z})});
  Vec3 t{hero.x,hero.y+1.4f,hero.z};Vec3 c=t+Vec3{std::sin(yaw_)*std::cos(pitch_)*dist_,std::sin(pitch_)*dist_,std::cos(yaw_)*std::cos(pitch_)*dist_};
  float gh=height(c.x,c.z);if(c.y<gh+0.6f)c.y=gh+0.6f;
  r.setCamera(c,t,1.0f);r.setSun({0.5f,0.8f,0.35f},{hero.x,hero.y,hero.z},45);r.render(all);}
}
