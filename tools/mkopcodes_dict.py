#!/usr/bin/env python3
"""
mkopcodes_dict.py — Generate Skode 2.1.0 Command Dictionary & Autocompletion Assets

Generates:
  1. skred-web/skode_commands.js (Browser autocompletion library)
  2. skode/app/editor/skode_commands.js (Lustre web editor autocompletion library)
  3. skode/src/skode/gen/opcodes_gen.gleam (Gleam typed definitions for 2.1.0 opcodes)
"""

import os
import re
import sys
import json

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SK3_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
BOOK_ROOT = os.path.abspath(os.path.join(SK3_ROOT, ".."))

SKRED_OPCODES_GEN = os.path.join(SK3_ROOT, "src", "gen", "skred_opcodes_gen.c")
SKRED_WEB_DIR = os.path.join(BOOK_ROOT, "skred-web")
SKODE_DIR = os.path.join(BOOK_ROOT, "skode")
SKODE_EDITOR_DIR = os.path.join(SKODE_DIR, "app", "editor")

def main():
    if not os.path.exists(SKRED_OPCODES_GEN):
        print(f"Error: {SKRED_OPCODES_GEN} not found.", file=sys.stderr)
        sys.exit(1)

    with open(SKRED_OPCODES_GEN, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()

    # Collect word definitions
    word_entries = []
    for m in re.finditer(
        r"\{\s*\.name\s*=\s*\"([^\"]+)\",\s*\.alias\s*=\s*\"([^\"]*)\",\s*\.group\s*=\s*([A-Z0-9_]+),\s*\.clause_count\s*=\s*(\d+),\s*\.clauses\s*=\s*([a-zA-Z0-9_]+)",
        text,
        re.DOTALL
    ):
        name, alias, group, clause_count, clause_var = m.groups()
        cm = re.search(r"static const skode_clause_t " + clause_var + r"\[\] = \{(.*?)\n\};", text, re.DOTALL)
        summary = ""
        doc = ""
        if cm:
            c_body = cm.group(1)
            sm = re.search(r"\.summary\s*=\s*\"([^\"]*)\"", c_body)
            if sm: summary = sm.group(1)
            dm = re.search(r"\.doc\s*=\s*\"([^\"]*)\"", c_body)
            if dm: doc = dm.group(1)

        group_clean = group.replace("SKODE_GRP_", "").lower()
        word_entries.append({
            "name": name,
            "alias": alias,
            "group": group_clean,
            "summary": summary or doc or ""
        })

    # Collect all tokens to complete
    tokens = {}
    for w in word_entries:
        summary = w["summary"]
        if w["alias"]:
            tokens[w["alias"]] = {
                "cmd": w["alias"],
                "alt": w["name"],
                "group": w["group"],
                "summary": summary
            }
        if w["name"]:
            tokens[w["name"]] = {
                "cmd": w["name"],
                "alt": w["alias"],
                "group": w["group"],
                "summary": summary
            }

    # Shell / meta commands
    shell_cmds = [
        ("-restart", "reboot audio engine with optional voices and bufsize (e.g. -restart 16 256)"),
        (".restart", "reboot audio engine (alias for -restart)"),
        ("-stop", "stop audio engine output"),
        (".stop", "stop audio engine output"),
        ("-font", "toggle or set terminal font: mono or 2p"),
        (".font", "toggle or set terminal font: mono or 2p"),
        ("-dark", "switch UI to dark Atari theme"),
        (".dark", "switch UI to dark Atari theme"),
        ("-light", "switch UI to light Erasmus theme"),
        (".light", "switch UI to light Erasmus theme"),
        ("-theme", "set theme: dark or light"),
        (".theme", "set theme: dark or light"),
        ("-help", "display interactive help and cheat sheet"),
        (".help", "display interactive help and cheat sheet"),
        ("-clear", "clear terminal output"),
        (".clear", "clear terminal output"),
        ("-status", "print current audio engine and voice allocation status"),
        (".status", "print current audio engine and voice allocation status"),
        ("-panic", "silence all voices immediately"),
        (".panic", "silence all voices immediately"),
        ("?", "query help or documentation"),
        ("??", "list all available commands in engine")
    ]

    for cmd, desc in shell_cmds:
        if cmd not in tokens:
            tokens[cmd] = {
                "cmd": cmd,
                "alt": "",
                "group": "sys",
                "summary": desc
            }

    tokens_list = list(tokens.values())
    tokens_list.sort(key=lambda x: x["cmd"])

    print(f"mkopcodes_dict: gathered {len(tokens_list)} unique tokens across {len(word_entries)} words.")

    # 1. Generate JS dictionary
    js_content = generate_js(tokens_list)
    if os.path.exists(SKRED_WEB_DIR):
        out_web_js = os.path.join(SKRED_WEB_DIR, "skode_commands.js")
        with open(out_web_js, "w", encoding="utf-8") as f:
            f.write(js_content)
        print(f"mkopcodes_dict: wrote {out_web_js}")

    if os.path.exists(SKODE_EDITOR_DIR):
        out_editor_js = os.path.join(SKODE_EDITOR_DIR, "skode_commands.js")
        with open(out_editor_js, "w", encoding="utf-8") as f:
            f.write(js_content)
        print(f"mkopcodes_dict: wrote {out_editor_js}")

    # 2. Generate Gleam module
    gleam_gen_dir = os.path.join(SKODE_DIR, "src", "skode", "gen")
    if os.path.exists(gleam_gen_dir):
        out_gleam = os.path.join(gleam_gen_dir, "opcodes_gen.gleam")
        gleam_content = generate_gleam(tokens_list, word_entries)
        with open(out_gleam, "w", encoding="utf-8") as f:
            f.write(gleam_content)
        print(f"mkopcodes_dict: wrote {out_gleam}")


def generate_js(tokens):
    json_data = json.dumps(tokens, separators=(',', ':'))
    return f"""// Generated by sk3 tools/mkopcodes_dict.py (Skode 2.1.0). DO NOT EDIT DIRECTLY.
(function(root) {{
  "use strict";

  const RAW_COMMANDS = {json_data};

  const DICT = Object.create(null);
  const LIST = [];

  for (let i = 0; i < RAW_COMMANDS.length; i++) {{
    const item = RAW_COMMANDS[i];
    DICT[item.cmd] = item;
    LIST.push(item.cmd);
  }}

  // Sort: primary sort case-insensitive, secondary exact
  LIST.sort((a, b) => {{
    const al = a.toLowerCase();
    const bl = b.toLowerCase();
    if (al < bl) return -1;
    if (al > bl) return 1;
    return a.localeCompare(b);
  }});

  function skodeComplete(prefix) {{
    if (!prefix) return [];
    // 1. Exact case prefix matches
    let matches = LIST.filter(cmd => cmd.startsWith(prefix));
    // 2. Fallback to case-insensitive if no matches
    if (matches.length === 0) {{
      const lower = prefix.toLowerCase();
      matches = LIST.filter(cmd => cmd.toLowerCase().startsWith(lower));
    }}
    return matches;
  }}

  function skodeCommonPrefix(words) {{
    if (!words || !words.length) return "";
    let p = words[0];
    for (let i = 1; i < words.length; i++) {{
      while (!words[i].startsWith(p)) {{
        p = p.slice(0, -1);
        if (!p) return "";
      }}
    }}
    return p;
  }}

  function skodeGetInfo(cmd) {{
    return DICT[cmd] || null;
  }}

  root.SKODE_COMMANDS_DICT = DICT;
  root.SKODE_COMPLETION_LIST = LIST;
  root.skodeComplete = skodeComplete;
  root.skodeCommonPrefix = skodeCommonPrefix;
  root.skodeGetInfo = skodeGetInfo;
}})(typeof window !== "undefined" ? window : (typeof globalThis !== "undefined" ? globalThis : this));
"""


def generate_gleam(tokens, word_entries):
    lines = [
        "//// Generated by sk3 tools/mkopcodes_dict.py (Skode 2.1.0). DO NOT EDIT.",
        "",
        "import gleam/list",
        "import gleam/string",
        "",
        "pub type CommandInfo {",
        "  CommandInfo(",
        "    cmd: String,",
        "    alt: String,",
        "    group: String,",
        "    summary: String,",
        "  )",
        "}",
        "",
        f"pub const total_commands = {len(tokens)}",
        "",
        "pub const all_commands: List(String) = ["
    ]
    for t in tokens:
        cmd_esc = t["cmd"].replace("\\", "\\\\").replace('"', '\\"')
        lines.append(f'  "{cmd_esc}",')
    lines.append("]")
    lines.append("")
    lines.append("pub fn complete(prefix: String) -> List(String) {")
    lines.append("  case prefix {")
    lines.append('    "" -> []')
    lines.append("    _ -> {")
    lines.append("      let exact = list.filter(all_commands, fn(c) { string.starts_with(c, prefix) })")
    lines.append("      case exact {")
    lines.append("        [_, ..] -> exact")
    lines.append("        [] -> {")
    lines.append("          let lower = string.lowercase(prefix)")
    lines.append("          list.filter(all_commands, fn(c) { string.starts_with(string.lowercase(c), lower) })")
    lines.append("        }")
    lines.append("      }")
    lines.append("    }")
    lines.append("  }")
    lines.append("}")
    lines.append("")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    main()
