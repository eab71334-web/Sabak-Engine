// Sabak Engine - GLB/glTF 2.0 loader: meshes, PBR materials, textures, node hierarchy, skins and animations.
import { Node } from './scene.js';
import { mat4, quat } from './math.js';
import { defaultMaterial } from './renderer.js';

const COMP = { 5120: Int8Array, 5121: Uint8Array, 5122: Int16Array, 5123: Uint16Array, 5125: Uint32Array, 5126: Float32Array };
const NUM = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4, MAT2: 4, MAT3: 9, MAT4: 16 };

function parseGlb(buf) {
  const dv = new DataView(buf);
  if (dv.getUint32(0, true) !== 0x46546c67) throw new Error('not a GLB');
  let off = 12, json = null, bin = null;
  while (off < buf.byteLength) {
    const len = dv.getUint32(off, true), type = dv.getUint32(off + 4, true); off += 8;
    if (type === 0x4e4f534a) json = JSON.parse(new TextDecoder().decode(new Uint8Array(buf, off, len)));
    else if (type === 0x004e4942) bin = buf.slice(off, off + len);
    off += len;
  }
  return { json, bin };
}

function accessor(json, bin, i) {
  const a = json.accessors[i], bv = json.bufferViews[a.bufferView ?? 0], T = COMP[a.componentType], n = NUM[a.type];
  const base = (bv.byteOffset || 0) + (a.byteOffset || 0), stride = bv.byteStride || 0, esz = T.BYTES_PER_ELEMENT * n;
  let out;
  if (!stride || stride === esz) out = new T(bin.slice(base, base + a.count * esz));
  else { out = new T(a.count * n); const src = new DataView(bin); for (let k = 0; k < a.count; k++) for (let c = 0; c < n; c++) { const o = base + k * stride + c * T.BYTES_PER_ELEMENT; out[k * n + c] = T === Float32Array ? src.getFloat32(o, true) : T === Uint16Array ? src.getUint16(o, true) : T === Uint8Array ? src.getUint8(o) : T === Uint32Array ? src.getUint32(o, true) : T === Int16Array ? src.getInt16(o, true) : src.getInt8(o); } }
  if (a.normalized && T !== Float32Array) { const f = new Float32Array(out.length), d = T === Uint8Array ? 255 : T === Uint16Array ? 65535 : T === Int8Array ? 127 : 32767; for (let k = 0; k < out.length; k++) f[k] = Math.max(out[k] / d, -1); out = f; }
  return { data: out, count: a.count, n, min: a.min, max: a.max };
}

export class GltfAsset {
  constructor() { this.nodes = []; this.roots = []; this.skins = []; this.animations = []; this.meshes = []; this.materials = []; }

  // clone the node tree. returns { root, animator, nodes }
  instantiate(name = 'model') {
    const root = new Node(name), nodes = this.nodes.map((t) => { const n = new Node(t.name); n.position = [...t.t]; n.rotation = [...t.r]; n.scale = [...t.s]; return n; });
    this.nodes.forEach((t, i) => { t.children.forEach((c) => nodes[i].add(nodes[c])); });
    this.roots.forEach((r) => root.add(nodes[r]));
    const skinsInst = this.skins.map((s) => ({ jointNodes: s.joints.map((j) => nodes[j]), ibm: s.ibm, matrices: new Float32Array(s.joints.length * 16), _tmp: new Float32Array(16) }));
    this.nodes.forEach((t, i) => {
      if (t.mesh != null) { const m = this.meshes[t.mesh]; nodes[i].mesh = { prims: m.prims, bounds: m.bounds, skin: t.skin != null ? skinsInst[t.skin] : null }; }
    });
    const animator = new Animator(this, nodes);
    animator.skins = skinsInst;
    return { root, nodes, animator, skins: skinsInst };
  }
}

