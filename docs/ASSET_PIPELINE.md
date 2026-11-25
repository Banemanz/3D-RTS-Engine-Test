# Production asset format decision — glTF + compiled RTSMESH1

## Architecture decision

The independent RTS engine and its F7/F8/F10 editors should use **glTF 2.0**
as the authored **interchange** format, especially self-contained \`.glb\`
for models exported from Blender, while maintaining the original JSON
Unit -> Actor -> Model object graph as the gameplay/presentation contract.

Never make creators use a proprietary modeling format or Blender plugin.
The engine may cache validated, optimized data in a private binary format;
it must not place gameplay logic or authoritative collision inside that file.

Short-term static mesh runtime format: **RTSMESH1** compiled from glTF, with:
- Little-endian 8-byte magic \`RTSMESH1\`, followed by four u32 values:
  vertex count, triangle index count, optional image width/height.
- Four float32 linear base color factors (RGBA).
- Interleaved float32 vertices: position XYZ, normal XYZ, UV0 XY.
- Triangle uint32 indices, then optional tightly packed RGBA8 texture.
- Limits on geometry/image dimensions and strict file-length validation.

\`tools/import_gltf.py\` is a dependency-free C++-runtime-independent
Python compiler for the first **static** subset. It accepts .glb and .gltf,
bound-checks accessors and image resources, supports inlined/external
buffers, CRC-validated 8-bit PNG RGB/RGBA textures, node translation and
positive axis scales, glTF baseColorFactor and one material/triangle mesh.

**Do not claim full glTF support yet.** Missing by explicit design:
multiple meshes/primitives, animation, skinning, skeleton hierarchy,
morph targets, rotated node transforms, required extensions,
normal/metallic/roughness/emissive maps, texture sampler modes,
JPEG and KTX2 decoding, transparent cutout materials and material variants.
The converter must reject unsupported features, never silently pretend
to import them. Subsequent steps can replace this narrow converter with
a pinned glTF parser and a more capable content build pipeline.

Recommended full roadmap:

1. **Asset IDs and manifest** — stable content paths independent of OS and
   working directory, safe project-relative resolution, dependencies, hash
   tracking, errors in F7/F8. No author script execution.
2. **Static geometry in F8 and F9** — load validated compiled mesh/PNG
   into cached OpenGL VAO/VBO/IBO/texture (not per-frame upload), preserve
   procedural fallback; do not alter simulation or vision rules.
3. **Materials and textures** — PBR metallic/roughness subset in shader,
   sRGB color handling, proper alpha/culling; retain simple legacy mode.
4. **Skeletons and animation clips** — glTF skinning, indexed joints,
   bind poses, event markers with Actor graph actions, attachment sockets
   for weapons/projectiles/props. Sim controls event timing; renderer
   processes presentation without changing damage.
5. **Texture processing** — author from PNG/JPEG and use optional
   KTX2/Basis Universal (UASTC/ETC1S) at package build time, with GPU
   transcoding and fallback. Avoid making KTX2 mandatory for hobbyists.
6. **Asset scale** — background I/O, asset-id cache, versioned content
   hash cache, deduplication, atlas/instancing, mesh LOD, GPU upload budgets,
   editor hot reload and missing-asset placeholders.

**Audio:** WAV authoring, Ogg Vorbis/Opus later for streamed content;
sound events should be presentation-only and independent of replays.

## RTS and format research (November 2026)

- [Recoil glTF documentation](https://recoilengine.org/docs/guides/getting-started/blender-gltf-import/):
  modern standardized files, unlike legacy S3O; their initial loader itself
  deliberately does not implement every glTF material or animation feature.
- [0 A.D.](https://play0ad.com/tr/community/participate/):
  model/texture/animation assets referenced through actor definitions and
  variant/prop data; separation is more important than preserving DAE.
- [OpenRA modding](https://github-wiki-see.page/m/OpenRA/OpenRA/wiki/Modding-Guide):
  explicit mod manifests and declarative actor/view data; not directly
  relevant as a 3D mesh format, but a good authoring organization reference.
- [Warzone 2100 PIE4](https://github.com/Warzone2100/warzone2100/blob/master/doc/PIE.md):
  mature custom model specification, not a reason to repeat proprietary format
  costs in a new engine.
- [MegaGlest](https://glest.org/en/engine.php): G3D and XML; legacy authoring
  tooling illustrates why new creators should not depend on old exporters.
- [openage media conversion](https://github.com/SFTtech/openage/blob/master/doc/code/converter/workflow.md):
  lazily convert needed asset dependencies, not every asset on map load.
- [Khronos glTF registry](https://registry.khronos.org/glTF/), [KTX](https://www.khronos.org/ktx/):
  standards and GPU-native texture delivery.

Do not copy GPL implementation code or proprietary game assets. A
data-driven RTS creation tool must preserve both Linux/Windows parity and
authored-content ownership boundaries.

## Editor asset library and scene placement — November 17

F7 now discovers and validates bounded project-local compiled mesh assets
from the map's assets/ directory on initial open or manual Scan / refresh.
A creator can inspect mesh count, triangle count and imported texture size,
then Register Model + Actor to add the linked typed catalog entries in one
validated/undoable transaction. Re-registering an existing reference is
idempotent. The F8 Scene editor provides an imported Actor palette with
Place at camera focus and Use terrain doodad brush buttons. Scanning
increments an asset revision, explicitly invalidating the cached OpenGL
mesh/texture resources once so modified compiled models can reload.

This workflow does NOT yet import raw .glb from a file picker: creators
convert via tools/import_gltf.py into assets/*.rtsmesh, then register it
in the editor. Linux and Windows game/editor binaries need no Python runtime
once compiled. The existing strict single-primitive static glTF subset
limitations still apply. Test: asset_library CTest.

## Standalone packaging audit

Use `RTSProjectTool --asset-report projects/my_map.rtsproject.json`
from the repository root or the portable installed bundle to verify that
every Model with a nonprocedural mesh reference resolves to a valid
project-local RTSMESH1 file. The audit checks exact mesh binary sizes,
indices, vertices, references and path safety. It reports per-Model
errors, counts unique source files to prevent repeated validation, and
returns a nonzero exit status when an asset is missing, unsupported or
corrupt. `--validate` remains a gameplay/catalog validation command
and intentionally does not require external asset files. CI verifies
bundled sample dependencies with both Windows and Linux executables.

## Importer regression and geometric constraints

The Python self-test also constructs a valid GLB 2.0 binary container
with a BIN chunk from the original glTF fixture and verifies that
both glTF and GLB produce **identical compiled geometry and texture
bytes**. The importer explicitly rejects node hierarchies, extra
tangent/skin/secondary-UV attributes, floating-point index buffers,
and malformed GLB lengths rather than discarding information or
silently distorting a model. The OpenGL imported-mesh vertex shader
uses an inverse-transpose normal matrix so nonuniform scaling of
original doodads does not shear normal-based lighting.


## November 17 — native static asset import

F7 now imports raw glTF/GLB through an in-editor source picker using pinned
cgltf and stb_image, without an external Python process. Native quaternion
rotation, positive nonuniform scale, PNG/JPEG base color, validated publication
and a matching CLI are implemented; the existing Model/Actor registration and
F8 placement workflow remains. This is a single-node/primitive static subset,
not animation or full PBR support. See [native import](NATIVE_ASSET_IMPORT.md).

## November 18 — multiple primitives and material draw ranges

The native importer now retains up to 32 static triangle primitives on one
root glTF mesh node, with independent opaque albedo PNG/JPEG and base-color
factors. An original two-part beacon demonstrates this in actual gameplay.
The original byte-for-byte `RTSMESH1` format remains the output for one
primitive; `RTSMESH2` adds bounded, validated contiguous material/index
ranges with one cached GPU draw per material. F7 lists the material count.
All new C++ CPU loader, glTF/GLB import, binary corruption and installed
Windows/Linux package checks cover the new format. See
`projects/multi_material_showcase.rtsproject.json` and the exact binary
layout in [Native Import](NATIVE_ASSET_IMPORT.md).

**Still missing:** node hierarchies, more advanced glTF materials, metal/rough
maps, normal maps, scene animation/skeletons, streaming and production LOD.
Older sections above document the historic single-primitive milestone;
the native multi-material stage here supersedes those limitations.

## November 25 — static scene assemblies and searchable asset inspection

The native importer now bakes a bounded glTF node forest into the existing
RTSMESH1/2 geometry and material draw ranges. Group nodes, child transforms
and repeated meshes are supported. The original authored scene stays glTF;
compiled files do not retain node names or runtime animation/attachment
transforms. F7 adds asset path search, an invalid-only filter and expandable
model-space bounds and decoded texture byte counts. The headless CLI
regression verifies nested nonuniform scale and 45-degree rotation,
inverse-transpose normals, repeated index ranges, GLB parity, unreachable
nodes and invalid graph rejection. See [Native asset import](NATIVE_ASSET_IMPORT.md)
for the current contract.

## Retained static model nodes

RTSMESH3 extends the multi-material cache with validated parent-first node
records, local matrices, names and per-node surface ranges. Model-space
matrices reconstruct on read. Existing one-node RTSMESH1/2 outputs and
renderer behavior stay compatible. F7 has a read-only model-node inspector;
the headless `--model-info` command exposes JSON and unique-name lookup.
This establishes socket metadata, not skeletal animation or runtime
articulation. See [Native asset import](NATIVE_ASSET_IMPORT.md).

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

## November 25 — rigid animation and live model sockets

The existing RTSMESH3 bind geometry and retained node matrices support
rigid part animation without a binary format change. Actor field
`rigid_clip` is an inherited object with `duration` (0.05..600 seconds),
`loop` (boolean) and up to 16 uniquely named node tracks. Each track
contains 1..64 strictly increasing keyframe times in the clip interval,
with local translation offset, Euler XYZ angles (degrees, explicitly
interpolated even across a full 360 degree rotation) and positive scale.
Missing transform components default to zero offset/rotation and unit
scale. No source glTF animation channels, skeletons or vertex skinning
are imported yet.

A track transforms the original node local matrix by T * Rz * Ry * Rx * S.
Parent-first composition yields the posed model matrix; each owned
surface draws baked vertices through posedModel * inverseBindModel.
Unaffected surfaces use an exact identity delta. Child attachments use
the same posed socket matrices. Inverse bind matrices are precomputed
when the renderer loads its asset; the pose uses bounded stack arrays.
Unresolvable or numerically invalid poses fall back to the bind model.
The project asset audit diagnoses missing/ambiguous animation nodes and
noninvertible bind matrices. Runtime animation is presentation only,
sampled on the global fixed simulation clock (60 ticks/s); all matching
actors share that clock. Nonloop clips clamp, rather than restart.
The simulation does not store or hash pose matrices.

F7 Actor properties contain a typed keyframe editor with node picker,
duration/loop, track and key add/delete, sortable key times, and
transactional Apply. The Model Viewer can choose a matching Actor,
display its attached models, play/pause/rewind and scrub the clip while
selected axes follow the sampled pose. Bounds shown remain bind-pose
bounds. The preview has an independent clock and does not advance the
game. Preview caches at most one parent and its eight child Model assets.
`RTSProjectTool --actor-pose project actor-id seconds` emits JSON with
posed node matrices, surface deltas and mounted child transforms.
`projects/rigid_animation_showcase.rtsproject.json` demonstrates
a rotating, translating, tilting cap assembly with a moving attachment.
The old static showcase is unchanged.

Regression coverage: affine inverse and propagation, interpolation and
loop/clamp, static surface preservation, posed attachment origins,
invalid track/node handling, undo/redo, independent CLI expectations,
same simulation hashes with animation enabled/disabled and replay
verification. CI captures the actual preview and fixed-tick F9 game.
Next major work: imported quaternion TRS channels and multiple named
clips, per-Actor playback/event state and blending, then skinning/rigs.
