#include "model.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <functional>
#include <array>
#include <fstream>
#include <sstream>
#include <filesystem>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
namespace sk {
std::string g_assetDir="assets";

static Mat4 fromQ(const float* q){float x=q[0],y=q[1],z=q[2],w=q[3];Mat4 r=Mat4::identity();
  r.m[0]=1-2*(y*y+z*z);r.m[1]=2*(x*y+z*w);r.m[2]=2*(x*z-y*w);r.m[4]=2*(x*y-z*w);r.m[5]=1-2*(x*x+z*z);r.m[6]=2*(y*z+x*w);r.m[8]=2*(x*z+y*w);r.m[9]=2*(y*z-x*w);r.m[10]=1-2*(x*x+y*y);return r;}
static Mat4 toM(const TRS& k){Mat4 r=fromQ(k.r);for(int i=0;i<3;i++){r.m[i]*=k.s.x;r.m[4+i]*=k.s.y;r.m[8+i]*=k.s.z;}r.m[12]=k.t.x;r.m[13]=k.t.y;r.m[14]=k.t.z;return r;}
static void nlerp(const float* a,const float* b,float k,float* o){float d=a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3];float s=d<0?-1.f:1.f;float l=0;for(int i=0;i<4;i++){o[i]=a[i]*(1-k)+b[i]*s*k;l+=o[i]*o[i];}l=1/std::sqrt(std::max(l,1e-8f));for(int i=0;i<4;i++)o[i]*=l;}

void Model::sample(int ai,float t,std::vector<TRS>& out) const{out=rest;if(ai<0||ai>=(int)anims.size())return;const Anim& an=anims[ai];
  for(const auto& c:an.ch){if(c.t.empty())continue;int n=(int)c.t.size();int comp=c.path==1?4:3;TRS& k=out[c.node];
    int i=(int)(std::upper_bound(c.t.begin(),c.t.end(),t)-c.t.begin());float f=0;int i0,i1;
    if(i<=0){i0=i1=0;}else if(i>=n){i0=i1=n-1;}else{i0=i-1;i1=i;f=(t-c.t[i0])/std::max(1e-6f,c.t[i1]-c.t[i0]);if(c.step)f=0;}
    const float* a=&c.v[i0*comp];const float* b=&c.v[i1*comp];
    if(c.path==1){nlerp(a,b,f,k.r);}else{Vec3 v{a[0]+(b[0]-a[0])*f,a[1]+(b[1]-a[1])*f,a[2]+(b[2]-a[2])*f};if(c.path==0)k.t=v;else k.s=v;}}}

void ModelInst::play(const std::string& name,float fade){if(!m||name==cur)return;int i=m->animIndex(name);if(i<0)return;
  prev=a;pt=loco?phase:t;prevLoco=loco;a=i;t=0;phase=0;blend=prev<0?1.f:0.f;cur=name;loco=name=="Locomotion";(void)fade;
  int ci=i-m->builtin;if(ci>=0&&ci<(int)m->clips.size())loop=m->clips[ci].loop;else loop=!(name.rfind("die",0)==0||name=="mount"||name=="dismount"||name.rfind("attack",0)==0||name.rfind("jump",0)==0||name.rfind("hit",0)==0);}
void ModelInst::update(float dt,float speed){lastDt=dt;needStep=true;if(!m||a<0)return;if(a>=(int)m->anims.size()){a=-1;cur.clear();return;}if(prev>=(int)m->anims.size())prev=-1;float d=m->anims[a].dur;float t0=t;
  if(loco){phase+=dt*speed/std::max(0.2f,m->locoCycle(param));phase-=std::floor(phase);}
  else{t+=dt*speed;if(loop){if(d>0)t=std::fmod(t,d);}else t=std::min(t,d);
    int ci=a-m->builtin;if(ci>=0&&ci<(int)m->clips.size()&&!m->clips[ci].events.empty()){for(auto& ev:m->clips[ci].events){bool hit=t>=t0?(ev.first>t0&&ev.first<=t):(ev.first>t0||ev.first<=t);if(hit&&dt*speed>0)fired.push_back(ev.second);}}}
  if(blend<1){blend=std::min(1.f,blend+dt/0.2f);if(prev>=0&&!prevLoco){float pd=m->anims[prev].dur;pt+=dt*speed;if(pd>0)pt=std::fmod(pt,pd);}}}
void ModelInst::evaluate(){if(!m)return;if(a>=(int)m->anims.size()){a=-1;cur.clear();loco=false;}if(prev>=(int)m->anims.size())prev=-1;const Model& M=*m;std::vector<TRS> p;if(loco)M.sampleLoco(param,phase,p);else M.sample(a,t,p);
  if(blend<1&&prev>=0){std::vector<TRS> q;if(prevLoco)M.sampleLoco(param,pt,q);else M.sample(prev,pt,q);for(size_t i=0;i<p.size();i++){float k=blend;TRS& o=p[i];const TRS& s=q[i];
      o.t=s.t+(o.t-s.t)*k;o.s=s.s+(o.s-s.s)*k;float r[4];nlerp(s.r,o.r,k,r);for(int j=0;j<4;j++)o.r[j]=r[j];}}
  post(p);size_t n=p.size();std::vector<Mat4> w(n);std::vector<char> done(n,0);
  std::function<void(int)> go=[&](int i){if(done[i])return;int pa=M.parent[i];Mat4 l=toM(p[i]);if(pa>=0){go(pa);w[i]=w[pa]*l;}else w[i]=l;done[i]=1;};
  for(size_t i=0;i<n;i++)go((int)i);world=w;
  joints.resize(M.joints.size()*16);for(size_t j=0;j<M.joints.size();j++){Mat4 jm=w[M.joints[j]]*M.ibm[j];std::memcpy(&joints[j*16],jm.m,64);}}

static void addLoco(Model& m){if(m.animIndex("idle")>=0&&m.animIndex("walk")>=0&&m.animIndex("run")>=0&&m.animIndex("Locomotion")<0){Anim a;a.name="Locomotion";a.dur=1;m.anims.push_back(a);}}
static bool loadFbx(Renderer& r,const std::string& path,float targetHeight,Model& out);

// ---- auto-rig: static (unskinned) glTF gets a humanoid skeleton (hero naming) and distance-based skin weights
static float segDist(const float* p,const float* a,const float* b){float ab[3]={b[0]-a[0],b[1]-a[1],b[2]-a[2]},ap[3]={p[0]-a[0],p[1]-a[1],p[2]-a[2]};float l=ab[0]*ab[0]+ab[1]*ab[1]+ab[2]*ab[2];float t=l>1e-9f?(ap[0]*ab[0]+ap[1]*ab[1]+ap[2]*ab[2])/l:0;t=std::max(0.f,std::min(1.f,t));float d[3]={ap[0]-ab[0]*t,ap[1]-ab[1]*t,ap[2]-ab[2]*t};return std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);}
bool rigStatic(Renderer& r,std::vector<RigPrim>& ps,Model& out,float targetHeight);
static bool autoRigGltf(Renderer& r,cgltf_data* d,Model& out,float targetHeight){
  std::vector<RigPrim> ps;
  for(size_t ni=0;ni<d->nodes_count;ni++){cgltf_node* n=&d->nodes[ni];if(!n->mesh)continue;float wm[16];cgltf_node_transform_world(n,wm);
    for(size_t pi=0;pi<n->mesh->primitives_count;pi++){cgltf_primitive* pr=&n->mesh->primitives[pi];if(pr->type!=cgltf_primitive_type_triangles)continue;cgltf_accessor *ap=nullptr,*an=nullptr,*au=nullptr;
      for(size_t k=0;k<pr->attributes_count;k++){cgltf_attribute* at=&pr->attributes[k];if(at->type==cgltf_attribute_type_position)ap=at->data;else if(at->type==cgltf_attribute_type_normal)an=at->data;else if(at->type==cgltf_attribute_type_texcoord&&at->index==0)au=at->data;}
      if(!ap)continue;RigPrim p;p.v.resize(ap->count);p.mat=pr->material?(int)(pr->material-d->materials):0;
      for(size_t v=0;v<ap->count;v++){SkinVertex& s=p.v[v];float q[3],nn[3]={0,1,0};cgltf_accessor_read_float(ap,v,q,3);if(an)cgltf_accessor_read_float(an,v,nn,3);s.uv[0]=s.uv[1]=0;if(au)cgltf_accessor_read_float(au,v,s.uv,2);
        for(int k=0;k<3;k++){s.p[k]=wm[k]*q[0]+wm[4+k]*q[1]+wm[8+k]*q[2]+wm[12+k];s.n[k]=wm[k]*nn[0]+wm[4+k]*nn[1]+wm[8+k]*nn[2];}
        float l=std::sqrt(s.n[0]*s.n[0]+s.n[1]*s.n[1]+s.n[2]*s.n[2]);if(l>1e-6f)for(int k=0;k<3;k++)s.n[k]/=l;}
      if(pr->indices){p.idx.resize(pr->indices->count);for(size_t k=0;k<p.idx.size();k++)p.idx[k]=(uint32_t)cgltf_accessor_read_index(pr->indices,k);}else{p.idx.resize(ap->count);for(size_t k=0;k<p.idx.size();k++)p.idx[k]=(uint32_t)k;}
      ps.push_back(std::move(p));}}
  return rigStatic(r,ps,out,targetHeight);}