export class Animator {
  constructor(asset, nodes) {
    this.asset = asset; this.nodes = nodes; this.clips = new Map(asset.animations.map((a) => [a.name, a]));
    this.layers = []; // {clip,time,weight,speed,loop,fadeRate,target}
    this.rest = asset.nodes.map((t) => ({ t: t.t, r: t.r, s: t.s }));
    this.skins = [];
    this.current = null;
  }
  has(name) { return this.clips.has(name); }
  play(name, { loop = true, fade = 0.2, speed = 1, restart = false } = {}) {
    const clip = this.clips.get(name); if (!clip) return false;
    const cur = this.layers.find((l) => l.clip === clip && l.target > 0);
    if (cur && !restart) { cur.speed = speed; cur.loop = loop; return true; }
    for (const l of this.layers) { l.target = 0; l.fadeRate = fade > 0 ? 1 / fade : 1e9; }
    this.layers.push({ clip, time: 0, weight: fade > 0 ? 0 : 1, speed, loop, target: 1, fadeRate: fade > 0 ? 1 / fade : 1e9, done: false });
    this.current = name; return true;
  }
  get finished() { const l = this.layers.find((x) => x.target > 0); return !!(l && !l.loop && l.time >= l.clip.duration); }
  update(dt) {
    for (const l of this.layers) {
      l.time += dt * l.speed;
      if (l.loop && l.clip.duration > 0) l.time %= l.clip.duration; else l.time = Math.min(l.time, l.clip.duration);
      l.weight += Math.sign(l.target - l.weight) * Math.min(Math.abs(l.target - l.weight), l.fadeRate * dt);
    }
    this.layers = this.layers.filter((l) => l.weight > 0.001 || l.target > 0);
    if (this.layers.length) {
      const acc = new Map(); let wsum = 0;
      for (const l of this.layers) wsum += l.weight;
      for (const l of this.layers) {
        const w = l.weight / (wsum || 1);
        for (const ch of l.clip.channels) {
          const v = sample(ch, l.time);
          let e = acc.get(ch.node); if (!e) { e = {}; acc.set(ch.node, e); }
          const p = ch.path, cur = e[p];
          if (!cur) e[p] = { v, w };
          else { const tw = cur.w + w, k = w / tw; cur.v = p === 'rotation' ? quat.slerp(cur.v, v, k) : cur.v.map((x, i) => x + (v[i] - x) * k); cur.w = tw; }
        }
      }
      for (const [ni, e] of acc) { const n = this.nodes[ni]; if (e.translation) n.position = e.translation.v; if (e.rotation) n.rotation = e.rotation.v; if (e.scale) n.scale = e.scale.v; }
    }
  }
  // call after the scene updated world matrices
  updateSkins() {
    for (const s of this.skins) {
      for (let j = 0; j < s.jointNodes.length; j++) {
        mat4.mul(s.jointNodes[j].world, s.ibm.subarray(j * 16, j * 16 + 16), s._tmp);
        s.matrices.set(s._tmp, j * 16);
      }
    }
  }
}

function sample(ch, t) {
  const times = ch.times, n = times.length, comp = ch.comp;
  if (n === 1 || t <= times[0]) return Array.from(ch.values.subarray(0, comp));
  if (t >= times[n - 1]) return Array.from(ch.values.subarray((n - 1) * comp, n * comp));
  let lo = 0, hi = n - 1; while (hi - lo > 1) { const m = (lo + hi) >> 1; if (times[m] <= t) lo = m; else hi = m; }
  const k = (t - times[lo]) / (times[hi] - times[lo]), a = ch.values.subarray(lo * comp, lo * comp + comp), b = ch.values.subarray(hi * comp, hi * comp + comp);
  if (ch.interp === 'STEP') return Array.from(a);
  if (ch.path === 'rotation') return quat.slerp(a, b, k);
  return Array.from(a, (x, i) => x + (b[i] - x) * k);
}

export async function loadGlb(url, renderer) {
  const res = await fetch(url); if (!res.ok) throw new Error('fetch ' + url + ' ' + res.status);
  return parseGltf(await res.arrayBuffer(), renderer, new URL(url, location.href).href);
}

