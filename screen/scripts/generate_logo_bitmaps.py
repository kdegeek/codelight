#!/usr/bin/env python3
"""Generate tiny monochrome provider logo bitmaps for the ESP8266 firmware."""

from __future__ import annotations

from pathlib import Path
from math import cos, sin, pi

SIZE = 32
OUT = Path(__file__).resolve().parents[1] / "src" / "logo_assets.h"


def blank() -> list[list[int]]:
    return [[0 for _ in range(SIZE)] for _ in range(SIZE)]


def point(canvas: list[list[int]], x: int, y: int, r: int = 0) -> None:
    for yy in range(y - r, y + r + 1):
        for xx in range(x - r, x + r + 1):
            if 0 <= xx < SIZE and 0 <= yy < SIZE and (xx - x) ** 2 + (yy - y) ** 2 <= r * r:
                canvas[yy][xx] = 1


def line(canvas: list[list[int]], x0: int, y0: int, x1: int, y1: int, thick: int = 0) -> None:
    dx = abs(x1 - x0)
    sx = 1 if x0 < x1 else -1
    dy = -abs(y1 - y0)
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        point(canvas, x0, y0, thick)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def circle(canvas: list[list[int]], cx: int, cy: int, radius: int, thick: int = 0) -> None:
    for deg in range(360):
        rad = deg * pi / 180
        point(canvas, round(cx + cos(rad) * radius), round(cy + sin(rad) * radius), thick)


def poly(canvas: list[list[int]], pts: list[tuple[int, int]], thick: int = 0, close: bool = True) -> None:
    pairs = zip(pts, pts[1:])
    for (x0, y0), (x1, y1) in pairs:
        line(canvas, x0, y0, x1, y1, thick)
    if close:
        line(canvas, pts[-1][0], pts[-1][1], pts[0][0], pts[0][1], thick)


def codex() -> list[list[int]]:
    c = blank()
    nodes = [(16, 5), (25, 10), (25, 21), (16, 27), (7, 21), (7, 10)]
    for i, (x, y) in enumerate(nodes):
        x2, y2 = nodes[(i + 2) % len(nodes)]
        line(c, x, y, x2, y2)
    for x, y in nodes:
        circle(c, x, y, 3)
    circle(c, 16, 16, 4)
    return c


def claude() -> list[list[int]]:
    c = blank()
    for deg in range(0, 360, 45):
        rad = deg * pi / 180
        x = round(16 + cos(rad) * 10)
        y = round(16 + sin(rad) * 10)
        line(c, 16, 16, x, y, thick=1)
        point(c, x, y, 3)
    point(c, 16, 16, 4)
    return c


def ollama() -> list[list[int]]:
    c = blank()
    head = [(9, 11), (11, 4), (15, 11), (17, 11), (21, 4), (23, 11), (23, 24), (20, 28), (12, 28), (9, 24)]
    poly(c, head, thick=1)
    point(c, 13, 17, 1)
    point(c, 19, 17, 1)
    line(c, 14, 23, 18, 23)
    return c


def antigravity() -> list[list[int]]:
    c = blank()
    circle(c, 16, 16, 13)
    circle(c, 16, 16, 8)
    line(c, 3, 16, 29, 16)
    line(c, 16, 3, 16, 29)
    line(c, 7, 25, 25, 7)
    point(c, 24, 8, 2)
    return c


LOGOS = {
    "codex": codex,
    "claude": claude,
    "ollama": ollama,
    "antigravity": antigravity,
}


def pack(canvas: list[list[int]]) -> list[int]:
    data: list[int] = []
    for y in range(SIZE):
        for byte_x in range(0, SIZE, 8):
            byte = 0
            for bit in range(8):
                if canvas[y][byte_x + bit]:
                    byte |= 1 << bit
            data.append(byte)
    return data


def emit() -> None:
    lines = [
        "#pragma once",
        "#include <Arduino.h>",
        "",
        "#define LOGO_BITMAP_SIZE 32",
        "",
    ]
    for name, factory in LOGOS.items():
        data = pack(factory())
        lines.append(f"static const uint8_t logo_{name}[] PROGMEM = {{")
        for i in range(0, len(data), 12):
            lines.append("    " + ", ".join(f"0x{b:02x}" for b in data[i : i + 12]) + ",")
        lines.append("};")
        lines.append("")
    lines.extend(
        [
            "struct LogoAsset {",
            "    const char* provider;",
            "    const uint8_t* data;",
            "};",
            "",
            "static const LogoAsset LOGO_ASSETS[] = {",
            '    {"codex", logo_codex},',
            '    {"claude", logo_claude},',
            '    {"ollama", logo_ollama},',
            '    {"antigravity", logo_antigravity},',
            "};",
            "",
        ]
    )
    OUT.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    emit()
