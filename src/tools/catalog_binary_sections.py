"""按原iOS读取器核对此前未覆盖的BIG实体；输出每个样本的真实字段偏移。"""
from pathlib import Path
import json
import struct

from catalog_game_entries import Reader


ROOT = Path(__file__).resolve().parents[2]
INPUT = ROOT / "out/game-entry-catalog.json"
OUTPUT = ROOT / "out/binary-research/section-catalog.json"
SUPPORTED = {
    "BULLET", "DAILYBONUS", "ENEMY", "LEVEL", "PARTICLEEFFECT", "PICKUP", "PLAYER",
    "PLAYERPROGRESSION", "PRIZE", "PROP", "SOUNDEFFECT", "TILESET", "TUTORIAL", "CHALLENGE", "MP_MATCH",
}


def scalars(reader, prefix, names, kind):
    for name in names:
        reader.read(prefix + name, kind)


def object_array(reader, name, typed=False, count_kind="B"):
    count = reader.read(name + ".count", count_kind)
    for index in range(count):
        prefix = f"{name}[{index}]"
        if typed:
            reader.read(prefix + ".objectType", "B")
        reader.object(prefix)


def collision(reader, name):
    count = reader.read(name + ".vertexCount", "H")
    for index in range(count):
        scalars(reader, f"{name}.vertices[{index}].", ("x", "y"), "i")
    count = reader.read(name + ".edgeCount", "H")
    for index in range(count):
        prefix = f"{name}.edges[{index}]."
        reader.read(prefix + "type", "B")
        scalars(reader, prefix, ("firstVertex", "lastVertex"), "H")


def sprite_moves(reader):
    reader.read("moves.packHash", "I")
    reader.read("moves.archetype", "B")
    reader.read("moves.action", "B")
    count = reader.read("moves.moveCount", "B")
    if count == 0:
        # 本批导出器仍写末尾u16零；原Init :99488提前返回，未消费这两个字节。
        if len(reader.data) - reader.position == 2:
            reader.read("moves.exporterUnusedAllocationWord", "H")
        return
    total = reader.read("moves.totalEntryCount", "H")
    if total == 0 and len(reader.data) - reader.position < count * 5:
        # pack9_xga_0074_0x41dd.bin短尾：count=4，后面仅4个FF，原读取器需要至少20字节。
        # 保留异常证据，不能为了匹配样本而给原读取器发明另一套分支。
        reader.raw("moves.truncatedOriginalMoveBodies", len(reader.data) - reader.position)
        return
    actual = 0
    for index in range(count):
        prefix = f"moves.moves[{index}]."
        scalars(reader, prefix, ("animation", "flag", "restarts", "speedRaw"), "B")
        length = reader.read(prefix + "entryCount", "B")
        actual += length
        for item in range(length):
            scalars(reader, f"{prefix}entries[{item}].", ("byte0", "byte1", "byte2", "byte3"), "B")
    if total != actual:
        raise ValueError(f"CMoveSet entries count mismatch: {total} != {actual}")


def particles(reader):
    reader.read("spritePackHash", "I")
    count = reader.read("emitterCount", "B")
    for index in range(count):
        prefix = f"emitters[{index}]."
        reader.read(prefix + "archetype", "B")
        reader.read(prefix + "animationMask", "I")
        scalars(reader, prefix, ("intervalMinimumSeconds", "intervalMaximumSeconds", "startSeconds", "endSeconds"), "f")
        reader.read(prefix + "alignToVelocity", "B")
        scalars(reader, prefix, ("accelerationX", "accelerationY"), "f")
        for channel in range(8):
            channel_name = f"{prefix}channels[{channel}]."
            keys = reader.read(channel_name + "keyCount", "B")
            for key in range(keys):
                key_name = f"{channel_name}keys[{key}]."
                keep = reader.read(key_name + "keepPreviousStart", "B")
                scalars(reader, key_name, ("startMs", "durationMs"), "I")
                if keep == 0:
                    scalars(reader, key_name, ("startMin", "startMax"), "f")
                scalars(reader, key_name, ("endMin", "endMax"), "f")
        pattern = reader.read(prefix + "pattern", "B")
        if pattern not in (0, 1, 2):
            raise ValueError(f"Unknown particle pattern {pattern}")
        values = 4
        if pattern == 2:
            values = 6
        for value in range(values):
            reader.read(f"{prefix}patternValues[{value}]", "f")
        velocity = reader.read(prefix + "velocity", "B")
        if velocity not in (0, 1):
            raise ValueError(f"Unknown particle velocity {velocity}")
        for value in range(4):
            reader.read(f"{prefix}velocityValues[{value}]", "f")


