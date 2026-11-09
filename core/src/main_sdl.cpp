// Sabak Core - windowed app (iOS / Android / desktop) using SDL3.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string>
#include "app.h"
#include "model.h"
#include "gl.h"
int main(int, char**){
  if(!SDL_Init(SDL_INIT_VIDEO)){SDL_Log("SDL_Init failed: %s",SDL_GetError());return 1;}
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
  SDL_Window* win=SDL_CreateWindow("Sabak Engine",1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN|SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if(!win){SDL_Log("window failed: %s",SDL_GetError());return 1;}
  SDL_GLContext ctx=SDL_GL_CreateContext(win);if(!ctx){SDL_Log("GL context failed: %s",SDL_GetError());return 1;}
  SDL_GL_SetSwapInterval(1);
  int W=0,H=0;SDL_GetWindowSizeInPixels(win,&W,&H);if(W<=0||H<=0)SDL_GetWindowSize(win,&W,&H);if(W<=0||H<=0){W=1704;H=786;}
  unsigned winFbo=0,winRb=0;
#ifdef __APPLE__
  SDL_PropertiesID wp=SDL_GetWindowProperties(win);
  winFbo=(unsigned)SDL_GetNumberProperty(wp,SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER,0);
  winRb=(unsigned)SDL_GetNumberProperty(wp,SDL_PROP_WINDOW_UIKIT_OPENGL_RENDERBUFFER_NUMBER,0);
#endif
  {const char* bp=SDL_GetBasePath();sk::g_assetDir=std::string(bp?bp:"./")+"assets";}
  sk::Renderer r;bool ok=r.init(W,H);
  if(!ok){float cg=r.shadersOk?0.0f:0.5f;   // diagnostic: red = GL error, orange = shader error
    while(true){SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_EVENT_QUIT)return 0;}
      glBindFramebuffer(GL_FRAMEBUFFER,winFbo);glViewport(0,0,W,H);glClearColor(1,cg,0,1);glClear(GL_COLOR_BUFFER_BIT);
      if(winRb)glBindRenderbuffer(GL_RENDERBUFFER,winRb);SDL_GL_SwapWindow(win);}}
  r.setExternalTarget(winFbo);
  // project files live in the app's Documents folder so they show up in the Files app
  std::string data;const char* docs=SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
  if(docs&&*docs)data=docs;else{char* pp=SDL_GetPrefPath("SABAK","SabakEngine");data=pp?pp:".";if(pp)SDL_free(pp);}
  while(data.size()>1&&data.back()=='/')data.pop_back();
  sk::App* app=new sk::App();
  if(!app->init(r,data,W,H)){while(true){SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_EVENT_QUIT)return 0;}
      glBindFramebuffer(GL_FRAMEBUFFER,winFbo);glClearColor(1,0,1,1);glClear(GL_COLOR_BUFFER_BIT);if(winRb)glBindRenderbuffer(GL_RENDERBUFFER,winRb);SDL_GL_SwapWindow(win);}}   // magenta = UI init failed
  int ww=0,wh=0;SDL_GetWindowSize(win,&ww,&wh);float mx=ww>0?(float)W/ww:1.0f;
  bool run=true;Uint64 last=SDL_GetTicksNS();
  while(run){
    SDL_Event e;while(SDL_PollEvent(&e)){
      if(e.type==SDL_EVENT_QUIT)run=false;
      else if(e.type==SDL_EVENT_KEY_DOWN||e.type==SDL_EVENT_KEY_UP){bool d=e.type==SDL_EVENT_KEY_DOWN;const char* k=nullptr;
        switch(e.key.key){case SDLK_W:k="W";break;case SDLK_A:k="A";break;case SDLK_S:k="S";break;case SDLK_D:k="D";break;default:break;}
        if(k)app->key(k,d);}
      else if(e.type==SDL_EVENT_FINGER_DOWN)app->pointer(0,(int)(e.tfinger.fingerID&0x7fffffff),e.tfinger.x*W,e.tfinger.y*H);
      else if(e.type==SDL_EVENT_FINGER_MOTION)app->pointer(1,(int)(e.tfinger.fingerID&0x7fffffff),e.tfinger.x*W,e.tfinger.y*H);
      else if(e.type==SDL_EVENT_FINGER_UP||e.type==SDL_EVENT_FINGER_CANCELED)app->pointer(2,(int)(e.tfinger.fingerID&0x7fffffff),e.tfinger.x*W,e.tfinger.y*H);
      else if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&e.button.which!=SDL_TOUCH_MOUSEID)app->pointer(0,1000,e.button.x*mx,e.button.y*mx);
      else if(e.type==SDL_EVENT_MOUSE_MOTION&&e.motion.which!=SDL_TOUCH_MOUSEID&&(e.motion.state&SDL_BUTTON_LMASK))app->pointer(1,1000,e.motion.x*mx,e.motion.y*mx);
      else if(e.type==SDL_EVENT_MOUSE_BUTTON_UP&&e.button.which!=SDL_TOUCH_MOUSEID)app->pointer(2,1000,e.button.x*mx,e.button.y*mx);
    }
    Uint64 now=SDL_GetTicksNS();float dt=(float)((now-last)/1e9);last=now;if(dt>0.05f)dt=0.05f;
    app->update(dt);app->render();if(winRb)glBindRenderbuffer(GL_RENDERBUFFER,winRb);SDL_GL_SwapWindow(win);
  }
  SDL_Quit();return 0;}
