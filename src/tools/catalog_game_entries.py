"""只读索引 BIG 数据分区，并按原读取顺序核对已写 .bt 的条目边界。"""
from collections import Counter
import json
import struct

from catalog_ui_movies import ROOT, extracted_resources, name_key, pack_records, read_name_table


OUTPUT = ROOT / "out/game-entry-catalog.json"
GUIDE = ROOT / "docs/game-entry-templates.md"
SECTIONS = (
    "ACHIEVEMENT", "ACHIEVEMENTLIST", "ARMOR", "BULLET", "DAILYBONUS",
    "ENEMY", "GUN", "LEVEL", "LEVELPROGRESSION", "MISSION", "MISSIONOBJECTIVE",
    "PARTICLEEFFECT", "PICKUP", "PLANET", "PLATFORM", "PLAYER",
    "PLAYERPROGRESSION", "POWERUP", "PRIZE", "PROP", "REFINEMENT", "SOUNDEFFECT",
    "STORE", "TILELAYER", "TILESET", "TUTORIAL", "CHALLENGE", "MP_MATCH",
    "PNG", "WAV", "MODEL", "LEVEL_REFS", "COUNTS",
)
TEMPLATES = {
    "ARMOR": "armor_template.bt", "GUN": "gun_template.bt",
    "MISSION": "mission_entry.bt", "MISSIONOBJECTIVE": "mission_objective_entry.bt",
    "PLANET": "planet_entry.bt", "POWERUP": "powerup_template.bt",
    "REFINEMENT": "refinement_entry.bt", "STORE": "store_entry.bt",
}


class Reader:
    """记录每个实际读取字段的偏移，越界或未读完即报错。"""

    def __init__(self, data):
        self.data = data
        self.position = 0
        self.fields = []

    def read(self, name, kind):
        size = struct.calcsize("<" + kind)
        value = struct.unpack_from("<" + kind, self.data, self.position)[0]
        self.fields.append({"offset": self.position, "name": name, "bytes": size, "value": value})
        self.position += size
        return value

    def raw(self, name, size):
        end = self.position + size
        if end > len(self.data):
            raise ValueError(f"Truncated {name} at {self.position}")
        value = self.data[self.position:end].hex()
        self.fields.append({"offset": self.position, "name": name, "bytes": size, "hex": value})
        self.position = end

    def asset(self, name):
        self.read(name + ".packHash", "I")
        self.read(name + ".assetOrdinal", "i")

    def object(self, name):
        pack_hash = self.read(name + ".packHash", "I")
        if pack_hash != 0:
            self.read(name + ".localOrdinal", "B")

    def sprite(self, name):
        self.read(name + ".packHash", "I")
        self.read(name + ".archetype", "B")
        self.read(name + ".action", "B")
        self.read(name + ".animation", "B")

    def table(self, name, kind="I"):
        count = self.read(name + ".count", "H")
        for index in range(count):
            self.read(f"{name}[{index}]", kind)
        return count

    def code(self, name):
        size = self.read(name + ".byteLength", "B")
        self.raw(name + ".payload", size)

    def script(self):
        """只解释 CScript 容器；字节码留原始 hex，不猜 native 调用。"""
        if self.read("script.present", "B") == 0:
            return
        self.raw("script.discardedHeader", 6)
        count = self.read("script.exportCount", "B")
        self.raw("script.exports", count)
        count = self.read("script.secondaryCount", "B")
        self.raw("script.secondary", count)
        count = self.read("script.resourceCount", "B")
        for index in range(count):
            name = f"script.resources[{index}]"
            self.read(name + ".packHash", "I")
            self.read(name + ".sectionOrTypeRaw", "B")
            self.read(name + ".resourceOrdinal", "I")
        count = self.read("script.dataBlockCount", "B")
        for index in range(count):
            name = f"script.dataBlocks[{index}]"
            length = self.read(name + ".count", "B")
            for item in range(length):
                self.read(f"{name}[{item}]", "h")
        count = self.read("script.variableCount", "B")
        for index in range(count):
            self.read(f"script.variables[{index}]", "h")
        count = self.read("script.stateCount", "B")
        for index in range(count):
            name = f"script.states[{index}]"
            self.read(name + ".parent", "B")
            length = self.read(name + ".sequenceCount", "B")
            self.raw(name + ".sequence", length)
            length = self.read(name + ".exportCount", "B")
            for item in range(length):
                self.read(f"{name}.exports[{item}].id", "B")
                self.code(f"{name}.exports[{item}].code")
            self.code(name + ".enter")
            self.code(name + ".exit")
        count = self.read("script.functionCount", "B")
        for index in range(count):
            self.code(f"script.functions[{index}]")

    def moves(self):
        self.read("moves.packHash", "I")
        count = self.read("moves.meshConfigCount", "B")
        for index in range(count):
            self.read(f"moves.meshes[{index}].meshOrdinal", "B")
            self.read(f"moves.meshes[{index}].textureOrdinal", "B")
        count = self.read("moves.moveCount", "B")
        for index in range(count):
            name = f"moves.moves[{index}]"
            self.read(name + ".configIndex", "B")
            self.read(name + ".firstFrame", "H")
            self.read(name + ".lastFrame", "H")
            self.read(name + ".restarts", "B")
            self.read(name + ".speedFixed16", "i")
            self.read(name + ".unknownRaw", "I")
            length = self.read(name + ".soundCount", "B")
            for item in range(length):
                self.read(f"{name}.sounds[{item}].frame", "H")
                self.read(f"{name}.sounds[{item}].wavOrdinal", "B")


