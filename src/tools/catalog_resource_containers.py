"""核对资源索引、语言聚合包和字体；只读原文件，输出字段及字符串内容。"""
import json
import struct

from catalog_game_entries import Reader
from catalog_ui_movies import ROOT, extracted_resources, name_key, pack_records, read_name_table


OUTPUT = ROOT / "out/binary-research/container-catalog.json"


def read_init(reader):
    count = reader.read("leadingCount", "I")
    for index in range(count):
        reader.read(f"leadingValues[{index}]", "I")
    count = reader.read("localeCount", "I")
    code_size = reader.read("localeCodeBytes", "I")
    name_size = reader.read("localeNameBytes", "I")
    locale_ids = []
    if count and code_size:
        for index in range(count):
            locale_ids.append(reader.read(f"localeResourceIds[{index}]", "I"))
        reader.raw("localeCodesUtf8", count * code_size)
        reader.raw("localeDisplayNamesUtf8", count * name_size)
    count = reader.read("aggregateCount", "I")
    aggregate_ids = []
    for index in range(count):
        aggregate_ids.append(reader.read(f"aggregateResourceIds[{index}]", "I"))
    return {"locale_ids": locale_ids, "aggregate_ids": aggregate_ids}


def read_keyset(reader):
    count = reader.read("count", "H")
    handles = []
    for index in range(count):
        handles.append(reader.read(f"handles[{index}]", "I"))
    return {"handles": handles}


def read_names(reader):
    count = reader.read("count", "I")
    for index in range(count):
        reader.read(f"entries[{index}].nameHash", "I")
        reader.read(f"entries[{index}].handleOrScalar", "I")


def read_aggregate(reader):
    flags = reader.read("flags", "H")
    count = reader.read("entryCount", "H")
    first_id = 0
    if flags & 0x8000:
        first_id = reader.read("firstEntryId", "H")
    entries = []
    for index in range(count):
        prefix = f"entries[{index}]."
        entry_id = first_id + index
        if not flags & 0x8000:
            entry_id = reader.read(prefix + "id", "H")
        offset_kind = "H"
        if flags & 0x4000:
            offset_kind = "I"
        offset = reader.read(prefix + "offset", offset_kind)
        entries.append({"id": entry_id, "offset": offset})
    end_offset = reader.read("finalEndOffset", "I")
    if flags & 0x2000:
        for index in range(count):
            entries[index]["mime_key"] = reader.read(f"entries[{index}].mimeKey", "I")
    for index in range(count):
        entry = entries[index]
        end = end_offset
        if index + 1 < count:
            end = entries[index + 1]["offset"]
        if reader.position != entry["offset"]:
            raise ValueError(f"Aggregate offset mismatch {index}: {reader.position}/{entry['offset']}")
        prefix = f"entries[{index}]."
        header_size = reader.read(prefix + "resourceHeaderBytes", "H")
        reader.read(prefix + "resourceFlags", "B")
        reader.read(prefix + "reservedHeaderByte", "B")
        if header_size != 4:
            raise ValueError(f"Unusual aggregate resource header {header_size}")
        payload = reader.data[reader.position:end]
        reader.raw(prefix + "utf8WithNul", end - reader.position)
        if entry.get("mime_key") == 0xF686AADC:
            entry["text"] = payload.rstrip(b"\0").decode("utf-8")
        entry["bytes"] = end - entry["offset"]
    return {"strings": entries}


def read_jm_utf(reader, prefix):
    high = reader.read(prefix + ".lengthHigh", "B")
    low = reader.read(prefix + ".lengthLow", "B")
    size = high * 256 + low
    data = reader.data[reader.position:reader.position + size]
    reader.raw(prefix + ".utf8Bytes", size)
    return data.decode("utf-8")


def read_font(reader):
    version = reader.read("version", "B")
    reader.raw("headerUnknown", 3)
    for name in ("maximumHeight", "bodyHeight", "characterSpacing", "lineSpacing"):
        reader.read(name, "b")
    glyph_count = reader.read("glyphCount", "h")
    control_count = reader.read("controlCount", "h")
    codes = ""
    if version == 2 and glyph_count:
        codes = read_jm_utf(reader, "glyphCharacters")
        if len(codes) != glyph_count:
            raise ValueError("Glyph character count mismatch")
    for index in range(glyph_count):
        prefix = f"glyphs[{index}]."
        if version != 2:
            reader.read(prefix + "characterCode", "H")
        for name in ("atlasX", "atlasY"):
            reader.read(prefix + name, "h")
        for name in ("width", "height", "offsetX", "offsetY", "advance", "byte11Raw"):
            reader.read(prefix + name, "b")
    controls = ""
    if version == 2 and control_count:
        controls = read_jm_utf(reader, "controlCharacters")
        if len(controls) != control_count:
            raise ValueError("Control character count mismatch")
    for index in range(control_count):
        prefix = f"controls[{index}]."
        if version != 2:
            reader.read(prefix + "characterCode", "H")
        reader.read(prefix + "byte2Raw", "b")
        reader.read(prefix + "advance", "b")
    return {"glyph_characters": codes, "control_characters": controls}


def main():
    records = []
    seen = set()

    def add(pack, resources, handle, category, parser):
        resource_id = handle & 0x7FFF
        key = (pack, resource_id)
        if key in seen:
            return None
        seen.add(key)
        path = resources[resource_id][0]
        reader = Reader(path.read_bytes())
        details = parser(reader)
        if reader.position != len(reader.data):
            raise ValueError(f"Unconsumed {path}: {reader.position}/{len(reader.data)}")
        record = {"pack": pack, "logical_id": resource_id, "category": category,
                  "path": path.relative_to(ROOT).as_posix(), "bytes": len(reader.data), "fields": reader.fields}
        if details:
            record.update(details)
        records.append(record)
        return details

    for pack, toc_id in pack_records():
        resources = extracted_resources(pack)
        names = read_name_table(resources[toc_id][0])
        add(pack, resources, toc_id, "NAME_TABLE", read_names)
        init = add(pack, resources, names[name_key("___INIT_DATA")], "INIT_DATA", read_init)
        for handle in init["locale_ids"] + init["aggregate_ids"]:
            add(pack, resources, handle, "STRING_AGGREGATE", read_aggregate)
        # MIME句柄类别5是keyset，排除聚合资源标志，避免把字符串元素误当独立文件。
        keyset_handles = set()
        for handle in names.values():
            if handle & 0x3FFF0000 == 0x05000000 and handle & 0x7FFF != 0:
                keyset_handles.add(handle)
        for handle in sorted(keyset_handles):
            parsed = add(pack, resources, handle, "KEYSET", read_keyset)
            if handle == names.get(name_key("FONT_KEYSET")):
                handles = parsed["handles"]
                for index in range(0, len(handles), 2):
                    add(pack, resources, handles[index], "BITMAP_FONT", read_font)
        print(f"Parsed containers: {pack}")
    OUTPUT.write_text(json.dumps({"entries": records}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Verified {len(records)} resource containers")


if __name__ == "__main__":
    main()
