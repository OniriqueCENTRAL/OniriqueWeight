# Onirique Weight - procedural Blender model of the front (Blender 4.x), after the EQP-1A
# Layout measured from reference photos (3U panel, 19").
# Builds everything into the collection "Pultec EQP-1A" of the current scene.
# Re-running replaces that collection. Units: metres, real-world size.
import bpy, bmesh, math, os
from mathutils import Vector, Matrix

MM = 0.001
COLL_NAME = "Onirique Weight (EQP-1A)"
PANEL_W, PANEL_H, PANEL_T = 482.6, 133.35, 4.8   # 19" x 5.25" (3U)
HW, HH = PANEL_W / 2, PANEL_H / 2
FONT_DIR = "/System/Library/Fonts/Supplemental/"

# ----------------------------------------------------------------- cleanup
def purge_collection(name):
    c = bpy.data.collections.get(name)
    if not c:
        return
    for ch in list(c.children_recursive):
        for o in list(ch.objects):
            bpy.data.objects.remove(o, do_unlink=True)
        bpy.data.collections.remove(ch)
    for o in list(c.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.collections.remove(c)
    for blocks in (bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.lights, bpy.data.cameras):
        for b in list(blocks):
            if b.users == 0:
                blocks.remove(b)

purge_collection("Pultec EQP-1A")
purge_collection(COLL_NAME)
scene = bpy.context.scene
root = bpy.data.collections.new(COLL_NAME)
scene.collection.children.link(root)
def sub(name):
    c = bpy.data.collections.new(name)
    root.children.link(c)
    return c
C_PANEL, C_KNOBS, C_PRINT, C_CHASSIS, C_STUDIO = (sub(n) for n in
    ("Front Panel", "Controls", "Silkscreen", "Chassis", "Studio"))
scene.unit_settings.system = 'METRIC'
scene.unit_settings.length_unit = 'MILLIMETERS'

# --------------------------------------------------------------- materials
def mat(name, color, metallic=0.0, rough=0.5, **extra):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    for k, v in extra.items():
        b.inputs[k.replace("_", " ")].default_value = v
    m.diffuse_color = (*color, 1)
    return m

def add_bump(m, scale, strength, dist=0.0002, kind="voronoi", color_var=None):
    nt = m.node_tree; N = nt.nodes; L = nt.links
    b = N["Principled BSDF"]
    tc = N.new("ShaderNodeTexCoord")
    if kind == "voronoi":
        tex = N.new("ShaderNodeTexVoronoi"); out = tex.outputs["Distance"]
        tex.inputs["Randomness"].default_value = 0.9
    else:
        tex = N.new("ShaderNodeTexNoise"); out = tex.outputs["Fac"]
        tex.inputs["Detail"].default_value = 8
    tex.inputs["Scale"].default_value = scale
    L.new(tc.outputs["Object"], tex.inputs["Vector"])
    bump = N.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = strength
    bump.inputs["Distance"].default_value = dist
    L.new(out, bump.inputs["Height"])
    L.new(bump.outputs["Normal"], b.inputs["Normal"])
    if color_var:
        ramp = N.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].color = (*color_var[0], 1)
        ramp.color_ramp.elements[1].color = (*color_var[1], 1)
        L.new(out, ramp.inputs["Fac"])
        L.new(ramp.outputs["Color"], b.inputs["Base Color"])
    return m

# classic Pultec blue-grey satin enamel (smooth, slight orange-peel)
# Onirique "Weight" DA: matte dark-red paint, white lettering, blue pilot lamp, accent #e23444
M_PANEL   = add_bump(mat("WEIGHT Panel Matte Dark Red", (0.118, 0.0075, 0.012), rough=0.62),
                     900, 0.03, kind="noise")
M_PRINT   = mat("WEIGHT Lettering White", (0.86, 0.85, 0.84), rough=0.45)
M_BAKE    = mat("EQP Black Bakelite", (0.010, 0.010, 0.010), rough=0.22, Coat_Weight=0.5, Coat_Roughness=0.1)
M_POINTER = mat("EQP Pointer White", (0.85, 0.85, 0.82), rough=0.35)
M_CHROME  = mat("EQP Chrome", (0.9, 0.9, 0.9), metallic=1.0, rough=0.08)
M_NICKEL  = mat("EQP Nickel Screws", (0.70, 0.68, 0.62), metallic=1.0, rough=0.25)
M_SLOT    = mat("EQP Screw Slot", (0.03, 0.03, 0.03), metallic=1.0, rough=0.5)
M_JEWEL   = mat("WEIGHT Blue Jewel", (0.15, 0.6, 1.0), rough=0.04, Transmission_Weight=0.8, IOR=1.55,
                Emission_Color=(0.05, 0.45, 1.0, 1), Emission_Strength=2.5)
M_ACCENT  = mat("WEIGHT Accent Red", (0.76, 0.034, 0.057), rough=0.4)
M_HAMMER  = add_bump(mat("EQP Chassis Grey Hammertone", (0.10, 0.10, 0.10), metallic=0.5, rough=0.4),
                     380, 0.35, color_var=((0.06, 0.06, 0.065), (0.13, 0.13, 0.13)))
M_GREY    = mat("EQP Transformer Grey Paint", (0.22, 0.23, 0.23), rough=0.45)
M_LAM     = add_bump(mat("EQP Laminations", (0.10, 0.08, 0.07), metallic=0.7, rough=0.4), 60, 0.3, kind="noise")
M_ALU     = add_bump(mat("EQP Aluminium Can", (0.78, 0.78, 0.78), metallic=1.0, rough=0.22), 220, 0.08, kind="noise")
M_SHIELD  = mat("EQP Tube Shield", (0.85, 0.85, 0.85), metallic=1.0, rough=0.12)
M_BLACKPL = mat("EQP Black Plastic", (0.015, 0.015, 0.015), rough=0.45)
M_FLOOR   = mat("EQP Studio Floor", (0.025, 0.022, 0.024), rough=0.3)

