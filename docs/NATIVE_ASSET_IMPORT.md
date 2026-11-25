# Native static model import

## Creator workflow

F7 → Imported 3D asset library → **Import glTF / GLB...** opens an in-editor
file picker. Browse folders or paste a source filename (absolute, or relative
to the current map). Choose an output name and click **Import model**. This
creates `assets/name.rtsmesh` beside the map and refreshes the asset library.
Click **Register Model + Actor** to create the linked catalog entries, then
use the F8 actor palette to place it. Ctrl+S saves the catalog/placements.

Import writes a new resource immediately. Catalog registration is a separate
undoable transaction; undo does not delete imported resource files. Existing
files are never overwritten. Choose a new name to import another version.
The picker lists at most 2048 directory entries; a full source path can also
be entered. It is the same ImGui implementation on Windows and Linux.

The portable executable includes the compiler. No Python, Blender installation,
network service or runtime download is needed. An original source example is
shipped as `examples/beacon.gltf`.

Headless equivalent (does not register catalog entries):

```sh
RTSProjectTool --import-gltf examples/beacon.gltf projects/my_map.rtsproject.json assets/beacon.rtsmesh
```

## Supported contract

- glTF 2.0 JSON and binary GLB, one scene with up to 128 nodes, 32 distinct
  meshes and 32 combined triangle primitive draws. Each draw has its own
  opaque material and optional PNG/JPEG albedo. Empty groups and repeated
  mesh instances are supported; every node must be reachable exactly once.
- POSITION and NORMAL float32 attributes; optional UV0 (float or normalized
  unsigned byte/short). Indexed unsigned byte/short/int or nonindexed triangles.
- Translation, normalized quaternion rotation and positive nonuniform scale.
  Parent transforms compose before positions are baked; normals use the full
  composed inverse transpose then normalization.
- Base color tint and optional PNG/JPEG image, embedded URI, GLB bufferView
  or relative external file. One opaque single-sided material, repeat wrapping
  and linear filtering. glTF OPAQUE ignores image/factor alpha. An imported
  multi-primitive mesh retains one draw range, material and image per primitive.
- One primitive still uses the byte-identical legacy **RTSMESH1** format.
  Two or more primitives use **RTSMESH2**, read by the same bounded CPU loader.
  The GL3.3 renderer uses a single indexed GPU mesh and one cached texture/draw
  per material range. No gameplay, collision or replay logic changes.

**Not full glTF or PBR rendering.** Lighting is still the engine's simple
ambient/diffuse preview; scalar metallic/roughness values have no effect.
Normal/occlusion/emissive/metallic-roughness maps, nondefault samplers,
material extensions, alpha blending/cutout, double-sided rendering,
node matrices, mirrored/singular scales, skins,
animations, morphs, sparse accessors and extra vertex attributes are rejected.
The Python converter remains available with its older, narrower transform
and image contract; native import is now the preferred creator workflow.

## RTSMESH2 binary surface layout

The cache remains little-endian and versioned. The v2 24-byte header contains
ASCII `RTSMESH2`, u32 vertex count, u32 index count, u32 surface count
(1..32) and a zero reserved u32. Interleaved 8-float vertices (position XYZ,
normal XYZ, UV0 XY) and u32 indices follow. Each material adds a
32-byte header: four u32 values (first index, index count, image width,
image height), then four float32 RGBA factors, followed by optional RGBA8
image pixels. All surfaces must cover consecutive, non-overlapping,
triangle-aligned index spans; image byte counts and exact file end are
strictly checked before allocation. Out-of-range/invalid references fail
validation and receive a procedural fallback rather than crashing the game.

The included [dual-material scene](../projects/multi_material_showcase.rtsproject.json)
uses an original [two-primitive glTF](../assets/source/beacon_dual.gltf)
with a textured blue main body and an untextured amber cap. F7 lists surface
counts, and Windows/Linux release packages independently reconstruct the
compiled asset with their native importer and compare exact bytes.

## Validation and publication

Parser allocations are capped at 128 MiB cumulatively.
Input/resource files are limited to 64 MiB, aggregate declared buffers to
64 MiB, 32 buffers, 256 views/accessors, 250,000 **total** vertices, 750,000
**total** indices, 1–32 primitives and up to 2048×2048 decoded images
per material. The complete compiled mesh must remain under 130 MiB. Validate buffer and accessor arithmetic before
calling parser helpers that read geometry. Reject network, absolute, escaped
and source-directory-traversing resource URIs, including symlink escapes.
No importer subprocesses or author scripts run.

Compile entirely in memory before writing. Reserve a sibling `.importing`
directory, serialize there, validate using the production runtime loader,
and publish using an exclusive hard link. Exceptions clean the staging area.
This prevents partial final assets and existing-file overwrites, including
concurrent imports. Destination filesystems must support hard links (ordinary
NTFS/ext4 do); unsupported volumes report an error, not a false success.
A process kill can leave the reserved `.importing` directory, which must be
removed before retrying that same output name.

## Dependencies and regression coverage

Vendored, unchanged upstream sources (MIT licenses shipped in `licenses/`):

- cgltf `85cd62382dfea638278962690cf515023f33ed00`,
  https://github.com/jkuhlmann/cgltf
- stb_image `9600522de53973336fa033a289858a4354cbb04c`,
  https://github.com/nothings/stb (PNG/JPEG only, filesystem API disabled).

Only `RTSGltfImport.cpp` defines each implementation. `RTSAuthoring` links it
into both CLI and native creator. No third-party parser is added to simulation
updates or per-frame rendering.

