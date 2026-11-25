#!/usr/bin/env python3
"""
Unit tests for tools/parse_stages.py
Covers all 20 test requirements specified in the SK3 2.1.0 annotation audit specification.
"""

import unittest
import tempfile
import os
import sys

# Ensure tools directory is in sys.path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from parse_stages import StageParser, VALID_SCOPES, VALID_RATES, VALID_PHASES


class TestStageParser(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.temp_dir.cleanup()

    def _write_file(self, content: str, filename: str = "test.c") -> str:
        filepath = os.path.join(self.temp_dir.name, filename)
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(content)
        return filepath

    # 1. Valid single stage
    def test_01_valid_single_stage(self):
        code = """
        /* @stage test_filter
         * @title     Biquad Voice Filter
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @reads     [sv.filter_mode[n], sv.filter_freq[n]]
         * @writes    [sv.sample[n]]
         * @configures [j, k, q]
         * @consumes  [voice_signal]
         * @produces  [voice_signal]
         * @depends   []
         * @guard     sv.filter_mode[n] != 0
         */
        do_filter();
        /* @end test_filter */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        self.assertFalse(parser.has_errors(), f"Unexpected errors: {parser.diagnostics}")
        self.assertIn("test_filter", parser.stages)
        s = parser.stages["test_filter"]
        self.assertEqual(s.title, "Biquad Voice Filter")
        self.assertEqual(s.scope, "voice")
        self.assertEqual(s.rate, "sample")
        self.assertEqual(s.phase, "voice_dsp")
        self.assertEqual(s.reads, ["sv.filter_mode[n]", "sv.filter_freq[n]"])
        self.assertEqual(s.writes, ["sv.sample[n]"])
        self.assertEqual(s.configures, ["j", "k", "q"])
        self.assertEqual(s.guard, "sv.filter_mode[n] != 0")

    # 2. Valid nested stages
    def test_02_valid_nested_stages(self):
        code = """
        /* @stage parent_container
         * @title     Parent Stage
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         */
            /* @stage child_first
             * @title     First Child
             * @scope     voice
             * @rate      sample
             * @phase     voice_dsp
             */
            run_first();
            /* @end child_first */

            /* @stage child_second
             * @title     Second Child
             * @scope     voice
             * @rate      sample
             * @phase     voice_dsp
             * @depends   [child_first]
             */
            run_second();
            /* @end child_second */
        /* @end parent_container */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        parser.validate_global_dependencies()
        self.assertFalse(parser.has_errors(), f"Unexpected errors: {parser.diagnostics}")
        parent = parser.stages["parent_container"]
        c1 = parser.stages["child_first"]
        c2 = parser.stages["child_second"]
        self.assertIsNone(parent.parent)
        self.assertEqual(parent.children, ["child_first", "child_second"])
        self.assertEqual(c1.parent, "parent_container")
        self.assertEqual(c2.parent, "parent_container")
        self.assertEqual(c2.depends, ["child_first"])

    # 3. Duplicate stage IDs across files
    def test_03_duplicate_stage_ids_across_files(self):
        code1 = """
        /* @stage dup_stage
         * @title     First Definition in File 1
         * @scope     engine
         * @rate      block
         * @phase     prepass
         */
        /* @end dup_stage */
        """
        code2 = """
        /* @stage dup_stage
         * @title     Second Definition in File 2
         * @scope     engine
         * @rate      block
         * @phase     prepass
         */
        /* @end dup_stage */
        """
        path1 = self._write_file(code1, "file1.c")
        path2 = self._write_file(code2, "file2.c")
        parser = StageParser()
        parser.parse_file(path1)
        parser.parse_file(path2)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("Duplicate stage ID 'dup_stage'" in e.message for e in errors))
        # Ensure parser did not replace original stage
        self.assertEqual(parser.stages["dup_stage"].title, "First Definition in File 1")

    # 4. Duplicate metadata tags
    def test_04_duplicate_metadata_tags(self):
        code = """
        /* @stage dup_tag_stage
         * @title     Title One
         * @title     Title Two
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         */
        /* @end dup_tag_stage */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("Duplicate metadata tag '@title'" in e.message for e in errors))

    # 5. Missing required tags
    def test_05_missing_required_tags(self):
        code = """
        /* @stage missing_tags
         * @scope     voice
         */
        /* @end missing_tags */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("missing required @title" in e.message for e in errors))
        self.assertTrue(any("missing required @rate" in e.message for e in errors))

    # 6. Invalid scope and rate
    def test_06_invalid_scope_and_rate(self):
        for bad_scope in ["osc", "frame", "sequencer"]:
            code = f"""
            /* @stage bad_scope
             * @title     Bad Scope Stage
             * @scope     {bad_scope}
             * @rate      sample
             * @phase     voice_dsp
             */
            /* @end bad_scope */
            """
            path = self._write_file(code)
            parser = StageParser()
            parser.parse_file(path)
            errors = [d for d in parser.diagnostics if d.is_error]
            self.assertTrue(any(f"Invalid @scope '{bad_scope}'" in e.message for e in errors))

        for bad_rate in ["audio", "fast", "tick"]:
            code = f"""
            /* @stage bad_rate
             * @title     Bad Rate Stage
             * @scope     voice
             * @rate      {bad_rate}
             * @phase     voice_dsp
             */
            /* @end bad_rate */
            """
            path = self._write_file(code)
            parser = StageParser()
            parser.parse_file(path)
            errors = [d for d in parser.diagnostics if d.is_error]
            self.assertTrue(any(f"Invalid @rate '{bad_rate}'" in e.message for e in errors))

    # 7. Invalid phase if phase is enumerated
    def test_07_invalid_phase(self):
        code = """
        /* @stage bad_phase_stage
         * @title     Bad Phase
         * @scope     voice
         * @rate      sample
         * @phase     totally_unknown_phase
         */
        /* @end bad_phase_stage */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("Invalid @phase 'totally_unknown_phase'" in e.message for e in errors))

    # 8. Valid multiline lists
    def test_08_valid_multiline_lists(self):
        code = """
        /* @stage multi_stage
         * @title     Multiline Stage
         * @scope     engine
         * @rate      sample
         * @phase     frame_setup
         * @reads     [sv.foo[n],
         *             sv.bar[n],
         *             sv.baz[n]]
         * @writes    [sv.out]
         */
        /* @end multi_stage */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        self.assertFalse(parser.has_errors(), f"Unexpected errors: {parser.diagnostics}")
        s = parser.stages["multi_stage"]
        self.assertEqual(s.reads, ["sv.foo[n]", "sv.bar[n]", "sv.baz[n]"])
        self.assertEqual(s.writes, ["sv.out"])

    # 9. Nested brackets in list references
    def test_09_nested_brackets_in_list_references(self):
        code = """
        /* @stage nested_bracket_stage
         * @title     Nested Bracket Stage
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @reads     [sv.table[voice], sv.lut[sv.index[n]]]
         */
        /* @end nested_bracket_stage */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        self.assertFalse(parser.has_errors(), f"Unexpected errors: {parser.diagnostics}")
        s = parser.stages["nested_bracket_stage"]
        self.assertEqual(s.reads, ["sv.table[voice]", "sv.lut[sv.index[n]]"])

    # 10. Quoted values if supported
    def test_10_quoted_values(self):
        code = """
        /* @stage quoted_stage
         * @title     Quoted Values Stage
         * @scope     engine
         * @rate      sample
         * @phase     frame_setup
         * @consumes  ["item, with comma", 'single quoted item', plain_item]
         */
        /* @end quoted_stage */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        self.assertFalse(parser.has_errors(), f"Unexpected errors: {parser.diagnostics}")
        s = parser.stages["quoted_stage"]
        self.assertEqual(s.consumes, ["item, with comma", "single quoted item", "plain_item"])

    # 11. Unmatched opening and closing brackets
    def test_11_unmatched_brackets(self):
        bad_opens = """
        /* @stage bad_open
         * @title     Unmatched Open Bracket
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @reads     [sv.foo[n, sv.bar]
         */
        /* @end bad_open */
        """
        path = self._write_file(bad_opens)
        parser = StageParser()
        parser.parse_file(path)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("Unmatched opening bracket" in e.message for e in errors))

        bad_closes = """
        /* @stage bad_close
         * @title     Unmatched Close Bracket
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @reads     [sv.foo, sv.bar]]
         */
        /* @end bad_close */
        """
        path2 = self._write_file(bad_closes, "bad_close.c")
        parser2 = StageParser()
        parser2.parse_file(path2)
        errors2 = [d for d in parser2.diagnostics if d.is_error]
        self.assertTrue(any("Unmatched closing bracket" in e.message or "must be enclosed" in e.message for e in errors2))

    # 12. Empty list elements and trailing commas
    def test_12_empty_list_elements_and_trailing_commas(self):
        empty_elem = """
        /* @stage empty_elem
         * @title     Empty List Element
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @reads     [sv.foo, , sv.bar]
         */
        /* @end empty_elem */
        """
        path1 = self._write_file(empty_elem)
        p1 = StageParser()
        p1.parse_file(path1)
        self.assertTrue(any("Empty list element" in e.message for e in p1.diagnostics if e.is_error))

        trailing_comma = """
        /* @stage trailing_comma
         * @title     Trailing Comma
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @reads     [sv.foo, sv.bar, ]
         */
        /* @end trailing_comma */
        """
        path2 = self._write_file(trailing_comma, "trailing.c")
        p2 = StageParser()
        p2.parse_file(path2)
        self.assertTrue(any("Trailing comma in list" in e.message for e in p2.diagnostics if e.is_error))

    # 13. Unmatched stage start/end markers
    def test_13_unmatched_stage_start_end_markers(self):
        unclosed = """
        /* @stage unclosed_stage
         * @title     Unclosed Stage
         * @scope     engine
         * @rate      block
         * @phase     prepass
         */
        void run() {}
        """
        path1 = self._write_file(unclosed)
        p1 = StageParser()
        p1.parse_file(path1)
        self.assertTrue(any("Unclosed stage 'unclosed_stage'" in e.message for e in p1.diagnostics if e.is_error))

        extra_end = """
        /* @end stray_end */
        """
        path2 = self._write_file(extra_end, "extra_end.c")
        p2 = StageParser()
        p2.parse_file(path2)
        self.assertTrue(any("Unmatched @end stray_end" in e.message for e in p2.diagnostics if e.is_error))

    # 14. Mismatched nested end IDs
    def test_14_mismatched_nested_end_ids(self):
        mismatched = """
        /* @stage outer_stage
         * @title     Outer
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         */
            /* @stage inner_stage
             * @title     Inner
             * @scope     voice
             * @rate      sample
             * @phase     voice_dsp
             */
            inner_func();
        /* @end outer_stage */
            /* @end inner_stage */
        """
        path = self._write_file(mismatched)
        parser = StageParser()
        parser.parse_file(path)
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("Mismatched @end outer_stage; expected @end inner_stage" in e.message for e in errors))

    # 15. Unknown metadata preservation
    def test_15_unknown_metadata_preservation(self):
        code = """
        /* @stage custom_stage
         * @title     Custom Metadata Stage
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @experimental_flag high_precision
         */
        /* @end custom_stage */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        self.assertFalse(parser.has_errors(), f"Unexpected errors: {parser.diagnostics}")
        s = parser.stages["custom_stage"]
        self.assertIn("experimental_flag", s.extras)
        self.assertEqual(s.extras["experimental_flag"], "high_precision")
        self.assertIn("experimental_flag", s.unknown_tags)
        warnings = [d for d in parser.diagnostics if not d.is_error]
        self.assertTrue(any("Unknown annotation tag @experimental_flag" in w.message for w in warnings))

    # 16. Unknown dependency references
    def test_16_unknown_dependency_references(self):
        code = """
        /* @stage dep_test
         * @title     Dep Test
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @depends   [non_existent_stage_xyz]
         */
        /* @end dep_test */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        parser.validate_global_dependencies()
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("depends on unknown stage 'non_existent_stage_xyz'" in e.message for e in errors))

    # 17. Dependency cycles
    def test_17_dependency_cycles(self):
        code = """
        /* @stage cycle_a
         * @title     Cycle A
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @depends   [cycle_c]
         */
        /* @end cycle_a */

        /* @stage cycle_b
         * @title     Cycle B
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @depends   [cycle_a]
         */
        /* @end cycle_b */

        /* @stage cycle_c
         * @title     Cycle C
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @depends   [cycle_b]
         */
        /* @end cycle_c */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        parser.validate_global_dependencies()
        errors = [d for d in parser.diagnostics if d.is_error]
        self.assertTrue(any("Dependency cycle detected" in e.message for e in errors))
        self.assertTrue(any("cycle_a" in e.message and "cycle_b" in e.message and "cycle_c" in e.message for e in errors))

    # 18. Repeated unknown metadata
    def test_18_repeated_unknown_metadata(self):
        code = """
        /* @stage repeated_unknown
         * @title     Repeated Unknown Stage
         * @scope     voice
         * @rate      sample
         * @phase     voice_dsp
         * @custom_metric alpha
         * @custom_metric beta
         */
        /* @end repeated_unknown */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        # Should emit warning, not crash, and preserve all occurrences
        warnings = [d for d in parser.diagnostics if not d.is_error]
        self.assertTrue(any("Repeated unknown metadata tag '@custom_metric'" in w.message for w in warnings))
        s = parser.stages["repeated_unknown"]
        self.assertIn("custom_metric", s.unknown_tags)
        entries = [val for _, val in s.unknown_tags["custom_metric"]]
        self.assertEqual(entries, ["alpha", "beta"])

    # 19. A condition containing commas or parentheses
    def test_19_condition_with_commas_or_parentheses(self):
        code = """
        /* @stage condition_syntax
         * @title     Condition with Commas and Parens
         * @scope     engine
         * @rate      event
         * @phase     sequencer
         * @guard     (!queue_is_empty(&seq_q) && check_status(engine, 1, 2))
         */
        /* @end condition_syntax */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        self.assertFalse(parser.has_errors(), f"Unexpected errors: {parser.diagnostics}")
        s = parser.stages["condition_syntax"]
        self.assertEqual(s.guard, "(!queue_is_empty(&seq_q) && check_status(engine, 1, 2))")

    # 20. A side-effecting expression retained as opaque text
    def test_20_side_effecting_expression_as_opaque_text(self):
        code = """
        /* @stage side_effect_stage
         * @title     Side Effecting Condition
         * @scope     engine
         * @rate      sample
         * @phase     sequencer
         * @early_exit simple_mutex_trylock(&seq_edit_mutex) == 0
         */
        /* @end side_effect_stage */
        """
        path = self._write_file(code)
        parser = StageParser()
        parser.parse_file(path)
        self.assertFalse(parser.has_errors(), f"Unexpected errors: {parser.diagnostics}")
        s = parser.stages["side_effect_stage"]
        self.assertEqual(s.early_exit, "simple_mutex_trylock(&seq_edit_mutex) == 0")


if __name__ == "__main__":
    unittest.main()
