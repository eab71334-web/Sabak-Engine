// Sabak Core - Animator studio tools: graph editor (Bezier easing), onion skin, pose library, auto pose, FX toggles, state machine, camera preview.
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "app.h"
#include "theme.h"
#include "icons.h"
namespace sk {
using namespace sk::th;
namespace fs=std::filesystem;
static const Col WHITE=Col::hex(0xffffff);
static const float OX=6,OY=217,OH=148;
static const char* TABN[5]={"Graph","Poses","FX","State","Auto"};
static const char* TRIG_N[5]={"attack","jump","hit","die","use"};
static const char* KIND_N[4]={"speed >","speed <","trigger","finished"};
static const unsigned AXC[3]={0xe5534b,0x5fb35f,0x4b92e6};
static int keyNear(const std::vector<ClipKey>& ks,float t){for(size_t i=0;i<ks.size();i++)if(std::fabs(ks[i].t-t)<0.02f)return (int)i;return -1;}
static ClipKey& putKey(std::vector<ClipKey>& ks,float t){int ki=keyNear(ks,t);if(ki<0){ClipKey nk;nk.t=t;clipEval(ks,t,nk.rot,nk.pos);ks.push_back(nk);std::sort(ks.begin(),ks.end(),[](const ClipKey& a,const ClipKey& b){return a.t<b.t;});ki=keyNear(ks,t);}return ks[ki];}
static std::string safeN(std::string s){for(char& c:s)if(!isalnum((unsigned char)c))c='_';return s;}
static float cl(float v,float a,float b){return std::max(a,std::min(b,v));}
struct Geo{float gx,gy,gw,gh,bx,by,bs;};
static Geo geo(){Geo g;g.gx=OX+10;g.gy=OY+30;g.gw=330;g.gh=OH-54;g.bx=g.gx+g.gw+14;g.by=g.gy+4;g.bs=84;return g;}
static float bzY(float v,const Geo& g){return g.by+g.bs*(1.25f-v)/1.5f;}
static float bzX(float v,const Geo& g){return g.bx+g.bs*v;}

static Vec3 xfp(const Mat4& m,Vec3 p){return {m.m[0]*p.x+m.m[4]*p.y+m.m[8]*p.z+m.m[12],m.m[1]*p.x+m.m[5]*p.y+m.m[9]*p.z+m.m[13],m.m[2]*p.x+m.m[6]*p.y+m.m[10]*p.z+m.m[14]};}

void App::aeLoadPoses(){aePoses_.clear();AePose rest;rest.name="Rest (zero)";rest.builtin=true;aePoses_.push_back(rest);Model* m=aeModel();if(!m||cur_<0||cur_>=(int)projects_.size())return;
  std::string dir=projects_[cur_].dir+"/poses",pre=safeN(aeId())+"__";std::error_code ec;if(!fs::is_directory(dir,ec))return;std::vector<std::string> files;
  for(auto& e:fs::directory_iterator(dir,ec)){std::string n=e.path().filename().string();if(n.rfind(pre,0)==0&&e.path().extension()==".pose")files.push_back(e.path().string());}std::sort(files.begin(),files.end());
  std::map<std::string,int> byName;for(size_t i=0;i<m->nodeName.size();i++)if(!byName.count(m->nodeName[i]))byName[m->nodeName[i]]=(int)i;
  for(auto& fn:files){std::ifstream f(fn);std::string line;std::getline(f,line);if(line.rfind("SABAKPOSE",0)!=0)continue;AePose p;std::getline(f,p.name);
    while(std::getline(f,line)){std::vector<std::string> q;std::string c;for(char ch:line){if(ch=='|'){q.push_back(c);c.clear();}else c+=ch;}q.push_back(c);if(q.size()>=7&&byName.count(q[0]))p.d[byName[q[0]]]={Vec3{(float)atof(q[1].c_str()),(float)atof(q[2].c_str()),(float)atof(q[3].c_str())},Vec3{(float)atof(q[4].c_str()),(float)atof(q[5].c_str()),(float)atof(q[6].c_str())}};}
    p.file=fn;aePoses_.push_back(p);}}

void App::aeOnionDraw(){if(!aeOnion_||aeCam_)return;Model* m=aeModel();Entity* e=scene_.get(sel_);if(!m||!e)return;auto it=minst_.find({&scene_,sel_});if(it==minst_.end())return;ModelInst& mi=it->second;if(mi.loco||mi.a<0||mi.a>=(int)m->anims.size())return;
  float d=m->anims[mi.a].dur;if(d<=0)return;Mat4 wm=scene_.world(e->id)*Mat4::scale({m->fit,m->fit,m->fit})*Mat4::translate({0,-m->foot,0});float s=ui.s();
  const float st[4]={-0.16f,-0.08f,0.08f,0.16f};
  for(int k=0;k<4;k++){ModelInst g=mi;g.blend=1;g.prev=-1;g.fxOff=true;float tt=mi.t+st[k]*d;if(mi.loop){tt=std::fmod(tt,d);if(tt<0)tt+=d;}else tt=cl(tt,0,d);g.t=tt;g.evaluate();
    std::vector<float> sx(m->nodeName.size(),0),sy(m->nodeName.size(),0);std::vector<char> ok(m->nodeName.size(),0);
    for(int node:m->joints){const Mat4& w=g.world[node];Vec3 p=xfp(wm,{w.m[12],w.m[13],w.m[14]});float x,y;if(r_->project(p,x,y)){sx[node]=x/s;sy[node]=y/s;ok[node]=1;}}
    Col c=st[k]<0?Col::hex(0x4b92e6,k==1?0.55f:0.30f):Col::hex(0x5fb35f,k==2?0.55f:0.30f);
    for(int node:m->joints){int pn=m->parent[node];if(pn<0||!ok[node]||!ok[pn])continue;ui.line(sx[pn],sy[pn],sx[node],sy[node],2.2f,c);}}}

void App::aeCamDraw(float W,float H){ui.rrect(0,0,W,34,0,Col::hex(0,1),Col::hex(0,1));ui.rrect(0,H-34,W,34,0,Col::hex(0,1),Col::hex(0,1));
  drawToolBtn(206,W-84,6,76,22,"Exit cam");drawToolBtn(129,W-170,6,78,22,aePlay_?"Pause":"Play");ui.text("CINEMATIC PREVIEW",14,H-24,10,TXT2,F_BOLD);}

void App::aeStudioDraw(float W,float H){(void)H;Model* m=aeModel();if(!m)return;
  for(int i=0;i<5;i++)drawToolBtn(200+i,262+i*45,8,43,24,TABN[i],aeTab_==i+1);drawToolBtn(205,262+5*45,8,43,24,"Onion",aeOnion_);drawToolBtn(206,262+6*45,8,43,24,"Cam",aeCam_);drawToolBtn(209,262+7*45,8,56,24,"Export GLB");
  if(aeTab_<=0)return;float ow=W-248;ClipDef* c=aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;
  ui.shadow(OX,OY+2,ow,OH,5,10,Col::hex(0,0.4f));ui.rrect(OX,OY,ow,OH,5,BG1.with(0.97f),BG1.with(0.97f));ui.ring(OX,OY,ow,OH,5,1,LINE2);ui.rrect(OX+1,OY+1,ow-2,22,4,BG2,BG2);
  std::string title=aeTab_==1?"GRAPH EDITOR":aeTab_==2?"POSE LIBRARY":aeTab_==3?"FX / PROCEDURAL":aeTab_==4?"STATE MACHINE":"AUTO POSE";ui.text(title,OX+10,OY+6,10,TXT2,F_BOLD);drawToolBtn(129,OX+ow-74,OY+3,68,18,aePlay_?"Pause":"Play");
  int bone=m->joints.empty()?0:m->joints[std::min<int>(aeBone_,(int)m->joints.size()-1)];
  if(aeTab_==1){Geo g=geo();ui.text(m->nodeName[bone]+"   rot X Y Z (deg)",OX+ow/2,OY+6,10,TXT3,F_REG,1);
    ui.rrect(g.gx,g.gy,g.gw,g.gh,3,BG0,BG0);ui.ring(g.gx,g.gy,g.gw,g.gh,3,1,LINE);ui.rrect(g.gx,g.gy+g.gh/2,g.gw,1,0,LINE2,LINE2);
    if(!c||c->raw){ui.text(c?"Imported clip is locked":"Create a clip (+ New)",g.gx+g.gw/2,g.gy+g.gh/2-6,11,TXT3,F_REG,1);}
    else{auto f=c->keys.find(bone);float dur=std::max(0.1f,c->dur);float vmax=15;if(f!=c->keys.end())for(auto& k:f->second)vmax=std::max(vmax,std::max(std::fabs(k.rot.x),std::max(std::fabs(k.rot.y),std::fabs(k.rot.z)))*1.2f);aeGVmax_=vmax;
      ui.text(std::to_string((int)vmax),g.gx+3,g.gy+1,9,TXT3);ui.text("-"+std::to_string((int)vmax),g.gx+3,g.gy+g.gh-11,9,TXT3);
      if(f!=c->keys.end()&&!f->second.empty()){const auto& ks=f->second;
        for(int ch=0;ch<3;ch++){float px=0,py=0;for(int i=0;i<=40;i++){float t=dur*i/40.f;Vec3 r,p;clipEval(ks,t,r,p);float v=ch==0?r.x:ch==1?r.y:r.z;float x=g.gx+g.gw*i/40.f,y=g.gy+g.gh/2-v/vmax*g.gh/2;if(i>0)ui.line(px,py,x,y,1.6f,Col::hex(AXC[ch],0.9f));px=x;py=y;}}
        for(size_t ki=0;ki<ks.size();ki++)for(int ch=0;ch<3;ch++){float v=ch==0?ks[ki].rot.x:ch==1?ks[ki].rot.y:ks[ki].rot.z;float x=g.gx+g.gw*ks[ki].t/dur,y=g.gy+g.gh/2-v/vmax*g.gh/2;bool sel=(int)ki==aeKeySel_&&ch==aeChan_;
          ui.circle(x,y,sel?5.f:3.4f,sel?WHITE:Col::hex(AXC[ch]),sel?WHITE:Col::hex(AXC[ch]));if(sel)ui.ring(x-7,y-7,14,14,7,1.2f,WHITE);}}
      else ui.text("No keys on this bone. Drag it to key it.",g.gx+g.gw/2,g.gy+g.gh/2-6,10.5f,TXT3,F_REG,1);
      float tx=g.gx+g.gw*cl(aeT_/dur,0,1);ui.rrect(tx-0.5f,g.gy,1.5f,g.gh,0,ACC,ACC);}
    // ease box
    ui.rrect(g.bx,g.by,g.bs,g.bs,3,BG0,BG0);ui.ring(g.bx,g.by,g.bs,g.bs,3,1,LINE);ui.line(bzX(0,g),bzY(0,g),bzX(1,g),bzY(1,g),1,LINE2);ui.text("EASE (next segment)",g.bx,g.by-14,9,TXT3);
    int ease=-1;float bz[4]={0.42f,0,0.58f,1};if(c&&!c->raw){auto f=c->keys.find(bone);if(f!=c->keys.end()&&aeKeySel_>=0&&aeKeySel_<(int)f->second.size()){ease=f->second[aeKeySel_].ease;for(int z=0;z<4;z++)bz[z]=f->second[aeKeySel_].bz[z];}}
    if(ease>=0){float px=0,py=0;for(int i=0;i<=24;i++){float x=i/24.f;float y=ease==5?easeBz(bz,x):easeF(ease,x);float sx=bzX(x,g),sy=bzY(y,g);if(i>0)ui.line(px,py,sx,sy,1.8f,ORG);px=sx;py=sy;}
      if(ease==5){ui.line(bzX(0,g),bzY(0,g),bzX(bz[0],g),bzY(bz[1],g),1,TXT3);ui.line(bzX(1,g),bzY(1,g),bzX(bz[2],g),bzY(bz[3],g),1,TXT3);ui.circle(bzX(bz[0],g),bzY(bz[1],g),4.5f,ACC,ACC);ui.circle(bzX(bz[2],g),bzY(bz[3],g),4.5f,ACC,ACC);}}
    else ui.text("select a key",g.bx+g.bs/2,g.by+g.bs/2-6,9.5f,TXT3,F_REG,1);
    const char* pn[6]={"Linear","Smooth","Ease in","Ease out","Step","Bezier"};for(int i=0;i<6;i++)drawToolBtn(213+i,g.bx+g.bs+10+(i%2)*60,g.by+(i/2)*30,56,26,pn[i],ease==(i==5?5:i));
    ui.text("Tap a dot to select, drag to edit. Tap empty space to scrub.",OX+10,OY+OH-13,9,TXT3);}
  else if(aeTab_==2){float x=OX+8,y=OY+30;int shown=0;for(int i=aePoseOff_;i<(int)aePoses_.size()&&shown<5;i++,shown++){bool sel=i==aePoseSel_;float ry=y+shown*24;btn(250+shown,x,ry,200,22);ui.rrect(x,ry,200,22,3,sel?ACC.with(0.28f):BG3,sel?ACC.with(0.2f):BG2);ui.ring(x,ry,200,22,3,1,sel?ACC:LINE);
      std::string nm=aePoses_[i].name;if(nm.size()>26)nm=nm.substr(0,25)+"..";ui.text(nm,x+10,ry+5,11,TXT,F_BOLD);}
    float bx=x+210,bw=ow-230;drawToolBtn(210,bx,y,bw,22,"Apply at time");drawToolBtn(211,bx,y+25,bw,22,"Save current");drawToolBtn(212,bx,y+50,bw,22,"Delete");drawToolBtn(207,bx,y+75,bw/2-2,22,"Up");drawToolBtn(208,bx+bw/2+2,y+75,bw/2-2,22,"Down");
    ui.text("Save the pose at the playhead, apply it anywhere.",OX+10,OY+OH-14,9,TXT3);}
  else if(aeTab_==3){float x=OX+8,y=OY+32,bw=(ow-26)/2;const char* nm[6]={"Gait boost","Breathing","Tail/hair spring","Foot planting","Look-At (drag)","FX preview"};
    bool on[6]={m->fx.gait,m->fx.breath,m->fx.secondary,m->fx.foot,aeLook_,aeFxPrev_||aeLook_};
    for(int i=0;i<6;i++)drawToolBtn(220+i,x+(i%2)*(bw+10),y+(i/2)*30,bw,25,nm[i],on[i]);
    ui.text("FX run on top of clips in the game. Preview is off while editing so bones match your touch.",OX+10,OY+OH-13,9,TXT3);}
  else if(aeTab_==4){float x=OX+8,y=OY+30;int ns=(int)m->smStates.size();for(int i=0;i<std::min(ns,4);i++){bool sel=i==aeSmSel_;float ry=y+i*22;btn(256+i,x,ry,148,20);ui.rrect(x,ry,148,20,3,sel?ACC.with(0.28f):BG3,sel?ACC.with(0.2f):BG2);ui.ring(x,ry,148,20,3,1,sel?ACC:LINE);ui.text(m->smStates[i].name,x+8,ry+4,10.5f,TXT,F_BOLD);}
    if(ns==0)ui.text("No states yet. Tap Auto build.",x,y+6,10,TXT3);
    float by=OY+OH-24;drawToolBtn(240,x,by,46,18,"+ State");drawToolBtn(241,x+49,by,46,18,"- State");drawToolBtn(242,x+98,by,52,18,"Auto build");
    float rx=x+158,rw=ow-176;if(ns>0&&aeSmSel_<ns){drawToolBtn(243,rx,y,18,20,"<");drawToolBtn(244,rx+rw-18,y,18,20,">");ui.text(m->smStates[aeSmSel_].clip,rx+rw/2,y+4,10.5f,ORG,F_BOLD,1);
      int shown=0;std::vector<int> tl;for(size_t i=0;i<m->smTrans.size();i++)if(m->smTrans[i].from==aeSmSel_||m->smTrans[i].from==-1)tl.push_back((int)i);
      for(size_t k=0;k<tl.size()&&shown<3;k++,shown++){const SMTrans& t=m->smTrans[tl[k]];bool sel=(int)k==aeTrSel_;float ry=y+24+shown*22;btn(260+shown,rx,ry,rw,20);ui.rrect(rx,ry,rw,20,3,sel?ACC.with(0.28f):BG3,sel?ACC.with(0.2f):BG2);ui.ring(rx,ry,rw,20,3,1,sel?ACC:LINE);
        char buf[96];std::string to=t.to>=0&&t.to<ns?m->smStates[t.to].name:"?";if(t.kind<2)snprintf(buf,sizeof buf,"-> %s  %s %.1f",to.c_str(),KIND_N[t.kind],t.val);else if(t.kind==2)snprintf(buf,sizeof buf,"-> %s  trigger %s",to.c_str(),t.trig.c_str());else snprintf(buf,sizeof buf,"-> %s  when finished",to.c_str());ui.text(buf,rx+6,ry+4,10,TXT);}
      float cy=OY+OH-24,w6=(rw-10)/6;const char* lb[6]={"Kind","-","+","To","+Tr","-Tr"};int ids[6]={245,246,247,248,249,239};for(int i=0;i<6;i++)drawToolBtn(ids[i],rx+i*(w6+2),cy,w6,18,lb[i]);}}
  else{float x=OX+8,y=OY+32,bw=(ow-26)/2;const char* nm[6]={"Mirror loop","Close loop","Hold pose","Smooth all","Linear all","Bezier all"};
    for(int i=0;i<6;i++)drawToolBtn(230+i,x+(i%2)*(bw+10),y+(i/2)*30,bw,25,nm[i]);
    ui.text("Mirror loop: keep the first half, rebuild the second as the L/R mirrored pose. Hold copies the pose forward.",OX+10,OY+OH-13,9,TXT3);}
}

bool App::aeStudioPointer(int type,int id,float x,float y){Model* m=aeModel();if(!m)return false;
  if(type==0&&aeTab_==1){float W=W_/ui.s();float ow=W-248;if(x<OX||x>OX+ow||y<OY||y>OY+OH)return false;Geo g=geo();ClipDef* c=aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;if(!c||c->raw)return true;
    int bone=m->joints.empty()?0:m->joints[std::min<int>(aeBone_,(int)m->joints.size()-1)];auto f=c->keys.find(bone);
    // ease handles
    if(f!=c->keys.end()&&aeKeySel_>=0&&aeKeySel_<(int)f->second.size()&&f->second[aeKeySel_].ease==5){ClipKey& k=f->second[aeKeySel_];for(int h=0;h<2;h++){float hx=bzX(k.bz[h*2],g),hy=bzY(k.bz[h*2+1],g);if((x-hx)*(x-hx)+(y-hy)*(y-hy)<14*14){aeGMode_=2+h;aeGId_=id;aePush();return true;}}}
    if(x>=g.gx&&x<=g.gx+g.gw&&y>=g.gy&&y<=g.gy+g.gh&&f!=c->keys.end()){float dur=std::max(0.1f,c->dur);float bd=14*14;int bk=-1,bc=0;
      for(size_t ki=0;ki<f->second.size();ki++)for(int ch=0;ch<3;ch++){const ClipKey& k=f->second[ki];float v=ch==0?k.rot.x:ch==1?k.rot.y:k.rot.z;float kx=g.gx+g.gw*k.t/dur,ky=g.gy+g.gh/2-v/aeGVmax_*g.gh/2;float d=(x-kx)*(x-kx)+(y-ky)*(y-ky);if(d<bd){bd=d;bk=(int)ki;bc=ch;}}
      if(bk>=0){aeKeySel_=bk;aeChan_=bc;aeGMode_=1;aeGId_=id;aePush();aePlay_=false;aeT_=f->second[bk].t;return true;}}
    if(x>=g.gx&&x<=g.gx+g.gw&&y>=g.gy&&y<=g.gy+g.gh){float dur=std::max(0.1f,c->dur);aePlay_=false;aeT_=cl((x-g.gx)/g.gw*dur,0,dur);aeGMode_=5;aeGId_=id;return true;}
    return false;}
  if(id!=aeGId_||aeGMode_==0)return false;
  if(type==2){aeGMode_=0;aeGId_=-1;return true;}
  ClipDef* c=aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;if(!c)return true;int bone=m->joints.empty()?0:m->joints[std::min<int>(aeBone_,(int)m->joints.size()-1)];auto f=c->keys.find(bone);Geo g=geo();float dur=std::max(0.1f,c->dur);
  if(aeGMode_==5){aeT_=cl((x-g.gx)/g.gw*dur,0,dur);return true;}
  if(f==c->keys.end()||aeKeySel_<0||aeKeySel_>=(int)f->second.size())return true;auto& ks=f->second;ClipKey& k=ks[aeKeySel_];
  if(aeGMode_==1){float v=(g.gy+g.gh/2-y)/(g.gh/2)*aeGVmax_;v=cl(v,-180,180);if(aeChan_==0)k.rot.x=v;else if(aeChan_==1)k.rot.y=v;else k.rot.z=v;
    float lo=aeKeySel_>0?ks[aeKeySel_-1].t+0.02f:0,hi=aeKeySel_+1<(int)ks.size()?ks[aeKeySel_+1].t-0.02f:dur;if(!(aeKeySel_==0&&k.t<0.001f)&&!(aeKeySel_+1==(int)ks.size()&&std::fabs(k.t-dur)<0.001f))k.t=cl((x-g.gx)/g.gw*dur,lo,hi);aeT_=k.t;m->bakeClip(aeClip_);return true;}
  if(aeGMode_==2||aeGMode_==3){int h=aeGMode_-2;k.ease=5;k.bz[h*2]=cl((x-g.bx)/g.bs,0,1);k.bz[h*2+1]=cl(1.25f-(y-g.by)/g.bs*1.5f,-0.5f,1.5f);m->bakeClip(aeClip_);return true;}
  return true;}

void App::aeStudioTap(int id){Model* m=aeModel();Entity* e=scene_.get(sel_);if(!m||!e)return;ClipDef* c=aeClip_>=0&&aeClip_<(int)m->clips.size()?&m->clips[aeClip_]:nullptr;
  int bone=m->joints.empty()?0:m->joints[std::min<int>(aeBone_,(int)m->joints.size()-1)];
  auto editable=[&]()->ClipDef*{if(!c){toast("Tap + New to create an animation");return nullptr;}if(c->raw){toast("Imported clips are locked. Tap + New and use it as Base");return nullptr;}return c;};
  if(id>=200&&id<205){int t=id-199;aeTab_=aeTab_==t?0:t;if(aeTab_==2)aeLoadPoses();aeKeySel_=-1;return;}
  if(id==205){aeOnion_=!aeOnion_;return;}
  if(id==209){std::string err,out=data_+"/Models/Exports/"+safeN(aeId())+"_animated.glb";if(exportGlb(*m,out,err))toast("Exported: Files > SabakEngine > Models > Exports");else toast("Export failed: "+err);return;}
  if(id==206){aeCam_=!aeCam_;if(aeCam_){aePlay_=true;aeTab_=0;}return;}
  if(id>=213&&id<=218){ClipDef* cc=editable();if(!cc)return;auto f=cc->keys.find(bone);if(f==cc->keys.end()||aeKeySel_<0||aeKeySel_>=(int)f->second.size()){toast("Select a key in the graph first");return;}aePush();ClipKey& k=f->second[aeKeySel_];
    k.ease=id-213;if(k.ease==5){k.bz[0]=0.42f;k.bz[1]=0;k.bz[2]=0.58f;k.bz[3]=1;}m->bakeClip(aeClip_);return;}
  // ---- poses
  if(id>=250&&id<255){int i=aePoseOff_+(id-250);if(i<(int)aePoses_.size())aePoseSel_=i;return;}
  if(id==207){aePoseOff_=std::max(0,aePoseOff_-1);return;}if(id==208){aePoseOff_=std::min(std::max(0,(int)aePoses_.size()-5),aePoseOff_+1);return;}
  if(id==210){ClipDef* cc=editable();if(!cc||aePoseSel_<0||aePoseSel_>=(int)aePoses_.size())return;aePush();const AePose& p=aePoses_[aePoseSel_];
    for(int node:m->joints){Vec3 r{0,0,0},ps{0,0,0};auto it=p.d.find(node);if(it!=p.d.end()){r=it->second.first;ps=it->second.second;}if(p.builtin||it!=p.d.end()){ClipKey& k=putKey(cc->keys[node],aeT_);k.rot=r;k.pos=ps;}}
    m->bakeClip(aeClip_);toast("Pose applied");return;}
  if(id==211){ClipDef* cc=editable();if(!cc)return;if(cur_<0||cur_>=(int)projects_.size())return;AePose p;int n=1;for(auto& q:aePoses_)if(!q.builtin)n++;p.name="Pose "+std::to_string(n);
    for(int node:m->joints){auto it=cc->keys.find(node);if(it==cc->keys.end()||it->second.empty())continue;Vec3 r,ps;clipEval(it->second,aeT_,r,ps);p.d[node]={r,ps};}
    std::string dir=projects_[cur_].dir+"/poses";std::error_code ec;fs::create_directories(dir,ec);p.file=dir+"/"+safeN(aeId())+"__"+safeN(p.name)+".pose";std::ofstream f(p.file);f<<"SABAKPOSE 1\n"<<p.name<<"\n";
    for(auto& kv:p.d)f<<m->nodeName[kv.first]<<'|'<<kv.second.first.x<<'|'<<kv.second.first.y<<'|'<<kv.second.first.z<<'|'<<kv.second.second.x<<'|'<<kv.second.second.y<<'|'<<kv.second.second.z<<'\n';
    aePoses_.push_back(p);aePoseSel_=(int)aePoses_.size()-1;aePoseOff_=std::max(0,(int)aePoses_.size()-5);toast("Pose saved");return;}
  if(id==212){if(aePoseSel_>0&&aePoseSel_<(int)aePoses_.size()&&!aePoses_[aePoseSel_].builtin){std::error_code ec;fs::remove(aePoses_[aePoseSel_].file,ec);aePoses_.erase(aePoses_.begin()+aePoseSel_);aePoseSel_=0;toast("Pose deleted");}return;}
  // ---- fx
  if(id>=220&&id<226){switch(id-220){case 0:m->fx.gait=!m->fx.gait;break;case 1:m->fx.breath=!m->fx.breath;break;case 2:m->fx.secondary=!m->fx.secondary;break;case 3:m->fx.foot=!m->fx.foot;break;case 4:aeLook_=!aeLook_;break;case 5:aeFxPrev_=!aeFxPrev_;break;}return;}
  // ---- auto pose
  if(id>=230&&id<236){ClipDef* cc=editable();if(!cc)return;aePush();float dur=std::max(0.2f,cc->dur);
    if(id==230){std::map<int,std::vector<ClipKey>> out;for(auto& kv:cc->keys){for(auto& k:kv.second)if(k.t<=dur/2+0.001f)out[kv.first].push_back(k);}
      std::map<int,std::vector<ClipKey>> add;for(auto& kv:out){int mn=m->mirrorNode(kv.first);for(auto& k:kv.second){ClipKey nk=k;nk.t=k.t+dur/2;nk.rot={k.rot.x,-k.rot.y,-k.rot.z};nk.pos={-k.pos.x,k.pos.y,k.pos.z};add[mn].push_back(nk);}}
      for(auto& kv:add)for(auto& k:kv.second){auto& v=out[kv.first];int ki=keyNear(v,k.t);if(ki<0)v.push_back(k);}
      for(auto& kv:out){auto& v=kv.second;std::sort(v.begin(),v.end(),[](const ClipKey& a,const ClipKey& b){return a.t<b.t;});if(!v.empty()){ClipKey first=v.front();first.t=dur;if(keyNear(v,dur)<0)v.push_back(first);}}
      cc->keys=out;toast("Second half built from the mirrored pose");}
    else if(id==231){for(auto& kv:cc->keys){if(kv.second.empty())continue;ClipKey first=kv.second.front();for(auto& k:kv.second)if(k.t<first.t)first=k;first.t=dur;int ki=keyNear(kv.second,dur);if(ki>=0)kv.second.erase(kv.second.begin()+ki);kv.second.push_back(first);std::sort(kv.second.begin(),kv.second.end(),[](const ClipKey& a,const ClipKey& b){return a.t<b.t;});}toast("Loop closed");}
    else if(id==232){float t2=std::min(dur,aeT_+0.4f);for(auto& kv:cc->keys){Vec3 r,p;clipEval(kv.second,aeT_,r,p);ClipKey& k=putKey(kv.second,t2);k.rot=r;k.pos=p;}toast("Pose held for 0.4s");}
    else{int e2=id==233?1:id==234?0:5;for(auto& kv:cc->keys)for(auto& k:kv.second){k.ease=e2;if(e2==5){k.bz[0]=0.42f;k.bz[1]=0;k.bz[2]=0.58f;k.bz[3]=1;}}toast("Easing applied to all keys");}
    m->bakeClip(aeClip_);return;}
  // ---- state machine
  int ns=(int)m->smStates.size();
  auto animNames=[&](){std::vector<std::string> v;for(auto& a:m->anims)if(!a.name.empty())v.push_back(a.name);return v;};
  if(id==240){auto v=animNames();m->smStates.push_back({"State "+std::to_string(ns+1),v.empty()?"idle":v[0]});aeSmSel_=ns;return;}
  if(id==241){if(ns>0&&aeSmSel_<ns){int d=aeSmSel_;m->smStates.erase(m->smStates.begin()+d);std::vector<SMTrans> nt;for(auto t:m->smTrans){if(t.from==d||t.to==d)continue;if(t.from>d)t.from--;if(t.to>d)t.to--;nt.push_back(t);}m->smTrans=nt;aeSmSel_=std::max(0,d-1);}return;}
  if(id==242){m->smStates.clear();m->smTrans.clear();auto has=[&](const char* n){return m->animIndex(n)>=0;};int idl=-1,wk=-1,rn=-1;
    if(has("idle")){idl=(int)m->smStates.size();m->smStates.push_back({"Idle","idle"});}if(has("walk")){wk=(int)m->smStates.size();m->smStates.push_back({"Walk","walk"});}if(has("run")){rn=(int)m->smStates.size();m->smStates.push_back({"Run","run"});}
    if(idl>=0&&wk>=0){m->smTrans.push_back({idl,wk,0,0.5f,""});m->smTrans.push_back({wk,idl,1,0.3f,""});}if(wk>=0&&rn>=0){m->smTrans.push_back({wk,rn,0,4.f,""});m->smTrans.push_back({rn,wk,1,3.5f,""});}
    for(const char* nm:{"attack","jump","hit"}){if(has(nm)&&idl>=0){int s=(int)m->smStates.size();std::string N=nm;N[0]=(char)toupper(N[0]);m->smStates.push_back({N,nm});m->smTrans.push_back({-1,s,2,0.f,nm});m->smTrans.push_back({s,idl,3,0.f,""});}}
    aeSmSel_=0;toast("State machine built. Set the model anim to StateMachine");return;}
  if(ns==0||aeSmSel_>=ns)return;
  if(id>=256&&id<260){int i=id-256;if(i<ns)aeSmSel_=i,aeTrSel_=0;return;}
  if(id==243||id==244){auto v=animNames();if(v.empty())return;int cur=0;for(size_t i=0;i<v.size();i++)if(v[i]==m->smStates[aeSmSel_].clip)cur=(int)i;cur=(cur+(id==244?1:-1)+(int)v.size())%(int)v.size();m->smStates[aeSmSel_].clip=v[cur];return;}
  std::vector<int> tl;for(size_t i=0;i<m->smTrans.size();i++)if(m->smTrans[i].from==aeSmSel_||m->smTrans[i].from==-1)tl.push_back((int)i);
  if(id>=260&&id<263){int k=id-260;if(k<(int)tl.size())aeTrSel_=k;return;}
  if(id==249){SMTrans t;t.from=aeSmSel_;t.to=(aeSmSel_+1)%ns;t.kind=0;t.val=1.f;m->smTrans.push_back(t);aeTrSel_=(int)tl.size();return;}
  if(aeTrSel_<0||aeTrSel_>=(int)tl.size())return;SMTrans& t=m->smTrans[tl[aeTrSel_]];
  if(id==239){m->smTrans.erase(m->smTrans.begin()+tl[aeTrSel_]);aeTrSel_=0;return;}
  if(id==245){t.kind=(t.kind+1)%4;if(t.kind==2&&t.trig.empty())t.trig=TRIG_N[0];return;}
  if(id==246||id==247){if(t.kind<2)t.val=std::max(0.f,t.val+(id==247?0.5f:-0.5f));else if(t.kind==2){int i=0;for(int k=0;k<5;k++)if(t.trig==TRIG_N[k])i=k;i=(i+(id==247?1:4))%5;t.trig=TRIG_N[i];}return;}
  if(id==248){t.to=(t.to+1)%ns;return;}
}
}
