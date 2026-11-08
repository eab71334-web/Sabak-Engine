// Sabak Engine - scene graph + lightweight ECS (components are plain objects on a node; systems run every frame)
import { mat4, quat } from './math.js';

export class Node {
  constructor(name = 'node') {
    this.name = name;
    this.position = [0, 0, 0];
    this.rotation = [0, 0, 0, 1];
    this.scale = [1, 1, 1];
    this.parent = null;
    this.children = [];
    this.local = mat4.identity();
    this.world = mat4.identity();
    this.visible = true;
    this.mesh = null;      // { prims:[{data|gpu, material}], skin:null|Skin, bounds:{c:[x,y,z], r} }
    this.instances = null; // { matrices: Float32Array, count }
    this.components = {};  // name -> object
    this.userData = {};
  }
  add(c) { if (c.parent) c.parent.remove(c); c.parent = this; this.children.push(c); return c; }
  remove(c) { const i = this.children.indexOf(c); if (i >= 0) this.children.splice(i, 1); c.parent = null; return c; }
  find(name) { if (this.name === name) return this; for (const c of this.children) { const r = c.find(name); if (r) return r; } return null; }
  setPosition(x, y, z) { this.position[0] = x; this.position[1] = y; this.position[2] = z; return this; }
  setEuler(x, y, z) { this.rotation = quat.fromEuler(x, y, z); return this; }
  updateWorld(parentWorld) {
    mat4.fromTRS(this.position, this.rotation, this.scale, this.local);
    if (parentWorld) mat4.mul(parentWorld, this.local, this.world); else this.world.set(this.local);
    for (const c of this.children) c.updateWorld(this.world);
  }
  forEach(fn) { fn(this); for (const c of this.children) c.forEach(fn); }
  get worldPosition() { return [this.world[12], this.world[13], this.world[14]]; }
}

export class Scene {
  constructor() {
    this.root = new Node('root');
    this.systems = [];
    this.camera = { fov: 60 * Math.PI / 180, near: 0.1, far: 400, position: [0, 5, 10], target: [0, 0, 0], up: [0, 1, 0] };
    this.sun = { dir: [-0.5, -0.8, -0.35], color: [1.0, 0.96, 0.88], intensity: 3.2 };
    this.ambient = { sky: [0.42, 0.55, 0.75], ground: [0.28, 0.24, 0.2], intensity: 1.0 };
    this.fog = { color: [0.62, 0.72, 0.85], density: 0.006 };
    this.sky = { top: [0.22, 0.42, 0.78], horizon: [0.78, 0.86, 0.95], bottom: [0.45, 0.5, 0.45] };
    this.nodes = new Set(); // entities registered for queries
  }
  add(node, parent = this.root) { parent.add(node); node.forEach((n) => this.nodes.add(n)); return node; }
  remove(node) { node.forEach((n) => this.nodes.delete(n)); node.parent && node.parent.remove(node); }
  query(component) { const out = []; for (const n of this.nodes) if (n.components[component]) out.push(n); return out; }
  addSystem(fn) { this.systems.push(fn); return fn; }
  update(dt, engine) {
    for (const s of this.systems) s(dt, this, engine);
    this.root.updateWorld(null);
  }
}