bool rigStatic(Renderer& r,std::vector<RigPrim>& ps,Model& out,float targetHeight){
  if(ps.empty())return false;float mn[3]={1e9f,1e9f,1e9f},mx[3]={-1e9f,-1e9f,-1e9f};for(auto& p:ps)for(auto& v:p.v)for(int k=0;k<3;k++){mn[k]=std::min(mn[k],v.p[k]);mx[k]=std::max(mx[k],v.p[k]);}
  float H=std::max(0.01f,mx[1]-mn[1]),W=mx[0]-mn[0],cx=(mx[0]+mn[0])*0.5f,cz=(mx[2]+mn[2])*0.5f,y0=mn[1];bool tpose=W/H>0.85f;
  struct J{const char* name;int par;float x,y,z;};   // x as +left fraction of H, y fraction of H, z fraction of H (forward)
  float ax1=tpose?0.30f:0.20f,ax2=tpose?std::min(0.47f,0.46f*W/H):0.27f,ay1=tpose?0.79f:0.61f,ay2=tpose?0.79f:0.46f;
  std::vector<J> js={{"root",-1,0,.52f,0},{"spine05",0,0,.54f,0},{"spine03",1,0,.64f,0},{"spine01",2,0,.74f,0},{"neck01",3,0,.82f,0},{"head",4,0,.87f,0},
    {"clavicle.L",3,.03f,.80f,0},{"clavicle.R",3,-.03f,.80f,0},{"upperarm01.L",6,.12f,.79f,0},{"upperarm01.R",7,-.12f,.79f,0},{"lowerarm01.L",8,ax1,ay1,0},{"lowerarm01.R",9,-ax1,ay1,0},{"wrist.L",10,ax2,ay2,.01f},{"wrist.R",11,-ax2,ay2,.01f},
    {"upperleg01.L",0,.06f,.50f,0},{"upperleg01.R",0,-.06f,.50f,0},{"lowerleg01.L",14,.065f,.27f,0},{"lowerleg01.R",15,-.065f,.27f,0},{"foot.L",16,.065f,.05f,0},{"foot.R",17,-.065f,.05f,0}};
  size_t nj=js.size();std::vector<std::array<float,3>> wp(nj);for(size_t i=0;i<nj;i++)wp[i]={cx+js[i].x*H,y0+js[i].y*H,cz+js[i].z*H};
  out.nodeName.resize(nj);out.parent.resize(nj);out.rest.resize(nj);out.joints.clear();out.ibm.clear();
  for(size_t i=0;i<nj;i++){out.nodeName[i]=js[i].name;out.parent[i]=js[i].par;TRS t;int pa=js[i].par;t.t={wp[i][0]-(pa>=0?wp[pa][0]:0),wp[i][1]-(pa>=0?wp[pa][1]:0),wp[i][2]-(pa>=0?wp[pa][2]:0)};out.rest[i]=t;out.joints.push_back((int)i);out.ibm.push_back(Mat4::translate({-wp[i][0],-wp[i][1],-wp[i][2]}));}
  // bone segments (joint index, from, to)
  struct Sg{int j;std::array<float,3> a,b;int side;};std::vector<Sg> sg;auto side=[&](int j){const std::string& n=out.nodeName[j];return n.size()>2&&n[n.size()-2]=='.'?(n.back()=='L'?1:-1):0;};
  auto ext=[&](int j,float dx,float dy,float dz){return std::array<float,3>{wp[j][0]+dx*H,wp[j][1]+dy*H,wp[j][2]+dz*H};};
  std::vector<std::pair<int,int>> link={{0,1},{1,2},{2,3},{3,4},{4,5},{6,8},{7,9},{8,10},{9,11},{10,12},{11,13},{14,16},{15,17},{16,18},{17,19}};
  for(auto& l:link)sg.push_back({l.first,wp[l.first],wp[l.second],side(l.first)});
  sg.push_back({5,wp[5],ext(5,0,.10f,0),0});float hx=(tpose?.06f:.04f);sg.push_back({12,wp[12],ext(12,hx,tpose?0:-.05f,0),1});sg.push_back({13,wp[13],ext(13,-hx,tpose?0:-.05f,0),-1});sg.push_back({18,wp[18],ext(18,0,-.04f,.10f),1});sg.push_back({19,wp[19],ext(19,0,-.04f,.10f),-1});
  float margin=0.035f*H;
  for(auto& p:ps){for(auto& v:p.v){float best[4]={0,0,0,0};int bj[4]={0,0,0,0};
    std::vector<std::pair<float,int>> cand;float dx=v.p[0]-cx;
    for(auto& g:sg){if(g.side!=0&&std::fabs(dx)>margin&&((dx>0)!=(g.side>0)))continue;float dd=segDist(v.p,g.a.data(),g.b.data());float w=1.f/((dd*dd+1e-5f)*(dd*dd+1e-5f));cand.push_back({w,g.j});}
    if(cand.empty())cand.push_back({1.f,0});std::sort(cand.begin(),cand.end(),[](const std::pair<float,int>&a,const std::pair<float,int>&b){return a.first>b.first;});
    // merge segments of the same joint
    std::vector<std::pair<float,int>> m;for(auto& c:cand){bool f=false;for(auto& e:m)if(e.second==c.second){e.first=std::max(e.first,c.first);f=true;break;}if(!f)m.push_back(c);}
    std::sort(m.begin(),m.end(),[](const std::pair<float,int>&a,const std::pair<float,int>&b){return a.first>b.first;});float sum=0;for(int k=0;k<4&&k<(int)m.size();k++){best[k]=m[k].first;bj[k]=m[k].second;sum+=best[k];}
    for(int k=0;k<4;k++){v.w[k]=sum>0?best[k]/sum:0;v.j[k]=(unsigned char)bj[k];}}
    ModelPrim mp;mp.mesh=r.uploadSkin(p.v,p.idx);mp.mat=p.mat;mp.cv=p.v;mp.ci=p.idx;out.prims.push_back(std::move(mp));}
  float rad=0;for(auto& p:ps)for(auto& v:p.v)rad=std::max(rad,std::sqrt((v.p[0]-cx)*(v.p[0]-cx)+(v.p[2]-cz)*(v.p[2]-cz)));
  out.fit=targetHeight/H;out.foot=y0;out.height=targetHeight;out.radius=std::max(0.5f,rad*out.fit);out.autoRig=true;
  Anim lo;lo.name="Locomotion";lo.dur=1;out.anims.push_back(lo);out.builtin=(int)out.anims.size();return true;}
