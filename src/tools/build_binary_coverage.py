"""合并原资源逐文件验证证据，生成模板总入口；覆盖与字段语义置信度分别表达。"""
from pathlib import Path
from collections import Counter
import json


ROOT = Path(__file__).resolve().parents[2]
RESEARCH = ROOT / "out/binary-research"
TEMPLATE_ROOT = "_Big_tool/binary template/"
TEMPLATES = {
    "ACHIEVEMENT": "big_assets/entries/empty_sections.bt",
    "ACHIEVEMENTLIST": "big_assets/entries/empty_sections.bt",
    "LEVELPROGRESSION": "big_assets/entries/empty_sections.bt",
    "PLATFORM": "big_assets/entries/platform_template.bt",
    "ARMOR": "big_assets/entries/armor_template.bt",
    "GUN": "big_assets/entries/gun_template.bt",
    "BULLET": "big_assets/entries/bullet_template.bt",
    "DAILYBONUS": "big_assets/entries/daily_bonus_template.bt",
    "ENEMY": "big_assets/enemy_template.bt",
    "LEVEL": "big_assets/entries/level_template.bt",
    "MISSION": "big_assets/entries/mission_entry.bt",
    "MISSIONOBJECTIVE": "big_assets/entries/mission_objective_entry.bt",
    "PARTICLEEFFECT": "big_assets/entries/particle_effect.bt",
    "PICKUP": "big_assets/entries/pickup_template.bt",
    "PLANET": "big_assets/entries/planet_entry.bt",
    "PLAYER": "big_assets/entries/player_template.bt",
    "PLAYERPROGRESSION": "big_assets/entries/player_progression.bt",
    "POWERUP": "big_assets/entries/powerup_template.bt",
    "PRIZE": "big_assets/entries/prize_entry.bt",
    "PROP": "big_assets/entries/prop_template.bt",
    "REFINEMENT": "big_assets/entries/refinement_entry.bt",
    "SOUNDEFFECT": "big_assets/entries/sound_effect.bt",
    "STORE": "big_assets/entries/store_entry.bt",
    "TILELAYER": "big_assets/maps/map.bt",
    "TILESET": "big_assets/maps/tileset.bt",
    "TUTORIAL": "big_assets/entries/tutorial_entry.bt",
    "CHALLENGE": "big_assets/entries/challenge_template.bt",
    "MP_MATCH": "big_assets/entries/mp_match_template.bt",
    "MODEL": "big_assets/mesh.bt",
    "LEVEL_REFS": "big_assets/level_requirements.bt",
    "COUNTS": "big_assets/object_counts.bt",
    "MOVIE": "big_assets/ui_movie.bt",
    "SPRITE_ARCHETYPE": "big_assets/sprite_archetype.bt",
    "SPRITE_GLOBAL": "big_assets/sprite_global.bt",
    "TEXTURE_MAP": "big_assets/sprite_texture_map.bt",
    "TEXTURE_MAP_GLOBAL": "big_assets/sprite_texture_pages.bt",
    "NAME_TABLE": "big_assets/name_table.bt",
    "INIT_DATA": "big_assets/init_data.bt",
    "KEYSET": "big_assets/big_keyset.bt",
    "STRING_AGGREGATE": "big_assets/string_pack.bt",
    "BITMAP_FONT": "big_assets/bitmap_font.bt",
    "PNG": "big_assets/png_texture.bt",
    "PNG_OUTSIDE_GAME_SECTION": "big_assets/png_texture.bt",
    "WAV": "big_assets/wav_audio.bt",
    "WAV_OUTSIDE_GAME_SECTION": "big_assets/wav_audio.bt",
    "APP_PROPERTIES_TEXT": "big_assets/text_resource.bt",
    "PACK_LABEL_UTF8": "big_assets/text_resource.bt",
    "EMPTY_RESOURCE_PLACEHOLDER": "big_assets/entries/empty_sections.bt",
}
CATALOGS = (
    ("out/game-entry-catalog.json", "entries"),
    ("out/ui-movie-catalog.json", "movies"),
    ("out/binary-research/section-catalog.json", "entries"),
    ("out/binary-research/geometry-catalog.json", "entries"),
    ("out/binary-research/sprite-catalog.json", "entries"),
    ("out/binary-research/container-catalog.json", "entries"),
    ("out/binary-research/standard-catalog.json", "entries"),
)


