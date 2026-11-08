// Sabak Core - CPU mesh data + primitive generators
#pragma once
#include <vector>
#include <cstdint>
#include <functional>
#include "vmath.h"
namespace sk {
struct Vertex { float p[3]; float n[3]; float c[3]; };
struct MeshData { std::vector<Vertex> v; std::vector<uint32_t> i; };
MeshData makeBox(float w,float h,float d,Vec3 color);
MeshData makeSphere(float r,int seg,int rings,Vec3 color);
MeshData makeCone(float r,float h,int seg,Vec3 color);       // base at y=0, tip at y=h
MeshData makeCylinder(float r0,float r1,float h,int seg,Vec3 color); // base y=0
MeshData makeTerrain(float size,int res,const std::function<float(float,float)>& height,const std::function<Vec3(float,float,float)>& color);
}
