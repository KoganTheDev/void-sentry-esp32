import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import coverage_gate  # noqa: E402

LIB = "/home/runner/work/repo/ESP32/lib"
TRACE = f"""TN:
SF:{LIB}/remote/src/remote_command.cpp
LF:10
LH:8
BRF:4
BRH:2
end_of_record
SF:{LIB}/remote/src/remote_inbox.cpp
LF:10
LH:10
end_of_record
SF:{LIB}/network/src/wifi_http.cpp
LF:50
LH:0
end_of_record
SF:/usr/include/c++/13/bits/stl_vector.h
LF:99
LH:1
end_of_record
"""


class ParseTest(unittest.TestCase):
    def test_groups_by_library_and_skips_system_headers(self):
        libs = coverage_gate.parse(TRACE)
        self.assertEqual(set(libs), {"remote", "network"})
        self.assertEqual((libs["remote"].lines_found, libs["remote"].lines_hit), (20, 18))
        self.assertEqual((libs["remote"].branches_found, libs["remote"].branches_hit), (4, 2))

    def test_windows_paths(self):
        libs = coverage_gate.parse("SF:C:\\r\\ESP32\\lib\\aiming\\src\\a.cpp\nLF:4\nLH:1\nend_of_record\n")
        self.assertEqual(libs["aiming"].lines_hit, 1)


class ReportTest(unittest.TestCase):
    def test_only_pure_libs_count_toward_total(self):
        _, total = coverage_gate.report(coverage_gate.parse(TRACE))
        self.assertAlmostEqual(total, 90.0)

    def test_empty_library_counts_as_full(self):
        self.assertEqual(coverage_gate.pct(0, 0), 100.0)


class GateTest(unittest.TestCase):
    def run_gate(self, text, floor, branch_floor=None):
        with tempfile.NamedTemporaryFile("w", suffix=".info", delete=False) as f:
            f.write(text)
        try:
            os.environ.pop("GITHUB_STEP_SUMMARY", None)
            args = [f.name, "--floor", str(floor)]
            if branch_floor is not None:
                args += ["--branch-floor", str(branch_floor)]
            return coverage_gate.main(args)
        finally:
            os.unlink(f.name)

    def full_trace(self):
        return "".join(f"SF:{LIB}/{name}/src/x.cpp\nLF:10\nLH:9\nend_of_record\n" for name in coverage_gate.PURE_LIBS)

    def test_passes_at_or_above_floor(self):
        self.assertEqual(self.run_gate(self.full_trace(), 90), 0)

    def test_fails_below_floor(self):
        self.assertEqual(self.run_gate(self.full_trace(), 91), 1)

    def branch_trace(self, hit, found):
        return "".join(
            f"SF:{LIB}/{name}/src/x.cpp\nLF:10\nLH:10\nBRF:{found}\nBRH:{hit}\nend_of_record\n"
            for name in coverage_gate.PURE_LIBS
        )

    def test_branch_floor_passes_at_or_above(self):
        self.assertEqual(self.run_gate(self.branch_trace(9, 10), 0, branch_floor=90), 0)

    def test_branch_floor_fails_below(self):
        self.assertEqual(self.run_gate(self.branch_trace(9, 10), 0, branch_floor=91), 1)

    def test_default_branch_floor_is_applied(self):
        low = int(coverage_gate.BRANCH_FLOOR_PERCENT) - 1
        self.assertEqual(self.run_gate(self.branch_trace(low, 100), 0), 1)
        self.assertEqual(self.run_gate(self.branch_trace(int(coverage_gate.BRANCH_FLOOR_PERCENT), 100), 0), 0)

    def test_no_branch_data_counts_as_full(self):
        self.assertEqual(self.run_gate(self.full_trace(), 90), 0)

    def test_non_pure_libs_do_not_count_toward_branch_floor(self):
        text = self.branch_trace(100, 100) + f"SF:{LIB}/network/src/y.cpp\nLF:1\nLH:1\nBRF:100\nBRH:0\nend_of_record\n"
        self.assertEqual(self.run_gate(text, 0, branch_floor=99), 0)

    def test_fails_when_a_pure_lib_is_missing(self):
        self.assertEqual(self.run_gate(TRACE, 0), 1)


if __name__ == "__main__":
    unittest.main()
