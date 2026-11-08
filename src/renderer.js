// Sabak Engine - WebGL2 renderer: PBR-lite forward shading, cascaded-free directional shadow map (PCF), skinning via joint texture,
// hardware instancing, procedural sky, exponential fog, ACES tone mapping, frustum culling.
import { mat4, v3 } from './math.js';

const VS = `#version 300 es
precision highp float;
layout(location=0) in vec3 a_pos;
layout(location=1) in vec3 a_nrm;
layout(location=2) in vec2 a_uv;
layout(location=3) in vec4 a_col;
layout(location=4) in uvec4 a_joints;
layout(location=5) in vec4 a_weights;
layout(location=6) in vec4 a_i0;
layout(location=7) in vec4 a_i1;
layout(location=8) in vec4 a_i2;
layout(location=9) in vec4 a_i3;
uniform mat4 u_model, u_viewProj, u_lightVP;
uniform bool u_skinned, u_instanced;
uniform sampler2D u_jointTex;
out vec3 v_nrm; out vec3 v_wpos; out vec2 v_uv; out vec4 v_col; out vec4 v_lpos;
mat4 joint(uint j){ int b=int(j)*4; return mat4(texelFetch(u_jointTex,ivec2(b,0),0),texelFetch(u_jointTex,ivec2(b+1,0),0),texelFetch(u_jointTex,ivec2(b+2,0),0),texelFetch(u_jointTex,ivec2(b+3,0),0)); }
void main(){
  mat4 m = u_instanced ? mat4(a_i0,a_i1,a_i2,a_i3) * u_model : u_model;
  vec4 p = vec4(a_pos,1.0); vec3 n = a_nrm;
  if(u_skinned){
    mat4 s = joint(a_joints.x)*a_weights.x + joint(a_joints.y)*a_weights.y + joint(a_joints.z)*a_weights.z + joint(a_joints.w)*a_weights.w;
    p = s*p; n = mat3(s)*n; m = mat4(1.0);
  }
  vec4 wp = m*p;
  v_wpos = wp.xyz; v_nrm = normalize(mat3(m)*n); v_uv=a_uv; v_col=a_col;
  v_lpos = u_lightVP*wp;
  gl_Position = u_viewProj*wp;
}`;

