#!/usr/bin/env python3
"""
Builds the three "deep-dive" docs pages (Software Stack Reference, Flight
Readiness Review, Flight Software Logbook) plus an index, as static HTML,
from CHANGELOG.md in the repo root and the structured data files in
docs-site/data/.

What's fully automatic (needs no human judgment, just parses the file):
  - The Logbook's entries, subsystem status grid and blocking list — all
    come straight out of CHANGELOG.md's "Current status" section and
    "Entries" section.
  - The subsystem status grid and blocking list on the Readiness page are
    synced from the same "Current status" section, so both pages agree
    with the changelog by construction.

What still needs a person (or Claude, asked to do it) to edit, because it's
judgment rather than parsing — architecture inventory and audit findings:
  - docs-site/data/stack.yaml   (Software Stack Reference)
  - docs-site/data/readiness.yaml (Flight Readiness Review's findings/gates)

Either way: editing any of CHANGELOG.md, stack.yaml or readiness.yaml and
pushing rebuilds the live site within a minute or two via
.github/workflows/docs.yml. There is no dependency on this script running
anywhere but GitHub's own runners.

Usage:
    python3 generate.py [--changelog PATH] [--out DIR]
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit("Missing dependency 'PyYAML'. Run: pip install -r docs-site/requirements.txt")

try:
    import markdown as md
except ImportError:
    sys.exit("Missing dependency 'Markdown'. Run: pip install -r docs-site/requirements.txt")

try:
    from jinja2 import Environment, FileSystemLoader, select_autoescape
except ImportError:
    sys.exit("Missing dependency 'Jinja2'. Run: pip install -r docs-site/requirements.txt")


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_CHANGELOG = REPO_ROOT / "CHANGELOG.md"
HUB_FILENAME = "drone_delivery_hub.html"
DEFAULT_HUB = REPO_ROOT / HUB_FILENAME
DATA_DIR = SCRIPT_DIR / "data"
TEMPLATES_DIR = SCRIPT_DIR / "templates"
DEFAULT_OUT = SCRIPT_DIR / "dist"

MD_EXTENSIONS = ["fenced_code", "tables", "sane_lists"]


def render_md(text: str | None, inline: bool = False) -> str:
    """Render a markdown string to HTML. inline=True strips the wrapping <p>."""
    if not text:
        return ""
    html = md.markdown(text.strip(), extensions=MD_EXTENSIONS)
    if inline and html.startswith("<p>") and html.endswith("</p>"):
        html = html[3:-4]
    return html


def mdf(obj, *fields):
    """Return obj with the named top-level string fields markdown-rendered
    (inline). Only prose fields should be listed — labels, ids and names are
    left as plain text so markdown syntax in a package name (underscores,
    braces) is never misread as formatting, and so they go through Jinja's
    normal autoescaping exactly once."""
    if obj is None:
        return obj
    out = dict(obj)
    for f in fields:
        if f in out and isinstance(out[f], str):
            out[f] = render_md(out[f], inline=True)
    return out


def mdf_list(items, *fields):
    return [mdf(item, *fields) for item in (items or [])]


def render_stack(data: dict) -> dict:
    data = dict(data)
    data["intro"] = render_md(data.get("intro"), inline=True)
    data["how_to_read"] = render_md(data.get("how_to_read"), inline=True)
    layers = []
    for layer in data.get("layers", []):
        layer = dict(layer)
        layer["components"] = mdf_list(layer.get("components"), "desc")
        layer["note"] = render_md(layer.get("note"), inline=True) if layer.get("note") else ""
        layer["tested"] = render_md(layer.get("tested"), inline=True) if layer.get("tested") else ""
        layers.append(layer)
    data["layers"] = layers
    data["decisions"] = [render_md(d, inline=True) for d in data.get("decisions", [])]
    return data


def render_readiness(data: dict) -> dict:
    data = dict(data)
    if data.get("audit"):
        data["audit"] = mdf(data["audit"], "standfirst", "banner")
    if data.get("verdict"):
        data["verdict"] = mdf(data["verdict"], "headline", "body")
    data["blockers"] = mdf_list(data.get("blockers"), "body", "fix", "resolution_text")
    data["safety_gaps"] = mdf_list(data.get("safety_gaps"), "body", "fix", "resolution_text")
    data["smaller_defects"] = mdf_list(data.get("smaller_defects"), "issue", "status")
    data["gates"] = mdf_list(data.get("gates"), "body", "gate")
    data["closing"] = [render_md(c, inline=True) for c in data.get("closing", [])]
    return data


# ---------------------------------------------------------------------------
# CHANGELOG.md parsing
# ---------------------------------------------------------------------------

TABLE_ROW_RE = re.compile(r"^\|(.+)\|\s*$")


def parse_md_table(lines: list[str]) -> list[list[str]]:
    """Parse a GitHub-flavoured markdown table starting at lines[0] (the header
    row). Returns rows of cells, header included as rows[0]."""
    rows = []
    for line in lines:
        line = line.rstrip()
        m = TABLE_ROW_RE.match(line)
        if not m:
            break
        cells = [c.strip() for c in m.group(1).split("|")]
        # skip the '---|---' separator row
        if all(re.fullmatch(r":?-+:?", c) for c in cells):
            continue
        rows.append(cells)
    return rows


def find_table(lines: list[str], start: int) -> tuple[list[list[str]], int]:
    """Find the next markdown table at/after lines[start]. Returns (rows, index
    of the line after the table)."""
    i = start
    while i < len(lines) and not TABLE_ROW_RE.match(lines[i].rstrip()):
        i += 1
    if i >= len(lines):
        return [], i
    j = i
    while j < len(lines) and TABLE_ROW_RE.match(lines[j].rstrip()):
        j += 1
    return parse_md_table(lines[i:j]), j


def parse_current_status(section_text: str) -> dict:
    lines = section_text.splitlines()
    subsystem_rows, after = find_table(lines, 0)
    subsystems = [{"name": r[0], "state": render_md(r[1], inline=True)} for r in subsystem_rows[1:]]

    # find "### Blocking a first flight"
    blocking = []
    closed_note = ""
    for idx, line in enumerate(lines[after:], start=after):
        if line.strip().startswith("### Blocking"):
            block_rows, block_end = find_table(lines, idx + 1)
            for r in block_rows[1:]:
                if len(r) >= 3:
                    blocking.append({"n": r[0], "what": render_md(r[1], inline=True), "who": r[2]})
            # trailing paragraph(s) after the table, up to end of section
            trailing = "\n".join(lines[block_end:]).strip()
            closed_note = render_md(trailing, inline=False)
            break

    return {"subsystems": subsystems, "blocking": blocking, "closed_note": closed_note}


ENTRY_HEADING_RE = re.compile(r"^##\s+(\d{4}-\d{2}-\d{2})\s*(.*?)\s*—\s*(.+?)\s*$")
ONE_SENTENCE_RE = re.compile(r"^\*\*In one sentence:\*\*\s*(.+)$", re.DOTALL)


def parse_entries(section_text: str) -> list[dict]:
    lines = section_text.splitlines()
    # find indices of top-level "## " headings within the Entries section
    heading_idxs = [i for i, l in enumerate(lines) if l.startswith("## ")]
    entries = []
    for n, idx in enumerate(heading_idxs):
        end = heading_idxs[n + 1] if n + 1 < len(heading_idxs) else len(lines)
        heading = lines[idx][3:].strip()
        m = ENTRY_HEADING_RE.match("## " + heading)
        if m:
            date, subtitle, title = m.group(1), m.group(2).strip(" ·,"), m.group(3)
        else:
            date, subtitle, title = "", "", heading
        body_lines = lines[idx + 1 : end]
        body_text = "\n".join(body_lines).strip()

        lede = ""
        one_sentence = ONE_SENTENCE_RE.match(body_text)
        rest = body_text
        if one_sentence:
            # only take up to the next blank-line-delimited paragraph break
            after_marker = body_text.split("**In one sentence:**", 1)[1]
            para, _, remainder = after_marker.lstrip().partition("\n\n")
            lede = render_md(para, inline=True)
            rest = remainder.strip()

        # split remaining body into ### subsections
        sub_idxs = [i for i, l in enumerate(rest.splitlines()) if l.startswith("### ")]
        rest_lines = rest.splitlines()
        sections = []
        if sub_idxs:
            # anything before the first ### is extra lede-ish prose
            preface = "\n".join(rest_lines[: sub_idxs[0]]).strip()
            if preface:
                sections.append({"heading": "", "html": render_md(preface)})
            for si, sidx in enumerate(sub_idxs):
                send = sub_idxs[si + 1] if si + 1 < len(sub_idxs) else len(rest_lines)
                shead = rest_lines[sidx][4:].strip()
                sbody = "\n".join(rest_lines[sidx + 1 : send]).strip()
                sections.append({"heading": shead, "html": render_md(sbody)})
        else:
            if rest.strip():
                sections.append({"heading": "", "html": render_md(rest)})

        entries.append(
            {
                "date": date,
                "subtitle": subtitle,
                "title": title,
                "lede": lede,
                "sections": sections,
            }
        )
    return entries


def strip_md_bold(text: str) -> str:
    return re.sub(r"\*\*(.+?)\*\*", r"\1", text).strip()


def parse_glossary(full_text: str) -> list[dict]:
    m = re.search(r"^#{1,2}\s+Glossary\s*$", full_text, re.MULTILINE)
    if not m:
        return []
    tail = full_text[m.end():]
    # stop at the next top-level heading, if any
    stop = re.search(r"^#\s+\S", tail, re.MULTILINE)
    section = tail[: stop.start()] if stop else tail
    lines = section.splitlines()
    rows, _ = find_table(lines, 0)
    if rows:
        return [
            {"term": strip_md_bold(r[0]), "definition": render_md(r[1], inline=True)}
            for r in rows[1:]
        ]
    # fall back to a definition-list style: "**term** — definition"
    glossary = []
    for line in lines:
        m2 = re.match(r"^[-*]?\s*\*\*(.+?)\*\*\s*[—:-]\s*(.+)$", line.strip())
        if m2:
            glossary.append({"term": m2.group(1), "definition": render_md(m2.group(2), inline=True)})
    return glossary


def parse_changelog(text: str) -> dict:
    status_m = re.search(r"^##\s+Current status\s*$", text, re.MULTILINE)
    entries_m = re.search(r"^#\s+Entries\s*$", text, re.MULTILINE)

    status = {"subsystems": [], "blocking": [], "closed_note": ""}
    if status_m:
        section_end = entries_m.start() if entries_m else len(text)
        # also stop at the next "---" horizontal rule, whichever comes first
        rule_m = re.search(r"^---\s*$", text[status_m.end():section_end], re.MULTILINE)
        stop = status_m.end() + rule_m.start() if rule_m else section_end
        status = parse_current_status(text[status_m.end():stop])

    entries = []
    if entries_m:
        next_h1 = re.search(r"^#\s+\S", text[entries_m.end():], re.MULTILINE)
        section_end = entries_m.end() + next_h1.start() if next_h1 else len(text)
        entries = parse_entries(text[entries_m.end():section_end])

    glossary = parse_glossary(text)

    return {"status": status, "entries": entries, "glossary": glossary}


# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------


def load_yaml(name: str) -> dict:
    path = DATA_DIR / name
    if not path.exists():
        return {}
    with path.open() as f:
        return yaml.safe_load(f) or {}


def build(changelog_path: Path, out_dir: Path, hub_path: Path | None = None) -> None:
    if not changelog_path.exists():
        sys.exit(f"CHANGELOG.md not found at {changelog_path}")
    changelog_text = changelog_path.read_text(encoding="utf-8")
    changelog = parse_changelog(changelog_text)

    stack = render_stack(load_yaml("stack.yaml"))
    readiness = render_readiness(load_yaml("readiness.yaml"))
    # subsystem board + blocking list are always synced live from CHANGELOG.md
    readiness["subsystem_board"] = changelog["status"]["subsystems"]
    readiness["blocking"] = changelog["status"]["blocking"]
    readiness["closed_note"] = changelog["status"]["closed_note"]

    built_at = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

    env = Environment(
        loader=FileSystemLoader(str(TEMPLATES_DIR)),
        autoescape=select_autoescape(disabled_extensions=("txt",)),
        trim_blocks=True,
        lstrip_blocks=True,
    )
    # markdown is already rendered to HTML by this point — mark trusted strings
    # safe so Jinja's autoescaping doesn't double-escape them. We do this with
    # a filter rather than turning autoescaping off globally.
    from markupsafe import Markup

    env.filters["safe_html"] = lambda s: Markup(s) if s else Markup("")

    common = {
        "built_at": built_at,
        "status": changelog["status"],
        "entry_count": len(changelog["entries"]),
    }

    pages = {
        "index.html": {**common, "here": "index"},
        "stack.html": {**common, "here": "stack", "stack": stack},
        "readiness.html": {**common, "here": "readiness", "readiness": readiness},
        "logbook.html": {
            **common,
            "here": "logbook",
            "entries": changelog["entries"],
            "glossary": changelog["glossary"],
        },
    }

    out_dir.mkdir(parents=True, exist_ok=True)
    for filename, ctx in pages.items():
        template = env.get_template(filename)
        (out_dir / filename).write_text(template.render(**ctx), encoding="utf-8")
        print(f"wrote {out_dir / filename}")

    # The standalone hub page is copied in verbatim rather than generated, so
    # that the deployed site is the whole documentation surface instead of just
    # the four generated pages. Doing it here (not in the workflow) keeps a
    # local build byte-identical to what CI publishes.
    if hub_path is not None:
        if hub_path.exists():
            shutil.copy2(hub_path, out_dir / HUB_FILENAME)
            print(f"copied {out_dir / HUB_FILENAME}")
        else:
            print(f"note: no hub page at {hub_path} - skipping it", file=sys.stderr)

    # .nojekyll so GitHub Pages serves files/folders starting with an
    # underscore (Jinja/static assets, if any get added later) as-is
    (out_dir / ".nojekyll").write_text("", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--changelog", type=Path, default=DEFAULT_CHANGELOG)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument(
        "--hub",
        type=Path,
        default=DEFAULT_HUB,
        help="standalone hub page copied into the output as-is",
    )
    args = parser.parse_args()
    build(args.changelog, args.out, args.hub)


if __name__ == "__main__":
    main()
