import { Engine, prim, makeTerrain, mat4, quat, fbm, clamp, hash2, v3 } from '../src/index.js';

const canvas = document.getElementById('game');
const engine = new Engine(canvas, { shadowSize: 2048, shadowRadius: 40 });
engine.showStats = true;
window.engine = engine;
const { scene, physics, input, renderer } = engine;

// ---- world
const terrain = makeTerrain({ size: 260, res: 180, height: (x, z) => {
  const d = Math.hypot(x, z), flat = 1 - clamp((d - 18) / 30, 0, 1);
  const h = fbm(x * 0.011, z * 0.011, 5) * 22 - 5; return h * (1 - flat * 0.9) + 0.4 * flat;
} });
physics.heightFn = terrain.height;
engine.mesh(terrain.data, { rough: 1, useVC: true, receive: true, cast: false }, { name: 'terrain' });

// water
engine.mesh(prim.plane(400, 400), { base: [0.12, 0.35, 0.5, 0.78], alpha: 'BLEND', metal: 0.1, rough: 0.08, cast: false, receive: false }, { name: 'water' }).position = [0, -0.9, 0];

// trees (instanced): trunk + two cones
const trunkM = [], crownM = [], rockM = [];
for (let i = 0; i < 420; i++) {
  const x = (hash2(i, 1) - 0.5) * 240, z = (hash2(i, 2) - 0.5) * 240, y = terrain.height(x, z);
  if (y < 0.3 || Math.hypot(x, z) < 22 || y > 13) continue;
  const s = 0.8 + hash2(i, 3) * 0.9, q = quat.fromAxisAngle([0, 1, 0], hash2(i, 4) * 6.28);
  trunkM.push(mat4.fromTRS([x, y + 1.1 * s, z], q, [s, s, s]));
  crownM.push(mat4.fromTRS([x, y + 3.6 * s, z], q, [s, s, s]));
  if (hash2(i, 5) > 0.82) rockM.push(mat4.fromTRS([x + 2, terrain.height(x + 2, z) + 0.3, z], q, [s * 1.3, s, s * 1.1]));
  physics.addBox(x, y + 1, z, 0.7 * s, 4, 0.7 * s, 'tree');
}
engine.instanced(prim.cylinder(0.22, 0.32, 2.2, 8), { base: [0.36, 0.24, 0.14, 1], rough: 0.95 }, trunkM, 'trunks');
engine.instanced(prim.cone(1.5, 4.2, 10), { base: [0.16, 0.38, 0.15, 1], rough: 0.9 }, crownM, 'crowns');
engine.instanced(prim.sphere(0.9, 10, 7), { base: [0.5, 0.5, 0.52, 1], rough: 0.95 }, rockM, 'rocks');

// a small village on the flat ground
const houses = [[-9, -8], [8, -10], [12, 6], [-12, 8], [0, -16]];
houses.forEach(([x, z], i) => {
  const y = terrain.height(x, z), w = 5 + (i % 2) * 1.5, d = 4.5, h = 3;
  const wall = engine.mesh(prim.box(w, h, d), { base: [0.82, 0.74, 0.58, 1], rough: 0.9 }); wall.position = [x, y + h / 2, z];
  const roof = engine.mesh(prim.cylinder(0.01, w * 0.78, 2.2, 4), { base: [0.55, 0.2, 0.14, 1], rough: 0.85 }); roof.position = [x, y + h + 1.1, z]; roof.rotation = quat.fromEuler(0, Math.PI / 4, 0);
  physics.addBox(x, y + h / 2, z, w, h, d, 'house');
});
// glowing crystals (emissive)
for (let i = 0; i < 5; i++) { const a = i / 5 * 6.28, x = Math.cos(a) * 7, z = Math.sin(a) * 7; const c = engine.mesh(prim.sphere(0.45, 12, 8), { base: [0.3, 0.8, 1, 1], emissive: [0.2, 0.9, 1.2], unlit: false, rough: 0.2, metal: 0.3 }); c.position = [x, terrain.height(x, z) + 1.2, z]; c.scale = [0.7, 1.4, 0.7]; c.userData.spin = i; }

// ---- player
const hero = await engine.spawn('../assets/hero.glb', { name: 'hero' });
const anim = hero.components.animator;
const body = physics.addBody(hero, { radius: 0.4, height: 1.8 });
hero.position = [0, terrain.height(0, 0) + 0.2, 4];
body.speedMax = 5.5;
let yaw = 0, pitch = 0.28, dist = 6.5, face = 0, state = '';
const horseNode = await engine.spawn('../assets/horse.glb', { name: 'horse' });
horseNode.position = [4, terrain.height(4, 2), 2]; horseNode.components.animator.play('idle');
horseNode.rotation = quat.fromEuler(0, -0.6, 0);

document.getElementById('boot').style.opacity = 0; setTimeout(() => document.getElementById('boot').remove(), 500);

engine.onUpdate = (dt) => {
  // camera look
  yaw -= input.look[0] * 0.006; pitch = clamp(pitch + input.look[1] * 0.005, -0.2, 1.2); dist = clamp(dist + input.zoom * 0.6, 3, 14);
  if (input.down('KeyQ')) yaw += dt * 1.8; if (input.down('KeyE')) yaw -= dt * 1.8;
  // movement relative to camera
  const [ax, ay] = input.axes(), mag = Math.hypot(ax, ay);
  const fwd = [-Math.sin(yaw), -Math.cos(yaw)], right = [Math.cos(yaw), -Math.sin(yaw)];
  const wx = right[0] * ax + fwd[0] * -ay, wz = right[1] * ax + fwd[1] * -ay;
  body.wish[0] = wx; body.wish[1] = wz; body.speedMax = input.down('ShiftLeft') ? 8 : mag > 0.75 ? 5.5 : 2.4;
  if (mag > 0.1) { const target = Math.atan2(wx, wz); let d = target - face; while (d > Math.PI) d -= 6.283; while (d < -Math.PI) d += 6.283; face += d * Math.min(1, dt * 12); }
  hero.rotation = quat.fromEuler(0, face, 0);
  if ((input.hit('Space')) && body.onGround) { body.vel[1] = 8; engine.audio.sfx('jump'); }
  const sp = Math.hypot(body.vel[0], body.vel[2]);
  const want = sp < 0.4 ? 'idle' : sp < 3.6 ? 'walk' : 'run';
  if (want !== state && anim.has(want)) { anim.play(want, { fade: 0.18 }); state = want; }
  // shadows follow the player; camera orbits
  const p = hero.position, target = [p[0], p[1] + 1.4, p[2]];
  const cp = [target[0] + Math.sin(yaw) * Math.cos(pitch) * dist, target[1] + Math.sin(pitch) * dist, target[2] + Math.cos(yaw) * Math.cos(pitch) * dist];
  const gh = terrain.height(cp[0], cp[2]); if (cp[1] < gh + 0.6) cp[1] = gh + 0.6;
  scene.camera.position = cp; scene.camera.target = target; renderer.shadowFocus = [p[0], p[1], p[2]];
  // spin crystals
  scene.nodes.forEach((n) => { if (n.userData.spin !== undefined) { n.rotation = quat.fromEuler(0, engine.time * 0.8 + n.userData.spin, 0); n.position[1] += Math.sin(engine.time * 2 + n.userData.spin) * 0.002; } });
  engine.ui.text = ['SABAK ENGINE - demo', 'move: WASD / left thumb   look: drag   jump: Space'];
};
engine.start();
