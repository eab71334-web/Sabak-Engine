# Sabak Engine - hard-surface coupe (G82-style) built in Blender from real blueprint data.
# Run:  blender -b -P tools/blender/car_g82.py -- out_dir
# Blender axes: X right, Y backward (front of the car is -Y so it exports as glTF +Z), Z up.
import bpy, bmesh, math, sys, os
from mathutils import Vector

OUT = sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else '/tmp/car_out'
os.makedirs(OUT, exist_ok=True)

# ---------------- blueprint data (metres, forward = +f, from the G82 drawing) ----------------
TOP = [(2.397,.713),(2.305,.747),(2.212,.793),(2.12,.828),(2.027,.851),(1.935,.885),(1.843,.908),(1.75,.931),(1.658,.943),(1.565,.955),(1.473,.955),(1.381,.966),(1.288,.978),(1.196,.978),(1.103,.989),(1.011,.989),(.919,1.0),(.826,1.058),(.734,1.104),(.641,1.161),(.549,1.208),(.457,1.242),(.364,1.299),(.272,1.334),(.179,1.369),(.087,1.38),(-.005,1.403),(-.098,1.403),(-.19,1.38),(-.283,1.38),(-.375,1.38),(-.467,1.403),(-.56,1.403),(-.652,1.391),(-.745,1.38),(-.837,1.369),(-.929,1.38),(-1.022,1.391),(-1.114,1.311),(-1.207,1.288),(-1.299,1.265),(-1.391,1.23),(-1.484,1.208),(-1.576,1.173),(-1.669,1.15),(-1.761,1.104),(-1.853,1.081),(-1.946,1.069),(-2.038,1.046),(-2.131,1.035),(-2.223,1.035),(-2.315,1.012)]
HW = [(2.40,.50),(2.33,.66),(2.26,.76),(2.12,.87),(1.98,.925),(1.84,.941),(-1.50,.941),(-1.62,.93),(-1.76,.895),(-1.90,.86),(-2.04,.825),(-2.18,.72),(-2.32,.53),(-2.40,.42)]
L_F, L_R = 2.397, -2.397
WB_F, WB_R = 1.48, -1.377           # axle positions (wheelbase 2.857)
WHEEL_R = 0.35
BELT = 0.985

def interp(tab, f):
    tab = sorted(tab)
    if f <= tab[0][0]: return tab[0][1]
    for i in range(len(tab)-1):
        if f <= tab[i+1][0]:
            t = (f-tab[i][0])/(tab[i+1][0]-tab[i][0]); t = t*t*(3-2*t)
            return tab[i][1]*(1-t)+tab[i+1][1]*t
    return tab[-1][1]
top_h = lambda f: interp(TOP, f)
half_w = lambda f: interp(HW, f)

# ---------------- helpers ----------------
def clear():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    for c in (bpy.data.meshes, bpy.data.materials):
        for b in list(c): c.remove(b)

