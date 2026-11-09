#include "scene.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
namespace sk {
const char* PRIM_NAME[P_COUNT]={"Empty","Box","Sphere","Cylinder","Cone","Plane","Terrain","Pyramid"};
static float hash(int a,int b){unsigned h=a*374761393u+b*668265263u;h=(h^(h>>13))*1274126177u;return ((h^(h>>16))&0xffffff)/16777215.0f;}
static float vnoise(float x,float z){int xi=(int)std::floor(x),zi=(int)std::floor(z);float fx=x-xi,fz=z-zi;fx=fx*fx*(3-2*fx);fz=fz*fz*(3-2*fz);
  float a=hash(xi,zi),b=hash(xi+1,zi),c=hash(xi,zi+1),d=hash(xi+1,zi+1);return (a+(b-a)*fx)+((c+(d-c)*fx)-(a+(b-a)*fx))*fz;}
static float fbm(float x,float z){float s=0,a=0.5f;for(int i=0;i<5;i++){s+=a*vnoise(x,z);x*=2;z*=2;a*=0.5f;}return s;}
float Scene::height(float x,float z) const{float d=std::sqrt(x*x+z*z),flat=clampf(1-(d-18)/30,0,1);float h=fbm(x*0.011f,z*0.011f)*22-5;return h*(1-flat*0.9f)+0.4f*flat;}
Entity& Scene::add(int prim,const std::string& name,int parent){Entity e;e.id=nextId++;e.prim=prim;e.name=name;e.parent=parent;e.color=Col{0.8f,0.8f,0.85f,1};ents.push_back(e);return ents.back();}
Entity* Scene::get(int id){for(auto&e:ents)if(e.id==id)return &e;return nullptr;}
const Entity* Scene::get(int id) const{for(auto&e:ents)if(e.id==id)return &e;return nullptr;}
Entity* Scene::find(const std::string& n){for(auto&e:ents)if(e.name==n)return &e;return nullptr;}
std::vector<int> Scene::childrenOf(int id) const{std::vector<int> r;for(auto&e:ents)if(e.parent==id)r.push_back(e.id);return r;}
int Scene::depthOf(int id) const{int d=0;const Entity* e=get(id);while(e&&e->parent>=0&&d<16){e=get(e->parent);d++;}return d;}
void Scene::remove(int id){std::vector<int> del{id};for(size_t i=0;i<del.size();i++)for(int c:childrenOf(del[i]))del.push_back(c);
  ents.erase(std::remove_if(ents.begin(),ents.end(),[&](const Entity& e){return std::find(del.begin(),del.end(),e.id)!=del.end();}),ents.end());}
int Scene::duplicate(int id){const Entity* o=get(id);if(!o)return -1;Entity c=*o;c.id=nextId++;c.name=o->name+" copy";c.pos.x+=1.5f;ents.push_back(c);int nid=c.id;
  std::vector<std::pair<int,int>> map{{id,nid}};for(size_t i=0;i<map.size();i++)for(int ch:childrenOf(map[i].first)){Entity cc=*get(ch);cc.id=nextId++;cc.parent=map[i].second;ents.push_back(cc);map.push_back({ch,cc.id});}return nid;}
Mat4 Scene::local(const Entity& e) const{const float D=0.0174532925f;return Mat4::translate(e.pos)*Mat4::rotY(e.rot.y*D)*Mat4::rotX(e.rot.x*D)*Mat4::rotZ(e.rot.z*D)*Mat4::scale(e.scale);}
Mat4 Scene::world(int id) const{const Entity* e=get(id);if(!e)return Mat4::identity();Mat4 m=local(*e);int guard=0;while(e->parent>=0&&guard++<16){e=get(e->parent);if(!e)break;m=local(*e)*m;}return m;}
Vec3 Scene::worldPos(int id) const{Mat4 m=world(id);return {m.m[12],m.m[13],m.m[14]};}
bool Scene::save(const std::string& path) const{std::ofstream f(path);if(!f)return false;f<<"SABAKSCENE 1\n";
  for(auto&e:ents){std::string n=e.name;for(char&c:n)if(c=='|'||c=='\n')c='_';
    f<<e.id<<'|'<<e.parent<<'|'<<e.prim<<'|'<<n<<'|'<<e.color.r<<','<<e.color.g<<','<<e.color.b<<'|'<<e.pos.x<<','<<e.pos.y<<','<<e.pos.z<<'|'<<e.rot.x<<','<<e.rot.y<<','<<e.rot.z<<'|'<<e.scale.x<<','<<e.scale.y<<','<<e.scale.z<<'|'<<e.script<<'\n';}return true;}
static std::vector<std::string> split(const std::string& s,char d){std::vector<std::string> r;std::string cur;for(char c:s){if(c==d){r.push_back(cur);cur.clear();}else cur+=c;}r.push_back(cur);return r;}
static Vec3 v3(const std::string& s){auto p=split(s,',');if(p.size()<3)return {};return {(float)atof(p[0].c_str()),(float)atof(p[1].c_str()),(float)atof(p[2].c_str())};}
bool Scene::load(const std::string& path){std::ifstream f(path);if(!f)return false;std::string line;std::getline(f,line);if(line.rfind("SABAKSCENE",0)!=0)return false;ents.clear();nextId=1;
  while(std::getline(f,line)){auto p=split(line,'|');if(p.size()<9)continue;Entity e;e.id=atoi(p[0].c_str());e.parent=atoi(p[1].c_str());e.prim=std::min(P_COUNT-1,std::max(0,atoi(p[2].c_str())));e.name=p[3];
    Vec3 c=v3(p[4]);e.color=Col{c.x,c.y,c.z,1};e.pos=v3(p[5]);e.rot=v3(p[6]);e.scale=v3(p[7]);e.script=p[8];ents.push_back(e);nextId=std::max(nextId,e.id+1);}return true;}
static Col H(unsigned h){return Col::hex(h);}
void Scene::buildTemplate(int t){ents.clear();nextId=1;
  { auto& g=add(P_TERRAIN,"Terrain");g.color=Col{1,1,1,1}; }
  if(t==1){float hs[5][2]={{-9,-8},{8,-10},{12,6},{-12,8},{0,-16}};
    for(int i=0;i<5;i++){float x=hs[i][0],z=hs[i][1],w=5+(i%2)*1.5f;int root=add(P_EMPTY,"House "+std::to_string(i+1)).id;get(root)->pos={x,height(x,z),z};
      auto& b=add(P_BOX,"Walls",root);b.pos={0,1.5f,0};b.scale={w,3,4.5f};b.color=H(0xd8c4a0);auto& r=add(P_PYRAMID,"Roof",root);r.pos={0,3,0};r.rot={0,45,0};r.scale={w*0.78f,2.2f,w*0.78f};r.color=H(0xb23a2a);}}
  if(t!=0){int n=0;for(int i=0;i<420&&n<110;i++){float x=(hash(i,1)-0.5f)*240,z=(hash(i,2)-0.5f)*240,y=height(x,z);if(y<0.3f||std::sqrt(x*x+z*z)<22||y>13)continue;float s=0.8f+hash(i,3)*0.9f;n++;
      int root=add(P_EMPTY,"Tree "+std::to_string(n)).id;get(root)->pos={x,y,z};get(root)->scale={s,s,s};
      auto& tr=add(P_CYL,"Trunk",root);tr.scale={0.5f,2.2f,0.5f};tr.color=H(0x5c3d24);auto& cr=add(P_CONE,"Crown",root);cr.pos={0,2.2f,0};cr.scale={3,4.2f,3};cr.color=H(0x2a6b2d);}}
  if(t!=2){for(int i=0;i<5;i++){float a=i/5.0f*6.28f,x=std::cos(a)*7,z=std::sin(a)*7;auto& c=add(P_SPHERE,"Crystal "+std::to_string(i+1));c.pos={x,height(x,z)+1.2f,z};c.scale={0.9f,1.7f,0.9f};c.color=H(0x4dd9ff);c.script="Bobber";}}
  int hero=add(P_EMPTY,"Hero").id;get(hero)->pos={0,height(0,4),4};get(hero)->script="PlayerController";
  auto& body=add(P_CYL,"Body",hero);body.scale={0.76f,1.5f,0.76f};body.color=H(0xb32428);auto& head=add(P_SPHERE,"Head",hero);head.pos={0,1.75f,0};head.scale={0.52f,0.52f,0.52f};head.color=H(0xe8b894);}
}
