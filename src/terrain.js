// Sabak Engine - terrain: heightfield mesh with slope/height vertex colours + matching height function for physics.
import { fbm, clamp, lerp } from './math.js';

export function makeTerrain({ size = 240, res = 160, height = (x, z) => fbm(x * 0.012, z * 0.012, 5) * 18 - 4, color = null } = {}) {
  const n = res + 1, P = new Float32Array(n * n * 3), N = new Float32Array(n * n * 3), U = new Float32Array(n * n * 2), C = new Float32Array(n * n * 4);
  const half = size / 2, e = size / res * 0.5;
  const defColor = (y, slope) => {
    const grass = [0.30, 0.46, 0.16], dry = [0.50, 0.46, 0.22], rock = [0.45, 0.43, 0.40], sand = [0.76, 0.69, 0.48], snow = [0.95, 0.96, 0.98];
    let c = y < 0.2 ? sand : y < 9 ? grass : y < 13 ? dry : snow;
    if (y > 0.2 && y < 9) c = c.map((v, i) => lerp(v, dry[i], clamp((y - 5) / 4, 0, 1) * 0.5));
    const k = clamp((slope - 0.35) / 0.35, 0, 1); return c.map((v, i) => lerp(v, rock[i], k));
  };
  for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) {
    const x = -half + i / res * size, z = -half + j / res * size, y = height(x, z), k = j * n + i;
    P[k * 3] = x; P[k * 3 + 1] = y; P[k * 3 + 2] = z;
    const dx = height(x + e, z) - height(x - e, z), dz = height(x, z + e) - height(x, z - e);
    const nl = Math.hypot(dx, 2 * e, dz); N[k * 3] = -dx / nl; N[k * 3 + 1] = 2 * e / nl; N[k * 3 + 2] = -dz / nl;
    U[k * 2] = i / res * size / 4; U[k * 2 + 1] = j / res * size / 4;
    const slope = 1 - N[k * 3 + 1], v = 0.92 + 0.16 * fbm(x * 0.35, z * 0.35, 2), c = (color || defColor)(y, slope);
    C[k * 4] = c[0] * v; C[k * 4 + 1] = c[1] * v; C[k * 4 + 2] = c[2] * v; C[k * 4 + 3] = 1;
  }
  const I = new Uint32Array(res * res * 6); let t = 0;
  for (let j = 0; j < res; j++) for (let i = 0; i < res; i++) { const a = j * n + i, b = a + 1, c = a + n, d = c + 1; I[t++] = a; I[t++] = c; I[t++] = b; I[t++] = b; I[t++] = c; I[t++] = d; }
  return { data: { positions: P, normals: N, uvs: U, colors: C, indices: I }, height, size };
}
