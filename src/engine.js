// Sabak Engine - main class. Owns renderer, scene, physics, input, audio and the game loop.
import { Renderer, defaultMaterial } from './renderer.js';
import { Scene, Node } from './scene.js';
import { Physics } from './physics.js';
import { Input } from './input.js';
import { Audio } from './audio.js';
import { loadGlb } from './gltf.js';
import { Driver } from './driver.js';

export class Engine {
  constructor(canvas, opts = {}) {
    this.canvas = canvas;
    this.renderer = new Renderer(canvas, opts);
    this.scene = new Scene();
    this.physics = new Physics();
    this.input = new Input(canvas);
    this.audio = new Audio();
    this.animators = new Set();
    this.models = new Map();
    this.time = 0; this.dt = 0; this.fps = 60; this._acc = 0; this._frames = 0;
    this.onUpdate = null; this.running = false;
    this.ui = this._makeUI(canvas);
    this.driver = new Driver(this); if (typeof window !== 'undefined') window.sabak = this.driver;
    addEventListener('pointerdown', () => this.audio.unlock(), { once: true });
  }
  _makeUI(canvas) {
    const c = document.createElement('canvas'); Object.assign(c.style, { position: 'absolute', left: 0, top: 0, width: '100%', height: '100%', pointerEvents: 'none' });
    canvas.parentElement.appendChild(c); return { canvas: c, ctx: c.getContext('2d'), text: [] };
  }
  async loadModel(url) { if (!this.models.has(url)) this.models.set(url, loadGlb(url, this.renderer)); return this.models.get(url); }
  async spawn(url, { name, parent } = {}) {
    const asset = await this.loadModel(url), inst = asset.instantiate(name || url.split('/').pop());
    this.scene.add(inst.root, parent); this.animators.add(inst.animator); inst.root.components.animator = inst.animator;
    inst.root.userData.nodes = inst.nodes; inst.root.userData.asset = asset; return inst.root;
  }
  despawn(node) { node.forEach((n) => this.animators.delete(n.components.animator)); this.scene.remove(node); }
  // mesh node from generated primitive data
  mesh(data, material = {}, { name = 'mesh', parent } = {}) {
    const n = new Node(name), mat = { ...defaultMaterial(), ...material };
    let mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9]; const p = data.positions;
    for (let i = 0; i < p.length; i += 3) for (let k = 0; k < 3; k++) { mn[k] = Math.min(mn[k], p[i + k]); mx[k] = Math.max(mx[k], p[i + k]); }
    n.mesh = { prims: [{ data, material: mat }], skin: null, bounds: { c: mn.map((v, k) => (v + mx[k]) / 2), r: Math.hypot(...mn.map((v, k) => (mx[k] - v) / 2)) } };
    return this.scene.add(n, parent);
  }
  // many copies of one mesh drawn in one call. matrices: array of Float32Array(16) or one flat Float32Array
  instanced(data, material, matrices, name = 'instanced') {
    const flat = matrices instanceof Float32Array ? matrices : (() => { const f = new Float32Array(matrices.length * 16); matrices.forEach((m, i) => f.set(m, i * 16)); return f; })();
    const n = this.mesh(data, material, { name }); n.instances = { matrices: flat, count: flat.length / 16, version: 1 }; n.mesh.bounds = null; return n;
  }
  start() {
    if (this.running) return; this.running = true; let last = performance.now();
    const frame = (now) => {
      if (!this.running) return;
      const dt = Math.min(0.05, (now - last) / 1000); last = now; if (!this.driver.paused) this.tick(dt); requestAnimationFrame(frame);
    };
    requestAnimationFrame(frame);
  }
  tick(dt) {
    this.dt = dt; this.time += dt;
    this._acc += dt; this._frames++; if (this._acc >= 0.5) { this.fps = Math.round(this._frames / this._acc); this._acc = 0; this._frames = 0; }
    this.onUpdate && this.onUpdate(dt, this);
    this.physics.update(dt);
    for (const a of this.animators) a.update(dt);
    this.scene.update(dt, this);
    for (const a of this.animators) a.updateSkins();
    this.audio.listener = this.scene.camera.position;
    this.renderer.render(this.scene);
    this._drawUI();
    this.input.endFrame();
  }
  _drawUI() {
    const u = this.ui, c = u.canvas, ctx = u.ctx, dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = c.clientWidth * dpr, h = c.clientHeight * dpr; if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
    ctx.clearRect(0, 0, w, h); ctx.save(); ctx.scale(dpr, dpr);
    const j = this.input.joystick;
    if (this.input.touchUI) {
      const cx = j.active ? j.cx : 90, cy = j.active ? j.cy : c.clientHeight - 100;
      ctx.globalAlpha = 0.35; ctx.fillStyle = '#fff'; ctx.beginPath(); ctx.arc(cx, cy, j.r, 0, 7); ctx.fill();
      ctx.globalAlpha = 0.6; ctx.beginPath(); ctx.arc(cx + j.x * j.r, cy + j.y * j.r, 26, 0, 7); ctx.fill();
    }
    ctx.globalAlpha = 1; ctx.fillStyle = '#fff'; ctx.shadowColor = '#000'; ctx.shadowBlur = 4; ctx.font = '600 14px system-ui, sans-serif';
    let y = 20; for (const t of u.text) { ctx.fillText(t, 12, y); y += 18; }
    if (this.showStats) ctx.fillText(`${this.fps} fps  ${this.renderer.stats.draws} draws  ${Math.round(this.renderer.stats.tris / 1000)}k tris`, 12, c.clientHeight - 12);
    ctx.restore();
  }
}
