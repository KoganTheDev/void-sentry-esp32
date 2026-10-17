"""Tests for html_header: run with `python -m unittest discover -s web -p "test_*.py"` from the repo root."""

import gzip
import os
import re
import tempfile
import unittest

import html_header

SAMPLE = b"<!doctype html><html><body><h1>Void Sentry</h1></body></html>"


def _array_bytes(header: str) -> bytes:
    body = re.search(r"HTML_PAGE_GZ\[\] PROGMEM = \{(.*?)\};", header, re.S).group(1)
    return bytes(int(v) for v in body.replace("\n", " ").split(",") if v.strip())


class RenderHeaderTests(unittest.TestCase):
    def test_array_decompresses_to_input(self):
        header = html_header.render_header(SAMPLE)
        self.assertEqual(gzip.decompress(_array_bytes(header)), SAMPLE)

    def test_length_constant_matches_array(self):
        header = html_header.render_header(SAMPLE)
        length = int(re.search(r"HTML_PAGE_GZ_LEN = (\d+);", header).group(1))
        self.assertEqual(length, len(_array_bytes(header)))

    def test_output_is_deterministic(self):
        # gzip normally embeds the current time; the header must not change between builds
        self.assertEqual(html_header.render_header(SAMPLE), html_header.render_header(SAMPLE))

    def test_header_keeps_firmware_contract(self):
        header = html_header.render_header(SAMPLE)
        self.assertIn("#pragma once", header)
        self.assertIn("#include <Arduino.h>", header)
        self.assertIn("static const size_t HTML_PAGE_GZ_LEN", header)
        self.assertIn("static const uint8_t HTML_PAGE_GZ[] PROGMEM", header)
        self.assertIn("generated", header.lower())


def _wifi_array_bytes(header: str) -> bytes:
    body = re.search(r"WIFI_PAGE_GZ\[\] PROGMEM = \{(.*?)\};", header, re.S).group(1)
    return bytes(int(v) for v in body.replace("\n", " ").split(",") if v.strip())


class RenderWifiHeaderTests(unittest.TestCase):
    def test_array_decompresses_to_input(self):
        header = html_header.render_wifi_header(SAMPLE)
        self.assertEqual(gzip.decompress(_wifi_array_bytes(header)), SAMPLE)

    def test_length_constant_matches_array(self):
        header = html_header.render_wifi_header(SAMPLE)
        length = int(re.search(r"WIFI_PAGE_GZ_LEN = (\d+);", header).group(1))
        self.assertEqual(length, len(_wifi_array_bytes(header)))

    def test_output_is_deterministic(self):
        self.assertEqual(html_header.render_wifi_header(SAMPLE), html_header.render_wifi_header(SAMPLE))

    def test_header_contract_and_no_symbol_clash_with_index_header(self):
        header = html_header.render_wifi_header(SAMPLE)
        self.assertIn("#pragma once", header)
        self.assertIn("static const size_t WIFI_PAGE_GZ_LEN", header)
        self.assertIn("static const uint8_t WIFI_PAGE_GZ[] PROGMEM", header)
        self.assertNotIn("HTML_PAGE_GZ", header)
        self.assertNotIn("WIFI_PAGE_GZ", html_header.render_header(SAMPLE))

    def test_same_payload_same_bytes_as_index_header(self):
        self.assertEqual(_wifi_array_bytes(html_header.render_wifi_header(SAMPLE)),
                         _array_bytes(html_header.render_header(SAMPLE)))


class WriteIfChangedTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.path = os.path.join(self.dir.name, "index_html.h")

    def tearDown(self):
        self.dir.cleanup()

    def test_creates_missing_file(self):
        self.assertTrue(html_header.write_if_changed(self.path, "a"))
        with open(self.path, encoding="utf-8") as f:
            self.assertEqual(f.read(), "a")

    def test_identical_content_is_not_rewritten(self):
        html_header.write_if_changed(self.path, "a")
        os.utime(self.path, (1_000_000, 1_000_000))
        self.assertFalse(html_header.write_if_changed(self.path, "a"))
        self.assertEqual(os.path.getmtime(self.path), 1_000_000)

    def test_changed_content_is_rewritten(self):
        html_header.write_if_changed(self.path, "a")
        self.assertTrue(html_header.write_if_changed(self.path, "b"))
        with open(self.path, encoding="utf-8") as f:
            self.assertEqual(f.read(), "b")


if __name__ == "__main__":
    unittest.main()
