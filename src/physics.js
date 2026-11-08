// Sabak Engine - physics: static boxes + heightfield terrain, dynamic sphere/capsule-ish bodies, triggers, raycasts.
import { clamp } from './math.js';

export class Physics {
  constructor() {
    this.gravity = -24;
    this.boxes = [];      // static AABBs {min:[x,y,z], max:[x,y,z], tag}
    this.bodies = [];     // dynamic bodies
    this.triggers = [];   // {min,max,onEnter,onExit,inside:Set}
    this.heightFn = null; // (x,z) => y
  }
  addBox(cx, cy, cz, w, h, d, tag = '') { const b = { min: [cx - w / 2, cy - h / 2, cz - d / 2], max: [cx + w / 2, cy + h / 2, cz + d / 2], tag }; this.boxes.push(b); return b; }
  addTrigger(cx, cy, cz, w, h, d, onEnter, onExit) { const t = { min: [cx - w / 2, cy - h / 2, cz - d / 2], max: [cx + w / 2, cy + h / 2, cz + d / 2], onEnter, onExit, inside: new Set() }; this.triggers.push(t); return t; }
  // dynamic body: vertical capsule approximated by radius + height; position is at the feet
  addBody(node, { radius = 0.4, height = 1.8, mass = 1, bounce = 0, step = 0.45 } = {}) {
    const b = { node, radius, height, bounce, step, vel: [0, 0, 0], onGround: false, groundY: 0, wish: [0, 0], speedMax: 6, accel: 40, noGravity: false };
    this.bodies.push(b); node.components.body = b; return b;
  }
  removeBody(b) { const i = this.bodies.indexOf(b); if (i >= 0) this.bodies.splice(i, 1); }
  height(x, z) { return this.heightFn ? this.heightFn(x, z) : -1e9; }

