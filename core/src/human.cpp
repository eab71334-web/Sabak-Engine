// Sabak Core - realistic human generator: deforms the CC0 MakeHuman base mesh (assets/human.bin) with macro + detail targets, skins it, adds eyes/brows/hair/clothes.
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <map>
#include "model.h"
namespace sk {
const HumanSlider kHumanSliders[24]={
 {"Head width","head/head-scale-horiz","decr","incr",false},{"Head depth","head/head-scale-depth","decr","incr",false},{"Head height","head/head-scale-vert","decr","incr",false},
 {"Chin width","chin/chin-width","decr","incr",false},{"Chin forward","chin/chin-prominent","decr","incr",false},{"Chin height","chin/chin-height","decr","incr",false},
 {"Nose size","nose/nose-volume","decr","incr",false},{"Nose width","nose/nose-width1","decr","incr",false},{"Nose length","nose/nose-scale-vert","decr","incr",false},{"Nose bridge","nose/nose-hump","decr","incr",false},
 {"Eye size","eyes/%-eye-scale","decr","incr",true},{"Eye height","eyes/%-eye-height1","decr","incr",true},{"Eye distance","eyes/%-eye-trans","in","out",true},
 {"Upper lip","mouth/mouth-upperlip-volume","decr","incr",false},{"Lower lip","mouth/mouth-lowerlip-volume","decr","incr",false},{"Mouth width","mouth/mouth-scale-horiz","decr","incr",false},
 {"Ear size","ears/%-ear-scale","decr","incr",true},{"Cheek bones","cheek/%-cheek-bones","decr","incr",true},{"Cheek volume","cheek/%-cheek-volume","decr","incr",true},
 {"Forehead","forehead/forehead-scale-vert","decr","incr",false},{"Neck width","neck/neck-scale-horiz","decr","incr",false},{"Chest","torso/torso-muscle-pectoral","decr","incr",false},
 {"V-shape","torso/torso-vshape","decr","incr",false},{"Belly","stomach/stomach-pregnant","decr","incr",false}};
namespace {
struct Tgt{float sc=0;std::vector<uint16_t> i;std::vector<int16_t> d;};
struct HBone{std::string name;int parent;std::vector<uint32_t> head,tail;};
struct HData{bool ok=false;uint32_t nV=0;std::vector<float> base;std::vector<uint32_t> oOrig;std::vector<float> oUV;std::vector<uint32_t> idx;std::vector<HBone> bones;std::vector<unsigned char> wj,ww;std::map<std::string,Tgt> T;};
HData& hd(){static HData d;static bool tried=false;if(tried)return d;tried=true;
  std::ifstream f(g_assetDir+"/human.bin",std::ios::binary);if(!f)return d;std::string buf((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());size_t p=0;
  auto rd=[&](void* dst,size_t n){if(p+n>buf.size()){p=buf.size();return;}std::memcpy(dst,buf.data()+p,n);p+=n;};
  auto u32=[&](){uint32_t v=0;rd(&v,4);return v;};auto str=[&](){unsigned char l=0;rd(&l,1);std::string s(l,' ');if(l)rd(&s[0],l);return s;};
  char mg[4];rd(mg,4);if(std::memcmp(mg,"SKHM",4))return d;u32();d.nV=u32();d.base.resize(d.nV*3);rd(d.base.data(),d.nV*12);
  uint32_t no=u32();d.oOrig.resize(no);d.oUV.resize(no*2);for(uint32_t i=0;i<no;i++){d.oOrig[i]=u32();rd(&d.oUV[i*2],8);}
  uint32_t ni=u32();d.idx.resize(ni);rd(d.idx.data(),ni*4);
  uint32_t nb=u32();d.bones.resize(nb);for(auto& b:d.bones){b.name=str();int16_t pa;rd(&pa,2);b.parent=pa;uint32_t n=u32();b.head.resize(n);rd(b.head.data(),n*4);n=u32();b.tail.resize(n);rd(b.tail.data(),n*4);}
  d.wj.resize(13380*4);d.ww.resize(13380*4);for(int v=0;v<13380;v++){rd(&d.wj[v*4],4);rd(&d.ww[v*4],4);}
  uint32_t nt=u32();for(uint32_t t=0;t<nt;t++){std::string n=str();Tgt g;uint32_t c=u32();rd(&g.sc,4);g.i.resize(c);g.d.resize(c*3);for(uint32_t k=0;k<c;k++){rd(&g.i[k],2);rd(&g.d[k*3],6);}d.T[n]=std::move(g);}
  d.ok=p<=buf.size()&&!d.bones.empty();return d;}
void applyT(const HData& d,std::vector<float>& pos,const std::string& n,float w){if(w<1e-4f)return;auto it=d.T.find(n);if(it==d.T.end())return;const Tgt& t=it->second;float k=t.sc/32767.f*w;for(size_t e=0;e<t.i.size();e++){size_t v=t.i[e];pos[v*3]+=t.d[e*3]*k;pos[v*3+1]+=t.d[e*3+1]*k;pos[v*3+2]+=t.d[e*3+2]*k;}}
void applySlider(const HData& d,std::vector<float>& pos,const HumanSlider& s,float v){float w=(v-0.5f)*2.f;if(std::fabs(w)<1e-3f)return;const char* sides[2]={"l","r"};
  for(int sd=0;sd<(s.sym?2:1);sd++){std::string base=s.pat;size_t q=base.find('%');if(q!=std::string::npos)base.replace(q,1,sides[sd]);applyT(d,pos,base+"-"+(w<0?s.neg:s.pos),std::fabs(w));}}
struct Mb{std::vector<SkinVertex> v;std::vector<uint32_t> idx;};
}
bool humanAvailable(){return hd().ok;}
bool buildHuman(Renderer& r,const CharDef& cd,Model& out){
  const HData& d=hd();if(!d.ok)return false;std::vector<float> pos=d.base;
  float g=std::min(1.f,std::max(0.f,cd.hv[0])),ao=std::min(1.f,std::max(0.f,cd.hv[1]));float mus=cd.hv[2],wei=cd.hv[3];
  auto tri3=[](float x,float* o){o[0]=x<0.5f?1-2*x:0;o[1]=x<0.5f?2*x:2-2*x;o[2]=x<0.5f?0:2*x-1;};
  float mw[3],ww[3];tri3(mus,mw);tri3(wei,ww);const char* mn[3]={"minmuscle","averagemuscle","maxmuscle"};const char* wn[3]={"minweight","averageweight","maxweight"};
  const char* gn[2]={"female","male"};const char* an[2]={"young","old"};float gw[2]={1-g,g},aw[2]={1-ao,ao};
  for(int a=0;a<2;a++)for(int s=0;s<2;s++)for(int m=0;m<3;m++)for(int w=0;w<3;w++)applyT(d,pos,std::string("u-")+gn[s]+"-"+an[a]+"-"+mn[m]+"-"+wn[w],gw[s]*aw[a]*mw[m]*ww[w]);
  float rw[3]={cd.hv[5],cd.hv[6],cd.hv[7]};float rs=rw[0]+rw[1]+rw[2];if(rs<1e-3f){rw[0]=rw[1]=0;rw[2]=1;rs=1;}const char* rn[3]={"african","asian","caucasian"};
  for(int k=0;k<3;k++)for(int a=0;a<2;a++)for(int s=0;s<2;s++)applyT(d,pos,std::string("r-")+rn[k]+"-"+gn[s]+"-"+an[a],rw[k]/rs*gw[s]*aw[a]);
  for(int k=0;k<24;k++)applySlider(d,pos,kHumanSliders[k],cd.hv[8+k]);
  // metres, feet on the ground, centred
  for(auto& x:pos)x*=0.1f;float mnY=1e9f,mxY=-1e9f,cx=0;for(int v=0;v<13380;v++){mnY=std::min(mnY,pos[v*3+1]);mxY=std::max(mxY,pos[v*3+1]);}
  for(int b=0;b<(int)d.bones.size();b++)if(d.bones[b].name=="root"){double sx=0;for(uint32_t v:d.bones[b].head)sx+=pos[v*3];cx=(float)(sx/d.bones[b].head.size());}
  for(uint32_t v=0;v<d.nV;v++){pos[v*3]-=cx;pos[v*3+1]-=mnY;}
  float Hh=std::max(0.5f,mxY-mnY);bool flip=false;
  auto joint=[&](const std::vector<uint32_t>& vs){Vec3 c{0,0,0};for(uint32_t v:vs){c.x+=pos[v*3];c.y+=pos[v*3+1];c.z+=pos[v*3+2];}float n=(float)std::max<size_t>(1,vs.size());return Vec3{c.x/n,c.y/n,c.z/n};};
  for(auto& b:d.bones)if(b.name=="upperarm01.L"&&joint(b.head).x<0)flip=true;
  if(flip)for(uint32_t v=0;v<d.nV;v++)pos[v*3]=-pos[v*3];
  // skeleton
  size_t nb=d.bones.size();out=Model();out.name=cd.name;out.nodeName.resize(nb);out.parent.resize(nb);out.rest.resize(nb);std::vector<Vec3> hp(nb);
  for(size_t i=0;i<nb;i++){out.nodeName[i]=d.bones[i].name;out.parent[i]=d.bones[i].parent;hp[i]=joint(d.bones[i].head);}
  for(size_t i=0;i<nb;i++){TRS t;int pa=d.bones[i].parent;t.t={hp[i].x-(pa>=0?hp[pa].x:0),hp[i].y-(pa>=0?hp[pa].y:0),hp[i].z-(pa>=0?hp[pa].z:0)};out.rest[i]=t;out.joints.push_back((int)i);out.ibm.push_back(Mat4::translate({-hp[i].x,-hp[i].y,-hp[i].z}));}
  auto bidx=[&](const char* n){for(size_t i=0;i<nb;i++)if(d.bones[i].name==n)return (int)i;return 0;};int jHead=bidx("head"),jEyeL=bidx("eye.L"),jEyeR=bidx("eye.R");
  // body mesh
  Mb body;size_t no=d.oOrig.size();body.v.resize(no);
  for(size_t i=0;i<no;i++){SkinVertex& s=body.v[i];uint32_t o=d.oOrig[i];s.p[0]=pos[o*3];s.p[1]=pos[o*3+1];s.p[2]=pos[o*3+2];s.uv[0]=d.oUV[i*2];s.uv[1]=1-d.oUV[i*2+1];s.n[0]=0;s.n[1]=1;s.n[2]=0;
    for(int k=0;k<4;k++){s.j[k]=d.wj[o*4+k];s.w[k]=d.ww[o*4+k]/255.f;}}
  body.idx=d.idx;if(flip)for(size_t i=0;i+2<body.idx.size();i+=3)std::swap(body.idx[i+1],body.idx[i+2]);
  auto normals=[&](Mb& m){std::vector<Vec3> nn(m.v.size(),Vec3{0,0,0});for(size_t i=0;i+2<m.idx.size();i+=3){auto P=[&](uint32_t k){return Vec3{m.v[k].p[0],m.v[k].p[1],m.v[k].p[2]};};Vec3 a=P(m.idx[i]),b=P(m.idx[i+1]),c=P(m.idx[i+2]);Vec3 e1=b-a,e2=c-a;Vec3 f{e1.y*e2.z-e1.z*e2.y,e1.z*e2.x-e1.x*e2.z,e1.x*e2.y-e1.y*e2.x};for(int k=0;k<3;k++)nn[m.idx[i+k]]=nn[m.idx[i+k]]+f;}
    // weld normals of vertices sharing the same source position (UV seams)
    std::map<std::array<int,3>,Vec3> acc;auto key=[&](const SkinVertex& s){return std::array<int,3>{(int)std::lround(s.p[0]*10000),(int)std::lround(s.p[1]*10000),(int)std::lround(s.p[2]*10000)};};
    for(size_t i=0;i<m.v.size();i++){auto& a=acc[key(m.v[i])];a=a+nn[i];}
    for(size_t i=0;i<m.v.size();i++){Vec3 n=acc[key(m.v[i])];float l=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);if(l<1e-9f){m.v[i].n[0]=0;m.v[i].n[1]=1;m.v[i].n[2]=0;}else{m.v[i].n[0]=n.x/l;m.v[i].n[1]=n.y/l;m.v[i].n[2]=n.z/l;}}};
  normals(body);
  // helpers: dominant bone of each out-vertex
  auto dom=[&](const SkinVertex& s){int bi=0;float bw=-1;for(int k=0;k<4;k++)if(s.w[k]>bw){bw=s.w[k];bi=s.j[k];}return bi;};
  auto nameOf=[&](int b){return d.bones[b].name;};
  Vec3 eL=joint(d.bones[jEyeL].head),eR=joint(d.bones[jEyeR].head);float eyeY=(eL.y+eR.y)*0.5f,headZ=(eL.z+eR.z)*0.5f;
  auto patch=[&](auto pred,float off,Vec3 col,int cat,Mb& dst){// copy triangles whose vertices all satisfy pred, pushed out along the normal
    std::map<uint32_t,uint32_t> mp;for(size_t i=0;i+2<body.idx.size();i+=3){bool ok=true;for(int k=0;k<3;k++){const SkinVertex& s=body.v[body.idx[i+k]];if(!pred(s,nameOf(dom(s)))){ok=false;break;}}if(!ok)continue;
      for(int k=0;k<3;k++){uint32_t o=body.idx[i+k];auto it=mp.find(o);if(it==mp.end()){SkinVertex s=body.v[o];for(int c=0;c<3;c++)s.p[c]+=s.n[c]*off;mp[o]=(uint32_t)dst.v.size();dst.v.push_back(s);it=mp.find(o);}dst.idx.push_back(it->second);}}
    (void)col;(void)cat;};
  Mb shirt,pants,shoes,hair,brow,eyeW,iris;
  auto starts=[](const std::string& n,const char* p){return n.rfind(p,0)==0;};
  auto sstep=[](float x){x=std::min(1.f,std::max(0.f,x));return x*x*(3-2*x);};
  // weighted patch: region weight w in (0,1] tapers to 0 at the borders so edges sit flush on the skin (no jagged cut line); offset function gives thickness
  auto patchF=[&](auto wf,auto of,Mb& dst){std::map<uint32_t,uint32_t> mp;std::vector<float> wv(body.v.size(),-1.f);
    for(size_t i=0;i<body.v.size();i++){const SkinVertex& s=body.v[i];wv[i]=wf(s,nameOf(dom(s)));}
    for(size_t i=0;i+2<body.idx.size();i+=3){if(wv[body.idx[i]]<=0||wv[body.idx[i+1]]<=0||wv[body.idx[i+2]]<=0)continue;
      for(int k=0;k<3;k++){uint32_t o=body.idx[i+k];auto it=mp.find(o);if(it==mp.end()){SkinVertex s=body.v[o];float t=of(s,wv[o]);for(int c=0;c<3;c++)s.p[c]+=s.n[c]*t;mp[o]=(uint32_t)dst.v.size();dst.v.push_back(s);it=mp.find(o);}dst.idx.push_back(it->second);}}};
  Vec3 hNeck=hp[bidx("neck01")],hUA=hp[bidx("upperarm01.L")],hLA=hp[bidx("lowerarm01.L")],hWr=hp[bidx("wrist.L")];
  float armEnd=std::fabs(hUA.x)+(std::fabs(hLA.x)-std::fabs(hUA.x))*0.55f;
  float armEndLong=std::fabs(hWr.x)-0.02f;
  float sleeveEnd=cd.outfit==1?std::fabs(hUA.x)*0.9f:armEnd;
  bool longSl=cd.outfit==3;if(longSl)sleeveEnd=armEndLong;
  float waistY=hp[bidx("spine05")].y;
  if(cd.outfit!=2){
    float fold=1.f;
    patchF([&](const SkinVertex& s,const std::string& n){bool torso=starts(n,"spine")||starts(n,"clavicle")||n=="root";bool arm=starts(n,"upperarm")||(longSl&&starts(n,"lowerarm"));
        if(!torso&&!arm&&n!="neck01")return -1.f;float hemY=waistY-0.10f;float w=sstep((s.p[1]-hemY)/0.05f);
        if(n=="neck01"||s.p[1]>hNeck.y-0.012f)w=std::min(w,sstep((hNeck.y+0.03f-s.p[1])/0.03f));
        if(arm||starts(n,"clavicle")){float ax=std::fabs(s.p[0]);w=std::min(w,sstep((sleeveEnd-ax)/0.035f));}
        if(cd.outfit==1&&arm)return -1.f; return w;},
      [&](const SkinVertex& s,float w){float f=0.0016f*std::sin(s.p[1]*95.f+s.p[0]*38.f)+0.0010f*std::sin(s.p[1]*41.f-s.p[0]*57.f+s.p[2]*20.f);
        float loose=0.0045f+0.0035f*sstep((hNeck.y-0.18f-s.p[1])/0.2f)*(s.p[1]<waistY+0.15f?1.f:0.5f);return std::max(0.0030f,(loose+f*fold)*(0.6f+0.4f*w));},shirt);
    patchF([&](const SkinVertex& s,const std::string& n){bool leg=starts(n,"upperleg")||starts(n,"lowerleg");bool pel=n=="root"||starts(n,"spine05");if(!leg&&!pel)return -1.f;
        float topY=waistY+0.01f;if(s.p[1]>topY)return -1.f;float w=sstep((topY-s.p[1])/0.03f);
        float botY=cd.outfit==1?Hh*0.27f:Hh*0.06f;w=std::min(w,sstep((s.p[1]-botY)/0.04f));if(s.p[1]<botY)return -1.f;return std::max(w,0.001f);},
      [&](const SkinVertex& s,float w){float f=0.0013f*std::sin(s.p[1]*80.f+s.p[0]*45.f)+0.0008f*std::sin(s.p[1]*33.f+s.p[2]*60.f);return std::max(0.0028f,(0.0040f+0.0018f*sstep((waistY-s.p[1])/0.4f)+f)*(0.6f+0.4f*w));},pants);
    patchF([&](const SkinVertex& s,const std::string& n){if(!starts(n,"foot")&&!starts(n,"toe"))return -1.f;float lim=Hh*0.07f;if(s.p[1]>=lim)return -1.f;return std::max(0.001f,sstep((lim-s.p[1])/0.03f));},
      [&](const SkinVertex&,float w){return 0.0032f*(0.7f+0.3f*w);},shoes);}
  // hair: tapered cap with a real hairline, side/back coverage, clumped strand ripples
  if(cd.hair>0){
    patchF([&](const SkinVertex& s,const std::string& n){bool hd2=n=="head"||n=="jaw"||(cd.hair==2&&n=="neck01");if(!hd2)return -1.f;
        float dz=s.p[2]-headZ;float ax=std::fabs(s.p[0]);
        float hl=eyeY+0.052f+(dz>0.02f?0.012f:0.f)+(ax>0.07f&&dz>-0.01f?0.015f:0.f);       // forehead & temple hairline
        float w=sstep((s.p[1]-hl)/0.028f);
        if(cd.hair==2){float back=sstep((-dz-0.015f)/0.03f);float low=eyeY-0.17f;float wb=back*sstep((s.p[1]-low)/0.04f)*(ax<0.075f?1.f:0.f);w=std::max(w,wb);}
        else if(cd.hair==1){float back=sstep((-dz-0.03f)/0.03f);float wb=back*sstep((s.p[1]-(eyeY-0.03f))/0.03f)*(ax<0.07f?1.f:0.f);w=std::max(w,wb);} // nape
        if(cd.hair==3)w=std::min(w,sstep((s.p[1]-(eyeY+0.07f))/0.02f));
        return w;},
      [&](const SkinVertex& s,float w){float ang=std::atan2(s.p[0],s.p[2]-headZ);float cl=0.0022f*std::sin(ang*26.f+s.p[1]*55.f)+0.0015f*std::sin(ang*53.f-s.p[1]*31.f);
        float base=cd.hair==3?0.0025f:cd.hair==2?0.007f:0.0035f;float top=sstep((s.p[1]-eyeY)/0.12f);return std::max(0.0022f,(base*(0.5f+0.5f*top)+(cd.hair==3?0.f:cl))*(0.5f+0.5f*w));},hair);}
  for(int sd=0;sd<2;sd++){Vec3 e=sd?eR:eL;float sgn=sd?-1.f:1.f;patchF([&](const SkinVertex& s,const std::string&){float dx=(s.p[0]-e.x)*sgn;float dy=s.p[1]-e.y-0.026f-0.005f*std::max(0.f,1.f-std::fabs(dx+0.002f)/0.026f)-0.003f*std::max(0.f,-dx/0.026f);if(s.p[2]<e.z+0.004f)return -1.f;float u=dx/0.027f,v=dy/(0.0068f*(1.f-0.40f*std::max(0.f,u)));float q=1.f-(u*u+v*v);(void)q;return -1.f;},[&](const SkinVertex&,float w){return 0.0010f+0.0014f*std::min(1.f,w*2.f);},brow);}
  // ---- realistic hair + brows: strand cards (alpha-cut strand texture, Kajiya-Kay shading, sway morphs)
  Mb cardsU,cardsO[3],cardsF,cardsB,strands[9];std::vector<float> tipU,tipO[3],tipF,tipB,tipS[9];
  struct V3{float x,y,z;};
  auto vsub=[](V3 a,V3 b){return V3{a.x-b.x,a.y-b.y,a.z-b.z};};auto vadd=[](V3 a,V3 b){return V3{a.x+b.x,a.y+b.y,a.z+b.z};};auto vmul=[](V3 a,float s){return V3{a.x*s,a.y*s,a.z*s};};
  auto vdot=[](V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};auto vcr=[](V3 a,V3 b){return V3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
  auto vnorm=[&](V3 a){float l=std::sqrt(vdot(a,a));return l<1e-9f?V3{0,1,0}:vmul(a,1.f/l);};
  unsigned seed=cd.hair*7919u+13u;auto rnd=[&](){seed=seed*1664525u+1013904223u;return ((seed>>8)&0xffff)/65535.f;};
  V3 hc{0.f,eyeY+0.010f,headZ-0.020f};
  // ribbon from a path: side vectors face the viewer; uv.x in [u0,u1] picks a strand sub-range of the texture
  auto buildCard=[&](Mb& dst,std::vector<float>& tip,const std::vector<V3>& path,const std::vector<V3>& outN,float wid,float u0,float u1,float taper,float aoBase=1.f,float aoRoot=0.6f){
    int K=(int)path.size();uint32_t base=(uint32_t)dst.v.size();
    for(int i=0;i<K;i++){V3 dd=i+1<K?vsub(path[i+1],path[i]):vsub(path[i],path[i-1]);V3 sd=vnorm(vcr(dd,outN[i]));float f=(float)i/(K-1);float ww=wid*(1.f-taper*f*f)*0.5f;
      for(int s=0;s<2;s++){SkinVertex sv{};V3 q=vadd(path[i],vmul(sd,(s?1.f:-1.f)*ww));sv.p[0]=q.x;sv.p[1]=q.y;sv.p[2]=q.z;sv.n[0]=outN[i].x;sv.n[1]=outN[i].y;sv.n[2]=outN[i].z;sv.uv[0]=s?u1:u0;sv.uv[1]=f;sv.j[0]=(unsigned char)jHead;sv.w[0]=1;{float ao=aoBase*(aoRoot+(1.f-aoRoot)*std::min(1.f,f*2.5f));unsigned char c8=(unsigned char)(std::min(1.f,ao)*255.f);sv.c[0]=sv.c[1]=sv.c[2]=c8;sv.c[3]=255;}dst.v.push_back(sv);tip.push_back(std::pow(f,1.5f));}}
    for(int i=0;i+1<K;i++){uint32_t a=base+i*2,b=a+1,c=a+2,e=a+3;dst.idx.push_back(a);dst.idx.push_back(c);dst.idx.push_back(b);dst.idx.push_back(b);dst.idx.push_back(c);dst.idx.push_back(e);}};
  if(cd.hair==1||cd.hair==2||cd.hair==4){
    std::vector<float> cum;std::vector<int> triIdx;float tot=0;
    for(size_t i=0;i+2<hair.idx.size();i+=3){const SkinVertex &a=hair.v[hair.idx[i]],&b=hair.v[hair.idx[i+1]],&c=hair.v[hair.idx[i+2]];
      V3 e1{b.p[0]-a.p[0],b.p[1]-a.p[1],b.p[2]-a.p[2]},e2{c.p[0]-a.p[0],c.p[1]-a.p[1],c.p[2]-a.p[2]};V3 cr=vcr(e1,e2);tot+=0.5f*std::sqrt(vdot(cr,cr));cum.push_back(tot);triIdx.push_back((int)i);}
    V3 crown{0.f,eyeY+0.085f,headZ-0.035f};
    auto samp=[&](V3& P,V3& Nn){float r=rnd()*tot;size_t lo=0,hi=cum.size()-1;while(lo<hi){size_t mid=(lo+hi)/2;if(cum[mid]<r)lo=mid+1;else hi=mid;}
      int ti=triIdx[lo];const SkinVertex &A=hair.v[hair.idx[ti]],&B=hair.v[hair.idx[ti+1]],&Cc=hair.v[hair.idx[ti+2]];float u=rnd(),v=rnd();if(u+v>1){u=1-u;v=1-v;}float w=1-u-v;
      P=V3{A.p[0]*w+B.p[0]*u+Cc.p[0]*v,A.p[1]*w+B.p[1]*u+Cc.p[1]*v,A.p[2]*w+B.p[2]*u+Cc.p[2]*v};
      Nn=vnorm(V3{A.n[0]*w+B.n[0]*u+Cc.n[0]*v,A.n[1]*w+B.n[1]*u+Cc.n[1]*v,A.n[2]*w+B.n[2]*u+Cc.n[2]*v});};
    auto keepOutside=[&](V3 np,float margin){V3 q=vsub(np,hc);V3 qs{q.x/0.088f,q.y/0.108f,q.z/0.108f};float ql=std::sqrt(vdot(qs,qs));float R=1.0f+margin;if(ql<R){float k2=R/std::max(ql,1e-3f);np=V3{hc.x+q.x*k2,hc.y+q.y*k2,hc.z+q.z*k2};}return np;};
    // ---- groom: guide strands (the "style") -> thousands of child strands with clumping, frizz and depth layers
    const int K=cd.hair==2?10:8;const int NG=cum.empty()?0:(cd.hair==2?420:560);
    struct Guide{V3 root,Nn;std::vector<V3> path;float len;};std::vector<Guide> guides;
    for(int n=0;n<NG;n++){V3 P,Nn;samp(P,Nn);
      float rel=P.y-eyeY;float top=sstep((rel-0.02f)/0.11f);bool front=P.z>headZ+0.012f;bool backNape=P.z<headZ-0.035f;
      float len;if(cd.hair==2){len=0.12f+0.14f*(0.35f+0.65f*(backNape?1.f:top))+0.02f*(rnd()-0.5f);}else{len=0.036f+0.036f*top;if(front&&top>0.4f)len*=1.20f;if(backNape&&rel<0.0f)len*=0.8f;len+=0.008f*(rnd()-0.5f);}len*=cd.hairLen;
      V3 fl=vsub(P,crown);fl=vsub(fl,vmul(Nn,vdot(fl,Nn)));fl=vnorm(fl);
      V3 sweep{front?0.85f:0.32f,0.f,0.f};fl=vnorm(vadd(fl,vmul(sweep,top)));
      float g=cd.hair==2?0.85f:(front?0.12f:0.36f);V3 d=vnorm(vadd(vmul(Nn,0.20f),vmul(fl,0.90f)));
      float ymin=front?eyeY+0.047f:(std::fabs(P.x)>0.055f&&P.z>headZ-0.03f)?eyeY-0.030f:eyeY-0.115f;
      Guide G;G.root=vadd(P,vmul(Nn,0.0015f));G.Nn=Nn;G.len=len;G.path.resize(K);G.path[0]=G.root;float seg=len/(K-1);float curl=(rnd()-0.5f)*0.10f;
      for(int i=1;i<K;i++){float f=(float)i/(K-1);d=vnorm(vadd(d,vadd(vmul(fl,0.10f),V3{curl*std::sin(f*4.f+n),-g*std::pow(f,1.2f)*0.55f,curl*std::cos(f*3.f)})));
        V3 np=keepOutside(vadd(G.path[i-1],vmul(d,seg)),0.008f);if(np.y<ymin)np.y=ymin+(np.y-ymin)*0.12f;G.path[i]=np;}
      guides.push_back(G);}
    static const float tintMul[3]={0.80f,1.0f,1.38f};
    int NCH=guides.empty()?0:(int)((cd.hair==2?17000:24000)*cd.hairDens);
    for(int n=0;n<NCH;n++){V3 P,Nn;samp(P,Nn);
      float rel=P.y-eyeY;bool front=P.z>headZ+0.012f;if(front&&rel<0.044f&&rnd()<0.80f)continue;
      int gi=0;float bd=1e9f;for(int q=0;q<(int)guides.size();q++){V3 dv=vsub(guides[q].root,P);float dd=vdot(dv,dv)*(0.7f+0.6f*((q*2654435761u>>8&255)/255.f)*0.0f+0.f);if(dd<bd){bd=dd;gi=q;}}
      const Guide& G=guides[gi];
      float rd=rnd();int depth=rd<0.30f?0:(rd<0.65f?1:2);
      float lenS=(depth==0?0.62f:depth==1?0.88f:1.08f)*(0.90f+0.2f*rnd());
      float tr=rnd();int tint=tr<0.30f?0:(tr<0.85f?1:2);int grp=depth*3+tint;
      V3 off=vsub(P,G.root);float clump=(0.75f+(0.08f-0.75f)*cd.hairClump)*(0.8f+0.4f*rnd());
      std::vector<V3> path(K),on(K);float frz=(cd.hair==2?0.0030f:0.0016f)*cd.hairFrizz;float ph=rnd()*6.28f;float ph2=rnd()*6.28f;float curlA=(rnd()<0.12f+0.88f*cd.hairCurl)?0.0030f+0.0050f*cd.hairCurl:0.f;float cf=14.f+10.f*cd.hairCurl;
      for(int i=0;i<K;i++){float f=(float)i/(K-1);V3 gp=vadd(G.root,vmul(vsub(G.path[i],G.root),lenS));
        float oFac=1.f-(1.f-clump)*std::pow(f,0.8f);V3 q=vadd(gp,vmul(off,oFac));
        q=vadd(q,V3{frz*f*std::sin(ph+f*7.f)+curlA*f*std::sin(f*cf+ph2),frz*f*std::sin(ph2+f*5.f),frz*f*std::cos(ph+f*6.f)+curlA*f*std::cos(f*cf+ph2)});
        q=keepOutside(q,0.0045f+0.0032f*depth);path[i]=q;}
      path[0]=vadd(P,vmul(Nn,0.0008f+0.0016f*depth));
      for(int i=0;i<K;i++)on[i]=vnorm(vsub(path[i],hc));
      buildCard(strands[grp],tipS[grp],path,on,0.00135f*(0.8f+0.4f*rnd()),0.f,1.f,0.65f,depth==0?0.40f:depth==1?0.70f:1.0f,0.50f);
    }
    // flyaways + hairline wisps: single thin strands that break the silhouette
    int NF=cum.empty()?0:(cd.hair==2?420:330);
    for(int n=0;n<NF;n++){V3 P,Nn;samp(P,Nn);float rel=P.y-eyeY;bool wisp=n%3==0;if(wisp&&(P.z<headZ+0.01f||rel>0.075f))continue;
      V3 fl=vsub(P,crown);fl=vsub(fl,vmul(Nn,vdot(fl,Nn)));fl=vnorm(vadd(vnorm(fl),V3{wisp?0.9f:0.2f,0.f,0.f}));
      V3 rv{rnd()-0.5f,rnd()-0.5f,rnd()-0.5f};V3 d=vnorm(vadd(vadd(vmul(Nn,wisp?0.30f:0.55f),vmul(fl,0.6f)),vmul(V3{rv.x,rv.y*0.5f-0.1f,rv.z},wisp?0.5f:0.9f)));
      int KK=6;float len=wisp?0.014f+0.016f*rnd():0.022f+0.026f*rnd();float seg=len/(KK-1);std::vector<V3> path(KK),on(KK);path[0]=vadd(P,vmul(Nn,0.003f));
      for(int i=1;i<KK;i++){float f=(float)i/(KK-1);d=vnorm(vadd(d,V3{0.f,-0.25f*f,0.f}));V3 np=keepOutside(vadd(path[i-1],vmul(d,seg)),0.006f);
        if(wisp&&np.y<eyeY+0.047f)np.y=eyeY+0.047f+(np.y-eyeY-0.047f)*0.1f;path[i]=np;}
      for(int i=0;i<KK;i++)on[i]=vnorm(vsub(path[i],hc));
      buildCard(strands[4+(n%2)],tipS[4+(n%2)],path,on,0.0016f,0.f,1.f,0.6f);}
  }
  // eyebrows: individual short hairs along the brow arch, growing up/inward at the head and flattening outward
  {std::vector<V3> faceV;for(size_t i=0;i<body.v.size();i++){const SkinVertex& s=body.v[i];if(s.p[2]>headZ-0.01f&&std::fabs(s.p[1]-eyeY)<0.12f&&std::fabs(s.p[0])<0.12f)faceV.push_back(V3{s.p[0],s.p[1],s.p[2]});}
   auto surfZ=[&](float x,float y,float& z){float sw=0,sz=0;for(const V3& q:faceV){float dx=q.x-x,dy=q.y-y;float dd=std::sqrt(dx*dx+dy*dy);if(dd<0.008f){float w=1.f-dd/0.008f;w*=w;sw+=w;sz+=w*q.z;}}if(sw<1e-5f)return false;z=sz/sw;return true;};
   for(int sd=0;sd<2;sd++){Vec3 e=sd?eR:eL;float sgn=sd?-1.f:1.f;
     for(int n=0;n<(int)(260*cd.browK);n++){float t=-0.88f+1.88f*std::pow(rnd(),0.85f);float dx=t*0.029f;
       float yc=e.y+0.0165f+0.004f*std::max(0.f,1.f-std::fabs(dx+0.002f)/0.026f)+0.003f*std::max(0.f,-dx/0.026f);
       float h=0.0085f*(1.f-std::pow(std::fabs(t),3.f))*(t>0?1.f-0.45f*t:1.f);float y0=yc+(rnd()-0.5f)*h;float x0=e.x+dx*sgn;
       float ang=78.f+(-8.f-78.f)*((t+1.f)*0.5f)+ (rnd()-0.5f)*18.f;float a=ang*3.14159265f/180.f;
       float len=0.011f+0.006f*rnd()+0.002f*(1.f-std::fabs(t));
       std::vector<V3> path(4),on(4);bool ok=true;
       for(int i=0;i<4;i++){float f=(float)i/3.f;float aa=a-0.45f*f*(a>0?1.f:0.f)-0.25f*f;float x=x0+sgn*std::cos(aa)*len*f;float y=y0+std::sin(aa)*len*f;float z;if(!surfZ(x,y,z)){ok=false;break;}path[i]=V3{x,y,z+0.0007f+0.0006f*f};on[i]=V3{0.f,0.f,1.f};}
       if(!ok)continue;buildCard(cardsB,tipB,path,on,0.0036f*(0.75f+0.5f*cd.browK),0.f,1.f,0.45f,0.95f,0.55f);}}}
  // strand texture: many fine strands per card, root-dark -> lighter tips, strongly varied brightness, tapering to points (alpha-cut)
  unsigned hairTex=0;
  {const int TW=512,TH=1024,S=26;std::vector<unsigned char> img((size_t)TW*TH*4);unsigned sd=99991u;auto rr=[&](){sd=sd*1664525u+1013904223u;return ((sd>>8)&0xffff)/65535.f;};
   struct St{float c,ph,fr,am,hw,len,br;};std::vector<St> st(S+2);
   for(int k=0;k<S+2;k++){st[k]={(k-0.5f)/S+0.012f*(rr()-0.5f),rr()*6.28f,2.f+4.f*rr(),0.008f+0.016f*rr(),0.34f/S*(0.8f+0.5f*rr()),0.60f+0.40f*rr(),0.45f+0.85f*rr()};}
   for(int y=0;y<TH;y++){float v=(y+0.5f)/TH;for(int x=0;x<TW;x++){float u=(x+0.5f)/TW;float a=0,br=0;
       int k0=(int)std::floor(u*S+0.5f);for(int k=std::max(0,k0-1);k<=std::min(S+1,k0+1);k++){const St& s=st[k];if(v>s.len)continue;float cx=s.c+s.am*std::sin(v*s.fr+s.ph);float tap=1.f-std::pow(v/s.len,2.2f)*0.92f;float hw=s.hw*tap*(v<0.03f?0.6f+v*13.f:1.f);float dd=std::fabs(u-cx);float cov=1.f-std::min(1.f,std::max(0.f,(dd-hw*0.50f)/(hw*0.50f+1e-5f)));
         if(cov>a){a=cov;br=s.br*(1.f-0.35f*dd/(hw+1e-5f)*0.5f);}}
       float root=0.42f+0.58f*sstep(v/0.40f);float tipL=1.f+0.18f*sstep((v-0.65f)/0.35f);float val=std::min(1.f,br*root*tipL);
       unsigned char* q=&img[((size_t)y*TW+x)*4];q[0]=q[1]=q[2]=(unsigned char)(val*255.f);q[3]=(unsigned char)(a>0.5f?255:0);}}
   hairTex=r.createTexture(img.data(),TW,TH);}
  unsigned stripTex=0;
  {const int W=32,H=256;std::vector<unsigned char> im((size_t)W*H*4);for(int y=0;y<H;y++){float v=(y+0.5f)/H;for(int x=0;x<W;x++){float u=(x+0.5f)/W;float c=std::fabs(2.f*u-1.f);float round_=1.f-0.30f*c*c;float root=0.50f+0.50f*sstep(v/0.45f);float tipL=1.f+0.15f*sstep((v-0.7f)/0.3f);float val=std::min(1.f,round_*root*tipL);unsigned char* q=&im[((size_t)y*W+x)*4];q[0]=q[1]=q[2]=(unsigned char)(val*255.f);q[3]=(unsigned char)(255.f*std::min(1.f,std::max(0.f,(0.94f-c)/0.28f)));}}stripTex=r.createTexture(im.data(),W,H);}
  // eyes: white ball + iris
  auto sphere=[&](Mb& m,Vec3 c,Vec3 rr,int jt){uint32_t base=(uint32_t)m.v.size();int seg=16,rings=10;for(int j=0;j<=rings;j++){float ph=3.14159265f*j/rings;for(int i=0;i<=seg;i++){float th=6.2831853f*i/seg;SkinVertex s{};Vec3 n{std::sin(ph)*std::cos(th),std::cos(ph),std::sin(ph)*std::sin(th)};s.p[0]=c.x+n.x*rr.x;s.p[1]=c.y+n.y*rr.y;s.p[2]=c.z+n.z*rr.z;s.n[0]=n.x;s.n[1]=n.y;s.n[2]=n.z;s.uv[0]=(float)i/seg;s.uv[1]=(float)j/rings;s.j[0]=(unsigned char)jt;s.w[0]=1;m.v.push_back(s);}}
    for(int j=0;j<rings;j++)for(int i=0;i<seg;i++){uint32_t a=base+j*(seg+1)+i,b=a+1,c2=a+seg+1,dd=c2+1;m.idx.push_back(a);m.idx.push_back(c2);m.idx.push_back(b);m.idx.push_back(b);m.idx.push_back(c2);m.idx.push_back(dd);}};
  (void)jEyeR;for(int sd=0;sd<2;sd++){Vec3 e=sd?eR:eL;sphere(eyeW,e,{0.0122f,0.0122f,0.0122f},jHead);sphere(iris,{e.x,e.y,e.z+0.0090f},{0.0066f,0.0066f,0.0042f},jHead);}
  // ---- mouth interior: dark cavity, upper/lower teeth, tongue (lower parts follow the jaw via the same morph deltas)
  Mb teethU,teethL,tongue,cavity;Vec3 lipC{0,0,0};float lipFront=0;Vec3 jawD{0,0,0};bool haveMouth=false;
  {auto itc=d.T.find("x/mouth-compression"),ito=d.T.find("x/mouth-open");
   if(itc!=d.T.end()&&ito!=d.T.end()){const Tgt& tc=itc->second;float mx=0;std::vector<float> mag(tc.i.size());
     for(size_t e=0;e<tc.i.size();e++){float a=tc.d[e*3],b2=tc.d[e*3+1],c2=tc.d[e*3+2];mag[e]=std::sqrt(a*a+b2*b2+c2*c2);mx=std::max(mx,mag[e]);}
     double sx=0,sy=0,sz=0;int n=0;float zmax=-1e9f;
     for(size_t e=0;e<tc.i.size();e++){if(mag[e]<0.55f*mx)continue;uint32_t v=tc.i[e];float x=pos[v*3],y=pos[v*3+1],z=pos[v*3+2];if(std::fabs(x)>0.05f)continue;sx+=x;sy+=y;sz+=z;n++;if(std::fabs(x)<0.008f)zmax=std::max(zmax,z);}
     if(n>6){lipC={(float)(sx/n),(float)(sy/n),(float)(sz/n)};lipFront=zmax>-1e8f?zmax:lipC.z;haveMouth=true;
       // jaw displacement = mean open-delta of vertices around the lower lip / chin front
       const Tgt& to=ito->second;float k=to.sc/32767.f;double dx=0,dy=0,dz=0;int m=0;
       for(size_t e=0;e<to.i.size();e++){uint32_t v=to.i[e];float x=pos[v*3],y=pos[v*3+1],z=pos[v*3+2];if(std::fabs(x)<0.012f&&y<lipC.y&&y>lipC.y-0.03f&&z>lipC.z-0.02f){dx+=to.d[e*3]*k;dy+=to.d[e*3+1]*k;dz+=to.d[e*3+2]*k;m++;}}
       if(m>0)jawD={(float)(dx/m)*0.1f,(float)(dy/m)*0.1f,(float)(dz/m)*0.1f};}}}
  if(std::getenv("SABAK_HDBG"))fprintf(stderr,"mouth ok=%d lip=(%.3f %.3f %.3f) front=%.3f eyeY=%.3f jawD=(%.4f %.4f %.4f)\n",haveMouth,lipC.x,lipC.y,lipC.z,lipFront,eyeY,jawD.x,jawD.y,jawD.z);
  if(haveMouth){
    float depth=std::max(0.02f,lipFront-lipC.z+0.022f);float zc=lipFront-0.013f;   // mouth centre plane just behind the lips
    auto ell=[&](Mb& m,Vec3 c,Vec3 rr){sphere(m,c,rr,jHead);};
    // cavity: big dark ellipsoid behind the lips (inside-facing look comes from dark matte colour)
    ell(cavity,{lipC.x,lipC.y,zc-0.040f},{0.034f,0.020f,0.040f});
    // teeth arch: ellipsoids along a curve
    const int NU=12,NL=12;
    for(int i=0;i<NU;i++){float u=(i+0.5f)/NU*2.f-1.f;float x=u*0.0285f;float w=(1.f-0.35f*std::fabs(u));float z=zc-0.0085f-0.020f*u*u;
      ell(teethU,{lipC.x+x,lipC.y+0.0022f,z},{0.0024f*w+0.0006f,0.0062f*w+0.0010f,0.0020f});}
    for(int i=0;i<NL;i++){float u=(i+0.5f)/NL*2.f-1.f;float x=u*0.0260f;float w=(1.f-0.30f*std::fabs(u));float z=zc-0.0075f-0.018f*u*u;
      ell(teethL,{lipC.x+x,lipC.y-0.0042f,z},{0.0020f*w+0.0006f,0.0052f*w+0.0010f,0.0018f});}
    // gums
    ell(teethU,{lipC.x,lipC.y+0.0105f,zc-0.0125f},{0.029f,0.0040f,0.0075f});ell(teethL,{lipC.x,lipC.y-0.0120f,zc-0.0115f},{0.026f,0.0040f,0.0070f});
    // tongue: flat ellipsoid on the lower jaw
    ell(tongue,{lipC.x,lipC.y-0.0095f,zc-0.0150f},{0.0185f,0.0058f,0.0195f});
  }
  // ---- facial hair (Hair Studio): stubble / short / full beard / moustache, grown from face triangles
  Mb beardS[2];std::vector<float> tipBd[2];
  if(haveMouth&&cd.beard>0){
    auto sm=[&](float a,float b,float x){float t=std::min(1.f,std::max(0.f,(x-a)/(b-a)));return t*t*(3-2*t);};
    int st=cd.beard;  // 1 stubble 2 short 3 full 4 moustache
    auto zoneW=[&](float x,float y,float z)->float{float ax=std::fabs(x),dy=y-lipC.y;if(z<lipC.z-0.085f)return 0.f;float w=0.f;
      float mous=(dy>0.003f&&dy<0.024f&&ax<0.031f)?(1.f-sm(0.018f,0.031f,ax))*sm(0.003f,0.008f,dy)*(1.f-sm(0.018f,0.024f,dy)):0.f;
      float chin=(dy<-0.010f&&dy>-0.074f&&ax<0.042f)?sm(-0.010f,-0.020f,dy)*(1.f-sm(-0.062f,-0.074f,dy))*(1.f-sm(0.026f,0.042f,ax)):0.f;
      float cheek=(dy>-0.070f&&dy<0.038f&&ax>0.022f&&ax<0.082f)?sm(0.020f,0.034f,ax)*(1.f-sm(0.060f,0.082f,ax))*(1.f-sm(0.010f,0.038f,dy))*sm(-0.070f,-0.040f,dy):0.f;
      float lipsClear=(std::fabs(dy)<0.008f&&ax<0.027f)?0.f:1.f;
      if(st==4)w=mous;else if(st==2)w=std::max(chin,std::max(mous,cheek*0.55f));else w=std::max(chin,std::max(mous,cheek));
      return w*lipsClear;};
    std::vector<float> cum;std::vector<int> tris;float tot=0;
    for(size_t i=0;i+2<body.idx.size();i+=3){const SkinVertex &a=body.v[body.idx[i]],&b=body.v[body.idx[i+1]],&c2=body.v[body.idx[i+2]];
      float cx=(a.p[0]+b.p[0]+c2.p[0])/3,cy=(a.p[1]+b.p[1]+c2.p[1])/3,cz=(a.p[2]+b.p[2]+c2.p[2])/3;float w=zoneW(cx,cy,cz);if(w<0.04f)continue;
      V3 e1{b.p[0]-a.p[0],b.p[1]-a.p[1],b.p[2]-a.p[2]},e2{c2.p[0]-a.p[0],c2.p[1]-a.p[1],c2.p[2]-a.p[2]};V3 cr=vcr(e1,e2);tot+=0.5f*std::sqrt(vdot(cr,cr))*w;cum.push_back(tot);tris.push_back((int)i);}
    int N=cum.empty()?0:(st==1?15000:st==2?12000:st==3?16000:4200);
    for(int n=0;n<N;n++){float r0=rnd()*tot;size_t lo=0,hi=cum.size()-1;while(lo<hi){size_t mid=(lo+hi)/2;if(cum[mid]<r0)lo=mid+1;else hi=mid;}
      int ti=tris[lo];const SkinVertex &A=body.v[body.idx[ti]],&B=body.v[body.idx[ti+1]],&Cc=body.v[body.idx[ti+2]];float u=rnd(),v=rnd();if(u+v>1){u=1-u;v=1-v;}float w=1-u-v;
      V3 P{A.p[0]*w+B.p[0]*u+Cc.p[0]*v,A.p[1]*w+B.p[1]*u+Cc.p[1]*v,A.p[2]*w+B.p[2]*u+Cc.p[2]*v};
      V3 Nn=vnorm(V3{A.n[0]*w+B.n[0]*u+Cc.n[0]*v,A.n[1]*w+B.n[1]*u+Cc.n[1]*v,A.n[2]*w+B.n[2]*u+Cc.n[2]*v});
      float zw=zoneW(P.x,P.y,P.z);if(rnd()>zw)continue;
      float dy=P.y-lipC.y;bool mous=dy>0.0f;
      float len=st==1?0.0013f+0.0008f*rnd():st==2?(0.009f+0.005f*rnd()):st==3?(mous?0.016f:(dy<-0.030f?0.032f:0.020f))*(0.8f+0.4f*rnd()):(0.011f+0.004f*rnd());
      V3 down{0.f,-1.f,0.f};V3 dir=vsub(down,vmul(Nn,vdot(down,Nn)));
      if(mous)dir=vadd(dir,V3{(P.x>0?1.f:-1.f)*0.55f,-0.2f,0.f});else dir=vadd(dir,V3{-P.x*5.f,0.f,0.f});
      dir=vnorm(vadd(vmul(vnorm(dir),0.8f),vmul(Nn,st==1?0.55f:0.30f)));
      if(st==3&&dy<-0.030f)dir=vnorm(vadd(dir,V3{0.f,-0.2f,0.25f}));
      const int KK=4;std::vector<V3> path(KK),on(KK,Nn);path[0]=vadd(P,vmul(Nn,0.0004f));V3 d=dir;float seg=len/(KK-1);
      for(int i=1;i<KK;i++){float f=(float)i/(KK-1);d=vnorm(vadd(d,V3{0.2f*(rnd()-0.5f),-0.15f*f*(st>=2?1.f:0.2f),0.2f*(rnd()-0.5f)}));path[i]=vadd(path[i-1],vmul(d,seg));}
      int g=rnd()<0.5f?0:1;buildCard(beardS[g],tipBd[g],path,on,st==1?0.0009f:0.0013f,0.f,1.f,0.6f,0.85f,0.45f);}
  }
  // assemble
  std::vector<unsigned char> white{255,255,255,255};unsigned wt=r.createTexture(white.data(),1,1);
  struct Part{Mb* m;unsigned col;};unsigned skin=cd.col[0];
  auto darker=[&](unsigned c,float k){return ((unsigned)(((c>>16)&255)*k)<<16)|((unsigned)(((c>>8)&255)*k)<<8)|(unsigned)((c&255)*k);};
  Part parts[8]={{&body,skin},{&shirt,cd.col[2]},{&pants,cd.col[3]},{&shoes,cd.col[4]},{&hair,darker(cd.col[1],0.45f)},{&brow,cd.col[1]},{&eyeW,0xf2f2ee},{&iris,0x4a2f1c}};
  for(int i=0;i<8;i++){Mb& m=*parts[i].m;if(m.idx.empty())continue;if(i>0&&i<6&&i!=0)normals(m);ModelMat mm;mm.tex=wt;mm.kind=(i==0?1:i<=3?2:i<=5?3:4);unsigned c=parts[i].col;mm.col[0]=((c>>16)&255)/255.f;mm.col[1]=((c>>8)&255)/255.f;mm.col[2]=(c&255)/255.f;mm.col[3]=1;int mi=(int)out.mats.size();out.mats.push_back(mm);
    ModelPrim mp;mp.mesh=r.uploadSkin(m.v,m.idx);mp.mat=mi;mp.cv=m.v;mp.ci=m.idx;out.prims.push_back(std::move(mp));}
  // expression morphs (on the body prim = prim 0)
  struct Mix{const char* name;std::vector<std::pair<const char*,float>> u;};
  std::vector<Mix> mixes={{"Smile",{{"mouth-corner-puller",1.f},{"mouth-elevation",0.25f}}},{"MouthOpen",{{"mouth-open",1.f}}},{"Blink",{{"eye-left-closure",1.f},{"eye-right-closure",1.f}}},
    {"BrowUp",{{"eyebrows-left-up",1.f},{"eyebrows-right-up",1.f}}},{"Angry",{{"eyebrows-left-down",1.f},{"eyebrows-right-down",1.f},{"mouth-compression",0.5f},{"eye-left-slit",0.3f},{"eye-right-slit",0.3f}}},
    {"Sad",{{"eyebrows-left-inner-up",1.f},{"eyebrows-right-inner-up",1.f},{"mouth-depression",0.7f}}},
    // lip-sync visemes (indices 6..11): driven live by the voice mouth track
    {"vOpen",{{"mouth-open",1.f}}},{"vPucker",{{"mouth-pursing",1.f},{"mouth-protusion",0.55f}}},{"vWide",{{"mouth-corner-puller",0.55f},{"mouth-retraction",0.7f}}},
    {"vPress",{{"mouth-compression",1.f}}},{"vUpTeeth",{{"mouth-upward-retraction",1.f}}},{"vLowLip",{{"mouth-depression-retraction",1.f}}},{"vTongue",{}},{"vSwayX",{}},{"vSwayZ",{}}};
  ModelPrim& bp=out.prims[0];bp.md.assign(mixes.size(),std::vector<float>(bp.cv.size()*3,0.f));out.morphNames.clear();
  for(size_t k=0;k<mixes.size();k++){out.morphNames.push_back(mixes[k].name);std::vector<float> dl(d.nV*3,0.f);for(auto& u:mixes[k].u)applyT(d,dl,std::string("x/")+u.first,u.second);
    for(size_t i=0;i<bp.cv.size();i++){uint32_t o=d.oOrig[i];for(int c=0;c<3;c++){float val=dl[o*3+c]*0.1f;if(flip&&c==0)val=-val;bp.md[k][i*3+c]=val;}}}
  // mouth-interior prims (after the body morphs so prim 0 stays the body)
  if(haveMouth){
    auto addExtra=[&](Mb& m,Vec3 col,int kind,std::vector<std::pair<int,Vec3>> mv){if(m.idx.empty())return;ModelMat mm;mm.tex=wt;mm.kind=kind;mm.col[0]=col.x;mm.col[1]=col.y;mm.col[2]=col.z;mm.col[3]=1;int mi=(int)out.mats.size();out.mats.push_back(mm);
      ModelPrim mp;mp.mesh=r.uploadSkin(m.v,m.idx);mp.mat=mi;mp.cv=m.v;mp.ci=m.idx;mp.md.assign(mixes.size(),std::vector<float>());
      for(auto& q:mv){std::vector<float>& a=mp.md[q.first];a.assign(m.v.size()*3,0.f);for(size_t i=0;i<m.v.size();i++){a[i*3]=q.second.x;a[i*3+1]=q.second.y;a[i*3+2]=q.second.z;}}
      out.prims.push_back(std::move(mp));};
    Vec3 none{0,0,0};(void)none;
    addExtra(cavity,{0.09f,0.02f,0.03f},9,{{6,jawD*0.35f}});
    addExtra(teethU,{0.90f,0.87f,0.78f},9,{});
    addExtra(teethL,{0.88f,0.85f,0.76f},9,{{6,jawD}});
    addExtra(tongue,{0.62f,0.24f,0.25f},1,{{6,jawD*0.7f},{12,Vec3{0.f,0.0075f,0.006f}}});
  }
  {auto addCards=[&](Mb& m,std::vector<float>& tip,Vec3 col,float swayK,unsigned tx=0,int kd=16){if(m.idx.empty())return;ModelMat mm;mm.tex=tx?tx:hairTex;mm.kind=kd;mm.mask=true;mm.col[0]=std::min(1.f,col.x);mm.col[1]=std::min(1.f,col.y);mm.col[2]=std::min(1.f,col.z);mm.col[3]=1;int mi=(int)out.mats.size();out.mats.push_back(mm);
      ModelPrim mp;mp.mesh=r.uploadSkin(m.v,m.idx);mp.mat=mi;mp.cv=m.v;mp.ci=m.idx;mp.md.assign(mixes.size(),std::vector<float>());
      if(swayK>0&&false){float sw=(cd.hair==2?0.030f:0.010f)*swayK;std::vector<float>& ax=mp.md[13];std::vector<float>& az=mp.md[14];ax.assign(m.v.size()*3,0.f);az.assign(m.v.size()*3,0.f);
        for(size_t i=0;i<m.v.size();i++){float t=tip[i];ax[i*3]=sw*t;ax[i*3+1]=-sw*0.25f*t;az[i*3+2]=sw*t;az[i*3+1]=-sw*0.15f*t;}}
      out.prims.push_back(std::move(mp));};
   Vec3 hc0{((cd.col[1]>>16)&255)/255.f,((cd.col[1]>>8)&255)/255.f,(cd.col[1]&255)/255.f};
   static const float tm[3]={0.80f,1.0f,1.38f},am[3]={0.55f,0.85f,1.10f};
   for(int g=0;g<9;g++)addCards(strands[g],tipS[g],hc0*(tm[g%3]*am[g/3]),0.f,stripTex);
   addCards(cardsB,tipB,hc0*1.0f,0.f,0,17);addCards(beardS[0],tipBd[0],hc0*0.75f,0.f,stripTex,17);addCards(beardS[1],tipBd[1],hc0*1.05f,0.f,stripTex,17);}
  out.morphW.assign(mixes.size(),0.f);for(int k=0;k<6;k++)out.morphW[k]=cd.ex[k];out.applyMorphs(r);
  float rad=0.5f;out.fit=cd.height/Hh;out.foot=0;out.height=cd.height;out.radius=std::max(0.5f,rad*out.fit);out.autoRig=true;
  Anim lo;lo.name="Locomotion";lo.dur=1;out.anims.push_back(lo);out.builtin=(int)out.anims.size();return true;}
}
