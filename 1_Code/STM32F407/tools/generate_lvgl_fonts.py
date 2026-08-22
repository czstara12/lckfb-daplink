# SPDX-License-Identifier: MIT
# Origin: Generated for this repository's LVGL font optimization.
# Created-By: gpt-5
# Signed-off-by: xcwynya

"""扫描 LVGL 静态文本并生成最小字符集字体。"""

import argparse
import ast
import hashlib
import re
import shutil
import subprocess
import sys
import tempfile
import unicodedata
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI_DIR = ROOT / "applications/lvgl/ui"
FONT_DIR = UI_DIR / "fonts"
FONT_INPUT_DIR = ROOT / "tools/lv_font_conv/fonts"
LOCAL_CONVERTER = ROOT / "tools/lv_font_conv/node_modules/.bin/lv_font_conv"


def _gb2312_hanzi():
    """返回 GB2312 一级、二级汉字，不混入 ASCII。"""
    characters = []
    for area in range(16, 88):
        for position in range(1, 95):
            try:
                characters.append(bytes((area + 0xA0, position + 0xA0)).decode("gb2312"))
            except UnicodeDecodeError:
                pass
    assert len(characters) == len(set(characters)) == 6763
    assert all(ord(character) > 0x7F for character in characters)
    return "".join(characters)


def _gb2312_fullwidth_punctuation():
    """返回 GB2312 全角标点，不包含其他语言文字。"""
    characters = []
    for area in range(1, 16):
        for position in range(1, 95):
            try:
                character = bytes((area + 0xA0, position + 0xA0)).decode("gb2312")
            except UnicodeDecodeError:
                continue
            if unicodedata.category(character).startswith("P") or character in "　～":
                characters.append("·" if character == "・" else character)
    characters = list(dict.fromkeys(characters))
    assert len(characters) == 55
    return "".join(characters)


# ponytail: 缓冲区动态文本无法静态反推；格式变化时在每项末尾补充运行时字符。
FONT_SPECS = {
    "ui_font_jetbrainsMonoMedium16": ("JetBrainsMono-Medium.ttf", 16, 4, " 0123456789.-VAmW"),
    "ui_font_jetbrainsMonoMedium20": (
        "JetBrainsMono-Medium.ttf",
        20,
        4,
        " 0123456789.,:ABCDEFJanFebMarAprMayJunJulAugSepOctNovDecVersionx",
    ),
    "ui_font_jetbrainsMonoMedium25": ("JetBrainsMono-Medium.ttf", 25, 4, " 0123456789.-%KHMhzVAmW"),
    "ui_font_PuHuiTi": ("Alibaba-PuHuiTi-Medium.ttf", 21, 1, ""),
    "ui_font_PuHuiTi16GB2312": (
        "Alibaba-PuHuiTi-Medium.ttf",
        16,
        1,
        _gb2312_hanzi() + _gb2312_fullwidth_punctuation(),
    ),
    "ui_font_PuHuiTi25": ("Alibaba-PuHuiTi-Medium.ttf", 25, 1, "任意据数"),
    "ui_font_PuHuiTi30": ("Alibaba-PuHuiTi-Medium.ttf", 30, 1, ""),
}

CREATE_RE = re.compile(r"\b(\w+)\s*=\s*lv_\w+_create\s*\(\s*(\w+)")
FONT_RE = re.compile(r"\blv_obj_set_style_text_font\s*\(\s*(\w+)\s*,\s*&\s*(ui_font_\w+)")
TEXT_RE = re.compile(
    r"\b(?:lv_label_set_text|lv_checkbox_set_text|lv_dropdown_set_options|"
    r"lv_roller_set_options|lv_textarea_set_placeholder_text|lv_tabview_add_tab)"
    r"\s*\(\s*(\w+)\s*,\s*((?:\"(?:\\.|[^\"\\])*\"\s*)+)",
    re.DOTALL,
)
PROPERTY_TEXT_RE = re.compile(
    r"\b_ui_label_set_property\s*\(\s*(\w+)\s*,\s*"
    r"_UI_LABEL_PROPERTY_TEXT\s*,\s*((?:\"(?:\\.|[^\"\\])*\"\s*)+)",
    re.DOTALL,
)
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"', re.DOTALL)
GLYPH_RE = re.compile(r"/\* U\+([0-9A-Fa-f]+)")


def _strip_comments(source):
    """移除 C 注释，同时保留字符串中的注释符号。"""
    output = []
    index = 0
    quote = None
    while index < len(source):
        char = source[index]
        pair = source[index:index + 2]
        if quote:
            output.append(char)
            if char == "\\" and index + 1 < len(source):
                index += 1
                output.append(source[index])
            elif char == quote:
                quote = None
        elif char in {'"', "'"}:
            quote = char
            output.append(char)
        elif pair == "//":
            index = source.find("\n", index)
            if index < 0:
                break
            output.append("\n")
        elif pair == "/*":
            end = source.find("*/", index + 2)
            index = len(source) if end < 0 else end + 1
        else:
            output.append(char)
        index += 1
    return "".join(output)


def _decode_strings(expression):
    """解码相邻 C 字符串字面量。"""
    return "".join(ast.literal_eval(token) for token in STRING_RE.findall(expression))


