"""Small original E1-X assets; Python standard library only, no downloaded content."""
import json
import pathlib
import struct
import zlib


def png():
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    # Asymmetric UV chart: red/green above blue/yellow, alpha cutouts on the right.
    colors = [(225, 70, 35, 255), (50, 180, 85, 0), (35, 90, 220, 255), (240, 190, 45, 128)]
    rows = b''.join(b'\0' + b''.join(bytes(colors[(y // 4) * 2 + x // 4]) for x in range(8)) for y in range(8))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 8, 8, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')


class Asset:
    def __init__(self):
        self.data = bytearray()
        self.doc = {'asset': {'version': '2.0', 'generator': 'ZiiNAN E1-X deterministic original fixture'},
                    'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'name': 'market-stall', 'children': []}],
                    'meshes': [], 'bufferViews': [], 'accessors': [], 'materials': [
                        {'name': 'wood-chart', 'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'baseColorFactor': [1, .8, .65, 1]}},
                        {'name': 'mask-banner', 'alphaMode': 'MASK', 'alphaCutoff': .6, 'doubleSided': True,
                         'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}}},
                        {'name': 'glass', 'alphaMode': 'BLEND', 'doubleSided': True,
                         'pbrMetallicRoughness': {'baseColorFactor': [.1, .65, 1, .45]}},
                        {'name': 'roof', 'pbrMetallicRoughness': {'baseColorFactor': [.7, .16, .08, 1]}}]}
        image = self.view(png())
        self.doc.update(images=[{'bufferView': image, 'mimeType': 'image/png'}], textures=[{'source': 0}])

    def view(self, data, stride=None):
        self.data.extend(b'\0' * (-len(self.data) % 4))
        view = {'buffer': 0, 'byteOffset': len(self.data), 'byteLength': len(data)}
        if stride:
            view['byteStride'] = stride
        self.doc['bufferViews'].append(view)
        self.data.extend(data)
        return len(self.doc['bufferViews']) - 1

    def accessor(self, view, component, count, kind, offset=0):
        self.doc['accessors'].append({'bufferView': view, 'byteOffset': offset, 'componentType': component, 'count': count, 'type': kind})
        return len(self.doc['accessors']) - 1

    def cube(self, material):
        vertices, indices = [], []
        # Counterclockwise when viewed from outside.
        for normal, corners in [((0, 0, 1), [(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]),
                                ((0, 0,-1), [(1,-1,-1),(-1,-1,-1),(-1,1,-1),(1,1,-1)]),
                                ((1,0,0), [(1,-1,1),(1,-1,-1),(1,1,-1),(1,1,1)]),
                                ((-1,0,0), [(-1,-1,-1),(-1,-1,1),(-1,1,1),(-1,1,-1)]),
                                ((0,1,0), [(-1,1,1),(1,1,1),(1,1,-1),(-1,1,-1)]),
                                ((0,-1,0), [(-1,-1,-1),(1,-1,-1),(1,-1,1),(-1,-1,1)])]:
            base = len(vertices)
            for point, uv in zip(corners, [(0,1),(1,1),(1,0),(0,0)]):
                vertices.append(tuple(v / 2 for v in point) + normal + uv)
            indices.extend(base + i for i in [0,1,2,0,2,3])
        view = self.view(b''.join(struct.pack('<8f', *v) for v in vertices), 32)
        attrs = {key: self.accessor(view, 5126, len(vertices), kind, off) for key, kind, off in
                 [('POSITION','VEC3',0),('NORMAL','VEC3',12),('TEXCOORD_0','VEC2',24)]}
        iview = self.view(struct.pack('<' + 'H' * len(indices), *indices))
        self.doc['meshes'].append({'name': 'box', 'primitives': [{'attributes': attrs, 'indices': self.accessor(iview, 5123, len(indices), 'SCALAR'), 'material': material}]})
        return len(self.doc['meshes']) - 1

    def node(self, name, mesh, position, scale):
        self.doc['nodes'][0]['children'].append(len(self.doc['nodes']))
        self.doc['nodes'].append({'name': name, 'mesh': mesh, 'translation': position, 'scale': scale})

    def save(self, path):
        self.doc['buffers'] = [{'byteLength': len(self.data)}]
        j = json.dumps(self.doc, separators=(',', ':')).encode()
        j += b' ' * (-len(j) % 4)
        binary = bytes(self.data) + b'\0' * (-len(self.data) % 4)
        glb = struct.pack('<III', 0x46546c67, 2, 28 + len(j) + len(binary))
        glb += struct.pack('<II', len(j), 0x4e4f534a) + j + struct.pack('<II', len(binary), 0x004e4942) + binary
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(glb)
        print(f'{path.name}: {len(glb)} bytes')


def main():
    out = pathlib.Path(__file__).parent / 'fixtures'
    a = Asset()
    wood, banner, glass, roof = [a.cube(i) for i in range(4)]
    a.node('counter', wood, [0,.7,0], [2.4,.22,1.1])
    for x in [-1,1]:
        for z in [-.45,.45]:
            a.node('post', wood, [x,1.15,z], [.12,2.3,.12])
    a.node('awning', roof, [0,2.3,0], [2.8,.18,1.5])
    a.node('cutout-banner', banner, [0,1.95,.62], [1.5,.45,.025])
    a.node('glass-display', glass, [.5,1.1,.38], [.6,.55,.035])
    a.node('crate', wood, [-.5,.96,0], [.45,.32,.5])
    a.save(out / 'market_stall.glb')
    a.doc['materials'][2]['alphaMode'] = 'OPAQUE'
    a.doc['materials'][2]['pbrMetallicRoughness']['baseColorFactor'][3] = 1
    a.save(out / 'market_stall_static.glb')


if __name__ == '__main__':
    main()