bool loadModel(Renderer& r,const std::string& name,float targetHeight,Model& out){
  if(name.size()>4){std::string x=name.substr(name.size()-4);for(char& ch:x)ch=(char)tolower(ch);if(x==".fbx")return loadFbx(r,name,targetHeight,out);}
  std::string path=name;size_t sl=path.find_last_of("/\\");std::string base=sl==std::string::npos?".":path.substr(0,sl);cgltf_options opt{};cgltf_data* d=nullptr;
  if(cgltf_parse_file(&opt,path.c_str(),&d)!=cgltf_result_success)return false;
  if(cgltf_load_buffers(&opt,d,path.c_str())!=cgltf_result_success){cgltf_free(d);return false;}
  out=Model();out.name=name;out.nodeName.resize(d->nodes_count);for(size_t i=0;i<d->nodes_count;i++)out.nodeName[i]=d->nodes[i].name?d->nodes[i].name:"";
  // nodes
  size_t nn=d->nodes_count;out.parent.assign(nn,-1);out.rest.resize(nn);
  for(size_t i=0;i<nn;i++){cgltf_node* n=&d->nodes[i];if(n->parent)out.parent[i]=(int)(n->parent-d->nodes);TRS& k=out.rest[i];
    if(n->has_translation)k.t={n->translation[0],n->translation[1],n->translation[2]};if(n->has_rotation)for(int j=0;j<4;j++)k.r[j]=n->rotation[j];if(n->has_scale)k.s={n->scale[0],n->scale[1],n->scale[2]};}
  // skin
  if(d->skins_count>0){cgltf_skin* sk=&d->skins[0];for(size_t j=0;j<sk->joints_count;j++){out.joints.push_back((int)(sk->joints[j]-d->nodes));Mat4 m=Mat4::identity();if(sk->inverse_bind_matrices)cgltf_accessor_read_float(sk->inverse_bind_matrices,j,m.m,16);out.ibm.push_back(m);}}
  bool rig=d->skins_count==0;if(!rig&&(out.joints.empty()||out.joints.size()>80)){cgltf_free(d);return false;}
  // materials + textures
  std::map<std::string,unsigned> cache;std::vector<unsigned char> white{255,255,255,255};unsigned wt=r.createTexture(white.data(),1,1);std::map<unsigned,std::pair<std::vector<unsigned char>,std::string>> rawImg;
  auto loadImg=[&](cgltf_texture* t)->unsigned{if(!t||!t->image)return wt;std::string key;int w=0,h=0,c=0;unsigned char* px=nullptr;
    if(t->image->uri&&std::string(t->image->uri).rfind("data:",0)!=0){key=t->image->uri;auto it=cache.find(key);if(it!=cache.end())return it->second;px=stbi_load((base+"/"+key).c_str(),&w,&h,&c,4);if(px){std::ifstream ff(base+"/"+key,std::ios::binary);std::vector<unsigned char> by((std::istreambuf_iterator<char>(ff)),std::istreambuf_iterator<char>());size_t dot=key.rfind('.');std::string ex=dot==std::string::npos?"":key.substr(dot+1);rawImg[0xFFFFFFFFu]={by,(ex=="jpg"||ex=="jpeg")?"image/jpeg":"image/png"};}}
    else if(t->image->buffer_view){const unsigned char* b=(const unsigned char*)t->image->buffer_view->buffer->data+t->image->buffer_view->offset;px=stbi_load_from_memory(b,(int)t->image->buffer_view->size,&w,&h,&c,4);key=std::to_string((size_t)t->image);}
    if(!px)return wt;unsigned id=r.createTexture(px,w,h);stbi_image_free(px);cache[key]=id;{auto ri=rawImg.find(0xFFFFFFFFu);if(ri!=rawImg.end()){rawImg[id]=ri->second;rawImg.erase(ri);}else if(t->image->buffer_view){const unsigned char* b=(const unsigned char*)t->image->buffer_view->buffer->data+t->image->buffer_view->offset;std::string mt=t->image->mime_type?t->image->mime_type:"image/png";rawImg[id]={std::vector<unsigned char>(b,b+t->image->buffer_view->size),mt};}}return id;};
  for(size_t i=0;i<d->materials_count;i++){cgltf_material* m=&d->materials[i];ModelMat mm;if(m->has_pbr_metallic_roughness){for(int j=0;j<4;j++)mm.col[j]=m->pbr_metallic_roughness.base_color_factor[j];mm.tex=loadImg(m->pbr_metallic_roughness.base_color_texture.texture);{auto ri=rawImg.find(mm.tex);if(ri!=rawImg.end()){mm.img=ri->second.first;mm.mime=ri->second.second;}}}else mm.tex=wt;
    mm.mask=m->alpha_mode!=cgltf_alpha_mode_opaque;out.mats.push_back(mm);}
  if(out.mats.empty()){ModelMat mm;mm.tex=wt;out.mats.push_back(mm);}
  if(rig){bool ok=autoRigGltf(r,d,out,targetHeight);cgltf_free(d);return ok;}
  // meshes (every skinned primitive uses skin 0 / joint indices directly)
  float mn=1e9f,mx=-1e9f,rad=0;
  for(size_t ni=0;ni<nn;ni++){cgltf_node* n=&d->nodes[ni];if(!n->mesh)continue;
    for(size_t pi=0;pi<n->mesh->primitives_count;pi++){cgltf_primitive* pr=&n->mesh->primitives[pi];if(pr->type!=cgltf_primitive_type_triangles)continue;
      cgltf_accessor *ap=nullptr,*an=nullptr,*au=nullptr,*aj=nullptr,*aw=nullptr;
      for(size_t k=0;k<pr->attributes_count;k++){cgltf_attribute* at=&pr->attributes[k];if(at->type==cgltf_attribute_type_position)ap=at->data;else if(at->type==cgltf_attribute_type_normal)an=at->data;
        else if(at->type==cgltf_attribute_type_texcoord&&at->index==0)au=at->data;else if(at->type==cgltf_attribute_type_joints&&at->index==0)aj=at->data;else if(at->type==cgltf_attribute_type_weights&&at->index==0)aw=at->data;}
      if(!ap||!aj||!aw)continue;size_t vc=ap->count;std::vector<SkinVertex> vs(vc);
      for(size_t v=0;v<vc;v++){SkinVertex& s=vs[v];cgltf_accessor_read_float(ap,v,s.p,3);if(an)cgltf_accessor_read_float(an,v,s.n,3);else{s.n[0]=0;s.n[1]=1;s.n[2]=0;}if(au)cgltf_accessor_read_float(au,v,s.uv,2);
        cgltf_uint j4[4]={0,0,0,0};cgltf_accessor_read_uint(aj,v,j4,4);for(int k=0;k<4;k++)s.j[k]=(unsigned char)std::min<cgltf_uint>(j4[k],79);cgltf_accessor_read_float(aw,v,s.w,4);
        float sw=s.w[0]+s.w[1]+s.w[2]+s.w[3];if(sw>1e-6f)for(int k=0;k<4;k++)s.w[k]/=sw;else{s.w[0]=1;}
        mn=std::min(mn,s.p[1]);mx=std::max(mx,s.p[1]);rad=std::max(rad,std::sqrt(s.p[0]*s.p[0]+s.p[2]*s.p[2]));}
      std::vector<uint32_t> idx;if(pr->indices){idx.resize(pr->indices->count);for(size_t k=0;k<idx.size();k++)idx[k]=(uint32_t)cgltf_accessor_read_index(pr->indices,k);}else{idx.resize(vc);for(size_t k=0;k<vc;k++)idx[k]=(uint32_t)k;}
      ModelPrim mp;mp.mesh=r.uploadSkin(vs,idx);mp.mat=pr->material?(int)(pr->material-d->materials):0;mp.cv=vs;mp.ci=idx;
      if(pr->targets_count>0){size_t nt=std::min<size_t>(pr->targets_count,32);if(out.morphNames.size()<nt){size_t o=out.morphNames.size();out.morphNames.resize(nt);for(size_t k=o;k<nt;k++)out.morphNames[k]=(n->mesh->target_names&&k<n->mesh->target_names_count&&n->mesh->target_names[k])?n->mesh->target_names[k]:"morph"+std::to_string(k);}
        mp.md.resize(nt);for(size_t k=0;k<nt;k++){cgltf_accessor* tp=nullptr;for(size_t q=0;q<pr->targets[k].attributes_count;q++)if(pr->targets[k].attributes[q].type==cgltf_attribute_type_position)tp=pr->targets[k].attributes[q].data;
          if(!tp||tp->count!=vc)continue;mp.md[k].resize(vc*3);for(size_t v=0;v<vc;v++)cgltf_accessor_read_float(tp,v,&mp.md[k][v*3],3);}}
      out.prims.push_back(std::move(mp));}}
  // animations
  for(size_t ai=0;ai<d->animations_count;ai++){cgltf_animation* a=&d->animations[ai];Anim an;an.name=a->name?a->name:"anim"+std::to_string(ai);
    for(size_t ci=0;ci<a->channels_count;ci++){cgltf_animation_channel* c=&a->channels[ci];if(!c->target_node||!c->sampler)continue;AnimChannel ch;ch.node=(int)(c->target_node-d->nodes);
      if(c->target_path==cgltf_animation_path_type_translation)ch.path=0;else if(c->target_path==cgltf_animation_path_type_rotation)ch.path=1;else if(c->target_path==cgltf_animation_path_type_scale)ch.path=2;else continue;
      ch.step=c->sampler->interpolation==cgltf_interpolation_type_step;bool cubic=c->sampler->interpolation==cgltf_interpolation_type_cubic_spline;int comp=ch.path==1?4:3;
      size_t n=c->sampler->input->count;ch.t.resize(n);ch.v.resize(n*comp);for(size_t k=0;k<n;k++){cgltf_accessor_read_float(c->sampler->input,k,&ch.t[k],1);cgltf_accessor_read_float(c->sampler->output,cubic?k*3+1:k,&ch.v[k*comp],comp);}
      if(n)an.dur=std::max(an.dur,ch.t.back());an.ch.push_back(std::move(ch));}
    out.anims.push_back(std::move(an));}
  float raw=std::max(0.01f,mx-mn);out.fit=targetHeight/raw;out.foot=mn;out.height=targetHeight;out.radius=std::max(0.5f,rad*out.fit);addLoco(out);out.builtin=(int)out.anims.size();
  cgltf_free(d);return !out.prims.empty();}
}

#include "ufbx.h"
namespace sk {
static void qmul(const float* a,const float* b,float* o){o[0]=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];o[1]=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];o[2]=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];o[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];}
static void qeuler(Vec3 d,float* q){const float D=0.0087266463f;float sx=std::sin(d.x*D),cx=std::cos(d.x*D),sy=std::sin(d.y*D),cy=std::cos(d.y*D),sz=std::sin(d.z*D),cz=std::cos(d.z*D);
  float qx[4]={sx,0,0,cx},qy[4]={0,sy,0,cy},qz[4]={0,0,sz,cz},t[4];qmul(qz,qy,t);qmul(t,qx,q);}
