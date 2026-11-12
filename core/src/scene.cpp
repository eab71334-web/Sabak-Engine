#include <cmath>
#include "scene.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
namespace sk {
const char* PRIM_NAME[P_COUNT]={"Empty","Box","Sphere","Cylinder","Cone","Plane","Terrain","Pyramid","Model"};
static float hash(int a,int b){unsigned h=a*374761393u+b*668265263u;h=(h^(h>>13))*1274126177u;return ((h^(h>>16))&0xffffff)/16777215.0f;}
static float vnoise(float x,float z){int xi=(int)std::floor(x),zi=(int)std::floor(z);float fx=x-xi,fz=z-zi;fx=fx*fx*(3-2*fx);fz=fz*fz*(3-2*fz);
  float a=hash(xi,zi),b=hash(xi+1,zi),c=hash(xi,zi+1),d=hash(xi+1,zi+1);return (a+(b-a)*fx)+((c+(d-c)*fx)-(a+(b-a)*fx))*fz;}
static float fbm(float x,float z){float s=0,a=0.5f;for(int i=0;i<5;i++){s+=a*vnoise(x,z);x*=2;z*=2;a*=0.5f;}return s;}
float Scene::height(float x,float z) const{float d=std::sqrt(x*x+z*z),flat=clampf(1-(d-18)/30,0,1);float h=fbm(x*0.011f,z*0.011f)*22-5;return h*(1-flat*0.9f)+0.4f*flat;}
float Scene::noise(float x,float z) const{return fbm(x,z);}
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
    f<<e.id<<'|'<<e.parent<<'|'<<e.prim<<'|'<<n<<'|'<<e.color.r<<','<<e.color.g<<','<<e.color.b<<'|'<<e.pos.x<<','<<e.pos.y<<','<<e.pos.z<<'|'<<e.rot.x<<','<<e.rot.y<<','<<e.rot.z<<'|'<<e.scale.x<<','<<e.scale.y<<','<<e.scale.z<<'|'<<e.script<<'|'<<e.asset<<'|'<<e.anim<<'|';for(auto& b:e.blocks){std::string t=b.s;for(char& c:t)if(c=='|'||c==':'||c==';'||c=='\n')c='_';f<<b.type<<':'<<b.a<<':'<<b.b<<':'<<t<<';';}f<<'\n';}return true;}
static std::vector<std::string> split(const std::string& s,char d){std::vector<std::string> r;std::string cur;for(char c:s){if(c==d){r.push_back(cur);cur.clear();}else cur+=c;}r.push_back(cur);return r;}
static Vec3 v3(const std::string& s){auto p=split(s,',');if(p.size()<3)return {};return {(float)atof(p[0].c_str()),(float)atof(p[1].c_str()),(float)atof(p[2].c_str())};}
bool Scene::load(const std::string& path){std::ifstream f(path);if(!f)return false;std::string line;std::getline(f,line);if(line.rfind("SABAKSCENE",0)!=0)return false;ents.clear();nextId=1;
  while(std::getline(f,line)){auto p=split(line,'|');if(p.size()<9)continue;Entity e;e.id=atoi(p[0].c_str());e.parent=atoi(p[1].c_str());e.prim=std::min(P_COUNT-1,std::max(0,atoi(p[2].c_str())));e.name=p[3];
    Vec3 c=v3(p[4]);e.color=Col{c.x,c.y,c.z,1};e.pos=v3(p[5]);e.rot=v3(p[6]);e.scale=v3(p[7]);e.script=p[8];if(p.size()>10){e.asset=p[9];e.anim=p[10].empty()?"idle":p[10];}if(p.size()>11){for(auto& bs:split(p[11],';')){auto q=split(bs,':');if(q.size()<4)continue;Block b;b.type=atoi(q[0].c_str());b.a=(float)atof(q[1].c_str());b.b=(float)atof(q[2].c_str());b.s=q[3];e.blocks.push_back(b);}}ents.push_back(e);nextId=std::max(nextId,e.id+1);}return true;}

static bool invAffine(const Mat4& a,Mat4& o){const float* m=a.m;float A[3][3]={{m[0],m[4],m[8]},{m[1],m[5],m[9]},{m[2],m[6],m[10]}};
  float det=A[0][0]*(A[1][1]*A[2][2]-A[1][2]*A[2][1])-A[0][1]*(A[1][0]*A[2][2]-A[1][2]*A[2][0])+A[0][2]*(A[1][0]*A[2][1]-A[1][1]*A[2][0]);if(std::fabs(det)<1e-12f)return false;float id=1.f/det;float I[3][3];
  I[0][0]=(A[1][1]*A[2][2]-A[1][2]*A[2][1])*id;I[0][1]=(A[0][2]*A[2][1]-A[0][1]*A[2][2])*id;I[0][2]=(A[0][1]*A[1][2]-A[0][2]*A[1][1])*id;
  I[1][0]=(A[1][2]*A[2][0]-A[1][0]*A[2][2])*id;I[1][1]=(A[0][0]*A[2][2]-A[0][2]*A[2][0])*id;I[1][2]=(A[0][2]*A[1][0]-A[0][0]*A[1][2])*id;
  I[2][0]=(A[1][0]*A[2][1]-A[1][1]*A[2][0])*id;I[2][1]=(A[0][1]*A[2][0]-A[0][0]*A[2][1])*id;I[2][2]=(A[0][0]*A[1][1]-A[0][1]*A[1][0])*id;
  for(int i=0;i<16;i++)o.m[i]=0;for(int r=0;r<3;r++)for(int c=0;c<3;c++)o.m[c*4+r]=I[r][c];float t[3]={m[12],m[13],m[14]};for(int r=0;r<3;r++)o.m[12+r]=-(I[r][0]*t[0]+I[r][1]*t[1]+I[r][2]*t[2]);o.m[15]=1;return true;}
