import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "runtime.cpp"


def compiler_command():
    configured = os.environ.get("CXX")
    candidates = [shutil.which(configured)] if configured else []
    candidates.extend(shutil.which(name) for name in ("clang++", "g++", "c++"))
    return next((candidate for candidate in candidates if candidate), None)


class RuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = compiler_command()
        if not cls.compiler:
            raise unittest.SkipTest("Install clang++, g++, or c++ to run DOSCU tests")
        cls.build_dir = tempfile.TemporaryDirectory(prefix="doscu-tests-")
        cls.binary = Path(cls.build_dir.name) / "doscu"
        subprocess.run(
            [cls.compiler, "-std=c++17", str(RUNTIME), "-o", str(cls.binary)],
            check=True,
            capture_output=True,
            text=True,
        )

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "build_dir"):
            cls.build_dir.cleanup()

    def run_source(self, source, files=None):
        with tempfile.TemporaryDirectory(prefix="doscu-case-") as temp:
            root = Path(temp)
            for name, contents in (files or {}).items():
                (root / name).write_text(contents, encoding="utf-8")
            program = root / "main.doscu"
            program.write_text(source, encoding="utf-8")
            return subprocess.run(
                [str(self.binary), str(program)],
                capture_output=True,
                text=True,
                check=False,
            )

    def test_header_function_return_and_part_output(self):
        result = self.run_source('''
#head() {
    int score = 0;
    int add(int left, int right) { return left + right; }
}
#part("main", true) {
    score = add(2, 3);
    cout << "Score: " << score << endl;
}
''')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "Score: 5\n")

    def test_break_and_continue(self):
        result = self.run_source('''
#head() { int total = 0; }
#part("loop", true) {
    int i = 0;
    while (i < 5) {
        i += 1;
        if (i == 2) { continue; }
        if (i == 4) { break; }
        total += i;
    }
    cout << total << endl;
}
''')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "4\n")

    def test_wait_continues_with_next_statement(self):
        result = self.run_source('''
#part("wait", true) {
    cout << "A";
    wait(1);
    cout << "B" << endl;
}
''')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "AB\n")

    def test_stop_prevents_later_statements(self):
        result = self.run_source('''
#part("stop", true) {
    cout << "before" << endl;
    stop();
    cout << "after" << endl;
}
''')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "before\n")

    def test_include_percent_and_disabled_part(self):
        result = self.run_source('''
#include "helper.doscu"
#part("disabled", false) { print("wrong"); }
#part("active", true) { cout << triple(2) << ":" << 25% << endl; }
        ''', {"helper.doscu": '#head() { int triple(int n) { return n * 3; } }'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "6:0.25\n")

    def test_file_runner_compiles_and_uses_selected_source(self):
        with tempfile.TemporaryDirectory(prefix="doscu-runner-") as temp:
            source = Path(temp) / "selected.doscu"
            source.write_text(
                '#part("selected", true) { cout << "selected" << endl; }',
                encoding="utf-8",
            )
            result = subprocess.run(
                ["python3", str(ROOT / "run-doscu.py"), str(source)],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("selected", result.stdout)


if __name__ == "__main__":
    unittest.main()
