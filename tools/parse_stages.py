#!/usr/bin/env python3
"""
SK3 Render Stage Annotation Parser
Parses and validates machine-readable C comment tags (@stage ... @end)
in the SK3 synthesizer engine codebase.

Strict, deterministic, and robust parser adhering to the SK3 2.1.0 specification.
"""

import sys
import os
import re
import json
import argparse
from typing import Dict, List, Optional, Any, Tuple, Set, Union

VALID_SCOPES = {"engine", "voice"}
VALID_RATES = {"event", "block", "sample"}
VALID_PHASES = {
    "prepass",
    "frame_setup",
    "voice_dsp",
    "mix_effects",
    "output_routing",
    "osc_eval",
    "sequencer"
}
REQUIRED_TAGS = {"stage", "title", "scope", "rate"}

KNOWN_LIST_TAGS = {"reads", "writes", "opcodes", "consumes", "produces", "depends", "configures"}
KNOWN_SCALAR_TAGS = {"stage", "title", "scope", "rate", "phase", "guard", "notes"}
KNOWN_CONDITIONAL_TAGS = {
    "early_exit", "branch", "condition", "active_when", "gate",
    "bypass_when", "bypass_condition", "bypassed"
}
ALL_KNOWN_TAGS = KNOWN_LIST_TAGS | KNOWN_SCALAR_TAGS | KNOWN_CONDITIONAL_TAGS