# ------------------------------------------------------------- mesh helpers
ROT_FRONT = Matrix.Rotation(math.radians(90), 4, 'X')    # local +Z -> world -Y (out of front)
ROT_REAR = Matrix.Rotation(math.radians(-90), 4, 'X')    # local +Z -> world +Y (out of rear)

def front(x, z, y=0.0):
    return Matrix.Translation((x * MM, y * MM, z * MM)) @ ROT_FRONT

def faces_of(verts):
    return {f for v in verts for f in v.link_faces}

def setmat(verts, idx):
    for f in faces_of(verts):
        f.material_index = idx

def box(bm, size, center, idx=0, M=None):
    v = bmesh.ops.create_cube(bm, size=1.0)["verts"]
    bmesh.ops.scale(bm, vec=Vector(size) * MM, verts=v)
    bmesh.ops.translate(bm, vec=Vector(center) * MM, verts=v)
    if M is not None:
        bmesh.ops.transform(bm, matrix=M, verts=v)
    setmat(v, idx)
    return v

def lathe(bm, prof, segs=48, idx=0, cap_bot=True, cap_top=True, flute=None, M=None, rmod=None):
    """Surface of revolution around local Z. prof = [(r_mm, z_mm)] bottom->top.
    flute = (lobes, depth, power) gives a scalloped outline."""
    rings = []
    for r, z in prof:
        if r <= 1e-6:
            rings.append([bm.verts.new((0, 0, z * MM))]); continue
        ring = []
        for i in range(segs):
            a = 2 * math.pi * i / segs
            rr = r
            if flute:
                n, d, p = flute
                g = 0.5 + 0.5 * math.cos(n * a)
                rr = r * (1 - d * g ** p)
            if rmod:
                rr = rr * rmod(a)
            ring.append(bm.verts.new((rr * MM * math.cos(a), rr * MM * math.sin(a), z * MM)))
        rings.append(ring)
    fs = []
    for a, b in zip(rings, rings[1:]):
        if len(a) == 1 and len(b) == 1:
            continue
        for i in range(segs):
            j = (i + 1) % segs
            if len(a) == 1:
                fs.append(bm.faces.new((a[0], b[j], b[i])))
            elif len(b) == 1:
                fs.append(bm.faces.new((a[i], a[j], b[0])))
            else:
                fs.append(bm.faces.new((a[i], a[j], b[j], b[i])))
    if cap_bot and len(rings[0]) > 1:
        fs.append(bm.faces.new(list(reversed(rings[0]))))
    if cap_top and len(rings[-1]) > 1:
        fs.append(bm.faces.new(rings[-1]))
    for f in fs:
        f.material_index = idx
    allv = [v for r in rings for v in r]
    if M is not None:
        bmesh.ops.transform(bm, matrix=M, verts=allv)
    return allv

def extrude_outline(bm, pts, z0, z1, idx=0, top_scale=1.0, M=None):
    bot = [bm.verts.new((x * MM, y * MM, z0 * MM)) for x, y in pts]
    top = [bm.verts.new((x * MM * top_scale, y * MM * top_scale, z1 * MM)) for x, y in pts]
    n = len(pts)
    fs = [bm.faces.new(list(reversed(bot))), bm.faces.new(top)]
    for i in range(n):
        j = (i + 1) % n
        fs.append(bm.faces.new((bot[i], bot[j], top[j], top[i])))
    for f in fs:
        f.material_index = idx
    if M is not None:
        bmesh.ops.transform(bm, matrix=M, verts=bot + top)
    return bot + top

def hull(points):
    pts = sorted(set((round(x, 6), round(y, 6)) for x, y in points))
    def cross(o, a, b):
        return (a[0]-o[0])*(b[1]-o[1]) - (a[1]-o[1])*(b[0]-o[0])
    lo, up = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0: lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0: up.pop()
        up.append(p)
    return lo[:-1] + up[:-1]

def circle_pts(cx, cy, r, n=48):
    return [(cx + r * math.cos(2*math.pi*i/n), cy + r * math.sin(2*math.pi*i/n)) for i in range(n)]

def rounded_rect(x0, y0, x1, y1, r, n=10):
    return hull(circle_pts(x0 + r, y0 + r, r, 4*n) + circle_pts(x1 - r, y0 + r, r, 4*n) +
                circle_pts(x0 + r, y1 - r, r, 4*n) + circle_pts(x1 - r, y1 - r, r, 4*n))

def finish(bm, name, mats, coll, loc=(0, 0, 0), bevel=None, sharp_deg=32, rot=None, rot_mode='XYZ'):
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-7)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        f.smooth = True
    lim = math.radians(sharp_deg)
    for e in bm.edges:
        if len(e.link_faces) != 2 or e.calc_face_angle(0) > lim:
            e.smooth = False
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me); bm.free()
    for m in mats:
        me.materials.append(m)
    ob = bpy.data.objects.new(name, me)
    ob.location = Vector(loc) * MM
    if rot is not None:
        ob.rotation_mode = rot_mode
        ob.rotation_euler = rot
    coll.objects.link(ob)
    if bevel:
        bv = ob.modifiers.new("Bevel", 'BEVEL')
        bv.width = bevel * MM; bv.segments = 3; bv.limit_method = 'ANGLE'
        bv.angle_limit = math.radians(40); bv.harden_normals = True
    return ob

# ------------------------------------------------------------- plugin layout
# Positions come from the plugin's Source/ui/Layout.h (front = 1200 x 331 points),
# so the Blender renders land exactly on the editor's controls.
PT_PER_MM = 1200.0 / PANEL_W
def P(x, y):
    """Layout.h point -> panel mm (x right, z up, origin at the panel centre)."""
    return ((x - 600.0) / PT_PER_MM, (331.0 / 2 - y) / PT_PER_MM)

# ------------------------------------------------------------- front panel
SLOT_Z = (P(0, 94)[1], P(0, 240)[1])     # Layout.h kSlotY
bm = bmesh.new()
box(bm, (PANEL_W, PANEL_T, PANEL_H), (0, PANEL_T / 2, 0))
panel = finish(bm, "Front Panel", [M_PANEL], C_PANEL)

