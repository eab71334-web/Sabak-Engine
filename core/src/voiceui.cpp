// Sabak Core - Voice Studio: type / pick a line, choose a voice, hear it, watch the lips move.
#include "app.h"
#include "theme.h"
#include <cmath>
#include <algorithm>
namespace sk {
using namespace th;
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
  if(id==1000){vcFocus_=!vcFocus_;}
  else if(id>=1001&&id<=1005){vcVoice_=id-1001;vcFocus_=false;}
  else if(id>=1006&&id<=1011){vcPreset_=id-1006;vcText_=VC_PRESET_TEXT[vcPreset_];vcFocus_=false;}
  else if(id==1012)voicePlay();
  else if(id==1013){vcPlaying_=false;vcT_=0;if(playAudio)playAudio(nullptr,0,24000);}
  else if(id==1014){vcText_.clear();vcPreset_=-1;vcFocus_=true;}}

void App::drawFace(float cx,float cy,float sc){
  MouthKey m{};m.jaw=0;m.wide=0.3f;
  if(vcHas_){size_t i=(size_t)std::max(0.f,std::min((float)vcSp_.mouth.size()-1,vcT_*100.f));if(vcPlaying_&&!vcSp_.mouth.empty())m=vcSp_.mouth[i];}
  float bob=vcPlaying_?std::sin(vcT_*9.f)*1.2f*sc*m.energy:0.f;cy+=bob;
  Col skin=Col::hex(0xd9a77f),skin2=Col::hex(0xc08760);
  // neck + shoulders (teacher shirt)
  R(cx-100*sc,cy+92*sc,200*sc,56*sc,14*sc,Col::hex(0x35507a),Col::hex(0x28405f));R(cx-18*sc,cy+70*sc,36*sc,40*sc,8*sc,skin2,skin2);
  R(cx-26*sc,cy+96*sc,52*sc,28*sc,4*sc,Col::hex(0xe8e6df),Col::hex(0xe8e6df));
  // head
  SH(cx-82*sc,cy-100*sc+4,164*sc,200*sc,80*sc,18,Col::hex(0,0.4f));
  R(cx-82*sc,cy-100*sc,164*sc,200*sc,78*sc,skin,skin2);
  // hair
  R(cx-84*sc,cy-108*sc,168*sc,58*sc,42*sc,Col::hex(0x2b2118),Col::hex(0x1d1610));
  // beard shadow (adult male feel)
  R(cx-58*sc,cy+40*sc,116*sc,56*sc,30*sc,Col::hex(0x3a2d24,0.28f),Col::hex(0x3a2d24,0.45f));
  // eyes
  float blink=(std::fmod(t_,3.6f)>3.45f)?0.15f:1.f;
  for(int s=-1;s<=1;s+=2){float ex=cx+s*34*sc,ey=cy-22*sc;R(ex-13*sc,ey-8*sc*blink,26*sc,16*sc*blink,8*sc*blink,Col::hex(0xf4f1ea),Col::hex(0xe5e0d6));
    if(blink>0.5f){ui.circle(ex+s*0.5f*sc,ey+oy_,5.5f*sc,Col::hex(0x3a2a1c),Col::hex(0x20150b));}
    R(ex-17*sc,ey-24*sc,34*sc,5*sc,2.5f*sc,Col::hex(0x2b2118),Col::hex(0x2b2118));}
  // nose
  R(cx-6*sc,cy-6*sc,12*sc,26*sc,6*sc,skin2.with(0.6f),skin2.with(0.9f));
  // mouth
  float my=cy+55*sc+m.jaw*7*sc;
  float w=(44+24*m.wide-16*m.round-10*m.close+4*m.fv)*sc;
  float h=(3+34*m.jaw)*sc*(1.f-0.7f*m.close);if(m.fv>0.5f)h=std::max(h,6*sc);
  float lipT=5.5f*sc;
  Col lip=Col::hex(0xa8514f),lip2=Col::hex(0x8c3f40);
  float ow=w+2*lipT*0.8f,oh=h+2*lipT*(m.close>0.5f?0.4f:1.f);
  R(cx-ow/2,my-oh/2,ow,oh,std::min(ow,oh)/2,lip,lip2);
  if(h>2.5f*sc){
    float iw=w,ih=h;R(cx-iw/2,my-ih/2,iw,ih,std::min(iw,ih)/2,Col::hex(0x2a0a0d),Col::hex(0x14030a));
    float tt=m.teeth>0.3f||m.jaw>0.22f?std::min(ih*0.42f,5.5f*sc*(0.5f+m.teeth)):0.f;
    if(tt>0.5f)R(cx-iw*0.42f,my-ih/2,iw*0.84f,tt,2*sc,Col::hex(0xf2efe6),Col::hex(0xe3dfd3));
    if(m.fv>0.5f)R(cx-iw*0.4f,my-ih/2,iw*0.8f,std::min(ih*0.6f,6*sc),2*sc,Col::hex(0xf2efe6),Col::hex(0xe3dfd3));
    float tg=std::max(m.tongue,m.jaw*0.45f);
    if(ih>5*sc)R(cx-iw*0.30f,my+ih/2-ih*(0.25f+0.4f*tg),iw*0.60f,ih*(0.25f+0.4f*tg),ih*0.3f,Col::hex(0xb5544f),Col::hex(0x9c403c));}
  else{R(cx-w/2,my-0.8f*sc,w,1.6f*sc,0.8f*sc,Col::hex(0x4a1c1c),Col::hex(0x4a1c1c));}
  // chin highlight
  R(cx-30*sc,cy+88*sc+m.jaw*7*sc,60*sc,6*sc,3*sc,skin.with(0.0f),skin.with(0.0f));
}