  update(dt) {
    const steps = Math.max(1, Math.ceil(dt / (1 / 60)));
    const h = dt / steps;
    for (let s = 0; s < steps; s++) for (const b of this.bodies) this._step(b, h);
    for (const t of this.triggers) for (const b of this.bodies) {
      const p = b.node.position, inside = p[0] >= t.min[0] && p[0] <= t.max[0] && p[1] + b.height * 0.5 >= t.min[1] && p[1] <= t.max[1] && p[2] >= t.min[2] && p[2] <= t.max[2];
      if (inside && !t.inside.has(b)) { t.inside.add(b); t.onEnter && t.onEnter(b); }
      else if (!inside && t.inside.has(b)) { t.inside.delete(b); t.onExit && t.onExit(b); }
    }
  }
  _step(b, dt) {
    const p = b.node.position, v = b.vel;
    // horizontal: accelerate towards wish velocity
    const tx = b.wish[0] * b.speedMax, tz = b.wish[1] * b.speedMax, k = Math.min(1, b.accel * dt * (b.onGround ? 1 : 0.35));
    v[0] += (tx - v[0]) * k; v[2] += (tz - v[2]) * k;
    if (!b.noGravity) v[1] += this.gravity * dt;
    p[0] += v[0] * dt; p[1] += v[1] * dt; p[2] += v[2] * dt;
    // terrain
    b.onGround = false;
    const gy = this.height(p[0], p[2]);
    // slope limit: if terrain is much higher than feet+step, push back
    if (gy > -1e8) {
      if (p[1] <= gy) {
        if (gy - p[1] > b.step + Math.abs(v[1]) * dt && b.wish[0] * b.wish[0] + b.wish[1] * b.wish[1] > 0) {
          p[0] -= v[0] * dt; p[2] -= v[2] * dt; v[0] = v[2] = 0;
        } else { p[1] = gy; if (v[1] < 0) v[1] = -v[1] * b.bounce; if (Math.abs(v[1]) < 0.5) v[1] = 0; b.onGround = true; b.groundY = gy; }
      } else if (p[1] - gy < 0.08 && v[1] <= 0) { p[1] = gy; v[1] = 0; b.onGround = true; b.groundY = gy; }
    }
    // static boxes: cylinder (radius) vs AABB
    const r = b.radius;
    for (const bx of this.boxes) {
      if (p[1] + b.height < bx.min[1] || p[1] > bx.max[1]) continue;
      const cx = clamp(p[0], bx.min[0], bx.max[0]), cz = clamp(p[2], bx.min[2], bx.max[2]);
      let dx = p[0] - cx, dz = p[2] - cz, d2 = dx * dx + dz * dz;
      if (d2 >= r * r) continue;
      const top = bx.max[1];
      // step up / stand on top
      if (p[1] >= top - b.step && p[1] <= top + 0.3 && v[1] <= 0) { p[1] = top; v[1] = 0; b.onGround = true; b.groundY = top; continue; }
      if (d2 < 1e-8) { // centre inside the box: push out along the shallowest axis
        const ox = Math.min(p[0] - bx.min[0], bx.max[0] - p[0]), oz = Math.min(p[2] - bx.min[2], bx.max[2] - p[2]);
        if (ox < oz) { dx = p[0] - (bx.min[0] + bx.max[0]) / 2 > 0 ? 1 : -1; dz = 0; p[0] += dx * (ox + r); } else { dz = p[2] - (bx.min[2] + bx.max[2]) / 2 > 0 ? 1 : -1; dx = 0; p[2] += dz * (oz + r); }
        v[0] = v[2] = 0; continue;
      }
      const d = Math.sqrt(d2), push = r - d; p[0] += dx / d * push; p[2] += dz / d * push;
      const vn = v[0] * dx / d + v[2] * dz / d; if (vn < 0) { v[0] -= vn * dx / d; v[2] -= vn * dz / d; }
    }
    // body vs body (soft push)
    for (const o of this.bodies) {
      if (o === b) continue; const q = o.node.position, dx = p[0] - q[0], dz = p[2] - q[2], rr = r + o.radius, d2 = dx * dx + dz * dz;
      if (d2 < rr * rr && d2 > 1e-8 && Math.abs(p[1] - q[1]) < b.height) { const d = Math.sqrt(d2), push = (rr - d) * 0.5; p[0] += dx / d * push; p[2] += dz / d * push; }
    }
  }
  // ray vs boxes + heightfield. returns {t, point, normal?, tag} or null
  raycast(o, d, maxT = 100) {
    let best = null;
    for (const bx of this.boxes) {
      let t0 = 0, t1 = maxT, ok = true;
      for (let i = 0; i < 3; i++) {
        if (Math.abs(d[i]) < 1e-9) { if (o[i] < bx.min[i] || o[i] > bx.max[i]) { ok = false; break; } }
        else { let a = (bx.min[i] - o[i]) / d[i], c = (bx.max[i] - o[i]) / d[i]; if (a > c) [a, c] = [c, a]; t0 = Math.max(t0, a); t1 = Math.min(t1, c); if (t0 > t1) { ok = false; break; } }
      }
      if (ok && (!best || t0 < best.t)) best = { t: t0, tag: bx.tag, box: bx };
    }
    if (this.heightFn) {
      const stepLen = 0.5; let prev = 0;
      for (let t = stepLen; t <= maxT; t += stepLen) {
        const x = o[0] + d[0] * t, z = o[2] + d[2] * t, y = o[1] + d[1] * t;
        if (y <= this.height(x, z)) { let lo = prev, hi = t; for (let i = 0; i < 8; i++) { const m = (lo + hi) / 2; if (o[1] + d[1] * m <= this.height(o[0] + d[0] * m, o[2] + d[2] * m)) hi = m; else lo = m; } if (!best || hi < best.t) best = { t: hi, tag: 'terrain' }; break; }
        prev = t;
      }
    }
    if (best) best.point = [o[0] + d[0] * best.t, o[1] + d[1] * best.t, o[2] + d[2] * best.t];
    return best;
  }
}
