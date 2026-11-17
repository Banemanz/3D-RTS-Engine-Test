#!/usr/bin/env python3
"""Bounded glTF 2.0 / GLB -> original RTSMESH1 static asset compiler.

Only the explicitly supported subset is accepted. Unsupported skinning,
animation, morphs, multiple primitives/materials and non-PNG images FAIL with
an actionable error rather than silently discarding important asset data.
No third-party Python packages or external network downloads are required.
"""
import argparse
import base64
import json
import math
from pathlib import Path
import struct
import sys
import zlib

MAGIC = b"RTSMESH1"
MAX_INPUT = 64 * 1024 * 1024
MAX_VERTICES = 250000
MAX_INDICES = 750000
MAX_TEX_DIM = 2048
COMPONENTS = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
COMPONENT_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


class ImportErrorAsset(ValueError):
    pass


def require(ok, description):
    if not ok:
        raise ImportErrorAsset(description)


def safe_resource(uri, source):
    if uri.startswith("data:"):
        require(";base64," in uri, "Only base64 glTF data URIs are supported")
        raw = uri.partition(";base64,")[2]
        require(len(raw) < MAX_INPUT * 2, "Embedded resource too large")
        try:
            return base64.b64decode(raw, validate=True)
        except (ValueError, base64.binascii.Error) as exc:
            raise ImportErrorAsset("Invalid base64 resource") from exc
    require("://" not in uri and not uri.startswith(("/", "\\")) and
            "%" not in uri and ":" not in uri and "\\" not in uri,
            "Unsafe or unsupported external URI")
    root = source.parent.resolve()
    resource = (root / uri).resolve()
    require(resource != root and root in resource.parents,
            "External resource escapes the model directory")
    require(resource.is_file() and resource.stat().st_size <= MAX_INPUT,
            "Missing or oversized model resource: " + uri)
    return resource.read_bytes()


def load_document(source):
    require(source.is_file() and source.stat().st_size <= MAX_INPUT,
            "Missing or oversized source")
    raw = source.read_bytes()
    glb_bin = None
    if source.suffix.lower() == ".glb":
        require(len(raw) >= 20 and raw[:4] == b"glTF",
                "Invalid GLB signature")
        version, length = struct.unpack_from("<II", raw, 4)
        require(version == 2 and length == len(raw), "GLB must be version 2")
        offset, chunk_json = 12, None
        while offset < len(raw):
            require(offset + 8 <= len(raw), "Truncated GLB chunk")
            size, tag = struct.unpack_from("<I4s", raw, offset)
            offset += 8
            require(size % 4 == 0 and offset + size <= len(raw),
                    "Invalid GLB chunk bounds or alignment")
            chunk = raw[offset:offset + size]
            offset += size
            if tag == b"JSON":
                require(chunk_json is None, "Duplicate GLB JSON")
                chunk_json = chunk
            elif tag == b"BIN\x00":
                require(glb_bin is None, "Duplicate GLB binary chunk")
                glb_bin = chunk
        require(chunk_json is not None, "GLB has no JSON chunk")
        raw = chunk_json.rstrip(b" \x00")
    else:
        require(source.suffix.lower() == ".gltf", "Use .gltf or .glb")
    try:
        data = json.loads(raw.decode("utf-8"))
    except (ValueError, UnicodeDecodeError) as exc:
        raise ImportErrorAsset("Invalid glTF JSON") from exc
    require(data.get("asset", {}).get("version", "").startswith("2."),
            "Only glTF 2.0 is supported")
    require(not data.get("extensionsRequired"),
            "Required glTF extensions need a supported pre-conversion")
    require(not data.get("skins") and not data.get("animations"),
            "Skins and animation clips are not supported in static v1")
    buffers = []
    for entry in data.get("buffers", []):
        payload = (glb_bin if "uri" not in entry
                   else safe_resource(entry["uri"], source))
        require(payload is not None and len(payload) >= entry["byteLength"],
                "Missing/truncated glTF buffer")
        buffers.append(payload[:entry["byteLength"]])
    return data, buffers