void Scene::buildSolids(){solids.clear();for(auto& e:ents){if(e.prim==P_EMPTY||e.prim==P_PLANE||e.prim==P_TERRAIN||e.prim==P_MODEL||!e.script.empty())continue;const std::string& n=e.name;
    if(n.rfind("Path",0)==0||n.find("Floor")!=std::string::npos||n.find("Ground")!=std::string::npos||n.find("Stage")!=std::string::npos||n.find("Grass")!=std::string::npos)continue;
    SolidBox b;b.w=world(e.id);if(!invAffine(b.w,b.inv))continue;const float* m=b.w.m;
    b.sx=std::sqrt(m[0]*m[0]+m[1]*m[1]+m[2]*m[2]);b.sy=std::sqrt(m[4]*m[4]+m[5]*m[5]+m[6]*m[6]);b.sz=std::sqrt(m[8]*m[8]+m[9]*m[9]+m[10]*m[10]);
    float hy=0.5f*(std::fabs(m[1])+std::fabs(m[5])+std::fabs(m[9]));b.ymin=m[13]-hy;b.ymax=m[13]+hy;if(b.ymax-b.ymin<0.5f||b.sx<0.2f||b.sz<0.2f)continue;solids.push_back(b);}}
void Scene::resolve(Vec3& p,float radius,float height) const{for(int it=0;it<2;it++)for(auto& b:solids){if(p.y>b.ymax-0.15f||p.y+height<b.ymin+0.2f)continue;const float* iv=b.inv.m;
    float lx=iv[0]*p.x+iv[4]*p.y+iv[8]*p.z+iv[12],lz=iv[2]*p.x+iv[6]*p.y+iv[10]*p.z+iv[14];float rx=radius/b.sx,rz=radius/b.sz;float ex=0.5f+rx,ez=0.5f+rz;if(std::fabs(lx)>=ex||std::fabs(lz)>=ez)continue;
    float px=(ex-std::fabs(lx))*b.sx,pz=(ez-std::fabs(lz))*b.sz;if(px<pz)lx=(lx<0?-ex:ex);else lz=(lz<0?-ez:ez);
    const float* w=b.w.m;float ly=iv[1]*p.x+iv[5]*p.y+iv[9]*p.z+iv[13];p.x=w[0]*lx+w[4]*ly+w[8]*lz+w[12];p.z=w[2]*lx+w[6]*ly+w[10]*lz+w[14];}}