# open-ended rack notches cut from the side edges
bm = bmesh.new()
for sx in (-1, 1):
    for sz in SLOT_Z:
        pts = hull(circle_pts(0, 0, 3.0, 32) + [(20, -3.0), (20, 3.0)])     # 28 x 15 pt slot
        if sx < 0:
            pts = [(-x, y) for x, y in reversed(pts)]
            pts = hull(pts)
        extrude_outline(bm, pts, -6, 6,
                        M=Matrix.Translation(((sx * (HW - 28.0 / PT_PER_MM + 3.0)) * MM, PANEL_T / 2 * MM, sz * MM)) @ ROT_FRONT)
cutter = finish(bm, "_slot_cutter", [M_PANEL], C_PANEL)
boo = panel.modifiers.new("Rack Slots", 'BOOLEAN')
boo.operation = 'DIFFERENCE'; boo.solver = 'EXACT'; boo.object = cutter
dg = bpy.context.evaluated_depsgraph_get()
new_me = bpy.data.meshes.new_from_object(panel.evaluated_get(dg))
old = panel.data
panel.modifiers.remove(boo)
panel.data = new_me
bpy.data.meshes.remove(old)
new_me.name = "Front Panel"
bpy.data.objects.remove(cutter, do_unlink=True)
bv = panel.modifiers.new("Bevel", 'BEVEL'); bv.width = 0.5 * MM; bv.segments = 3
bv.limit_method = 'ANGLE'; bv.harden_normals = True

# ------------------------------------------------------------- screws
def screw_head(bm, M, r=4.0, h=2.0, slot_ang=20.0, phillips=False):
    prof = [(r, 0), (r, 0.5)]
    for k in range(1, 7):
        t = k / 6
        prof.append((r * math.cos(t * math.pi / 2) ** 0.7, 0.5 + (h - 0.5) * math.sin(t * math.pi / 2)))
    prof[-1] = (0, prof[-1][1])
    lathe(bm, prof, 40, idx=0, M=M)
    for ang in ((0, 90) if phillips else (0,)):
        v = box(bm, (r * 2.1, 0.8, 1.4), (0, 0, h - 0.3), idx=1)
        bmesh.ops.rotate(bm, cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(ang + slot_ang), 3, 'Z'), verts=v)
        bmesh.ops.transform(bm, matrix=M, verts=v)

bm = bmesh.new()
for i, (sx, sz) in enumerate([P(87, 65), P(87, 281), P(1112, 65), P(1112, 281)]):
    screw_head(bm, front(sx, sz), r=4.3, h=2.2, slot_ang=(15, -25, 40, -5)[i])
finish(bm, "Panel Screws", [M_NICKEL, M_SLOT], C_PANEL, sharp_deg=50)

# ------------------------------------------------------------- knobs
def lobes(n=8, depth=0.13):
    """rounded bulging lobes with sharp valleys (UA / Pultec crown)."""
    return lambda a: 1 - depth * (1 - abs(math.cos(n * a / 2)))

def prism_yz(bm, pts_yz, x0, x1, idx=0):
    """Extrude a side profile drawn in the (y, z) plane across x (mm)."""
    L = [bm.verts.new((x0 * MM, y * MM, z * MM)) for y, z in pts_yz]
    R = [bm.verts.new((x1 * MM, y * MM, z * MM)) for y, z in pts_yz]
    n = len(pts_yz)
    fs = [bm.faces.new(list(reversed(L))), bm.faces.new(R)]
    for i in range(n):
        j = (i + 1) % n
        fs.append(bm.faces.new((L[i], L[j], R[j], R[i])))
    for f in fs:
        f.material_index = idx
    return L + R

def slanted_box(bm, size, center, rx_deg, idx):
    v = box(bm, size, (0, 0, 0), idx=idx)
    bmesh.ops.rotate(bm, cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(rx_deg), 3, 'X'), verts=v)
    bmesh.ops.translate(bm, vec=Vector(center) * MM, verts=v)
    return v

def knob_object(bm, name, x, z, theta, mats, bevel=None):
    ob = finish(bm, name, mats, C_KNOBS, loc=(x, 0, z),
                rot=(math.radians(90), 0, -math.radians(theta)), rot_mode='ZYX', sharp_deg=38)
    if bevel:
        bv = ob.modifiers.new("Bevel", 'BEVEL'); bv.width = bevel * MM; bv.segments = 3
        bv.limit_method = 'ANGLE'; bv.angle_limit = math.radians(40)
    return ob

def big_knob(name, x, z, theta):
    """UA-style Pultec knob: smooth conical skirt with white index line,
    8-lobe crown, and a top with twin concentric rings around a smooth disc."""
    bm = bmesh.new()
    # low flared skirt (mostly hidden under the crown, carries the index line)
    lathe(bm, [(20.4, 0.0), (20.4, 0.8), (19.9, 1.7), (18.7, 3.0), (17.3, 3.8), (16.2, 4.0)], 128, idx=0)
    slanted_box(bm, (1.35, 0.5, 3.6), (0, 19.15, 2.45), 47, idx=1)          # white index line
    # dominant 8-lobe crown
    lathe(bm, [(16.6, 3.0), (16.6, 12.4), (16.3, 13.6), (15.4, 14.6), (14.0, 15.1), (12.8, 15.2)], 256, idx=0,
          rmod=lobes(8, 0.15))
    # raised outer ring
    lathe(bm, [(12.9, 15.1), (12.7, 15.7), (12.2, 16.1), (11.5, 16.1), (11.1, 15.7), (10.9, 15.2)], 128, idx=0,
          cap_bot=False, cap_top=False)
    # groove, thin inner ring, smooth slightly dished centre disc
    lathe(bm, [(11.0, 15.15), (10.4, 15.05), (10.2, 15.5), (9.9, 15.8), (9.5, 15.8), (9.2, 15.45),
               (8.9, 15.2), (5.0, 15.05), (0, 15.0)], 128, idx=0, cap_bot=False)
    return knob_object(bm, name, x, z, theta, [M_BAKE, M_POINTER])

