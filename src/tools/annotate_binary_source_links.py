"""将模板中的反编译行号接到原符号/原工程文件；只维护有标记的追加注释，不改用户原注释。"""
from pathlib import Path
import bisect
import json
import re


ROOT = Path(__file__).resolve().parents[2]
TEMPLATES = ROOT / "_Big_tool/binary template"
INDEX = ROOT / "out/binary-research/source-index.json"
OUTPUT = ROOT / "out/binary-research/template-source-links.json"
BEGIN = "// BEGIN VERIFIED SOURCE INDEX"
END = "// END VERIFIED SOURCE INDEX"


def original_path(record):
    path = record.get("original_source")
    if not path:
        return "原工程文件名尚未从此符号恢复；不猜文件名"
    marker = "/Projects/GunBros1/"
    if marker in path:
        path = path.split(marker, 1)[1]
    return path


def main():
    functions = json.loads(INDEX.read_text(encoding="utf-8"))
    starts = []
    for function in functions:
        starts.append(function["line"])
    records = []
    for path in sorted(TEMPLATES.rglob("*.bt")):
        body = path.read_text(encoding="utf-8-sig")
        if BEGIN in body:
            start = body.index(BEGIN)
            end = body.index(END, start) + len(END)
            body = body[:start] + body[end:]
        citations = set()
        # 中文紧随数字也算Unicode单词字符，不能用\b，否则“:203033写”会漏掉。
        for match in re.finditer(r":\s*(\d{5,6})(?!\d)", body):
            citations.add(int(match.group(1)))
        matched = {}
        for line in sorted(citations):
            index = bisect.bisect_right(starts, line) - 1
            if index < 0:
                continue
            function = functions[index]
            matched.setdefault(function["line"], {"function": function, "citations": []})["citations"].append(line)
        lines = [BEGIN,
                 "// 以下自动追加的是导航依据，不是磁盘字段；来自gunbros的ARMv7 N_FUN/N_SO调试符号。",
                 "// 行号均为_IDA_OUT/gunbros_3.6.0_IOS.c，地址为该ARMv7代码地址，不是bin偏移。",
                 "// 原工程文件路径由N_SO恢复；原工程本身的行号未恢复，不能把反编译行号冒充原.cpp行号。"]
        for result in matched.values():
            function = result["function"]
            citation_text = ", ".join(map(str, result["citations"]))
            lines.append(f"// [{citation_text}] {function['signature']}")
            lines.append(f"//   定义行 {function['line']}；地址 {function['address']}；原文件 {original_path(function)}")
        if not matched:
            lines.append("// 本模板没有可定位的函数行号；可能为标准格式或其他模板的入口，不能据此补造原类名。")
        lines.append(END)
        path.write_text(body.rstrip() + "\n\n" + "\n".join(lines) + "\n", encoding="utf-8")
        records.append({"template": path.relative_to(ROOT).as_posix(), "source_functions": list(matched.values())})
    OUTPUT.write_text(json.dumps({"templates": records}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Annotated source navigation for {len(records)} templates")


if __name__ == "__main__":
    main()