def read_accessor(data, buffers, index, expected_type):
    accessors = data.get("accessors", [])
    require(type(index) is int and 0 <= index < len(accessors),
            "Invalid accessor reference")
    a = accessors[index]
    require("sparse" not in a, "Sparse accessors must be flattened before import")
    require(a.get("type") == expected_type, "Accessor type mismatch")
    count = a.get("count")
    require(type(count) is int and 0 < count <= MAX_INDICES, "Accessor count exceeds limits")
    component = a.get("componentType")
    require(component in COMPONENTS, "Unsupported accessor component type")
    char, width = COMPONENTS[component]
    count_parts = COMPONENT_COUNT[expected_type]
    views = data.get("bufferViews", [])
    view_index = a.get("bufferView")
    require(type(view_index) is int and 0 <= view_index < len(views),
            "Accessor needs a valid bufferView")
    view = views[view_index]
    buffer_index = view.get("buffer")
    require(type(buffer_index) is int and 0 <= buffer_index < len(buffers),
            "Invalid bufferView buffer")
    payload = buffers[buffer_index]
    view_start = view.get("byteOffset", 0)
    view_length = view.get("byteLength", 0)
    a_offset = a.get("byteOffset", 0)
    stride = view.get("byteStride", width * count_parts)
    item_size = width * count_parts
    require(all(type(n) is int and n >= 0 for n in
                (view_start, view_length, a_offset, stride)),
            "Negative or noninteger buffer offsets")
    require(item_size <= stride <= 252 and stride % width == 0,
            "Unsupported glTF accessor stride")
    require(view_start + view_length <= len(payload) and
            a_offset + (count - 1) * stride + item_size <= view_length,
            "Accessor reads outside bufferView")
    base = view_start + a_offset
    fmt = "<" + char * count_parts
    result = [struct.unpack_from(fmt, payload, base + i * stride)
              for i in range(count)]
    if a.get("normalized", False) and component != 5126:
        divisor = {5121: 255.0, 5123: 65535.0, 5125: 4294967295.0}[component]
        result = [tuple(float(v) / divisor for v in row) for row in result]
    return result


def decode_png(raw):
    require(raw.startswith(b"\x89PNG\r\n\x1a\n"), "Only PNG base-color images are supported")
    i, ihdr, idat = 8, None, bytearray()
    ended = False
    while i < len(raw):
        require(i + 12 <= len(raw), "Truncated PNG chunk")
        length = struct.unpack_from(">I", raw, i)[0]
        tag = raw[i + 4:i + 8]
        end = i + 12 + length
        require(length <= MAX_INPUT and end <= len(raw), "PNG chunk exceeds bounds")
        payload = raw[i + 8:i + 8 + length]
        crc = struct.unpack_from(">I", raw, i + 8 + length)[0]
        require(zlib.crc32(tag + payload) & 0xffffffff == crc,
                "PNG CRC check failed")
        if tag == b"IHDR":
            require(ihdr is None and length == 13, "Invalid PNG IHDR")
            ihdr = struct.unpack(">IIBBBBB", payload)
        elif tag == b"IDAT":
            require(len(idat) + length <= MAX_INPUT, "PNG image data too large")
            idat.extend(payload)
        elif tag == b"IEND":
            ended = True
            break
        i = end
    require(ended and ihdr is not None, "Missing PNG IHDR or IEND")
    w, h, depth, mode, compression, filtering, interlace = ihdr
    require(0 < w <= MAX_TEX_DIM and 0 < h <= MAX_TEX_DIM,
            "PNG dimensions exceed static importer limits")
    require(depth == 8 and mode in (2, 6) and
            compression == filtering == interlace == 0,
            "Only 8-bit noninterlaced RGB/RGBA PNG is supported")
    channels = 3 if mode == 2 else 4
    stride = w * channels
    expected = h * (stride + 1)
    decompressor = zlib.decompressobj()
    unpacked = decompressor.decompress(bytes(idat), expected + 1)
    require(len(unpacked) == expected and not decompressor.unconsumed_tail and
            decompressor.eof, "PNG decompression size mismatch")
    rgba, previous, offset = bytearray(), bytearray(stride), 0
    for _ in range(h):
        filter_type = unpacked[offset]
        row = bytearray(unpacked[offset + 1:offset + 1 + stride])
        offset += stride + 1
        require(filter_type in (0, 1, 2, 3, 4), "Unsupported PNG row filter")
        for j in range(stride):
            left = row[j - channels] if j >= channels else 0
            above = previous[j]
            upper_left = previous[j - channels] if j >= channels else 0
            if filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                p = left + above - upper_left
                scores = (abs(p-left), abs(p-above), abs(p-upper_left))
                predictor = (left, above, upper_left)[scores.index(min(scores))]
            else:
                predictor = 0
            row[j] = (row[j] + predictor) & 255
        for x in range(w):
            rgba.extend(row[x*channels:x*channels+3])
            rgba.append(row[x*channels+3] if channels == 4 else 255)
        previous = row
    return w, h, bytes(rgba)