def read_entry(section, data):
    """字段排列依据各原版 Init；模板与此索引器分别保留可阅读形式。"""
    reader = Reader(data)
    if section == "STORE":
        reader.read("category", "B")
        reader.read("flagsRaw", "B")
        reader.read("runtime8Raw", "I")
        count = reader.read("objectCount", "B")
        for index in range(count):
            reader.read(f"objects[{index}].type", "B")
            reader.object(f"objects[{index}]")
        reader.read("requiredLevel", "H")
        reader.read("commonCurrency", "I")
        reader.read("rareCurrency", "I")
        reader.read("currencyDirectionOrRaw", "B")
        for name in ("asset0Raw", "icon", "name", "description", "expandedText", "foldedText"):
            reader.asset(name)
        for index in range(8):
            reader.table(f"statGroups[{index}]", "i")
        reader.read("displayOrder", "h")
        reader.read("runtime242Raw", "B")
        reader.read("singlePurchase", "B")
        reader.read("runtime244Raw", "B")
    elif section == "ARMOR":
        reader.read("slot", "B")
        for index in range(2):
            reader.asset(f"mesh{index}")
            reader.asset(f"image{index}")
            reader.read(f"attachmentNode{index}", "B")
        reader.asset("spriteImage0")
        reader.asset("spriteImage1")
        reader.script()
    elif section == "POWERUP":
        reader.asset("name")
        reader.sprite("sprite")
        reader.read("runtime28Raw", "B")
        reader.read("runtime29Raw", "B")
        reader.script()
        reader.read("runtime30Raw", "B")
        reader.read("runtime112Raw", "B")
        reader.object("effect")
        reader.read("runtime124Raw", "B")
    elif section == "PLANET":
        reader.asset("name")
        reader.asset("description")
        reader.read("layoutSelectorRaw", "H")
        for name in ("mapSprite", "detailSprite", "sprite44"):
            reader.sprite(name)
        count = reader.read("missionCount", "B")
        for index in range(count):
            reader.object(f"missions[{index}]")
        reader.object("object12Raw")
        reader.read("requiredLevel", "H")
    elif section == "MISSION":
        for name in ("title", "description", "requirements", "overview"):
            reader.asset(name)
        reader.object("level")
        count = reader.read("objectiveCount", "B")
        for index in range(count):
            reader.object(f"objectives[{index}]")
        reader.script()
        reader.read("missionType", "B")
        reader.read("runtime64Raw", "H")
        reader.read("runtime66Raw", "B")
    elif section == "MISSIONOBJECTIVE":
        reader.asset("title")
        reader.asset("description")
        reader.read("type", "B")
        reader.read("runtime32Raw", "H")
        reader.read("runtime36Raw", "H")
    elif section == "GUN":
        reader.read("category", "B")
        reader.asset("mesh")
        reader.asset("image")
        reader.object("defaultBullet")
        reader.read("fireIntervalMs", "H")
        reader.read("scalar144Fixed16", "i")
        reader.read("criticalDamageScale", "H")
        reader.read("scalar152Fixed16", "i")
        reader.read("handedness", "B")
        reader.script()
        for index in range(6):
            reader.table(f"masteryTables[{index}]")
        reader.moves()
    elif section == "REFINEMENT":
        reader.table("minutes")
        reader.table("efficiencyPercent")
        count = reader.table("commonPrice")
        reader.read("repeatedRareCount", "H")
        for index in range(count):
            reader.read(f"rarePrice[{index}]", "I")
        reader.table("gate")
    else:
        raise ValueError(f"No reader: {section}")
    if reader.position != len(data):
        raise ValueError(f"{section}: read {reader.position}, file {len(data)}")
    return reader.fields


