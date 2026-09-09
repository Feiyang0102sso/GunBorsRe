"""把已验证的全部内嵌Flow代码转成带偏移和原函数证据的可读清单，不执行脚本。"""
from pathlib import Path
import json

from catalog_ui_movies import ROOT
from flow_native_notes import NATIVE_NOTES


RESEARCH = ROOT / "out/binary-research"
OUTPUT = RESEARCH / "flow-disassembly"
ARMOR_VARIABLES = ("DEF", "ATK", "SPD", "XP", "Xplodium")


def operand_text(operand):
    """显示运行时操作数种类；资源槽和定点单位必须由具体native参数决定。"""
    meaning = operand["meaning"]
    kind = meaning["kind"]
    if kind == "immediate":
        text = str(meaning["value"])
    elif kind == "local_variable":
        text = f"local[{meaning['slot']}]"
    elif kind == "host_variable":
        text = f"{meaning['class']}.variable[{meaning['slot']}]"
        if meaning["class"] == "CArmor" and meaning["slot"] < len(ARMOR_VARIABLES):
            text = "CArmor." + ARMOR_VARIABLES[meaning["slot"]]
    else:
        text = f"interpreter.short_at[{meaning['interpreter_offset']}]"
    return f"{text}<{operand['token']}>"


def call_text(call):
    arguments = []
    for argument in call["arguments"]:
        arguments.append(operand_text(argument))
    function = "internal_" + call["function_id"]
    if "native_slot" in call:
        function = f"{call['class']}.native_{call['function_id']}"
    return function + "(" + ", ".join(arguments) + ")"


def code_lines(block, indentation=0):
    """递归展开子块，保留条件链/事件主体，不进行反编译优化或猜测变量名。"""
    lines = []
    prefix = "    " * indentation
    lines.append(f"{prefix}// 块 @0x{block['offset']:X}，长度字节={block['byte_length']}（不含长度自身）")
    for statement in block["statements"]:
        lead = f"{prefix}@0x{statement['offset']:X} "
        opcode = statement["opcode"]
        if opcode == 0:
            lines.append(lead + call_text(statement["call"]))
        elif opcode == 1:
            lines.append(lead + f"set_state({statement['set_state']}) // 本块返回，状态改变的后续行为看解释器")
        elif opcode == 2:
            target = operand_text(statement["destination"])
            if "call" in statement:
                lines.append(lead + target + " = " + call_text(statement["call"]))
            else:
                source = operand_text(statement["operand"])
                if "source_data_block" in statement:
                    source = f"dataBlock[{statement['source_data_block']}][{source}]"
                operator = statement["operator"]
                lines.append(lead + f"{target} {operator} {source}")
                if operator in ("++", "--"):
                    lines.append(prefix + "// 原编码仍存source token；++/--语义按解释器执行，不能删掉该磁盘操作数。")
        elif opcode == 3:
            lines.append(lead + f"event({statement['event_id']}) {{ // 普通Execute跳过，Evaluate按事件条件进入")
            lines.extend(code_lines(statement["body"], indentation + 1))
            lines.append(prefix + "}")
        elif opcode == 4:
            for index, branch in enumerate(statement["branches"]):
                label = "if"
                if index > 0:
                    label = "else_if"
                condition = operand_text(branch["left"]) + " " + branch["comparison"] + " " + operand_text(branch["right"])
                lines.append(lead + label + " (" + condition + ") {")
                lines.extend(code_lines(branch["body"], indentation + 1))
                lines.append(prefix + f"}} // 后续分支标志={branch['continuation']}")
        elif opcode == 5:
            lines.append(lead + "return " + operand_text(statement["return"]))
    return lines


