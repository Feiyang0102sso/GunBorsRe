"""按原 Mach-O 符号只读反汇编，补证反编译遗漏的参数及小型函数。"""
from pathlib import Path
import argparse
import struct
import sys

from extract_menu_statics import read_image


ROOT = Path(__file__).resolve().parents[2]
# Research-only local dependency; does not change system Python packages.
sys.path.insert(0, str(ROOT / "out/research-python"))
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB


def read_symbols(data):
    """保留 n_desc 的 Thumb 标志，不能仅凭函数名或地址猜指令集。"""
    base = 0
    if data[:4] == bytes.fromhex("cafebabe"):
        for index in range(struct.unpack_from(">I", data, 4)[0]):
            cpu, subtype, offset, size, alignment = struct.unpack_from(">5I", data, 8 + index * 20)
            if cpu == 12 and subtype == 9:
                base = offset
                break
    position = base + 28
    for index in range(struct.unpack_from("<I", data, base + 16)[0]):
        command, size = struct.unpack_from("<II", data, position)
        if command == 2:
            symbol_offset, count, string_offset, string_size = struct.unpack_from("<4I", data, position + 8)
            symbols = []
            for item in range(count):
                name_offset, kind, section, description, address = struct.unpack_from(
                    "<IBBHI", data, base + symbol_offset + item * 12)
                if kind & 0xE0 or not section or not address:
                    continue
                first = base + string_offset + name_offset
                last = data.index(0, first)
                name = data[first:last].decode("utf-8")
                symbols.append((address & ~1, name, description))
            symbols.sort()
            return symbols
        position += size
    raise ValueError("Mach-O symbol table not found")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("query", help="原修饰符号的完整名称或子串")
    parser.add_argument("--max-bytes", type=int, default=4096)
    args = parser.parse_args()
    data, segments = read_image()
    symbols = read_symbols(data)
    matched = False
    for index, (address, name, description) in enumerate(symbols):
        if args.query not in name:
            continue
        matched = True
        limit = address + args.max_bytes
        for next_address, next_name, next_description in symbols[index + 1:]:
            if next_address > address:
                limit = min(limit, next_address)
                break
        for start, length, offset in segments:
            if not start <= address < start + length:
                continue
            mode = CS_MODE_ARM
            mode_name = "ARM"
            if description & 8:
                mode = CS_MODE_THUMB
                mode_name = "Thumb"
            first = offset + address - start
            limit = min(limit, start + length)
            print(f"\n{name} VA={address:#x} file={first:#x} n_desc={description:#x} mode={mode_name}")
            decoder = Cs(CS_ARCH_ARM, mode)
            for instruction in decoder.disasm(data[first:first + limit - address], address):
                print(f"{instruction.address:08x}  {instruction.bytes.hex(' '):12} {instruction.mnemonic:8} {instruction.op_str}")
            break
    if not matched:
        raise ValueError(f"Original symbol not found: {args.query}")


if __name__ == "__main__":
    main()
