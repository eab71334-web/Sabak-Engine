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
    patchF([&](const SkinVertex& s,const std::string& n){bool torso=starts(n,"spine")||starts(n,"clavicle");bool arm=starts(n,"upperarm")||(longSl&&starts(n,"lowerarm"));
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
        float base=cd.hair==3?0.0025f:cd.hair==2?0.016f:0.0115f;float top=sstep((s.p[1]-eyeY)/0.12f);return std::max(0.0022f,(base*(0.5f+0.5f*top)+(cd.hair==3?0.f:cl))*(0.5f+0.5f*w));},hair);}
  for(int sd=0;sd<2;sd++){Vec3 e=sd?eR:eL;float sgn=sd?-1.f:1.f;patchF([&](const SkinVertex& s,const std::string&){float dx=(s.p[0]-e.x)*sgn;float dy=s.p[1]-e.y-0.026f-0.005f*std::max(0.f,1.f-std::fabs(dx+0.002f)/0.026f)-0.003f*std::max(0.f,-dx/0.026f);if(s.p[2]<e.z+0.004f)return -1.f;float u=dx/0.027f,v=dy/(0.0075f*(1.f-0.35f*std::max(0.f,u)));float q=1.f-(u*u+v*v);return q>0.f?q:-1.f;},[&](const SkinVertex&,float w){return 0.0010f+0.0014f*std::min(1.f,w*2.f);},brow);}
  // eyes: white ball + iris
  auto sphere=[&](Mb& m,Vec3 c,Vec3 rr,int jt){uint32_t base=(uint32_t)m.v.size();int seg=16,rings=10;for(int j=0;j<=rings;j++){float ph=3.14159265f*j/rings;for(int i=0;i<=seg;i++){float th=6.2831853f*i/seg;SkinVertex s{};Vec3 n{std::sin(ph)*std::cos(th),std::cos(ph),std::sin(ph)*std::sin(th)};s.p[0]=c.x+n.x*rr.x;s.p[1]=c.y+n.y*rr.y;s.p[2]=c.z+n.z*rr.z;s.n[0]=n.x;s.n[1]=n.y;s.n[2]=n.z;s.uv[0]=(float)i/seg;s.uv[1]=(float)j/rings;s.j[0]=(unsigned char)jt;s.w[0]=1;m.v.push_back(s);}}
    for(int j=0;j<rings;j++)for(int i=0;i<seg;i++){uint32_t a=base+j*(seg+1)+i,b=a+1,c2=a+seg+1,dd=c2+1;m.idx.push_back(a);m.idx.push_back(c2);m.idx.push_back(b);m.idx.push_back(b);m.idx.push_back(c2);m.idx.push_back(dd);}};
  (void)jEyeR;for(int sd=0;sd<2;sd++){Vec3 e=sd?eR:eL;sphere(eyeW,e,{0.0122f,0.0122f,0.0122f},jHead);sphere(iris,{e.x,e.y,e.z+0.0090f},{0.0066f,0.0066f,0.0042f},jHead);}
  // assemble
  std::vector<unsigned char> white{255,255,255,255};unsigned wt=r.createTexture(white.data(),1,1);
  struct Part{Mb* m;unsigned col;};unsigned skin=cd.col[0];
  auto darker=[&](unsigned c,float k){return ((unsigned)(((c>>16)&255)*k)<<16)|((unsigned)(((c>>8)&255)*k)<<8)|(unsigned)((c&255)*k);};
  Part parts[8]={{&body,skin},{&shirt,cd.col[2]},{&pants,cd.col[3]},{&shoes,cd.col[4]},{&hair,cd.col[1]},{&brow,darker(cd.col[1],0.8f)},{&eyeW,0xf2f2ee},{&iris,0x4a2f1c}};
  for(int i=0;i<8;i++){Mb& m=*parts[i].m;if(m.idx.empty())continue;if(i>0&&i<6&&i!=0)normals(m);ModelMat mm;mm.tex=wt;mm.kind=(i==0?1:i<=3?2:i<=5?3:4);unsigned c=parts[i].col;mm.col[0]=((c>>16)&255)/255.f;mm.col[1]=((c>>8)&255)/255.f;mm.col[2]=(c&255)/255.f;mm.col[3]=1;int mi=(int)out.mats.size();out.mats.push_back(mm);
    ModelPrim mp;mp.mesh=r.uploadSkin(m.v,m.idx);mp.mat=mi;mp.cv=m.v;mp.ci=m.idx;out.prims.push_back(std::move(mp));}
  // expression morphs (on the body prim = prim 0)
  struct Mix{const char* name;std::vector<std::pair<const char*,float>> u;};
  std::vector<Mix> mixes={{"Smile",{{"mouth-corner-puller",1.f},{"mouth-elevation",0.25f}}},{"MouthOpen",{{"mouth-open",1.f}}},{"Blink",{{"eye-left-closure",1.f},{"eye-right-closure",1.f}}},
    {"BrowUp",{{"eyebrows-left-up",1.f},{"eyebrows-right-up",1.f}}},{"Angry",{{"eyebrows-left-down",1.f},{"eyebrows-right-down",1.f},{"mouth-compression",0.5f},{"eye-left-slit",0.3f},{"eye-right-slit",0.3f}}},
    {"Sad",{{"eyebrows-left-inner-up",1.f},{"eyebrows-right-inner-up",1.f},{"mouth-depression",0.7f}}}};
  ModelPrim& bp=out.prims[0];bp.md.assign(mixes.size(),std::vector<float>(bp.cv.size()*3,0.f));out.morphNames.clear();
  for(size_t k=0;k<mixes.size();k++){out.morphNames.push_back(mixes[k].name);std::vector<float> dl(d.nV*3,0.f);for(auto& u:mixes[k].u)applyT(d,dl,std::string("x/")+u.first,u.second);
    for(size_t i=0;i<bp.cv.size();i++){uint32_t o=d.oOrig[i];for(int c=0;c<3;c++){float val=dl[o*3+c]*0.1f;if(flip&&c==0)val=-val;bp.md[k][i*3+c]=val;}}}
  out.morphW.assign(mixes.size(),0.f);for(int k=0;k<6;k++)out.morphW[k]=cd.ex[k];out.applyMorphs(r);
  float rad=0.5f;out.fit=cd.height/Hh;out.foot=0;out.height=cd.height;out.radius=std::max(0.5f,rad*out.fit);out.autoRig=true;
  Anim lo;lo.name="Locomotion";lo.dur=1;out.anims.push_back(lo);out.builtin=(int)out.anims.size();return true;}
}