static float bezEase(const float* b,float x){float lo=0,hi=1,t=x;for(int i=0;i<24;i++){float u=1-t;float bx=3*u*u*t*b[0]+3*u*t*t*b[2]+t*t*t;if(bx<x)lo=t;else hi=t;t=(lo+hi)*0.5f;}float u=1-t;return 3*u*u*t*b[1]+3*u*t*t*b[3]+t*t*t;}
float easeBz(const float* b,float x){return bezEase(b,x);}
float easeF(int e,float f){switch(e){case 0:return f;case 2:return f*f;case 3:return 1-(1-f)*(1-f);case 4:return 0;default:return f*f*(3-2*f);}}
static void keyAt(const std::vector<ClipKey>& ks,float t,Vec3& rot,Vec3& pos);
void clipEval(const std::vector<ClipKey>& ks,float t,Vec3& rot,Vec3& pos){keyAt(ks,t,rot,pos);}
static void keyAt(const std::vector<ClipKey>& ks,float t,Vec3& rot,Vec3& pos){rot={};pos={};if(ks.empty())return;
  if(t<=ks.front().t){rot=ks.front().rot;pos=ks.front().pos;return;}if(t>=ks.back().t){rot=ks.back().rot;pos=ks.back().pos;return;}
  for(size_t i=1;i<ks.size();i++)if(t<=ks[i].t){float f=(t-ks[i-1].t)/std::max(1e-5f,ks[i].t-ks[i-1].t);f=ks[i-1].ease==5?bezEase(ks[i-1].bz,f):easeF(ks[i-1].ease,f);rot=ks[i-1].rot+(ks[i].rot-ks[i-1].rot)*f;pos=ks[i-1].pos+(ks[i].pos-ks[i-1].pos)*f;return;}}
static std::string roleOf(std::string n);
static void qnorm(float* q){float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);if(l<1e-8f){q[0]=q[1]=q[2]=0;q[3]=1;return;}for(int i=0;i<4;i++)q[i]/=l;}
static void qarc(Vec3 u,Vec3 v,float* q){float lu=std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z),lv=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);if(lu<1e-8f||lv<1e-8f){q[0]=q[1]=q[2]=0;q[3]=1;return;}u=u*(1/lu);v=v*(1/lv);
  float d=u.x*v.x+u.y*v.y+u.z*v.z;if(d<-0.99999f){Vec3 ax=std::fabs(u.x)<0.9f?Vec3{1,0,0}:Vec3{0,1,0};Vec3 c{u.y*ax.z-u.z*ax.y,u.z*ax.x-u.x*ax.z,u.x*ax.y-u.y*ax.x};float l=std::sqrt(c.x*c.x+c.y*c.y+c.z*c.z);q[0]=c.x/l;q[1]=c.y/l;q[2]=c.z/l;q[3]=0;return;}
  q[0]=u.y*v.z-u.z*v.y;q[1]=u.z*v.x-u.x*v.z;q[2]=u.x*v.y-u.y*v.x;q[3]=1+d;qnorm(q);}
static void worldPose(const Model& m,const std::vector<TRS>& p,std::vector<Mat4>& w,std::vector<std::array<float,4>>& wq){size_t n=p.size();w.assign(n,Mat4::identity());wq.assign(n,{0,0,0,1});std::vector<char> dn(n,0);
  std::function<void(int)> go=[&](int i){if(dn[i])return;int pa=m.parent[i];Mat4 l=toM(p[i]);std::array<float,4> lq{p[i].r[0],p[i].r[1],p[i].r[2],p[i].r[3]};if(pa>=0){go(pa);w[i]=w[pa]*l;qmul(wq[pa].data(),lq.data(),wq[i].data());}else{w[i]=l;wq[i]=lq;}dn[i]=1;};for(size_t i=0;i<n;i++)go((int)i);}
static Vec3 posOf(const Mat4& m){return {m.m[12],m.m[13],m.m[14]};}
void ik2(const Model& m,std::vector<TRS>& p,int end,Vec3 T){int b=m.parent[end];if(b<0)return;int a=m.parent[b];if(a<0)return;
  std::vector<Mat4> w;std::vector<std::array<float,4>> wq;worldPose(m,p,w,wq);Vec3 A=posOf(w[a]),B=posOf(w[b]),C=posOf(w[end]);
  auto len=[](Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);};float l1=len(B-A),l2=len(C-B);if(l1<1e-5f||l2<1e-5f)return;Vec3 dv=T-A;float d=len(dv);if(d<1e-6f)return;
  d=std::min(std::max(d,std::fabs(l1-l2)+1e-4f),l1+l2-1e-4f);Vec3 dir=dv*(1/len(dv));Vec3 Tc=A+dir*d;float along=(l1*l1-l2*l2+d*d)/(2*d),hh=std::sqrt(std::max(0.f,l1*l1-along*along));
  Vec3 ab=B-A;float dp=ab.x*dir.x+ab.y*dir.y+ab.z*dir.z;Vec3 perp=ab-dir*dp;float pl=len(perp);if(pl<1e-5f){perp=Vec3{dir.y*0-dir.z*0,0,1};if(std::fabs(dir.z)>0.9f)perp=Vec3{1,0,0};pl=len(perp);}perp=perp*(1/pl);
  Vec3 B2=A+dir*along+perp*hh;
  auto applyRot=[&](int bone,const float* qw){int pa=m.parent[bone];float pq[4]={0,0,0,1};if(pa>=0)for(int i=0;i<4;i++)pq[i]=wq[pa][i];float nw[4],ip[4]={-pq[0],-pq[1],-pq[2],pq[3]},loc[4];qmul(qw,wq[bone].data(),nw);qmul(ip,nw,loc);qnorm(loc);for(int i=0;i<4;i++)p[bone].r[i]=loc[i];};
  float q1[4];qarc(B-A,B2-A,q1);applyRot(a,q1);worldPose(m,p,w,wq);Vec3 C1=posOf(w[end]),B1=posOf(w[b]);float q2[4];qarc(C1-B1,Tc-B1,q2);applyRot(b,q2);}
// ---- post-pose stage: gait boost, breathing, secondary motion, look-at, foot planting
static void rotAdd(TRS& t,float rx,float ry,float rz){float q[4];qeuler({rx,ry,rz},q);float o[4];qmul(t.r,q,o);qnorm(o);for(int i=0;i<4;i++)t.r[i]=o[i];}
static void amplifyRot(TRS& t,const TRS& rest,float k){float o[4];nlerp(rest.r,t.r,k,o);for(int i=0;i<4;i++)t.r[i]=o[i];}
void ModelInst::post(std::vector<TRS>& p){if(!m||p.empty()||fxOff)return;const Model& M=*m;float dt=needStep?std::min(lastDt,0.05f):0;needStep=false;clock+=dt;size_t n=p.size();
  auto find=[&](const char* nm){for(size_t i=0;i<n;i++)if(M.nodeName[i]==nm)return (int)i;return -1;};
  float cyc=loco?phase:(a>=0&&a<(int)M.anims.size()&&M.anims[a].dur>0?t/M.anims[a].dur:0);
  float g=0;if(loco)g=std::min(1.f,param/6.f);else if(cur=="run")g=1;else if(cur=="walk")g=0.5f;
  if(M.fx.gait&&g>0.01f){int hu[4]={find("hl_u"),find("hr_u"),find("fl_u"),find("fr_u")};bool quad=hu[0]>=0&&hu[1]>=0;
    if(quad){for(int k=0;k<4;k++)if(hu[k]>=0)amplifyRot(p[hu[k]],M.rest[hu[k]],1+0.3f*g);int hk[2]={find("hl_l"),find("hr_l")};for(int k=0;k<2;k++)if(hk[k]>=0)amplifyRot(p[hk[k]],M.rest[hk[k]],1+0.15f*g);
      int sp3[2]={0,find("chest")};for(int k=0;k<2;k++)if(sp3[k]>=0)amplifyRot(p[sp3[k]],M.rest[sp3[k]],1+0.7f*g);
      int nk=find("neck");if(nk>=0)rotAdd(p[nk],5.f*g*std::sin(cyc*12.5664f+1.0f),0,0);int rt=0;p[rt].t.y+=0.035f*g*std::sin(cyc*12.5664f+0.5f);}}
  if(M.fx.breath){float b=std::sin(clock*1.9f)*0.9f*(1-std::min(1.f,g));for(size_t i=0;i<n;i++){const std::string& nm=M.nodeName[i];if(nm=="chest"||M.boneRole((int)i)=="spine2")rotAdd(p[i],b,0,0);}}
  if(M.fx.secondary){for(size_t i=0;i<n;i++){const std::string& nm=M.nodeName[i];bool sec=false;for(const char* k:{"tail","hair","mane","cape","cloak","skirt","scarf","cloth","pony"})if(nm.find(k)!=std::string::npos){sec=true;break;}if(!sec)continue;
      int pa=M.parent[i];if(pa<0)continue;Vec3 d;quatToEuler(p[pa].r,d);Vec3 d0;quatToEuler(M.rest[pa].r,d0);d=d-d0;auto& s=sp[(int)i];
      float dd[3]={d.x,d.y,d.z};float lag[3];for(int k=0;k<3;k++){if(dt>0){s[k*2+1]+=(70.f*(dd[k]-s[k*2])-9.f*s[k*2+1])*dt;s[k*2]+=s[k*2+1]*dt;}lag[k]=(s[k*2]-dd[k])*1.2f;}
      float wag=std::sin(clock*(2.2f+g*5.f))*(3.f+g*6.f);rotAdd(p[i],lag[0],lag[1]+wag,lag[2]);}}
  if(lookOn||std::fabs(lcY)>0.05f||std::fabs(lcP)>0.05f){float ty=lookOn?lookYaw:0,tp=lookOn?lookPitch:0;ty=std::max(-70.f,std::min(70.f,ty));tp=std::max(-35.f,std::min(35.f,tp));float k=std::min(1.f,dt*8.f);lcY+=(ty-lcY)*k;lcP+=(tp-lcP)*k;
    int nk=-1,hd=-1;for(size_t i=0;i<n;i++){std::string r=M.boneRole((int)i);if(r=="neck"&&nk<0)nk=(int)i;if(r=="head"&&hd<0)hd=(int)i;}
    if(nk>=0)rotAdd(p[nk],-lcP*0.4f,lcY*0.4f,0);if(hd>=0)rotAdd(p[hd],-lcP*0.6f,lcY*0.6f,0);}
  if(M.fx.foot){if(!M.footInit){M.footInit=true;for(size_t i=0;i<n;i++){std::string r=M.boneRole((int)i);const std::string& nm=M.nodeName[i];if(r=="footL"||r=="footR"||(nm.size()>2&&nm.compare(nm.size()-2,2,"_h")==0))M.footNodes.push_back((int)i);}
      std::vector<Mat4> w;std::vector<std::array<float,4>> wq;worldPose(M,M.rest,w,wq);for(int f:M.footNodes)M.footRestY.push_back(w[f].m[13]);}
    if(!M.footNodes.empty()){std::vector<Mat4> w;std::vector<std::array<float,4>> wq;worldPose(M,p,w,wq);for(size_t k=0;k<M.footNodes.size();k++){int f=M.footNodes[k];float y=w[f].m[13],ry=M.footRestY[k];if(y<ry-0.02f){Vec3 T{w[f].m[12],ry,w[f].m[14]};ik2(M,p,f,T);worldPose(M,p,w,wq);}}}}}

