// Sabak Core - C++ gameplay scripts. Write a class deriving from Script, then register it with SABAK_SCRIPT(Name).
// Scripts live in game/scripts/*.cpp and are compiled together with the engine (iOS cannot compile code on the device).
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "vmath.h"
namespace sk {
struct Entity; class Scene;
struct Input { float moveX=0,moveY=0; float camYaw=0; bool jump=false; };
struct ScriptContext { Scene* scene=nullptr; Input* input=nullptr; float time=0; };
struct Script { Entity* self=nullptr; ScriptContext* ctx=nullptr; virtual ~Script(){} virtual void onStart(){} virtual void onUpdate(float){} };
using ScriptFactory=std::function<Script*()>;
std::map<std::string,ScriptFactory>& scriptRegistry();
struct ScriptRegistrar { ScriptRegistrar(const char* n,ScriptFactory f){scriptRegistry()[n]=f;} };
#define SABAK_SCRIPT(T) static ::sk::ScriptRegistrar sabak_reg_##T(#T,[]()->::sk::Script*{return new T();});
}