def bar_knob(name, x, z, theta):
    """UA-style rectangular bar selector (CPS / KCS) with bevelled pointer end and white line."""
    bm = bmesh.new()
    lathe(bm, [(10.8, 0), (10.8, 1.4), (10.3, 2.4), (10.3, 9.0), (9.4, 10.6), (0, 11.0)], 96, idx=0)   # pivot boss
    prism_yz(bm, [(-19.0, 1.5), (21.0, 1.5), (21.0, 7.0), (15.5, 16.5), (-17.0, 16.5), (-19.0, 14.5)],
             -6.5, 6.5, idx=0)
    box(bm, (1.2, 12.5, 0.5), (0, 9.25, 16.6), idx=1)                       # line on the top face
    slanted_box(bm, (1.2, 10.6, 0.5), (0, 18.47, 11.87), -60, idx=1)        # line down the bevelled end
    return knob_object(bm, name, x, z, theta, [M_BAKE, M_POINTER], bevel=0.9)

def chicken_head(name, x, z, theta):
    """UA-style ATTEN SEL pointer: round hub with a tapered blade."""
    bm = bmesh.new()
    lathe(bm, [(9.6, 0), (9.6, 1.4), (9.1, 2.4), (9.1, 9.0), (8.2, 11.2), (5.5, 12.6), (0, 13.0)], 96, idx=0)
    base = hull(circle_pts(0, -1.0, 5.2, 48) + [(0, 21.0)])
    bot = [bm.verts.new((px * MM, py * MM, 2.0 * MM)) for px, py in base]
    top = [bm.verts.new((px * 0.42 * MM, (py * 0.88 + 1.2) * MM, 17.5 * MM)) for px, py in base]
    n = len(base)
    fs = [bm.faces.new(list(reversed(bot))), bm.faces.new(top)]
    for i in range(n):
        j = (i + 1) % n
        fs.append(bm.faces.new((bot[i], bot[j], top[j], top[i])))
    return knob_object(bm, name, x, z, theta, [M_BAKE], bevel=0.7)

def power_knob(name, x, z, theta):
    """UA-style OFF/ON knob: glossy dome with a small pointed tab."""
    bm = bmesh.new()
    lathe(bm, [(9.3, 0), (9.3, 1.0), (9.1, 6.4), (8.3, 9.0), (6.2, 10.7), (0, 11.3)], 96, idx=0)
    extrude_outline(bm, hull(circle_pts(0, 0, 8.8, 64) + [(0, 13.6)]), 0.0, 2.6, idx=0)
    return knob_object(bm, name, x, z, theta, [M_BAKE], bevel=0.5)

TOGGLE_UP_DEG, TOGGLE_DOWN_DEG = 66.0, 114.0     # lever object X rotation (IN = up, OUT = down)

def toggle_switch(name, x, z, up=True):
    bm = bmesh.new()
    M = front(x, z)
    lathe(bm, [(8.0, 0), (8.0, 0.6), (6.6, 0.7)], 48, idx=0, M=M)              # locking washer
    lathe(bm, [(6.8, 0.6), (6.8, 3.2), (6.2, 3.6)], 6, idx=0, M=M)             # hex nut
    lathe(bm, [(4.6, 3.0), (4.6, 9.0), (4.1, 9.5)], 48, idx=0, M=M)            # bushing
    base = finish(bm, name, [M_CHROME], C_KNOBS, sharp_deg=45)
    bm = bmesh.new()                                                            # lever, pivot at origin
    lathe(bm, [(0, 0), (2.2, 0.3), (1.6, 3.0), (1.5, 10.0)], 32, idx=0)         # chrome bat
    lathe(bm, [(1.6, 9.0), (2.4, 10.5), (2.9, 16.5), (2.8, 19.0), (2.1, 20.6), (0, 21.0)], 32, idx=1)
    lever = finish(bm, name + " Lever", [M_CHROME, M_BAKE], C_KNOBS, loc=(x, -8.0, z),
                   rot=(math.radians(TOGGLE_UP_DEG if up else TOGGLE_DOWN_DEG), 0, 0), sharp_deg=45)
    lever.parent = base
    lever.matrix_parent_inverse = base.matrix_world.inverted()
    return base, lever

def pilot_lamp(name, x, z):
    bm = bmesh.new()
    M = front(x, z)
    lathe(bm, [(9.0, 0), (9.0, 0.8), (8.0, 1.0)], 6, idx=0, M=M)                         # hex nut
    lathe(bm, [(8.2, 0.8), (8.2, 4.5), (7.6, 6.5), (6.8, 7.0)], 48, idx=0, M=M, flute=(36, 0.03, 1))  # knurled bezel
    lathe(bm, [(6.6, 5.5), (6.6, 7.4), (5.6, 9.0), (3.4, 10.2), (0, 10.6)], 12, idx=1, M=M)  # faceted jewel
    ob = finish(bm, name, [M_CHROME, M_JEWEL], C_KNOBS, sharp_deg=20)
    L = bpy.data.lights.new(name + " Glow", 'POINT')
    L.energy = 0.05; L.color = (0.25, 0.7, 1.0); L.shadow_soft_size = 0.004
    lo = bpy.data.objects.new(name + " Glow", L)
    lo.location = (x * MM, -14 * MM, z * MM)
    C_KNOBS.objects.link(lo)
    return ob

# ------------------------------------------------------------- layout (mm, panel centre)
X_LB, TOP_Z = P(341, 85)
X_LA = P(495, 85)[0]
X_HB = P(701, 85)[0]
X_HA = P(857, 85)[0]
X_BW, Z_BW = P(599, 248)
X_LF, Z_LF = P(416, 250)
X_HF, Z_HF = P(778, 252)
X_AS, Z_AS = P(1007, 92)
X_IN, Z_IN = P(265, 239)
X_PL, Z_PL = P(967, 178)
X_PW, Z_PW = P(1018, 257)

