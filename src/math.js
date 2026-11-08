// Sabak Engine - math: vec3 (arrays), quat [x,y,z,w], mat4 (column-major Float32Array)
export const DEG = Math.PI / 180;
export const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
export const lerp = (a, b, t) => a + (b - a) * t;
export const smooth = (t) => t * t * (3 - 2 * t);

export const v3 = {
  add: (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]],
  sub: (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]],
  mul: (a, s) => [a[0] * s, a[1] * s, a[2] * s],
  dot: (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2],
  cross: (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]],
  len: (a) => Math.hypot(a[0], a[1], a[2]),
  norm: (a) => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; },
  lerp: (a, b, t) => [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t],
  dist: (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]),
};

export const quat = {
  identity: () => [0, 0, 0, 1],
  fromEuler(x, y, z) { // radians, YXZ order (yaw, pitch, roll)
    const cx = Math.cos(x / 2), sx = Math.sin(x / 2), cy = Math.cos(y / 2), sy = Math.sin(y / 2), cz = Math.cos(z / 2), sz = Math.sin(z / 2);
    return [sx * cy * cz + cx * sy * sz, cx * sy * cz - sx * cy * sz, cx * cy * sz - sx * sy * cz, cx * cy * cz + sx * sy * sz];
  },
  fromAxisAngle(axis, a) { const s = Math.sin(a / 2); return [axis[0] * s, axis[1] * s, axis[2] * s, Math.cos(a / 2)]; },
  mul(a, b) {
    return [
      a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
      a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
      a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
      a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]];
  },
  normalize(q) { const l = Math.hypot(q[0], q[1], q[2], q[3]) || 1; return [q[0] / l, q[1] / l, q[2] / l, q[3] / l]; },
  slerp(a, b, t) {
    let cos = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    let bb = b;
    if (cos < 0) { cos = -cos; bb = [-b[0], -b[1], -b[2], -b[3]]; }
    let s0, s1;
    if (cos > 0.9995) { s0 = 1 - t; s1 = t; }
    else { const th = Math.acos(cos), sn = Math.sin(th); s0 = Math.sin((1 - t) * th) / sn; s1 = Math.sin(t * th) / sn; }
    return quat.normalize([a[0] * s0 + bb[0] * s1, a[1] * s0 + bb[1] * s1, a[2] * s0 + bb[2] * s1, a[3] * s0 + bb[3] * s1]);
  },
  rotate(q, v) { // rotate vec3 by quat
    const [x, y, z, w] = q, [vx, vy, vz] = v;
    const tx = 2 * (y * vz - z * vy), ty = 2 * (z * vx - x * vz), tz = 2 * (x * vy - y * vx);
    return [vx + w * tx + (y * tz - z * ty), vy + w * ty + (z * tx - x * tz), vz + w * tz + (x * ty - y * tx)];
  },
};

