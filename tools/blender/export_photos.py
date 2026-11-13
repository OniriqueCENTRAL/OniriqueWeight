# Onirique Weight - export the Blender model as the plugin's photographs.
#
# Produces exactly what Source/PluginEditor.cpp loads from Resources/photo:
#   front-on.jpg / front-off.jpg   the front without its moving parts, lamp lit / dark (kWidth x kHeight x kPhotoScale)
#   <photo>.png                    every position of a control (part + its shadow, transparent), square cells,
#                                  eight to a row, ordered like the parameter (see Layout.h)
#   preview.png                    front-on + every control at 2/3 of its travel, composited like the editor
#
# Positions, cell sizes, frame counts and angles are read from the plugin's Source/ui/Layout.h.
#
# Headless:  Blender -b model.blend --python tools/blender/export_photos.py -- <out_dir> [quality] [only,names]
#            (only: front, dial, lowfreq, highfreq, attensel, eqin, power, preview)
import bpy, math, os, re, sys, time
import numpy as np

REPO = '/Users/tom/Desktop/vst/OniriqueWeight'                       # fallback when run from inside Blender
if "__file__" in globals() and os.path.exists(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Source")):
    REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
LAYOUT_H = os.path.join(REPO, "Source", "ui", "Layout.h")
DEFAULT_OUT = os.path.join(REPO, "build", "photo-blender")

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = os.path.abspath(argv[0]) if argv else DEFAULT_OUT
QUALITY = float(argv[1]) if len(argv) > 1 else 1.0
ONLY = [s for s in argv[2].split(",") if s] if len(argv) > 2 else []
def wanted(name):
    return not ONLY or name in ONLY

# ------------------------------------------------------------------ Layout.h
src = open(LAYOUT_H).read()
kWidth = int(re.search(r"kWidth\s*=\s*(\d+)", src).group(1))
kHeight = int(re.search(r"kHeight\s*=\s*(\d+)", src).group(1))
kScale = int(re.search(r"kPhotoScale\s*=\s*(\d+)", src).group(1))
FRAMES = {k: (float(a), float(b)) for k, a, b in
          re.findall(r"case Kind::(\w+):\s*return \{\s*([-\d.]+)f,\s*([-\d.]+)f\s*\}", src)}
CONTROLS = []
for m in re.finditer(r'\{\s*"(\w+)",\s*"([^"]+)",\s*"(\w+)",\s*Kind::(\w+),\s*([-\d.]+)f,\s*([-\d.]+)f,\s*'
                     r'([-\d.]+)f,\s*(\d+),\s*([-\d.]+)f,\s*([-\d.]+)f\s*\}', src):
    cid, name, photo, kind, x, y, size, frames, a0, a1 = m.groups()
    CONTROLS.append(dict(id=cid, photo=photo, kind=kind, x=float(x), y=float(y), frames=int(frames),
                         a0=float(a0), a1=float(a1)))
assert CONTROLS and FRAMES, "Layout.h could not be parsed"

# Layout.h control id -> Blender object(s); the first is the one that turns
OBJ = {
    "lowboost": ["LF Boost"], "lowatten": ["LF Atten"], "highboost": ["HF Boost"], "highatten": ["HF Atten"],
    "bandwidth": ["HF Bandwidth"], "lowfreq": ["LF Frequency (CPS)"], "highfreq": ["HF Frequency (KCS)"],
    "attensel": ["HF Atten Select (KCS)"], "eqin": ["EQ In-Out Switch Lever", "EQ In-Out Switch"],
    "bypass": ["Power Switch"],
}
TOGGLE_UP_DEG, TOGGLE_DOWN_DEG = 66.0, 114.0     # lever X rotation in the model (IN up, OUT down)
CATCHERS = ["Front Panel", "Silkscreen Lettering", "Onirique Logo Decal", "Silkscreen Dial Ticks"]

PANEL_W_MM = 482.6
PT_PER_MM = kWidth / PANEL_W_MM
MM = 0.001
def P(x, y):
    """Layout point -> model metres (x, z)."""
    return ((x - kWidth / 2) / PT_PER_MM * MM, (kHeight / 2 - y) / PT_PER_MM * MM)

scene = bpy.context.scene
ctrl = bpy.data.objects.get("WEIGHT Controls")
moving = set()
for ids in OBJ.values():
    for n in ids:
        if bpy.data.objects.get(n):
            moving.add(n)

# ------------------------------------------------------------------ render setup
saved = dict(engine=scene.render.engine, rx=scene.render.resolution_x, ry=scene.render.resolution_y,
             pct=scene.render.resolution_percentage, cam=scene.camera, transp=scene.render.film_transparent,
             path=scene.render.filepath, fmt=scene.render.image_settings.file_format,
             mode=scene.render.image_settings.color_mode)
hidden = {o.name: o.hide_render for o in scene.objects}
catch = {o.name: o.is_shadow_catcher for o in scene.objects}
rot = {n: tuple(bpy.data.objects[n].rotation_euler) for n in moving}
power0 = ctrl["power"] if ctrl and "power" in ctrl.keys() else None

# only the Weight model is photographed: anything else in the scene (a default cube...) is left out
weight_coll = bpy.data.collections.get("Onirique Weight (EQP-1A)")
if weight_coll:
    ours = {o.name for o in weight_coll.all_objects}
    for o in scene.objects:
        if o.name not in ours and o.type != 'CAMERA':
            o.hide_render = True

scene.render.engine = 'CYCLES'
try:
    prefs = bpy.context.preferences.addons["cycles"].preferences
    prefs.compute_device_type = 'METAL'
    prefs.get_devices()
    for d in prefs.devices:
        d.use = True
    scene.cycles.device = 'GPU'
except Exception as ex:
    print("GPU not available, CPU render:", ex)
scene.cycles.use_denoising = True
scene.render.resolution_percentage = 100
scene.render.use_border = False

cam_data = bpy.data.cameras.new("WEIGHT UI Camera")
cam_data.type = 'ORTHO'
cam_data.sensor_fit = 'HORIZONTAL'
cam_data.clip_start = 0.01
cam_data.clip_end = 5.0
cam = bpy.data.objects.new("WEIGHT UI Camera", cam_data)
cam.rotation_euler = (math.radians(90), 0, 0)          # looks along +Y, straight at the front
scene.collection.objects.link(cam)
scene.camera = cam

def place_camera(cx_pt, cy_pt, width_pt, px_w, px_h):
    x, z = P(cx_pt, cy_pt)
    cam.location = (x, -1.0, z)
    cam_data.ortho_scale = width_pt / PT_PER_MM * MM
    scene.render.resolution_x, scene.render.resolution_y = px_w, px_h

def refresh():
    if ctrl:
        ctrl.update_tag()
    scene.frame_set(scene.frame_current)

def mute_drivers(names, state):
    for n in names:
        ob = bpy.data.objects.get(n)
        if ob and ob.animation_data:
            for fc in ob.animation_data.drivers:
                fc.mute = state

def render_to(path, fmt):
    scene.render.image_settings.file_format = fmt
    if fmt == 'JPEG':
        scene.render.image_settings.color_mode = 'RGB'
        scene.render.image_settings.quality = 95
    else:
        scene.render.image_settings.color_mode = 'RGBA'
        scene.render.image_settings.color_depth = '8'
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)

