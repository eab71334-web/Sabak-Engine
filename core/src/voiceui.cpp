// Sabak Core - Voice Studio: type / pick a line, choose a voice, hear it, watch the lips move.
#include "app.h"
#include "theme.h"
#include <cmath>
#include <algorithm>
namespace sk {
using namespace th;
static const unsigned HAIR_COLS[6]={0x241812,0x35210f,0x4a2f1e,0x8a6a3a,0x9a4a22,0x9a9a98};
static const char* VOICE_NAMES[5]={"Male","Female","Child","Old man","Phone voice"};
static const char* VC_PRESET_LABEL[6]={"Welcome","Lesson","Question","Arabic: Morning","Arabic: Lesson","Arabic: Open books"};
static const char* VC_PRESET_TEXT[6]={
  "Good morning students. Welcome to our class.",
  "Today we will learn about science. Please open your books, and listen carefully.",
  "Do you understand? Who has a question?",
  "\xd8\xb5\xd9\x8e\xd8\xa8\xd9\x8e\xd8\xa7\xd8\xad\xd9\x8f \xd8\xa7\xd9\x84\xd8\xae\xd9\x8e\xd9\x8a\xd9\x92\xd8\xb1\xd9\x90 \xd9\x8a\xd9\x8e\xd8\xa7 \xd8\xb7\xd9\x8f\xd9\x84\xd9\x8e\xd9\x91\xd8\xa7\xd8\xa8\xd9\x8f",
  "\xd8\xa7\xd9\x84\xd9\x8a\xd9\x8e\xd9\x88\xd9\x92\xd9\x85\xd9\x8e \xd8\xb3\xd9\x8e\xd9\x86\xd9\x8e\xd8\xaa\xd9\x8e\xd8\xb9\xd9\x8e\xd9\x84\xd9\x8e\xd9\x91\xd9\x85\xd9\x8f \xd8\xaf\xd9\x8e\xd8\xb1\xd9\x92\xd8\xb3\xd9\x8b\xd8\xa7 \xd8\xac\xd9\x8e\xd8\xaf\xd9\x90\xd9\x8a\xd8\xaf\xd9\x8b\xd8\xa7",
  "\xd8\xa7\xd9\x81\xd9\x92\xd8\xaa\xd9\x8e\xd8\xad\xd9\x8f\xd9\x88\xd8\xa7 \xd9\x83\xd9\x8f\xd8\xaa\xd9\x8f\xd8\xa8\xd9\x8e\xd9\x83\xd9\x8f\xd9\x85\xd9\x92 \xd9\x88\xd9\x8e\xd8\xa7\xd8\xb3\xd9\x92\xd9\x85\xd9\x8e\xd8\xb9\xd9\x8f\xd9\x88\xd8\xa7"};
static bool hasArabic(const std::string& s){for(size_t i=0;i+1<s.size();i++){unsigned char c=s[i];if(c==0xD8||c==0xD9)return true;}return false;}

bool App::wantsText() const {return scr_==VOICE&&vcFocus_;}
void App::textInput(const std::string& s){if(scr_!=VOICE||!vcFocus_)return;if(vcText_.size()<220)vcText_+=s;vcPreset_=-1;}
void App::keyCode(int c){if(scr_!=VOICE)return;if(c==1&&!vcText_.empty()){size_t n=vcText_.size()-1;while(n>0&&(vcText_[n]&0xC0)==0x80)n--;vcText_.erase(n);vcPreset_=-1;}if(c==2){vcFocus_=false;}}

void App::voicePlay(){
  vcFocus_=false;
  std::string text=vcText_;if(text.empty()){toast("Type something first");return;}
  if(vcVoice_==4){
    vcBusy_=true;vcStatus_="Asking the phone's voice engine...";
    std::string lang=hasArabic(text)?"ar-SA":"en-US";
    speakSystem(text,lang,1.0f,[this](bool ok,Speech& s){std::lock_guard<std::mutex> lk(vcMx_);vcPend_=s;vcPendOk_=ok;vcPendSet_=true;});
    return;}
  VoiceParams vp=vcVoice_==1?VoiceParams::femaleTeacher():vcVoice_==2?VoiceParams::child():vcVoice_==3?VoiceParams::oldMan():VoiceParams::maleTeacher();
  if(!speak(text,vp,vcSp_)){toast("Nothing to say");return;}
  vcHas_=true;vcT_=0;vcPlaying_=true;vcStatus_=std::string("Synth voice: ")+VOICE_NAMES[vcVoice_];
  if(playAudio)playAudio(vcSp_.pcm.data(),(int)vcSp_.pcm.size(),vcSp_.rate);}

void App::voiceUpdate(float dt){
  {std::lock_guard<std::mutex> lk(vcMx_);
   if(vcPendSet_){vcPendSet_=false;vcBusy_=false;
     if(vcPendOk_){vcSp_=vcPend_;vcHas_=true;vcT_=0;vcPlaying_=true;vcStatus_="Real phone voice";if(playAudio)playAudio(vcSp_.pcm.data(),(int)vcSp_.pcm.size(),vcSp_.rate);}
     else{vcStatus_="Phone voice not available here - using synth";vcVoice_=0;}}}
  if(vcPlaying_){vcT_+=dt;if(vcT_>vcSp_.duration()+0.05f){vcPlaying_=false;}}}

void App::voiceTap(int id){
  CharDef& cd=teacherDef();
  if(id==1072){vcMode_=2;vcFocus_=false;return;}
  if(id>=1080&&id<=1082){teachTap(id);return;}
  if(id==1070){vcMode_=0;return;}
  if(id==1071){vcMode_=1;vcFocus_=false;return;}
  if(id>=1020&&id<=1027){int row=(id-1020)/2;bool plus=(id&1)==1;float* pv[4]={&cd.hairLen,&cd.hairDens,&cd.hairCurl,&cd.hairClump};float lo[4]={0.3f,0.3f,0.f,0.f},hi[4]={1.8f,1.5f,1.f,1.f};
    *pv[row]=std::max(lo[row],std::min(hi[row],*pv[row]+(plus?0.15f:-0.15f)));vcHairDirty_=true;return;}
  if(id>=1030&&id<=1035){applyHairPreset(id-1030);rebuildTeacher();return;}
  if(id>=1040&&id<=1045){vcColor_=id-1040;cd.col[1]=HAIR_COLS[vcColor_];rebuildTeacher();return;}
  if(id>=1050&&id<=1054){vcBeard_=id-1050;cd.beard=vcBeard_;rebuildTeacher();return;}
  if(id==1056||id==1057){cd.browK=std::max(0.4f,std::min(1.8f,cd.browK+(id==1057?0.2f:-0.2f)));rebuildTeacher();return;}
  if(id==1060){rebuildTeacher();return;}
  if(id==1000){vcFocus_=!vcFocus_;}
  else if(id>=1001&&id<=1005){vcVoice_=id-1001;vcFocus_=false;}
  else if(id>=1006&&id<=1011){vcPreset_=id-1006;vcText_=VC_PRESET_TEXT[vcPreset_];vcFocus_=false;}
  else if(id==1012)voicePlay();
  else if(id==1013){vcPlaying_=false;vcT_=0;if(playAudio)playAudio(nullptr,0,24000);}
  else if(id==1014){vcText_.clear();vcPreset_=-1;vcFocus_=true;}}

void App::voiceMouthApply(Model* m){
  // mouth key -> viseme morph weights (6 vOpen,7 vPucker,8 vWide,9 vPress,10 vUpTeeth,11 vLowLip,12 vTongue; 2 blink, 3 brows)
  MouthKey k{};k.jaw=0;if(vcHas_&&vcPlaying_&&!vcSp_.mouth.empty()){size_t i=(size_t)std::max(0.f,std::min((float)vcSp_.mouth.size()-1,vcT_*100.f));k=vcSp_.mouth[i];}
  float tgt[16]={0};float press=k.close;
  tgt[6]=std::min(1.f,k.jaw*1.05f)*(1.f-press);tgt[7]=k.round*(1.f-0.5f*k.wide);tgt[8]=k.wide*(1.f-press);tgt[9]=press;
  tgt[10]=std::min(1.f,k.teeth*0.5f+k.fv*0.6f);tgt[11]=k.fv*0.7f;tgt[12]=k.tongue;
  float bl=(std::fmod(t_,3.7f)>3.55f)?1.f:0.f;tgt[2]=bl;tgt[3]=0.f;
  static float prevT=0;float dt=std::max(0.001f,std::min(0.05f,t_-prevT));prevT=t_;float a=1.f-std::exp(-dt*28.f);
  for(int i=0;i<13&&i<(int)m->morphNames.size();i++){vcW_[i]+=(tgt[i]-vcW_[i])*(i==2?1.f:a);m->morphW[i]=vcW_[i];}
  {float sx=std::sin(t_*1.3f)*0.5f+std::sin(t_*0.47f)*0.3f,sz=std::sin(t_*0.9f+1.f)*0.4f;sx+=(vcPlaying_?std::sin(t_*7.f)*0.25f*k.energy:0.f);m->sway[0]=sx*0.010f;m->sway[1]=-0.0015f*std::fabs(sx);m->sway[2]=sz*0.008f;}
  m->applyMorphs(*r_);
}

void App::voiceRenderFace(){
  if(!vcFaceOk_){if(vcFace_.fbo==0&&!r_->createTarget(vcFace_,1024,880,4))return;
    vcScene_=Scene();{auto& f=vcScene_.add(P_PLANE,"StudioFloor");f.pos={0,0,0};f.scale={40,1,40};f.color=Col::hex(0x23272e);}
    auto& en=vcScene_.add(P_MODEL,"Teacher");en.asset="gen:teacher";en.pos={0,0,0};vcFaceId_=en.id;vcFaceOk_=true;}
  Model* m=modelFor("gen:teacher");if(!m)return;
  voiceMouthApply(m);
  Entity* en=vcScene_.get(vcFaceId_);if(en){en->rot={0,std::sin(t_*0.6f)*4.f,0};}
  shadowR_=0.30f;renderThumb(vcFace_,vcScene_,Vec3{0.012f,1.672f,0.80f},Vec3{0.012f,1.650f,0.0f},0.50f);shadowR_=55.f;
}
void App::drawFace(float cx,float cy,float sc){(void)cx;(void)cy;(void)sc;}


CharDef& App::teacherDef(){
  if(!vcCdInit_){CharDef cd;cd.name="Teacher";cd.height=1.78f;cd.hv[0]=1.f;cd.hv[1]=0.38f;cd.hv[2]=0.45f;cd.hv[3]=0.5f;cd.hv[5]=0.f;cd.hv[6]=0.f;cd.hv[7]=1.f;cd.hair=1;cd.outfit=3;cd.col[0]=0xc99872;cd.col[1]=0x4a2f1e;cd.col[2]=0xe6e4dc;cd.col[3]=0x2b2f3a;vcCd_=cd;vcCdInit_=true;}
  return vcCd_;}
void App::applyHairPreset(int i){CharDef& c=teacherDef();vcHairPreset_=i;
  struct P{int hair;float len,dens,curl,clump,frizz;};static const P ps[6]={{1,1.f,1.f,0.f,0.35f,1.f},{1,0.42f,1.15f,0.f,0.15f,0.6f},{1,1.3f,1.f,0.25f,0.55f,1.8f},{2,1.f,1.f,0.15f,0.4f,1.2f},{1,1.1f,1.1f,1.f,0.6f,1.f},{3,1.f,1.f,0.f,0.3f,1.f}};
  const P& p=ps[i];c.hair=p.hair;c.hairLen=p.len;c.hairDens=p.dens;c.hairCurl=p.curl;c.hairClump=p.clump;c.hairFrizz=p.frizz;vcHairDirty_=true;}
void App::rebuildTeacher(){models_.erase("gen:teacher");for(float& w:vcW_)w=0;vcHairDirty_=false;toast("Hair rebuilt");}
static const char* HAIR_COL_NAMES[6]={"Black","D.Brown","Brown","Blond","Ginger","Grey"};
static const char* HAIR_PRESET[6]={"Side part","Crew cut","Messy","Long","Curly","Buzz"};
static const char* BEARD_NAMES[5]={"No beard","Stubble","Short","Full","Moustache"};
void App::drawHairPanels(){
  CharDef& cd=teacherDef();
  sect(1);panel(24,84,430,46,"STYLE");
  for(int i=0;i<6;i++)fbtn(1030+i,32+i*68,104,64,22,HAIR_PRESET[i],3,vcHairPreset_==i);
  sect(2);panel(24,134,430,92,"SHAPE   (tap - / + then Apply)");
  struct Row{const char* n;float* v;float lo,hi;};Row rows[4]={{"Length",&cd.hairLen,0.3f,1.8f},{"Density",&cd.hairDens,0.3f,1.5f},{"Curl",&cd.hairCurl,0.f,1.f},{"Clumping",&cd.hairClump,0.f,1.f}};
  fbtn(1060,396,136,52,15,"Apply",vcHairDirty_?0:3,false);
  for(int i=0;i<4;i++){float y=154+i*17.f;T(rows[i].n,34,y+2,10.5f,TXT2);fbtn(1020+i*2,130,y,24,15,"-",3,false);
    float t=(*rows[i].v-rows[i].lo)/(rows[i].hi-rows[i].lo);R(160,y+5,236,5,2.5f,BG0,BG0);R(160,y+5,std::max(5.f,236*t),5,2.5f,ACC,ACC2);fbtn(1021+i*2,402,y,24,15,"+",3,false);}
  sect(3);panel(24,230,430,46,"COLOUR");
  for(int i=0;i<6;i++){float bx=32+i*68;btn(1040+i,bx,250,64,22);float k=pressOf(1040+i);unsigned c=HAIR_COLS[i];
    R(bx,250,64,22,4,Col::hex(c).mul(1.4f+0.2f*k),Col::hex(c).mul(1.1f));RING(bx,250,64,22,4,vcColor_==i?2.f:1.f,vcColor_==i?ACC:LINE2);T(HAIR_COL_NAMES[i],bx+32,255,9.5f,i==5||i==3?Col::hex(0x111111):Col::hex(0xffffff),F_BOLD,1);}
  sect(4);panel(24,280,430,50,"FACIAL HAIR  /  EYEBROWS");
  for(int i=0;i<5;i++)fbtn(1050+i,32+i*66,300,62,22,BEARD_NAMES[i],3,vcBeard_==i);
  fbtn(1056,364,300,40,22,"Brow -",3,false);fbtn(1057,408,300,40,22,"Brow +",3,false);
}

void App::drawVoice(){
  float W=W_/ui.s(),H=H_/ui.s();(void)H;
  header("Voice Studio","Type or pick a line, choose a voice, press Play - lips follow the sound.");
  if(vcMode_==2){drawTeach();return;}
  fbtn(1070,24,60,100,20,"Voice",1,vcMode_==0);fbtn(1071,128,60,100,20,"Hair",1,vcMode_==1);fbtn(1072,232,60,100,20,"Class",1,false);
  if(vcMode_==1){drawHairPanels();}else{
  // ---- text panel
  sect(1);panel(24,84,430,92,"TEXT");
  btn(1000,32,104,414,66);float k=pressOf(1000);
  R(32,104,414,66,5,BG0.mul(1+0.4f*k),BG0.mul(1+0.4f*k));RING(32,104,414,66,5,vcFocus_?1.6f:1.f,vcFocus_?ACC:LINE2);
  {std::string tx=vcText_;if(hasArabic(tx))tx="[Arabic line]  "+std::string(vcPreset_>=0?VC_PRESET_LABEL[vcPreset_]:"");
   // wrap on 3 lines ~ 58 chars
   size_t pos=0;for(int ln=0;ln<3&&pos<tx.size();ln++){size_t end=std::min(tx.size(),pos+58);if(end<tx.size()){size_t sp=tx.rfind(' ',end);if(sp!=std::string::npos&&sp>pos)end=sp;}
     std::string seg=tx.substr(pos,end-pos);if(ln==2&&end<tx.size())seg+="...";
     bool lastLine=(end>=tx.size());if(vcFocus_&&lastLine&&std::fmod(t_,1.f)<0.5f)seg+="|";
     T(seg,42,109+ln*19,12.5f,TXT);pos=end;while(pos<tx.size()&&tx[pos]==' ')pos++;}
   if(tx.empty())T(vcFocus_?"|":"Tap here and type (English)...",42,109,12.5f,TXT3);}
  // ---- voices
  sect(2);panel(24,182,430,64,"VOICE");
  for(int i=0;i<5;i++){float bw=i==4?100:74;float bx=32+i*80;if(i==4)bx=32+4*80-0;fbtn(1001+i,bx,206,bw,30,VOICE_NAMES[i],i==4?0:1,vcVoice_==i);}
  // ---- presets
  sect(3);panel(24,254,430,76,"QUICK LINES  (Arabic lines have diacritics for correct reading)");
  for(int i=0;i<6;i++){float bx=32+(i%3)*138,by=278+(i/3)*24;fbtn(1006+i,bx,by,132,22,VC_PRESET_LABEL[i],3,vcPreset_==i);}
  }
  // ---- play bar
  sect(4);
  fbtn(1012,24,338,150,40,vcBusy_?"Working...":"Play",0);fbtn(1013,182,338,90,40,"Stop",1);fbtn(1014,280,338,90,40,"Clear",3);
  T(vcStatus_,380,350,10.5f,TXT3,F_REG,0);
  // ---- face
  sect(2);float px=470,py=62,pw=W-470-24,ph=316;panel(px,py,pw,ph,"PREVIEW");
  if(vcFaceOk_){float fw=pw-12,fh=ph-28;ui.image(vcFace_.tex,px+6,py+22+oy_,fw,fh,5,Col{1,1,1,A_});}
  if(vcHas_){float dur=vcSp_.duration();float bw=pw-28;R(px+14,py+ph-22,bw,6,3,BG0,BG0);float f=dur>0?std::min(1.f,vcT_/dur):0.f;R(px+14,py+ph-22,std::max(6.f,bw*f),6,3,ACC,ACC2);
    char buf[48];snprintf(buf,sizeof buf,"%.1f / %.1f s",std::min(vcT_,dur),dur);T(buf,px+pw-14,py+8,10.5f,TXT3,F_BOLD,2);}
  (void)W;
}
}
