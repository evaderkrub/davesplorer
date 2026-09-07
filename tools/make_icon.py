"""Draws the Davesplorer icon: a red folder.

Renders at 1024px with anti-aliasing, then writes a multi-size .ico (the
Windows resource) and a 256px .png. Run from anywhere:

    python tools/make_icon.py
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "assets" / "icon"
SIZE = 1024

# FreeWili red family: the theme's accent (200,48,48) as the front flap,
# darker behind, a warm highlight along the flap's top edge.
BACK = (150, 30, 32, 255)
FRONT = (214, 52, 52, 255)
HIGHLIGHT = (240, 110, 100, 255)
SHADOW = (0, 0, 0, 90)


def rounded(draw, box, radius, fill):
    draw.rounded_rectangle(box, radius=radius, fill=fill)


def render() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))

    # Soft drop shadow so the icon reads on light and dark backgrounds.
    shadow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    rounded(sd, (110, 380, 940, 900), 70, SHADOW)
    shadow = shadow.filter(ImageFilter.GaussianBlur(28))
    img.alpha_composite(shadow)

    d = ImageDraw.Draw(img)
    # Back panel with the tab on the left.
    rounded(d, (96, 150, 440, 330), 56, BACK)
    rounded(d, (96, 215, 928, 860), 64, BACK)
    # Front flap, slightly inset at the top so the back shows as a band.
    rounded(d, (96, 345, 928, 860), 64, FRONT)
    # Highlight line along the flap's top edge.
    rounded(d, (140, 345, 884, 366), 10, HIGHLIGHT)
    return img


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    img = render()
    img.resize((256, 256), Image.Resampling.LANCZOS).save(OUT_DIR / "davesplorer.png")
    sizes = [(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (96, 96), (128, 128), (256, 256)]
    img.save(OUT_DIR / "davesplorer.ico", format="ICO", sizes=sizes)
    print(f"wrote {OUT_DIR / 'davesplorer.ico'} and davesplorer.png")


if __name__ == "__main__":
    main()