def mat(name, color, metal=0.0, rough=0.5, coat=0.0, alpha=1.0, emit=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    def S(keys, v):
        for k in keys:
            if k in b.inputs:
                b.inputs[k].default_value = v; return
    S(['Base Color'], (color[0], color[1], color[2], 1)); S(['Metallic'], metal); S(['Roughness'], rough)
    S(['Coat Weight', 'Clearcoat'], coat); S(['Coat Roughness', 'Clearcoat Roughness'], 0.03)
    if alpha < 1: S(['Alpha'], alpha); m.blend_method = 'BLEND' if hasattr(m, 'blend_method') else None
    if emit: S(['Emission Color', 'Emission'], (emit[0], emit[1], emit[2], 1)); S(['Emission Strength'], 3.0)
    return m

def rgb(h): return ((h >> 16 & 255)/255.0, (h >> 8 & 255)/255.0, (h & 255)/255.0)

def to_obj(name, bm, mats, smooth=True):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(ob)
    for m in mats: me.materials.append(m)
    if smooth:
        for p in me.polygons: p.use_smooth = True
    return ob

def modifier(ob, kind, **kw):
    m = ob.modifiers.new(kind, kind)
    for k, v in kw.items(): setattr(m, k, v)
    return m

def apply_mods(ob):
    bpy.context.view_layer.objects.active = ob
    for m in list(ob.modifiers):
        try: bpy.ops.object.modifier_apply(modifier=m.name)
        except Exception as e: print('mod fail', m.name, e)

def bool_op(ob, other, op='DIFFERENCE'):
    m = ob.modifiers.new('b', 'BOOLEAN'); m.operation = op; m.object = other; m.solver = 'EXACT'
    apply_mods(ob)

def box(name, c, s, m=None, bevel=0.0):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co = Vector((v.co.x*s[0], v.co.y*s[1], v.co.z*s[2]))
    ob = to_obj(name, bm, [m] if m else [], False); ob.location = c
    if bevel: modifier(ob, 'BEVEL', width=bevel, segments=3); 
    return ob

# ---------------- body lofted from blueprint ----------------
def ring_pts(f):
    """right-half cross-section points (u fraction of half width, z). Closed by mirroring."""
    hw = half_w(f); deck = min(top_h(f), BELT)
    # arch lift is done by boolean, not here
    zb = 0.16 + (0.0 if abs(f) < 2.0 else 0.10*(abs(f)-2.0)/0.4)
    zs = [zb, zb, zb+0.06, 0.40, 0.68, deck-0.12, deck-0.03, deck-0.006, deck-0.004, deck]
    us = [0.0, 0.80, 0.96, 1.0, 1.0, 0.985, 0.92, 0.865 if deck > 0.95 else 0.6, 0.45, 0.0]
    if deck < 0.9:  # low nose / tail: pull shoulder in
        pass
    return [(u*hw, z) for u, z in zip(us, zs)]

def loft_rings(name, stations, ringfn, mats, matfn=None, caps=True):
    bm = bmesh.new(); rows = []
    for f in stations:
        right = ringfn(f); pts = right + [(-x, z) for x, z in reversed(right[1:-1])]
        rows.append([bm.verts.new(Vector((x, -f, z))) for x, z in pts])
    n = len(rows[0])
    for i in range(len(rows)-1):
        for j in range(n):
            j2 = (j+1) % n
            try: fc = bm.faces.new([rows[i][j], rows[i][j2], rows[i+1][j2], rows[i+1][j]])
            except ValueError: continue
            if matfn: fc.material_index = matfn((stations[i]+stations[i+1])/2, j, n)
    if caps:
        for r in (rows[0], rows[-1]):
            try: bm.faces.new(r if r is rows[-1] else list(reversed(r)))
            except ValueError: pass
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return to_obj(name, bm, mats)

clear()
bpy.context.scene.unit_settings.system = 'METRIC'
M_PAINT = mat('Paint', rgb(0x2a4a8c), metal=0.35, rough=0.22, coat=1.0)
M_BLACK = mat('BlackTrim', rgb(0x0a0a0c), rough=0.45)
M_GLASS = mat('Glass', rgb(0x0d1418), rough=0.03, alpha=0.35)
M_CHROME = mat('Chrome', rgb(0xdadde3), metal=1.0, rough=0.12)
M_TIRE = mat('Rubber', rgb(0x0c0c0d), rough=0.85)
M_RIM = mat('Rim', rgb(0x5a5f69), metal=1.0, rough=0.25)
M_RED = mat('Caliper', rgb(0xc4161c), rough=0.4)
M_DISC = mat('Disc', rgb(0x8a8d93), metal=1.0, rough=0.35)
M_LEATHER = mat('Leather', rgb(0x3a261c), rough=0.55)
M_INT = mat('InteriorDark', rgb(0x16171a), rough=0.6)
M_HEAD = mat('HeadLight', rgb(0xeaf6ff), rough=0.1, emit=rgb(0xcfe6ff))
M_TAIL = mat('TailLight', rgb(0xc01018), rough=0.2, emit=rgb(0xff2020))

st_body = [L_F, 2.36, 2.30, 2.20, 2.05, 1.85, 1.65, 1.45, 1.25, 1.05, .85, .6, .3, 0, -.3, -.6, -.9, -1.2, -1.5, -1.8, -2.0, -2.15, -2.28, L_R]
body = loft_rings('Body', st_body, ring_pts, [M_PAINT])
# crease the shoulder + sill lines for subsurf
bm = bmesh.new(); bm.from_mesh(body.data)
cl = bm.edges.layers.float.get('crease_edge') or bm.edges.layers.float.new('crease_edge')
n = 16
for e in bm.edges:
    a, b = e.verts; za, zb_ = a.co.z, b.co.z
    if abs(a.co.y-b.co.y) > 1e-4 and abs(za-zb_) < 1e-4 and 0.15 < za:   # longitudinal edges
        e[cl] = 0.9
bm.to_mesh(body.data); bm.free()
modifier(body, 'SUBSURF', levels=2, render_levels=2)
apply_mods(body)

# greenhouse (cabin) lofted separately, tumblehome
def cab_ring(f):
    hw = half_w(f); roof = top_h(f)
    base = BELT-0.01
    if roof <= base+0.02: roof = base+0.02
    return [(0.0, base), (0.86*hw, base), (0.80*hw, base+0.10*(roof-base)/0.4+0.12), (0.66*hw, roof-0.07), (0.40*hw, roof-0.015), (0.0, roof)]
st_cab = [1.0, .9, .75, .55, .35, .1, -.15, -.4, -.65, -.9, -1.1, -1.3, -1.5, -1.65, -1.78]
def cab_mat(f, j, n):
    # right side: ring indices 0..5 -> segments j=0 (floor), 1 (lower side), 2 (upper side glass), 3 (roof shoulder), 4 (roof)
    jj = min(j, n-1-j) if j >= n//2 else j
    seg = j if j < 5 else (n-1-j)
    glass_side = seg in (1, 2)
    winf = -1.05 < f < 0.62
    pillarB = -0.20 < f < 0.02
    if glass_side and winf and not pillarB: return 1
    if seg in (3, 4) and (f > 0.45 or f < -1.12): return 1   # windshield + rear window
    return 0
cab = loft_rings('Cabin', st_cab, cab_ring, [M_PAINT, M_GLASS], cab_mat, caps=True)
modifier(cab, 'SUBSURF', levels=1, render_levels=1); apply_mods(cab)

# ---------------- wheel arches (boolean) ----------------
def arch_cutter(f):
    bpy.ops.mesh.primitive_cylinder_add(radius=WHEEL_R+0.085, depth=2.4, vertices=64, rotation=(0, math.radians(90), 0), location=(0, -f, WHEEL_R))
    return bpy.context.active_object
for f in (WB_F, WB_R):
    cu = arch_cutter(f); bool_op(body, cu); bpy.data.objects.remove(cu)

# ---------------- doors: split off a door panel from body + cabin ----------------
def door_cutter(side, grow=0.0):
    # door spans from front hinge f=+0.72 to rear edge f=-0.46, sill z>0.22
    return box('cut', (side*0.55, -(0.13), 0.80), (1.0+grow, 1.18+grow*2, 1.30+grow))
doors = {}
for side, nm in ((1, 'door.R'), (-1, 'door.L')):
    parts = []
    for src in (body, cab):
        cp = src.copy(); cp.data = src.data.copy(); bpy.context.collection.objects.link(cp)
        cu = door_cutter(side); bool_op(cp, cu, 'INTERSECT'); bpy.data.objects.remove(cu); parts.append(cp)
        cu2 = door_cutter(side, 0.012); bool_op(src, cu2, 'DIFFERENCE'); bpy.data.objects.remove(cu2)
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts: p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]; bpy.ops.object.join()
    d = bpy.context.active_object; d.name = nm; doors[nm] = d
    # hinge origin at the front A-pillar edge
    bpy.context.scene.cursor.location = (side*0.93, -0.72, 0.8); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')

