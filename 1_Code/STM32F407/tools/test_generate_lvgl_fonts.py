# SPDX-License-Identifier: MIT
# Origin: Generated for this repository's LVGL font optimization.
# Created-By: gpt-5
# Signed-off-by: xcwynya

import unittest

from generate_lvgl_fonts import missing_glyphs, scan_symbols


class ScanSymbolsTest(unittest.TestCase):
    def test_collects_inherited_text_and_excludes_other_fonts(self):
        sources = [
            """
            ui_panel = lv_obj_create(screen);
            ui_label = lv_label_create(ui_panel);
            lv_obj_set_style_text_font(ui_panel, &ui_font_test, 0);
            lv_label_set_text(ui_label, "中文 A");

            other = lv_label_create(screen);
            lv_obj_set_style_text_font(other, &ui_font_other, 0);
            lv_label_set_text(other, "不应收录");
            """,
            'lv_label_set_text(ui_label, "动态补充");',
            '_ui_label_set_property(ui_label, _UI_LABEL_PROPERTY_TEXT, "DISABLE");',
        ]

        self.assertEqual(set("中文 A动态补充DISABLE"), scan_symbols(sources)["ui_font_test"])

    def test_reports_missing_generated_glyph(self):
        generated = '/* U+0041 "A" */'

        self.assertEqual({"中"}, missing_glyphs(generated, "A中"))


if __name__ == "__main__":
    unittest.main()