void App::drawVoice(){
  float W=W_/ui.s(),H=H_/ui.s();(void)H;
  header("Voice Studio","Type or pick a line, choose a voice, press Play - lips follow the sound.");
  // ---- text panel
  sect(1);panel(24,62,430,112,"TEXT");
  btn(1000,32,86,414,80);float k=pressOf(1000);
  R(32,86,414,80,5,BG0.mul(1+0.4f*k),BG0.mul(1+0.4f*k));RING(32,86,414,80,5,vcFocus_?1.6f:1.f,vcFocus_?ACC:LINE2);
  {std::string tx=vcText_;if(hasArabic(tx))tx="[Arabic line]  "+std::string(vcPreset_>=0?VC_PRESET_LABEL[vcPreset_]:"");
   // wrap on 3 lines ~ 58 chars
   size_t pos=0;for(int ln=0;ln<3&&pos<tx.size();ln++){size_t end=std::min(tx.size(),pos+58);if(end<tx.size()){size_t sp=tx.rfind(' ',end);if(sp!=std::string::npos&&sp>pos)end=sp;}
     std::string seg=tx.substr(pos,end-pos);if(ln==2&&end<tx.size())seg+="...";
     bool lastLine=(end>=tx.size());if(vcFocus_&&lastLine&&std::fmod(t_,1.f)<0.5f)seg+="|";
     T(seg,42,94+ln*21,13,TXT);pos=end;while(pos<tx.size()&&tx[pos]==' ')pos++;}
   if(tx.empty())T(vcFocus_?"|":"Tap here and type (English)...",42,94,13,TXT3);}
  // ---- voices
  sect(2);panel(24,182,430,64,"VOICE");
  for(int i=0;i<5;i++){float bw=i==4?100:74;float bx=32+i*80;if(i==4)bx=32+4*80-0;fbtn(1001+i,bx,206,bw,30,VOICE_NAMES[i],i==4?0:1,vcVoice_==i);}
  // ---- presets
  sect(3);panel(24,254,430,76,"QUICK LINES  (Arabic lines have diacritics for correct reading)");
  for(int i=0;i<6;i++){float bx=32+(i%3)*138,by=278+(i/3)*24;fbtn(1006+i,bx,by,132,22,VC_PRESET_LABEL[i],3,vcPreset_==i);}
  // ---- play bar
  sect(4);
  fbtn(1012,24,338,150,40,vcBusy_?"Working...":"Play",0);fbtn(1013,182,338,90,40,"Stop",1);fbtn(1014,280,338,90,40,"Clear",3);
  T(vcStatus_,380,350,10.5f,TXT3,F_REG,0);
  // ---- face
  sect(2);float px=470,py=62,pw=W-470-24,ph=316;panel(px,py,pw,ph,"PREVIEW");
  drawFace(px+pw/2,py+ph/2-6,0.98f);
  if(vcHas_){float dur=vcSp_.duration();float bw=pw-28;R(px+14,py+ph-22,bw,6,3,BG0,BG0);float f=dur>0?std::min(1.f,vcT_/dur):0.f;R(px+14,py+ph-22,std::max(6.f,bw*f),6,3,ACC,ACC2);
    char buf[48];snprintf(buf,sizeof buf,"%.1f / %.1f s",std::min(vcT_,dur),dur);T(buf,px+pw-14,py+8,10.5f,TXT3,F_BOLD,2);}
  (void)W;
}
}
