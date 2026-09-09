"""只读核对PNG块CRC、WAVE块边界以及被命名为bin的属性文本，补齐资源覆盖。"""
from pathlib import Path
from collections import Counter
import json
import struct
import zlib


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "out/binary-research/standard-catalog.json"


def png_chunks(data):
    """图像像素不逐个展开；保存每块位置/长度和IHDR，校验每块CRC。"""
    position = 8
    chunks = []
    details = {}
    while position < len(data):
        size = struct.unpack_from(">I", data, position)[0]
        kind = data[position + 4:position + 8]
        end = position + 12 + size
        if end > len(data):
            raise ValueError("Truncated PNG chunk")
        checksum = struct.unpack_from(">I", data, end - 4)[0]
        if checksum != zlib.crc32(data[position + 4:end - 4]):
            raise ValueError("PNG CRC mismatch")
        chunks.append({"offset": position, "type": kind.decode("ascii"), "bytes": size, "crc_valid": True})
        if kind == b"IHDR":
            if size != 13:
                raise ValueError("Unexpected IHDR size")
            values = struct.unpack_from(">IIBBBBB", data, position + 8)
            names = ("width", "height", "bit_depth", "color_type", "compression", "filter", "interlace")
            for index, name in enumerate(names):
                details[name] = values[index]
        position = end
        if kind == b"IEND":
            break
    if position != len(data) or not chunks or chunks[-1]["type"] != "IEND":
        raise ValueError("PNG missing IEND or trailing data")
    details["chunks"] = chunks
    return details


def wave_chunks(data):
    """WAVE为RIFF块容器；fmt说明真实编码，不把所有data都硬称PCM。"""
    declared = struct.unpack_from("<I", data, 4)[0]
    if declared + 8 != len(data):
        raise ValueError("RIFF size mismatch")
    position = 12
    chunks = []
    details = {}
    while position < len(data):
        kind = data[position:position + 4]
        size = struct.unpack_from("<I", data, position + 4)[0]
        end = position + 8 + size
        if end > len(data):
            raise ValueError("Truncated WAVE chunk")
        chunks.append({"offset": position, "type": kind.decode("ascii"), "bytes": size})
        if kind == b"fmt ":
            values = struct.unpack_from("<HHIIHH", data, position + 8)
            names = ("format_tag", "channels", "sample_rate", "bytes_per_second", "block_align", "bits_per_sample")
            for index, name in enumerate(names):
                details[name] = values[index]
            details["format_extension_hex"] = data[position + 24:end].hex(" ")
        elif kind == b"fact" and size >= 4:
            details["sample_count"] = struct.unpack_from("<I", data, position + 8)[0]
        elif kind == b"data":
            details["audio_data_bytes"] = size
        position = end + (size % 2)
    if position != len(data):
        raise ValueError("WAVE alignment mismatch")
    details["chunks"] = chunks
    return details


def main():
    inventory = json.loads((OUTPUT.parent / "resource-inventory.json").read_text(encoding="utf-8"))
    entries = []
    counts = Counter()
    formats = Counter()
    for resource in inventory["resources"]:
        category = resource["category"]
        kind = resource.get("format")
        if kind not in ("PNG", "WAV") and category not in ("APP_PROPERTIES_TEXT", "PACK_LABEL_UTF8"):
            continue
        data = (ROOT / resource["path"]).read_bytes()
        if kind == "PNG":
            details = png_chunks(data)
        elif kind == "WAV":
            details = wave_chunks(data)
            formats[details["format_tag"]] += 1
        else:
            kind = category
            text = data.rstrip(b"\0").decode("utf-8")
            details = {"text": text}
            if category == "APP_PROPERTIES_TEXT":
                properties = []
                for index, line in enumerate(text.splitlines()):
                    if not line.strip() or line.lstrip().startswith("#"):
                        continue
                    key, value = line.split("=", 1)
                    properties.append({"line": index + 1, "key": key.strip(), "value": value.strip()})
                details["properties"] = properties
        counts[kind] += 1
        entries.append({"path": resource["path"], "category": kind, "bytes": len(data), "details": details})
    OUTPUT.write_text(json.dumps({"totals": dict(counts), "wave_formats": dict(formats), "entries": entries}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Verified standard resources: {dict(counts)}; WAVE format tags: {dict(formats)}")


if __name__ == "__main__":
    main()
