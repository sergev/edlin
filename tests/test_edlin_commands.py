"""
Integration tests: drive ./edlin via pexpect and assert on stdout + files.

Requires: pip install -r requirements-dev.txt (pexpect).
"""

from __future__ import annotations

import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

try:
    import pexpect
except ImportError:
    pexpect = None


REPO_ROOT = Path(__file__).resolve().parent.parent
EDLIN_BIN = REPO_ROOT / "edlin"

# Output after "New file" then main command prompt (TTY uses \\n).
RE_NEWFILE_PROMPT = re.compile(r"New file\r?\n\*")
# Blank-line edit: DOS-style "%1:%2" header + displayed text + newline (same as DISPLAY).
RE_LINE_HEADER = re.compile(r"\s*\d{1,6}:[\* ][^\r\n]*\r?\n")
# Insert / line-replace: msg_line_prompt — no newline after the '     n:*' prefix before stdin.
RE_LINE_PROMPT_TAIL = re.compile(r"\s*\d{1,6}:\*")


def setUpModule():
    if pexpect is None:
        raise unittest.SkipTest("pexpect is required: pip install -r requirements-dev.txt")
    if not EDLIN_BIN.is_file():
        raise unittest.SkipTest(f"edlin binary not found at {EDLIN_BIN} — run `make` first")


def run_edlin_script(workdir: Path, filepath: str, stdin: bytes, timeout: float = 30) -> subprocess.CompletedProcess:
    """Run edlin with raw stdin bytes (reliable for Ctrl-Z during insert; pipe semantics like bash)."""
    return subprocess.run(
        [str(EDLIN_BIN), filepath],
        cwd=str(workdir),
        input=stdin,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
        check=False,
    )


class EdlinSession:
    """Spawn edlin in a temp cwd with optional extra env."""

    def __init__(self, workdir: Path, env: dict | None = None):
        self.workdir = workdir
        self.env = {**os.environ, **(env or {})}
        self.child: pexpect.spawn | None = None

    def spawn(self, argv: list[str], test: unittest.TestCase | None = None):
        self.child = pexpect.spawn(
            argv[0],
            argv[1:],
            cwd=str(self.workdir),
            env=self.env,
            encoding="utf-8",
            timeout=15,
        )
        if test is not None:
            test.addCleanup(self.close)

    def expect_prompt(self):
        """Main `*` prompt — not the `*` in column listings (`     12:* `)."""
        assert self.child is not None
        self.child.expect(re.compile(r"(?:^|[\r\n])\*"))

    def expect_new_file_then_prompt(self):
        assert self.child is not None
        self.child.expect(RE_NEWFILE_PROMPT)

    def expect_line_header(self):
        """Wait for msg_line_out blank-line header (newline-terminated) before stdin read."""
        assert self.child is not None
        self.child.expect(RE_LINE_HEADER)

    def expect_line_prompt_tail(self):
        """Wait for msg_line_prompt (no newline until user types) — same as insert mode."""
        assert self.child is not None
        self.child.expect(RE_LINE_PROMPT_TAIL)

    def expect_entry_error_prompt(self):
        assert self.child is not None
        # Pty translates newline to CRLF on many platforms.
        self.child.expect(re.compile(r"Entry error\r?\n\*"))

    def expect_message_then_prompt(self, msg: str):
        """Message line followed by main * prompt."""
        assert self.child is not None
        self.child.expect(re.compile(re.escape(msg) + r"\r?\n\*"))

    def send_line(self, s: str):
        assert self.child is not None
        self.child.sendline(s)

    def send_eof(self):
        assert self.child is not None
        self.child.sendeof()

    def expect_eof(self, timeout=10):
        assert self.child is not None
        self.child.expect(pexpect.EOF, timeout=timeout)

    def wait_exit(self):
        assert self.child is not None
        self.child.wait()
        return self.child.exitstatus

    def close(self):
        if self.child and self.child.isalive():
            self.child.close(force=True)
        self.child = None

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass



