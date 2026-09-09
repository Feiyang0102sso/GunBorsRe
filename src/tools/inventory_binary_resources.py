"""覆盖全部解包逻辑资源，使用原程序字符串哈希还原TOC名称；不按目录名臆测格式。"""
from pathlib import Path
from collections import Counter
import json
import re

from catalog_ui_movies import ROOT, extracted_resources, name_key, pack_records, read_name_table


OUTPUT = ROOT / "out/binary-research/resource-inventory.json"


def main():
    data = (ROOT / "gunbros").read_bytes()
    candidates = re.findall(rb"[A-Za-z_][A-Za-z0-9_./-]{3,180}\x00", data)
    names_by_hash = {}
    for candidate in candidates:
        name = candidate[:-1].decode("ascii")
        names_by_hash.setdefault(name_key(name), set()).add(name)
    assigned = {}
    game = json.loads((ROOT / "out/game-entry-catalog.json").read_text(encoding="utf-8"))
    for entry in game["entries"]:
        assigned[(entry["pack"], entry["logical_id"])] = entry["section"]
    movie_catalog = json.loads((ROOT / "out/ui-movie-catalog.json").read_text(encoding="utf-8"))
    movies = movie_catalog["movies"]
    for entry in movies:
        assigned[(entry["pack"], int(entry["handle"], 16) & 0x7FFF)] = "MOVIE"
    for filename in ("sprite-catalog.json", "container-catalog.json"):
        extra = json.loads((ROOT / "out/binary-research" / filename).read_text(encoding="utf-8"))
        for entry in extra["entries"]:
            assigned[(entry["pack"], entry["logical_id"])] = entry["category"]
    records = []
    unknown = Counter()
    for pack, toc_id in pack_records():
        resources = extracted_resources(pack)
        names = read_name_table(resources[toc_id][0])
        aliases = {}
        for key, handle in names.items():
            resource_id = handle & 0x7FFF
            for name in names_by_hash.get(key, ()):
                if name.endswith("_COUNT") or handle & 0x20000000:
                    continue
                aliases.setdefault(resource_id, set()).add(name)
        for resource_id, (path, row) in resources.items():
            size = int(row["original size"])
            category = assigned.get((pack, resource_id), "UNASSIGNED")
            if resource_id == toc_id:
                category = "NAME_TABLE"
            record = {
                "pack": pack, "logical_id": resource_id, "path": path.relative_to(ROOT).as_posix(),
                "bytes": size, "category": category, "aliases": sorted(aliases.get(resource_id, ())),
            }
            if category == "UNASSIGNED" and size == 0:
                category = "EMPTY_RESOURCE_PLACEHOLDER"
            if category == "UNASSIGNED" and "BIN_DATA_APPPROPERTIES" in record["aliases"]:
                category = "APP_PROPERTIES_TEXT"
            if category == "UNASSIGNED" and path.parent.name.lower() == "0xf686aadc":
                category = "PACK_LABEL_UTF8"
            record["category"] = category
            if size > 0:
                payload = path.read_bytes()
                record["header_hex"] = payload[:32].hex(" ")
                if payload.startswith(b"\x89PNG\r\n\x1a\n"):
                    record["format"] = "PNG"
                    if category == "UNASSIGNED":
                        category = "PNG_OUTSIDE_GAME_SECTION"
                elif payload.startswith(b"RIFF") and payload[8:12] == b"WAVE":
                    record["format"] = "WAV"
                    if category == "UNASSIGNED":
                        category = "WAV_OUTSIDE_GAME_SECTION"
                record["category"] = category
            records.append(record)
            if category == "UNASSIGNED":
                unknown[(pack, size, " / ".join(record["aliases"]))] += 1
    OUTPUT.write_text(json.dumps({"resources": records}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Inventoried {len(records)} logical resources")
    for key, count in unknown.items():
        print(count, key)


if __name__ == "__main__":
    main()
