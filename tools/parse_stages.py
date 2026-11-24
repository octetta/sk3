#!/usr/bin/env python3
"""
SK3 Render Stage Annotation Parser
Parses and validates machine-readable C comment tags (@stage ... @end)
in the SK3 synthesizer engine codebase.
"""

import sys
import os
import re
import json
import argparse
from typing import Dict, List, Optional, Any, Tuple

VALID_SCOPES = {"engine", "voice"}
VALID_RATES = {"event", "block", "sample"}
REQUIRED_TAGS = {"stage", "title", "scope", "rate"}

KNOWN_LIST_TAGS = {"reads", "writes", "opcodes", "consumes", "produces", "depends"}
KNOWN_SCALAR_TAGS = {
    "stage", "title", "scope", "rate", "phase",
    "condition", "active_when", "gate",
    "bypass_when", "bypass_condition", "bypassed",
    "notes"
}


class ParseDiagnostic:
    def __init__(self, file: str, line: int, message: str, is_error: bool = True):
        self.file = file
        self.line = line
        self.message = message
        self.is_error = is_error

    def __str__(self):
        prefix = "ERROR" if self.is_error else "WARNING"
        return f"{self.file}:{self.line}: {prefix}: {self.message}"


class StageInfo:
    def __init__(self, stage_id: str, file: str, start_line: int):
        self.id = stage_id
        self.file = file
        self.start_line = start_line
        self.end_line: Optional[int] = None
        self.parent: Optional[str] = None
        self.children: List[str] = []

        self.title: str = ""
        self.scope: str = ""
        self.rate: str = ""
        self.phase: Optional[str] = None

        self.reads: List[str] = []
        self.writes: List[str] = []
        self.opcodes: List[str] = []
        self.consumes: List[str] = []
        self.produces: List[str] = []
        self.depends: List[str] = []

        self.condition: Optional[str] = None
        self.bypass_when: Optional[str] = None
        self.notes: Optional[str] = None
        self.extras: Dict[str, Any] = {}

    def to_dict(self) -> Dict[str, Any]:
        d = {
            "id": self.id,
            "title": self.title,
            "scope": self.scope,
            "rate": self.rate,
            "file": self.file,
            "start_line": self.start_line,
            "end_line": self.end_line,
            "parent": self.parent,
            "children": self.children,
        }
        if self.phase:
            d["phase"] = self.phase
        d["reads"] = self.reads
        d["writes"] = self.writes
        d["opcodes"] = self.opcodes
        d["consumes"] = self.consumes
        d["produces"] = self.produces
        d["depends"] = self.depends
        if self.condition:
            d["condition"] = self.condition
        if self.bypass_when:
            d["bypass_when"] = self.bypass_when
        if self.notes:
            d["notes"] = self.notes
        if self.extras:
            d["extras"] = self.extras
        return d


