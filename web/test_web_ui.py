"""Tests for web_ui.html and wifi_setup.html: run with `python -m unittest discover -s web -p "test_*.py"` from the repo root.

The pages are plain files with no build step, so these checks read the source and look for the
mistakes that have bitten this HUD before: an external asset that breaks the offline hotspot mode,
unbalanced markup, a /commands message the firmware does not parse, a zoom lock that hurts accessibility.
"""

import os
import re
import unittest
from html.parser import HTMLParser

WEB_DIR = os.path.dirname(os.path.abspath(__file__))

VOID_ELEMENTS = frozenset(
    ["area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr"]
)

# Attributes that make the browser fetch something.
URL_ATTRIBUTES = ("src", "href", "srcset", "action", "formaction", "poster", "data", "manifest", "xlink:href")

# Every /commands message the HUD may send, by static prefix. Must match the firmware's parser.
ACCEPTED_COMMAND_PREFIXES = (" ", "move:", "mode:ai", "mode:user", "cal:start", "cal:point", "cal:cancel", "aim:", "speed:")

# 'mode:' + m is built at run time; the tail is checked by test_mode_requests_send_only_ai_or_user.
DYNAMIC_TAIL_PREFIXES = ("mode:",)

SEND_COMMAND_CALL = re.compile(
    r"""sendCommand\(\s*(?:'((?:\\.|[^'\\\n])*)'|"((?:\\.|[^"\\\n])*)"|`([^`]*)`)"""
)
REQUEST_MODE_CALL = re.compile(r"(?<!function )requestMode\(([^)]*)\)")
FETCH_OR_SOCKET_CALL = re.compile(r"\b(?:fetch|WebSocket)\s*\(")
FETCH_OR_SOCKET_LITERAL = re.compile(
    r"""\b(?:fetch|WebSocket)\s*\(\s*(?:'([^'\n]*)'|"([^"\n]*)"|`([^`]*)`)"""
)
# A quoted URL with a scheme or protocol-relative. 'https:' alone is a protocol comparison, not a URL.
ABSOLUTE_LITERAL = re.compile(r"""(['"`])(?:(?:https?|wss?):)?//""", re.IGNORECASE)
CSS_URL = re.compile(r"""url\(\s*(['"]?)(.*?)\1\s*\)""", re.IGNORECASE | re.DOTALL)
CSS_IMPORT = re.compile(r"""@import\s+(?:url\(\s*)?(['"]?)([^'"\s;)]*)""", re.IGNORECASE)
EM_DASH = chr(0x2014)  # written as chr() so this file itself contains no dash character


def read_page(name):
    with open(os.path.join(WEB_DIR, name), encoding="utf-8") as f:
        return f.read()


def is_external(url):
    """True for an absolute http(s)/ws(s) URL or a protocol-relative //host URL. data: and relative paths are fine."""
    return url.strip().lower().startswith(("http:", "https:", "ws:", "wss:", "//"))


