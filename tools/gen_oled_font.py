#!/usr/bin/env python3

import gzip
from pathlib import Path

CHINESE_CHARS = (
    "香橙派系统正常设备温度网络运行连接成功失败"
    "电压风扇离线在线故障启动关闭串口传感器"
)

OUTPUT = Path("src/oled_font.c")

FONT_CANDIDATES = [
    Path("/usr/share/unifont/unifont.hex"),
    Path("/usr/share/unifont/unifont.hex.gz"),
    Path("/usr/share/fonts/unifont/unifont.hex"),
    Path("/usr/share/fonts/unifont/unifont.hex.gz"),
]


def find_font():
    for path in FONT_CANDIDATES:
        if path.exists():
            return path

    for root in (Path("/usr/share/unifont"), Path("/usr/share/fonts")):
        if not root.exists():
            continue

        for path in root.rglob("unifont*.hex"):
            return path

        for path in root.rglob("unifont*.hex.gz"):
            return path

    raise RuntimeError(
        "GNU Unifont hex font not found. "
        "Install it with: sudo apt install unifont"
    )


def load_font(path):
    if path.suffix == ".gz":
        file_obj = gzip.open(path, "rt", encoding="ascii")
    else:
        file_obj = path.open("r", encoding="ascii")

    glyphs = {}

    with file_obj as f:
        for line in f:
            line = line.strip()

            if not line or ":" not in line:
                continue

            codepoint_text, bitmap_text = line.split(":", 1)

            try:
                codepoint = int(codepoint_text, 16)
                bitmap = bytes.fromhex(bitmap_text)
            except ValueError:
                continue

            glyphs[codepoint] = bitmap

    return glyphs


def normalize_ascii(bitmap):
    if len(bitmap) == 16:
        return list(bitmap)

    if len(bitmap) == 32:
        result = []

        for row in range(16):
            left = bitmap[row * 2]
            right = bitmap[row * 2 + 1]
            merged = left | right
            result.append(merged)

        return result

    return [0] * 16


def normalize_zh(bitmap):
    if len(bitmap) == 32:
        return list(bitmap)

    if len(bitmap) == 16:
        result = []

        for value in bitmap:
            result.append(value)
            result.append(0)

        return result

    return [0] * 32


def c_bytes(values):
    return ", ".join(f"0x{value:02X}" for value in values)


def main():
    font_path = find_font()
    glyphs = load_font(font_path)

    unique_chinese = "".join(dict.fromkeys(CHINESE_CHARS))

    lines = [
        '#include "oled_font.h"',
        "",
        "#include <stddef.h>",
        "",
        "typedef struct",
        "{",
        "    uint32_t codepoint;",
        "    uint8_t bitmap[32];",
        "} OledZhGlyph;",
        "",
        "static const uint8_t ascii8x16[95][16] =",
        "{",
    ]

    for codepoint in range(32, 127):
        bitmap = normalize_ascii(glyphs.get(codepoint, b""))
        lines.append(f"    {{{c_bytes(bitmap)}}},")

    lines.extend([
        "};",
        "",
        "static const OledZhGlyph zh16[] =",
        "{",
    ])

    for ch in unique_chinese:
        codepoint = ord(ch)
        bitmap = normalize_zh(glyphs.get(codepoint, b""))

        lines.append(
            f"    {{0x{codepoint:04X}, {{{c_bytes(bitmap)}}}}},"
        )

    lines.extend([
        "};",
        "",
        "const uint8_t *oled_font_ascii8x16(unsigned char ch)",
        "{",
        "    if (ch < 32 || ch > 126)",
        "    {",
        "        ch = '?';",
        "    }",
        "",
        "    return ascii8x16[ch - 32];",
        "}",
        "",
        "const uint8_t *oled_font_zh16(uint32_t codepoint)",
        "{",
        "    for (size_t i = 0; i < sizeof(zh16) / sizeof(zh16[0]); i++)",
        "    {",
        "        if (zh16[i].codepoint == codepoint)",
        "        {",
        "            return zh16[i].bitmap;",
        "        }",
        "    }",
        "",
        "    return NULL;",
        "}",
        "",
    ])

    OUTPUT.write_text("\n".join(lines), encoding="utf-8")

    print(f"generated {OUTPUT}")
    print(f"font: {font_path}")
    print(f"chinese glyphs: {unique_chinese}")


if __name__ == "__main__":
    main()