class StageParser:
    def __init__(self):
        self.stages: Dict[str, StageInfo] = {}
        self.diagnostics: List[ParseDiagnostic] = []

    def error(self, file: str, line: int, message: str):
        self.diagnostics.append(ParseDiagnostic(file, line, message, is_error=True))

    def warn(self, file: str, line: int, message: str):
        self.diagnostics.append(ParseDiagnostic(file, line, message, is_error=False))

    @staticmethod
    def _parse_list_tokens(raw: str) -> List[str]:
        raw = raw.strip()
        if raw.startswith("[") and raw.endswith("]"):
            raw = raw[1:-1]
        tokens = []
        for item in raw.split(","):
            s = item.strip()
            if s:
                tokens.append(s)
        return tokens

    def parse_file(self, file_path: str) -> None:
        if not os.path.isfile(file_path):
            self.error(file_path, 0, f"File not found: {file_path}")
            return

        with open(file_path, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()

        stage_stack: List[str] = []
        in_comment_block = False
        current_stage: Optional[StageInfo] = None
        multiline_tag: Optional[str] = None
        multiline_buffer: List[str] = []
        multiline_start_line = 0

        # Regex patterns
        re_stage_start = re.compile(r'/\*\s*@stage\s+([a-zA-Z0-9_]+)')
        re_stage_end = re.compile(r'/\*\s*@end\s+([a-zA-Z0-9_]+)\s*\*/')
        re_tag_line = re.compile(r'^\s*(?:\*|/\*)\s*@([a-zA-Z0-9_]+)\s*(.*)$')
        re_comment_end = re.compile(r'\*/')

        for line_num, line in enumerate(lines, start=1):
            # Check for stage end marker
            end_match = re_stage_end.search(line)
            if end_match:
                end_id = end_match.group(1)
                if not stage_stack:
                    self.error(file_path, line_num, f"Unmatched @end {end_id} (stage stack is empty)")
                elif stage_stack[-1] != end_id:
                    self.error(
                        file_path,
                        line_num,
                        f"Mismatched @end {end_id}; expected @end {stage_stack[-1]}"
                    )
                else:
                    popped_id = stage_stack.pop()
                    if popped_id in self.stages:
                        self.stages[popped_id].end_line = line_num
                continue

            # Check for stage start marker
            start_match = re_stage_start.search(line)
            if start_match:
                stage_id = start_match.group(1)
                if stage_id in self.stages:
                    prev_stage = self.stages[stage_id]
                    self.error(
                        file_path,
                        line_num,
                        f"Duplicate stage ID '{stage_id}' (previously defined in {prev_stage.file}:{prev_stage.start_line})"
                    )

                new_stage = StageInfo(stage_id, file_path, line_num)
                if stage_stack:
                    parent_id = stage_stack[-1]
                    new_stage.parent = parent_id
                    if parent_id in self.stages:
                        self.stages[parent_id].children.append(stage_id)

                stage_stack.append(stage_id)
                self.stages[stage_id] = new_stage
                current_stage = new_stage
                in_comment_block = True
                multiline_tag = None
                multiline_buffer = []

            # If inside the metadata comment block for a stage
            if in_comment_block and current_stage is not None:
                # Check for comment block close
                has_comment_close = re_comment_end.search(line)

                # Process multiline continuation first
                if multiline_tag is not None:
                    # Strip leading '*' or whitespace
                    content = line.strip()
                    if content.startswith("*"):
                        content = content[1:].strip()
                    # Check if end of list ']' or comment close
                    if "]" in content or has_comment_close:
                        idx = content.find("]")
                        if idx != -1:
                            multiline_buffer.append(content[:idx + 1])
                            self._apply_tag_value(
                                current_stage,
                                multiline_tag,
                                " ".join(multiline_buffer),
                                file_path,
                                multiline_start_line
                            )
                            multiline_tag = None
                            multiline_buffer = []
                        else:
                            multiline_buffer.append(content)
                            if has_comment_close:
                                self.error(
                                    file_path,
                                    line_num,
                                    f"Unclosed multiline list for tag @{multiline_tag} before comment end"
                                )
                                self._apply_tag_value(
                                    current_stage,
                                    multiline_tag,
                                    " ".join(multiline_buffer),
                                    file_path,
                                    multiline_start_line
                                )
                                multiline_tag = None
                                multiline_buffer = []
                    else:
                        multiline_buffer.append(content)

                # If not continuing a multiline list, check for new tag
                if multiline_tag is None:
                    tag_match = re_tag_line.match(line)
                    if tag_match:
                        tag_name = tag_match.group(1)
                        val = tag_match.group(2).strip()

                        # Strip trailing '*/' if on the same line
                        if val.endswith("*/"):
                            val = val[:-2].strip()

                        if tag_name in KNOWN_LIST_TAGS:
                            if val.startswith("[") and not val.endswith("]"):
                                multiline_tag = tag_name
                                multiline_buffer = [val]
                                multiline_start_line = line_num
                            else:
                                self._apply_tag_value(current_stage, tag_name, val, file_path, line_num)
                        elif tag_name != "stage":  # @stage already recorded
                            self._apply_tag_value(current_stage, tag_name, val, file_path, line_num)

                if has_comment_close:
                    in_comment_block = False
                    self._validate_stage_definition(current_stage, file_path, line_num)
                    current_stage = None

        if stage_stack:
            for unclosed in stage_stack:
                st = self.stages.get(unclosed)
                line_ref = st.start_line if st else 0
                self.error(
                    file_path,
                    line_ref,
                    f"Unclosed stage '{unclosed}' reached end of file without matching /* @end {unclosed} */"
                )

    def _apply_tag_value(self, stage: StageInfo, tag: str, value: str, file: str, line: int):
        val = value.strip()
        if tag == "title":
            stage.title = val
        elif tag == "scope":
            if val not in VALID_SCOPES:
                self.error(file, line, f"Invalid @scope '{val}'. Must be one of {sorted(VALID_SCOPES)}")
            stage.scope = val
        elif tag == "rate":
            if val not in VALID_RATES:
                self.error(file, line, f"Invalid @rate '{val}'. Must be one of {sorted(VALID_RATES)}")
            stage.rate = val
        elif tag == "phase":
            stage.phase = val
        elif tag == "reads":
            stage.reads = self._parse_list_tokens(val)
        elif tag == "writes":
            stage.writes = self._parse_list_tokens(val)
        elif tag == "opcodes":
            stage.opcodes = self._parse_list_tokens(val)
        elif tag == "consumes":
            stage.consumes = self._parse_list_tokens(val)
        elif tag == "produces":
            stage.produces = self._parse_list_tokens(val)
        elif tag == "depends":
            stage.depends = self._parse_list_tokens(val)
        elif tag in ("condition", "active_when", "gate"):
            # Opaque C boolean expression text
            if val.startswith("[") and val.endswith("]"):
                val = val[1:-1].strip()
            stage.condition = val
        elif tag in ("bypass_when", "bypass_condition", "bypassed"):
            # Opaque C boolean expression text
            if val.startswith("[") and val.endswith("]"):
                val = val[1:-1].strip()
            stage.bypass_when = val
            if tag == "bypassed":
                self.warn(file, line, "@bypassed is deprecated; use @condition or @bypass_when")
        elif tag == "notes":
            stage.notes = val
        elif tag == "fields":
            self.warn(file, line, "@fields is deprecated; use @reads and @writes instead")
            stage.extras["fields"] = self._parse_list_tokens(val)
        elif tag == "schema":
            self.warn(file, line, "@schema is deprecated; state owners are resolved via registry")
            stage.extras["schema"] = self._parse_list_tokens(val)
        else:
            self.warn(file, line, f"Unknown annotation tag @{tag}; stored in extras")
            stage.extras[tag] = val

    def _validate_stage_definition(self, stage: StageInfo, file: str, line: int):
        if not stage.title:
            self.error(file, stage.start_line, f"Stage '{stage.id}' is missing required @title")
        if not stage.scope:
            self.error(file, stage.start_line, f"Stage '{stage.id}' is missing required @scope ('engine' or 'voice')")
        if not stage.rate:
            self.error(file, stage.start_line, f"Stage '{stage.id}' is missing required @rate ('event', 'block', or 'sample')")

    def validate_global_dependencies(self):
        for stage_id, stage in self.stages.items():
            for dep in stage.depends:
                if dep not in self.stages:
                    self.error(
                        stage.file,
                        stage.start_line,
                        f"Stage '{stage_id}' depends on unknown stage '{dep}'"
                    )

    def print_diagnostics(self) -> Tuple[int, int]:
        errors = [d for d in self.diagnostics if d.is_error]
        warnings = [d for d in self.diagnostics if not d.is_error]

        for w in warnings:
            print(f"\033[33m{w}\033[0m", file=sys.stderr)
        for e in errors:
            print(f"\033[31m{e}\033[0m", file=sys.stderr)

        return len(errors), len(warnings)


def main():
    parser = argparse.ArgumentParser(description="SK3 Render Stage Annotation Parser")
    parser.add_argument("files", nargs="+", help="Source files to parse")
    parser.add_argument("--json", action="store_true", help="Output stage metadata in JSON format")
    parser.add_argument("--markdown", action="store_true", help="Output inventory as Markdown table")
    parser.add_argument("--check-deps", action="store_true", default=True, help="Verify inter-stage @depends references")
    args = parser.parse_args()

    stage_parser = StageParser()
    for f in args.files:
        stage_parser.parse_file(f)

    if args.check_deps:
        stage_parser.validate_global_dependencies()

    error_count, warning_count = stage_parser.print_diagnostics()

    if args.json:
        data = {s_id: s.to_dict() for s_id, s in stage_parser.stages.items()}
        print(json.dumps(data, indent=2))
    elif args.markdown:
        print("# SK3 Stage Inventory\n")
        print("| Stage ID | Scope | Rate | Phase | Title | File:Lines |")
        print("| :--- | :--- | :--- | :--- | :--- | :--- |")
        for s_id, s in stage_parser.stages.items():
            phase_str = s.phase or "-"
            lines_str = f"{s.start_line}-{s.end_line or '?'}"
            rel_file = os.path.basename(s.file)
            print(f"| `{s.id}` | `{s.scope}` | `{s.rate}` | `{phase_str}` | {s.title} | {rel_file}:{lines_str} |")
    else:
        print(f"Validated {len(stage_parser.stages)} stages with {error_count} error(s) and {warning_count} warning(s).")

    sys.exit(1 if error_count > 0 else 0)


if __name__ == "__main__":
    main()