class _Document(HTMLParser):
    """Collects start tags, script and style bodies, and nesting errors. Script/style bodies are not parsed as markup."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.stack = []
        self.errors = []
        self.starts = []  # (tag, {attribute: value}) for every start tag
        self.scripts = []
        self.styles = []

    def handle_starttag(self, tag, attrs):
        self._record(tag, attrs)
        if tag not in VOID_ELEMENTS:
            self.stack.append(tag)

    def handle_startendtag(self, tag, attrs):
        # XML-style <path/> is treated as already closed (inline SVG uses it).
        self._record(tag, attrs)

    def handle_endtag(self, tag):
        if tag in VOID_ELEMENTS:
            self.errors.append(f"end tag </{tag}> for a void element")
        elif tag not in self.stack:
            self.errors.append(f"stray end tag </{tag}>")
        elif self.stack[-1] == tag:
            self.stack.pop()
        else:
            self.errors.append(f"</{tag}> closes while <{self.stack[-1]}> is still open")
            while self.stack.pop() != tag:
                pass

    def handle_data(self, data):
        if self.stack and self.stack[-1] == "script":
            self.scripts.append(data)
        elif self.stack and self.stack[-1] == "style":
            self.styles.append(data)

    def _record(self, tag, attrs):
        self.starts.append((tag, {name: (value or "") for name, value in attrs}))


def parse_document(text):
    doc = _Document()
    doc.feed(text)
    doc.close()
    doc.errors.extend(f"<{tag}> is never closed" for tag in doc.stack)
    return doc


def external_attribute_urls(doc):
    hits = []
    for tag, attrs in doc.starts:
        for name in URL_ATTRIBUTES:
            value = attrs.get(name)
            if value is None:
                continue
            candidates = [value]
            if name == "srcset":
                candidates = [part.split()[0] for part in value.split(",") if part.strip()]
            for url in candidates:
                if is_external(url):
                    hits.append(f"<{tag} {name}={url}>")
    return hits


def external_css_urls(css_texts):
    hits = []
    for css in css_texts:
        hits.extend(m.group(2) for m in CSS_URL.finditer(css) if is_external(m.group(2)))
        hits.extend("@import " + m.group(2) for m in CSS_IMPORT.finditer(css) if is_external(m.group(2)))
    return hits


def css_sources(doc):
    """Every CSS body in the page: <style> blocks and style="" attributes."""
    attr_css = [attrs["style"] for _, attrs in doc.starts if "style" in attrs]
    return doc.styles + attr_css


def command_calls(script):
    """(static prefix, is_dynamic, matched text) for every sendCommand call with a literal first argument."""
    calls = []
    for m in SEND_COMMAND_CALL.finditer(script):
        literal = next(g for g in m.groups() if g is not None)
        static = literal.split("${", 1)[0]
        followed_by_plus = re.match(r"\s*\+", script[m.end():]) is not None
        dynamic = "${" in literal or followed_by_plus
        calls.append((static, dynamic, m.group(0)))
    return calls


def command_accepted(static, dynamic):
    if static.startswith(ACCEPTED_COMMAND_PREFIXES):
        return True
    return dynamic and static in DYNAMIC_TAIL_PREFIXES


def requested_modes(script, data_modes):
    """Every mode value that requestMode() can receive from the page. Raises on an argument it cannot read."""
    values = set()
    for m in REQUEST_MODE_CALL.finditer(script):
        arg = m.group(1).strip()
        if arg == "b.dataset.mode":
            values |= set(data_modes)
            continue
        literals = [a or b for a, b in re.findall(r"'(\w+)'|\"(\w+)\"", arg)]
        if not literals:
            raise ValueError(f"unrecognised requestMode argument: {arg!r}")
        values |= set(literals)
    return values


class CommonPageChecks:
    """Checks that apply to both pages. Subclasses set `page`."""

    page = None

    def setUp(self):
        self.text = read_page(self.page)
        self.doc = parse_document(self.text)
        self.script = "\n".join(self.doc.scripts)
        self.assertTrue(self.script.strip(), "page has no script text; the script checks would pass vacuously")

    def test_no_external_attribute_urls(self):
        self.assertEqual(external_attribute_urls(self.doc), [])

    def test_no_external_css_urls_or_imports(self):
        self.assertEqual(external_css_urls(css_sources(self.doc)), [])

    def test_scripts_have_no_absolute_urls(self):
        self.assertIsNone(ABSOLUTE_LITERAL.search(self.script), "absolute URL literal in a script")

    def test_fetch_and_websocket_targets_are_same_origin(self):
        self.assertTrue(FETCH_OR_SOCKET_CALL.search(self.script), "no fetch/WebSocket calls found; check is vacuous")
        for m in FETCH_OR_SOCKET_LITERAL.finditer(self.script):
            literal = next(g for g in m.groups() if g is not None)
            if literal.startswith("${"):
                # Built from a variable: must come from location (the page's own origin).
                self.assertIn("location", literal, f"target not built from location: {m.group(0)}")
            else:
                self.assertFalse(is_external(literal), f"absolute fetch/WebSocket target: {m.group(0)}")

    def test_markup_is_balanced(self):
        self.assertEqual(self.doc.errors, [])

    def test_no_em_dash(self):
        self.assertNotIn(EM_DASH, self.text)


class WebUiTests(CommonPageChecks, unittest.TestCase):
    page = "web_ui.html"

    def test_send_command_literals_have_accepted_prefixes(self):
        bad = [call for static, dynamic, call in command_calls(self.script)
               if not command_accepted(static, dynamic)]
        self.assertEqual(bad, [], "sendCommand literal with an unaccepted prefix")

    def test_send_command_call_count_is_plausible(self):
        # Guards the regex: if it stopped matching, the prefix test above would pass on nothing.
        self.assertGreaterEqual(len(command_calls(self.script)), 8)

    def test_fire_is_never_sent(self):
        for static, _dynamic, call in command_calls(self.script):
            self.assertFalse(static.startswith("fire"), f"fire command sent: {call}")

    def test_mode_requests_send_only_ai_or_user(self):
        # 'mode:' + m sends whatever requestMode receives. Calibrate returns before the send, so the rest must be ai/user.
        data_modes = re.findall(r'data-mode="(\w+)"', self.text)
        self.assertTrue(data_modes, "no data-mode buttons found; check is vacuous")
        values = requested_modes(self.script, data_modes)
        sent = values - {"calibrate"}
        self.assertTrue(sent)
        self.assertLessEqual(sent, {"ai", "user"})

    def test_viewport_allows_zoom(self):
        viewports = [attrs.get("content", "") for tag, attrs in self.doc.starts
                     if tag == "meta" and attrs.get("name", "").lower() == "viewport"]
        self.assertEqual(len(viewports), 1)
        content = viewports[0].lower().replace(" ", "")
        self.assertNotRegex(content, r"user-scalable=(no|0)")
        self.assertNotRegex(content, r"maximum-scale=1(\.0+)?(?![\d.])")

    def test_honours_reduced_motion(self):
        self.assertIn("prefers-reduced-motion", self.text)

    def test_no_console_log(self):
        self.assertNotIn("console.log(", self.text)


class WifiSetupTests(CommonPageChecks, unittest.TestCase):
    page = "wifi_setup.html"


class CheckerSelfTests(unittest.TestCase):
    """The checkers must catch the defects they are meant for, or the page tests pass for the wrong reason."""

    def test_markup_checker_flags_mismatch_and_unclosed(self):
        self.assertTrue(parse_document("<div><span></div></span>").errors)
        self.assertIn("<main> is never closed", parse_document("<main>").errors)

    def test_markup_checker_ignores_script_and_style_bodies(self):
        html_text = "<style>a > b {}</style><script>if (a < b) { x('<div>'); }</script><p>x</p>"
        self.assertEqual(parse_document(html_text).errors, [])

    def test_url_checkers_flag_external_and_allow_data(self):
        doc = parse_document(
            '<link rel="stylesheet" href="https://example.test/a.css">'
            '<img src="data:image/png;base64,AA//BB">'
        )
        self.assertEqual(len(external_attribute_urls(doc)), 1)
        hits = external_css_urls(["@import url(https://example.test/a.css); "
                                  "p{background:url(//cdn.example.test/x.png)} i{background:url(data:image/png;base64,AA//)}"])
        self.assertIn("https://example.test/a.css", hits)
        self.assertIn("//cdn.example.test/x.png", hits)
        self.assertFalse(any(h.startswith("data:") for h in hits))


if __name__ == "__main__":
    unittest.main()