export const mat4 = {
  identity: () => new Float32Array([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]),
  mul(a, b, out = new Float32Array(16)) {
    for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) {
      out[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
    }
    return out;
  },
  fromTRS(t, q, s, out = new Float32Array(16)) {
    const [x, y, z, w] = q, x2 = x + x, y2 = y + y, z2 = z + z;
    const xx = x * x2, xy = x * y2, xz = x * z2, yy = y * y2, yz = y * z2, zz = z * z2, wx = w * x2, wy = w * y2, wz = w * z2;
    out[0] = (1 - (yy + zz)) * s[0]; out[1] = (xy + wz) * s[0]; out[2] = (xz - wy) * s[0]; out[3] = 0;
    out[4] = (xy - wz) * s[1]; out[5] = (1 - (xx + zz)) * s[1]; out[6] = (yz + wx) * s[1]; out[7] = 0;
    out[8] = (xz + wy) * s[2]; out[9] = (yz - wx) * s[2]; out[10] = (1 - (xx + yy)) * s[2]; out[11] = 0;
    out[12] = t[0]; out[13] = t[1]; out[14] = t[2]; out[15] = 1;
    return out;
  },
  perspective(fovy, aspect, near, far, out = new Float32Array(16)) {
    const f = 1 / Math.tan(fovy / 2), nf = 1 / (near - far);
    out.fill(0);
    out[0] = f / aspect; out[5] = f; out[10] = (far + near) * nf; out[11] = -1; out[14] = 2 * far * near * nf;
    return out;
  },
  ortho(l, r, b, t, n, f, out = new Float32Array(16)) {
    out.fill(0);
    out[0] = 2 / (r - l); out[5] = 2 / (t - b); out[10] = -2 / (f - n);
    out[12] = -(r + l) / (r - l); out[13] = -(t + b) / (t - b); out[14] = -(f + n) / (f - n); out[15] = 1;
    return out;
  },
  lookAt(eye, target, up = [0, 1, 0], out = new Float32Array(16)) {
    const z = v3.norm(v3.sub(eye, target));
    let x = v3.cross(up, z);
    if (v3.len(x) < 1e-6) x = [1, 0, 0];
    x = v3.norm(x);
    const y = v3.cross(z, x);
    out[0] = x[0]; out[1] = y[0]; out[2] = z[0]; out[3] = 0;
    out[4] = x[1]; out[5] = y[1]; out[6] = z[1]; out[7] = 0;
    out[8] = x[2]; out[9] = y[2]; out[10] = z[2]; out[11] = 0;
    out[12] = -v3.dot(x, eye); out[13] = -v3.dot(y, eye); out[14] = -v3.dot(z, eye); out[15] = 1;
    return out;
  },
  invert(m, out = new Float32Array(16)) {
    const a00 = m[0], a01 = m[1], a02 = m[2], a03 = m[3], a10 = m[4], a11 = m[5], a12 = m[6], a13 = m[7], a20 = m[8], a21 = m[9], a22 = m[10], a23 = m[11], a30 = m[12], a31 = m[13], a32 = m[14], a33 = m[15];
    const b00 = a00 * a11 - a01 * a10, b01 = a00 * a12 - a02 * a10, b02 = a00 * a13 - a03 * a10, b03 = a01 * a12 - a02 * a11, b04 = a01 * a13 - a03 * a11, b05 = a02 * a13 - a03 * a12;
    const b06 = a20 * a31 - a21 * a30, b07 = a20 * a32 - a22 * a30, b08 = a20 * a33 - a23 * a30, b09 = a21 * a32 - a22 * a31, b10 = a21 * a33 - a23 * a31, b11 = a22 * a33 - a23 * a32;
    let det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    if (!det) return mat4.identity();
    det = 1 / det;
    out[0] = (a11 * b11 - a12 * b10 + a13 * b09) * det; out[1] = (a02 * b10 - a01 * b11 - a03 * b09) * det; out[2] = (a31 * b05 - a32 * b04 + a33 * b03) * det; out[3] = (a22 * b04 - a21 * b05 - a23 * b03) * det;
    out[4] = (a12 * b08 - a10 * b11 - a13 * b07) * det; out[5] = (a00 * b11 - a02 * b08 + a03 * b07) * det; out[6] = (a32 * b02 - a30 * b05 - a33 * b01) * det; out[7] = (a20 * b05 - a22 * b02 + a23 * b01) * det;
    out[8] = (a10 * b10 - a11 * b08 + a13 * b06) * det; out[9] = (a01 * b08 - a00 * b10 - a03 * b06) * det; out[10] = (a30 * b04 - a31 * b02 + a33 * b00) * det; out[11] = (a21 * b02 - a20 * b04 - a23 * b00) * det;
    out[12] = (a11 * b07 - a10 * b09 - a12 * b06) * det; out[13] = (a00 * b09 - a01 * b07 + a02 * b06) * det; out[14] = (a31 * b01 - a30 * b03 - a32 * b00) * det; out[15] = (a20 * b03 - a21 * b01 + a22 * b00) * det;
    return out;
  },
  transformPoint(m, p) {
    return [m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13], m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]];
  },
};

// small deterministic noise for terrain etc.
export function hash2(x, y) { let h = Math.imul(x | 0, 374761393) + Math.imul(y | 0, 668265263); h = Math.imul(h ^ (h >>> 13), 1274126177); return ((h ^ (h >>> 16)) >>> 0) / 4294967296; }
export function noise2(x, y) {
  const xi = Math.floor(x), yi = Math.floor(y), xf = x - xi, yf = y - yi;
  const u = smooth(xf), v = smooth(yf);
  return lerp(lerp(hash2(xi, yi), hash2(xi + 1, yi), u), lerp(hash2(xi, yi + 1), hash2(xi + 1, yi + 1), u), v);
}
export function fbm(x, y, oct = 4) { let a = 0.5, s = 0, f = 1; for (let i = 0; i < oct; i++) { s += a * noise2(x * f, y * f); f *= 2; a *= 0.5; } return s; }