def cont_angle(v):            # 0..10 -> degrees clockwise from 12 o'clock
    return -135 + 27 * v

def detents(labels, first, last):
    n = len(labels)
    step = (last - first) / (n - 1)
    return [(s, first + step * i) for i, s in enumerate(labels)], first, step

LF_STEPS, LF_A0, LF_DA = detents(["20", "30", "60", "100"], -46.5, 46.5)
HF_STEPS, HF_A0, HF_DA = detents(["3", "4", "5", "8", "10", "12", "16"], -90.0, 90.0)
AS_STEPS, AS_A0, AS_DA = detents(["5", "10", "20"], -30.0, 30.0)
PW_OFF, PW_ON = 0.0, 52.0

# default settings: classic "low-end trick" - 60 cps boost 5 / atten 3, 10 kcs boost 3, atten sel 20
K = {}
K["lf_boost"] = big_knob("LF Boost", X_LB, TOP_Z, 0)
K["lf_atten"] = big_knob("LF Atten", X_LA, TOP_Z, 0)
K["hf_boost"] = big_knob("HF Boost", X_HB, TOP_Z, 0)
K["hf_atten"] = big_knob("HF Atten", X_HA, TOP_Z, 0)
K["atten_sel"] = chicken_head("HF Atten Select (KCS)", X_AS, Z_AS, 0)
K["lf_freq"] = bar_knob("LF Frequency (CPS)", X_LF, Z_LF, 0)
K["bandwidth"] = big_knob("HF Bandwidth", X_BW, Z_BW, 0)
K["hf_freq"] = bar_knob("HF Frequency (KCS)", X_HF, Z_HF, 0)
_tb, K["eq_in"] = toggle_switch("EQ In-Out Switch", X_IN, Z_IN, up=True)
LAMP = pilot_lamp("Pilot Lamp", X_PL, Z_PL)
K["power"] = power_knob("Power Switch", X_PW, Z_PW, 0)

# ------------------------------------------------------------- functional controls
# One empty holds every control value; drivers turn the knobs, flip the lever and light the lamp.
CTRL = bpy.data.objects.new("WEIGHT Controls", None)
CTRL.empty_display_type = 'PLAIN_AXES'; CTRL.empty_display_size = 0.01
CTRL.location = (0, -0.06, 0.09)
C_KNOBS.objects.link(CTRL)

# name: (default, min, max, is_int, description)   defaults = classic low-end trick
CONTROLS = {
    "lf_boost":  (5.0, 0.0, 10.0, False, "LOW FREQUENCY BOOST (0-10)"),
    "lf_atten":  (3.0, 0.0, 10.0, False, "LOW FREQUENCY ATTEN (0-10)"),
    "lf_freq":   (2,   0,   3,    True,  "CPS: 0=20 1=30 2=60 3=100"),
    "hf_boost":  (3.0, 0.0, 10.0, False, "HIGH FREQUENCY BOOST (0-10)"),
    "bandwidth": (5.0, 0.0, 10.0, False, "BANDWIDTH: 0=SHARP 10=BROAD"),
    "hf_freq":   (4,   0,   6,    True,  "KCS: 0=3 1=4 2=5 3=8 4=10 5=12 6=16"),
    "hf_atten":  (0.0, 0.0, 10.0, False, "HIGH FREQUENCY ATTEN (0-10)"),
    "atten_sel": (2,   0,   2,    True,  "ATTEN SEL KCS: 0=5 1=10 2=20"),
    "eq_in":     (1,   0,   1,    True,  "EQ: 1=IN 0=OUT"),
    "power":     (1,   0,   1,    True,  "POWER: 1=ON 0=OFF"),
}
for key, (dv, lo, hi, is_int, desc) in CONTROLS.items():
    CTRL[key] = int(dv) if is_int else float(dv)
    ui = CTRL.id_properties_ui(key)
    if is_int:
        ui.update(min=int(lo), max=int(hi), soft_min=int(lo), soft_max=int(hi), description=desc)
    else:
        ui.update(min=lo, max=hi, soft_min=lo, soft_max=hi, step=10, precision=2, description=desc)

def drive(target, path, index, key, expr):
    fc = target.driver_add(path, index) if index is not None else target.driver_add(path)
    d = fc.driver
    d.type = 'SCRIPTED'
    v = d.variables.new(); v.name = "v"; v.type = 'SINGLE_PROP'
    v.targets[0].id_type = 'OBJECT'; v.targets[0].id = CTRL; v.targets[0].data_path = '["%s"]' % key
    d.expression = expr
    return fc

def rz(deg0, deg_per_unit):
    """knob Z rotation (rad) = -(deg0 + deg_per_unit * v)"""
    return "%.6f - %.6f*v" % (-math.radians(deg0), math.radians(deg_per_unit))

for key in ("lf_boost", "lf_atten", "hf_boost", "hf_atten", "bandwidth"):
    drive(K[key], "rotation_euler", 2, key, rz(-135.0, 27.0))
drive(K["lf_freq"], "rotation_euler", 2, "lf_freq", rz(LF_A0, LF_DA))
drive(K["hf_freq"], "rotation_euler", 2, "hf_freq", rz(HF_A0, HF_DA))
drive(K["atten_sel"], "rotation_euler", 2, "atten_sel", rz(AS_A0, AS_DA))
drive(K["power"], "rotation_euler", 2, "power", rz(PW_OFF, PW_ON - PW_OFF))
drive(K["eq_in"], "rotation_euler", 0, "eq_in",
      "%.6f - %.6f*v" % (math.radians(TOGGLE_DOWN_DEG), math.radians(TOGGLE_DOWN_DEG - TOGGLE_UP_DEG)))
# pilot lamp follows POWER
em_idx = [i for i, s in enumerate(M_JEWEL.node_tree.nodes["Principled BSDF"].inputs) if s.name == "Emission Strength"][0]
drive(M_JEWEL.node_tree, 'nodes["Principled BSDF"].inputs[%d].default_value' % em_idx, None, "power", "2.5*v")
glow = bpy.data.objects.get("Pilot Lamp Glow")
if glow:
    drive(glow.data, "energy", None, "power", "0.05*v")