void ModelInst::smTick(float speed){if(!m||m->smStates.empty())return;const Model& M=*m;int ns=(int)M.smStates.size();if(smState<0||smState>=ns){smState=0;play(M.smStates[0].clip);}
  bool fin=false;if(a>=0&&a<(int)M.anims.size()&&!loop&&M.anims[a].dur>0&&t>=M.anims[a].dur-0.03f)fin=true;
  for(auto& tr:M.smTrans){if(tr.from!=-1&&tr.from!=smState)continue;if(tr.to<0||tr.to>=ns||tr.to==smState)continue;bool ok=false;
    if(tr.kind==0)ok=speed>tr.val;else if(tr.kind==1)ok=speed<tr.val;else if(tr.kind==2)ok=!trigger.empty()&&trigger==tr.trig;else if(tr.kind==3)ok=fin;
    if(ok){smState=tr.to;if(tr.kind==2)trigger.clear();cur.clear();play(M.smStates[tr.to].clip);return;}}
  if(!trigger.empty()&&false)trigger.clear();}

std::string Model::boneRole(int n) const{return n>=0&&n<(int)nodeName.size()?roleOf(nodeName[n]):"";}
int Model::mirrorNode(int n) const{std::string r=boneRole(n);if(r.empty())return n;char s=r.back();if(s!='L'&&s!='R')return n;r.back()=s=='L'?'R':'L';for(size_t i=0;i<nodeName.size();i++)if(boneRole((int)i)==r)return (int)i;return n;}
bool Model::isUpper(int n) const{for(int i=n;i>=0;i=parent[i])if(boneRole(i)=="spine0")return true;return false;}
void quatToEuler(const float* q,Vec3& d){float x=q[0],y=q[1],z=q[2],w=q[3];float r20=2*(x*z-y*w),r21=2*(y*z+x*w),r22=1-2*(x*x+y*y),r10=2*(x*y+z*w),r00=1-2*(y*y+z*z);const float R=57.2957795f;d.y=-std::asin(std::max(-1.f,std::min(1.f,r20)))*R;d.x=std::atan2(r21,r22)*R;d.z=std::atan2(r10,r00)*R;}
void Model::basePose(int ci,float t,std::vector<TRS>& p) const{p=rest;if(ci<0||ci>=(int)clips.size())return;const ClipDef& d=clips[ci];int self=builtin+ci;int bi=animIndex(d.base);if(bi==self)bi=-1;int oi=animIndex(d.overlay);if(oi==self)oi=-1;size_t nn=rest.size();
  if(bi>=0){float bd=anims[bi].dur;sample(bi,bd>0?std::fmod(t,bd):0,p);}
  if(oi>=0){std::vector<TRS> o;float od=anims[oi].dur;sample(oi,od>0?std::fmod(t,od):0,o);float wgt=clampf(d.weight,0,1);int hips=-1;for(size_t i=0;i<nn;i++)if(boneRole((int)i)=="hips")hips=(int)i;
    for(size_t i=0;i<nn;i++){bool up=isUpper((int)i);bool in=d.mask==2||(d.mask==0&&up)||(d.mask==1&&!up);if(!in)continue;float r[4];nlerp(p[i].r,o[i].r,wgt,r);for(int j=0;j<4;j++)p[i].r[j]=r[j];if(d.mask==2||(i==(size_t)hips&&d.mask==1))p[i].t=p[i].t+(o[i].t-p[i].t)*wgt;}}}
void Model::bakeClip(int ci){if(ci<0||ci>=(int)clips.size())return;anims.resize(builtin+clips.size());ClipDef& d=clips[ci];Anim& an=anims[builtin+ci];
  if(d.raw){if(ci<(int)rawData.size())an=rawData[ci];an.name=d.name;return;}
  an=Anim();an.name=d.name;an.dur=std::max(0.2f,d.dur);int self=builtin+ci;int bi=animIndex(d.base);if(bi==self)bi=-1;int oi=animIndex(d.overlay);if(oi==self)oi=-1;
  int fr=(int)std::ceil(an.dur*30)+1;size_t nn=rest.size();std::vector<AnimChannel> ct(nn),cr(nn);
  for(size_t i=0;i<nn;i++){ct[i].node=cr[i].node=(int)i;ct[i].path=0;cr[i].path=1;}
  std::vector<char> up(nn,0);for(size_t i=0;i<nn;i++)up[i]=isUpper((int)i)?1:0;int hips=-1;for(size_t i=0;i<nn;i++)if(boneRole((int)i)=="hips")hips=(int)i;
  std::vector<TRS> p,o;for(int k=0;k<fr;k++){float t=std::min(an.dur,k/30.0f);if(bi>=0){float bd=anims[bi].dur;sample(bi,bd>0?std::fmod(t,bd):0,p);}else p=rest;
    if(oi>=0){float od=anims[oi].dur;sample(oi,od>0?std::fmod(t,od):0,o);float wgt=clampf(d.weight,0,1);for(size_t i=0;i<nn;i++){bool in=d.mask==2||(d.mask==0&&up[i])||(d.mask==1&&!up[i]);if(!in)continue;float r[4];nlerp(p[i].r,o[i].r,wgt,r);for(int j=0;j<4;j++)p[i].r[j]=r[j];if(d.mask==2||(i==(size_t)hips&&d.mask==1))p[i].t=p[i].t+(o[i].t-p[i].t)*wgt;}}
    for(auto& kv:d.keys){if(kv.first<0||kv.first>=(int)nn)continue;Vec3 rot,pos;keyAt(kv.second,t,rot,pos);TRS& s=p[kv.first];float q[4],r[4];qeuler(rot,q);qmul(s.r,q,r);for(int j=0;j<4;j++)s.r[j]=r[j];s.t=s.t+pos*0.01f;}
    for(auto& pin:d.pins)if(pin.first>=0&&pin.first<(int)nn)ik2(*this,p,pin.first,pin.second);
    if(d.inPlace&&hips>=0){p[hips].t.x=rest[hips].t.x;p[hips].t.z=rest[hips].t.z;}
    for(size_t i=0;i<nn;i++){ct[i].t.push_back(t);cr[i].t.push_back(t);ct[i].v.insert(ct[i].v.end(),{p[i].t.x,p[i].t.y,p[i].t.z});cr[i].v.insert(cr[i].v.end(),{p[i].r[0],p[i].r[1],p[i].r[2],p[i].r[3]});}}
  for(size_t i=0;i<nn;i++){an.ch.push_back(std::move(ct[i]));an.ch.push_back(std::move(cr[i]));}}
