"""只读定位解包目录中的 CMovie；输出原名、文件路径和结构索引。"""
from pathlib import Path
import csv
import json
import re
import struct


ROOT = Path(__file__).resolve().parents[2]
EXTRACTED = ROOT / "big_360_out"
OUTPUT = ROOT / "out/ui-movie-catalog.json"
GUIDE = ROOT / "docs/ui-binary-templates.md"
FRAME_SIZES = {0: 33, 1: 48, 2: 6, 3: 12, 5: 4, 6: 37, 7: 42, 8: 6}
TYPE_NAMES = {
    0: "CMovieSprite", 1: "CMovieTiledSprite", 2: "CEmbededMovie",
    3: "CMovieText", 5: "CMovieChapter", 6: "CMovieRegion",
    7: "CMovieFill", 8: "CMovieSoundSet",
}
IMPORTANT_MOVIES = (
    "GLU_MOVIE_HEADER", "GLU_MOVIE_INFO_CLUSTER", "GLU_MOVIE_WIPE",
    "GLU_MOVIE_TRUNK_BUTTONS", "GLU_MOVIE_MAP_PARALAX_COPY",
    "GLU_MOVIE_STORE_MENU", "GLU_MOVIE_STORE_SCROLL", "GLU_MOVIE_SHOP_BOX",
    "GLU_MOVIE_SORT_BAR", "GLU_MOVIE_MISSION_MENU", "GLU_MOVIE_MISSION_LIST",
    "GLU_MOVIE_MISSION_BOX", "GLU_MOVIE_WAVE_SELECT", "GLU_MOVIE_PLAYER_SELECT",
    "GLU_MOVIE_LIST_MENU", "GLU_MOVIE_OFFLINE_BROHOOD", "GLU_MOVIE_WELCOME_NEW",
)


def file_link(path):
    """文档从 docs 目录链接回原文件，不复制或重命名用户资源。"""
    return f"[{Path(path).name}](<../{path}>)"