class TestInvocation(unittest.TestCase):
    """Startup / argv / fileio_startup."""

    def test_missing_filename_exits_1(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            child = pexpect.spawn(
                str(EDLIN_BIN),
                cwd=str(wd),
                encoding="utf-8",
                timeout=5,
            )
            self.addCleanup(child.close, True)
            child.expect(pexpect.EOF)
            child.wait()
            self.assertEqual(child.exitstatus, 1)
            self.assertIn("File name must be specified", child.before)

    def test_bak_extension_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            f = wd / "x.bak"
            f.write_text("nope\n", encoding="ascii")
            child = pexpect.spawn(str(EDLIN_BIN), [str(f)], cwd=str(wd), encoding="utf-8", timeout=5)
            self.addCleanup(child.close, True)
            child.expect(pexpect.EOF)
            child.wait()
            self.assertEqual(child.exitstatus, 1)
            self.assertIn("Cannot edit .BAK file", child.before)

    def test_new_file_message(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "new.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_eof()
            s.expect_eof()

    def test_binary_flag_accepted(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "bin.txt"
            # 0x1A mid-stream: binary mode should keep following bytes as data
            p.write_bytes(b"a\n\x1bb\n")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), "/B", str(p)], self)
            s.expect_prompt()
            s.send_line("L")
            s.expect_prompt()
            out = s.child.before
            self.assertIn("^[", out)  # display of control character
            s.send_eof()
            s.expect_eof()