const FS = `#version 300 es
precision highp float;
precision highp sampler2DShadow;
in vec3 v_nrm; in vec3 v_wpos; in vec2 v_uv; in vec4 v_col; in vec4 v_lpos;
uniform vec4 u_base; uniform vec3 u_emissive; uniform float u_metal, u_rough, u_alphaCut;
uniform bool u_hasTex, u_useVC, u_unlit, u_doubleSided, u_receive;
uniform sampler2D u_tex; uniform sampler2DShadow u_shadow;
uniform vec3 u_cam, u_sunDir, u_sunCol, u_skyAmb, u_gndAmb, u_fogCol; uniform float u_fogDen, u_exposure;
uniform float u_shadowTexel;
out vec4 o;
float shadow(vec3 n, vec3 l){
  if(!u_receive) return 1.0;
  vec3 pr = v_lpos.xyz/v_lpos.w*0.5+0.5;
  if(pr.x<0.0||pr.x>1.0||pr.y<0.0||pr.y>1.0||pr.z>1.0) return 1.0;
  float bias = max(0.0012*(1.0-dot(n,l)),0.0004);
  float s=0.0;
  for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) s += texture(u_shadow, vec3(pr.xy+vec2(x,y)*u_shadowTexel, pr.z-bias));
  float fade = smoothstep(0.80,1.0,max(abs(pr.x-0.5),abs(pr.y-0.5))*2.0);
  return mix(s/9.0,1.0,fade);
}
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }
void main(){
  vec4 base = u_base;
  if(u_hasTex) base *= texture(u_tex, v_uv);
  if(u_useVC) base *= v_col;
  if(base.a < u_alphaCut) discard;
  vec3 col;
  vec3 n = normalize(v_nrm);
  vec3 V = normalize(u_cam - v_wpos);
  if(u_doubleSided && dot(n,V)<0.0) n=-n;
  if(u_unlit){ col = base.rgb + u_emissive; }
  else {
    vec3 albedo = pow(base.rgb, vec3(2.2));
    vec3 L = normalize(-u_sunDir);
    float ndl = max(dot(n,L),0.0);
    float sh = shadow(n,L);
    vec3 H = normalize(L+V);
    float ndh = max(dot(n,H),0.0), ndv = max(dot(n,V),0.001);
    float a = max(u_rough*u_rough,0.04), a2=a*a;
    float d = ndh*ndh*(a2-1.0)+1.0; float D = a2/(3.14159*d*d);
    vec3 F0 = mix(vec3(0.04),albedo,u_metal);
    vec3 F = F0+(1.0-F0)*pow(1.0-max(dot(H,V),0.0),5.0);
    float k=(u_rough+1.0)*(u_rough+1.0)/8.0; float Gv = ndv/(ndv*(1.0-k)+k), Gl = ndl/(ndl*(1.0-k)+k);
    vec3 spec = D*F*Gv*Gl/max(4.0*ndv*ndl,0.001);
    vec3 diff = albedo*(1.0-u_metal)/3.14159*(1.0-F);
    vec3 direct = (diff+spec)*u_sunCol*ndl*sh;
    float hemi = n.y*0.5+0.5;
    vec3 amb = mix(pow(u_gndAmb,vec3(2.2)),pow(u_skyAmb,vec3(2.2)),hemi);
    vec3 ambient = albedo*amb*(1.0-u_metal*0.7) + F0*amb*0.25*pow(1.0-ndv,3.0)*0.0;
    float rim = pow(1.0-ndv,3.0)*0.18*(0.35+0.65*sh);
    col = direct + ambient + rim*pow(u_skyAmb,vec3(2.2)) + pow(u_emissive,vec3(2.2));
    col = aces(col*u_exposure);
    col = pow(col, vec3(1.0/2.2));
  }
  float dist = length(u_cam - v_wpos);
  float f = 1.0-exp(-u_fogDen*u_fogDen*dist*dist);
  col = mix(col, u_fogCol, clamp(f,0.0,1.0));
  o = vec4(col, base.a);
}`;

const SHADOW_FS = `#version 300 es
precision highp float; uniform sampler2D u_tex; uniform bool u_hasTex; uniform vec4 u_base; uniform float u_alphaCut; in vec2 v_uv; in vec4 v_col;
void main(){ float a=u_base.a; if(u_hasTex) a*=texture(u_tex,v_uv).a; if(a<u_alphaCut) discard; }`;

const SKY_VS = `#version 300 es
out vec2 v_p; void main(){ vec2 p=vec2((gl_VertexID<<1)&2, gl_VertexID&2); v_p=p*2.0-1.0; gl_Position=vec4(v_p,0.9999,1.0); }`;
const SKY_FS = `#version 300 es
precision highp float; in vec2 v_p; out vec4 o;
uniform mat4 u_invVP; uniform vec3 u_cam, u_top, u_hor, u_bot, u_sunDir, u_sunCol, u_fogCol;
void main(){
  vec4 w = u_invVP*vec4(v_p,1.0,1.0); vec3 d = normalize(w.xyz/w.w - u_cam);
  float h = d.y;
  vec3 c = h>0.0 ? mix(u_hor,u_top,pow(clamp(h,0.0,1.0),0.55)) : mix(u_hor,u_bot,pow(clamp(-h*3.0,0.0,1.0),0.7));
  vec3 s = normalize(-u_sunDir);
  float sd = max(dot(d,s),0.0);
  c += u_sunCol*(pow(sd,900.0)*5.0 + pow(sd,12.0)*0.18 + pow(sd,3.0)*0.06);
  c = mix(c, u_fogCol, exp(-max(h,0.0)*14.0)*0.55);
  o = vec4(c,1.0);
}`;

