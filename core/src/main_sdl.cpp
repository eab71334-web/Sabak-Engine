// Sabak Core - windowed app (iOS / Android / desktop) using SDL3. Touch: left half = move stick, right half = look.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cmath>
#include "game.h"
int main(int, char**){
  if(!SDL_Init(SDL_INIT_VIDEO)){SDL_Log("SDL_Init failed: %s",SDL_GetError());return 1;}
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
  SDL_Window* win=SDL_CreateWindow("Sabak Engine",1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN|SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if(!win){SDL_Log("window failed: %s",SDL_GetError());return 1;}
  SDL_GLContext ctx=SDL_GL_CreateContext(win);if(!ctx){SDL_Log("GL context failed: %s",SDL_GetError());return 1;}
  SDL_GL_SetSwapInterval(1);
  int W=0,H=0;SDL_GetWindowSizeInPixels(win,&W,&H);
  // render at most 1280 px wide for speed; the blit scales it to the screen
  float sc=W>1280?1280.0f/W:1.0f;int RW=(int)(W*sc),RH=(int)(H*sc);
  sk::Renderer r;if(!r.init(RW,RH)){SDL_Log("renderer init failed");return 1;}
  sk::Game g;g.init(r);
  unsigned winFbo=0;
#ifdef __APPLE__
  winFbo=(unsigned)SDL_GetNumberProperty(SDL_GetWindowProperties(win),SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER,0);
#endif
  SDL_FingerID stickId=0,lookId=0;bool hasStick=false,hasLook=false;float sx0=0,sy0=0;
  bool run=true;Uint64 last=SDL_GetTicksNS();
  while(run){
    SDL_Event e;while(SDL_PollEvent(&e)){
      if(e.type==SDL_EVENT_QUIT)run=false;
      else if(e.type==SDL_EVENT_KEY_DOWN||e.type==SDL_EVENT_KEY_UP){bool d=e.type==SDL_EVENT_KEY_DOWN;const char* k=nullptr;
        switch(e.key.key){case SDLK_W:k="W";break;case SDLK_A:k="A";break;case SDLK_S:k="S";break;case SDLK_D:k="D";break;default:break;}
        if(k){if(d)g.press(k);else g.release(k);}if(e.key.key==SDLK_ESCAPE)run=false;}
      else if(e.type==SDL_EVENT_FINGER_DOWN){
        if(e.tfinger.x<0.5f&&!hasStick){hasStick=true;stickId=e.tfinger.fingerID;sx0=e.tfinger.x;sy0=e.tfinger.y;}
        else if(!hasLook){hasLook=true;lookId=e.tfinger.fingerID;}}
      else if(e.type==SDL_EVENT_FINGER_MOTION){
        if(hasStick&&e.tfinger.fingerID==stickId){float dx=(e.tfinger.x-sx0)*W/90.0f,dy=(e.tfinger.y-sy0)*H/90.0f;float l=std::sqrt(dx*dx+dy*dy);if(l>1){dx/=l;dy/=l;}g.stick(dx,dy);}
        else if(hasLook&&e.tfinger.fingerID==lookId)g.look(-e.tfinger.dx*W*0.006f,e.tfinger.dy*H*0.005f);}
      else if(e.type==SDL_EVENT_FINGER_UP||e.type==SDL_EVENT_FINGER_CANCELED){
        if(hasStick&&e.tfinger.fingerID==stickId){hasStick=false;g.stick(0,0);}
        if(hasLook&&e.tfinger.fingerID==lookId)hasLook=false;}
      else if(e.type==SDL_EVENT_MOUSE_MOTION&&(e.motion.state&SDL_BUTTON_LMASK))g.look(-e.motion.xrel*0.006f,e.motion.yrel*0.005f);
    }
    Uint64 now=SDL_GetTicksNS();float dt=(float)((now-last)/1e9);last=now;if(dt>0.05f)dt=0.05f;
    g.update(dt);g.draw(r);r.present(winFbo,W,H);SDL_GL_SwapWindow(win);
  }
  SDL_Quit();return 0;}