def read_section(section, data):
    reader = Reader(data)
    if section == "BULLET":
        reader.sprite("sprite")
        reader.asset("mesh")
        reader.asset("image")
        reader.raw("discarded", 1)
        scalars(reader, "", ("offset16Raw", "offset18Raw", "radius"), "h")
        reader.read("baseDamage", "B")
        reader.read("scalar28Fixed16Raw", "i")
        reader.script()
        reader.read("flags", "I")
        scalars(reader, "", ("spriteScaleFixed16", "meshScaleFixed16", "accelerationFixed16", "scalar120Fixed16Raw"), "i")
        reader.read("value124Raw", "H")
        reader.read("trajectoryHeightFixed16", "i")
        reader.read("trajectoryDurationMs", "H")
        reader.read("trajectoryType", "B")
    elif section == "DAILYBONUS":
        object_array(reader, "prizes")
        reader.read("consecutiveDayMax", "H")
    elif section == "ENEMY":
        reader.read("debugScriptEnabled", "B")
        reader.asset("name")
        reader.script()
        reader.moves()
        reader.object("bullet")
        scalars(reader, "", ("health", "speed"), "H")
        scalars(reader, "", ("collisionGroupRaw", "flagsRaw"), "B")
        scalars(reader, "", ("experience", "xplodium"), "H")
        collision(reader, "collision")
    elif section == "LEVEL":
        reader.object("map")
        reader.script()
        scalars(reader, "", ("wavesPerRevolution", "waveLimit", "perfectWaveRewardPercent"), "H")
    elif section == "PARTICLEEFFECT":
        particles(reader)
    elif section == "PICKUP":
        reader.asset("name")
        reader.sprite("sprite")
        reader.object("particleEffect")
        reader.script()
        object_array(reader, "storeItems")
    elif section == "PLAYER":
        reader.script()
        reader.moves()
        reader.object("reference104Raw")
        reader.read("modelScale", "H")
        reader.object("discardedObject0")
        reader.object("discardedObject1")
        reader.sprite("sprite")
    elif section == "PLAYERPROGRESSION":
        reader.table("experienceThresholds")
        # 原ReadUInt32后截成u16写内存，磁盘仍是每元素4字节。
        reader.table("levelValuesTruncated16")
        reader.read("bonusFixed16Raw", "i")
        reader.read("value24Raw", "i")
    elif section == "PRIZE":
        scalars(reader, "", ("value4Raw", "value8Raw", "value12Raw"), "I")
        object_array(reader, "storeItems")
        for name in ("icon", "name", "description"):
            reader.asset(name)
        reader.read("percentFixed16", "i")
        reader.read("attributes", "I")
    elif section == "PROP":
        reader.sprite("sprite")
        scalars(reader, "", ("value17Raw", "value16Raw"), "B")
        collision(reader, "collision20")
        collision(reader, "collision48")
        reader.read("value18Raw", "B")
        reader.script()
        sprite_moves(reader)
    elif section == "SOUNDEFFECT":
        reader.asset("wav")
    elif section == "TUTORIAL":
        reader.asset("asset4Raw")
        reader.asset("asset16Raw")
        reader.read("value28Raw", "B")
    elif section == "TILESET":
        count = reader.read("imageCount", "B")
        for index in range(count):
            reader.asset(f"images[{index}]")
        count = reader.read("tileCount", "B")
        for index in range(count):
            prefix = f"tiles[{index}]."
            reader.read(prefix + "imageIndex", "B")
            scalars(reader, prefix, ("x", "y", "width", "height"), "H")
    elif section == "CHALLENGE":
        reader.read("categoryRaw", "B")
        reader.asset("name")
        reader.asset("description")
        scalars(reader, "", ("value32Raw", "value33Raw", "value34Raw"), "B")
        for name in ("prize36Raw", "prize44Raw", "object52Raw", "level"):
            reader.object(name)
        reader.read("flags", "I")
        reader.read("requiredKills", "H")
        scalars(reader, "", ("circumstanceMask", "gunCategoryMask"), "I")
        object_array(reader, "weaponTypes", True)
        object_array(reader, "enemies")
        scalars(reader, "", ("firstWave", "lastWave", "requiredPerfectWaves", "value120Raw", "value122Raw"), "H")
        object_array(reader, "powerups")
    elif section == "MP_MATCH":
        reader.read("modeRaw", "B")
        object_array(reader, "objects8Raw")
        object_array(reader, "objects16Raw")
        count = reader.table("spawnWeight", "i")
        for table in (1, 2):
            stored_count = reader.read(f"table{table}.discardedCount", "H")
            # 原Init忽略后两count，以第一表的count为准；单独记录是否一致。
            for index in range(count):
                reader.read(f"table{table}[{index}]", "i")
            if stored_count != count:
                print(f"MP table count differs: {stored_count} vs {count}")
        scalars(reader, "", ("startingHealth", "killLimit", "matchDurationSeconds", "respawnDelaySeconds"), "H")
    else:
        raise ValueError(f"Unsupported section {section}")
    if reader.position != len(data):
        raise ValueError(f"Unconsumed bytes: {reader.position}/{len(data)}")
    return reader.fields


def main():
    catalog = json.loads(INPUT.read_text(encoding="utf-8"))
    records = []
    totals = {}
    errors = []
    anomalies = []
    for entry in catalog["entries"]:
        if entry["bytes"] == 0 or entry["section"] not in SUPPORTED:
            continue
        record = dict(entry)
        data = (ROOT / entry["path"]).read_bytes()
        try:
            record["fields"] = read_section(entry["section"], data)
            for field in record["fields"]:
                if field["name"] == "moves.truncatedOriginalMoveBodies":
                    record["status"] = "truncated_original_move_bodies"
                    anomalies.append({"path": entry["path"], "reason": record["status"]})
        except (ValueError, struct.error) as error:
            record["error"] = str(error)
            errors.append({"path": entry["path"], "section": entry["section"], "error": str(error)})
        records.append(record)
        totals.setdefault(entry["section"], {"files": 0, "errors": 0})
        totals[entry["section"]]["files"] += 1
        if "error" in record:
            totals[entry["section"]]["errors"] += 1
    OUTPUT.write_text(json.dumps({"totals": totals, "errors": errors, "anomalies": anomalies, "entries": records}, ensure_ascii=False, indent=2), encoding="utf-8")
    for section, total in totals.items():
        print(f"{section}: {total}")
    for error in errors[:12]:
        print(error)
    print(f"Known source-layout anomalies: {len(anomalies)}")
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
