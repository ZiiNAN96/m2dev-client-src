"""Compare compiled assets with the independent SDK probe, without loading the SDK."""
import argparse
import csv
import json
import math
import struct
from pathlib import Path


def check(value, message):
    if not value:
        raise ValueError(message)


def close(actual, expected, tolerance=0.04):
    return len(actual) == len(expected) and all(abs(a-b) <= tolerance for a, b in zip(actual, expected))


class GLB:
    def __init__(self, path):
        data = path.read_bytes()
        magic, version, length, size, kind = struct.unpack_from('<5I', data)
        check((magic, version, length, kind) == (0x46546c67, 2, len(data), 0x4e4f534a), 'GLB header')
        self.json = json.loads(data[20:20+size])
        binary_size, binary_kind = struct.unpack_from('<2I', data, 20+size)
        check(binary_kind == 0x004e4942, 'GLB BIN chunk')
        self.binary = data[28+size:28+size+binary_size]

    def accessor(self, index):
        item = self.json['accessors'][index]
        view = self.json['bufferViews'][item['bufferView']]
        width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[item['type']]
        char, size = {5126: ('f', 4), 5125: ('I', 4), 5123: ('H', 2), 5121: ('B', 1)}[item['componentType']]
        offset = view.get('byteOffset', 0)+item.get('byteOffset', 0)
        stride = view.get('byteStride', width*size)
        return [struct.unpack_from('<'+char*width, self.binary, offset+i*stride) for i in range(item['count'])]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--audit', type=Path, required=True)
    parser.add_argument('--compiled', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    corpus = json.loads(args.audit.read_text(encoding='utf8'))
    registry = json.loads((args.compiled/'vegetation/registry.json').read_text())['entries']
    results = []
    for row in corpus['rows']:
        key, reference = row['legacy'], row['reference']
        try:
            check(row['packed'] and row['packed']['identical'], 'source differs from effective packed SPT')
            check(key in registry, 'registry missing')
            metadata = json.loads((args.compiled/registry[key]).read_text())
            glb = GLB(args.compiled/metadata['geometry'])
            expected = []
            for kind, field in enumerate(('branchLODs', 'frondLODs')):
                for lod, entry in enumerate(reference[field]):
                    g = entry['geometry']
                    if g['triangles']:
                        expected.append((kind, lod, g['vertices'], g['triangles']*3, g))
            for lod, g in enumerate(reference['leafLODs']):
                if g['count']:
                    expected.append((2, lod, g['count']*4, g['count']*6, g))
            expected.append((3, 0, 4, 6, {}))
            check(len(expected) == len(metadata['parts']) == len(glb.json['meshes']), 'mesh count')
            check(close(metadata['bounds'], reference['bounds'], 0.001), 'reference bounds')
            textures = {image['uri'] for image in glb.json['images']}
            check(all(t['available'] and (t['kind'] == 'selfShadow' or t['path'] in textures) for t in row['textures']), 'texture references')
            check(metadata['shadowTexture'] == next((t['path'] for t in row['textures'] if t['kind'] == 'selfShadow'), ''), 'shadow reference')
            for index, (kind, lod, vertices, indices, golden) in enumerate(expected):
                check(metadata['parts'][index] == [kind, lod, index], 'part/LOD mapping')
                primitive = glb.json['meshes'][index]['primitives'][0]
                attributes = primitive['attributes']
                check(set(('POSITION', 'NORMAL', 'TEXCOORD_0', 'COLOR_0', 'TEXCOORD_1', '_ZIINAN_PIVOT', '_ZIINAN_FLEXIBILITY', '_ZIINAN_CARD_PITCH_COS', '_ZIINAN_CARD_PITCH_SIN')) <= attributes.keys(), 'vertex channels')
                position = glb.accessor(attributes['POSITION'])
                check(len(position) == vertices and len(glb.accessor(primitive['indices'])) == indices, 'vertices/indices')
                check(all(all(math.isfinite(x) for x in v) and abs(sum(x*x for x in v)-1) < 1e-4 for v in glb.accessor(attributes['NORMAL'])), 'finite unit normals')
                client_position = [(x*100, -z*100, y*100) for x, y, z in position]
                uv = glb.accessor(attributes['TEXCOORD_0'])
                colors = glb.accessor(attributes['COLOR_0'])
                if kind < 2:
                    for selected in golden['selectedVertices']:
                        v = selected['index']
                        check(close(client_position[v], selected['position']), 'selected indexed position')
                        check(close(uv[v], selected['uv'], 1e-6), 'selected indexed UV')
                        c = selected['color']
                        check(close(colors[v], [(c>>16&255)/255, (c>>8&255)/255, (c&255)/255, (c>>24&255)/255], 1e-6), 'baked color')
                elif kind == 2:
                    for selected in golden['selectedLeaves']:
                        for corner in range(4):
                            v = selected['index']*4+corner
                            expected_position = [selected['center'][k]+selected['offsets'][corner*4+k] for k in range(3)]
                            check(close(client_position[v], expected_position), 'selected leaf position/LOD size')
                            check(close(uv[v], selected['uv'][corner*2:corner*2+2], 1e-6), 'selected leaf UV')
                material = glb.json['materials'][primitive['material']]
                check(material['alphaMode'] == 'MASK' and bool(material.get('doubleSided')) == (kind != 0), 'material alpha/cull')
            for sample in reference['samples']:
                state = metadata['lods'][round(sample['lod']*256)]
                for slot, field in ((0, 'branch'), (1, 'frond'), (2, 'leaf')):
                    if state[slot] >= 0:
                        check(metadata['parts'][state[slot]][1] == sample[field], 'sampled discrete LOD')
                check((state[4] >= 0) == sample['billboard0'], 'billboard activation')
            results.append({'asset': key, 'production': row['productionReferenced'], 'instances': row['instances'], 'meshes': len(expected), 'status': 'PASS'})
        except (ValueError, KeyError, OSError, IndexError) as error:
            results.append({'asset': key, 'production': row['productionReferenced'], 'instances': row['instances'], 'status': 'FAIL', 'error': str(error)})
    failed = [r for r in results if r['status'] != 'PASS']
    summary = {'totalSPT': len(results), 'verified': len(results)-len(failed), 'failed': len(failed),
               'productionReferenced': corpus['productionReferenced'], 'productionReferencedUnsupported': sum(r['production'] for r in failed)+len(corpus['missingProductionSPT']),
               'productionInstances': corpus['productionInstances'], 'registryEntries': len(registry), 'rows': results}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, indent=2)+'\n', encoding='utf8')
    with args.output.with_suffix('.csv').open('w', newline='', encoding='utf8') as file:
        writer = csv.DictWriter(file, fieldnames=('asset', 'production', 'instances', 'meshes', 'status', 'error'))
        writer.writeheader(); writer.writerows(results)
    print(json.dumps({k: v for k, v in summary.items() if k != 'rows'}))
    for row in failed:
        print(row['asset'], row['error'])
    raise SystemExit(bool(failed) or summary['productionReferencedUnsupported'] != 0 or len(registry) != len(results))


if __name__ == '__main__':
    main()
