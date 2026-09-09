"""把实际Flow调用编号接到原FunctionResolver分支、参数读取行和原工程文件。"""
from collections import Counter
import json
import re

from catalog_flow_bytecode import HOSTS
from catalog_ui_movies import ROOT
from flow_native_notes import NATIVE_NOTES


OUTPUT = ROOT / "out/binary-research/flow-native-sources.json"
TEMPLATE = ROOT / "_Big_tool/binary template/big_assets/flow_native_sources.bt"


def original_filename(record):
    source = record.get("original_source", "未找到原文件符号")
    anchor = "/Projects/GunBros1/"
    if anchor in source:
        source = source.split(anchor, 1)[1]
    return source


def legacy_signatures(host):
    """版本未知的.link只提供辅助名称，单独标示，不拿它覆盖3.6原读取行为。"""
    link_names = {"CGame": "game", "CLevel": "level", "CBrother": "player", "CEnemy": "enemy",
                  "CGun": "gun", "CBullet": "bullet", "CPickup": "pickup", "CProp": "prop",
                  "IEnemySpawnerScriptInterface": "spawner", "CArmor": "armor", "Mission": "mission", "CPowerup": "powerup"}
    path = ROOT / "flow scripts" / (link_names[host] + ".link")
    if not path.exists():
        return []
    text = re.sub(r"//[^\n]*", "", path.read_text(encoding="utf-8-sig"))
    block = re.search(r"functions\s+\w+\s*\{([^}]*)\}", text, re.DOTALL)
    if not block:
        return []
    return re.findall(r"\w+\s*\([^)]*\)", block.group(1))


def main():
    source_lines = (ROOT / "_IDA_OUT/gunbros_3.6.0_IOS.c").read_text(encoding="utf-8").splitlines()
    functions = json.loads((ROOT / "out/binary-research/source-index.json").read_text(encoding="utf-8"))
    flow = json.loads((ROOT / "out/binary-research/flow-bytecode-catalog.json").read_text(encoding="utf-8"))
    calls = {}
    for entry in flow["entries"]:
        for call in entry["calls"]:
            if "native_slot" not in call:
                continue
            record = calls.setdefault(call["function_id"], {"argument_counts": Counter(), "examples": []})
            record["argument_counts"][len(call["arguments"])] += 1
            if len(record["examples"]) < 2:
                record["examples"].append({"path": entry["path"], "offset": call["offset"], "arguments": call["arguments"]})
    lines = ["// Flow原生调用/宿主变量证据索引（无磁盘读取入口，配合flow_bytecode.bt）。",
             "// 此文件按class和native slot组织，脚本只存编号；每段列原函数、文件、地址和反编译行号。",
             "// 参数a3[n]是第n个已解引用int16实参，a4是实参数量；a1是IScriptContext宿主。",
             "// 参数类型/单位须看case内转换，不由解码token或旧.link中的名字决定。",
             "// .link参考签名版本未知，仅是阅读提示；原3.6.0分支才是行为证据，缺case不自动证明无实现。",
             "// 原代码使用共享LABEL，分支后的跳转行会提示目标；完整源码保留原有控制流。",
             "// 枪脚本fixed速度在:128231/:128246乘1/256；fixed秒在:128270/:128274乘1000/256成毫秒。",
             "// 因此脚本形参的8.8定点与实体文件字段的16.16必须分开理解。", ""]
    results = []
    for class_id, host in HOSTS.items():
        old_names = legacy_signatures(host)
        lines.extend([f"// ===== class 0x{class_id:02X}: {host} =====", ""])
        for function_index, function in enumerate(functions):
            signature = function["signature"]
            if host + "::FunctionResolver(" not in signature and host + "::VariableResolver(" not in signature:
                continue
            start = function["line"] - 1
            end = len(source_lines)
            if function_index + 1 < len(functions):
                end = functions[function_index + 1]["line"] - 2
            body = source_lines[start:end]
            lines.append(f"// 原函数 {signature}")
            lines.append(f"// 原文件 {original_filename(function)}；地址{function['address']}；反编译入口:{function['line']}。")
            is_variable = "::VariableResolver(" in signature
            case_rows = []
            for index, line in enumerate(body):
                match = re.match(r"(\s*)case\s+(0x[0-9A-Fa-f]+|[0-9]+)(?:u|U)?:", line)
                if match:
                    case_rows.append((len(match.group(1)), index, int(match.group(2), 0)))
            indentation = 1000
            for indent, index, slot in case_rows:
                indentation = min(indentation, indent)
            top_rows = []
            for indent, index, slot in case_rows:
                if indent == indentation:
                    top_rows.append((index, slot))
            result = dict(function)
            result["class_id"] = class_id
            result["kind"] = "function"
            if is_variable:
                result["kind"] = "variable"
            result["branches"] = []
            labels = {}
            for index, line in enumerate(body):
                label = re.match(r"\s*(LABEL_\d+):", line)
                if label:
                    labels[label.group(1)] = start + index + 1
            if not top_rows or is_variable:
                lines.append("// 以下为完整短读取器/变量地址映射，编号若由if判断请直接对照条件：")
                for index, line in enumerate(body):
                    lines.append(f"// :{start + index + 1} {line}")
            else:
                for row_index, (first, slot) in enumerate(top_rows):
                    last = len(body)
                    if row_index + 1 < len(top_rows):
                        last = top_rows[row_index + 1][0]
                    function_id = f"0x{class_id * 256 + slot:04X}"
                    branch = {"slot": slot, "function_id": function_id, "line": start + first + 1,
                              "source": [], "goto_targets": {}}
                    lines.extend(["", f"// {function_id} / {host} native slot {slot}，case入口:{start + first + 1}。"])
                    notes = NATIVE_NOTES.get(function_id, [])
                    branch["verified_parameter_notes"] = notes
                    for note in notes:
                        lines.append("// 用途/参数核对：" + note)
                    if slot < len(old_names):
                        lines.append("// 旧.link参考（非当前版本保证）：" + old_names[slot])
                    observed = calls.get(function_id)
                    if observed:
                        branch["observed"] = observed
                        lines.append("// 实际BIG调用实参数量分布：" + str(dict(observed["argument_counts"])))
                        example = observed["examples"][0]
                        lines.append(f"// 样本 {example['path']}，functionId字段偏移0x{example['offset']:X}。")
                    else:
                        lines.append("// 当前解码样本未直接调用此slot；保留原代码分支作为结构依据。")
                    # 连续case共享后续代码，明确指出，不误称前一个case是空操作。
                    if last == first + 1:
                        lines.append("// 连续case标签，与下一个分支共用函数体。")
                    for index in range(first, last):
                        line = body[index]
                        branch["source"].append({"line": start + index + 1, "text": line})
                        lines.append(f"// :{start + index + 1} {line}")
                        for label in re.findall(r"goto\s+(LABEL_\d+)", line):
                            if label in labels:
                                branch["goto_targets"][label] = labels[label]
                    for label, line_number in branch["goto_targets"].items():
                        lines.append(f"// 共享代码跳转 {label} 在:{line_number}，参数转换可能延续至那里。")
                    result["branches"].append(branch)
            results.append(result)
            lines.append("")
    OUTPUT.write_text(json.dumps({"resolvers": results, "observed_calls": calls}, ensure_ascii=False, indent=2), encoding="utf-8")
    TEMPLATE.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Indexed {len(results)} original resolvers, {len(calls)} observed native IDs, {len(lines)} annotated lines")


if __name__ == "__main__":
    main()
