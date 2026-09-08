"""汇总原电影研究截图，便于核对原版页面结构；不改动源图。"""
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
CELL_WIDTH = 256
CELL_HEIGHT = 210
COLS = 4
ROWS = 4


def main():
    """每页排布十六个原 CMovie 的实际渲染结果。"""
    source = ROOT / "out/ui-movies"
    for sheet in range(10):
        canvas = Image.new("RGB", (COLS * CELL_WIDTH, ROWS * CELL_HEIGHT), "#202020")
        labels = ImageDraw.Draw(canvas)
        for cell in range(COLS * ROWS):
            ordinal = sheet * COLS * ROWS + cell
            path = source / f"{ordinal:03d}.png"
            if not path.exists():
                continue
            with Image.open(path) as screenshot:
                screenshot.thumbnail((CELL_WIDTH, CELL_HEIGHT - 18))
                left = cell % COLS * CELL_WIDTH
                top = cell // COLS * CELL_HEIGHT
                canvas.paste(screenshot, (left, top + 18))
                labels.text((left + 4, top + 2), f"MOVIE {ordinal}", fill="white")
        output = ROOT / "out" / f"ui-movie-sheet-{sheet}.png"
        canvas.save(output)
        print(f"[contact-sheet] saved={output}")


if __name__ == "__main__":
    main()