export async function parseGltf(buf, renderer, baseUrl = location.href) {
  const { json, bin } = parseGlb(buf), asset = new GltfAsset();
  // textures (cached per image)
  const imgCache = new Map(), texCache = new Map();
  const getTex = async (ti) => {
    if (texCache.has(ti)) return texCache.get(ti);
    const t = json.textures[ti], ii = t.source;
    if (!imgCache.has(ii)) {
      const img = json.images[ii];
      const getBlob = async () => {
        if (img.bufferView != null) { const bv = json.bufferViews[img.bufferView]; return new Blob([bin.slice(bv.byteOffset || 0, (bv.byteOffset || 0) + bv.byteLength)], { type: img.mimeType || 'image/png' }); }
        const r = await fetch(new URL(img.uri, baseUrl).href); if (!r.ok) throw new Error('texture ' + img.uri); return r.blob();
      };
      imgCache.set(ii, getBlob().then((b) => createImageBitmap(b, { premultiplyAlpha: 'none', colorSpaceConversion: 'none' })).then((bm) => renderer.createTexture(bm)).catch(() => null));
    }
    const tex = await imgCache.get(ii); texCache.set(ti, tex); return tex;
  };
  for (const m of json.materials || [{}]) {
    const pbr = m.pbrMetallicRoughness || {}, mat = defaultMaterial();
    mat.base = pbr.baseColorFactor || [1, 1, 1, 1]; mat.metal = pbr.metallicFactor ?? 1; mat.rough = pbr.roughnessFactor ?? 1;
    mat.emissive = m.emissiveFactor || [0, 0, 0]; mat.alpha = m.alphaMode || 'OPAQUE'; mat.cutoff = m.alphaCutoff ?? 0.5; mat.doubleSided = !!m.doubleSided;
    if (pbr.baseColorTexture) mat.tex = await getTex(pbr.baseColorTexture.index);
    asset.materials.push(mat);
  }
  for (const m of json.meshes || []) {
    const prims = []; let mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9];
    for (const p of m.primitives) {
      const at = p.attributes, pos = accessor(json, bin, at.POSITION);
      const data = { positions: pos.data, normals: at.NORMAL != null ? accessor(json, bin, at.NORMAL).data : null, uvs: at.TEXCOORD_0 != null ? accessor(json, bin, at.TEXCOORD_0).data : null };
      if (at.COLOR_0 != null) { const c = accessor(json, bin, at.COLOR_0); if (c.n === 4) data.colors = c.data; else { const f = new Float32Array(c.count * 4); for (let i = 0; i < c.count; i++) { f[i * 4] = c.data[i * 3]; f[i * 4 + 1] = c.data[i * 3 + 1]; f[i * 4 + 2] = c.data[i * 3 + 2]; f[i * 4 + 3] = 1; } data.colors = f; } }
      if (at.JOINTS_0 != null) { data.joints = new Uint16Array(accessor(json, bin, at.JOINTS_0).data); data.weights = accessor(json, bin, at.WEIGHTS_0).data; }
      if (p.indices != null) { const ix = accessor(json, bin, p.indices).data; data.indices = ix instanceof Uint8Array ? Uint16Array.from(ix) : ix; }
      if (!data.normals) data.normals = computeNormals(data.positions, data.indices);
      const base = asset.materials[p.material ?? 0] || defaultMaterial();
      const material = { ...base, useVC: at.COLOR_0 != null };
      prims.push({ data, material });
      if (pos.min && pos.max) for (let i = 0; i < 3; i++) { mn[i] = Math.min(mn[i], pos.min[i]); mx[i] = Math.max(mx[i], pos.max[i]); }
    }
    const c = mn.map((v, i) => (v + mx[i]) / 2), r = Math.hypot(...mn.map((v, i) => (mx[i] - v) / 2));
    asset.meshes.push({ prims, bounds: { c, r } });
  }
  for (const n of json.nodes || []) {
    let t = [0, 0, 0], r = [0, 0, 0, 1], s = [1, 1, 1];
    if (n.matrix) { const m = n.matrix; t = [m[12], m[13], m[14]]; s = [Math.hypot(m[0], m[1], m[2]), Math.hypot(m[4], m[5], m[6]), Math.hypot(m[8], m[9], m[10])]; const rm = [m[0] / s[0], m[1] / s[0], m[2] / s[0], m[4] / s[1], m[5] / s[1], m[6] / s[1], m[8] / s[2], m[9] / s[2], m[10] / s[2]]; r = quat.normalize([rm[5] - rm[7], rm[6] - rm[2], rm[1] - rm[3], 1 + rm[0] + rm[4] + rm[8]]); }
    if (n.translation) t = n.translation; if (n.rotation) r = n.rotation; if (n.scale) s = n.scale;
    asset.nodes.push({ name: n.name || 'node', t, r, s, children: n.children || [], mesh: n.mesh, skin: n.skin });
  }
  for (const s of json.skins || []) {
    const ibm = s.inverseBindMatrices != null ? accessor(json, bin, s.inverseBindMatrices).data : null;
    asset.skins.push({ joints: s.joints, ibm: ibm || Float32Array.from({ length: s.joints.length * 16 }, (_, i) => (i % 16) % 5 === 0 ? 1 : 0) });
  }
  const scene = json.scenes?.[json.scene ?? 0]; asset.roots = scene ? scene.nodes : [0];
  for (const a of json.animations || []) {
    const channels = []; let duration = 0;
    for (const c of a.channels) {
      const s = a.samplers[c.sampler], t = accessor(json, bin, s.input), v = accessor(json, bin, s.output), path = c.target.path;
      if (c.target.node == null) continue;
      const comp = path === 'rotation' ? 4 : 3;
      let values = v.data, times = t.data;
      if (s.interpolation === 'CUBICSPLINE') { const k = new Float32Array(t.count * comp); for (let i = 0; i < t.count; i++) for (let j = 0; j < comp; j++) k[i * comp + j] = v.data[(i * 3 + 1) * comp + j]; values = k; }
      channels.push({ node: c.target.node, path, times, values, comp, interp: s.interpolation === 'STEP' ? 'STEP' : 'LINEAR' });
      duration = Math.max(duration, times[times.length - 1]);
    }
    asset.animations.push({ name: a.name || 'anim' + asset.animations.length, duration, channels });
  }
  return asset;
}

function computeNormals(pos, idx) {
  const n = new Float32Array(pos.length), I = idx || Uint32Array.from({ length: pos.length / 3 }, (_, i) => i);
  for (let i = 0; i < I.length; i += 3) {
    const a = I[i] * 3, b = I[i + 1] * 3, c = I[i + 2] * 3;
    const ux = pos[b] - pos[a], uy = pos[b + 1] - pos[a + 1], uz = pos[b + 2] - pos[a + 2], vx = pos[c] - pos[a], vy = pos[c + 1] - pos[a + 1], vz = pos[c + 2] - pos[a + 2];
    const nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    for (const k of [a, b, c]) { n[k] += nx; n[k + 1] += ny; n[k + 2] += nz; }
  }
  for (let i = 0; i < n.length; i += 3) { const l = Math.hypot(n[i], n[i + 1], n[i + 2]) || 1; n[i] /= l; n[i + 1] /= l; n[i + 2] /= l; }
  return n;
}
