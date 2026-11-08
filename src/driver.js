// Sabak Engine - Driver ("simulator" API). Lets a script (or Claude) control and inspect the engine headlessly.
// Exposed as window.sabak. Every method is JSON-friendly so it can be called from Playwright / a WebView bridge.
export class Driver {
  constructor(engine) { this.e = engine; this.paused = false; this.log = []; }
  // ---- time control: pause the real-time loop, then advance exactly N frames of dt seconds (deterministic)
  pause() { this.paused = true; return true; }
  resume() { this.paused = false; return true; }
  step(frames = 1, dt = 1 / 60) { for (let i = 0; i < frames; i++) this.e.tick(dt); return this.state(); }
  // ---- input injection
  press(...codes) { for (const c of codes) { if (!this.e.input.keys.has(c)) this.e.input.pressed.add(c); this.e.input.keys.add(c); } return true; }
  release(...codes) { for (const c of codes) this.e.input.keys.delete(c); return true; }
  releaseAll() { this.e.input.keys.clear(); this.e.input.move[0] = this.e.input.move[1] = 0; return true; }
  tap(code, frames = 2) { this.press(code); this.step(frames); this.release(code); return this.state(); }
  stick(x, y) { this.e.input.move[0] = x; this.e.input.move[1] = y; return true; }
  look(dx, dy) { this.e.input.look[0] += dx; this.e.input.look[1] += dy; return true; }
  // ---- scene access
  nodes() { const out = []; this.e.scene.nodes.forEach((n) => out.push(this._info(n))); return out; }
  find(name) { const n = this.e.scene.root.find(name); return n ? this._info(n) : null; }
  _info(n) { return { name: n.name, pos: n.position.map((v) => +v.toFixed(3)), scale: n.scale.map((v) => +v.toFixed(3)), visible: n.visible, mesh: !!n.mesh, instances: n.instances ? n.instances.count : 0, anim: n.components.animator ? n.components.animator.current || null : null }; }
  teleport(name, x, y, z) { const n = this.e.scene.root.find(name); if (!n) return false; n.position = [x, y, z]; return true; }
  camera(pos, target) { const c = this.e.scene.camera; if (pos) c.position = pos; if (target) c.target = target; return true; }
  // ---- state / metrics
  state() {
    const e = this.e, r = e.renderer.stats;
    return { time: +e.time.toFixed(3), fps: e.fps, draws: r.draws, tris: r.tris, keys: [...e.input.keys], nodes: e.scene.nodes.size, camera: { pos: e.scene.camera.position.map((v) => +v.toFixed(2)), target: e.scene.camera.target.map((v) => +v.toFixed(2)) }, paused: this.paused, log: this.log.slice(-10) };
  }
  // ---- capture: current frame as PNG data URL (renders one fresh frame without advancing time)
  shot() { this.e.tick(0); return this.e.canvas.toDataURL('image/png'); }
  // ---- run a list of commands: [["press","KeyW"],["step",60],["state"],["shot"]]
  run(cmds) {
    const out = [];
    for (const c of cmds) { const [m, ...a] = c; try { out.push({ cmd: m, result: this[m](...a) }); } catch (err) { out.push({ cmd: m, error: String(err) }); } }
    return out;
  }
}
