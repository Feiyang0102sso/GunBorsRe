"""读取带符号 iOS 程序中的原始菜单静态表，不修改程序和资源。"""
from pathlib import Path
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
PROGRAM = ROOT / "gunbros"
SOURCE = ROOT / "_IDA_OUT/gunbros_3.6.0_IOS.c"


def read_image():
    """将 32 位 Mach-O 虚拟地址映射回只读文件切片。"""
    data = PROGRAM.read_bytes()
    base = 0
    if data[:4] == bytes.fromhex("cafebabe"):
        count = struct.unpack_from(">I", data, 4)[0]
        for index in range(count):
            cpu, subtype, offset, size, align = struct.unpack_from(">5I", data, 8 + index * 20)
            if cpu == 12 and subtype == 9:
                base = offset
                break
    if struct.unpack_from("<I", data, base)[0] != 0xFEEDFACE:
        raise ValueError("Expected ARM 32-bit Mach-O")
    count = struct.unpack_from("<I", data, base + 16)[0]
    cursor = base + 28
    segments = []
    for index in range(count):
        command, size = struct.unpack_from("<II", data, cursor)
        if command == 1:
            address, length, offset, file_size = struct.unpack_from("<4I", data, cursor + 24)
            segments.append((address, file_size, base + offset))
        cursor += size
    return data, segments


def main():
    data, segments = read_image()

    def offset_of(address):
        for start, size, offset in segments:
            if start <= address < start + size:
                return offset + address - start
        raise ValueError(f"Unmapped address {address:x}")

    def string_at(address):
        if address == 0:
            return ""
        offset = offset_of(address)
        end = data.index(0, offset)
        return data[offset:end].decode("utf-8", errors="replace")

    source = SOURCE.read_text(encoding="utf-8")
    tables = {}
    for match in re.finditer(r"// ([0-9A-F]+): using guessed type char \*(MDS_\w+);", source):
        name = match.group(2)
        if name in tables:
            continue
        address = int(match.group(1), 16)
        offset = offset_of(address)
        pack_address, count = struct.unpack_from("<IH", data, offset)
        table = {"address": hex(address), "pack": string_at(pack_address), "count": count, "entries": []}
        for index in range(count):
            # Static header is 16 bytes; every original entry occupies 64 bytes.
            entry_offset = offset + 16 + index * 64
            words = struct.unpack_from("<16I", data, entry_offset)
            strings = []
            for value in words:
                text = ""
                if 0x200000 < value < 0x3F0000:
                    text = string_at(value)
                strings.append(text)
            table["entries"].append({"words": words, "strings": strings})
        tables[name] = table
    output = ROOT / "out/menu-statics.json"
    output.write_text(json.dumps(tables, indent=2), encoding="utf-8")
    generated = ["// Generated from the ARMv7 original by src/tools/extract_menu_statics.py."]
    for name, table in tables.items():
        for index, entry in enumerate(table["entries"]):
            words = entry["words"]
            strings = entry["strings"]
            labels = []
            sprites = []
            movies = []
            for slot in range(4):
                labels.append(json.dumps(strings[slot]))
                sprites.append(str(words[4 + slot]))
            for slot in range(2):
                movies.append(json.dumps(strings[10 + slot]))
            generated.append("{%s, %d, {%s}, {%s}, {%s}, %d, %d}," %
                (json.dumps(name), index, ", ".join(labels), ", ".join(sprites),
                 ", ".join(movies), words[14], words[15]))
    target = ROOT / "src/gun_bros_re/runtime/OriginalMenuData.inc"
    target.write_text("\n".join(generated) + "\n", encoding="utf-8")
    print(f"[menu-statics] tables={len(tables)} output={output}")
    for name in ("MDS_BUTTON_TRUNK", "MDS_BUTTON_PLAY", "MDS_BUTTON_STORE_CATEGORIES", "MDS_OPTIONS"):
        print(name, json.dumps(tables.get(name), indent=2))


if __name__ == "__main__":
    main()