function compile(gl, vs, fs) {
  const mk = (t, src) => { const s = gl.createShader(t); gl.shaderSource(s, src); gl.compileShader(s); if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error('shader: ' + gl.getShaderInfoLog(s)); return s; };
  const p = gl.createProgram(); gl.attachShader(p, mk(gl.VERTEX_SHADER, vs)); gl.attachShader(p, mk(gl.FRAGMENT_SHADER, fs)); gl.linkProgram(p);
  if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error('link: ' + gl.getProgramInfoLog(p));
  const u = {}; const n = gl.getProgramParameter(p, gl.ACTIVE_UNIFORMS);
  for (let i = 0; i < n; i++) { const info = gl.getActiveUniform(p, i); u[info.name.replace('[0]', '')] = gl.getUniformLocation(p, info.name); }
  return { p, u };
}

export const defaultMaterial = () => ({ base: [1, 1, 1, 1], metal: 0, rough: 0.8, emissive: [0, 0, 0], tex: null, useVC: false, unlit: false, doubleSided: false, alpha: 'OPAQUE', cutoff: 0.5, receive: true, cast: true });

export class Renderer {
  constructor(canvas, opts = {}) {
    const gl = canvas.getContext('webgl2', { antialias: opts.antialias !== false, alpha: false, powerPreference: 'high-performance', preserveDrawingBuffer: !!opts.preserve });
    if (!gl) throw new Error('WebGL2 not supported');
    this.gl = gl; this.canvas = canvas;
    this.main = compile(gl, VS, FS);
    this.shadowProg = compile(gl, VS, SHADOW_FS);
    this.sky = compile(gl, SKY_VS, SKY_FS);
    this.exposure = 1.0;
    this.shadowSize = opts.shadowSize ?? 2048;
    this.shadowRadius = opts.shadowRadius ?? 45;
    this.shadows = opts.shadows !== false;
    this.stats = { draws: 0, tris: 0 };
    this.whiteTex = this._tex1x1([255, 255, 255, 255]);
    this._initShadow();
    this._jointTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this._jointTex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, 4 * 256, 1, 0, gl.RGBA, gl.FLOAT, null);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    this._skyVao = gl.createVertexArray();
    this.viewProj = mat4.identity(); this.view = mat4.identity(); this.proj = mat4.identity(); this.lightVP = mat4.identity();
    this.camPos = [0, 0, 0];
  }
  _tex1x1(rgba) { const gl = this.gl, t = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D, t); gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(rgba)); return t; }
  _initShadow() {
    const gl = this.gl, S = this.shadowSize;
    this.shadowTex = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D, this.shadowTex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.DEPTH_COMPONENT24, S, S, 0, gl.DEPTH_COMPONENT, gl.UNSIGNED_INT, null);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_COMPARE_MODE, gl.COMPARE_REF_TO_TEXTURE); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_COMPARE_FUNC, gl.LEQUAL);
    this.shadowFbo = gl.createFramebuffer(); gl.bindFramebuffer(gl.FRAMEBUFFER, this.shadowFbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, this.shadowTex, 0);
    gl.drawBuffers([gl.NONE]); gl.readBuffer(gl.NONE);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }
  // ---- GPU resources
  createTexture(source, { repeat = true, srgb = false } = {}) {
    const gl = this.gl, t = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D, t);
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source);
    gl.generateMipmap(gl.TEXTURE_2D);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    const w = repeat ? gl.REPEAT : gl.CLAMP_TO_EDGE; gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, w); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, w);
    const ext = gl.getExtension('EXT_texture_filter_anisotropic'); if (ext) gl.texParameterf(gl.TEXTURE_2D, ext.TEXTURE_MAX_ANISOTROPY_EXT, 4);
    return t;
  }
  uploadPrim(d) {
    if (d.gpu) return d.gpu;
    const gl = this.gl, vao = gl.createVertexArray(); gl.bindVertexArray(vao);
    const buf = (data, loc, size, type = gl.FLOAT, integer = false) => {
      const b = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, b); gl.bufferData(gl.ARRAY_BUFFER, data, gl.STATIC_DRAW); gl.enableVertexAttribArray(loc);
      integer ? gl.vertexAttribIPointer(loc, size, type, 0, 0) : gl.vertexAttribPointer(loc, size, type, false, 0, 0);
    };
    const n = d.positions.length / 3;
    buf(d.positions, 0, 3);
    buf(d.normals || new Float32Array(n * 3).fill(0), 1, 3);
    buf(d.uvs || new Float32Array(n * 2), 2, 2);
    if (d.colors) { const c = d.colors; buf(c, 3, 4); } else gl.vertexAttrib4f(3, 1, 1, 1, 1);
    if (d.joints) { buf(d.joints instanceof Uint16Array ? d.joints : new Uint16Array(d.joints), 4, 4, gl.UNSIGNED_SHORT, true); buf(d.weights, 5, 4); }
    else { gl.vertexAttribI4ui(4, 0, 0, 0, 0); gl.vertexAttrib4f(5, 0, 0, 0, 0); }
    const ib = gl.createBuffer(); gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ib);
    const idx = d.indices || Uint32Array.from({ length: n }, (_, i) => i);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, idx, gl.STATIC_DRAW);
    gl.bindVertexArray(null);
    d.gpu = { vao, count: idx.length, type: idx instanceof Uint16Array ? gl.UNSIGNED_SHORT : gl.UNSIGNED_INT, instBuf: null };
    return d.gpu;
  }

  resize() {
    const dpr = Math.min(window.devicePixelRatio || 1, this.maxDpr ?? 2);
    const w = Math.max(1, Math.floor(this.canvas.clientWidth * dpr)), h = Math.max(1, Math.floor(this.canvas.clientHeight * dpr));
    if (this.canvas.width !== w || this.canvas.height !== h) { this.canvas.width = w; this.canvas.height = h; }
  }

  // ---- frame
  render(scene) {
    const gl = this.gl;
    this.resize();
    const cam = scene.camera, W = this.canvas.width, H = this.canvas.height;
    mat4.perspective(cam.fov, W / H, cam.near, cam.far, this.proj);
    mat4.lookAt(cam.position, cam.target, cam.up, this.view);
    mat4.mul(this.proj, this.view, this.viewProj);
    this.camPos = cam.position;
    // collect draws
    const opaque = [], blend = [], casters = [];
    const planes = this._frustum(this.viewProj);
    scene.root.forEach((n) => {
      if (!n.mesh || !n.visible) return;
      const skinned = !!n.mesh.skin;
      let inView = true;
      if (!skinned && n.mesh.bounds && !n.instances) {
        const c = mat4.transformPoint(n.world, n.mesh.bounds.c), s = Math.max(Math.hypot(n.world[0], n.world[1], n.world[2]), Math.hypot(n.world[4], n.world[5], n.world[6]), Math.hypot(n.world[8], n.world[9], n.world[10]));
        for (const p of planes) if (p[0] * c[0] + p[1] * c[1] + p[2] * c[2] + p[3] < -n.mesh.bounds.r * s) { inView = false; break; }
      }
      for (const pr of n.mesh.prims) {
        const item = { node: n, prim: pr, skinned };
        if (pr.material.cast !== false) casters.push(item);
        if (!inView) continue;
        (pr.material.alpha === 'BLEND' ? blend : opaque).push(item);
      }
    });
    blend.sort((a, b) => v3.dist(b.node.worldPosition, this.camPos) - v3.dist(a.node.worldPosition, this.camPos));
    this.stats.draws = 0; this.stats.tris = 0;
    // shadow pass
    if (this.shadows) {
      this._fitLight(scene);
      gl.bindFramebuffer(gl.FRAMEBUFFER, this.shadowFbo);
      gl.viewport(0, 0, this.shadowSize, this.shadowSize);
      gl.clear(gl.DEPTH_BUFFER_BIT); gl.enable(gl.DEPTH_TEST); gl.depthMask(true); gl.colorMask(false, false, false, false);
      gl.enable(gl.POLYGON_OFFSET_FILL); gl.polygonOffset(2, 4);
      gl.useProgram(this.shadowProg.p);
      this._setCommon(this.shadowProg, this.lightVP, scene, true);
      for (const it of casters) if (it.prim.material.alpha !== 'BLEND') this._draw(this.shadowProg, it, true);
      gl.disable(gl.POLYGON_OFFSET_FILL); gl.colorMask(true, true, true, true);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    }
    gl.viewport(0, 0, W, H);
    gl.clearColor(0, 0, 0, 1); gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    this._drawSky(scene);
    gl.enable(gl.DEPTH_TEST); gl.depthFunc(gl.LEQUAL);
    gl.useProgram(this.main.p);
    this._setCommon(this.main, this.viewProj, scene, false);
    gl.activeTexture(gl.TEXTURE2); gl.bindTexture(gl.TEXTURE_2D, this.shadowTex); gl.uniform1i(this.main.u.u_shadow, 2);
    gl.uniform1f(this.main.u.u_shadowTexel, 1 / this.shadowSize);
    for (const it of opaque) this._draw(this.main, it, false);
    gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA); gl.depthMask(false);
    for (const it of blend) this._draw(this.main, it, false);
    gl.depthMask(true); gl.disable(gl.BLEND);
  }
  _frustum(m) {
    const P = []; const row = (i) => [m[i], m[4 + i], m[8 + i], m[12 + i]];
    const r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
    const add = (a, b, s) => P.push([a[0] + s * b[0], a[1] + s * b[1], a[2] + s * b[2], a[3] + s * b[3]]);
    add(r3, r0, 1); add(r3, r0, -1); add(r3, r1, 1); add(r3, r1, -1); add(r3, r2, 1); add(r3, r2, -1);
    return P.map((p) => { const l = Math.hypot(p[0], p[1], p[2]) || 1; return [p[0] / l, p[1] / l, p[2] / l, p[3] / l]; });
  }
  _fitLight(scene) {
    const R = this.shadowRadius, f = this.shadowFocus || scene.camera.target;
    const d = v3.norm(scene.sun.dir), eye = v3.sub(f, v3.mul(d, R * 2));
    const view = mat4.lookAt(eye, f, Math.abs(d[1]) > 0.95 ? [0, 0, 1] : [0, 1, 0]);
    // snap to texel grid so shadows do not shimmer when the focus moves
    const proj = mat4.ortho(-R, R, -R, R, 0.1, R * 4);
    const vp = mat4.mul(proj, view);
    const o = mat4.transformPoint(vp, [0, 0, 0]); const half = this.shadowSize / 2;
    const sx = Math.round(o[0] * half) / half - o[0], sy = Math.round(o[1] * half) / half - o[1];
    proj[12] += sx; proj[13] += sy;
    mat4.mul(proj, view, this.lightVP);
  }
  _drawSky(scene) {
    const gl = this.gl, s = this.sky; gl.useProgram(s.p); gl.disable(gl.DEPTH_TEST);
    gl.uniformMatrix4fv(s.u.u_invVP, false, mat4.invert(this.viewProj));
    gl.uniform3fv(s.u.u_cam, this.camPos); gl.uniform3fv(s.u.u_top, scene.sky.top); gl.uniform3fv(s.u.u_hor, scene.sky.horizon); gl.uniform3fv(s.u.u_bot, scene.sky.bottom);
    gl.uniform3fv(s.u.u_sunDir, v3.norm(scene.sun.dir)); gl.uniform3fv(s.u.u_sunCol, scene.sun.color); gl.uniform3fv(s.u.u_fogCol, scene.fog.color);
    gl.bindVertexArray(this._skyVao); gl.drawArrays(gl.TRIANGLES, 0, 3);
  }
  _setCommon(P, vp, scene, shadowPass) {
    const gl = this.gl, u = P.u;
    gl.uniformMatrix4fv(u.u_viewProj, false, vp);
    gl.uniformMatrix4fv(u.u_lightVP, false, this.lightVP);
    gl.activeTexture(gl.TEXTURE1); gl.bindTexture(gl.TEXTURE_2D, this._jointTex); gl.uniform1i(u.u_jointTex, 1);
    if (shadowPass) return;
    const s = scene.sun, a = scene.ambient;
    gl.uniform3fv(u.u_cam, this.camPos); gl.uniform3fv(u.u_sunDir, v3.norm(s.dir));
    gl.uniform3fv(u.u_sunCol, v3.mul(s.color, s.intensity)); gl.uniform3fv(u.u_skyAmb, v3.mul(a.sky, a.intensity)); gl.uniform3fv(u.u_gndAmb, v3.mul(a.ground, a.intensity));
    gl.uniform3fv(u.u_fogCol, scene.fog.color); gl.uniform1f(u.u_fogDen, scene.fog.density); gl.uniform1f(u.u_exposure, this.exposure);
  }
  _draw(P, it, shadowPass) {
    const gl = this.gl, u = P.u, n = it.node, m = it.prim.material, d = it.prim.data;
    const g = this.uploadPrim(d);
    gl.uniformMatrix4fv(u.u_model, false, n.world);
    gl.uniform1i(u.u_skinned, it.skinned ? 1 : 0);
    if (it.skinned) {
      const j = n.mesh.skin.matrices; gl.activeTexture(gl.TEXTURE1); gl.bindTexture(gl.TEXTURE_2D, this._jointTex);
      gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, Math.min(j.length / 4, 4 * 256), 1, gl.RGBA, gl.FLOAT, j);
    }
    gl.uniform1i(u.u_instanced, n.instances ? 1 : 0);
    gl.uniform1f(u.u_alphaCut, m.alpha === 'MASK' ? m.cutoff : 0.0);
    gl.uniform4fv(u.u_base, m.base); gl.uniform1i(u.u_hasTex, m.tex ? 1 : 0);
    gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, m.tex || this.whiteTex); gl.uniform1i(u.u_tex, 0);
    if (m.doubleSided) gl.disable(gl.CULL_FACE); else { gl.enable(gl.CULL_FACE); gl.cullFace(gl.BACK); }
    if (!shadowPass) {
      gl.uniform1i(u.u_useVC, m.useVC ? 1 : 0); gl.uniform1i(u.u_unlit, m.unlit ? 1 : 0); gl.uniform1i(u.u_doubleSided, m.doubleSided ? 1 : 0);
      gl.uniform1i(u.u_receive, m.receive !== false && this.shadows ? 1 : 0);
      gl.uniform3fv(u.u_emissive, m.emissive); gl.uniform1f(u.u_metal, m.metal); gl.uniform1f(u.u_rough, m.rough);
    }
    gl.bindVertexArray(g.vao);
    if (n.instances) {
      if (!g.instBuf) { g.instBuf = gl.createBuffer(); }
      gl.bindBuffer(gl.ARRAY_BUFFER, g.instBuf);
      if (n.instances.dirty !== false || g.instVer !== n.instances.version) { gl.bufferData(gl.ARRAY_BUFFER, n.instances.matrices, gl.DYNAMIC_DRAW); g.instVer = n.instances.version; n.instances.dirty = false; }
      for (let k = 0; k < 4; k++) { gl.enableVertexAttribArray(6 + k); gl.vertexAttribPointer(6 + k, 4, gl.FLOAT, false, 64, k * 16); gl.vertexAttribDivisor(6 + k, 1); }
      gl.drawElementsInstanced(gl.TRIANGLES, g.count, g.type, 0, n.instances.count);
      for (let k = 0; k < 4; k++) { gl.vertexAttribDivisor(6 + k, 0); gl.disableVertexAttribArray(6 + k); }
      this.stats.tris += g.count / 3 * n.instances.count;
    } else { gl.drawElements(gl.TRIANGLES, g.count, g.type, 0); this.stats.tris += g.count / 3; }
    this.stats.draws++;
  }
}