void Model::sampleLoco(float param,float phase,std::vector<TRS>& out) const{int ii=animIndex("idle"),wi=animIndex("walk"),ri=animIndex("run");if(ii<0||wi<0||ri<0){out=rest;return;}
  int A,B;float w;if(param<=2.4f){A=ii;B=wi;w=clampf(param/2.4f,0,1);}else{A=wi;B=ri;w=clampf((param-2.4f)/3.6f,0,1);}
  std::vector<TRS> p,q;sample(A,phase*anims[A].dur,p);sample(B,phase*anims[B].dur,q);out=p;for(size_t i=0;i<p.size();i++){out[i].t=p[i].t+(q[i].t-p[i].t)*w;out[i].s=p[i].s+(q[i].s-p[i].s)*w;float r[4];nlerp(p[i].r,q[i].r,w,r);for(int j=0;j<4;j++)out[i].r[j]=r[j];}}
float Model::locoCycle(float param) const{int ii=animIndex("idle"),wi=animIndex("walk"),ri=animIndex("run");if(ii<0||wi<0||ri<0)return 1;float di=anims[ii].dur,dw=anims[wi].dur,dr=anims[ri].dur;if(param<=2.4f)return di+(dw-di)*clampf(param/2.4f,0,1);return dw+(dr-dw)*clampf((param-2.4f)/3.6f,0,1);}
std::string Model::uniqueClipName(const std::string& n) const{std::string r=n;int k=2;while(animIndex(r)>=0)r=n+" "+std::to_string(k++);return r;}
int Model::addClip(const std::string& name,const std::string& base){ClipDef d;d.name=uniqueClipName(name);d.base=base;clips.push_back(d);rawData.resize(clips.size());bakeClip((int)clips.size()-1);return (int)clips.size()-1;}
void Model::removeClip(int ci){if(ci<0||ci>=(int)clips.size())return;clips.erase(clips.begin()+ci);if(ci<(int)rawData.size())rawData.erase(rawData.begin()+ci);anims.resize(builtin);for(size_t i=0;i<clips.size();i++)bakeClip((int)i);}
void Model::clearClips(){clips.clear();rawData.clear();anims.resize(builtin);}
static std::string safeId(std::string s){for(char& c:s)if(c=='/'||c==':'||c=='\\'||c==' ')c='_';return s;}
std::string clipFilePath(const std::string& dir,const std::string& id,const std::string& name){return dir+"/"+safeId(id)+"__"+safeId(name)+".anim";}
bool Model::saveSM(const std::string& dir,const std::string& id) const{if(smStates.empty())return true;std::error_code ec;std::filesystem::create_directories(dir,ec);std::ofstream f(dir+"/"+safeId(id)+"__machine.sm");if(!f)return false;
  f<<"SABAKSM 1\n";for(auto& s:smStates)f<<"state|"<<s.name<<'|'<<s.clip<<'\n';for(auto& t:smTrans)f<<"trans|"<<t.from<<'|'<<t.to<<'|'<<t.kind<<'|'<<t.val<<'|'<<t.trig<<'\n';return true;}
void Model::loadSM(const std::string& dir,const std::string& id){std::ifstream f(dir+"/"+safeId(id)+"__machine.sm");if(!f)return;std::string line;std::getline(f,line);if(line.rfind("SABAKSM",0)!=0)return;smStates.clear();smTrans.clear();
  while(std::getline(f,line)){std::vector<std::string> q;std::string cur;for(char ch:line){if(ch=='|'){q.push_back(cur);cur.clear();}else cur+=ch;}q.push_back(cur);
    if(q[0]=="state"&&q.size()>=3)smStates.push_back({q[1],q[2]});else if(q[0]=="trans"&&q.size()>=5){SMTrans t;t.from=atoi(q[1].c_str());t.to=atoi(q[2].c_str());t.kind=atoi(q[3].c_str());t.val=(float)atof(q[4].c_str());if(q.size()>5)t.trig=q[5];smTrans.push_back(t);}}}
bool Model::saveClips(const std::string& dir,const std::string& id) const{std::error_code ec;std::filesystem::create_directories(dir,ec);bool ok=true;
  for(size_t i=0;i<clips.size();i++){const ClipDef& d=clips[i];if(d.lib)continue;std::string fn=dir+"/"+safeId(id)+"__"+safeId(d.name)+".anim";std::ofstream f(fn);if(!f){ok=false;continue;}
    f<<"SABAKANIM 1\n"<<d.name<<'|'<<d.base<<'|'<<d.dur<<'|'<<(d.raw?1:0)<<'|'<<(d.loop?1:0)<<'|'<<(d.inPlace?1:0)<<'|'<<d.overlay<<'|'<<d.mask<<'|'<<d.weight<<'\n';
    for(auto& ev:d.events)f<<"event|"<<ev.first<<'|'<<ev.second<<'\n';for(auto& pn:d.pins)f<<"pin|"<<nodeName[pn.first]<<'|'<<pn.second.x<<'|'<<pn.second.y<<'|'<<pn.second.z<<'\n';
    if(!d.raw){for(auto& kv:d.keys)for(auto& k:kv.second)f<<"key|"<<nodeName[kv.first]<<'|'<<k.t<<'|'<<k.rot.x<<'|'<<k.rot.y<<'|'<<k.rot.z<<'|'<<k.pos.x<<'|'<<k.pos.y<<'|'<<k.pos.z<<'|'<<k.ease<<'\n';}
    else if(i<rawData.size()){for(auto& c:rawData[i].ch){f<<"ch|"<<nodeName[c.node]<<'|'<<c.path<<'|'<<c.t.size()<<'\n';for(float t:c.t)f<<t<<' ';f<<'\n';for(float v:c.v)f<<v<<' ';f<<'\n';}}}
  return ok;}
void Model::loadClips(const std::string& dir,const std::string& id){std::error_code ec;if(!std::filesystem::is_directory(dir,ec))return;std::string pre=safeId(id)+"__";std::vector<std::string> files;
  for(auto& e:std::filesystem::directory_iterator(dir,ec)){std::string n=e.path().filename().string();if(n.rfind(pre,0)==0&&e.path().extension()==".anim")files.push_back(e.path().string());}
  std::sort(files.begin(),files.end());std::map<std::string,int> byName;for(size_t i=0;i<nodeName.size();i++)if(!byName.count(nodeName[i]))byName[nodeName[i]]=(int)i;
  for(auto& fn:files){std::ifstream f(fn);std::string line;std::getline(f,line);if(line.rfind("SABAKANIM",0)!=0)continue;std::getline(f,line);std::vector<std::string> p;{std::string cur;for(char ch:line){if(ch=='|'){p.push_back(cur);cur.clear();}else cur+=ch;}p.push_back(cur);}if(p.size()<4)continue;
    ClipDef d;d.name=p[0];d.base=p[1];d.dur=(float)atof(p[2].c_str());d.raw=p[3]=="1";if(p.size()>=9){d.loop=p[4]!="0";d.inPlace=p[5]=="1";d.overlay=p[6];d.mask=atoi(p[7].c_str());d.weight=(float)atof(p[8].c_str());}if(animIndex(d.name)>=0)continue;Anim raw;raw.name=d.name;raw.dur=d.dur;
    while(std::getline(f,line)){std::vector<std::string> q;std::string cur;for(char ch:line){if(ch=='|'){q.push_back(cur);cur.clear();}else cur+=ch;}q.push_back(cur);
      if(q[0]=="key"&&q.size()>=9&&byName.count(q[1])){ClipKey k;k.t=(float)atof(q[2].c_str());k.rot={(float)atof(q[3].c_str()),(float)atof(q[4].c_str()),(float)atof(q[5].c_str())};k.pos={(float)atof(q[6].c_str()),(float)atof(q[7].c_str()),(float)atof(q[8].c_str())};if(q.size()>9)k.ease=atoi(q[9].c_str());if(q.size()>13)for(int z=0;z<4;z++)k.bz[z]=(float)atof(q[10+z].c_str());d.keys[byName[q[1]]].push_back(k);}
      else if(q[0]=="event"&&q.size()>=3)d.events.push_back({(float)atof(q[1].c_str()),q[2]});
      else if(q[0]=="pin"&&q.size()>=5&&byName.count(q[1]))d.pins.push_back({byName[q[1]],Vec3{(float)atof(q[2].c_str()),(float)atof(q[3].c_str()),(float)atof(q[4].c_str())}});
      else if(q[0]=="ch"&&q.size()>=4&&byName.count(q[1])){AnimChannel c;c.node=byName[q[1]];c.path=atoi(q[2].c_str());size_t n=(size_t)atoi(q[3].c_str());int comp=c.path==1?4:3;std::getline(f,line);{std::istringstream s(line);float v;while(s>>v)c.t.push_back(v);}std::getline(f,line);{std::istringstream s(line);float v;while(s>>v)c.v.push_back(v);}
        if(c.t.size()==n&&c.v.size()==n*comp)raw.ch.push_back(std::move(c));}}
    for(auto& kv:d.keys)std::sort(kv.second.begin(),kv.second.end(),[](const ClipKey& a,const ClipKey& b){return a.t<b.t;});
    clips.push_back(d);rawData.resize(clips.size());if(d.raw)rawData.back()=raw;bakeClip((int)clips.size()-1);}}