def load(path):
    return json.loads((ROOT / path).read_text(encoding="utf-8"))


def write_guide(records, counts, anomalies, missing, empty_count):
    template_count = 0
    for path in (ROOT / TEMPLATE_ROOT).rglob("*.bt"):
        template_count += 1
    extension_counts = Counter()
    for record in records:
        extension_counts[Path(record["path"]).suffix.lower()] += 1
    lines = [
        "# 全资源 Binary Template 阅读入口", "",
        "范围：`big_360_out/`全部13包，以及`saves/`全部21个原存档。以iOS原程序和实际字节为依据；模板用于辅助理解，不要求010 Editor运行兼容性。", "",
        f"当前 **{len(records)} 个实际资源文件**全部归类：**{len(records)-empty_count} 个非空资源、{empty_count} 个零字节资源的.ref占位**。已提供 **{template_count} 份.bt**（包含共享定义、银行入口、标准格式及源码查找说明）；一个格式模板会覆盖多个bin。", "",
        f"按实际扩展名：{extension_counts['.bin']}个.bin、{extension_counts['.png']}个PNG、{extension_counts['.wav']}个WAV、{extension_counts['.txt']}个文本、{extension_counts['.ref']}个.ref。另有26个解包CSV元数据文件，不算游戏资源。", "",
        f"非空资源中 **{len(records)-empty_count-len(anomalies)-len(missing)} 个通过对应结构/格式核对，{len(anomalies)} 个保留明确的布局异常，{len(missing)} 个缺少解析证据**。通过核对表示长度、计数与结构边界一致，不表示每个字段的游戏语义、每条动画或脚本行为都已证明。", "",
        "## 从哪里开始读", "",
        "- 界面布局与动画：[ui_movie.bt](<../_Big_tool/binary template/big_assets/ui_movie.bt>)；具体175个Movie的名字和文件见[UI索引](ui-binary-templates.md)。",
        "- 图像组装：[sprite_global.bt](<../_Big_tool/binary template/big_assets/sprite_global.bt>) → [sprite_archetype.bt](<../_Big_tool/binary template/big_assets/sprite_archetype.bt>) → [sprite_texture_map.bt](<../_Big_tool/binary template/big_assets/sprite_texture_map.bt>) → PNG。",
        "- 商店与实体：[STORE](<../_Big_tool/binary template/big_assets/entries/store_entry.bt>)、[ARMOR](<../_Big_tool/binary template/big_assets/entries/armor_template.bt>)、[GUN](<../_Big_tool/binary template/big_assets/entries/gun_template.bt>)、[POWERUP](<../_Big_tool/binary template/big_assets/entries/powerup_template.bt>)；银行是STORE分类14–16，见[bank_entry.bt](<../_Big_tool/binary template/big_assets/entries/bank_entry.bt>)，没有另造银行磁盘格式。",
        "- 星球/选关：[PLANET](<../_Big_tool/binary template/big_assets/entries/planet_entry.bt>) → MISSION → LEVEL → TILELAYER；玩家解锁/完成情况来自存档。",
        "- 行为脚本：[common.bt](<../_Big_tool/binary template/big_assets/entries/common.bt>)解释CScript容器；[flow_bytecode.bt](<../_Big_tool/binary template/big_assets/flow_bytecode.bt>)解释操作码和操作数；[flow_native_sources.bt](<../_Big_tool/binary template/big_assets/flow_native_sources.bt>)按宿主class/函数ID列出原生参数读取代码。",
        "- 存档：[存档索引和封装说明](save-binary-templates.md)；不要把BIG的4/5字节ObjectRef套到存档8字节内存引用。", "",
        "## 数据与引擎的边界", "",
        "BIG包含布局时间线、精灵动画树、图集映射、商店文字/价格/数值表、实体参数、编译脚本、地图和模型；一个bin通常只是其中一种记录或组件。菜单代码负责取哪些条目、建立列表、给用户区域绑定输入、查询存档状态、触发Movie片段；脚本又调用原生移动、碰撞、射击等行为。引擎既有解析器，也有具体游戏逻辑，不能用‘一份bin完整规定一切’或‘数据都在代码里’概括。", "",
        "可以读到的原版数据应从原资源读取；运行时计算值、派生几何和原生控制逻辑应按源码实现。未知字段保留原位值与查证边界，不能拿重建代码中的手写常量证明原版数据。", "",
        "## 全类别覆盖表", "",
        "数量按物理资源文件，不对内容去重。‘空’取resources.csv的原始大小，.ref文字不是原负载。", "",
        "| 类别 | 非空 | 空 | 模板 | 核对结果 |", "|---|---:|---:|---|---|",
    ]
    for category in sorted(counts):
        count = counts[category]
        template = TEMPLATE_ROOT + TEMPLATES[category]
        status = "结构/格式核对通过"
        if count["nonempty"] == 0:
            status = "无非空样本；见源码/占位说明"
        if count["anomalies"]:
            status = f"{count['nonempty']-count['anomalies']}通过，{count['anomalies']}布局异常"
        lines.append(f"| {category} | {count['nonempty']} | {count['empty']} | [{Path(template).name}](<../{template}>) | {status} |")
    lines.extend(["", "## 异常与验证边界", ""])
    for anomaly in anomalies:
        lines.append(f"- [{Path(anomaly['path']).name}](<../{anomaly['path']}>): {anomaly['reason']}")
    lines.extend([
        "- 两个Sprite原型只确认与当前读取器不兼容；尚未确认是其他版本、损坏还是未消费资源。异常后的字段值是失败位置记录，不是认可的有效结构。",
        "- PROP异常尾部原样保留；moveCount=4而后面只有4字节，原读取器至少需要20字节。没有通过改长度规则掩盖异常。",
        "- PLATFORM虽然无当前非空样本，原读取器明确读取7字节SpriteGluRef；模板单列为源码分支。ACHIEVEMENT/ACHIEVEMENTLIST无工厂分配分支；LEVELPROGRESSION仅找到4字节运行时Progression对象，没有虚构磁盘表。",
        "- 几何解析核对索引范围及每帧字节边界，大型float数组保留偏移/元素数，未逐个渲染或证明所有浮点值在游戏里可用。PNG逐块CRC通过；WAV全部为PCM，核对RIFF尺寸和块对齐。",
        "- Sprite映射/原型依赖包级计数；不能脱离global文件仅靠单个bin推出所有外部参数。UI Movie的文字轨、地图Movie层/PathLink区域等无本批样本的分支以源码证据标注。",
        "- 本次未修改游戏实现、原BIG或原存档；没有运行010 Editor。原模板的用户注释保留，新增‘修正’说明指出旧猜测的边界。", "",
        "## 逐文件和逐字段证据", "",
        "[完整覆盖JSON](../out/binary-research/coverage.json)逐文件给出类型、模板、验证状态与字段目录位置；使用`path`搜索文件名即可。", "",
        "| 证据 | 内容 |", "|---|---|",
    ])
    for path, key in CATALOGS:
        lines.append(f"| [{Path(path).name}](../{path}) | {key}数组；使用path匹配原资源，含实际数值/偏移或格式验证信息 |")
    lines.extend([
        "| [flow-bytecode-catalog.json](../out/binary-research/flow-bytecode-catalog.json) | 675个含代码文件，6187个顶层代码块，包含嵌套语句和原生调用位置 |",
        "| [flow-native-sources.json](../out/binary-research/flow-native-sources.json) | 实际原生调用ID、原类、读取参数代码与源码行号 |",
        "| [全部Flow可读清单](flow-bytecode-reading.md) | 675份逐文件伪代码，保留指令偏移、操作数token、状态/依赖、已证实native参数注释 |",
        "| [template-source-links.json](../out/binary-research/template-source-links.json) | 每份模板的原函数、ARMv7地址、N_SO原工程文件路径 |",
        "| [source-index.json](../out/binary-research/source-index.json) | 原二进制符号与反编译函数导航 |", "",
        "模板中`:行号`均指`_IDA_OUT/gunbros_3.6.0_IOS.c`；`mem+N`是原对象内存偏移；JSON的`offset`才是对应文件的偏移。原工程文件名来自调试符号，原.cpp行号未恢复，二者不能混写。", "",
        "完整执行命令与退出码见[本轮记录](binary-research-progress.md)。", "",
    ])
    (ROOT / "docs/binary-template-catalog.md").write_text("\n".join(lines), encoding="utf-8")