def write_guide(movies, packs, type_totals):
    """生成供人查阅的模板入口、代表样本和全部文件映射。"""
    lines = [
        "# Gun Bros UI 二进制模板与文件索引", "",
        "本次任务以 `big_360_out` 为输入，定位原 UI 时间轴并制作辅助理解的 010 Editor `.bt`。不修改 UI 实现，不修改解包文件；按用户要求未执行 010 Editor 编译或运行测试。", "",
        "## 阅读入口", "",
        "- [ui_movie.bt](<../_Big_tool/binary template/big_assets/ui_movie.bt>)：逐字段中文注释，覆盖原 CMovie 的八种对象类型。",
        "- [完整结构索引](../out/ui-movie-catalog.json)：每个对象的偏移、类型、关键帧时间，以及用户区域的原始坐标、锚点、尺寸与显示状态。",
        "- [只读索引工具](../src/tools/catalog_ui_movies.py)：可重复生成本页及 JSON，使用 Python 标准库。", "",
        "## 来源与识别方法", "",
        "原名来自 `gunbros` 内的 GLU_MOVIE_* 字符串；用原 CStringToKey 哈希查询各包解出的 TOC，得到资源 handle。外层 `big/packTOC_xga.dat` 指明每个包的 TOC 逻辑 ID；每包 resources.csv 将逻辑 ID 对应到 physical_id 和归档 Offset，再找到解包文件。最后按原 CMovie 各 Init 的读取顺序检查全部字节。未使用重建页面的手写坐标或手填 Movie 数量作为识别依据。", "",
        "资源 handle、逻辑 ID、物理序号、原 BIG 偏移和 Movie 序号是不同的编号。Movie 序号 = handle - GLU_MOVIE_MOVIE 基址；普通 handle 的低 15 位是逻辑 ID。文件名中的末尾 Offset 是原 BIG 的位置，不是解压文件内的偏移。", "",
        "**不要对整个 BIG 应用 ui_movie.bt，也不要把 0xf4e02223 目录中所有 bin 都当作 Movie。** 该目录还含地图、脚本、模型等其他二进制。部分 CSV 标成 bin 的文件现已改成 png 扩展名，索引工具按文件主名匹配。", "",
        "## 主要界面模板", "",
        "| 原资源名 | Movie | 解压字节 | 具体文件 |", "|---|---:|---:|---|",
    ]
    by_alias = {}
    for movie in movies:
        if movie["pack"] == "pack0_core_xga":
            for alias in movie["aliases"]:
                by_alias[alias] = movie
    for alias in IMPORTANT_MOVIES:
        movie = by_alias.get(alias)
        if movie is not None:
            lines.append(f"| {alias} | {movie['ordinal']} | {movie['bytes']} | {file_link(movie['path'])} |")
    lines.extend(["", "## 从商店骨架开始读", "",
        "STORE_MENU 的头部为 `C0 03 80 02 DC 05 00 00 06 00`：宽 960、高 640、时长 1500ms、6 个对象。四个 type=6 区域只是动态内容的布局槽位，不内嵌商品列表、玩家模型或回调函数。", "",
        "| 对象序号 | 文件内偏移 | 类型 | 用户区域序号 |", "|---:|---:|---|---:|"])
    store = by_alias["GLU_MOVIE_STORE_MENU"]
    for item in store["objects"]:
        lines.append(f"| {item['object_index']} | 0x{item['offset']:X} | {item['type_name']} | {item.get('user_region_index', '—')} |")
    lines.extend(["", "四个区域按原 CMenuStore::Init (:180199) 分别绑定商品内容、分类、玩家模型、换枪按钮。对象序号与区域序号在本例恰好相同，其他 Movie 不保证相同。", "",
        "文件大小核对：`10 + 4 × (3 + 37) + (3 + 4) + (3 + 42) = 222` 字节。",
        "SHOP_BOX 则通过同一组区域的关键帧从折叠变为展开；章节文件数据为 800、1300，原引擎在内存中另补起始章节 0。", "",
        "## 格式要点", "",
        "| 类型 | 原类 | 文件单帧字节 | 本次对象数 |", "|---:|---|---:|---:|"])
    for kind, label in TYPE_NAMES.items():
        lines.append(f"| {kind} | {label} | {FRAME_SIZES[kind]} | {type_totals.get(kind, 0)} |")
    lines.extend(["", "- 单帧长度包括 4 字节时间，不包括对象的 3 字节头；所有字段小端、紧密排列。",
        "- x/y 为 int16，alpha/scale/rotation 为有符号 16.16 定点数。宽高原读取为 UInt16，布局计算按 int16 使用。",
        "- type=6 的末字节是可选回调标签，不是用户区域顺序编号；可见性在此前的 EmptyRegionGeometry 中。",
        "- type=2 的末字节是 visible；原 CEmbededMovie::Refresh 另行使用取模推进子电影，不应把这个字节命名为 loop。",
        "- type=7 最后 6 字节是两个 RGB 渐变颜色，不是两个 RGBA；type=8 最后两字节是声音索引与播放模式。",
        "- 原读取器支持 type=3，但当前 175 个文件没有这种对象；三个 UInt16 的语义保留为待解引用，模板仅保证依据原读取顺序描述其宽度。",
        "- 不能把 np_malloc 的内存步长直接当文件记录长度，尤其是 Sprite 33/36、Region 37/40、Text 12/40。",
        "- 当前批次有 9 个只有 10 字节头部的合法空 Movie，不应因对象数为 0 丢弃，也不据此判断其历史用途。", "",
        "## 如何关联其他资源", "",
        "Movie 保存精灵与动画选择，不保存 PNG 像素。图片继续沿 `SPRITEGLU__BINARY_GLOBAL`、`SPRITEGLU__BINARY_ARCHETYPE_000` 和纹理页映射查找；字体入口为 `FONT_KEYSET`。菜单的 MDS 组合表存于原程序，不能在这些 Movie 中凭空增加按钮动作、商品价格或函数地址字段。", "",
        "## 各包与完整文件索引", "",
        "这批文件中共识别 **" + str(len(movies)) + " 个 Movie**。名称栏为空表示未找到对应 GLU_MOVIE_* 别名，不表示文件无效。结构索引只描述资源布局，不是渲染结果或完整 UI 行为验证。", "",
        "| 资源包 | Movie 基址 | 数量 | 解包 TOC |", "|---|---|---:|---|"])
    for pack in packs:
        lines.append(f"| {pack['pack']} | {pack['base']} | {pack['count']} | {file_link(pack['toc_path'])} |")
    for pack in packs:
        if pack["count"] == 0:
            continue
        lines.extend(["", "### " + pack["pack"], "",
            "| Movie | 原名 | handle | 字节 | 对象 | 文件 |", "|---:|---|---|---:|---:|---|"])
        for movie in movies:
            if movie["pack"] != pack["pack"]:
                continue
            alias_text = " / ".join(movie["aliases"])
            lines.append(f"| {movie['ordinal']} | {alias_text} | {movie['handle']} | {movie['bytes']} | {movie['object_count']} | {file_link(movie['path'])} |")
    lines.extend(["", "## 本次核对边界", "",
        "执行 `D:/Python312/python.exe src/tools/catalog_ui_movies.py`，退出码 0。175 个资源均完整消费至文件末尾，原名映射均指向结构可读的 Movie。该检查核对文件定位与序列化长度，不代表 010 Editor 模板已实际执行。", "",
        "`.bt` 的类型名称、字段排列依据原反编译，完整出处在模板头部。未确认字段保留 Raw 后缀和说明。010 Editor 的格式属性参考 [官方 Template Variables 文档](https://www.sweetscape.com/010editor/manual/TemplateVariables.htm)。", ""])
    GUIDE.write_text("\n".join(lines), encoding="utf-8")


