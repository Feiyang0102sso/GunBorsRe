"""只读解码BIG内嵌Flow字节码，核对每条语句/分支长度，定位native调用和变量token。"""
from collections import Counter
import json
import struct

from catalog_game_entries import Reader
from catalog_ui_movies import ROOT


OUTPUT = ROOT / "out/binary-research/flow-bytecode-catalog.json"
HOSTS = {4: "CGame", 5: "CLevel", 6: "CBrother", 7: "CEnemy", 8: "CGun",
         9: "CBullet", 10: "CPickup", 11: "CProp", 12: "IEnemySpawnerScriptInterface",
         13: "CArmor", 14: "Mission", 15: "CPowerup"}
OPERATORS = {0: "+=", 1: "-=", 2: "++", 3: "--", 4: "*=", 5: "/=", 6: "=",
             7: "set_bit", 8: "clear_bit", 10: "=call_result"}
COMPARISONS = {0: "==", 1: "!=", 2: ">", 3: ">=", 4: "<", 5: "<=", 6: "always",
               7: "bit_is_set", 8: "bit_is_clear", 9: "masks_intersect", 10: "masks_disjoint"}


def operand(token):
    """按GetData :107424解释token；不把资源槽常量预先当成资源引用。"""
    if token & 0x8000:
        value = token & 0x7FFF
        if token & 0x4000:
            value = token - 0x10000
        return {"kind": "immediate", "value": value}
    if token >> 8:
        host = HOSTS.get(token >> 8, "UNRESOLVED_CLASS")
        return {"kind": "host_variable", "class": host, "slot": token & 255}
    if token < 250:
        return {"kind": "local_variable", "slot": token}
    return {"kind": "argument_or_temporary", "interpreter_offset": 22 + 2 * (254 - token)}


def read_operand(reader, name):
    token = reader.read(name, "H")
    return {"token": f"0x{token:04X}", "meaning": operand(token)}


def read_call(reader, calls):
    start = reader.position
    function_id = reader.read("functionId", "H")
    count = reader.read("argumentCount", "B")
    arguments = []
    for index in range(count):
        arguments.append(read_operand(reader, f"arguments[{index}]"))
    call = {"offset": start, "function_id": f"0x{function_id:04X}", "arguments": arguments}
    if function_id > 255:
        call["class"] = HOSTS.get(function_id >> 8, "UNRESOLVED_CLASS")
        call["native_slot"] = function_id & 255
    else:
        call["internal_function"] = function_id
    calls.append(call)
    return call


def read_code(reader, calls):
    start = reader.position
    size = reader.read("codeByteLength", "B")
    end = reader.position + size
    block = {"offset": start, "byte_length": size, "statements": []}
    if size == 0:
        return block
    count = reader.read("statementCount", "B")
    for index in range(count):
        statement = {"offset": reader.position}
        opcode = reader.read("opcode", "B")
        statement["opcode"] = opcode
        if opcode == 0:
            statement["call"] = read_call(reader, calls)
        elif opcode == 1:
            statement["set_state"] = reader.read("stateIndex", "B")
        elif opcode == 2:
            statement["destination"] = read_operand(reader, "destination")
            operator = reader.read("assignmentOperator", "B")
            statement["operator_raw"] = operator
            if operator == 10:
                statement["operator"] = "=call_result"
                statement["call"] = read_call(reader, calls)
            else:
                statement["operator"] = OPERATORS.get(operator & 15, "unknown")
                statement["operand"] = read_operand(reader, "source")
                if operator & 128:
                    statement["source_data_block"] = (operator >> 4) & 7
        elif opcode == 3:
            statement["event_id"] = f"0x{reader.read('eventId', 'H'):04X}"
            statement["body"] = read_code(reader, calls)
        elif opcode == 4:
            branches = []
            more = 1
            while more == 1:
                branch = {"left": read_operand(reader, "left"), "right": read_operand(reader, "right")}
                comparison = reader.read("comparison", "B")
                branch["comparison"] = COMPARISONS.get(comparison, "unknown")
                branch["comparison_raw"] = comparison
                branch["body"] = read_code(reader, calls)
                more = reader.read("hasNextBranch", "B")
                branch["continuation"] = more
                branches.append(branch)
            statement["branches"] = branches
        elif opcode == 5:
            statement["return"] = read_operand(reader, "returnToken")
        else:
            raise ValueError(f"Unknown opcode {opcode} at {statement['offset']}")
        block["statements"].append(statement)
    if reader.position != end:
        raise ValueError(f"Code boundary mismatch at {start}: {reader.position}/{end}")
    return block


def main():
    records = []
    errors = []
    calls_total = Counter()
    block_count = 0
    for filename in ("out/game-entry-catalog.json", "out/binary-research/section-catalog.json"):
        catalog = json.loads((ROOT / filename).read_text(encoding="utf-8"))
        for entry in catalog["entries"]:
            code_fields = []
            for field in entry.get("fields", []):
                if field["name"].endswith(".byteLength") and "script" in field["name"].lower():
                    code_fields.append(field)
            if not code_fields:
                continue
            record = {"path": entry["path"], "section": entry["section"], "blocks": [], "calls": []}
            data = (ROOT / entry["path"]).read_bytes()
            reader = Reader(data)
            for field in code_fields:
                reader.position = field["offset"]
                try:
                    block = read_code(reader, record["calls"])
                    block["container_field"] = field["name"]
                    record["blocks"].append(block)
                    block_count += 1
                except (ValueError, struct.error) as error:
                    errors.append({"path": entry["path"], "offset": field["offset"], "error": str(error)})
            for call in record["calls"]:
                calls_total[call["function_id"]] += 1
            records.append(record)
    OUTPUT.write_text(json.dumps({"files": len(records), "top_level_code_blocks": block_count,
                                 "errors": errors, "call_counts": dict(calls_total), "entries": records},
                                ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Flow decode: files={len(records)} blocks={block_count} errors={len(errors)} functionIds={len(calls_total)}")
    for error in errors[:12]:
        print(error)
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
