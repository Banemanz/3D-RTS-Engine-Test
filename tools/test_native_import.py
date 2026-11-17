#!/usr/bin/env python3
"""End-to-end native compiler contract, independent fixtures and failure rollback."""
import base64
import copy
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

exe = str(Path(sys.argv[1]).resolve())
root = Path(__file__).resolve().parents[1]
fixture = json.loads((root/'assets/source/beacon.gltf').read_text())

def run(source, project, reference, ok=True):
    process = subprocess.run([exe, '--import-gltf', str(source), str(project), reference],
                             capture_output=True, text=True, timeout=15)
    assert (process.returncode == 0) == ok, process.stdout + process.stderr
    if not ok:
        assert not (project.parent / reference).exists(), process.stderr
    return project.parent / reference

with tempfile.TemporaryDirectory() as tmp:
    folder = Path(tmp)
    source = folder/'source.gltf'
    project = folder/'destination'/'map.rtsproject.json'
    def compile_data(data, name='model', ok=True):
        source.write_text(json.dumps(data))
        return run(source, project, f'assets/{name}.rtsmesh', ok)
    result = compile_data(fixture).read_bytes()
    assert result == (root/'projects/assets/beacon.rtsmesh').read_bytes()
    # No overwrite and no stale staging directory, even after a valid compile.
    attempt = subprocess.run([exe, '--import-gltf', str(source), str(project),
                              'assets/model.rtsmesh'], capture_output=True)
    assert attempt.returncode != 0
    assert (project.parent/'assets/model.rtsmesh').read_bytes() == result
    assert not list(project.parent.rglob('*.importing'))

    glb = copy.deepcopy(fixture)
    binary = base64.b64decode(glb['buffers'][0].pop('uri').split(',')[1])
    document = json.dumps(glb).encode()
    document += b' ' * (-len(document) % 4)
    binary += b'\0' * (-len(binary) % 4)
    payload = (b'glTF' + struct.pack('<II', 2, 28+len(document)+len(binary)) +
               struct.pack('<I4s', len(document), b'JSON') + document +
               struct.pack('<I4s', len(binary), b'BIN\0') + binary)
    glb_path = folder/'source.glb'
    glb_path.write_bytes(payload)
    assert run(glb_path, project, 'assets/binary.rtsmesh').read_bytes() == result
    # A self-contained GLB stores its image in a bufferView, not a data URI.
    embedded = copy.deepcopy(glb)
    png = base64.b64decode(embedded['images'][0].pop('uri').split(',')[1])
    embedded['images'][0].update(mimeType='image/png', bufferView=len(embedded['bufferViews']))
    embedded['bufferViews'].append({'buffer':0,'byteOffset':len(binary),'byteLength':len(png)})
    packed = binary + png
    embedded['buffers'][0]['byteLength'] = len(packed)
    packed += b'\0' * (-len(packed)%4)
    js = json.dumps(embedded).encode()
    js += b' ' * (-len(js)%4)
    glb_path.write_bytes(b'glTF' + struct.pack('<II',2,28+len(js)+len(packed)) +
                        struct.pack('<I4s',len(js),b'JSON')+js +
                        struct.pack('<I4s',len(packed),b'BIN\0')+packed)
    assert run(glb_path,project,'assets/embedded_image.rtsmesh').read_bytes() == result
    glb_path.write_bytes(payload[:-3])
    run(glb_path, project, 'assets/corrupt.rtsmesh', False)

    # Real quaternion rotation + nonuniform scale + translation. Verify geometry
    # numerically against independently computed +90deg Y rotation.
    rotated = copy.deepcopy(fixture)
    rotated['nodes'][0].update(rotation=[0,math.sqrt(.5),0,math.sqrt(.5)],
                               scale=[2,3,4], translation=[5,6,7])
    transformed = compile_data(rotated, 'rotated').read_bytes()
    vertex_count = struct.unpack_from('<I', result, 8)[0]
    for i in range(vertex_count):
        a = struct.unpack_from('<8f', result, 40+32*i)
        b = struct.unpack_from('<8f', transformed, 40+32*i)
        expected = (a[2]*4+5,a[1]*3+6,-a[0]*2+7)
        normal = (a[5]/4,a[4]/3,-a[3]/2)
        length = math.sqrt(sum(x*x for x in normal))
        expected += tuple(x/length for x in normal) + a[6:]
        assert all(abs(x-y) < 3e-6 for x,y in zip(expected,b)), (expected,b)

    # Original solid-color JPEG fixture; no imaging library required to run tests.
    jpeg = copy.deepcopy(fixture)
    jpeg['images'][0]['uri'] = 'data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAIBAQEBAQIBAQECAgICAgQDAgICAgUEBAMEBgUGBgYFBgYGBwkIBgcJBwYGCAsICQoKCgoKBggLDAsKDAkKCgr/2wBDAQICAgICAgUDAwUKBwYHCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgr/wAARCAACAAIDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDg6KKK/kM/0UP/2Q=='
    jpeg_result = compile_data(jpeg, 'jpeg').read_bytes()
    assert struct.unpack_from('<II', jpeg_result, 16) == (2,2)
    assert all(abs(x-y)<=3 for x,y in zip(jpeg_result[-16:-12], (210,80,30,255)))
    opaque = copy.deepcopy(fixture)
    opaque['materials'][0]['pbrMetallicRoughness']['baseColorFactor'][3] = 0.2
    assert struct.unpack_from('<f',compile_data(opaque,'opaque').read_bytes(),36)[0] == 1

    # Local resources (including spaces) work, but traversal and symlink escapes fail.
    external = copy.deepcopy(fixture)
    buffer = base64.b64decode(external['buffers'][0]['uri'].split(',')[1])
    (folder/'mesh data.bin').write_bytes(buffer)
    external['buffers'][0]['uri'] = 'mesh data.bin'
    assert compile_data(external, 'external').read_bytes() == result
    external['buffers'][0]['uri'] = '../outside.bin'
    compile_data(external, 'escape', False)
    external['buffers'][0]['uri'] = 'https://example.invalid/mesh.bin'
    compile_data(external, 'network', False)

    mutations = [
        lambda d: d.update(animations=[{'channels':[], 'samplers':[]}]),
        lambda d: d['nodes'].append({'children':[0]}),
        lambda d: d['nodes'][0].update(scale=[0,1,1]),
        lambda d: d['nodes'][0].update(rotation=[0,0,0,0]),
        lambda d: d['nodes'][0].update(scale=[-1,1,1]),
        lambda d: d['bufferViews'][0].update(byteOffset=2**63),
        lambda d: d['accessors'][0].update(count=2**60),
        lambda d: d['accessors'][0].update(byteOffset=2**63),
        lambda d: d['accessors'][3].update(componentType=5126),
        lambda d: d['buffers'][0].update(uri='data:application/octet-stream;base64,A==='),
        lambda d: d['materials'][0].update(alphaMode='MASK'),
        lambda d: d['materials'][0].update(doubleSided=True),
        lambda d: d['materials'][0].update(normalTexture={'index':0}),
        lambda d: d['meshes'][0]['primitives'][0]['attributes'].update(TANGENT=0),
    ]
    for i, mutate in enumerate(mutations):
        bad = copy.deepcopy(fixture)
        mutate(bad)
        compile_data(bad, f'rejected_{i}', False)
    # Invalid destinations must be rejected before file creation.
    source.write_text(json.dumps(fixture))
    for ref in ('../escape.rtsmesh','assets/../escape.rtsmesh','assets/bad name.rtsmesh'):
        run(source, project, ref, False)
    assert not list(project.parent.rglob('*.importing'))
print('Native glTF/GLB: byte parity, transforms, local resources, no overwrite and malformed-input rollback passed')