def field_values(entry):
    values = {}
    for field in entry.get("fields", []):
        if "value" in field:
            values[field["name"]] = field["value"]
    return values


def entry_link(entry):
    return f"[{entry['pack']} / {entry['section']}[{entry['ordinal']}]](<../{entry['path']}>)"


def write_guide(records, totals):
    """把全分区清单、模板入口及银行/星球实际样本汇总成人可读说明。"""
    lines = [
        "# Gun Bros 数据条目与 Binary Template", "",
        "范围：当前 `big_360_out` 的 13 个 xga 包，依据 iOS 3.6.0 原读取器定位。",
        "本页区分实体模板、商店条目和界面 Movie；它们共同参与组装，不能用一个 UI bin 代替全部数据。", "",
        "## 本次方案与验收", "",
        "按用户已确认的用途制作辅助理解的 010 Editor `.bt`，不要求实际运行。先从原 Init 核对字段，再用原 TOC/Keyset 定位文件，最后只读检查序列化边界。未知语义保留 Raw，不补造字段。", "",
        "各模板已按原代码的读取、赋值和消费路径补充逐字段注释，包含用途、单位、条件及源码行号。旧 Raw 字段名为便于对应目录而保留，是否已确认请看字段注释；主要发现与未解项见 [字段用途核对](entry-field-semantics.md)。", "",
        "本页保留第一阶段8类实体/商品的详细样本和33分区索引。后续两小时研究已补其他实体、几何、精灵、字节码及存档；最新完整覆盖见[全资源入口](binary-template-catalog.md)和[存档入口](save-binary-templates.md)。下表‘仅定位’仅指本索引工具，不代表后续没有解析。", "",
        "## 模板与实际样本", "",
        "| 数据类型 | 非空条目数 | 本批长度（字节） | 模板 | 实际样本 |",
        "|---|---:|---|---|---|",
    ]
    parsed_count = 0
    for section, template in TEMPLATES.items():
        total = totals[section]
        parsed_count += total["parsed"]
        sizes = sorted(total["bytes"])
        size_text = f"{sizes[0]}–{sizes[-1]}（{len(sizes)} 种）"
        if len(sizes) <= 6:
            size_text = ", ".join(map(str, sizes))
        sample = None
        for entry in records:
            if entry["section"] == section and entry["bytes"] > 0:
                sample = entry
                break
        lines.append(f"| {section} | {total['nonempty']} | {size_text} | [{template}](<../_Big_tool/binary template/big_assets/entries/{template}>) | {entry_link(sample)} |")
    lines.extend([
        "", "[common.bt](<../_Big_tool/binary template/big_assets/entries/common.bt>) 定义 AssetRef、ObjectRef、ObjectTypeRef、SpriteGluRef、CScript 容器和 MoveSetMesh，不单独作为文件入口。",
        "[bank_entry.bt](<../_Big_tool/binary template/big_assets/entries/bank_entry.bt>) 复用 store_entry.bt 并提示银行分类；银行不是第 34 个分区，也不另造 BankTemplate 格式。", "",
        "## 用户 Weapon 图与原格式的对应", "",
        "该图的结构对应 `CStoreItem::Init`，也就是 `23_STORE` 的商品卡条目。实体枪械在 `07_GUN`；两者都有数据，但职责不同。", "",
    ])
    for entry in records:
        if entry["section"] == "STORE" and entry["pack"] == "pack3_xga" and entry["ordinal"] == 39:
            lines.append("截图对应的商店样本：" + entry_link(entry) + "，221 字节。")
        if entry["section"] == "GUN" and entry["pack"] == "pack5_xga" and entry["ordinal"] == 39:
            lines.append("其中引用的枪械实体：" + entry_link(entry) + f"，{entry['bytes']} 字节。")
    lines.extend([
        "", "- `06 85 75 26 00 27` 是 type=6、pack5、局部序号39的实体引用。它绑定枪械模板，不能直接叫作第39张贴图。枪械模板再引用模型、贴图、子弹、脚本和动作。",
        "- 名称、简介、图标等 `AssetRef` 固定 8 字节（u32包哈希 + i32编号）。`ObjectRef` 则为 4/5 字节，`ObjectTypeRef` 为 5/6 字节；区别来自引用类型及空包哈希，不是编号按数值压缩成1–4字节。",
        "- 单个有效对象引用、八组各四个数值时，长度为 `7+6+11+48+8×18+5=221=0xDD`。礼包可能引用多个对象；银行条目没有对象引用。本批339条 STORE 中313条为221字节，其余有不同长度。",
        "- 图中的价格、商品顺序、文本引用及分组数值确实来自 BIG。原代码解析它们并执行购买/装备/显示逻辑；不能改成手写一套数值。",
        "- 当前233个 ARMOR 样本恰好全为86字节；格式仍包含变长脚本。它的 DEF/ATK/SPD 等装备属性由脚本设置，不应把商品卡的数值数组复制进 ArmorTemplate。", "",
        "## 银行实际条目", "",
        "以下18条均在 pack3 的 STORE 分区，文件长度均为215字节，objectCount=0。category14/15的货币字段是发放数量；不是美元等现实币种价格。category16的方向字节决定扣哪种游戏货币。", "",
        "| 条目文件 | category | 金币字段 | Warbucks字段 | 方向字节 |", "|---|---:|---:|---:|---:|",
    ])
    categories = Counter()
    for entry in records:
        if entry["section"] != "STORE" or entry["bytes"] == 0:
            continue
        values = field_values(entry)
        category = values["category"]
        categories[category] += 1
        if category >= 14:
            lines.append(f"| {entry_link(entry)} | {category} | {values['commonCurrency']} | {values['rareCurrency']} | {values['currencyDirectionOrRaw']} |")
    lines.extend([
        "", "原 `CurrencyPurchase`：category14发金币、15发Warbucks（可能乘促销比例）；16且方向非零时扣金币加Warbucks，否则扣Warbucks加金币。", "",
        "商店分类计数（包含隐藏或特殊条目，不代表当前可购买数量）：", "",
        "| 分类范围 | 类型 | 条目数 |", "|---|---|---:|",
    ])
    for first, last, label in ((0, 6, "枪械商品"), (7, 9, "盔甲商品"), (10, 13, "道具商品"), (14, 16, "银行商品")):
        count = 0
        for category in range(first, last + 1):
            count += categories[category]
        lines.append(f"| {first}–{last} | {label} | {count} |")
    lines.extend([
        "", "## 星球与选关数据", "",
        "Planet 是星球数据条目，不是一整个星图界面的模板。它保存名称/描述、精灵引用、任务列表及等级要求。Mission 再保存任务文本、关卡引用与编译脚本；布局和动画另查 [UI Movie 索引](ui-binary-templates.md)。", "",
        "```mermaid", "flowchart LR", '    UI["菜单代码 + CMovie布局/动画"] --> Planet["PLANET 星球条目"]',
        '    Planet --> Sprite["SpriteGlu 精灵/图片"]', '    Planet --> Mission["MISSION 任务条目"]',
        '    Mission --> Level["LEVEL 关卡模板/地图"]', '    Mission --> Script["嵌入 CScript"]',
        '    Mission --> Objective["MISSIONOBJECTIVE 可选目标"]', "```", "",
        "本批有5条97字节、1条46字节的 Planet。46字节样本含0个 missions 和空的额外引用；它仍是有结构的条目，不能当空文件或据此直接断定废案。", "",
        "| 文件 | 字节 | 星图槽位编号 | missions数量 | 等级要求 |", "|---|---:|---:|---:|---:|",
    ])
    for entry in records:
        if entry["section"] == "PLANET" and entry["bytes"] > 0:
            values = field_values(entry)
            lines.append(f"| {entry_link(entry)} | {entry['bytes']} | {values['layoutSelectorRaw']} | {values['missionCount']} | {values['requiredLevel']} |")
    lines.extend([
        "", "图中第三个精灵引用的 `06 06 FF` 是三个明确读取的字节（archetype/action/animation），不是未知长度填充。iOS版本还在任务引用列表后读取额外 ObjectRef，再读 u16 等级要求。额外引用的精确用途尚未在本次核定，保留 object12Raw。",
        "用户图中的旧PC样本保留为参考；本模板没有把未经独立确认的PC版本号或长度规则套给所有版本。", "",
        "## 全部分区盘点", "",
        "位置按原 `___GAME_TOC_KEYSET` 前33个 handle 的低15位确定；相邻分区基址给出逻辑ID范围。再用 resources.csv 的 logical_id → physical_id/Offset 定位解包文件。没有仅凭现有文件夹标签推断类型。",
        "空占位指原始大小0的资源；解包工具可能为其写出非空文本 .ref，不能把这段文本当实体负载。数量按逻辑条目计数，不作跨包去重。", "",
        "| Section | 原分区名 | 非空条目 | 空占位 | 本次逐字段核对 |", "|---:|---|---:|---:|---|",
    ])
    for index, section in enumerate(SECTIONS):
        total = totals[section]
        status = "仅定位"
        if section in TEMPLATES:
            status = f"{total['parsed']} / {total['nonempty']}"
        lines.append(f"| {index + 1} | {section} | {total['nonempty']} | {total['empty']} | {status} |")
    lines.extend([
        "", "这33分区是游戏对象 Keyset，不等于整个 BIG 的所有资源类别。CMovie、SpriteGlu、字体、字符串包等还通过其他 TOC 键定位。", "",
        "[完整索引及字段偏移](../out/game-entry-catalog.json) 包含以上各分区全部文件路径；已解析类型另有逐字段 offset、bytes、原始值与脚本负载hex。", "",
        "## 验证及边界", "",
        f"执行 `D:/Python312/python.exe src/tools/catalog_game_entries.py`，退出码0。8类共 **{parsed_count} 个非空条目** 均按所列结构完整读取至文件末尾，并核对解包字节数与CSV原大小一致。",
        "这是结构边界核对，不代表每个 Raw 字段的语义已经确认，也不代表所有脚本指令已反编译。按用户要求未启动010 Editor，未测试.bt运行兼容性。未修改原始BIG、解包文件或游戏UI。", "",
        "原版依据均为 `_IDA_OUT/gunbros_3.6.0_IOS.c`：CStoreItem::Init :159834；CurrencyPurchase :155009；CArmor::Template::Init :176438；CGun::Template::Init :127712；CPowerup::Template::Init :187947；Planet::Init :169935；Mission::Init :164402；MissionObjective::Init :175995；CRefinementManager::Template::Init :177773。共用结构来源另见 common.bt。", "",
    ])
    GUIDE.write_text("\n".join(lines), encoding="utf-8")