class ParseDiagnostic:
    def __init__(self, file: str, line: int, message: str, is_error: bool = True):
        self.file = file
        self.line = line
        self.message = message
        self.is_error = is_error

    def __str__(self):
        prefix = "ERROR" if self.is_error else "WARNING"
        return f"{self.file}:{self.line}: {prefix}: {self.message}"

    def __repr__(self):
        return f"ParseDiagnostic({self.file!r}, {self.line}, {self.message!r}, is_error={self.is_error})"


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
        self.configures: List[str] = []
        self.consumes: List[str] = []
        self.produces: List[str] = []
        self.depends: List[str] = []

        self.guard: Optional[str] = None
        self.early_exit: Optional[Union[str, List[str]]] = None
        self.branch: Optional[Union[str, List[str]]] = None
        self.condition: Optional[str] = None
        self.bypass_when: Optional[str] = None
        self.notes: Optional[str] = None

        # Unknown and extra tags:
        # extras stores last or consolidated values for easy access
        # unknown_tags preserves ALL occurrences with line numbers: Dict[tag, List[Tuple[line, value]]]
        self.extras: Dict[str, Any] = {}
        self.unknown_tags: Dict[str, List[Tuple[int, str]]] = {}

        # Internal tracking
        self.seen_tags: Dict[str, int] = {}

    def to_dict(self) -> Dict[str, Any]:
        d: Dict[str, Any] = {
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
        if self.configures:
            d["configures"] = self.configures
        d["consumes"] = self.consumes
        d["produces"] = self.produces
        d["depends"] = self.depends
        if self.guard:
            d["guard"] = self.guard
        if self.early_exit is not None:
            d["early_exit"] = self.early_exit
        if self.branch is not None:
            d["branch"] = self.branch
        if self.condition:
            d["condition"] = self.condition
        if self.bypass_when:
            d["bypass_when"] = self.bypass_when
        if self.notes:
            d["notes"] = self.notes
        if self.extras:
            d["extras"] = self.extras
        if self.unknown_tags:
            d["unknown_tags"] = {k: [v for _, v in entries] for k, entries in self.unknown_tags.items()}
        return d


class StageParser:
    def __init__(self):
        self.stages: Dict[str, StageInfo] = {}
        self.diagnostics: List[ParseDiagnostic] = []

    def error(self, file: str, line: int, message: str) -> None:
        self.diagnostics.append(ParseDiagnostic(file, line, message, is_error=True))

    def warn(self, file: str, line: int, message: str) -> None:
        self.diagnostics.append(ParseDiagnostic(file, line, message, is_error=False))

    def has_errors(self) -> bool:
        return any(d.is_error for d in self.diagnostics)

    def get_valid_inventory(self) -> Dict[str, StageInfo]:
        if self.has_errors():
            raise ValueError(f"Cannot produce valid inventory: parser encountered {len([d for d in self.diagnostics if d.is_error])} error(s)")
        return self.stages

    def parse_list_tokens(self, raw: str, file: str, line: int, tag_name: str) -> List[str]:
        """
        Strict list token parser.
        Supports:
        - Nested brackets (e.g. sv.table[voice])
        - Nested parentheses (e.g. fn(a, b))
        - Quoted elements (e.g. "foo, bar")
        Rejects:
        - Unmatched brackets/parentheses/quotes
        - Empty list elements (e.g. [a, , b])
        - Trailing commas (e.g. [a, b, ])
        - Leading commas (e.g. [, a])
        """
        s = raw.strip()
        if not (s.startswith("[") and s.endswith("]")):
            self.error(file, line, f"List for tag @{tag_name} must be enclosed in '[' and ']' (got: {raw!r})")
            return []

        inner = s[1:-1].strip()
        if not inner:
            return []

        tokens: List[str] = []
        buf: List[str] = []
        bracket_depth = 0
        paren_depth = 0
        in_quote: Optional[str] = None
        has_token_content = False

        i = 0
        n = len(inner)
        while i < n:
            ch = inner[i]

            if in_quote is not None:
                if ch == "\\":
                    buf.append(ch)
                    if i + 1 < n:
                        i += 1
                        buf.append(inner[i])
                elif ch == in_quote:
                    in_quote = None
                    buf.append(ch)
                else:
                    buf.append(ch)
            else:
                if ch in ('"', "'"):
                    in_quote = ch
                    buf.append(ch)
                    has_token_content = True
                elif ch == '[':
                    bracket_depth += 1
                    buf.append(ch)
                    has_token_content = True
                elif ch == ']':
                    bracket_depth -= 1
                    if bracket_depth < 0:
                        self.error(file, line, f"Unmatched closing bracket ']' in tag @{tag_name}")
                        return []
                    buf.append(ch)
                    has_token_content = True
                elif ch == '(':
                    paren_depth += 1
                    buf.append(ch)
                    has_token_content = True
                elif ch == ')':
                    paren_depth -= 1
                    if paren_depth < 0:
                        self.error(file, line, f"Unmatched closing parenthesis ')' in tag @{tag_name}")
                        return []
                    buf.append(ch)
                    has_token_content = True
                elif ch == ',' and bracket_depth == 0 and paren_depth == 0:
                    token_str = "".join(buf).strip()
                    if not has_token_content or not token_str:
                        if not tokens and not has_token_content:
                            self.error(file, line, f"Leading comma in list for tag @{tag_name}")
                        else:
                            self.error(file, line, f"Empty list element in tag @{tag_name}")
                        return []
                    tokens.append(self._strip_quotes(token_str))
                    buf = []
                    has_token_content = False
                else:
                    buf.append(ch)
                    if not ch.isspace():
                        has_token_content = True
            i += 1

        if in_quote is not None:
            self.error(file, line, f"Unclosed quote {in_quote} in tag @{tag_name}")
            return []
        if bracket_depth != 0:
            self.error(file, line, f"Unmatched opening bracket '[' in tag @{tag_name}")
            return []
        if paren_depth != 0:
            self.error(file, line, f"Unmatched opening parenthesis '(' in tag @{tag_name}")
            return []

        # Process final token
        token_str = "".join(buf).strip()
        if not has_token_content and len(tokens) > 0:
            self.error(file, line, f"Trailing comma in list for tag @{tag_name}")
            return []
        if not has_token_content and len(tokens) == 0:
            return []

        tokens.append(self._strip_quotes(token_str))
        return tokens

    @staticmethod
    def _strip_quotes(val: str) -> str:
        s = val.strip()
        if len(s) >= 2:
            if (s.startswith('"') and s.endswith('"')) or (s.startswith("'") and s.endswith("'")):
                return s[1:-1]
        return s

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
        multiline_bracket_depth = 0

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
                is_duplicate = stage_id in self.stages
                if is_duplicate:
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
                # Only insert if not duplicate, so original stage is not replaced
                if not is_duplicate:
                    self.stages[stage_id] = new_stage
                current_stage = new_stage
                in_comment_block = True
                multiline_tag = None
                multiline_buffer = []
                multiline_bracket_depth = 0

            # If inside the metadata comment block for a stage
            if in_comment_block and current_stage is not None:
                has_comment_close = re_comment_end.search(line)

                # Process multiline continuation first
                if multiline_tag is not None:
                    # Strip leading '*' or whitespace
                    content = line.strip()
                    if content.startswith("*"):
                        content = content[1:].strip()

                    # Check for comment close without proper list close
                    effective_content = content
                    if has_comment_close:
                        close_idx = content.find("*/")
                        if close_idx != -1:
                            effective_content = content[:close_idx].strip()

                    # Track brackets in effective_content
                    for ch in effective_content:
                        if ch == '[':
                            multiline_bracket_depth += 1
                        elif ch == ']':
                            multiline_bracket_depth -= 1

                    multiline_buffer.append(effective_content)

                    if multiline_bracket_depth == 0:
                        # Completed multiline list
                        full_val = " ".join(multiline_buffer)
                        self._apply_tag_value(
                            current_stage,
                            multiline_tag,
                            full_val,
                            file_path,
                            multiline_start_line
                        )
                        multiline_tag = None
                        multiline_buffer = []
                    elif has_comment_close:
                        self.error(
                            file_path,
                            line_num,
                            f"Unclosed multiline list for tag @{multiline_tag} before comment end"
                        )
                        full_val = " ".join(multiline_buffer)
                        self._apply_tag_value(
                            current_stage,
                            multiline_tag,
                            full_val,
                            file_path,
                            multiline_start_line
                        )
                        multiline_tag = None
                        multiline_buffer = []

                # If not continuing a multiline list, check for new tag
                if multiline_tag is None:
                    tag_match = re_tag_line.match(line)
                    if tag_match:
                        tag_name = tag_match.group(1)
                        val = tag_match.group(2).strip()

                        # Strip trailing '*/' if on the same line
                        if val.endswith("*/"):
                            val = val[:-2].strip()

                        # Check for multiline opening
                        if (tag_name in KNOWN_LIST_TAGS or val.startswith("[")) and val.startswith("[") and not val.endswith("]"):
                            # Count bracket depth
                            bracket_depth = val.count("[") - val.count("]")
                            if bracket_depth > 0:
                                multiline_tag = tag_name
                                multiline_buffer = [val]
                                multiline_start_line = line_num
                                multiline_bracket_depth = bracket_depth
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
                line_ref = st.start_line if st else len(lines)
                self.error(
                    file_path,
                    line_ref,
                    f"Unclosed stage '{unclosed}' reached end of file without matching /* @end {unclosed} */"
                )

    def _apply_tag_value(self, stage: StageInfo, tag: str, value: str, file: str, line: int) -> None:
        val = value.strip()

        # Duplicate metadata tag detection
        if tag in stage.seen_tags:
            prev_line = stage.seen_tags[tag]
            if tag in ALL_KNOWN_TAGS:
                self.error(
                    file,
                    line,
                    f"Duplicate metadata tag '@{tag}' in stage '{stage.id}' (previously defined at line {prev_line})"
                )
            else:
                self.warn(
                    file,
                    line,
                    f"Repeated unknown metadata tag '@{tag}' in stage '{stage.id}' (previously defined at line {prev_line})"
                )
        stage.seen_tags[tag] = line

        # Tag assignment & validation
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
            if val not in VALID_PHASES:
                self.error(file, line, f"Invalid @phase '{val}'. Must be one of {sorted(VALID_PHASES)}")
            stage.phase = val
        elif tag == "reads":
            stage.reads = self.parse_list_tokens(val, file, line, tag)
        elif tag == "writes":
            stage.writes = self.parse_list_tokens(val, file, line, tag)
        elif tag == "opcodes":
            stage.opcodes = self.parse_list_tokens(val, file, line, tag)
        elif tag == "configures":
            stage.configures = self.parse_list_tokens(val, file, line, tag)
        elif tag == "consumes":
            stage.consumes = self.parse_list_tokens(val, file, line, tag)
        elif tag == "produces":
            stage.produces = self.parse_list_tokens(val, file, line, tag)
        elif tag == "depends":
            stage.depends = self.parse_list_tokens(val, file, line, tag)
        elif tag == "guard":
            # Opaque C boolean expression text
            if val.startswith("[") and val.endswith("]"):
                val = val[1:-1].strip()
            stage.guard = val
        elif tag == "early_exit":
            if val.startswith("[") and val.endswith("]"):
                stage.early_exit = self.parse_list_tokens(val, file, line, tag)
            else:
                stage.early_exit = val
        elif tag == "branch":
            if val.startswith("[") and val.endswith("]"):
                stage.branch = self.parse_list_tokens(val, file, line, tag)
            else:
                stage.branch = val
        elif tag in ("condition", "active_when", "gate"):
            self.warn(file, line, f"@{tag} is deprecated; use @guard, @early_exit, or @branch")
            if val.startswith("[") and val.endswith("]"):
                val = val[1:-1].strip()
            stage.condition = val
        elif tag in ("bypass_when", "bypass_condition", "bypassed"):
            if val.startswith("[") and val.endswith("]"):
                val = val[1:-1].strip()
            stage.bypass_when = val
            if tag == "bypassed":
                self.warn(file, line, "@bypassed is deprecated; use @guard, @early_exit, or @bypass_when")
        elif tag == "notes":
            stage.notes = val
        elif tag == "fields":
            self.warn(file, line, "@fields is deprecated; use @reads and @writes instead")
            stage.extras["fields"] = self.parse_list_tokens(val, file, line, tag)
        elif tag == "schema":
            self.warn(file, line, "@schema is deprecated; state owners are resolved via registry")
            stage.extras["schema"] = self.parse_list_tokens(val, file, line, tag)
        else:
            self.warn(file, line, f"Unknown annotation tag @{tag}; stored in extras")
            stage.extras[tag] = val
            if tag not in stage.unknown_tags:
                stage.unknown_tags[tag] = []
            stage.unknown_tags[tag].append((line, val))

    def _validate_stage_definition(self, stage: StageInfo, file: str, line: int) -> None:
        if not stage.title:
            self.error(file, stage.start_line, f"Stage '{stage.id}' is missing required @title")
        if not stage.scope:
            self.error(file, stage.start_line, f"Stage '{stage.id}' is missing required @scope ('engine' or 'voice')")
        if not stage.rate:
            self.error(file, stage.start_line, f"Stage '{stage.id}' is missing required @rate ('event', 'block', or 'sample')")

    def validate_global_dependencies(self) -> None:
        """
        Validates that all @depends references point to known stages,
        and detects dependency cycles via depth-first search.
        """
        for stage_id, stage in self.stages.items():
            for dep in stage.depends:
                if dep not in self.stages:
                    self.error(
                        stage.file,
                        stage.start_line,
                        f"Stage '{stage_id}' depends on unknown stage '{dep}'"
                    )

        # Detect dependency cycles using DFS
        # States: 0 = unvisited, 1 = visiting (on current path), 2 = visited
        state: Dict[str, int] = {s_id: 0 for s_id in self.stages}
        path: List[str] = []

        def dfs(node: str) -> None:
            state[node] = 1
            path.append(node)

            for dep in self.stages[node].depends:
                if dep not in self.stages:
                    continue
                if state[dep] == 1:
                    # Cycle detected!
                    cycle_start_idx = path.index(dep)
                    cycle_nodes = path[cycle_start_idx:] + [dep]
                    cycle_repr = " -> ".join(
                        f"{s} ({self.stages[s].file}:{self.stages[s].start_line})"
                        for s in cycle_nodes
                    )
                    self.error(
                        self.stages[node].file,
                        self.stages[node].start_line,
                        f"Dependency cycle detected: {cycle_repr}"
                    )
                elif state[dep] == 0:
                    dfs(dep)

            path.pop()
            state[node] = 2

        for stage_id in list(self.stages.keys()):
            if state[stage_id] == 0:
                dfs(stage_id)

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
    parser.add_argument("--check-deps", action="store_true", default=True, help="Verify inter-stage @depends references and cycle freedom")
    args = parser.parse_args()

    stage_parser = StageParser()
    for f in args.files:
        stage_parser.parse_file(f)

    if args.check_deps:
        stage_parser.validate_global_dependencies()

    error_count, warning_count = stage_parser.print_diagnostics()

    if stage_parser.has_errors():
        print(f"FAILED: Found {error_count} error(s) and {warning_count} warning(s).", file=sys.stderr)
        sys.exit(1)

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

    sys.exit(0)


if __name__ == "__main__":
    main()