# sidebar panel (View3D > N > Weight). Stored in the .blend as a registered text block.
UI_SRC = """import bpy
class WEIGHT_PT_controls(bpy.types.Panel):
    bl_label = "ONIRIQUE WEIGHT"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Weight'
    def draw(self, context):
        c = bpy.data.objects.get("WEIGHT Controls")
        L = self.layout
        if c is None:
            L.label(text="WEIGHT Controls introuvable"); return
        def row(box, key, label):
            if key in c.keys():
                box.prop(c, '["%s"]' % key, text=label, slider=True)
        b = L.box(); b.label(text="LOW FREQUENCY")
        row(b, "lf_freq", "CPS (20/30/60/100)"); row(b, "lf_boost", "Boost"); row(b, "lf_atten", "Atten")
        b = L.box(); b.label(text="HIGH FREQUENCY")
        row(b, "hf_freq", "KCS (3-16)"); row(b, "hf_boost", "Boost"); row(b, "bandwidth", "Bandwidth")
        row(b, "hf_atten", "Atten"); row(b, "atten_sel", "Atten Sel (5/10/20)")
        b = L.box(); b.label(text="SWITCHES")
        row(b, "eq_in", "EQ In"); row(b, "power", "Power")
        b = L.box(); b.label(text="INTERFACE DU VST")
        b.operator("weight.export_vst", text="Exporter les photos", icon='RENDER_STILL').quality = 1.0
        b.operator("weight.export_vst", text="Export rapide (test)", icon='RENDER_STILL').quality = 0.25
        b.label(text="-> " + EXPORT_OUT)

class WEIGHT_OT_export_vst(bpy.types.Operator):
    bl_idname = "weight.export_vst"
    bl_label = "Exporter les photos du VST"
    bl_description = "Rend la facade et chaque position de chaque reglage au format de Resources/photo, en arriere-plan"
    quality: bpy.props.FloatProperty(default=1.0, min=0.05, max=4.0)
    def execute(self, context):
        import os, subprocess, tempfile
        os.makedirs(EXPORT_OUT, exist_ok=True)
        copy = os.path.join(tempfile.gettempdir(), "weight_export_copy.blend")
        bpy.ops.wm.save_as_mainfile(filepath=copy, copy=True)
        log = open(os.path.join(EXPORT_OUT, "export.log"), "w")
        subprocess.Popen([bpy.app.binary_path, "-b", copy, "--python", EXPORT_SCRIPT, "--",
                          EXPORT_OUT, str(self.quality)], stdout=log, stderr=subprocess.STDOUT)
        self.report({'INFO'}, "Export lance en arriere-plan -> " + EXPORT_OUT)
        return {'FINISHED'}

for cls in ("WEIGHT_PT_controls", "WEIGHT_OT_export_vst"):
    try:
        bpy.utils.unregister_class(getattr(bpy.types, cls))
    except Exception:
        pass
bpy.utils.register_class(WEIGHT_OT_export_vst)
bpy.utils.register_class(WEIGHT_PT_controls)
"""
EXPORT_DIR = '/Users/tom/Desktop/vst/OniriqueWeight/tools/blender'
UI_SRC = UI_SRC.replace("import bpy\n", "import bpy\nEXPORT_SCRIPT = %r\nEXPORT_OUT = %r\n" % (
    os.path.join(EXPORT_DIR, "export_photos.py"), os.path.join('/Users/tom/Desktop/vst/OniriqueWeight', "build", "photo-blender")), 1)
txt = bpy.data.texts.get("weight_controls_ui.py") or bpy.data.texts.new("weight_controls_ui.py")
txt.clear(); txt.write(UI_SRC); txt.use_module = True
try:
    exec(compile(UI_SRC, "weight_controls_ui.py", "exec"), {"__name__": "weight_controls_ui"})
except Exception as ex:
    print("UI panel not registered:", ex)

# ------------------------------------------------------------- silkscreen
def load_font(fname):
    p = os.path.join(FONT_DIR, fname)
    try:
        return bpy.data.fonts.load(p, check_existing=True) if os.path.exists(p) else None
    except Exception:
        return None

F_TXT = (load_font("Avenir Next.ttc") or load_font("Arial Bold.ttf"))
if F_TXT is None:
    p = "/System/Library/Fonts/Avenir Next.ttc"
    F_TXT = bpy.data.fonts.load(p, check_existing=True) if os.path.exists(p) else None

text_meshes = []
def text(s, x, z, size, align='CENTER', spacing=1.0):
    cu = bpy.data.curves.new("txt", 'FONT')
    cu.body = s
    cu.size = size * MM
    cu.align_x = align
    cu.align_y = 'CENTER'
    cu.extrude = 0.025 * MM
    cu.space_character = spacing
    if F_TXT:
        cu.font = F_TXT
    ob = bpy.data.objects.new("txt", cu)
    C_PRINT.objects.link(ob)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    me.transform(Matrix.Translation((x * MM, -0.04 * MM, z * MM)) @ ROT_FRONT)
    text_meshes.append(me)
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.curves.remove(cu)

def polar(cx, cz, theta, r):
    t = math.radians(theta)
    return cx + r * math.sin(t), cz + r * math.cos(t)

NUM = 4.6
LBL = 4.6
def cont_dial(cx, cz, r=24.5):
    for i in range(11):
        x, z = polar(cx, cz, cont_angle(i), r + (0.8 if i == 10 else 0))
        text(str(i), x, z, NUM)

def step_dial(cx, cz, steps, r):
    for s, a in steps:
        x, z = polar(cx, cz, a, r + 1.0 * (len(s) - 1))
        text(s, x, z, NUM)

TOP_LBL = 63.5
for xk, name in ((X_LB, "BOOST"), (X_LA, "ATTEN"), (X_HB, "BOOST"), (X_HA, "ATTEN")):
    cont_dial(xk, TOP_Z)
    text(name, xk, TOP_LBL, LBL, spacing=1.05)
