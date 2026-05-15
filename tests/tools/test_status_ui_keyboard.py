#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
UI_KEYBOARD = ROOT / "src" / "ui" / "ui_keyboard.c"
DISPLAY_TEXT = ROOT / "src" / "ui" / "display_text.c"


def read_ui_keyboard() -> str:
    return UI_KEYBOARD.read_text(encoding="utf-8")


def read_display_text() -> str:
    return DISPLAY_TEXT.read_text(encoding="utf-8")


def extract_key_cycles(source: str) -> dict[str, str]:
    pattern = re.compile(
        r'\{\s*UI_KEY_KIND_CHAR,\s*"(?P<label>[2-9][A-Z]+)",\s*"(?P<text>[^"]+)",\s*"(?P<symbol>[^"]+)"',
    )
    return {match.group("label"): match.group("text") for match in pattern.finditer(source)}


def test_phone_keys_cycle_digit_lowercase_then_uppercase() -> None:
    cycles = extract_key_cycles(read_ui_keyboard())
    expected = {
        "2ABC": "2abcABC",
        "3DEF": "3defDEF",
        "4GHI": "4ghiGHI",
        "5JKL": "5jklJKL",
        "6MNO": "6mnoMNO",
        "7PQRS": "7pqrsPQRS",
        "8TUV": "8tuvTUV",
        "9WXYZ": "9wxyzWXYZ",
    }
    assert cycles == expected


def test_lcd_text_renderer_preserves_lowercase_glyphs() -> None:
    source = read_display_text()
    glyph_rows = source[source.index("static const uint8_t *glyph_rows") : source.index("static bool display_text_is_supported_ascii")]
    assert "toupper" not in glyph_rows
    for ch in "abcdefghijklmnopqrstuvwxyz":
        assert f"case '{ch}': return glyph_lower_{ch};" in glyph_rows


if __name__ == "__main__":
    test_phone_keys_cycle_digit_lowercase_then_uppercase()
    test_lcd_text_renderer_preserves_lowercase_glyphs()
    print("status_ui_keyboard static tests passed")
