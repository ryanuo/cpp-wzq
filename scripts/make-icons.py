#!/usr/bin/env python3
"""生成应用图标：源 PNG(1024) + macOS .icns + Windows .ico
用法: python3 scripts/make-icons.py
依赖: Pillow（macOS .icns 额外需要 iconutil，macOS 自带）
设计: 深色圆角底 + 5×5 棋盘网格 + 黑白两子（五子棋），与首页卡片风格呼应
"""
import os
import struct
import subprocess
import tempfile

from PIL import Image, ImageDraw

SIZE = 1024
HERE = os.path.dirname(os.path.abspath(__file__))
ICON_DIR = os.path.normpath(os.path.join(HERE, "..", "app", "icon"))
PNG_1024 = os.path.join(ICON_DIR, "gobang-1024.png")


def draw_icon() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))

    # 四周留白（~9%）：macOS Dock 图标内容应占画布 ~80%，
    # 满幅圆角底会显得比其他图标大（Apple 规范：内容区留边距）
    m = int(SIZE * 0.09)
    inner = SIZE - 2 * m  # 内容区边长

    # 圆角遮罩（半径 170）
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).rounded_rectangle([m, m, SIZE - m, SIZE - m], radius=170, fill=255)

    # 垂直渐变底色（深蓝灰）
    top, bottom = (58, 74, 95), (35, 44, 58)
    bg = Image.new("RGBA", (SIZE, SIZE))
    for y in range(SIZE):
        t = y / SIZE
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)
        ImageDraw.Draw(bg).line([(0, y), (SIZE, y)], fill=c)
    img.paste(bg, (0, 0), mask)

    d = ImageDraw.Draw(img)

    # 5×5 棋盘网格（内容区内缩）
    g0, g1 = m + int(inner * 0.09), SIZE - m - int(inner * 0.09)
    step = (g1 - g0) // 4
    grid = (255, 255, 255, 70)
    for i in range(5):
        x = g0 + i * step
        d.line([(x, g0), (x, g1)], fill=grid, width=10)
        d.line([(g0, x), (g1, x)], fill=grid, width=10)

    def circle(cx, cy, r):
        return [cx - r, cy - r, cx + r, cy + r]

    # 黑白两子错位重叠（五子棋）
    # 黑子（偏左上）
    d.ellipse(circle(415, 480, 135), fill=(40, 42, 46, 255))
    d.ellipse(circle(465, 530, 38), fill=(255, 255, 255, 70))  # 高光

    # 白子（偏右下，带细边）
    d.ellipse(circle(610, 545, 135), fill=(250, 250, 250, 255),
              outline=(200, 200, 205, 255), width=12)
    d.ellipse(circle(660, 595, 38), fill=(255, 255, 255, 140))  # 高光

    return img


def make_icns(img: Image.Image) -> str:
    """1024 PNG -> iconset -> .icns（macOS iconutil）"""
    # iconutil 要求目录名以 .iconset 结尾
    iconset = tempfile.mkdtemp(prefix="gobang-icon-", suffix=".iconset")
    specs = {
        "icon_16x16.png": 16, "icon_16x16@2x.png": 32,
        "icon_32x32.png": 32, "icon_32x32@2x.png": 64,
        "icon_128x128.png": 128, "icon_128x128@2x.png": 256,
        "icon_256x256.png": 256, "icon_256x256@2x.png": 512,
        "icon_512x512.png": 512, "icon_512x512@2x.png": 1024,
    }
    for name, size in specs.items():
        img.resize((size, size), Image.LANCZOS).save(os.path.join(iconset, name))
    icns = os.path.join(ICON_DIR, "gobang.icns")
    subprocess.run(["iconutil", "-c", "icns", iconset, "-o", icns], check=True)
    return icns


def make_ico(img: Image.Image) -> str:
    """生成 BMP(DIB) 格式多尺寸 .ico。
    注意: 必须用 BMP 而非 PIL 默认的 PNG 压缩 —— MSVC 的 rc.exe 对 PNG 压缩
    图标兼容性差（表现为 exe 里无 RT_ICON 资源），BMP 格式 100% 兼容。
    """
    sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    blobs = []
    for w, h in sizes:
        im = img.resize((w, h), Image.LANCZOS).convert("RGBA")
        # 像素：自下而上 BGRA（32bpp）
        px = bytearray()
        for y in range(h - 1, -1, -1):
            for x in range(w):
                r, g, b, a = im.getpixel((x, y))
                px += bytes((b, g, r, a))
        # AND mask：32bpp 带 alpha 时全 0（不透明）
        and_row = ((w + 31) // 32) * 4
        and_mask = b"\x00" * (and_row * h)
        header = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, len(px), 0, 0, 0, 0)
        blobs.append((w, header + bytes(px) + and_mask))
    # ICONDIR + ICONDIRENTRY（256 写 0 表示 256）
    out = struct.pack("<HHH", 0, 1, len(blobs))
    offset = 6 + 16 * len(blobs)
    for w, data in blobs:
        out += struct.pack("<BBBBHHII", w & 0xFF, w & 0xFF, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    for _, data in blobs:
        out += data
    ico = os.path.join(ICON_DIR, "gobang.ico")
    with open(ico, "wb") as f:
        f.write(out)
    return ico


def main() -> None:
    os.makedirs(ICON_DIR, exist_ok=True)
    img = draw_icon()
    img.save(PNG_1024)
    print(f"PNG  : {PNG_1024}")
    print(f"ICNS : {make_icns(img)}")
    print(f"ICO  : {make_ico(img)}")


if __name__ == "__main__":
    main()