# ---------------- wheels ----------------
def make_wheel(name, f, side):
    x = side*0.905
    parts = []
    bpy.ops.mesh.primitive_cylinder_add(radius=0.355, depth=0.255, vertices=64, rotation=(0, math.radians(90), 0), location=(x, -f, WHEEL_R)); t = bpy.context.active_object
    modifier(t, 'BEVEL', width=0.05, segments=4); t.data.materials.append(M_TIRE); parts.append(t)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.24, depth=0.07, vertices=64, rotation=(0, math.radians(90), 0), location=(x+side*0.118, -f, WHEEL_R)); r = bpy.context.active_object
    modifier(r, 'BEVEL', width=0.012, segments=2); r.data.materials.append(M_RIM); parts.append(r)
    for k in range(10):
        a = k*math.tau/10
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x+side*0.152, -f, WHEEL_R)); s = bpy.context.active_object
        s.scale = (0.014, 0.04, 0.215); s.rotation_euler = (a, 0, 0); s.data.materials.append(M_RIM); parts.append(s)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.205, depth=0.03, vertices=48, rotation=(0, math.radians(90), 0), location=(x-side*0.07, -f, WHEEL_R)); dsc = bpy.context.active_object
    dsc.data.materials.append(M_DISC); parts.append(dsc)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x-side*0.045, -f-0.0, WHEEL_R+0.16)); cal = bpy.context.active_object
    cal.scale = (0.035, 0.07, 0.07); cal.data.materials.append(M_RED); parts.append(cal)
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts: p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]; bpy.ops.object.join()
    w = bpy.context.active_object; w.name = name
    bpy.context.scene.cursor.location = (x, -f, WHEEL_R); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    for p in w.data.polygons: p.use_smooth = True
    return w
wheels = [make_wheel('wheel.FR', WB_F, 1), make_wheel('wheel.FL', WB_F, -1), make_wheel('wheel.RR', WB_R, 1), make_wheel('wheel.RL', WB_R, -1)]

