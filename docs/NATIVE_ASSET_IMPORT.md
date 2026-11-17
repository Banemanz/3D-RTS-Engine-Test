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

- glTF 2.0 JSON and binary GLB, one root mesh node and one triangle primitive.
- POSITION and NORMAL float32 attributes; optional UV0 (float or normalized
  unsigned byte/short). Indexed unsigned byte/short/int or nonindexed triangles.
- Translation, normalized quaternion rotation and positive nonuniform scale.
  Positions are baked; normals use the inverse transpose then normalization.
- Base color tint and optional PNG/JPEG image, embedded URI, GLB bufferView
  or relative external file. One opaque single-sided material, repeat wrapping
  and linear filtering. glTF OPAQUE ignores image/factor alpha.
- Same versioned RTSMESH1 runtime format and existing cached GL3.3 renderer.
  Native compilation of the original beacon is byte-identical to the Python
  compiler's bundled result.

**Not full glTF or PBR rendering.** Lighting is still the engine's simple
ambient/diffuse preview; scalar metallic/roughness values have no effect.
Normal/occlusion/emissive/metallic-roughness maps, nondefault samplers,
material extensions, alpha blending/cutout, double-sided rendering, multiple
primitives/materials, hierarchy, matrices, mirrored/singular scales, skins,
animations, morphs, sparse accessors and extra vertex attributes are rejected.
The Python converter remains available with its older, narrower transform
and image contract; native import is now the preferred creator workflow.

## Validation and publication

Parser allocations are capped at 128 MiB cumulatively.
Input/resource files are limited to 64 MiB, aggregate declared buffers to
64 MiB, 32 buffers, 256 views/accessors, 250,000 vertices, 750,000 indices and
2048×2048 decoded images. Validate buffer and accessor arithmetic before
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

Next: multi-primitive/material runtime resource format, node hierarchy, preview
turntable/bounds, source manifest/content hash/reimport, background import jobs,
PBR/sRGB/alpha rendering, then skeletons/clips and actor animation events.