step_dial(X_AS, Z_AS, AS_STEPS, 20.5)
text("ATTEN SEL", X_AS, TOP_LBL, LBL, spacing=1.05)

step_dial(X_LF, Z_LF, LF_STEPS, 24.5)
text("CPS", X_LF, Z_LF + 31, LBL)
text("LOW FREQUENCY", X_LF, -62.0, LBL, spacing=1.08)

cont_dial(X_BW, Z_BW, 25.0)
text("SHARP", X_BW - 21.5, Z_BW - 23.0, 2.9)
text("BROAD", X_BW + 21.5, Z_BW - 23.0, 2.9)
text("BANDWIDTH", X_BW, -62.5, LBL, spacing=1.08)

step_dial(X_HF, Z_HF, HF_STEPS, 25.0)
text("KCS", X_HF, Z_HF + 31, LBL)
text("HIGH FREQUENCY", X_HF, -62.0, LBL, spacing=1.08)

text("IN", X_IN, Z_IN + 22.7, LBL)
text("OUT", X_IN, Z_IN - 23.7, LBL)
text("OFF", X_PW + 1, Z_PW + 17.9, LBL)
text("ON", X_PW + 15, Z_PW + 9.6, LBL)

text("ONIRIQUE", -175, 17.2, 6.4, spacing=1.12)
text("PROGRAM EQUALIZER", -175, 7.8, 4.2, spacing=1.04)
text("TYPE W-1", -175, -1.6, 4.2, spacing=1.08)
text("WEIGHT", -175, -12.1, 5.2, spacing=1.25)

def flat_poly(bm, pts_xz, y=-0.05):
    vs = [bm.verts.new((x * MM, y * MM, z * MM)) for x, z in pts_xz]
    f = bm.faces.new(vs); f.normal_update()
    if f.normal.y > 0:
        f.normal_flip()

bm = bmesh.new()
# outlined name plate around "WEIGHT"
x0, x1, z0, z1, w = -194.0, -156.0, -16.9, -7.3, 0.55
for (ax, az, bx, bz) in ((x0, z0, x1, z0 + w), (x0, z1 - w, x1, z1), (x0, z0, x0 + w, z1), (x1 - w, z0, x1, z1)):
    flat_poly(bm, [(ax, az), (bx, az), (bx, bz), (ax, bz)])
# four-point sparkle emblem (Onirique star)
def sparkle(cx, cz, r, inner=0.22, n=4, rot=0.0):
    pts = []
    for i in range(n * 2 * 6):
        a = math.radians(rot) + 2 * math.pi * i / (n * 12)
        k = abs(math.cos(n * (a - math.radians(rot)) / 2))
        rr = inner * r + (1 - inner) * r * k ** 2.5
        pts.append((cx + rr * math.sin(a), cz + rr * math.cos(a)))
    flat_poly(bm, pts)
for me in text_meshes:
    bm.from_mesh(me)
    bpy.data.meshes.remove(me)
for f in bm.faces:
    f.material_index = 0
finish(bm, "Silkscreen Lettering", [M_PRINT], C_PRINT, sharp_deg=10)

