"""Rank actual pack-selected terrain/environment references, without replacing assets.

Input: VegetationPackAudit --texture-impact JSON and the local production
vegetation registry. Outputs retain per-texture evidence and unresolved links.
Map counts mean distinct map roots with tile usage or placed property instances;
they are not screen coverage, play time, or a count of all texture-set declarations.
"""
import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct


def norm(value):
    return value.replace('\\', '/').lower().strip('" ')


def legacy_strings(value):
    if isinstance(value, str):
        if any(0xdc80 <= ord(c) <= 0xdcff for c in value):
            return value.encode('utf-8', errors='surrogateescape').decode('cp949', errors='replace')
        return value
    if isinstance(value, list):
        return [legacy_strings(item) for item in value]
    if isinstance(value, dict):
        return {legacy_strings(key): legacy_strings(item) for key, item in value.items()}
    return value


def category(path):
    for label, words in (
        ('Grass', ('grass',)), ('Cliff', ('cliff',)),
        ('Path', ('road', 'path',)), ('Bark', ('bark',)),
        ('Leaves', ('leaf', 'leaves', 'frond')),
        ('Wood', ('wood', 'plank',)), ('Stone', ('stone', 'pavement', 'tile')),
        ('Rock', ('rock',)), ('Dirt', ('dirt', 'field', 'soil', 'sand')),
    ):
        if any(word in path for word in words):
            return label
    return 'Unclassified'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pack_audit', type=Path)
    parser.add_argument('vegetation_root', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    # Legacy map/property comments may be CP949 inside the tool's byte-preserved
    # JSON strings. Reference tokens are ASCII; retain an explicit decode count.
    decoded = args.pack_audit.read_text(encoding='utf-8', errors='surrogateescape')
    packed = legacy_strings(json.loads(decoded))
    files = {norm(row['path']): row for row in packed['files']}
    available = set(map(norm, packed['availablePaths']))
    rows = {}
    missing = []

    def record(texture, asset, maps=(), placements=0, texels=0, hint=None):
        texture = norm(texture)
        row = rows.setdefault(texture, dict(texture=texture, categories=set(),
            assets=set(), maps=set(), placements=0, tile_samples=0, evidence=set()))
        row['assets'].add(asset)
        row['maps'].update(maps)
        row['placements'] += placements
        row['tile_samples'] += texels
        row['categories'].add(hint or category(texture))
        # These two source images were inspected: authored stone paving and
        # irregular paving, also visible on the native A1 square/paths.
        if texture in ('d:/ymir work/terrainmaps/b/tile/tile01.dds',
                       'd:/ymir work/terrainmaps/b/tile/tile02.dds'):
            row['categories'].add('Path')
        row['evidence'].add(asset)

    # Terrain indices are the authored one-byte splat indices, including the
    # duplicated tile border. Counting a tile as an asset reference avoids
    # inflating the reference count with millions of individual texels.
    mapsets = {}
    for key, row in files.items():
        if not key.endswith('/setting.txt'):
            continue
        match = re.search(r'(?im)^\s*TextureSet\s+([^\r\n]+)', row.get('text', ''))
        if match:
            setname = norm(match[1])
            # Match the current client's find_first_of("textureset", 0) rule,
            # including its legacy character-set rather than prefix semantics.
            if not setname or setname[0] not in 'textureset':
                setname = 'textureset/' + setname
            mapsets[key.rsplit('/', 1)[0]] = setname
    for key, row in files.items():
        if 'tileCounts' not in row:
            continue
        mapname = key.rsplit('/', 2)[0]
        setname = mapsets.get(mapname)
        if setname not in files:
            missing.append(dict(input=key, reason='missing texture set', target=setname))
            continue
        entries = dict((int(index), norm(texture)) for index, texture in re.findall(
            r'(?is)Start\s+Texture(\d+)\s+"([^"]+)"', files[setname].get('text', '')))
        for index, count in enumerate(row['tileCounts']):
            if not count or index == 0:
                continue
            if index not in entries:
                missing.append(dict(input=key, reason='missing splat index', target=index))
                continue
            record(entries[index], key, (mapname,), texels=count)

    # Map instance -> property CRC -> GR2 / compiled native vegetation.
    properties = {}
    for key, row in files.items():
        if not key.endswith(('.prb', '.prt')):
            continue
        text = row.get('text', '')
        target = re.search(r'(?im)^\s*(?:buildingfile|treefile)\s+"([^"]+)"', text)
        lines = text.splitlines()
        if target and len(lines) > 1:
            try:
                properties[int(lines[1].strip())] = (norm(target[1]), key)
            except ValueError:
                missing.append(dict(input=key, reason='invalid property CRC'))
    modelmaps, counts = defaultdict(set), Counter()
    unresolved_properties = Counter()
    for key, row in files.items():
        if not key.endswith(('areadata.txt', 'objectdata.txt')):
            continue
        mapname = key.rsplit('/', 2)[0]
        for block in re.findall(r'(?is)Start\s+Object\d+\s*(.*?)\s*End\s+Object', row.get('text', '')):
            lines = [line.strip() for line in block.splitlines() if line.strip()]
            if len(lines) < 2:
                continue
            try:
                crc = int(lines[1])
            except ValueError:
                continue
            if crc in properties:
                target, prop = properties[crc]
                modelmaps[target].add(mapname)
                counts[target] += 1
            else:
                unresolved_properties[(mapname, crc)] += 1

    parse_errors = []
    for key, row in files.items():
        if not key.endswith('.gr2') or key not in modelmaps:
            continue
        if 'parseError' in row:
            parse_errors.append(dict(asset=key, error=row['parseError'], maps=sorted(modelmaps[key])))
        for texture in set(row.get('textures', [])):
            if not texture.startswith('d:/'):
                texture = str(PurePosixPath(key).parent / texture)
            record(texture, key, modelmaps[key], counts[key])

    registry = json.loads((args.vegetation_root / 'vegetation/registry.json').read_text())['entries']
    for legacy, compiled in sorted(registry.items()):
        legacy = norm(legacy)
        if legacy not in modelmaps:
            continue
        metadata = json.loads((args.vegetation_root / compiled).read_text())
        binary = (args.vegetation_root / metadata['geometry']).read_bytes()
        magic, version, length, chunk_size, chunk_kind = struct.unpack_from('<5I', binary)
        if (magic, version, chunk_kind) != (0x46546c67, 2, 0x4e4f534a) or length != len(binary):
            raise ValueError('Invalid local vegetation GLB: ' + compiled)
        gltf = json.loads(binary[20:20 + chunk_size])
        referenced = defaultdict(set)
        for kind, lod, mesh_index in metadata['parts']:
            for primitive in gltf['meshes'][mesh_index]['primitives']:
                material = gltf['materials'][primitive['material']]
                # The native converter keeps original external texture paths.
                texture_index = material.get('pbrMetallicRoughness', {}).get('baseColorTexture', {}).get('index')
                if texture_index is None:
                    continue
                image_index = gltf['textures'][texture_index]['source']
                texture = gltf['images'][image_index]['uri']
                referenced[texture].add('Bark' if kind == 0 else 'Leaves' if kind in (1, 2) else 'Vegetation atlas')
        for texture, labels in referenced.items():
            record(texture, metadata['geometry'], modelmaps[legacy], counts[legacy], hint=sorted(labels)[0])
            rows[norm(texture)]['categories'].update(labels)

    # Modern water normals are generated from four analytic periodic waves.
    # There is no source texture to replace in CONTENT-TX; water.wtr presence
    # is reported separately and never advertised as actual visible coverage.
    watermaps = sorted({key.rsplit('/', 2)[0] for key in files if key.endswith('/water.wtr')})
    result = []
    for texture, row in rows.items():
        row.update(asset_references=len(row['assets']), map_count=len(row['maps']), available=texture in available)
        for field in ('assets', 'maps', 'categories', 'evidence'):
            row[field] = sorted(row[field])
        result.append(row)
    result.sort(key=lambda row: (-row['map_count'], -row['asset_references'], -row['placements'], row['texture']))
    summary = dict(schema=1, input_sha256=hashlib.sha256(args.pack_audit.read_bytes()).hexdigest(),
        legacy_text_decode_replacements=json.dumps(packed, ensure_ascii=False).count('\ufffd'),
        effective_entries=len(available), audited_files=len(files), maps_with_texture_sets=len(mapsets),
        placed_environment_assets=len(modelmaps), missing_packs=packed['missingPacks'],
        source_pack_mismatches=[dict(path=key, pack=row['pack']) for key, row in files.items() if not row['identical']],
        unresolved=missing, placed_gr2_parse_errors=parse_errors,
        unresolved_placement_properties=[dict(map=name, crc=crc, placements=count)
            for (name, crc), count in sorted(unresolved_properties.items())],
        all_gr2_parse_error_count=sum('parseError' in row for row in files.values()),
        missing_placed_models=sorted(key for key in modelmaps if key.endswith('.gr2') and key not in files),
        water_normals=dict(source='src/Renderer/DiligentWater.cpp', generated=True, source_texture=None,
                           maps_with_water_file=watermaps, note='File presence only; not visible water or active normal bindings.'),
        categories={label: sum(label in row['categories'] for row in result) for label in
                    ('Grass', 'Dirt', 'Path', 'Rock', 'Stone', 'Wood', 'Cliff', 'Bark', 'Leaves', 'Unclassified')},
        rows=result)
    (args.output / 'texture-impact.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    with (args.output / 'texture-impact.csv').open('w', newline='', encoding='utf-8') as stream:
        fields = ('texture', 'categories', 'map_count', 'asset_references', 'placements', 'tile_samples', 'available')
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in result:
            writer.writerow({key: '; '.join(row[key]) if key == 'categories' else row[key] for key in fields})
    lines = ['# G8 texture impact audit', '',
        'Counts come from effective pack entries, tile usage and placed environment properties. '
        'Terrain references count tile files; object references count distinct model files. '
        'Tile samples include duplicated borders. Map counts are distinct authored map roots, not active server maps. '
        'Categories use descriptive filenames or native vegetation part kinds; ambiguous names remain Unclassified.', '',
        '| Texture | Category | Maps | Asset references | Placed instances |',
        '| --- | --- | ---: | ---: | ---: |']
    for row in result[:50]:
        lines.append('| `%s` | %s | %d | %d | %d |' % (row['texture'], ', '.join(row['categories']), row['map_count'], row['asset_references'], row['placements']))
    lines += ['', '## Coverage', '', json.dumps({key: value for key, value in summary.items()
                if key not in ('rows', 'source_pack_mismatches', 'unresolved', 'unresolved_placement_properties', 'water_normals')}, indent=2), '',
        'Modern water normals are generated, so no replacement texture is ranked. '
        'Repository/pack mismatches and unresolved links remain in the JSON evidence. '
        'Pixel coverage and player travel frequency are not measured. No textures have been replaced.']
    (args.output / 'texture-impact.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(json.dumps({key: summary[key] for key in ('audited_files', 'maps_with_texture_sets', 'placed_environment_assets', 'categories')}))


if __name__ == '__main__':
    main()
