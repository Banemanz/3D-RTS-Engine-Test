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

    # Deliberate replacement: valid imports update bytes, invalid imports
    # leave the previous valid asset intact.
    def reimport(source_path,reference,ok=True):
        p=subprocess.run([exe,'--reimport-gltf',str(source_path),str(project),reference],
                         capture_output=True,text=True,timeout=15)
        assert (p.returncode==0)==ok,p.stdout+p.stderr
    changed=copy.deepcopy(fixture)
    changed['nodes'][0]['translation']=[1,0,0]
    source.write_text(json.dumps(changed))
    reimport(source,'assets/model.rtsmesh')
    assert (project.parent/'assets/model.rtsmesh').read_bytes()!=result
    source.write_text('invalid')
    previous=(project.parent/'assets/model.rtsmesh').read_bytes()
    reimport(source,'assets/model.rtsmesh',False)
    assert (project.parent/'assets/model.rtsmesh').read_bytes()==previous
    source.write_text(json.dumps(fixture))
    reimport(source,'assets/model.rtsmesh')
    assert (project.parent/'assets/model.rtsmesh').read_bytes()==result

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


    # Nested transforms: a parent nonuniform scale and rotated child
    # create shear in model space. Independent analytic expectations
    # verify positions, inverse-transpose normals and instance index bases.
    hierarchy=copy.deepcopy(fixture)
    hierarchy['nodes']=[
        {'name':'Assembly','translation':[3,4,5],'scale':[2,3,4],'children':[1,2]},
        {'name':'Rotated body','mesh':0,'translation':[1,2,3],
         'rotation':[0,math.sin(math.pi/8),0,math.cos(math.pi/8)]},
        {'name':'Second instance','mesh':0,'translation':[-2,0,0]}]
    hierarchy['scenes']=[{'nodes':[0]}]
    hierarchy['scene']=0
    nested=compile_data(hierarchy,'hierarchy').read_bytes()
    assert nested[:8]==b'RTSMESH3'
    assert struct.unpack_from('<IIII',nested,8)==(64,96,2,3)
    for j in range(vertex_count):
        a=struct.unpack_from('<8f',result,40+j*32)
        b=struct.unpack_from('<8f',nested,24+j*32)
        c=math.sqrt(.5)
        expected=(3+2*(c*(a[0]+a[2])+1),4+3*(a[1]+2),
                  5+4*(c*(-a[0]+a[2])+3))
        normal=(c*(a[3]+a[5])/2,a[4]/3,c*(-a[3]+a[5])/4)
        length=math.sqrt(sum(x*x for x in normal))
        expected+=tuple(x/length for x in normal)+a[6:]
        assert all(abs(x-y)<2e-5 for x,y in zip(expected,b)),(expected,b)
        second=struct.unpack_from('<8f',nested,24+(vertex_count+j)*32)
        expected2=(3+2*(a[0]-2),4+3*a[1],5+4*a[2])
        assert all(abs(x-y)<2e-5 for x,y in zip(expected2,second[:3]))
    idx=struct.unpack_from('<96I',nested,24+64*32)
    assert all(idx[j]+32==idx[48+j] for j in range(48))
    # Binary GLB follows the identical assembly path.
    hierarchy_glb=copy.deepcopy(hierarchy)
    body=base64.b64decode(hierarchy_glb['buffers'][0].pop('uri').split(',')[1])
    doc=json.dumps(hierarchy_glb).encode()
    doc+=b' '*(-len(doc)%4)
    body+=b'\0'*(-len(body)%4)
    hierarchy_file=folder/'hierarchy.glb'
    hierarchy_file.write_bytes(b'glTF'+struct.pack('<II',2,28+len(doc)+len(body))+
        struct.pack('<I4s',len(doc),b'JSON')+doc+
        struct.pack('<I4s',len(body),b'BIN\0')+body)
    assert run(hierarchy_file,project,'assets/hierarchy_glb.rtsmesh').read_bytes()==nested
    # Original scene fixture separates the two beacon primitives into nodes.
    assembly=run(root/'assets/source/beacon_assembly.gltf',project,
                 'assets/assembly.rtsmesh').read_bytes()
    assert assembly[:8]==b'RTSMESH3'
    assert struct.unpack_from('<IIII',assembly,8)==(64,96,2,4)
    assert struct.unpack_from('<IIII',assembly,24+64*32+96*4)==(0,48,2,2)
    # The actual native loader exposes retained local/model node transforms,
    # independently of the source glTF file and authoritative map loading.
    def info(reference, name=None, ok=True):
        args=[exe,'--model-info',str(project),reference]
        if name is not None:args.append(name)
        p=subprocess.run(args,capture_output=True,text=True,timeout=15)
        assert (p.returncode==0)==ok,p.stdout+p.stderr
        return json.loads(p.stdout) if ok else None
    assert assembly==(root/'projects/assets/beacon_assembly.rtsmesh').read_bytes()
    graph=info('assets/assembly.rtsmesh')
    assert (graph['vertices'],graph['triangles'],graph['surfaces'])==(64,32,2)
    assert [n['name'] for n in graph['nodes']]==[
        'Beacon assembly','Signal body','Cap support','Amber cap']
    assert [n['parent'] for n in graph['nodes']]==[-1,0,0,2]
    assert [n['surfaceCount'] for n in graph['nodes']]==[0,1,0,1]
    assert [n['firstSurface'] for n in graph['nodes']]==[0,0,1,1]
    cap=info('assets/assembly.rtsmesh','Amber cap')['nodes'][0]
    # Preserve old unique socket names during replacement.
    replacement=json.loads((root/'assets/source/beacon_assembly.gltf').read_text())
    replacement['nodes'][3]['name']='Renamed cap'
    source.write_text(json.dumps(replacement))
    reimport(source,'assets/assembly.rtsmesh',False)
    assert (project.parent/'assets/assembly.rtsmesh').read_bytes()==assembly
    replacement['nodes'][3]['name']='Amber cap'
    replacement['nodes'][2]['translation']=[.6,.35,0]
    source.write_text(json.dumps(replacement))
    reimport(source,'assets/assembly.rtsmesh')
    assert abs(info('assets/assembly.rtsmesh','Amber cap')['nodes'][0]['model'][12]-.6)<1e-6
    reimport(root/'assets/source/beacon_assembly.gltf','assets/assembly.rtsmesh')
    assert (project.parent/'assets/assembly.rtsmesh').read_bytes()==assembly
    assert cap['local'][12:15]==[0,0,0]
    assert all(abs(a-b)<1e-6 for a,b in zip(cap['model'][12:15],[.35,.35,0]))
    assert info('assets/model.rtsmesh')['nodes']==[]
    info('assets/assembly.rtsmesh','missing',False)
    duplicate=copy.deepcopy(hierarchy)
    duplicate['nodes'][1]['name']=duplicate['nodes'][2]['name']='Shared'
    compile_data(duplicate,'duplicate_name')
    info('assets/duplicate_name.rtsmesh','Shared',False)
    # Even an assembly with just one geometry primitive retains group nodes.
    grouped=copy.deepcopy(fixture)
    grouped['nodes']=[{'name':'Group','children':[1]},
                      {'name':'Body','mesh':0}]
    grouped['scenes']=[{'nodes':[0]}]
    one=compile_data(grouped,'one_surface').read_bytes()
    assert struct.unpack_from('<8sIIII',one)==(b'RTSMESH3',32,48,1,2)
    assert len(info('assets/one_surface.rtsmesh')['nodes'])==2
    # The geometry/material bytes are identical in layout to v2. Reject
    # corrupt node records via the production runtime reader and CLI.
    off=24+64*32+96*4+32+16+32
    def reject_node(blob):
        bad_path=project.parent/'assets/bad_node.rtsmesh'
        bad_path.write_bytes(blob)
        info('assets/bad_node.rtsmesh',ok=False)
    bad=bytearray(assembly);struct.pack_into('<I',bad,20,129);reject_node(bad)
    bad=bytearray(assembly);struct.pack_into('<I',bad,off,0);reject_node(bad)
    bad=bytearray(assembly);struct.pack_into('<I',bad,off+4,1);reject_node(bad)
    bad=bytearray(assembly);struct.pack_into('<I',bad,off+12,256);reject_node(bad)
    bad=bytearray(assembly);struct.pack_into('<f',bad,off+16,float('nan'));reject_node(bad)
    bad=bytearray(assembly);bad[off+80]=0;reject_node(bad)
    reject_node(assembly[:-1]);reject_node(assembly+b'!')
    # Different source mesh objects, not only instances of a shared mesh.
    separate=copy.deepcopy(hierarchy)
    separate['meshes'].append(copy.deepcopy(separate['meshes'][0]))
    separate['nodes'][2]['mesh']=1
    assert compile_data(separate,'separate').read_bytes()==nested
    # Disconnected nodes, repeated roots, over-budget instances and cycles.
    disconnected=copy.deepcopy(hierarchy)
    disconnected['nodes'].append({'name':'orphan'})
    compile_data(disconnected,'disconnected',False)
    repeated=copy.deepcopy(hierarchy)
    repeated['scenes'][0]['nodes']=[0,0]
    compile_data(repeated,'duplicate_root',False)
    cycle=copy.deepcopy(hierarchy)
    cycle['nodes'][1]['children']=[0]
    compile_data(cycle,'cycle',False)
    oversized=copy.deepcopy(fixture)
    oversized['nodes']=[{'mesh':0} for _ in range(33)]
    oversized['scenes']=[{'nodes':list(range(33))}]
    compile_data(oversized,'too_many_surfaces',False)

    # Original solid-color JPEG fixture; no imaging library required to run tests.
    jpeg = copy.deepcopy(fixture)
    jpeg['images'][0]['uri'] = 'data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAIBAQEBAQIBAQECAgICAgQDAgICAgUEBAMEBgUGBgYFBgYGBwkIBgcJBwYGCAsICQoKCgoKBggLDAsKDAkKCgr/2wBDAQICAgICAgUDAwUKBwYHCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgr/wAARCAACAAIDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDg6KKK/kM/0UP/2Q=='
    jpeg_result = compile_data(jpeg, 'jpeg').read_bytes()
    assert struct.unpack_from('<II', jpeg_result, 16) == (2,2)
    assert all(abs(x-y)<=3 for x,y in zip(jpeg_result[-16:-12], (210,80,30,255)))
    opaque = copy.deepcopy(fixture)
    opaque['materials'][0]['pbrMetallicRoughness']['baseColorFactor'][3] = 0.2
    assert struct.unpack_from('<f',compile_data(opaque,'opaque').read_bytes(),36)[0] == 1


    # Two-primitive source must compile bit-for-bit into the committed V2 asset.
    dual = json.loads((root/'assets/source/beacon_dual.gltf').read_text())
    dual_file=folder/'dual.gltf'
    dual_file.write_text(json.dumps(dual))
    dual_expected=(root/'projects/assets/beacon_dual.rtsmesh').read_bytes()
    dual_result=run(dual_file,project,'assets/dual.rtsmesh').read_bytes()
    assert dual_result==dual_expected, 'Native RTSMESH2 mismatch'
    assert struct.unpack_from('<IIII',dual_result,8)==(64,96,2,0)
    off=24+64*32+96*4
    assert struct.unpack_from('<IIII',dual_result,off)==(0,48,2,2)
    assert struct.unpack_from('<IIII',dual_result,off+48)==(48,48,0,0)
    dual_glb=copy.deepcopy(dual)
    packed=base64.b64decode(dual_glb['buffers'][0].pop('uri').split(',')[1])
    js=json.dumps(dual_glb).encode()
    js+=b' '*(-len(js)%4)
    packed+=b'\0'*(-len(packed)%4)
    blob=(b'glTF'+struct.pack('<II',2,28+len(js)+len(packed))+
          struct.pack('<I4s',len(js),b'JSON')+js+
          struct.pack('<I4s',len(packed),b'BIN\0')+packed)
    dual_glb_file=folder/'dual.glb'
    dual_glb_file.write_bytes(blob)
    assert run(dual_glb_file,project,'assets/dual_glb.rtsmesh').read_bytes()==dual_expected
    invalid=copy.deepcopy(dual)
    invalid['meshes'][0]['primitives'][1]['mode']=1
    dual_file.write_text(json.dumps(invalid))
    run(dual_file,project,'assets/dual_invalid.rtsmesh',False)

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

    # Portable source receipts capture exactly the dependencies the compiler
    # read, including external binary and texture data. No wall-clock reliance.
    import shutil
    sources=project.parent/'sources'
    sources.mkdir()
    tracked=sources/'tracked.gltf'
    tracked_data=copy.deepcopy(fixture)
    (sources/'mesh data.bin').write_bytes(buffer)
    tracked_data['buffers'][0]['uri']='mesh data.bin'
    png_bytes=base64.b64decode(tracked_data['images'][0]['uri'].split(',')[1])
    (sources/'color.png').write_bytes(png_bytes)
    tracked_data['images'][0]['uri']='color.png'
    tracked.write_text(json.dumps(tracked_data))
    reference='assets/tracked.rtsmesh'
    cache=run(tracked,project,reference)
    receipt=Path(str(cache)+'.source.json')
    def command(action,*args,ok=True):
        p=subprocess.run([exe,action,*map(str,args)],capture_output=True,text=True,timeout=15)
        assert (p.returncode==0)==ok,p.stdout+p.stderr
        return p.stdout
    def status(proj=project):
        return json.loads(command('--source-status',proj,reference))
    assert status()['state']=='untracked'
    command('--track-gltf',tracked,project,reference)
    assert status()['state']=='current' and status()['dependencies']==3
    record=json.loads(receipt.read_text())
    assert record['source']=='sources/tracked.gltf'
    assert {d['path'] for d in record['files']}=={
        'sources/tracked.gltf','sources/mesh data.bin','sources/color.png'}
    old_cache=cache.read_bytes();old_receipt=receipt.read_bytes()
    (sources/'color.png').write_bytes(b'broken')
    assert status()['state']=='source_changed' and status()['changed']==['sources/color.png']
    command('--rebuild-gltf',project,reference,ok=False)
    assert cache.read_bytes()==old_cache and receipt.read_bytes()==old_receipt
    (sources/'color.png').unlink()
    assert status()['state']=='source_unavailable'
    (sources/'color.png').write_bytes(png_bytes)
    assert status()['state']=='current'
    tracked_data['nodes'][0]['translation']=[2,0,0]
    tracked.write_text(json.dumps(tracked_data))
    assert status()['state']=='source_changed'
    command('--track-gltf',tracked,project,reference,ok=False)
    assert receipt.read_bytes()==old_receipt
    command('--rebuild-gltf',project,reference)
    assert cache.read_bytes()!=old_cache and status()['state']=='current'
    relocated=folder/'relocated'
    shutil.copytree(project.parent,relocated)
    assert status(relocated/project.name)['state']=='current'
    cache.write_bytes(old_cache)
    assert status()['state']=='cache_changed'
    command('--rebuild-gltf',project,reference,ok=False)
    assert cache.read_bytes()==old_cache
    bad_receipt=json.loads(receipt.read_text())
    bad_receipt['files'][0]['path']='../outside.bin'
    receipt.write_text(json.dumps(bad_receipt))
    assert status()['state']=='invalid_receipt'
    command('--track-gltf',source,project,reference,ok=False)
    # Tracked replacement must preserve the existing unique model sockets.
    linked_assembly=sources/'assembly.gltf'
    linked_assembly.write_bytes((root/'assets/source/beacon_assembly.gltf').read_bytes())
    assembly_ref='assets/tracked_assembly.rtsmesh'
    assembly_cache=run(linked_assembly,project,assembly_ref)
    command('--track-gltf',linked_assembly,project,assembly_ref)
    before=assembly_cache.read_bytes()
    assembly_receipt=Path(str(assembly_cache)+'.source.json')
    before_receipt=assembly_receipt.read_bytes()
    assembly_json=json.loads(linked_assembly.read_text())
    assembly_json['nodes'][3]['name']='Different name'
    linked_assembly.write_text(json.dumps(assembly_json))
    command('--rebuild-gltf',project,assembly_ref,ok=False)
    assert assembly_cache.read_bytes()==before and assembly_receipt.read_bytes()==before_receipt
    assert json.loads(command('--source-status',project,assembly_ref))['state']=='source_changed'
    assert not list(project.parent.rglob('*.importing'))

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
# Native command-line pose export: independently verify the authored 1.5s
# sample, loop wrapping, unchanged body surface, and mounted child position.
showcase=root/'projects/rigid_animation_showcase.rtsproject.json'
def actor_pose(seconds,ok=True):
    process=subprocess.run([exe,'--actor-pose',str(showcase),'dual_beacon_actor',str(seconds)],
                           capture_output=True,text=True,timeout=15)
    assert (process.returncode==0)==ok,process.stdout+process.stderr
    return json.loads(process.stdout) if ok else None
pose=actor_pose(1.5)
assert abs(pose['time']-1.5)<1e-6
assert [n['name'] for n in pose['nodes']]==['Beacon assembly','Signal body','Cap support','Amber cap']
support=pose['nodes'][2]['model']
assert all(abs(a-b)<1e-5 for a,b in zip(support[12:15],[.625,.95,0])),support
assert pose['surfaceDeltas'][0]==[1 if i%5==0 else 0 for i in range(16)]
assert pose['attachments'][0]['socket']=='Amber cap'
cap=pose['nodes'][3]['model']
# Local attachment offset is exactly [0,1,0], so child origin equals
# the cap's transformed Y basis plus its translation.
assert all(abs(pose['attachments'][0]['transform'][12+i]-(cap[12+i]+cap[4+i]))<1e-5 for i in range(3))
assert actor_pose(5.5)==pose
actor_pose('nan',False)
actor_pose('1.5junk',False)
print('Native glTF/GLB: byte parity, transforms, local resources, no overwrite and malformed-input rollback passed')