def extract_image(data, buffers, source, image_index):
    images = data.get("images", [])
    require(type(image_index) is int and 0 <= image_index < len(images),
            "Invalid material image index")
    image = images[image_index]
    if "uri" in image:
        return safe_resource(image["uri"], source)
    require(image.get("mimeType") == "image/png",
            "Embedded image must be PNG")
    views = data.get("bufferViews", [])
    at = image.get("bufferView")
    require(type(at) is int and 0 <= at < len(views),
            "Invalid image bufferView")
    view = views[at]
    idx = view.get("buffer")
    offset, length = view.get("byteOffset", 0), view.get("byteLength", 0)
    require(type(idx) is int and 0 <= idx < len(buffers) and
            type(offset) is int and type(length) is int and
            offset >= 0 and length > 0 and offset + length <= len(buffers[idx]),
            "Invalid embedded image bounds")
    return buffers[idx][offset:offset+length]


def compile_mesh(source):
    data, buffers = load_document(source)
    meshes = data.get("meshes", [])
    require(len(meshes) == 1 and len(meshes[0].get("primitives", [])) == 1,
            "Static v1 accepts one mesh with one triangle primitive")
    primitive = meshes[0]["primitives"][0]
    require(primitive.get("mode", 4) == 4 and not primitive.get("targets"),
            "Static v1 supports triangles only, no morph targets")
    attrs = primitive.get("attributes", {})
    require("POSITION" in attrs, "Triangle primitive needs POSITION")
    unsupported = set(attrs) - {"POSITION", "NORMAL", "TEXCOORD_0"}
    require(not unsupported,
            "Bake/remove unsupported vertex attributes: " + ", ".join(sorted(unsupported)))
    accessors = data.get("accessors", [])
    require(accessors[attrs["POSITION"]].get("componentType") == 5126,
            "POSITION must be glTF float32")
    if "NORMAL" in attrs:
        require(accessors[attrs["NORMAL"]].get("componentType") == 5126,
                "NORMAL must be glTF float32")
    if "indices" in primitive:
        require(accessors[primitive["indices"]].get("componentType") in (5121, 5123, 5125),
                "glTF triangle indices must be unsigned integers")
    positions = read_accessor(data, buffers, attrs["POSITION"], "VEC3")
    require(len(positions) <= MAX_VERTICES, "Too many mesh vertices")
    normals = (read_accessor(data, buffers, attrs["NORMAL"], "VEC3")
               if "NORMAL" in attrs else [(0.0, 1.0, 0.0)] * len(positions))
    uvs = (read_accessor(data, buffers, attrs["TEXCOORD_0"], "VEC2")
           if "TEXCOORD_0" in attrs else [(0.0, 0.0)] * len(positions))
    require(len(normals) == len(positions) and len(uvs) == len(positions),
            "Mismatched vertex attribute lengths")
    indices = (read_accessor(data, buffers, primitive["indices"], "SCALAR")
               if "indices" in primitive else [(i,) for i in range(len(positions))])
    indices = [int(row[0]) for row in indices]
    require(len(indices) <= MAX_INDICES and len(indices) % 3 == 0 and
            all(0 <= idx < len(positions) for idx in indices),
            "Triangle indices are malformed or out of range")
    nodes = data.get("nodes", [])
    require(len(nodes) <= 1,
            "Static v1 requires a single root mesh node; bake node hierarchies")
    require(not nodes or nodes[0].get("mesh") == 0,
            "Static v1 cannot ignore a scene node that omits the mesh")
    node = nodes[0] if nodes else {}
    require(not node.get("skin") and not node.get("children"),
            "Static v1 cannot bake skin or child hierarchy")
    require("matrix" not in node and
            node.get("rotation", [0, 0, 0, 1]) == [0, 0, 0, 1],
            "Bake node rotations/matrices before static v1 import")
    offset = node.get("translation", [0, 0, 0])
    scale = node.get("scale", [1, 1, 1])
    require(len(offset) == len(scale) == 3 and
            all(isinstance(n, (int, float)) and math.isfinite(n) for n in offset+scale) and
            all(n > 0 for n in scale), "Invalid node translation or scale")
    factor = (1.0, 1.0, 1.0, 1.0)
    tex_w = tex_h = 0
    tex_rgba = b""
    if "material" in primitive:
        mats = data.get("materials", [])
        idx = primitive["material"]
        require(type(idx) is int and 0 <= idx < len(mats),
                "Invalid material index")
        material = mats[idx]
        require(material.get("alphaMode", "OPAQUE") in ("OPAQUE", "BLEND"),
                "Alpha MASK requires a later material pipeline")
        pbr = material.get("pbrMetallicRoughness", {})
        factor = tuple(pbr.get("baseColorFactor", [1, 1, 1, 1]))
        require(len(factor) == 4 and all(isinstance(n, (int, float)) and
                math.isfinite(n) and 0 <= n <= 1 for n in factor),
                "Invalid base color factor")
        if "baseColorTexture" in pbr:
            tx = pbr["baseColorTexture"]
            require(tx.get("texCoord", 0) == 0 and "TEXCOORD_0" in attrs,
                    "Texture requires TEXCOORD_0")
            tex_id = tx.get("index")
            textures = data.get("textures", [])
            require(type(tex_id) is int and 0 <= tex_id < len(textures) and
                    "source" in textures[tex_id],
                    "Invalid baseColorTexture")
            tex_w, tex_h, tex_rgba = decode_png(
                extract_image(data, buffers, source, textures[tex_id]["source"]))
    vertices = []
    for position, normal, uv in zip(positions, normals, uvs):
        p = tuple(position[k] * scale[k] + offset[k] for k in range(3))
        n = [normal[k] / scale[k] for k in range(3)]
        length = math.sqrt(sum(v*v for v in n))
        n = tuple(v/length for v in n) if length > 1e-9 else (0, 1, 0)
        v = p + n + tuple(uv)
        require(len(v) == 8 and all(math.isfinite(n) and abs(n) < 1e7 for n in v),
                "Invalid or oversized vertex attribute")
        vertices.append(v)
    header = struct.pack("<8sIIII4f", MAGIC, len(vertices), len(indices),
                         tex_w, tex_h, *factor)
    return (header + b"".join(struct.pack("<8f", *v) for v in vertices) +
            struct.pack("<" + "I"*len(indices), *indices) + tex_rgba)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Input .gltf or .glb")
    parser.add_argument("output", type=Path, help="Output .rtsmesh")
    args = parser.parse_args()
    try:
        binary = compile_mesh(args.source)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(binary)
        print(f"RTSMESH1: {args.source} -> {args.output} ({len(binary)} bytes)")
    except (ImportErrorAsset, OSError, ValueError, KeyError, IndexError,
            TypeError, struct.error, zlib.error) as exc:
        print(f"Asset import rejected: {exc}", file=sys.stderr)
        raise SystemExit(2)


if __name__ == "__main__":
    main()
