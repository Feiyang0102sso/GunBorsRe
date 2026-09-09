"""从ARMv7调试符号关联原工程文件与反编译行号；不猜测原始源码行号。"""
from pathlib import Path, PurePosixPath
import json
import posixpath
import re
import struct


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "out/binary-research/source-index.json"


def main():
    data = (ROOT / "gunbros").read_bytes()
    magic, count = struct.unpack_from(">II", data)
    if magic != 0xCAFEBABE:
        raise ValueError("Expected universal Mach-O")
    base = None
    for index in range(count):
        cpu, subtype, offset, size, alignment = struct.unpack_from(">IIIII", data, 8 + 20 * index)
        if cpu == 12 and subtype == 9:
            base = offset
    if base is None:
        raise ValueError("Missing ARMv7 slice")
    command_count = struct.unpack_from("<I", data, base + 16)[0]
    position = base + 28
    for index in range(command_count):
        command, size = struct.unpack_from("<II", data, position)
        if command == 2:
            symbol_offset, symbol_count, string_offset, string_size = struct.unpack_from("<IIII", data, position + 8)
        position += size
    symbols = {}
    source = ""
    object_file = ""
    for index in range(symbol_count):
        string_index, kind, section, description, value = struct.unpack_from("<IBBHI", data, base + symbol_offset + index * 12)
        first = base + string_offset + string_index
        last = data.find(b"\0", first)
        name = data[first:last].decode("utf-8", errors="replace")
        if kind == 0x64:
            source = posixpath.normpath(name)
            if name == "":
                source = ""
        elif kind == 0x66:
            object_file = name
        elif kind == 0x24 and name:
            symbols[value & ~1] = {"symbol": name, "original_source": source, "object_file": object_file}
    lines = (ROOT / "_IDA_OUT/gunbros_3.6.0_IOS.c").read_text(encoding="utf-8").splitlines()
    functions = []
    for index, line in enumerate(lines):
        match = re.match(r"//----- \(([0-9A-Fa-f]+)\)", line)
        if not match:
            continue
        address = int(match.group(1), 16)
        signature = []
        for next_line in lines[index + 1:index + 16]:
            if next_line == "{":
                break
            signature.append(next_line.strip())
        record = {"line": index + 2, "address": f"0x{address:08X}", "signature": " ".join(signature)}
        record.update(symbols.get(address, {}))
        functions.append(record)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(functions, ensure_ascii=False, indent=2), encoding="utf-8")
    matched = 0
    for record in functions:
        if record.get("original_source"):
            matched += 1
    print(f"Indexed {len(functions)} functions; original source matched {matched}")


if __name__ == "__main__":
    main()
