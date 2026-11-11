// Sabak Core - application shell: launcher (home / templates / import) + project editor view.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include "scene.h"
#include "script.h"
#include "renderer.h"
#include "model.h"
#include "ui.h"
namespace sk {
struct Project { std::string name,dir; int tmpl=1; long created=0; };
class App {
public:
  bool init(Renderer& r,const std::string& dataDir,int w,int h);
  void resize(int w,int h);
  void update(float dt);
  void render();
  void pointer(int type,int id,float x,float y);   // type 0 down, 1 move, 2 up. x,y in framebuffer pixels
  void key(const std::string& k,bool down);
  const char* screenName() const;
  bool quit=false;
  int entityCount() const {return (int)scene_.ents.size();} int selected() const {return sel_;} bool playing() const {return play_;}

private:
  enum Screen { HOME,TEMPLATES,IMPORT,EDITOR };
  struct Btn { int id; float x,y,w,h; };
  void go(Screen s); void createProject(int tmpl); void openProject(int i); void rescan(); void rescanImports(); void importFound(int i);
  void toast(const std::string& m){toast_=m;toastT_=2.4f;}
  // ---- scene editor (editor.cpp)
  void drawEditor(); void editorPointer(int type,int id,float px,float py); void editorTap(int id); void editorUpdate(float dt);
  void renderScene(const Scene& sc,Vec3 cam,Vec3 target,float fov); int meshFor(int prim);
  void startPlay(); void stopPlay(); void saveScene(bool quiet=false); void select(int id); void rebuildRows();
  bool pickAt(float px,float py); void gizmoUpdate(); bool nearPanel(float x,float y) const;
  void panel(float x,float y,float w,float h,const std::string& title);
  void fbtn(int id,float x,float y,float w,float h,const std::string& label,int kind,bool active=false);   // kind 0 primary,1 normal,2 danger,3 ghost
  void sect(float k);
  void ensureThumbs(); void renderThumb(Target& t,const Scene& sc,Vec3 cam,Vec3 tg,float fov=0.9f); void cbTap(int id); void drawContent(float x0,float x1); void drawSelection(Vec3 cam,Vec3 tg);
  void drawBackground(); void drawHome(); void drawTemplates(); void drawImport(); void drawHud(); void header(const std::string& title,const std::string& sub);
  bool btn(int id,float x,float y,float w,float h);     // registers a hit area (design units); returns true if held
  float pressOf(int id){return id>=0&&id<512?press_[id]:0.f;}
  void onTap(int id);
  Col C(Col c){c.a*=A_;return c;}
  void R(float x,float y,float w,float h,float r,Col a,Col b){ui.rrect(x,y+oy_,w,h,r,C(a),C(b));}
  void RING(float x,float y,float w,float h,float r,float bw,Col c){ui.ring(x,y+oy_,w,h,r,bw,C(c));}
  void SH(float x,float y,float w,float h,float r,float bl,Col c){ui.shadow(x,y+oy_,w,h,r,bl,C(c));}
  void GL(float x,float y,float rad,Col c){ui.glow(x,y+oy_,rad,C(c));}
  void T(const std::string& t,float x,float y,float sz,Col c,int f=F_REG,int al=0){ui.text(t,x,y+oy_,sz,C(c),f,al);}
  void plusIcon(float cx,float cy,float sz,Col c); void downIcon(float cx,float cy,float sz,Col c); void cubeIcon(float cx,float cy,float sz,float a);
  Renderer* r_=nullptr; UI ui; std::string data_,projDir_,importDir_; int W_=0,H_=0; float trT_=0,t_=0,A_=1,oy_=0,tr_=1,toastT_=0,fps_=60,fpsAcc_=0; int fpsN_=0;
  Screen scr_=HOME; std::string toast_; std::vector<Project> projects_; std::vector<std::string> found_; int cur_=-1;
  std::vector<Btn> btns_,prevBtns_; float press_[512]={0}; int held_=-1,heldId_=-1;
  float stickX_=0,stickY_=0; int stickId_=-1,lookId_=-1; float sx0_=0,sy0_=0,lx_=0,ly_=0;
  std::map<std::string,Target> thumbs_; Target tmplT_[3],banner_; bool tmplOk_[3]={false,false,false},bannerOk_=false,thumbDirty_=false,cbOpen_=false; int cbTab_=0; float cbT_=0;
  Scene scene_,snap_,bg_; int meshIds_[P_COUNT]; std::map<std::string,std::unique_ptr<Model>> models_; std::map<std::pair<const Scene*,int>,ModelInst> minst_; Model* modelFor(const std::string& name); std::vector<std::string> mdlList_; std::map<std::string,Target> mthumbs_; void refreshModels(); void placeModel(const std::string& id);
  // animator (animator.cpp)
  void openAnimator(); void closeAnimator(); void animTap(int id); void animUpdate(float dt); void drawAnimator(float W,float H); void aeSeek(float x); Model* aeModel(); std::string aeId();
  bool aePointer(int type,int id,float px,float py); void drawToolBtn(int id,float x,float y,float w,float h,const std::string& label,bool on=false); void aeProject(); void aePush(); std::vector<std::vector<ClipDef>> aeUndo_,aeRedo_; std::map<int,std::pair<Vec3,Vec3>> aeCopy_; struct AeJ{int joint;float x,y;}; std::vector<AeJ> aeJ_; int aeEvName_=0,aeDragId_=-1; bool aeIK_=true,aeDragChanged_=false; float aeLx_=0,aeLy_=0;
  struct AePose{std::string name,file;std::map<int,std::pair<Vec3,Vec3>> d;bool builtin=false;}; std::vector<AePose> aePoses_;
  int aeTab_=0,aeKeySel_=-1,aeChan_=0,aeGMode_=0,aeGId_=-1,aeSmSel_=0,aeTrSel_=0,aePoseSel_=0,aePoseOff_=0; bool aeOnion_=false,aeCam_=false,aeLook_=false,aeFxPrev_=false,aeLookDrag_=false; float aeGVmax_=15;
  void aeStudioDraw(float W,float H); bool aeStudioPointer(int type,int id,float x,float y); void aeStudioTap(int id); void aeOnionDraw(); void aeCamDraw(float W,float H); void aeLoadPoses();
  bool animEd_=false,aePlay_=true,aeImp_=false; int aeBone_=0,aeClip_=-1; float aeT_=0,aeScroll_=0; std::string aePrevAnim_; std::vector<std::string> aeFiles_; std::map<int,std::unique_ptr<Script>> scripts_; Input in_; ScriptContext sctx_;
  bool play_=false,addMenu_=false,keyDown_[4]={false,false,false,false}; int sel_=-1,mode_=0; float hierScroll_=0,repeatT_=0,pYaw_=0,pPitch_=0.3f,playT_=0;
  float ecYaw_=0.8f,ecPitch_=0.5f,ecDist_=26; Vec3 ecTarget_{0,3,0};
  std::set<int> expanded_; std::vector<std::pair<int,int>> rows_;
  float gO_[2]={0,0},gT_[3][2]={{0,0},{0,0},{0,0}},gL_=1; bool gOk_=false; int dragAxis_=-1,dragId_=-1,orbitId_=-1,hierId_=-1,pinchA_=-1,pinchB_=-1; float lastX_=0,lastY_=0,downX_=0,downY_=0,pinchD_=0; bool moved_=false;
  float ptX_[8]={0},ptY_[8]={0}; int ptId_[8]={-1,-1,-1,-1,-1,-1,-1,-1};
};
}
