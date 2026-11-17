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
