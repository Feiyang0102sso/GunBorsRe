"""核对地图七类层、模型逐帧边界、关卡依赖表和对象计数表；大顶点数组只记录范围。"""
from collections import Counter
import json
import struct

from catalog_game_entries import Reader, SECTIONS
from catalog_binary_sections import collision, scalars
from catalog_ui_movies import ROOT


OUTPUT = ROOT / "out/binary-research/geometry-catalog.json"
SUPPORTED = ("TILELAYER", "MODEL", "LEVEL_REFS", "COUNTS")


def requirements(reader, prefix):
    groups = reader.read(prefix + "groupCount", "B")
    for group in range(groups):
        name = f"{prefix}groups[{group}]."
        reader.read(name + "objectType", "B")
        count = reader.read(name + "count", "B")
        for index in range(count):
            reader.read(f"{name}references[{index}].packHash", "I")
            reader.read(f"{name}references[{index}].ordinal", "B")


def read_map(reader):
    reader.object("tileSet")
    requirements(reader, "requirements.")
    layers = reader.read("layerCount", "B")
    for layer in range(layers):
        prefix = f"layers[{layer}]."
        kind = reader.read(prefix + "type", "B")
        if kind == 0:
            reader.read(prefix + "discardedMarker", "B")
            width = reader.read(prefix + "width", "H")
            height = reader.read(prefix + "height", "H")
            reader.raw(prefix + "tileAndFlipPairs", width * height * 2)
        elif kind == 1:
            collision(reader, prefix + "collision")
        elif kind == 2:
            total = reader.read(prefix + "totalObjectCount", "H")
            groups = reader.read(prefix + "groupCount", "B")
            actual = 0
            for group in range(groups):
                name = f"{prefix}groups[{group}]."
                object_type = reader.read(name + "objectType", "B")
                count = reader.read(name + "objectCount", "H")
                reader.read(name + "extraAllocationCount", "H")
                actual += count
                for index in range(count):
                    item = f"{name}objects[{index}]."
                    reader.read(item + "packHash", "I")
                    reader.read(item + "ordinal", "B")
                    extra = reader.read(item + "hasExtra", "B")
                    scalars(reader, item, ("x", "y"), "h")
                    reader.read(item + "spawnTag", "B")
                    if extra:
                        if object_type == 15:
                            reader.read(item + "playerExtra", "H")
                        elif object_type == 5:
                            reader.read(item + "enemyExtra", "B")
                            reader.read(item + "enemyParameter", "h")
                        elif object_type == 14:
                            reader.read(item + "platformExtra", "B")
            if actual != total:
                raise ValueError("Object count mismatch")
        elif kind == 3:
            reader.asset(prefix + "movie")
            scalars(reader, prefix, ("x", "y"), "h")
        elif kind == 4:
            for bounds in ("primary.", "secondary."):
                scalars(reader, prefix + bounds, ("x", "y", "width", "height"), "h")
        elif kind == 5:
            nodes = reader.read(prefix + "nodeCount", "B")
            links = reader.read(prefix + "linkCount", "B")
            regions = reader.read(prefix + "regionCount", "B")
            for index in range(nodes):
                scalars(reader, f"{prefix}nodes[{index}].", ("x", "y", "radius"), "h")
            for index in range(links):
                scalars(reader, f"{prefix}links[{index}].", ("first", "second"), "B")
            for index in range(regions):
                name = f"{prefix}regions[{index}]."
                count = reader.read(name + "nodeCount", "B")
                reader.raw(name + "nodeIndices", count)
                scalars(reader, name, ("bound0", "bound1", "bound2", "bound3"), "H")
        elif kind == 6:
            vertices = reader.read(prefix + "vertexCount", "H")
            nodes = reader.read(prefix + "nodeCount", "H")
            expected = reader.read(prefix + "neighborReferenceCount", "H")
            for index in range(vertices):
                scalars(reader, f"{prefix}vertices[{index}].", ("x", "y"), "h")
            actual = 0
            for index in range(nodes):
                name = f"{prefix}nodes[{index}]."
                reader.read(name + "flags", "B")
                scalars(reader, name, ("vertex0", "vertex1", "vertex2", "vertex3"), "h")
                count = reader.read(name + "neighborCount", "B")
                actual += count
                for neighbor in range(count):
                    reader.read(f"{name}neighbors[{neighbor}]", "H")
            if actual != expected:
                raise ValueError("Neighbor count mismatch")
        else:
            raise ValueError(f"Unknown layer type {kind}")


def skip_float_array(reader, name, count):
    size = count * 4
    end = reader.position + size
    if end > len(reader.data):
        raise ValueError(f"Truncated float array {name}")
    reader.fields.append({"offset": reader.position, "name": name, "bytes": size,
                          "element_type": "float32", "element_count": count})
    reader.position = end


def read_model(reader):
    reader.read("discardedVersion", "B")
    indices = reader.read("indexCount", "I")
    bones = reader.read("boneCount", "B")
    frames = reader.read("frameCount", "H")
    vertices = reader.read("vertexCount", "H")
    for index in range(bones):
        size = reader.read(f"bones[{index}].nameLength", "B")
        reader.raw(f"bones[{index}].name", size)
    for index in range(indices):
        value = reader.read(f"indices[{index}]", "H")
        if value >= vertices:
            raise ValueError(f"Vertex index out of range {value}/{vertices}")
    skip_float_array(reader, "uvPairs", vertices * 2)
    for index in range(frames):
        prefix = f"frames[{index}]."
        reader.read(prefix + "timeMs", "i")
        skip_float_array(reader, prefix + "bonePosition3Quaternion4", bones * 7)
        skip_float_array(reader, prefix + "vertexPositions3", vertices * 3)


def main():
    catalog = json.loads((ROOT / "out/game-entry-catalog.json").read_text(encoding="utf-8"))
    records = []
    totals = Counter()
    for entry in catalog["entries"]:
        if entry["section"] not in SUPPORTED or not entry["bytes"]:
            continue
        reader = Reader((ROOT / entry["path"]).read_bytes())
        kind = entry["section"]
        if kind == "TILELAYER":
            read_map(reader)
        elif kind == "MODEL":
            read_model(reader)
        elif kind == "LEVEL_REFS":
            requirements(reader, "")
        elif kind == "COUNTS":
            reader.read("allocationTypeCount", "B")
            # 原InitializeCounts硬编码循环28项，不能拿第一字节当任意可扩展文件数组长度。
            for index in range(28):
                reader.read("count." + SECTIONS[index], "B")
        if reader.position != len(reader.data):
            raise ValueError(f"Unconsumed {entry['path']}: {reader.position}/{len(reader.data)}")
        record = dict(entry)
        record["fields"] = reader.fields
        records.append(record)
        totals[kind] += 1
    OUTPUT.write_text(json.dumps({"totals": dict(totals), "entries": records}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Verified geometry: {dict(totals)}")


if __name__ == "__main__":
    main()