// ---- retargeting: world-space rotation deltas on bones matched by role name
static std::string roleOf(std::string n){size_t c=n.find(':');if(c!=std::string::npos)n=n.substr(c+1);for(char& ch:n)ch=(char)tolower(ch);std::string side;
  auto has=[&](const char* s){return n.find(s)!=std::string::npos;};
  if(has("left")||has(".l")||has("_l")||(n.size()>2&&n.substr(n.size()-2)=="_l")||(n.size()>1&&n.back()=='l'&&n.size()>2&&n[n.size()-2]=='.'))side="L";else if(has("right")||has(".r")||has("_r"))side="R";
  for(const char* f:{"index","middle","ring","pinky","thumb","toe","finger","twist","end","nub","eye","jaw"})if(has(f))return "";
  if(has("hips")||has("pelvis")||n=="root"||n=="hip")return "hips";
  if(n.rfind("spine",0)==0||has("chest")){if(has("05"))return "spine0";if(has("03"))return "spine1";if(has("01"))return "spine2";if(has("chest"))return "spine2";if(has("spine2"))return "spine2";if(has("spine1"))return "spine1";return "spine0";}
  if(has("neck"))return "neck";if(has("head"))return "head";
  if(has("clavicle")||has("shoulder"))return "clav"+side;
  if(has("forearm")||has("lowerarm"))return "loarm"+side;if(has("upperarm")||has("arm"))return "uparm"+side;
  if(has("hand")||has("wrist"))return "hand"+side;
  if(has("upleg")||has("upperleg")||has("thigh"))return "upleg"+side;
  if(has("lowerleg")||has("calf")||has("shin")||has("leg"))return "loleg"+side;
  if(has("foot")||has("ankle"))return "foot"+side;return "";}
int Model::retargetFrom(const Model& src,int& mapped,bool lib){mapped=0;std::map<std::string,int> sr;for(size_t i=0;i<src.nodeName.size();i++){std::string r=roleOf(src.nodeName[i]);if(!r.empty()&&!sr.count(r))sr[r]=(int)i;}
  size_t nn=rest.size();std::vector<int> map(nn,-1);for(size_t i=0;i<nn;i++){std::string r=roleOf(nodeName[i]);if(r.empty())continue;auto it=sr.find(r);if(it!=sr.end()){map[i]=it->second;mapped++;}}
  if(mapped<3)return 0;
  auto worldQ=[&](const Model& m,const std::vector<TRS>& p,std::vector<std::array<float,4>>& w){size_t n=p.size();w.assign(n,{0,0,0,1});std::vector<char> dn(n,0);std::function<void(int)> go=[&](int i){if(dn[i])return;int pa=m.parent[i];std::array<float,4> l{p[i].r[0],p[i].r[1],p[i].r[2],p[i].r[3]};if(pa>=0){go(pa);qmul(w[pa].data(),l.data(),w[i].data());}else w[i]=l;dn[i]=1;};for(size_t i=0;i<n;i++)go((int)i);};
  std::vector<std::array<float,4>> sRest,dRest;worldQ(src,src.rest,sRest);worldQ(*this,rest,dRest);
  float sH=src.height/std::max(1e-4f,src.fit),dH=height/std::max(1e-4f,fit),ratio=dH/std::max(1e-4f,sH);int hips=-1;for(size_t i=0;i<nn;i++)if(roleOf(nodeName[i])=="hips")hips=(int)i;
  int added=0;for(size_t ai=0;ai<(lib?(size_t)src.builtin:src.anims.size());ai++){const Anim& sa=src.anims[ai];if(sa.dur<=0||sa.name=="Locomotion")continue;ClipDef d;d.name=uniqueClipName(sa.name.substr(sa.name.find('|')==std::string::npos?0:sa.name.find('|')+1));d.dur=sa.dur;d.raw=true;d.lib=lib;
    Anim out;out.name=d.name;out.dur=sa.dur;int fr=(int)std::ceil(sa.dur*30)+1;std::vector<AnimChannel> cr(nn),ct(nn);for(size_t i=0;i<nn;i++){cr[i].node=ct[i].node=(int)i;cr[i].path=1;ct[i].path=0;}
    std::vector<TRS> sp;std::vector<std::array<float,4>> sw;std::vector<std::array<float,4>> dw(nn);
    for(int k=0;k<fr;k++){float t=std::min(sa.dur,k/30.0f);src.sample((int)ai,t,sp);worldQ(src,sp,sw);std::vector<char> dn(nn,0);std::vector<TRS> o=rest;
      std::function<void(int)> go=[&](int i){if(dn[i])return;int pa=parent[i];if(pa>=0)go(pa);std::array<float,4> pq=pa>=0?dw[pa]:std::array<float,4>{0,0,0,1};
        if(map[i]>=0){int s=map[i];float inv[4]={-sRest[s][0],-sRest[s][1],-sRest[s][2],sRest[s][3]},dl[4],wa[4];qmul(sw[s].data(),inv,dl);qmul(dl,dRest[i].data(),wa);for(int j=0;j<4;j++)dw[i][j]=wa[j];}
        else{float l[4]={rest[i].r[0],rest[i].r[1],rest[i].r[2],rest[i].r[3]},wa[4];qmul(pq.data(),l,wa);for(int j=0;j<4;j++)dw[i][j]=wa[j];}
        float ip[4]={-pq[0],-pq[1],-pq[2],pq[3]},loc[4];qmul(ip,dw[i].data(),loc);for(int j=0;j<4;j++)o[i].r[j]=loc[j];dn[i]=1;};
      for(size_t i=0;i<nn;i++)go((int)i);
      if(hips>=0&&map[hips]>=0){int s=map[hips];o[hips].t=rest[hips].t+(sp[s].t-src.rest[s].t)*ratio;}
      for(size_t i=0;i<nn;i++){cr[i].t.push_back(t);ct[i].t.push_back(t);cr[i].v.insert(cr[i].v.end(),{o[i].r[0],o[i].r[1],o[i].r[2],o[i].r[3]});ct[i].v.insert(ct[i].v.end(),{o[i].t.x,o[i].t.y,o[i].t.z});}}
    for(size_t i=0;i<nn;i++){out.ch.push_back(std::move(ct[i]));out.ch.push_back(std::move(cr[i]));}
    clips.push_back(d);rawData.resize(clips.size());rawData.back()=out;bakeClip((int)clips.size()-1);added++;}
  return added;}

