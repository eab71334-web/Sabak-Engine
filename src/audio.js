// Sabak Engine - audio: WebAudio. Load files, play (optionally positional), and procedural sfx (no files needed).
export class Audio {
  constructor() { this.ctx = null; this.master = null; this.buffers = new Map(); this.volume = 0.8; this.listener = [0, 0, 0]; }
  unlock() {
    if (this.ctx) { this.ctx.resume?.(); return; }
    const AC = window.AudioContext || window.webkitAudioContext; if (!AC) return;
    this.ctx = new AC(); this.master = this.ctx.createGain(); this.master.gain.value = this.volume; this.master.connect(this.ctx.destination);
  }
  async load(name, url) { this.unlock(); const r = await fetch(url); const b = await this.ctx.decodeAudioData(await r.arrayBuffer()); this.buffers.set(name, b); return b; }
  play(name, { volume = 1, rate = 1, loop = false, pos = null, maxDist = 40 } = {}) {
    if (!this.ctx || !this.buffers.has(name)) return null;
    const src = this.ctx.createBufferSource(); src.buffer = this.buffers.get(name); src.loop = loop; src.playbackRate.value = rate;
    const g = this.ctx.createGain(); g.gain.value = volume; let node = g;
    if (pos) { const d = Math.hypot(pos[0] - this.listener[0], pos[1] - this.listener[1], pos[2] - this.listener[2]); g.gain.value = volume * Math.max(0, 1 - d / maxDist) ** 1.5; }
    src.connect(g); node.connect(this.master); src.start(); return src;
  }
  // procedural effects
  tone(freq = 440, dur = 0.15, { type = 'square', volume = 0.2, slide = 0 } = {}) {
    if (!this.ctx) return; const t = this.ctx.currentTime, o = this.ctx.createOscillator(), g = this.ctx.createGain();
    o.type = type; o.frequency.setValueAtTime(freq, t); if (slide) o.frequency.exponentialRampToValueAtTime(Math.max(20, freq + slide), t + dur);
    g.gain.setValueAtTime(volume, t); g.gain.exponentialRampToValueAtTime(0.001, t + dur); o.connect(g); g.connect(this.master); o.start(t); o.stop(t + dur);
  }
  noise(dur = 0.2, { volume = 0.25, lowpass = 3000 } = {}) {
    if (!this.ctx) return; const n = Math.floor(this.ctx.sampleRate * dur), b = this.ctx.createBuffer(1, n, this.ctx.sampleRate), d = b.getChannelData(0);
    for (let i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / n);
    const s = this.ctx.createBufferSource(), f = this.ctx.createBiquadFilter(), g = this.ctx.createGain(); f.type = 'lowpass'; f.frequency.value = lowpass; g.gain.value = volume;
    s.buffer = b; s.connect(f); f.connect(g); g.connect(this.master); s.start();
  }
  sfx(name) { ({ hit: () => { this.noise(0.12, { volume: 0.3, lowpass: 1800 }); this.tone(140, 0.12, { type: 'sine', slide: -80, volume: 0.3 }); }, coin: () => { this.tone(880, 0.08, { type: 'square', volume: 0.12 }); setTimeout(() => this.tone(1320, 0.14, { type: 'square', volume: 0.12 }), 70); }, jump: () => this.tone(300, 0.18, { type: 'sine', slide: 380, volume: 0.2 }), step: () => this.noise(0.05, { volume: 0.08, lowpass: 900 }), boom: () => { this.noise(0.6, { volume: 0.5, lowpass: 600 }); this.tone(70, 0.5, { type: 'sine', slide: -40, volume: 0.4 }); } })[name]?.(); }
}
