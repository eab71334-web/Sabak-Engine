#include "model.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <functional>
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
  prev=a;pt=t;a=i;t=0;blend=prev<0?1.f:0.f;cur=name;(void)fade;
  loop=!(name.rfind("die",0)==0||name=="mount"||name=="dismount");}
void ModelInst::update(float dt,float speed){if(!m||a<0)return;float d=m->anims[a].dur;t+=dt*speed;if(loop){if(d>0)t=std::fmod(t,d);}else t=std::min(t,d);
  if(blend<1){blend=std::min(1.f,blend+dt/0.2f);if(prev>=0){float pd=m->anims[prev].dur;pt+=dt*speed;if(pd>0)pt=std::fmod(pt,pd);}}}
void ModelInst::evaluate(){if(!m)return;const Model& M=*m;std::vector<TRS> p;M.sample(a,t,p);
  if(blend<1&&prev>=0){std::vector<TRS> q;M.sample(prev,pt,q);for(size_t i=0;i<p.size();i++){float k=blend;TRS& o=p[i];const TRS& s=q[i];
      o.t=s.t+(o.t-s.t)*k;o.s=s.s+(o.s-s.s)*k;float r[4];nlerp(s.r,o.r,k,r);for(int j=0;j<4;j++)o.r[j]=r[j];}}
  size_t n=p.size();std::vector<Mat4> w(n);std::vector<char> done(n,0);
  std::function<void(int)> go=[&](int i){if(done[i])return;int pa=M.parent[i];Mat4 l=toM(p[i]);if(pa>=0){go(pa);w[i]=w[pa]*l;}else w[i]=l;done[i]=1;};
  for(size_t i=0;i<n;i++)go((int)i);
  joints.resize(M.joints.size()*16);for(size_t j=0;j<M.joints.size();j++){Mat4 jm=w[M.joints[j]]*M.ibm[j];std::memcpy(&joints[j*16],jm.m,64);}}