def main():
    flow = json.loads((RESEARCH / "flow-bytecode-catalog.json").read_text(encoding="utf-8"))
    native = json.loads((RESEARCH / "flow-native-sources.json").read_text(encoding="utf-8"))
    source_by_id = {}
    source_by_class = {}
    for resolver in native["resolvers"]:
        if resolver["kind"] != "function":
            continue
        source_by_class[resolver["class_id"]] = resolver
        for branch in resolver["branches"]:
            source_by_id[branch["function_id"]] = branch
    fields_by_path = {}
    for path in (ROOT / "out/game-entry-catalog.json", RESEARCH / "section-catalog.json"):
        catalog = json.loads(path.read_text(encoding="utf-8"))
        for entry in catalog["entries"]:
            if "fields" in entry:
                fields_by_path[entry["path"]] = entry["fields"]
    OUTPUT.mkdir(exist_ok=True)
    index = ["# 全部内嵌 Flow 可读清单", "",
             "由实际BIG代码字节解码，保留每条语句和操作数位置。只是辅助阅读的伪代码，不是原始.flow源码，也不执行游戏逻辑。",
             "共675个文件、6187个顶层代码块；嵌套条件和事件一并展开。每份文件前部列导出映射、资源依赖槽、变量初值、状态关系，后部连接用到的原生函数分支。", "",
             "`<0x....>`保留操作数token。常量数字本身不含单位；8.8定点、角度、毫秒、资源槽由native消费处解释。CArmor五个属性名有原变量地址及消费者依据，其余宿主变量保留类名和槽号。",
             "编译数据不含局部变量和内部函数原名，因此不虚构名称。旧.link的函数名没有用于给清单中的native自动改名。",
             "来源：`CScriptCode::Execute :106801`、`CScriptInterpreter::GetData :107424`以及[flow_bytecode.bt](<../_Big_tool/binary template/big_assets/flow_bytecode.bt>)。", "",
             "| 类型 | 原文件 | 带注释清单 |", "|---|---|---|"]
    for entry in flow["entries"]:
        original = Path(entry["path"])
        output = OUTPUT / (original.stem + ".flow.txt")
        lines = ["// 原资源：" + entry["path"], "// 类型：" + entry["section"],
                 "// 本清单从已通过边界核对的字节码生成；不是恢复出的原变量名，也不是可执行源码。",
                 "// @0x位置均为原bin绝对偏移；源码:行号均为_IDA_OUT/gunbros_3.6.0_IOS.c。", "",
                 "// ===== 容器、依赖、初值和状态元数据 ====="]
        for field in fields_by_path[entry["path"]]:
            name = field["name"]
            if not name.startswith("script.") or name.endswith(".payload") or name.endswith(".byteLength"):
                continue
            value = field.get("value")
            if value is None:
                value = "hex:" + field.get("hex", "")
            lines.append(f"// @0x{field['offset']:X} {name} = {value} （{field['bytes']}字节）")
        for block in entry["blocks"]:
            lines.extend(["", "// ===== " + block["container_field"] + " ====="])
            lines.extend(code_lines(block))
        function_ids = set()
        for call in entry["calls"]:
            if "native_slot" in call:
                function_ids.add(call["function_id"])
        lines.extend(["", "// ===== 本文件使用的原生函数及参数证据 ====="])
        for function_id in sorted(function_ids):
            class_id = int(function_id, 16) >> 8
            resolver = source_by_class[class_id]
            line = resolver["line"]
            if function_id in source_by_id:
                line = source_by_id[function_id]["line"]
            lines.append(f"// {function_id} -> {resolver['signature']} :{line}")
            lines.append("// 原文件：" + resolver.get("original_source", "符号未恢复"))
            for note in NATIVE_NOTES.get(function_id, []):
                lines.append("// " + note)
        output.write_text("\n".join(lines) + "\n", encoding="utf-8")
        relative = output.relative_to(ROOT).as_posix()
        index.append(f"| {entry['section']} | [{original.name}](<../{entry['path']}>) | [阅读](<../{relative}>) |")
    (ROOT / "docs/flow-bytecode-reading.md").write_text("\n".join(index) + "\n", encoding="utf-8")
    print(f"Rendered {len(flow['entries'])} Flow listings")


if __name__ == "__main__":
    main()