# ---- official Onirique star logo (brand kit: logos/logo-etoile-transparent.png) as a decal
LOGO_PATH = '/Users/tom/Desktop/vst/OniriqueWeight/tools/blender/onirique-logo-etoile.png'
if os.path.exists(LOGO_PATH):
    img = bpy.data.images.load(LOGO_PATH, check_existing=True)
    try:
        img.pack()
    except Exception:
        pass
    aspect = img.size[1] / img.size[0] if img.size[0] else 0.909
    LW = 17.0
    LH = LW * aspect
    m = bpy.data.materials.new("WEIGHT Onirique Logo Decal")
    m.use_nodes = True
    nt = m.node_tree; N = nt.nodes; Lk = nt.links
    bsdf = N["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (0.86, 0.85, 0.84, 1)
    bsdf.inputs["Roughness"].default_value = 0.45
    tx = N.new("ShaderNodeTexImage"); tx.image = img; tx.interpolation = 'Cubic'
    Lk.new(tx.outputs["Alpha"], bsdf.inputs["Alpha"])
    try:
        m.surface_render_method = 'BLENDED'
    except Exception:
        m.blend_method = 'BLEND'
    bm = bmesh.new()
    cx, cz = -175.5, -29.0
    vs = [bm.verts.new(((cx + dx * LW / 2) * MM, -0.06 * MM, (cz + dz * LH / 2) * MM))
          for dx, dz in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    f = bm.faces.new(vs)
    uv = bm.loops.layers.uv.new("UVMap")
    for loop, (u, v) in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
        loop[uv].uv = (u, v)
    f.normal_update()
    if f.normal.y > 0:
        f.normal_flip()
    me = bpy.data.meshes.new("Onirique Logo Decal"); bm.to_mesh(me); bm.free()
    me.materials.append(m)
    ob = bpy.data.objects.new("Onirique Logo Decal", me)
    C_PRINT.objects.link(ob)

# ------------------------------------------------------------- chassis box
CH_W, CH_H, CH_D = 432.0, 124.0, 205.0
Y0 = PANEL_T
YR = Y0 + CH_D
bm = bmesh.new()
box(bm, (CH_W, CH_D, CH_H), (0, Y0 + CH_D / 2, 0))
finish(bm, "Chassis", [M_HAMMER], C_CHASSIS, bevel=1.2)
bm = bmesh.new()
top = Matrix.Translation((0, 0, CH_H / 2 * MM))
for sx, sy in [(-205, 15), (205, 15), (-205, CH_D - 10), (205, CH_D - 10), (0, CH_D / 2)]:
    screw_head(bm, Matrix.Translation((sx * MM, (Y0 + sy) * MM, CH_H / 2 * MM)), r=3.2, h=1.5)
for sx, sz in [(-205, 52), (205, 52), (-205, -52), (205, -52)]:
    screw_head(bm, Matrix.Translation((sx * MM, YR * MM, sz * MM)) @ ROT_REAR, r=3.2, h=1.5)
finish(bm, "Chassis Screws", [M_NICKEL, M_SLOT], C_CHASSIS, sharp_deg=50)

def rear(x, z, y=0.0):
    """local +Z points out of the rear wall."""
    return Matrix.Translation((x * MM, (YR + y) * MM, z * MM)) @ ROT_REAR

# rear-mounted parts (x is in FRONT-view coordinates: +x = power side)
# power inlet with fuse drawer
bm = bmesh.new()
box(bm, (30, 40, 4), (0, 0, 2), M=rear(185, 22))
box(bm, (24, 16, 6), (0, -8, 3), idx=1, M=rear(185, 22))
box(bm, (22, 12, 5), (0, 9, 2.8), idx=1, M=rear(185, 22))
finish(bm, "IEC Inlet & Fuse", [M_BLACKPL, M_BAKE], C_CHASSIS, bevel=0.6)

# power transformer: lamination stack, grey end bells, bracket
bm = bmesh.new()
box(bm, (56, 48, 34), (0, 0, 21), idx=0, M=rear(150, -40))
for sx in (-1, 1):
    box(bm, (10, 52, 38), (sx * 33, 0, 21), idx=1, M=rear(150, -40))
box(bm, (90, 4, 42), (0, -27, 21), idx=1, M=rear(150, -40))
box(bm, (90, 56, 3), (0, 0, 1.5), idx=1, M=rear(150, -40))
finish(bm, "Power Transformer", [M_LAM, M_GREY], C_CHASSIS, bevel=1.5)

# 6X4 rectifier in horizontal shield
bm = bmesh.new()
lathe(bm, [(13, 0), (13, 3), (11.5, 3.5), (11.5, 52), (10.5, 55), (0, 55.5)], 48, M=rear(112, 24))
lathe(bm, [(14.5, 0), (14.5, 5)], 48, flute=(18, 0.06, 1), M=rear(112, 24))
finish(bm, "V4 6X4 Rectifier (shielded)", [M_SHIELD], C_CHASSIS, sharp_deg=30)

# electrolytic cans
bm = bmesh.new()
for z in (24, -32):
    lathe(bm, [(18.5, 0), (18.5, 3), (17, 4), (17, 70), (16.2, 72), (0, 72.3)], 64, M=rear(72, z))
finish(bm, "Filter Capacitor Cans", [M_ALU], C_CHASSIS, sharp_deg=30)

# inductor / network boxes (hammertone)
bm = bmesh.new()
box(bm, (92, 56, 82), (0, 0, 41), M=rear(-5, 27))
box(bm, (92, 56, 82), (0, 0, 41), M=rear(-5, -31))
finish(bm, "Inductor Housings", [M_HAMMER], C_CHASSIS, bevel=2.0)

# audio transformer cans (smooth grey)
bm = bmesh.new()
box(bm, (54, 50, 56), (0, 0, 28), M=rear(-82, 30))
box(bm, (58, 54, 66), (0, 0, 33), M=rear(-86, -30))
box(bm, (52, 50, 58), (0, 0, 29), M=rear(-150, -30))
finish(bm, "Audio Transformers", [M_GREY], C_CHASSIS, bevel=2.0)

# ECC82 / ECC83 horizontal tube shields
bm = bmesh.new()
for x in (-128, -172):
    lathe(bm, [(12.5, 0), (12.5, 4), (10.5, 4.5), (10.5, 40), (9.6, 43), (4.0, 43.5), (3.6, 41), (0, 41)],
          48, M=rear(x, 30))
    lathe(bm, [(14, 0), (14, 4.5)], 48, flute=(16, 0.07, 1), M=rear(x, 30))
finish(bm, "V1-V3 ECC82 / ECC83 (shielded)", [M_SHIELD], C_CHASSIS, sharp_deg=30)

# input / output connectors
bm = bmesh.new()
for z in (-2, -26):
    lathe(bm, [(11, 0), (11, 2.5), (9, 3), (9, 14)], 32, M=rear(-195, z))
    box(bm, (20, 20, 2), (0, 0, 1), M=rear(-195, z))
finish(bm, "Input / Output Connectors", [M_SHIELD], C_CHASSIS, sharp_deg=40)

# ------------------------------------------------------------- studio
bm = bmesh.new()
box(bm, (3000, 3000, 2), (0, 0, -HH - 1))
finish(bm, "Studio Floor", [M_FLOOR], C_STUDIO)

target = bpy.data.objects.new("Camera Target", None)
target.location = (0, 0.02, -0.005)
C_STUDIO.objects.link(target)

def area(name, loc, energy, size, color=(1, 1, 1)):
    L = bpy.data.lights.new(name, 'AREA'); L.energy = energy; L.size = size; L.color = color
    o = bpy.data.objects.new(name, L); o.location = loc
    C_STUDIO.objects.link(o)
    o.constraints.new('TRACK_TO').target = target
    return o

area("Key Light", (-0.55, -0.65, 0.55), 16, 0.9, (1.0, 0.96, 0.9))
area("Fill Light", (0.75, -0.55, 0.12), 3, 1.2, (0.88, 0.92, 1.0))
area("Rim Light", (0.3, 0.75, 0.55), 18, 0.7)

cam_d = bpy.data.cameras.new("EQP Camera"); cam_d.lens = 55
cam_d.dof.use_dof = True; cam_d.dof.focus_object = target; cam_d.dof.aperture_fstop = 8
cam = bpy.data.objects.new("EQP Camera", cam_d)
cam.location = (-0.26, -0.72, 0.17)
C_STUDIO.objects.link(cam)
cam.constraints.new('TRACK_TO').target = target
scene.camera = cam
if scene.world is None:
    scene.world = bpy.data.worlds.new("World")
scene.world.use_nodes = True
bg = scene.world.node_tree.nodes.get("Background")
if bg:
    bg.inputs["Color"].default_value = (0.02, 0.018, 0.02, 1)
    bg.inputs["Strength"].default_value = 1.0

print("ONIRIQUE WEIGHT (EQP-1A) built:", len(root.all_objects), "objects")