`native_gltf_import` runs the actual CLI against real glTF and synthesized GLB,
checks byte parity, quaternion/scale/translation against independent numeric
expectations, JPEG decoding, opaque alpha, external resources, no overwrite,
malformed offsets/counts, unsupported features, URI rejection and output
rollback. Existing tests continue to cover registration undo/redo, F8 placement
and asset audit. Both release-package jobs compile the bundled source using
the installed executable and compare output to the existing mesh. CI also
captures the actual F7 import modal and normal F7/F8/F9 asset views.

## Reference research and next work

Galaxy's documented Importer distinguishes source selection, project-relative
import location and file inspection; its screenshot shows a source directory
tree and explicit Import Path. This informed the original picker/output-name
workflow without copying UI artwork or proprietary formats:
https://s2editor-guides.readthedocs.io/New_Tutorials/01_Introduction/014_The_Importer/

Recoil's glTF guide and `rts/Rendering/Models/GLTFParser.cpp` demonstrate that
standard interchange and explicit engine-specific feature boundaries coexist.
Recoil uses fastgltf and separate model metadata/material conventions; no
Recoil source was copied:
https://recoilengine.org/docs/guides/getting-started/blender-gltf-import/
https://github.com/beyond-all-reason/RecoilEngine/blob/master/rts/Rendering/Models/GLTFParser.cpp

Next: retained node identities and sockets, rendered preview turntable, source
manifest/content hash/reimport, background import jobs, texture deduplication,
PBR/sRGB/alpha rendering, then skeletons/clips and actor animation events.
Static scene assembly import is implemented; full glTF/PBR parity is not.

## November 25 — static scene assemblies and asset inspection

Native import now accepts one scene containing up to 128 TRS nodes, 32
distinct meshes and 32 combined primitive draw surfaces, including empty
group nodes and multiple instances of one source mesh. Parent transforms
are composed before geometry is baked; nested rotated/nonuniformly scaled
normals use the full inverse transpose. All nodes must be reachable once;
cycles, repeated roots, disconnected nodes, unused meshes, matrices,
skinning and animation are rejected. Legacy single-root bytes remain
unchanged. Both RTSMESH1/2 remain unchanged; node names and transforms
are **not** retained in the compiled resource, so this is static assembly
import, not a runtime skeleton or attachment system. Aggregate decoded
image storage is bounded before allocation (118 MiB plus bounded geometry).
F7 adds case-insensitive path search, invalid-only filter, optional model
bounds and decoded image memory totals. Scan remains explicit, not per frame.
Nested TRS and repeated-mesh index rebasing have independent numerical CLI
regressions. Older single-root limitations in the preceding text are historical.

## November 25 — retained static nodes (RTSMESH3)

For scenes containing two or more nodes, the native compiler now produces
RTSMESH3. The 24-byte header and geometry/material ranges match RTSMESH2,
except the last u32 is the node count (2..128). After all surfaces, each
node stores four little-endian u32 values (parent index, first surface,
surface count, name byte length), 16 column-major float32 local matrix
elements, then exactly that many name bytes (0..255). Root parent is
0xffffffff; all other parents must precede the node. Each node owns a
contiguous range of surfaces (group nodes own zero), and all ranges must
cover the surface table. Model-space matrices are recomposed from local
matrices by the shared parent-first function. Invalid indices, transforms,
control characters, ranges, truncated records or trailing bytes reject
the asset. Single-node imports still produce byte-identical RTSMESH1/2.

F7 > Inspect model nodes opens a real selectable hierarchy and model-space
origin/basis inspector. Unique nonempty node names can be resolved through
`rtsFindUniqueModelNode`; duplicates are deliberately ambiguous. CLI:
`RTSProjectTool --model-info map.json assets/name.rtsmesh [node-name]`
returns JSON without loading the map, including both local and model
matrices. Named nodes are socket candidates, not yet live attachments.
Geometry remains baked; the engine does not animate these node transforms
or store skin weights. Model names are stable for the same import order,
not persistent GUIDs across arbitrary source edits.

## Model studio, cosmetic attachments and explicit reimport

F7 Model viewer now shows the actual OpenGL indexed textured model in a
bounded isolated color/depth framebuffer, with orbit drag, zoom wheel,
frame reset, bounds, selected-node axes and optional surface highlighting.
It works on legacy meshes as well as RTSMESH3. The viewport reuses the
production shader/draw implementation, restores the GL state it changes,
and retains only the current preview mesh in its own GPU cache.

Actor.attachments is an inherited array of up to eight objects with
`socket` (unique retained node name), `model` (typed Model ID),
`offset`, `scale`, `yaw` (degrees), `tint` and `opacity`.
F7 exposes a transactional editor for these properties. Model links
participate in catalog rename and deletion protection, including placement
Actor overrides. Runtime draws child models at parent world * socket model
* offset * yaw * scale (including child Model scale), for Units and
Doodads. Missing/ambiguous sockets are skipped at render time and rejected
by the asset audit. No recursive attachments, weapon muzzle changes,
collision changes, articulated animation or simulation-dependent effects.

Explicit `RTSProjectTool --reimport-gltf source project assets/name.rtsmesh`
or the F7 import dialog replacement checkbox recompiles a valid existing
asset at its same path using staged validation and same-filesystem rename.
A failed compile leaves the existing file untouched. All old unique named
nodes must remain uniquely resolvable in the replacement. No source
manifest or automatic dependency tracking yet. Asset file replacement is
not part of map undo. Keep original source files separately.