class TestParserChains(unittest.TestCase):
    """Semicolon chains, invalid recovery, numeric forms."""

    def test_semicolon_two_commands(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            r = run_edlin_script(
                wd,
                str(p),
                b"1I\nhello\n\x1a\nL;L\nq\ny\n",
            )
            self.assertEqual(r.returncode, 0)
            self.assertIn(b"hello", r.stdout)

    def test_ctrl_z_command_separator(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            r = run_edlin_script(
                wd,
                str(p),
                b"1I\na\n\x1a\nL\x1aL\nq\ny\n",
            )
            self.assertEqual(r.returncode, 0)

    def test_lowercase_command(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line("l")
            s.expect_prompt()

    def test_help_command(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line("H")
            s.expect_prompt()
            out = s.child.before
            self.assertIn("Commands:", out)
            self.assertIn("Print this help", out)

    def test_help_lowercase(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line("h")
            s.expect_prompt()
            self.assertIn("Commands:", s.child.before)

    def test_help_numeric_prefix_entry_error(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line("1H")
            s.expect_entry_error_prompt()

    def test_invalid_command_entry_error(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line("X")
            s.expect_entry_error_prompt()

    def test_parse_err_skips_to_semicolon_then_runs(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line("0L;l")
            s.expect_entry_error_prompt()

    def test_range_backward_entry_error(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("a\nb\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("2,1L")
            s.expect_entry_error_prompt()

    def test_lineref_dot_hash_plus_minus(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("l1\nl2\nl3\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            # Set current to line 2
            s.send_line("Sl2")
            s.expect_prompt()
            s.send_line("+1L")  # current+1 = 3 → list from line 3 default window
            s.expect_prompt()
            s.send_line("-1L")
            s.expect_prompt()
            s.send_line("#L")  # # = count+1 = 4 as start → entry error for L start > last
            s.expect_entry_error_prompt()


class TestSemicolonNoop(unittest.TestCase):
    def test_semicolon_alone(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line(";")
            s.expect_prompt()


class TestInsertDeleteBlank(unittest.TestCase):
    def test_insert_prompt_not_followed_by_newline_on_stdout(self):
        """Insert uses msg_line_prompt (no newline after the '     1:*' prefix before read).

        Pipe stdin is used so Ctrl-Z reaches the program as 0x1A (unlike many PTYs where
        Ctrl-Z is job-control). Quit without list so we do not match a blank msg_line_out line.
        """
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            r = run_edlin_script(wd, str(p), b"1I\n\x1a\nq\ny\n")
            self.assertEqual(r.returncode, 0)
            self.assertNotIn(b"     1:*\n", r.stdout)

    def test_insert_ctrl_z_exit(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            r = run_edlin_script(wd, str(p), b"1I\nfirst\n\x1a\nL\nq\ny\n")
            self.assertEqual(r.returncode, 0)
            self.assertIn(b"first", r.stdout)

    def test_insert_dot_exit(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            # Save with E so we assert '.' did not become a second buffer line (prompts contain `     2:`).
            r = run_edlin_script(wd, str(p), b"1I\nfirst\n.\nE\n")
            self.assertEqual(r.returncode, 0)
            raw = p.read_bytes().rstrip(b"\x1a")
            lines = [ln for ln in raw.split(b"\n") if ln != b""]
            self.assertEqual(lines, [b"first"])

    def test_insert_before_current_default(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("a\nb\n", encoding="ascii")
            r = run_edlin_script(wd, str(p), b"I\nnew\n\x1a\n1,4L\nq\ny\n")
            self.assertEqual(r.returncode, 0)
            self.assertIn(b"new", r.stdout)

    def test_blank_line_edit_replace(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("old\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("1")
            s.expect_line_header()
            s.send_line("new")
            s.expect_prompt()
            s.send_line("L")
            s.expect_prompt()
            self.assertIn("new", s.child.before)

    def test_blank_line_edit_hash_eof_no_read(self):
        """Bare `#` (last+1) matches classic NOCOM: move to EOF pseudo-line, no stdin prompt."""
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("a\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("#")
            s.expect_prompt()
            s.send_line("L")
            s.expect_prompt()
            out = s.child.before
            self.assertIn("a", out)
            # No second line of user text was added by bare `#`
            self.assertNotRegex(out, r"(?m)^\s*2:")

    def test_hash_insert_appends_at_end(self):
        """`#I` inserts before line last+1 — the way to append after the last line."""
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("a\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("#I")
            s.expect_line_prompt_tail()
            s.send_line("b")
            s.send_line(".")
            s.expect_prompt()
            s.send_line("L")
            s.expect_prompt()
            self.assertIn("a", s.child.before)
            self.assertIn("b", s.child.before)

    def test_delete_current_and_range(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("a\nb\nc\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("2D")
            s.expect_prompt()
            s.send_line("L")
            s.expect_prompt()
            self.assertNotIn("b", s.child.before)
            self.assertIn("a", s.child.before)
            self.assertIn("c", s.child.before)


class TestListPage(unittest.TestCase):
    def test_list_empty_no_output(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line("L")
            s.expect_prompt()
            # no line listing — before may be empty aside from prompt recovery
            self.assertNotRegex(s.child.before, r"\d{6}")

    def test_list_line_markers(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("one\ntwo\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("2L")
            s.expect_prompt()
            # Current line remains 1 unless moved; line 2 marker column is space not '*'.
            self.assertRegex(s.child.before, r"2:\s*two")

    def test_pager_continue_prompt(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            lines = "\n".join(f"line{i}" for i in range(1, 15))
            p.write_text(lines + "\n", encoding="ascii")
            env = {"EDLIN_LINES": "6"}
            s = EdlinSession(wd, env=env)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("1,14P")
            # May prompt Continues several times for a long explicit range.
            saw_prompt = False
            for _ in range(8):
                i = s.child.expect(
                    [
                        re.compile(r"Continue \(Y/N\)\?\r?\n"),
                        re.compile(r"(?:^|[\r\n])\*"),
                    ],
                    timeout=15,
                )
                if i == 0:
                    s.send_line("y")
                else:
                    saw_prompt = True
                    break
            assert saw_prompt


class TestSearchReplace(unittest.TestCase):
    def test_search_found(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("alpha\nbeta\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("Sbeta")
            s.expect_prompt()
            self.assertIn("beta", s.child.before)

    def test_search_not_found(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("alpha\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            # Default search starts at current+1; single-line buffer needs a range.
            s.send_line("1,1Sz")
            s.expect_message_then_prompt("Not found")

    def test_search_query_yes(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("findme\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("1?Sfindme")
            s.child.expect(re.compile(r"O\.K\.\? "))
            s.send_line("y")
            s.expect_prompt()

    def test_replace_simple(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("foo bar\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            # Range defaults start at current+1; single-line file needs explicit range.
            s.send_line("1,1Rfoo;baz")
            s.expect_prompt()
            s.send_line("L")
            s.expect_prompt()
            self.assertIn("baz bar", s.child.before)

    def test_replace_empty_old_entry_error(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("x\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("R;z")
            s.expect_entry_error_prompt()


class TestCopyMove(unittest.TestCase):
    def test_copy_block(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("a\nb\nc\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            # Third line number must use commas: 1,2,4C — not 1,2C4 (parsed as two params).
            s.send_line("1,2,4C")
            s.expect_prompt()
            s.send_line("L")
            s.expect_prompt()
            self.assertGreaterEqual(s.child.before.count("a"), 2)

    def test_copy_dest_zero_message(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("a\nb\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            # Third parameter slot empty at 'C' requires commas: 1,2,C not 1,2C0.
            s.send_line("1,2,C")
            s.expect_message_then_prompt("Must specify destination line number")

    def test_move_block(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("a\nb\nc\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("1,1M3")
            s.expect_prompt()
            s.send_line("L")
            s.expect_prompt()


class TestTransferMerge(unittest.TestCase):
    def test_merge_success(self):
        """Merge reads a second path via fopen."""
        with tempfile.TemporaryDirectory(prefix="tmp_edlin_", dir=str(REPO_ROOT)) as td:
            wd = Path(td)
            main = wd / "main.txt"
            main.write_text("mid\n", encoding="ascii")
            (wd / "other.txt").write_text("top\n", encoding="ascii")
            r = run_edlin_script(
                wd,
                str(main),
                b"1Tother.txt\n1,2L\nq\ny\n",
            )
            self.assertEqual(r.returncode, 0, msg=r.stdout.decode(errors="replace"))
            self.assertIn(b"top", r.stdout)

    def test_merge_bad_path(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "m.txt"
            p.write_text("x\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("1Tdoes_not_exist_12345.txt")
            s.expect_message_then_prompt("Invalid drive or file name")

    def test_merge_path_excludes_line_ending_single_char_file(self):
        """Interactive `1tb` must open file `b`, not `b` + newline (would fopen fail)."""
        with tempfile.TemporaryDirectory(prefix="tmp_edlin_", dir=str(REPO_ROOT)) as td:
            wd = Path(td)
            (wd / "a").write_text("only_in_a\n", encoding="ascii")
            (wd / "b").write_text("from_b\n", encoding="ascii")
            r = run_edlin_script(
                wd,
                str(wd / "a"),
                b"1tb\n1,2L\nq\ny\n",
            )
            self.assertEqual(r.returncode, 0, msg=r.stdout.decode(errors="replace"))
            out = r.stdout.decode(errors="replace")
            self.assertIn("from_b", out)
            self.assertIn("only_in_a", out)
            self.assertLess(out.index("from_b"), out.index("only_in_a"))


class TestAppendWriteEndQuit(unittest.TestCase):
    def test_append_eof_message(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("only\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("1A")
            s.expect_message_then_prompt("End of input file")

    def test_write_quarter_then_list(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("\n".join(str(i) for i in range(1, 9)) + "\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            s.send_line("W")
            s.expect_prompt()
            s.send_line("L")
            s.expect_prompt()
            # first 2 lines of 8 written away: (8+3)//4 = 2; listing starts at old line 3
            # Old line 3 becomes line 1 after the first two lines are written away.
            self.assertRegex(s.child.before, r"(?m)^\s*1:\*3")
            self.assertNotRegex(s.child.before, r"(?m)^\s*1:\*1")

    def test_end_saves_file(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "out.txt"
            # New file: blank-line `1` is EOF (no read); use insert to add a line, then E.
            r = run_edlin_script(wd, str(p), b"1I\nsaved\n.\nE\n")
            self.assertEqual(r.returncode, 0)
            text = p.read_bytes()
            self.assertTrue(text.startswith(b"saved\n"))
            self.assertTrue(text.endswith(b"\x1a"))


class TestQuit(unittest.TestCase):
    def test_quit_abort_yes(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line("Q")
            s.child.expect(re.compile(r"Abort edit \(Y/N\)\? "))
            s.send_line("y")
            s.expect_eof()
            self.assertEqual(s.wait_exit(), 0)

    def test_quit_abort_no(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line("Q")
            s.child.expect(re.compile(r"Abort edit \(Y/N\)\? "))
            s.send_line("n")
            s.expect_prompt()


class TestStdinEof(unittest.TestCase):
    def test_eof_exits_cleanly(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_eof()
            s.expect_eof()
            self.assertEqual(s.wait_exit(), 0)


class TestCommandRequirements(unittest.TestCase):
    def test_a_requires_one_param(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            p.write_text("x\n", encoding="ascii")
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_prompt()
            # Two comma-separated parameters → nparam != 1 for append
            s.send_line("1,2A")
            s.expect_entry_error_prompt()

    def test_e_requires_zero_param_form(self):
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            p = wd / "f.txt"
            s = EdlinSession(wd)
            s.spawn([str(EDLIN_BIN), str(p)], self)
            s.expect_new_file_then_prompt()
            s.send_line("1E")
            s.expect_entry_error_prompt()


if __name__ == "__main__":
    unittest.main()