# ---------------- grille, lights, plates, interior ----------------
dets = []
for side in (-1, 1):   # twin kidney grille (rounded boxes)
    g = box('kidney', (side*0.115, -2.36, 0.50), (0.19, 0.03, 0.21), M_BLACK, bevel=0.07); dets.append(g)
    fr = box('kidney_frame', (side*0.115, -2.37, 0.50), (0.205, 0.012, 0.225), M_CHROME, bevel=0.07); dets.append(fr)
    hl = box('headlight', (side*0.50, -2.22, 0.745), (0.30, 0.10, 0.07), M_HEAD, bevel=0.03); dets.append(hl)
    tl = box('taillight', (side*0.52, 2.25, 0.90), (0.36, 0.06, 0.08), M_TAIL, bevel=0.02); dets.append(tl)
    for q in range(2):
        ex = box('exhaust', (side*(0.28+q*0.17), 2.33, 0.34), (0.09, 0.09, 0.09), M_CHROME, bevel=0.04); dets.append(ex)
dets.append(box('splitter', (0, -2.30, 0.19), (1.25, 0.16, 0.025), M_BLACK, bevel=0.01))
dets.append(box('diffuser', (0, 2.30, 0.25), (1.30, 0.2, 0.08), M_BLACK, bevel=0.01))
dets.append(box('plate', (0, 2.36, 0.60), (0.52, 0.01, 0.12), M_CHROME))
for side in (-1, 1):
    dets.append(box('mirror', (side*1.0, -0.62, 1.04), (0.16, 0.10, 0.08), M_BLACK, bevel=0.03))
# seats
for side in (-1, 1):
    dets.append(box('seat', (side*0.36, 0.12, 0.52), (0.5, 0.55, 0.14), M_LEATHER, bevel=0.05))
    s2 = box('seatback', (side*0.36, 0.43, 0.82), (0.5, 0.14, 0.55), M_LEATHER, bevel=0.05); s2.rotation_euler = (math.radians(-12), 0, 0); dets.append(s2)
dets.append(box('rearbench', (0, 0.95, 0.52), (1.1, 0.5, 0.14), M_LEATHER, bevel=0.05))
dets.append(box('dash', (0, -0.78, 0.86), (1.55, 0.35, 0.16), M_INT, bevel=0.05))
dets.append(box('floor', (0, 0, 0.18), (1.7, 3.2, 0.05), M_INT))

# ---------------- hierarchy + export ----------------
root = bpy.data.objects.new('car', None); bpy.context.collection.objects.link(root)
body.name = 'body'; cab.name = 'cabin'
for ob in [body, cab, *doors.values(), *wheels, *dets]:
    ob.parent = root
bpy.ops.object.select_all(action='SELECT')
for ob in bpy.context.scene.objects:
    if ob.type == 'MESH':
        bpy.context.view_layer.objects.active = ob
        if ob.name.startswith(('body', 'cabin', 'door', 'wheel')):
            ob.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, 'car.glb'), export_format='GLB', export_apply=True, export_yup=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'car.blend'))

# ---------------- preview renders ----------------
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.samples = 24; sc.cycles.device = 'CPU'
sc.render.resolution_x, sc.render.resolution_y = 960, 540
bpy.ops.mesh.primitive_plane_add(size=60, location=(0, 0, 0)); gp = bpy.context.active_object; gp.data.materials.append(mat('Ground', (0.25, 0.25, 0.27), rough=0.9))
wd = bpy.data.worlds.new('w'); sc.world = wd; wd.use_nodes = True
bg = wd.node_tree.nodes['Background']; bg.inputs['Color'].default_value = (0.55, 0.65, 0.8, 1); bg.inputs['Strength'].default_value = 1.2
bpy.ops.object.light_add(type='SUN', location=(5, 5, 8)); sun = bpy.context.active_object; sun.data.energy = 4; sun.rotation_euler = (math.radians(50), 0, math.radians(35))
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); bpy.context.collection.objects.link(cam); sc.camera = cam
for name, loc, tgt in (('side', (0, 8.0, 1.0), (0, 0, 0.7)), ('front34', (-5.2, -5.2, 1.7), (0, 0, 0.6)), ('rear34', (5.0, 5.2, 1.7), (0, 0, 0.6)), ('top', (0.01, 0.0, 9.0), (0, 0, 0.5))):
    cam.location = loc; cam.data.lens = 50 if name != 'top' else 40
    d = Vector(tgt) - Vector(loc); cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, name + '.png'); bpy.ops.render.render(write_still=True)
print('DONE')
