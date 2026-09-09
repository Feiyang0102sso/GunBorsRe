"""逐包核对SpriteGlu全局表、绘制树、纹理映射和纹理页；输出可追溯字段偏移。"""
import json
import struct

from catalog_game_entries import Reader
from catalog_binary_sections import scalars
from catalog_ui_movies import ROOT, extracted_resources, name_key, pack_records, read_name_table


OUTPUT = ROOT / "out/binary-research/sprite-catalog.json"


def read_global(reader):
    count = reader.read("texturePackNameCount", "B")
    for index in range(count):
        length = reader.read(f"texturePackNames[{index}].length", "H")
        reader.raw(f"texturePackNames[{index}].bytes", length)
    reader.read("discardedTotal", "H")
    count = reader.read("imageSlotCount", "H")
    for index in range(count):
        reader.read(f"imageSlots[{index}].imageIndex", "H")
        reader.read(f"imageSlots[{index}].byteRaw", "B")
    map_count = reader.read("spriteMapCount", "H")
    for index in range(map_count):
        reader.read(f"spriteMaps[{index}].imageSlot", "H")
        reader.read(f"spriteMaps[{index}].transform", "B")
        reader.read(f"spriteMaps[{index}].blendFlags", "B")
    count = reader.read("primitiveCount", "H")
    for index in range(count):
        reader.read(f"primitives[{index}].color", "I")
        scalars(reader, f"primitives[{index}].", ("width", "height"), "H")
        reader.read(f"primitives[{index}].type", "B")
    count = reader.read("substitutionGroupCount", "B")
    for index in range(count):
        length = reader.read(f"substitutions[{index}].count", "H")
        for item in range(length):
            prefix = f"substitutions[{index}].entries[{item}]."
            scalars(reader, prefix, ("word4Raw", "word6Raw"), "H")
            reader.read(prefix + "byte0Raw", "B")
            scalars(reader, prefix, ("word8Raw", "word10Raw"), "H")
    archetypes = reader.read("archetypeCount", "B")
    return archetypes, map_count


def read_archetype(reader, map_count):
    for name in ("sprites", "frames"):
        count = reader.read(name + ".count", "H")
        for index in range(count):
            length = reader.read(f"{name}[{index}].partCount", "B")
            for item in range(length):
                prefix = f"{name}[{index}].parts[{item}]."
                reader.read(prefix + "index", "H")
                scalars(reader, prefix, ("x", "y"), "h")
    count = reader.read("animationCount", "H")
    for index in range(count):
        prefix = f"animations[{index}]."
        reader.read(prefix + "byte13Raw", "B")
        length = reader.read(prefix + "stepCount", "B")
        for item in range(length):
            reader.read(f"{prefix}steps[{item}].frameIndex", "H")
            reader.read(f"{prefix}steps[{item}].duration10Ms", "H")
    count = reader.read("actionCount", "B")
    for index in range(count):
        reader.raw(f"actions[{index}].discardedSpriteMapBits", (map_count + 7) // 8)
        reader.read(f"actions[{index}].substitutionGroup", "B")


def read_texture_map(reader):
    page_count = reader.read("pageCount", "B")
    for index in range(page_count):
        reader.read(f"pageFormats[{index}]", "B")
    count = reader.read("rectangleCount", "H")
    for index in range(count):
        prefix = f"rectangles[{index}]."
        reader.read(prefix + "page", "B")
        scalars(reader, prefix, ("x", "y", "width", "height"), "H")
    count = reader.read("imageIndexCount", "H")
    for index in range(count):
        reader.read(f"imageToRectangle[{index}]", "H")
    return page_count


def add_record(records, pack, resources, handle, kind, parser, *arguments):
    path, row = resources[handle & 0x7FFF]
    reader = Reader(path.read_bytes())
    try:
        result = parser(reader, *arguments)
    except (ValueError, struct.error) as error:
        if kind == "SPRITE_ARCHETYPE":
            records.append({"pack": pack, "logical_id": handle & 0x7FFF, "category": kind,
                            "path": path.relative_to(ROOT).as_posix(), "bytes": len(reader.data),
                            "fields": reader.fields, "error": str(error), "status": "incompatible_with_current_reader"})
            print(f"Archetype anomaly: {path.name}: {error}")
            return None
        raise ValueError(f"{pack} {kind} {path}: {error}") from error
    if reader.position != len(reader.data):
        raise ValueError(f"Incomplete {path}: {reader.position}/{len(reader.data)}")
    records.append({"pack": pack, "logical_id": handle & 0x7FFF, "category": kind,
                    "path": path.relative_to(ROOT).as_posix(), "bytes": len(reader.data), "fields": reader.fields})
    return result


def read_page_counts(reader, count):
    reader.read("discardedCount", "H")
    pages = []
    for index in range(count):
        pages.append(reader.read(f"pagesPerArchetype[{index}]", "B"))
    return pages


def main():
    records = []
    texture_pages = []
    for pack, toc_id in pack_records():
        resources = extracted_resources(pack)
        names = read_name_table(resources[toc_id][0])
        archetypes, maps = add_record(records, pack, resources, names[name_key("SPRITEGLU__BINARY_GLOBAL")],
                                      "SPRITE_GLOBAL", read_global)
        if archetypes != names[name_key("SPRITEGLU__ARCHETYPE_COUNT")]:
            raise ValueError(f"Archetype count mismatch {pack}")
        pages = add_record(records, pack, resources, names[name_key("TEXTURE_MAP_GLOBAL")],
                           "TEXTURE_MAP_GLOBAL", read_page_counts, archetypes)
        tree_base = names[name_key("SPRITEGLU__BINARY_ARCHETYPE_000")]
        map_base = names[name_key("BASE_TEXTURE_MAP")]
        page_base = names[name_key("BASE_TEXTURE_PAGE_0")]
        page_cursor = 0
        for index in range(archetypes):
            add_record(records, pack, resources, tree_base + index, "SPRITE_ARCHETYPE", read_archetype, maps)
            page_count = add_record(records, pack, resources, map_base + index, "TEXTURE_MAP", read_texture_map)
            if page_count != pages[index]:
                raise ValueError(f"Texture page count mismatch {pack}[{index}]")
            for page in range(page_count):
                handle = page_base + page_cursor
                path = resources[handle & 0x7FFF][0]
                texture_pages.append({"pack": pack, "logical_id": handle & 0x7FFF, "archetype": index,
                                      "page": page, "path": path.relative_to(ROOT).as_posix()})
                page_cursor += 1
        print(f"Parsed {pack}: archetypes={archetypes} spriteMaps={maps} texturePages={page_cursor}")
    OUTPUT.write_text(json.dumps({"entries": records, "texture_pages": texture_pages}, ensure_ascii=False, indent=2), encoding="utf-8")
    anomaly_count = 0
    for record in records:
        if "error" in record:
            anomaly_count += 1
    print(f"Sprite structures: {len(records) - anomaly_count} valid, {anomaly_count} anomalies; page references={len(texture_pages)}")


if __name__ == "__main__":
    main()
