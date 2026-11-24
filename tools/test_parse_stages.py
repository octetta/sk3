#!/usr/bin/env python3
"""
Unit tests for tools/parse_stages.py
"""

import unittest
import tempfile
import os
import sys

from parse_stages import StageParser, VALID_SCOPES, VALID_RATES


class TestStageParser(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.temp_dir.cleanup()

    def _write_temp_file(self, content: str) -> str:
        filepath = os.path.join(self.temp_dir.name, "test_file.c")
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(content)
        return filepath

    def test_valid_single_stage(self):
        code = """
        /* @stage test_stage
         * @title     Test Voice Filter Stage
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @reads     [sv.filter_mode[n], sv.filter_freq[n]]
         * @writes    [sv.filter_update_counter[n]]
         * @opcodes   [j, k, q]
         * @consumes  [voice_signal]
         * @produces  [voice_signal]
         * @depends   []
         * @condition sv.filter_mode[n] != 0
         */
        do_dsp();
        /* @end test_stage */
        """
        path = self._write_temp_file(code)
        parser = StageParser()
        parser.parse_file(path)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertEqual(len(errors), 0, f"Unexpected errors: {errors}")
        self.assertIn("test_stage", parser.stages)
        s = parser.stages["test_stage"]
        self.assertEqual(s.title, "Test Voice Filter Stage")
        self.assertEqual(s.scope, "voice")
        self.assertEqual(s.rate, "sample")
        self.assertEqual(s.phase, "voice_dsp")
        self.assertEqual(s.reads, ["sv.filter_mode[n]", "sv.filter_freq[n]"])
        self.assertEqual(s.writes, ["sv.filter_update_counter[n]"])
        self.assertEqual(s.opcodes, ["j", "k", "q"])
        self.assertEqual(s.consumes, ["voice_signal"])
        self.assertEqual(s.produces, ["voice_signal"])
        self.assertEqual(s.condition, "sv.filter_mode[n] != 0")

    def test_multiline_list_parsing(self):
        code = """
        /* @stage multi_list_stage
         * @title     Multiline List Stage
         * @scope     engine
         * @rate      block
         * @reads     [sv.a,
         *             sv.b,
         *             sv.c]
         * @writes    [sv.d]
         */
        do_block();
        /* @end multi_list_stage */
        """
        path = self._write_temp_file(code)
        parser = StageParser()
        parser.parse_file(path)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertEqual(len(errors), 0, f"Unexpected errors: {errors}")
        s = parser.stages["multi_list_stage"]
        self.assertEqual(s.reads, ["sv.a", "sv.b", "sv.c"])
        self.assertEqual(s.writes, ["sv.d"])

    def test_nested_stages_and_parent_child(self):
        code = """
        /* @stage parent_stage
         * @title     Parent Container
         * @scope     voice
         * @rate      sample
         */
          /* @stage child_a
           * @title     Child A
           * @scope     voice
           * @rate      sample
           */
          foo();
          /* @end child_a */

          /* @stage child_b
           * @title     Child B
           * @scope     voice
           * @rate      sample
           * @depends   [child_a]
           */
          bar();
          /* @end child_b */
        /* @end parent_stage */
        """
        path = self._write_temp_file(code)
        parser = StageParser()
        parser.parse_file(path)
        parser.validate_global_dependencies()
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertEqual(len(errors), 0, f"Unexpected errors: {errors}")

        parent = parser.stages["parent_stage"]
        child_a = parser.stages["child_a"]
        child_b = parser.stages["child_b"]

        self.assertIsNone(parent.parent)
        self.assertEqual(parent.children, ["child_a", "child_b"])
        self.assertEqual(child_a.parent, "parent_stage")
        self.assertEqual(child_b.parent, "parent_stage")
        self.assertEqual(child_b.depends, ["child_a"])

    def test_invalid_scope_rejection(self):
        # Requirements explicitly state: Do not use osc, frame, or sequencer as scope values.
        for bad_scope in ["osc", "frame", "sequencer", "master", "voice_block"]:
            code = f"""
            /* @stage bad_scope_stage_{bad_scope}
             * @title     Bad Scope
             * @scope     {bad_scope}
             * @rate      sample
             */
            /* @end bad_scope_stage_{bad_scope} */
            """
            path = self._write_temp_file(code)
            parser = StageParser()
            parser.parse_file(path)
            errors = [d for d in parser.diagnostics if d.is_error]
            self.assertTrue(any(f"Invalid @scope '{bad_scope}'" in e.message for e in errors),
                            f"Expected rejection of @scope '{bad_scope}'")

    def test_invalid_rate_rejection(self):
        for bad_rate in ["audio", "second", "cycle", "fast"]:
            code = f"""
            /* @stage bad_rate_stage_{bad_rate}
             * @title     Bad Rate
             * @scope     engine
             * @rate      {bad_rate}
             */
            /* @end bad_rate_stage_{bad_rate} */
            """
            path = self._write_temp_file(code)
            parser = StageParser()
            parser.parse_file(path)
            errors = [d for d in parser.diagnostics if d.is_error]
            self.assertTrue(any(f"Invalid @rate '{bad_rate}'" in e.message for e in errors),
                            f"Expected rejection of @rate '{bad_rate}'")

    def test_duplicate_stage_id(self):
        code = """
        /* @stage dup_stage
         * @title     First Instance
         * @scope     voice
         * @rate      sample
         */
        /* @end dup_stage */

        /* @stage dup_stage
         * @title     Second Instance
         * @scope     voice
         * @rate      sample
         */
        /* @end dup_stage */
        """
        path = self._write_temp_file(code)
        parser = StageParser()
        parser.parse_file(path)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("Duplicate stage ID 'dup_stage'" in e.message for e in errors))

    def test_unmatched_end(self):
        code = """
        /* @stage stage_one
         * @title     One
         * @scope     voice
         * @rate      sample
         */
        /* @end stage_wrong */
        """
        path = self._write_temp_file(code)
        parser = StageParser()
        parser.parse_file(path)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("Mismatched @end stage_wrong; expected @end stage_one" in e.message for e in errors))

    def test_unclosed_stage_at_eof(self):
        code = """
        /* @stage unclosed_stage
         * @title     Never Ended
         * @scope     voice
         * @rate      sample
         */
        void foo() {}
        """
        path = self._write_temp_file(code)
        parser = StageParser()
        parser.parse_file(path)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("Unclosed stage 'unclosed_stage'" in e.message for e in errors))

    def test_unknown_dependency_rejection(self):
        code = """
        /* @stage dep_stage
         * @title     Dep Test
         * @scope     voice
         * @rate      sample
         * @depends   [non_existent_stage]
         */
        /* @end dep_stage */
        """
        path = self._write_temp_file(code)
        parser = StageParser()
        parser.parse_file(path)
        parser.validate_global_dependencies()
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("depends on unknown stage 'non_existent_stage'" in e.message for e in errors))


if __name__ == "__main__":
    unittest.main()