os.makedirs(OUT, exist_ok=True)
tmp = os.path.join(OUT, "_frames")
os.makedirs(tmp, exist_ok=True)
t0 = time.time()
log = lambda s: (print("[weight-export %4.0fs] %s" % (time.time() - t0, s)), sys.stdout.flush())

def set_pose(c, degrees):
    ob = bpy.data.objects[OBJ[c["id"]][0]]
    if c["kind"] == "toggle":
        # Layout: 180 = handle down (OUT), 0 = handle up (IN)
        ob.rotation_euler[0] = math.radians(TOGGLE_UP_DEG + (TOGGLE_DOWN_DEG - TOGGLE_UP_DEG) * degrees / 180.0)
    else:
        ob.rotation_euler[2] = -math.radians(degrees)

try:
    # -------------------------------------------------------------- the front
    if wanted("front"):
        for n in moving:
            bpy.data.objects[n].hide_render = True
        scene.render.film_transparent = False
        scene.cycles.samples = max(16, int(256 * QUALITY))
        place_camera(kWidth / 2, kHeight / 2, kWidth, kWidth * kScale, kHeight * kScale)
        for lit, name in ((False, "front-off.jpg"), (True, "front-on.jpg")):
            if ctrl:
                ctrl["power"] = 1 if lit else 0
            refresh()
            render_to(os.path.join(OUT, name), 'JPEG')
            log(name)

    # -------------------------------------------------------------- each control's photographs
    done = set()
    for c in CONTROLS:
        if c["photo"] in done or not wanted(c["photo"]):
            continue
        done.add(c["photo"])
        before, after = FRAMES[c["kind"]]
        size = before + after
        px = int(round(size * kScale))
        cols = min(c["frames"], 8)
        rows = (c["frames"] + cols - 1) // cols
        names = [n for n in OBJ[c["id"]] if bpy.data.objects.get(n)]

        keep = set(names) | set(CATCHERS)
        for o in scene.objects:
            if o.type in {'LIGHT', 'CAMERA'} and o.name != "Pilot Lamp Glow":
                continue
            o.hide_render = o.name not in keep
            o.is_shadow_catcher = o.name in CATCHERS
        scene.render.film_transparent = True
        scene.cycles.samples = max(16, int(160 * QUALITY))
        mute_drivers(names, True)
        place_camera(c["x"] - before + size / 2, c["y"] - before + size / 2, size, px, px)

        sheet = np.zeros((rows * px, cols * px, 4), dtype=np.float32)
        for k in range(c["frames"]):
            deg = c["a0"] if c["frames"] == 1 else c["a0"] + (c["a1"] - c["a0"]) * k / (c["frames"] - 1)
            set_pose(c, deg)
            f = os.path.join(tmp, "%s_%02d.png" % (c["photo"], k))
            render_to(f, 'PNG')
            img = bpy.data.images.load(f)
            a = np.empty(px * px * 4, dtype=np.float32)
            img.pixels.foreach_get(a)
            bpy.data.images.remove(img)
            r, q = divmod(k, cols)
            cell = np.flipud(a.reshape(px, px, 4)).copy()
            # soft knee on the catcher's faint residue, and a feather towards the cell's edges,
            # so a shadow never ends on a hard line or a visible square
            al = cell[..., 3]
            al = np.clip((al - 0.012) / 0.05, 0.0, 1.0) * al
            yy, xx = np.mgrid[0:px, 0:px]
            edge = np.minimum(np.minimum(xx, px - 1 - xx), np.minimum(yy, px - 1 - yy)).astype(np.float32)
            al = al * np.clip(edge / (0.10 * px), 0.0, 1.0) ** 1.5
            cell[..., 3] = al
            sheet[r * px:(r + 1) * px, q * px:(q + 1) * px] = cell
            log("%s %d/%d" % (c["photo"], k + 1, c["frames"]))
        mute_drivers(names, False)
        for o in scene.objects:
            o.hide_render = hidden.get(o.name, o.hide_render)
            o.is_shadow_catcher = catch.get(o.name, False)

        out = bpy.data.images.new(c["photo"] + "_sheet", cols * px, rows * px, alpha=True)
        out.pixels.foreach_set(np.flipud(sheet).ravel())
        out.filepath_raw = os.path.join(OUT, c["photo"] + ".png")
        out.file_format = 'PNG'
        out.save()
        bpy.data.images.remove(out)
        log("%s.png  %dx%d (%d frames, %d px cells)" % (c["photo"], cols * px, rows * px, c["frames"], px))

    # -------------------------------------------------------------- preview, composited like the editor
    if wanted("preview") or not ONLY:
        fp = os.path.join(OUT, "front-on.jpg")
        if os.path.exists(fp):
            fi = bpy.data.images.load(fp)
            W, H = fi.size
            base = np.empty(W * H * 4, dtype=np.float32); fi.pixels.foreach_get(base)
            base = np.flipud(base.reshape(H, W, 4)).copy()
            bpy.data.images.remove(fi)
            sheets = {}
            for c in CONTROLS:
                sp = os.path.join(OUT, c["photo"] + ".png")
                if not os.path.exists(sp):
                    continue
                if c["photo"] not in sheets:
                    si = bpy.data.images.load(sp)
                    sw, sh = si.size
                    a = np.empty(sw * sh * 4, dtype=np.float32); si.pixels.foreach_get(a)
                    sheets[c["photo"]] = np.flipud(a.reshape(sh, sw, 4))
                    bpy.data.images.remove(si)
                before, after = FRAMES[c["kind"]]
                px = int(round((before + after) * kScale))
                cols = min(c["frames"], 8)
                k = (c["frames"] - 1) * 2 // 3
                r, q = divmod(k, cols)
                cell = sheets[c["photo"]][r * px:(r + 1) * px, q * px:(q + 1) * px]
                L = int(round((c["x"] - before) * kScale)); T = int(round((c["y"] - before) * kScale))
                x0, y0 = max(L, 0), max(T, 0)
                x1, y1 = min(L + px, W), min(T + px, H)
                sub = cell[y0 - T:y1 - T, x0 - L:x1 - L]
                al = sub[..., 3:4]
                base[y0:y1, x0:x1, :3] = sub[..., :3] * al + base[y0:y1, x0:x1, :3] * (1 - al)
            base[..., 3] = 1.0
            pv = bpy.data.images.new("weight_preview", W, H, alpha=False)
            pv.pixels.foreach_set(np.flipud(base).ravel())
            pv.filepath_raw = os.path.join(OUT, "preview.png"); pv.file_format = 'PNG'; pv.save()
            bpy.data.images.remove(pv)
            log("preview.png")
finally:
    # put the scene back as it was
    for n, r in rot.items():
        bpy.data.objects[n].rotation_euler = r
    for o in scene.objects:
        if o.name in hidden:
            o.hide_render = hidden[o.name]
        o.is_shadow_catcher = catch.get(o.name, False)
    mute_drivers(list(moving), False)
    if ctrl and power0 is not None:
        ctrl["power"] = power0
    bpy.data.objects.remove(cam)
    bpy.data.cameras.remove(cam_data)
    scene.render.engine = saved["engine"]
    scene.render.resolution_x, scene.render.resolution_y = saved["rx"], saved["ry"]
    scene.render.resolution_percentage = saved["pct"]
    scene.camera = saved["cam"]
    scene.render.film_transparent = saved["transp"]
    scene.render.filepath = saved["path"]
    scene.render.image_settings.file_format = saved["fmt"]
    scene.render.image_settings.color_mode = saved["mode"]
    refresh()

log("done -> " + OUT)
