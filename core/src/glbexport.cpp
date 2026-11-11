// Sabak Core - export a skinned model with its animations as a glTF 2.0 binary (.glb).
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include "model.h"
namespace sk {
namespace {
struct Bin{
  std::string d;
  int add(const void* p,size_t n){while(d.size()%4)d.push_back(0);int off=(int)d.size();d.append((const char*)p,n);return off;}
};
std::string esc(const std::string& s){std::string o;for(char c:s){if(c=='"'||c=='\\')o+='\\';if((unsigned char)c<32)continue;o+=c;}return o;}
std::string num(float v){if(!std::isfinite(v))v=0;char b[40];std::snprintf(b,sizeof b,"%.7g",v);return b;}
}
bool exportGlb(const Model& m,const std::string& path,std::string& err){
  if(m.prims.empty()||m.prims[0].cv.empty()){err="Model has no mesh data";return false;}
  Bin bin;std::ostringstream bv,ac,js;int nbv=0,nac=0;
  auto addView=[&](const void* p,size_t n,int target){int off=bin.add(p,n);if(nbv)bv<<',';bv<<"{\"buffer\":0,\"byteOffset\":"<<off<<",\"byteLength\":"<<n;if(target)bv<<",\"target\":"<<target;bv<<'}';return nbv++;};
  auto addAcc=[&](int view,int comp,size_t count,const char* type,const std::string& mm){if(nac)ac<<',';ac<<"{\"bufferView\":"<<view<<",\"componentType\":"<<comp<<",\"count\":"<<count<<",\"type\":\""<<type<<'"'<<mm<<'}';return nac++;};
  // ---- meshes
  std::ostringstream prims;size_t N=m.nodeName.size();
  for(size_t pi=0;pi<m.prims.size();pi++){const ModelPrim& p=m.prims[pi];size_t vc=p.cv.size();if(!vc||p.ci.empty())continue;
    std::vector<float> pos(vc*3),nor(vc*3),uv(vc*2),wt(vc*4);std::vector<unsigned char> jn(vc*4);float mn[3]={1e9f,1e9f,1e9f},mx[3]={-1e9f,-1e9f,-1e9f};
    for(size_t i=0;i<vc;i++){const SkinVertex& v=p.cv[i];for(int k=0;k<3;k++){pos[i*3+k]=v.p[k];nor[i*3+k]=v.n[k];mn[k]=std::min(mn[k],v.p[k]);mx[k]=std::max(mx[k],v.p[k]);}uv[i*2]=v.uv[0];uv[i*2+1]=v.uv[1];for(int k=0;k<4;k++){wt[i*4+k]=v.w[k];jn[i*4+k]=v.j[k];}}
    int vp=addView(pos.data(),pos.size()*4,34962),vn=addView(nor.data(),nor.size()*4,34962),vu=addView(uv.data(),uv.size()*4,34962),vj=addView(jn.data(),jn.size(),34962),vw=addView(wt.data(),wt.size()*4,34962),vi=addView(p.ci.data(),p.ci.size()*4,34963);
    char mm[160];std::snprintf(mm,sizeof mm,",\"min\":[%s,%s,%s],\"max\":[%s,%s,%s]",num(mn[0]).c_str(),num(mn[1]).c_str(),num(mn[2]).c_str(),num(mx[0]).c_str(),num(mx[1]).c_str(),num(mx[2]).c_str());
    int aP=addAcc(vp,5126,vc,"VEC3",mm),aN=addAcc(vn,5126,vc,"VEC3",""),aU=addAcc(vu,5126,vc,"VEC2",""),aJ=addAcc(vj,5121,vc,"VEC4",""),aW=addAcc(vw,5126,vc,"VEC4",""),aI=addAcc(vi,5125,p.ci.size(),"SCALAR","");
    if(prims.tellp()>0)prims<<',';prims<<"{\"attributes\":{\"POSITION\":"<<aP<<",\"NORMAL\":"<<aN<<",\"TEXCOORD_0\":"<<aU<<",\"JOINTS_0\":"<<aJ<<",\"WEIGHTS_0\":"<<aW<<"},\"indices\":"<<aI<<",\"material\":"<<p.mat;
    if(!p.md.empty()&&!m.morphNames.empty()){prims<<",\"targets\":[";for(size_t k=0;k<m.morphNames.size();k++){std::vector<float> d(vc*3,0.f);if(k<p.md.size()&&p.md[k].size()==vc*3)d=p.md[k];float tn[3]={1e9f,1e9f,1e9f},tx[3]={-1e9f,-1e9f,-1e9f};for(size_t i=0;i<vc;i++)for(int c=0;c<3;c++){tn[c]=std::min(tn[c],d[i*3+c]);tx[c]=std::max(tx[c],d[i*3+c]);}
        char mm2[160];std::snprintf(mm2,sizeof mm2,",\"min\":[%s,%s,%s],\"max\":[%s,%s,%s]",num(tn[0]).c_str(),num(tn[1]).c_str(),num(tn[2]).c_str(),num(tx[0]).c_str(),num(tx[1]).c_str(),num(tx[2]).c_str());
        int aT=addAcc(addView(d.data(),d.size()*4,34962),5126,vc,"VEC3",mm2);if(k)prims<<',';prims<<"{\"POSITION\":"<<aT<<'}';}prims<<']';}
    prims<<'}';}
  // ---- inverse bind matrices
  std::vector<float> ib;for(auto& mt:m.ibm)for(int k=0;k<16;k++)ib.push_back(mt.m[k]);int aIB=-1;if(!ib.empty())aIB=addAcc(addView(ib.data(),ib.size()*4,0),5126,m.ibm.size(),"MAT4","");
  // ---- materials, textures
  std::ostringstream mats,tex,img;int ntex=0;
  for(size_t i=0;i<m.mats.size();i++){const ModelMat& mt=m.mats[i];if(i)mats<<',';mats<<"{\"pbrMetallicRoughness\":{\"baseColorFactor\":["<<num(mt.col[0])<<','<<num(mt.col[1])<<','<<num(mt.col[2])<<','<<num(mt.col[3])<<"],\"metallicFactor\":0,\"roughnessFactor\":0.8";
    if(!mt.img.empty()){int v=addView(mt.img.data(),mt.img.size(),0);if(ntex){tex<<',';img<<',';}img<<"{\"bufferView\":"<<v<<",\"mimeType\":\""<<(mt.mime.empty()?"image/png":mt.mime)<<"\"}";tex<<"{\"source\":"<<ntex<<'}';mats<<",\"baseColorTexture\":{\"index\":"<<ntex<<'}';ntex++;}
    mats<<'}';if(mt.mask)mats<<",\"alphaMode\":\"MASK\"";mats<<",\"doubleSided\":true}";}
  if(m.mats.empty())mats<<"{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1],\"metallicFactor\":0}}";
  // ---- nodes
  std::ostringstream nodes;std::vector<std::vector<int>> kids(N);std::vector<int> roots;for(size_t i=0;i<N;i++){if(m.parent[i]>=0&&(size_t)m.parent[i]<N)kids[m.parent[i]].push_back((int)i);else roots.push_back((int)i);}
  for(size_t i=0;i<N;i++){const TRS& t=m.rest[i];if(i)nodes<<',';nodes<<"{\"name\":\""<<esc(m.nodeName[i])<<"\",\"translation\":["<<num(t.t.x)<<','<<num(t.t.y)<<','<<num(t.t.z)<<"],\"rotation\":["<<num(t.r[0])<<','<<num(t.r[1])<<','<<num(t.r[2])<<','<<num(t.r[3])<<"],\"scale\":["<<num(t.s.x)<<','<<num(t.s.y)<<','<<num(t.s.z)<<']';
    if(!kids[i].empty()){nodes<<",\"children\":[";for(size_t k=0;k<kids[i].size();k++){if(k)nodes<<',';nodes<<kids[i][k];}nodes<<']';}nodes<<'}';}
  nodes<<",{\"name\":\""<<esc(m.name.empty()?"Mesh":m.name)<<"\",\"mesh\":0,\"skin\":0}";
  // ---- animations
  std::ostringstream anims;int na=0;
  for(const Anim& a:m.anims){if(a.ch.empty()||a.name=="Locomotion")continue;std::ostringstream ch,sm;int ns=0;
    for(const AnimChannel& c:a.ch){if(c.t.empty()||c.node<0||(size_t)c.node>=N)continue;int comp=c.path==1?4:3;if(c.v.size()<c.t.size()*comp)continue;
      char mm[96];std::snprintf(mm,sizeof mm,",\"min\":[%s],\"max\":[%s]",num(c.t.front()).c_str(),num(c.t.back()).c_str());
      int aT=addAcc(addView(c.t.data(),c.t.size()*4,0),5126,c.t.size(),"SCALAR",mm),aV=addAcc(addView(c.v.data(),c.t.size()*comp*4,0),5126,c.t.size(),comp==4?"VEC4":"VEC3","");
      if(ns){sm<<',';ch<<',';}sm<<"{\"input\":"<<aT<<",\"output\":"<<aV<<",\"interpolation\":\""<<(c.step?"STEP":"LINEAR")<<"\"}";ch<<"{\"sampler\":"<<ns<<",\"target\":{\"node\":"<<c.node<<",\"path\":\""<<(c.path==0?"translation":c.path==1?"rotation":"scale")<<"\"}}";ns++;}
    if(!ns)continue;if(na)anims<<',';anims<<"{\"name\":\""<<esc(a.name)<<"\",\"samplers\":["<<sm.str()<<"],\"channels\":["<<ch.str()<<"]}";na++;}
  // ---- skin
  std::ostringstream skin;skin<<"{\"joints\":[";for(size_t i=0;i<m.joints.size();i++){if(i)skin<<',';skin<<m.joints[i];}skin<<']';if(aIB>=0)skin<<",\"inverseBindMatrices\":"<<aIB;skin<<'}';
  js<<"{\"asset\":{\"version\":\"2.0\",\"generator\":\"Sabak Engine\"},\"scene\":0,\"scenes\":[{\"nodes\":[";for(size_t i=0;i<roots.size();i++)js<<roots[i]<<',';js<<N<<"]}],\"nodes\":["<<nodes.str()<<"],\"meshes\":[{\"name\":\"Body\",\"primitives\":["<<prims.str()<<"]"<<(m.morphNames.empty()?"":([&]{std::string t=",\"extras\":{\"targetNames\":[";for(size_t k=0;k<m.morphNames.size();k++){if(k)t+=',';t+="\""+esc(m.morphNames[k])+"\"";}return t+"]}";})())<<"}],\"skins\":["<<skin.str()<<"],\"materials\":["<<mats.str()<<"]";
  if(ntex)js<<",\"textures\":["<<tex.str()<<"],\"images\":["<<img.str()<<"]";
  if(na)js<<",\"animations\":["<<anims.str()<<"]";
  js<<",\"accessors\":["<<ac.str()<<"],\"bufferViews\":["<<bv.str()<<"],\"buffers\":[{\"byteLength\":"<<bin.d.size()<<"}]}";
  std::string J=js.str();while(J.size()%4)J.push_back(' ');std::string B=bin.d;while(B.size()%4)B.push_back(0);
  std::error_code ec;std::filesystem::create_directories(std::filesystem::path(path).parent_path(),ec);std::ofstream f(path,std::ios::binary);if(!f){err="Cannot write file";return false;}
  uint32_t total=12+8+(uint32_t)J.size()+8+(uint32_t)B.size(),magic=0x46546C67,ver=2,jl=(uint32_t)J.size(),jt=0x4E4F534A,bl=(uint32_t)B.size(),bt=0x004E4942;
  f.write((char*)&magic,4);f.write((char*)&ver,4);f.write((char*)&total,4);f.write((char*)&jl,4);f.write((char*)&jt,4);f.write(J.data(),J.size());f.write((char*)&bl,4);f.write((char*)&bt,4);f.write(B.data(),B.size());
  return (bool)f;}
}