def main():
    records = []
    totals = {}
    for section in SECTIONS:
        totals[section] = {"nonempty": 0, "empty": 0, "bytes": Counter(), "parsed": 0}
    for pack, toc_id in pack_records():
        resources = extracted_resources(pack)
        names = read_name_table(resources[toc_id][0])
        keyset_handle = names[name_key("___GAME_TOC_KEYSET")]
        keyset = resources[keyset_handle & 0x7FFF][0].read_bytes()
        bases = struct.unpack_from("<33I", keyset, 2)
        for index, section in enumerate(SECTIONS):
            first_id = bases[index] & 0x7FFF
            last_id = first_id + 1
            if index < 32:
                last_id = bases[index + 1] & 0x7FFF
            for resource_id in range(first_id, last_id):
                path, row = resources[resource_id]
                size = int(row["original size"])
                record = {
                    "pack": pack, "section": section, "section_id": index + 1,
                    "ordinal": resource_id - first_id, "logical_id": resource_id,
                    "handle": f"0x{bases[index] + resource_id - first_id:08X}",
                    "path": path.relative_to(ROOT).as_posix(), "bytes": size,
                }
                if size == 0:
                    totals[section]["empty"] += 1
                else:
                    totals[section]["nonempty"] += 1
                    totals[section]["bytes"][size] += 1
                    if section in TEMPLATES:
                        data = path.read_bytes()
                        if len(data) != size:
                            raise ValueError(f"Size mismatch: {path}")
                        try:
                            record["fields"] = read_entry(section, data)
                        except (ValueError, struct.error) as error:
                            raise ValueError(f"{pack} {section}[{record['ordinal']}] {path}: {error}") from error
                        record["template"] = TEMPLATES[section]
                        totals[section]["parsed"] += 1
                records.append(record)
        print(f"Indexed {pack}")
    OUTPUT.parent.mkdir(exist_ok=True)
    OUTPUT.write_text(json.dumps({"sections": totals, "entries": records}, ensure_ascii=False, indent=2), encoding="utf-8")
    write_guide(records, totals)
    for section, total in totals.items():
        print(f"{section}: nonempty={total['nonempty']} empty={total['empty']} parsed={total['parsed']} sizeKinds={len(total['bytes'])}")
    print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    main()