def main():
    evidence = {}
    for catalog_path, key in CATALOGS:
        catalog = load(catalog_path)
        for index, entry in enumerate(catalog[key]):
            if catalog_path == "out/game-entry-catalog.json" and "fields" not in entry:
                continue
            evidence[entry["path"]] = {"catalog": catalog_path, "entry_array": key, "entry_index": index,
                                       "source_status": entry.get("status"), "error": entry.get("error")}
    inventory = load("out/binary-research/resource-inventory.json")
    counts = {}
    anomalies = []
    missing = []
    records = []
    indexed_paths = set()
    empty_count = 0
    for resource in inventory["resources"]:
        record = dict(resource)
        category = record["category"]
        record["template"] = TEMPLATE_ROOT + TEMPLATES[category]
        if not (ROOT / record["template"]).is_file():
            raise ValueError(f"Missing template: {record['template']}")
        count = counts.setdefault(category, {"nonempty": 0, "empty": 0, "anomalies": 0})
        indexed_paths.add(record["path"])
        if record["bytes"] == 0:
            count["empty"] += 1
            empty_count += 1
            record["validation"] = "empty_resource_placeholder"
        else:
            count["nonempty"] += 1
            proof = evidence.get(record["path"])
            record["evidence"] = proof
            record["validation"] = "structure_validated"
            if proof is None:
                record["validation"] = "missing_parser_evidence"
                missing.append(record["path"])
            elif proof["error"] or proof["source_status"] in ("truncated_original_move_bodies", "incompatible_with_current_reader"):
                record["validation"] = "source_layout_anomaly"
                count["anomalies"] += 1
                reason = proof["error"]
                if reason is None:
                    reason = proof["source_status"]
                anomalies.append({"path": record["path"], "reason": reason})
        records.append(record)
    actual_paths = set()
    for path in (ROOT / "big_360_out").rglob("*"):
        if path.is_file() and path.suffix != ".csv":
            actual_paths.add(path.relative_to(ROOT).as_posix())
    if actual_paths != indexed_paths:
        raise ValueError(f"Physical coverage differs: unindexed={actual_paths-indexed_paths}; missing={indexed_paths-actual_paths}")
    result = {"resource_count": len(records), "nonempty_count": len(records)-empty_count, "empty_count": empty_count,
              "anomalies": anomalies, "missing_parser_evidence": missing, "categories": counts, "resources": records}
    (RESEARCH / "coverage.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    write_guide(records, counts, anomalies, missing, empty_count)
    print(f"Coverage: {len(records)} files, {empty_count} empty, {len(anomalies)} anomalies, {len(missing)} missing parsers")
    if missing:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
