"""按原函数名摘录反编译正文，保留文件行号，便于模板注释核对。"""
from pathlib import Path
import sys


SOURCE = Path(__file__).resolve().parents[2] / "_IDA_OUT/gunbros_3.6.0_IOS.c"


def main():
    lines = SOURCE.read_text(encoding="utf-8").splitlines()
    starts = []
    for index, line in enumerate(lines):
        if line.startswith("//----- ("):
            starts.append(index)
    starts.append(len(lines))
    for query in sys.argv[1:]:
        found = 0
        for index in range(len(starts) - 1):
            first = starts[index]
            last = starts[index + 1]
            signature = " ".join(lines[first + 1:first + 7])
            signature = signature.split("{")[0]
            if query not in signature:
                continue
            found += 1
            print(f"\nSOURCE {SOURCE.name}:{first + 2}")
            for row in range(first + 1, last):
                print(f"{row + 1}: {lines[row]}")
        if found == 0:
            print(f"No function: {query}")


if __name__ == "__main__":
    main()
