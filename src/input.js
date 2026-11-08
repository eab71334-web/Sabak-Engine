// Sabak Engine - input: keyboard + mouse + touch with a built-in virtual joystick (left) and look-drag (right)
export class Input {
  constructor(canvas) {
    this.canvas = canvas; this.keys = new Set(); this.pressed = new Set();
    this.move = [0, 0]; this.look = [0, 0]; this.buttons = {}; this.touchUI = false;
    this._stick = null; this._lookId = null; this._last = null; this.joystick = { active: false, cx: 0, cy: 0, x: 0, y: 0, r: 60 };
    this.pinch = 0; this._touches = new Map(); this.zoom = 0;
    addEventListener('keydown', (e) => { if (!this.keys.has(e.code)) this.pressed.add(e.code); this.keys.add(e.code); });
    addEventListener('keyup', (e) => this.keys.delete(e.code));
    canvas.addEventListener('contextmenu', (e) => e.preventDefault());
    canvas.addEventListener('wheel', (e) => { this.zoom += Math.sign(e.deltaY); e.preventDefault(); }, { passive: false });
    canvas.addEventListener('pointerdown', (e) => this._down(e));
    canvas.addEventListener('pointermove', (e) => this._move(e));
    const up = (e) => this._up(e); canvas.addEventListener('pointerup', up); canvas.addEventListener('pointercancel', up);
    canvas.style.touchAction = 'none';
  }
  _down(e) {
    this.canvas.setPointerCapture?.(e.pointerId);
    this._touches.set(e.pointerId, { x: e.clientX, y: e.clientY });
    if (e.pointerType === 'touch') {
      this.touchUI = true;
      const r = this.canvas.getBoundingClientRect(), left = e.clientX - r.left < r.width * 0.45;
      if (left && this._stick == null) { this._stick = e.pointerId; Object.assign(this.joystick, { active: true, cx: e.clientX - r.left, cy: e.clientY - r.top, x: 0, y: 0 }); }
      else if (!left && this._lookId == null) { this._lookId = e.pointerId; this._last = [e.clientX, e.clientY]; }
    } else { this._lookId = e.pointerId; this._last = [e.clientX, e.clientY]; }
  }
  _move(e) {
    const t = this._touches.get(e.pointerId); if (t) { t.x = e.clientX; t.y = e.clientY; }
    if (e.pointerId === this._stick) {
      const r = this.canvas.getBoundingClientRect(), j = this.joystick; let dx = e.clientX - r.left - j.cx, dy = e.clientY - r.top - j.cy; const l = Math.hypot(dx, dy);
      if (l > j.r) { dx = dx / l * j.r; dy = dy / l * j.r; }
      j.x = dx / j.r; j.y = dy / j.r; this.move[0] = j.x; this.move[1] = j.y;
    } else if (e.pointerId === this._lookId && this._last) {
      if (e.pointerType !== 'touch' && e.buttons === 0) return;
      this.look[0] += e.clientX - this._last[0]; this.look[1] += e.clientY - this._last[1]; this._last = [e.clientX, e.clientY];
    }
  }
  _up(e) {
    this._touches.delete(e.pointerId);
    if (e.pointerId === this._stick) { this._stick = null; this.joystick.active = false; this.move[0] = this.move[1] = 0; }
    if (e.pointerId === this._lookId) { this._lookId = null; this._last = null; }
  }
  down(code) { return this.keys.has(code); }
  hit(code) { return this.pressed.has(code); }
  // combined movement axes (keyboard + joystick): x right, y forward(-1 up on screen)
  axes() {
    let x = this.move[0], y = this.move[1];
    if (this.down('KeyA') || this.down('ArrowLeft')) x -= 1; if (this.down('KeyD') || this.down('ArrowRight')) x += 1;
    if (this.down('KeyW') || this.down('ArrowUp')) y -= 1; if (this.down('KeyS') || this.down('ArrowDown')) y += 1;
    const l = Math.hypot(x, y); if (l > 1) { x /= l; y /= l; } return [x, y];
  }
  endFrame() { this.pressed.clear(); this.look[0] = this.look[1] = 0; this.zoom = 0; }
}