bool loadModel(Renderer& r,const std::string& name,float targetHeight,Model& out){
  std::string path=name;size_t sl=path.find_last_of("/\\");std::string base=sl==std::string::npos?".":path.substr(0,sl);cgltf_options opt{};cgltf_data* d=nullptr;
  if(cgltf_parse_file(&opt,path.c_str(),&d)!=cgltf_result_success)return false;
  if(cgltf_load_buffers(&opt,d,path.c_str())!=cgltf_result_success){cgltf_free(d);return false;}
  out=Model();out.name=name;
  // nodes
  size_t nn=d->nodes_count;out.parent.assign(nn,-1);out.rest.resize(nn);
  for(size_t i=0;i<nn;i++){cgltf_node* n=&d->nodes[i];if(n->parent)out.parent[i]=(int)(n->parent-d->nodes);TRS& k=out.rest[i];
    if(n->has_translation)k.t={n->translation[0],n->translation[1],n->translation[2]};if(n->has_rotation)for(int j=0;j<4;j++)k.r[j]=n->rotation[j];if(n->has_scale)k.s={n->scale[0],n->scale[1],n->scale[2]};}
  // skin
  if(d->skins_count>0){cgltf_skin* sk=&d->skins[0];for(size_t j=0;j<sk->joints_count;j++){out.joints.push_back((int)(sk->joints[j]-d->nodes));Mat4 m=Mat4::identity();if(sk->inverse_bind_matrices)cgltf_accessor_read_float(sk->inverse_bind_matrices,j,m.m,16);out.ibm.push_back(m);}}
  if(out.joints.empty()||out.joints.size()>32){cgltf_free(d);return false;}
  // materials + textures
  std::map<std::string,unsigned> cache;std::vector<unsigned char> white{255,255,255,255};unsigned wt=r.createTexture(white.data(),1,1);
  auto loadImg=[&](cgltf_texture* t)->unsigned{if(!t||!t->image)return wt;std::string key;int w=0,h=0,c=0;unsigned char* px=nullptr;
    if(t->image->uri&&std::string(t->image->uri).rfind("data:",0)!=0){key=t->image->uri;auto it=cache.find(key);if(it!=cache.end())return it->second;px=stbi_load((base+"/"+key).c_str(),&w,&h,&c,4);}
    else if(t->image->buffer_view){const unsigned char* b=(const unsigned char*)t->image->buffer_view->buffer->data+t->image->buffer_view->offset;px=stbi_load_from_memory(b,(int)t->image->buffer_view->size,&w,&h,&c,4);key=std::to_string((size_t)t->image);}
    if(!px)return wt;unsigned id=r.createTexture(px,w,h);stbi_image_free(px);cache[key]=id;return id;};
  for(size_t i=0;i<d->materials_count;i++){cgltf_material* m=&d->materials[i];ModelMat mm;if(m->has_pbr_metallic_roughness){for(int j=0;j<4;j++)mm.col[j]=m->pbr_metallic_roughness.base_color_factor[j];mm.tex=loadImg(m->pbr_metallic_roughness.base_color_texture.texture);}else mm.tex=wt;
    mm.mask=m->alpha_mode!=cgltf_alpha_mode_opaque;out.mats.push_back(mm);}
  if(out.mats.empty()){ModelMat mm;mm.tex=wt;out.mats.push_back(mm);}
  // meshes (every skinned primitive uses skin 0 / joint indices directly)
  float mn=1e9f,mx=-1e9f,rad=0;
  for(size_t ni=0;ni<nn;ni++){cgltf_node* n=&d->nodes[ni];if(!n->mesh)continue;
    for(size_t pi=0;pi<n->mesh->primitives_count;pi++){cgltf_primitive* pr=&n->mesh->primitives[pi];if(pr->type!=cgltf_primitive_type_triangles)continue;
      cgltf_accessor *ap=nullptr,*an=nullptr,*au=nullptr,*aj=nullptr,*aw=nullptr;
      for(size_t k=0;k<pr->attributes_count;k++){cgltf_attribute* at=&pr->attributes[k];if(at->type==cgltf_attribute_type_position)ap=at->data;else if(at->type==cgltf_attribute_type_normal)an=at->data;
        else if(at->type==cgltf_attribute_type_texcoord&&at->index==0)au=at->data;else if(at->type==cgltf_attribute_type_joints&&at->index==0)aj=at->data;else if(at->type==cgltf_attribute_type_weights&&at->index==0)aw=at->data;}
      if(!ap||!aj||!aw)continue;size_t vc=ap->count;std::vector<SkinVertex> vs(vc);
      for(size_t v=0;v<vc;v++){SkinVertex& s=vs[v];cgltf_accessor_read_float(ap,v,s.p,3);if(an)cgltf_accessor_read_float(an,v,s.n,3);else{s.n[0]=0;s.n[1]=1;s.n[2]=0;}if(au)cgltf_accessor_read_float(au,v,s.uv,2);
        cgltf_uint j4[4]={0,0,0,0};cgltf_accessor_read_uint(aj,v,j4,4);for(int k=0;k<4;k++)s.j[k]=(unsigned char)std::min<cgltf_uint>(j4[k],31);cgltf_accessor_read_float(aw,v,s.w,4);
        float sw=s.w[0]+s.w[1]+s.w[2]+s.w[3];if(sw>1e-6f)for(int k=0;k<4;k++)s.w[k]/=sw;else{s.w[0]=1;}
        mn=std::min(mn,s.p[1]);mx=std::max(mx,s.p[1]);rad=std::max(rad,std::sqrt(s.p[0]*s.p[0]+s.p[2]*s.p[2]));}
      std::vector<uint32_t> idx;if(pr->indices){idx.resize(pr->indices->count);for(size_t k=0;k<idx.size();k++)idx[k]=(uint32_t)cgltf_accessor_read_index(pr->indices,k);}else{idx.resize(vc);for(size_t k=0;k<vc;k++)idx[k]=(uint32_t)k;}
      ModelPrim mp;mp.mesh=r.uploadSkin(vs,idx);mp.mat=pr->material?(int)(pr->material-d->materials):0;out.prims.push_back(mp);}}
  // animations
  for(size_t ai=0;ai<d->animations_count;ai++){cgltf_animation* a=&d->animations[ai];Anim an;an.name=a->name?a->name:"anim"+std::to_string(ai);
    for(size_t ci=0;ci<a->channels_count;ci++){cgltf_animation_channel* c=&a->channels[ci];if(!c->target_node||!c->sampler)continue;AnimChannel ch;ch.node=(int)(c->target_node-d->nodes);
      if(c->target_path==cgltf_animation_path_type_translation)ch.path=0;else if(c->target_path==cgltf_animation_path_type_rotation)ch.path=1;else if(c->target_path==cgltf_animation_path_type_scale)ch.path=2;else continue;
      ch.step=c->sampler->interpolation==cgltf_interpolation_type_step;bool cubic=c->sampler->interpolation==cgltf_interpolation_type_cubic_spline;int comp=ch.path==1?4:3;
      size_t n=c->sampler->input->count;ch.t.resize(n);ch.v.resize(n*comp);for(size_t k=0;k<n;k++){cgltf_accessor_read_float(c->sampler->input,k,&ch.t[k],1);cgltf_accessor_read_float(c->sampler->output,cubic?k*3+1:k,&ch.v[k*comp],comp);}
      if(n)an.dur=std::max(an.dur,ch.t.back());an.ch.push_back(std::move(ch));}
    out.anims.push_back(std::move(an));}
  float raw=std::max(0.01f,mx-mn);out.fit=targetHeight/raw;out.foot=mn;out.height=targetHeight;out.radius=std::max(0.5f,rad*out.fit);
  cgltf_free(d);return !out.prims.empty();}
}