static Col H(unsigned h){return Col::hex(h);}
static Col jit(unsigned h,float seed,float amt){float k=1.0f+(hash((int)seed,7)-0.5f)*2*amt;Col c=Col::hex(h);return Col{std::min(1.f,c.r*k),std::min(1.f,c.g*(1.0f+(hash((int)seed,11)-0.5f)*amt)),std::min(1.f,c.b*k),1};}
void Scene::buildTemplate(int t){ents.clear();nextId=1;
  if(t==3||t==4||t==5){ // Animation Studio / Character Creator: dark stage, one hero
    { auto& f=add(P_PLANE,"StudioFloor");f.pos={0,0,0};f.scale={40,1,40};f.color=H(0x1b1714); }
    { auto& p=add(P_CYL,"Stage");p.pos={0,0.04f,0};p.scale={5.2f,0.08f,5.2f};p.color=H(0x3a2d24); }
    { auto& hero=add(P_MODEL,"Hero");hero.asset=t==5?"sculpt:S1":t==4?"char:C1":"hero";hero.color=Col{1,1,1,1};hero.pos={0,0.08f,0}; }
    return; }
  { auto& g=add(P_TERRAIN,"Terrain");g.color=Col{1,1,1,1}; }
  if(t==1){float hs[5][2]={{-10,-9},{9,-11},{13,6},{-13,8},{0,-17}};
    for(int i=0;i<5;i++){float x=hs[i][0],z=hs[i][1],w=5+(i%2)*1.5f;int root=add(P_EMPTY,"House "+std::to_string(i+1)).id;get(root)->pos={x,height(x,z),z};get(root)->rot={0,(float)((i*37)%40)-20,0};
      add(P_BOX,"Foundation",root).pos={0,0.2f,0};get(ents.back().id)->scale={w+0.4f,0.4f,4.9f};ents.back().color=H(0x6b665e);
      { auto& b=add(P_BOX,"Walls",root);b.pos={0,1.7f,0};b.scale={w,2.8f,4.5f};b.color=jit(0xcfc6b4,i*5,0.06f); }
      { auto& r=add(P_PYRAMID,"Roof",root);r.pos={0,3.1f,0};r.rot={0,45,0};r.scale={w*0.86f,2.3f,w*0.86f};r.color=jit(0x7a3a33,i*3,0.12f); }
      { auto& d=add(P_BOX,"Door",root);d.pos={0,1.0f,2.3f};d.scale={0.95f,1.7f,0.14f};d.color=H(0x4a3426); }
      for(int k=-1;k<=1;k+=2){auto& wn=add(P_BOX,k<0?"Window L":"Window R",root);wn.pos={k*w*0.3f,2.0f,2.28f};wn.scale={0.85f,0.8f,0.12f};wn.color=H(0x5d7f9a);}
      { auto& c=add(P_BOX,"Chimney",root);c.pos={w*0.25f,4.0f,-0.9f};c.scale={0.7f,1.8f,0.7f};c.color=H(0x857867); }}
    for(int i=0;i<6;i++){float x=-2+hash(i,31)*4,z=-14+i*3.2f;auto& p=add(P_PLANE,"Path "+std::to_string(i+1));p.pos={x,height(x,z)+0.06f,z};p.scale={3.0f,1,3.4f};p.rot={0,hash(i,5)*30-15,0};p.color=H(0x9a8c6c);}}
  if(t!=0){int n=0;for(int i=0;i<500&&n<115;i++){float x=(hash(i,1)-0.5f)*240,z=(hash(i,2)-0.5f)*240,y=height(x,z);if(y<0.3f||std::sqrt(x*x+z*z)<24||y>13)continue;float s=0.8f+hash(i,3)*0.9f;n++;
      int root=add(P_EMPTY,"Tree "+std::to_string(n)).id;get(root)->pos={x,y,z};get(root)->scale={s,s,s};
      { auto& tr=add(P_CYL,"Trunk",root);tr.scale={0.5f,2.4f,0.5f};tr.color=H(0x4a3a2c); }
      { auto& c1=add(P_CONE,"Crown A",root);c1.pos={0,1.6f,0};c1.scale={3.2f,3.4f,3.2f};c1.color=jit(0x2c5535,i,0.18f); }
      { auto& c2=add(P_CONE,"Crown B",root);c2.pos={0,3.5f,0};c2.scale={2.4f,2.8f,2.4f};c2.color=jit(0x346b3c,i+99,0.18f); }}}
  { int nb=t==0?10:26,nr=t==0?8:24;int n=0;for(int i=0;i<400&&n<nr;i++){float x=(hash(i,41)-0.5f)*200,z=(hash(i,42)-0.5f)*200,y=height(x,z);if(y<0.3f||std::sqrt(x*x+z*z)<(t==0?10:20))continue;n++;
      auto& r=add(P_SPHERE,"Rock "+std::to_string(n));float s=0.7f+hash(i,43)*1.6f;r.pos={x,y+0.15f*s,z};r.scale={1.6f*s,1.0f*s,1.3f*s};r.rot={0,hash(i,44)*180,0};r.color=jit(0x77777a,i,0.14f);}
    n=0;for(int i=0;i<400&&n<nb;i++){float x=(hash(i,51)-0.5f)*160,z=(hash(i,52)-0.5f)*160,y=height(x,z);if(y<0.4f||std::sqrt(x*x+z*z)<(t==0?9:19)||y>10)continue;n++;
      auto& b=add(P_SPHERE,"Bush "+std::to_string(n));float s=0.8f+hash(i,53)*0.8f;b.pos={x,y+0.35f*s,z};b.scale={1.4f*s,0.9f*s,1.4f*s};b.color=jit(0x3f6b3a,i+7,0.2f);}}
  if(t!=2){for(int i=0;i<5;i++){float a=i/5.0f*6.28f,x=std::cos(a)*7,z=std::sin(a)*7;auto& c=add(P_SPHERE,"Crystal "+std::to_string(i+1));c.pos={x,height(x,z)+1.2f,z};c.scale={0.8f,1.6f,0.8f};c.color=H(0x5fb8cc);c.script="Bobber";}}
  { auto& hero=add(P_MODEL,"Hero");hero.asset="hero";hero.color=Col{1,1,1,1};hero.pos={0,height(0,4),4};hero.script="PlayerController"; }
  if(t!=0){auto& hs=add(P_MODEL,"Horse");hs.asset="horse";hs.color=Col{1,1,1,1};hs.pos={4,height(4,2),2};hs.rot={0,-50,0};} }
}