static Mat4 fm(const ufbx_matrix& u){Mat4 r=Mat4::identity();for(int c=0;c<4;c++){r.m[c*4]=(float)u.cols[c].x;r.m[c*4+1]=(float)u.cols[c].y;r.m[c*4+2]=(float)u.cols[c].z;}return r;}
static bool loadFbx(Renderer& r,const std::string& path,float targetHeight,Model& out){
  ufbx_load_opts o{};o.target_axes=ufbx_axes_right_handed_y_up;o.target_unit_meters=1.0f;o.space_conversion=UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;o.generate_missing_normals=true;
  ufbx_error err;ufbx_scene* sc=ufbx_load_file(path.c_str(),&o,&err);if(!sc)return false;
  out=Model();size_t sl=path.find_last_of("/\\");out.name=path.substr(sl==std::string::npos?0:sl+1);std::string base=sl==std::string::npos?".":path.substr(0,sl);
  size_t nn=sc->nodes.count;out.parent.assign(nn,-1);out.rest.resize(nn);out.nodeName.resize(nn);
  for(size_t i=0;i<nn;i++){ufbx_node* n=sc->nodes.data[i];out.parent[i]=n->parent?(int)n->parent->typed_id:-1;out.nodeName[i]=std::string(n->name.data,n->name.length);TRS& k=out.rest[i];const ufbx_transform& t=n->local_transform;
    k.t={(float)t.translation.x,(float)t.translation.y,(float)t.translation.z};k.r[0]=(float)t.rotation.x;k.r[1]=(float)t.rotation.y;k.r[2]=(float)t.rotation.z;k.r[3]=(float)t.rotation.w;k.s={(float)t.scale.x,(float)t.scale.y,(float)t.scale.z};}
  std::vector<int> jointOf(nn,-1);auto addJoint=[&](int node,const Mat4& ibm)->int{if(jointOf[node]>=0)return jointOf[node];jointOf[node]=(int)out.joints.size();out.joints.push_back(node);out.ibm.push_back(ibm);return jointOf[node];};
  std::vector<unsigned char> white{255,255,255,255};unsigned wt=r.createTexture(white.data(),1,1);std::map<const ufbx_texture*,unsigned> tc;
  auto texOf=[&](const ufbx_texture* t)->unsigned{if(!t)return wt;auto it=tc.find(t);if(it!=tc.end())return it->second;int w=0,h=0,c=0;unsigned char* px=nullptr;
    if(t->content.size>0)px=stbi_load_from_memory((const unsigned char*)t->content.data,(int)t->content.size,&w,&h,&c,4);
    if(!px){std::string rel(t->relative_filename.data,t->relative_filename.length),fn(t->filename.data,t->filename.length);std::string bn=fn;size_t s2=bn.find_last_of("/\\");if(s2!=std::string::npos)bn=bn.substr(s2+1);
      for(const std::string& cand:{base+"/"+rel,base+"/"+bn,base+"/textures/"+bn,base+"/tex/"+bn,fn}){px=stbi_load(cand.c_str(),&w,&h,&c,4);if(px)break;}}
    unsigned id=wt;if(px){id=r.createTexture(px,w,h);stbi_image_free(px);}tc[t]=id;return id;};
  std::vector<Mat4> restW(nn);{std::vector<char> dn(nn,0);std::function<void(int)> go=[&](int i){if(dn[i])return;int pa=out.parent[i];Mat4 l=toM(out.rest[i]);if(pa>=0){go(pa);restW[i]=restW[pa]*l;}else restW[i]=l;dn[i]=1;};for(size_t i=0;i<nn;i++)go((int)i);}
  float mn=1e9f,mx=-1e9f,rad=0;std::map<std::pair<const ufbx_material*,int>,int> matIdx;
  for(size_t ni=0;ni<nn;ni++){ufbx_node* n=sc->nodes.data[ni];ufbx_mesh* me=n->mesh;if(!me||me->num_faces==0)continue;ufbx_skin_deformer* sk=me->skin_deformers.count?me->skin_deformers.data[0]:nullptr;
    std::vector<int> clJoint;if(sk){for(size_t c=0;c<sk->clusters.count;c++){ufbx_skin_cluster* cl=sk->clusters.data[c];clJoint.push_back(addJoint((int)cl->bone_node->typed_id,fm(cl->geometry_to_bone)));}}
    else{ufbx_matrix inv=ufbx_matrix_invert(&n->geometry_to_world);addJoint((int)ni,fm(inv));}
    size_t slots=std::max<size_t>(1,me->materials.count);
    for(size_t slot=0;slot<slots;slot++){std::vector<SkinVertex> vs;std::vector<uint32_t> idx;std::vector<uint32_t> tri((me->max_face_triangles+1)*3);
      for(size_t f=0;f<me->num_faces;f++){uint32_t fm_=me->face_material.count?me->face_material.data[f]:0;if(fm_!=slot)continue;ufbx_face face=me->faces.data[f];uint32_t nt=ufbx_triangulate_face(tri.data(),tri.size(),me,face);
        for(uint32_t k=0;k<nt*3;k++){uint32_t ci=tri[k];SkinVertex s{};ufbx_vec3 p=ufbx_get_vertex_vec3(&me->vertex_position,ci);s.p[0]=(float)p.x;s.p[1]=(float)p.y;s.p[2]=(float)p.z;
          if(me->vertex_normal.exists){ufbx_vec3 nr=ufbx_get_vertex_vec3(&me->vertex_normal,ci);s.n[0]=(float)nr.x;s.n[1]=(float)nr.y;s.n[2]=(float)nr.z;}else{s.n[1]=1;}
          if(me->vertex_uv.exists){ufbx_vec2 u=ufbx_get_vertex_vec2(&me->vertex_uv,ci);s.uv[0]=(float)u.x;s.uv[1]=1.0f-(float)u.y;}
          if(sk){uint32_t vi=me->vertex_indices.data[ci];ufbx_skin_vertex sv=sk->vertices.data[vi];float ws[4]={0,0,0,0};int js[4]={0,0,0,0};
            for(uint32_t w=0;w<sv.num_weights;w++){ufbx_skin_weight sw=sk->weights.data[sv.weight_begin+w];float wv=(float)sw.weight;int jj=clJoint[sw.cluster_index];for(int q=0;q<4;q++)if(wv>ws[q]){for(int z=3;z>q;z--){ws[z]=ws[z-1];js[z]=js[z-1];}ws[q]=wv;js[q]=jj;break;}}
            float tot=ws[0]+ws[1]+ws[2]+ws[3];if(tot<1e-6f){ws[0]=1;js[0]=clJoint.empty()?0:clJoint[0];tot=1;}for(int q=0;q<4;q++){s.w[q]=ws[q]/tot;s.j[q]=(unsigned char)std::min(js[q],79);}}
          else{s.w[0]=1;s.j[0]=(unsigned char)std::min(jointOf[ni],79);}
          // rest-pose position for bounds
          Vec3 acc{};for(int q=0;q<4;q++)if(s.w[q]>0&&s.j[q]<out.joints.size()){const Mat4 jm=restW[out.joints[s.j[q]]]*out.ibm[s.j[q]];Vec3 pp{jm.m[0]*s.p[0]+jm.m[4]*s.p[1]+jm.m[8]*s.p[2]+jm.m[12],jm.m[1]*s.p[0]+jm.m[5]*s.p[1]+jm.m[9]*s.p[2]+jm.m[13],jm.m[2]*s.p[0]+jm.m[6]*s.p[1]+jm.m[10]*s.p[2]+jm.m[14]};acc=acc+pp*s.w[q];}
          mn=std::min(mn,acc.y);mx=std::max(mx,acc.y);rad=std::max(rad,std::sqrt(acc.x*acc.x+acc.z*acc.z));
          idx.push_back((uint32_t)vs.size());vs.push_back(s);}}
      if(vs.empty())continue;
      const ufbx_material* mt=slot<me->materials.count?me->materials.data[slot]:nullptr;auto key=std::make_pair(mt,0);int mi;auto it=matIdx.find(key);
      if(it!=matIdx.end())mi=it->second;else{ModelMat mm;mm.tex=wt;if(mt){const ufbx_material_map& bc=mt->pbr.base_color.has_value?mt->pbr.base_color:mt->fbx.diffuse_color;mm.tex=texOf(bc.texture);if(bc.has_value&&!bc.texture){mm.col[0]=(float)bc.value_vec4.x;mm.col[1]=(float)bc.value_vec4.y;mm.col[2]=(float)bc.value_vec4.z;}}
        mi=(int)out.mats.size();out.mats.push_back(mm);matIdx[key]=mi;}
      ModelPrim mp;mp.mesh=r.uploadSkin(vs,idx);mp.mat=mi;mp.cv=vs;mp.ci=idx;out.prims.push_back(std::move(mp));}}
  if(out.joints.empty()||out.joints.size()>80||out.prims.empty()){ufbx_free_scene(sc);return false;}
  // animations (baked at 30 fps)
  for(size_t si=0;si<sc->anim_stacks.count;si++){ufbx_anim_stack* st=sc->anim_stacks.data[si];Anim an;an.name=std::string(st->name.data,st->name.length);if(an.name.empty())an.name="clip"+std::to_string(si);
    double t0=st->time_begin,t1=st->time_end;an.dur=(float)std::max(0.0,t1-t0);int fr=std::max(2,(int)std::ceil(an.dur*30)+1);
    std::vector<char> used(nn,0);for(int j:out.joints){int p=j;while(p>=0&&!used[p]){used[p]=1;p=out.parent[p];}}
    for(size_t ni=0;ni<nn;ni++){if(!used[ni])continue;AnimChannel ct,cr,cs;ct.node=cr.node=cs.node=(int)ni;ct.path=0;cr.path=1;cs.path=2;
      for(int k=0;k<fr;k++){double tt=std::min(t1,t0+k/30.0);ufbx_transform tr=ufbx_evaluate_transform(st->anim,sc->nodes.data[ni],tt);float tm=(float)(tt-t0);
        ct.t.push_back(tm);cr.t.push_back(tm);cs.t.push_back(tm);ct.v.insert(ct.v.end(),{(float)tr.translation.x,(float)tr.translation.y,(float)tr.translation.z});cr.v.insert(cr.v.end(),{(float)tr.rotation.x,(float)tr.rotation.y,(float)tr.rotation.z,(float)tr.rotation.w});cs.v.insert(cs.v.end(),{(float)tr.scale.x,(float)tr.scale.y,(float)tr.scale.z});}
      an.ch.push_back(std::move(ct));an.ch.push_back(std::move(cr));an.ch.push_back(std::move(cs));}
    out.anims.push_back(std::move(an));}
  float raw=std::max(0.01f,mx-mn);out.fit=targetHeight/raw;out.foot=mn;out.height=targetHeight;out.radius=std::max(0.5f,rad*out.fit);addLoco(out);out.builtin=(int)out.anims.size();
  ufbx_free_scene(sc);return true;}
}

namespace sk {
int Model::morphIndex(const std::string& n) const{for(size_t i=0;i<morphNames.size();i++)if(morphNames[i]==n)return (int)i;return -1;}
void Model::applyMorphs(Renderer& r) const{if(morphNames.empty())return;if(morphW.size()<morphNames.size())morphW.resize(morphNames.size(),0.f);
  for(const ModelPrim& p:prims){if(p.md.empty()||p.mesh<0)continue;std::vector<SkinVertex> v=p.cv;for(size_t k=0;k<p.md.size()&&k<morphW.size();k++){float w=morphW[k];if(std::fabs(w)<1e-4f||p.md[k].empty())continue;for(size_t i=0;i<v.size();i++)for(int c=0;c<3;c++)v[i].p[c]+=w*p.md[k][i*3+c];}
    r.updateSkin(p.mesh,v);}}
}