def name_key(name):
    """原 CStringToKey；资源名均为 ASCII。"""
    value = len(name)
    for byte in name.encode("ascii"):
        value = (((value << 4) | (value >> 28)) ^ byte) & 0xFFFFFFFF
    return value


def pack_records():
    """外层 packTOC 是大端：名字长度、名字、包内 TOC 逻辑 ID。"""
    data = (ROOT / "big/packTOC_xga.dat").read_bytes()
    cursor = 0
    records = []
    while cursor < len(data):
        length = struct.unpack_from(">H", data, cursor)[0]
        cursor += 2
        name = data[cursor:cursor + length].decode("ascii")
        cursor += length
        resource_id = struct.unpack_from(">I", data, cursor)[0]
        cursor += 4
        pack, key = name.split(":")
        if key == "TABLEOFCONTENTS":
            records.append((pack, resource_id))
    return records


def extracted_resources(pack):
    """CSV 的 physical_id 和 Offset 用于找文件，logical_id 用于解引用。"""
    directory = EXTRACTED / pack
    files = {}
    for path in directory.rglob("*"):
        if path.is_file() and path.suffix.lower() in (".bin", ".png", ".wav", ".txt", ".ref"):
            files[path.stem] = path
    resources = {}
    with (directory / (pack + "_resources.csv")).open(encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            # 部分原 bin 被人工识别为 PNG；不能假定 CSV type 等于当前扩展名。
            filename = f"{pack}_{int(row['physical_id']):04d}_{row['Offset']}"
            if filename not in files:
                raise ValueError(f"Missing extracted file: {filename}")
            resources[int(row["logical_id"], 16)] = (files[filename], row)
    return resources


def read_name_table(path):
    """包内 TOC 为小端 count + (name hash, handle) 对。"""
    data = path.read_bytes()
    count = struct.unpack_from("<I", data)[0]
    if len(data) != 4 + count * 8:
        raise ValueError(f"Unexpected TOC size: {path}")
    entries = {}
    for index in range(count):
        key, handle = struct.unpack_from("<II", data, 4 + index * 8)
        entries[key] = handle
    return entries


def read_movie(data):
    """按原各 Init 的流读取顺序解析，绝不按 malloc 的内存步长跳过。"""
    if len(data) < 10:
        return None
    width, height, duration, count = struct.unpack_from("<HHIH", data)
    if width == 0 or height == 0 or width > 4096 or height > 4096 or count > 255:
        return None
    cursor = 10
    objects = []
    region_index = 0
    for index in range(count):
        start = cursor
        if cursor + 3 > len(data):
            return None
        kind, frames = struct.unpack_from("<BH", data, cursor)
        cursor += 3
        if kind not in FRAME_SIZES:
            return None
        frame_size = FRAME_SIZES[kind]
        if cursor + frames * frame_size > len(data):
            return None
        times = []
        regions = []
        previous_time = -1
        for frame_index in range(frames):
            time = struct.unpack_from("<I", data, cursor)[0]
            if time < previous_time:
                return None
            previous_time = time
            times.append(time)
            if kind == 6:
                x, y, alpha, scale_x, scale_y, rotation = struct.unpack_from("<hhiiii", data, cursor + 4)
                layer, own_anchor, parent_anchor, parent = struct.unpack_from("<4B", data, cursor + 24)
                region_width, region_height = struct.unpack_from("<hh", data, cursor + 28)
                visible, far_anchor, far_x, far_y, callback_tag = struct.unpack_from("<5B", data, cursor + 32)
                regions.append({"time_ms": time, "x": x, "y": y, "width": region_width,
                    "height": region_height, "alpha": alpha / 65536, "scale_x": scale_x / 65536,
                    "scale_y": scale_y / 65536, "rotation": rotation / 65536, "layer": layer,
                    "self_anchor": own_anchor, "parent_anchor": parent_anchor, "parent": parent,
                    "visible": visible, "far_anchor": far_anchor, "far_x": far_x, "far_y": far_y,
                    "callback_tag": callback_tag})
            cursor += frame_size
        record = {"object_index": index, "offset": start, "type": kind,
            "type_name": TYPE_NAMES[kind], "keyframe_count": frames,
            "keyframe_bytes": frame_size, "times_ms": times}
        if kind == 6:
            record["user_region_index"] = region_index
            record["keyframes"] = regions
            region_index += 1
        objects.append(record)
    if cursor != len(data):
        return None
    return {"width": width, "height": height, "duration_ms": duration,
        "object_count": count, "user_region_count": region_index, "bytes": cursor, "objects": objects}


def main():
    # 名字来自原二进制；名字到句柄的映射来自解包后的原 TOC。
    names = set()
    program = (ROOT / "gunbros").read_bytes()
    for match in re.finditer(rb"GLU_MOVIE_[A-Z0-9_]+\x00", program):
        name = match.group()[:-1].decode("ascii")
        if name not in ("GLU_MOVIE_MOVIE", "GLU_MOVIE__SOUNDS_"):
            names.add(name)
    movies = []
    packs = []
    type_totals = {}
    for pack, toc_id in pack_records():
        resources = extracted_resources(pack)
        toc_path = resources[toc_id][0]
        table = read_name_table(toc_path)
        base = table.get(name_key("GLU_MOVIE_MOVIE"), 0)
        if base == 0:
            continue
        aliases = {}
        for name in sorted(names):
            handle = table.get(name_key(name), 0)
            if handle:
                aliases.setdefault(handle, []).append(name)
        found_handles = set()
        count = 0
        # 全扫描结构候选，避免依靠手填的每包 Movie 数量或提前遇错结束。
        for logical_id, (path, row) in sorted(resources.items()):
            if row["sub_group"].lower() != "0xf4e02223":
                continue
            movie = read_movie(path.read_bytes())
            if movie is None:
                continue
            handle = 0x03000000 | logical_id
            if handle < base:
                raise ValueError(f"Movie-shaped resource before base: {path}")
            movie.update({"pack": pack, "ordinal": handle - base, "handle": f"0x{handle:08X}",
                "logical_id": row["logical_id"], "physical_id": int(row["physical_id"]),
                "archive_offset": row["Offset"], "path": path.relative_to(ROOT).as_posix(),
                "aliases": aliases.get(handle, [])})
            movies.append(movie)
            found_handles.add(handle)
            count += 1
            for item in movie["objects"]:
                kind = item["type"]
                type_totals[kind] = type_totals.get(kind, 0) + 1
        for handle in aliases:
            if handle not in found_handles:
                raise ValueError(f"Named movie failed structural parse: {pack} {aliases[handle]}")
        packs.append({"pack": pack, "base": f"0x{base:08X}", "count": count,
            "toc_path": toc_path.relative_to(ROOT).as_posix()})
        print(f"[ui-catalog] {pack}: movies={count}")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps({"packs": packs, "object_type_counts": type_totals,
        "movie_count": len(movies), "movies": movies}, ensure_ascii=False, indent=2), encoding="utf-8")
    write_guide(movies, packs, type_totals)
    print(f"[ui-catalog] movies={len(movies)} types={type_totals} output={OUTPUT}")


if __name__ == "__main__":
    main()
