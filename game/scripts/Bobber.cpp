// Example script: makes an entity float up and down and spin.
#include <cmath>
#include "scene.h"
#include "script.h"
using namespace sk;
struct Bobber : Script {
  float baseY=0;
  void onStart() override { baseY=self->pos.y; }
  void onUpdate(float) override { self->pos.y=baseY+std::sin(ctx->time*2.0f+self->id)*0.35f; self->rot.y=ctx->time*60.0f; }
};
SABAK_SCRIPT(Bobber)
