#!/usr/bin/env python3
"""Cross-check genuine glTF and GLB import, deterministic caches and rejection."""
import base64
import copy
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
assert result==compiled.read_bytes(),"Rebuilt glTF differs from bundled mesh"
vertices,indices,width,height=struct.unpack_from("<IIII",result,8)
assert (vertices,indices,width,height)==(32,48,2,2)
assert len(result)==40+vertices*32+indices*4+width*height*4

fixture=json.loads(source.read_text(encoding="utf-8"))
def rejected(path,contents):
    path.write_text(json.dumps(contents),encoding="utf-8")
    try:compiler.compile_mesh(path)
    except (compiler.ImportErrorAsset,ValueError,KeyError,TypeError):return
    raise AssertionError("Unsafe or unsupported glTF feature was silently accepted")

with tempfile.TemporaryDirectory() as temporary:
    folder=Path(temporary)
    path=folder/"invalid.gltf"
    broken=copy.deepcopy(fixture)
    broken["animations"]=[{"channels":[]}]
    rejected(path,broken)
    broken=copy.deepcopy(fixture)
    broken["buffers"][0]["uri"]="../outside.bin"
    rejected(path,broken)
    broken=copy.deepcopy(fixture)
    broken["nodes"].append({"children":[0]})
    rejected(path,broken)
    broken=copy.deepcopy(fixture)
    broken["accessors"][fixture["meshes"][0]["primitives"][0]["indices"]]["componentType"]=5126
    rejected(path,broken)
    broken=copy.deepcopy(fixture)
    broken["meshes"][0]["primitives"][0]["attributes"]["TANGENT"]=0
    rejected(path,broken)

    # Production .glb uses a binary buffer chunk rather than a base64 URI.
    glb=copy.deepcopy(fixture)
    uri=glb["buffers"][0].pop("uri")
    binary=base64.b64decode(uri.partition(";base64,")[2])
    json_bytes=json.dumps(glb,separators=(",",":")).encode("utf-8")
    json_bytes+=b" " * ((-len(json_bytes))%4)
    binary+=b"\x00" * ((-len(binary))%4)
    size=12+8+len(json_bytes)+8+len(binary)
    payload=(b"glTF"+struct.pack("<II",2,size)+
             struct.pack("<I4s",len(json_bytes),b"JSON")+json_bytes+
             struct.pack("<I4s",len(binary),b"BIN\x00")+binary)
    glbPath=folder/"beacon.glb"
    glbPath.write_bytes(payload)
    assert compiler.compile_mesh(glbPath)==result,\
        "GLB and glTF geometry/textures must compile bit-identically"
    corrupted=bytearray(payload)
    struct.pack_into("<I",corrupted,8,size+4)
    glbPath.write_bytes(corrupted)
    try:compiler.compile_mesh(glbPath)
    except compiler.ImportErrorAsset:pass
    else:raise AssertionError("Invalid GLB size must fail")

print("glTF + GLB/PNG -> RTSMESH1: binary-equivalent output, "
      "hierarchy/attribute/security/corrupt-container rejects passed")