def _resolve_font(obj, fonts, parents):
    """沿 LVGL 父对象链查找对象继承的字体。"""
    visited = set()
    while obj not in visited:
        visited.add(obj)
        if obj in fonts:
            return fonts[obj]
        obj = parents.get(obj)
        if obj is None:
            return None
    return None


def scan_symbols(sources):
    """返回源码中每个自定义字体实际显示的静态字符集合。"""
    cleaned = [_strip_comments(source) for source in sources]
    global_parents = {}
    global_fonts = {}
    for source in cleaned:
        global_parents.update(
            (child, parent) for child, parent in CREATE_RE.findall(source) if child.startswith("ui_")
        )
        global_fonts.update(
            (obj, font) for obj, font in FONT_RE.findall(source) if obj.startswith("ui_")
        )

    symbols = {font: set() for font in set(global_fonts.values())}
    for source in cleaned:
        parents = dict(global_parents)
        parents.update(CREATE_RE.findall(source))
        fonts = dict(global_fonts)
        fonts.update(FONT_RE.findall(source))
        symbols.update((font, symbols.get(font, set())) for font in fonts.values())
        for obj, expression in TEXT_RE.findall(source) + PROPERTY_TEXT_RE.findall(source):
            font = _resolve_font(obj, fonts, parents)
            if font:
                symbols.setdefault(font, set()).update(_decode_strings(expression))

    for characters in symbols.values():
        characters.difference_update({"\0", "\n", "\r", "\t"})
    return symbols


def _converter_path():
    for local_converter in (LOCAL_CONVERTER, LOCAL_CONVERTER.with_suffix(".cmd")):
        if local_converter.is_file():
            return str(local_converter)
    converter = shutil.which("lv_font_conv")
    if converter:
        return converter
    raise SystemExit("缺少 lv_font_conv，请运行: npm install --prefix tools/lv_font_conv")


def _input_hash(font_path, size, bpp, symbols):
    digest = hashlib.sha256(font_path.read_bytes())
    digest.update(Path(__file__).read_bytes())
    digest.update(f"{size}:{bpp}:{symbols}".encode("utf-8"))
    return digest.hexdigest()


def missing_glyphs(generated_source, symbols):
    """返回转换结果中缺失的字符。"""
    generated = {int(codepoint, 16) for codepoint in GLYPH_RE.findall(generated_source)}
    return {character for character in symbols if ord(character) not in generated}


def _generate_font(converter, name, spec, symbols):
    font_file, size, bpp, dynamic_symbols = spec
    font_path = FONT_INPUT_DIR / font_file
    symbols = (
        dynamic_symbols
        if name == "ui_font_PuHuiTi16GB2312"
        else "".join(sorted(set(symbols).union(dynamic_symbols)))
    )
    input_hash = _input_hash(font_path, size, bpp, symbols)
    output = FONT_DIR / f"{name}.c"
    if output.is_file() and f"Font-Input-SHA256: {input_hash}" in output.read_text(
        encoding="utf-8", errors="ignore"
    )[:512]:
        return output

    with tempfile.NamedTemporaryFile(suffix=".c", dir=FONT_DIR, delete=False) as temporary:
        temporary_path = Path(temporary.name)
    try:
        subprocess.run(
            [
                converter,
                "--no-compress",
                "--no-prefilter",
                "--bpp",
                str(bpp),
                "--size",
                str(size),
                "--font",
                str(font_path),
                "--symbols",
                symbols,
                "--format",
                "lvgl",
                "--lv-include",
                "../ui.h",
                "--lv-font-name",
                name,
                "-o",
                str(temporary_path),
            ],
            check=True,
        )
        header = (
            "/*\n"
            " * SPDX-License-Identifier: MIT\n"
            " * Origin: Generated from repository fonts by lv_font_conv.\n"
            " * Created-By: gpt-5\n"
            " * Signed-off-by: xcwynya\n"
            f" * Font-Input-SHA256: {input_hash}\n"
            " */\n"
        )
        body = temporary_path.read_text(encoding="utf-8")
        missing = missing_glyphs(body, symbols)
        if missing:
            raise RuntimeError(f"{font_file} 缺少字模: {''.join(sorted(missing))}")
        body = body.replace(str(font_path), font_path.relative_to(ROOT).as_posix())
        body = body.replace(str(temporary_path), output.relative_to(ROOT).as_posix())
        if name == "ui_font_PuHuiTi16GB2312":
            body = body.replace(".fallback = NULL,", ".fallback = &lv_font_montserrat_12,")
            body = body.rstrip() + "\n"
        output.write_text(header + body, encoding="utf-8")
        print(f"已生成 {output.relative_to(ROOT)}（{len(symbols)} 个字符）", file=sys.stderr)
    finally:
        temporary_path.unlink(missing_ok=True)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--print-files", action="store_true", help="输出参与编译的字体源文件")
    args = parser.parse_args()
    sources = [path.read_text(encoding="utf-8") for path in UI_DIR.rglob("*.c") if path.parent != FONT_DIR]
    scanned = scan_symbols(sources)
    used_fonts = sorted(set(scanned).intersection(FONT_SPECS))
    converter = _converter_path()
    outputs = [_generate_font(converter, name, FONT_SPECS[name], scanned[name]) for name in used_fonts]
    if args.print_files:
        print("\n".join(str(path) for path in outputs))


if __name__ == "__main__":
    main()
