#!/usr/bin/env python3
"""Original glTF fixture must compile bit-identically with no dependencies."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("rts_import",root/"tools/import_gltf.py")
compiler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler)
source=root/"assets/source/beacon.gltf"
compiled=root/"projects/assets/beacon.rtsmesh"
result=compiler.compile_mesh(source)
assert result==compiled.read_bytes(),"Rebuilt glTF asset differs from bundled cache"
vertices,indices,width,height=struct.unpack_from("<IIII",result,8)
assert (vertices,indices,width,height)==(32,48,2,2)
assert len(result)==40+vertices*32+indices*4+width*height*4

broken=json.loads(source.read_text(encoding="utf-8"))
broken["animations"]=[{"channels":[]}]
with tempfile.TemporaryDirectory() as temporary:
    path=Path(temporary)/"bad.gltf"
    path.write_text(json.dumps(broken),encoding="utf-8")
    try:compiler.compile_mesh(path)
    except compiler.ImportErrorAsset:pass
    else:raise AssertionError("Animation must be explicitly rejected")
    broken.pop("animations")
    broken["buffers"][0]["uri"]="../unsafe.bin"
    path.write_text(json.dumps(broken),encoding="utf-8")
    try:compiler.compile_mesh(path)
    except compiler.ImportErrorAsset:pass
    else:raise AssertionError("External path traversal must be rejected")
print("glTF/PNG -> RTSMESH1 reproducibility and untrusted-content checks passed")
