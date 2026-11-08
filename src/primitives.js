// Sabak Engine - procedural meshes. Each returns { positions, normals, uvs, colors, indices }
function build(pos, nrm, uv, idx, color = [1, 1, 1, 1]) {
  const n = pos.length / 3, col = new Float32Array(n * 4);
  for (let i = 0; i < n; i++) col.set(color, i * 4);
  return { positions: new Float32Array(pos), normals: new Float32Array(nrm), uvs: new Float32Array(uv), colors: col, indices: new Uint32Array(idx) };
}
export function box(w = 1, h = 1, d = 1, color) {
  const x = w / 2, y = h / 2, z = d / 2, P = [], N = [], U = [], I = [];
  const faces = [
    [[1, 0, 0], [[x, -y, z], [x, -y, -z], [x, y, -z], [x, y, z]]], [[-1, 0, 0], [[-x, -y, -z], [-x, -y, z], [-x, y, z], [-x, y, -z]]],
    [[0, 1, 0], [[-x, y, z], [x, y, z], [x, y, -z], [-x, y, -z]]], [[0, -1, 0], [[-x, -y, -z], [x, -y, -z], [x, -y, z], [-x, -y, z]]],
    [[0, 0, 1], [[-x, -y, z], [x, -y, z], [x, y, z], [-x, y, z]]], [[0, 0, -1], [[x, -y, -z], [-x, -y, -z], [-x, y, -z], [x, y, -z]]]];
  faces.forEach(([n, vs], f) => { vs.forEach((v, i) => { P.push(...v); N.push(...n); U.push(i === 1 || i === 2 ? 1 : 0, i >= 2 ? 1 : 0); }); const b = f * 4; I.push(b, b + 1, b + 2, b, b + 2, b + 3); });
  return build(P, N, U, I, color);
}
export function plane(w = 10, d = 10, color) {
  const x = w / 2, z = d / 2;
  return build([-x, 0, z, x, 0, z, x, 0, -z, -x, 0, -z], [0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0], [0, 0, w, 0, w, d, 0, d], [0, 1, 2, 0, 2, 3], color);
}
export function sphere(r = 0.5, seg = 24, rings = 16, color) {
  const P = [], N = [], U = [], I = [];
  for (let y = 0; y <= rings; y++) for (let x = 0; x <= seg; x++) {
    const v = y / rings, u = x / seg, th = v * Math.PI, ph = u * Math.PI * 2;
    const nx = Math.sin(th) * Math.cos(ph), ny = Math.cos(th), nz = Math.sin(th) * Math.sin(ph);
    P.push(nx * r, ny * r, nz * r); N.push(nx, ny, nz); U.push(u, v);
  }
  for (let y = 0; y < rings; y++) for (let x = 0; x < seg; x++) { const a = y * (seg + 1) + x, b = a + seg + 1; I.push(a, b, a + 1, b, b + 1, a + 1); }
  return build(P, N, U, I, color);
}
export function cylinder(rTop = 0.5, rBot = 0.5, h = 1, seg = 16, color) {
  const P = [], N = [], U = [], I = [], slope = (rBot - rTop) / h;
  for (let i = 0; i <= seg; i++) {
    const a = i / seg * Math.PI * 2, c = Math.cos(a), s = Math.sin(a), l = Math.hypot(1, slope);
    P.push(c * rTop, h / 2, s * rTop, c * rBot, -h / 2, s * rBot); N.push(c / l, slope / l, s / l, c / l, slope / l, s / l); U.push(i / seg, 0, i / seg, 1);
  }
  for (let i = 0; i < seg; i++) { const a = i * 2; I.push(a, a + 1, a + 2, a + 1, a + 3, a + 2); }
  const cap = (y, r, ny) => {
    const b = P.length / 3; P.push(0, y, 0); N.push(0, ny, 0); U.push(0.5, 0.5);
    for (let i = 0; i <= seg; i++) { const a = i / seg * Math.PI * 2; P.push(Math.cos(a) * r, y, Math.sin(a) * r); N.push(0, ny, 0); U.push(0.5 + Math.cos(a) * 0.5, 0.5 + Math.sin(a) * 0.5); }
    for (let i = 0; i < seg; i++) ny > 0 ? I.push(b, b + i + 2, b + i + 1) : I.push(b, b + i + 1, b + i + 2);
  };
  if (rTop > 0) cap(h / 2, rTop, 1); if (rBot > 0) cap(-h / 2, rBot, -1);
  return build(P, N, U, I, color);
}
export const cone = (r = 0.5, h = 1, seg = 16, color) => cylinder(0.0001, r, h, seg, color);
