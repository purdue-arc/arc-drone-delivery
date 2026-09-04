# Flight docs — generator

This folder builds the three "deep-dive" documentation pages that used to
be hand-maintained Claude Artifacts (Software Stack Reference, Flight
Readiness Review, Flight Software Logbook), plus an index page linking
them, as a small static site. `.github/workflows/docs.yml` rebuilds and
deploys it to GitHub Pages automatically on every push that touches
`CHANGELOG.md` or this folder — there is no dependency on anyone (or
Claude) manually republishing anything.

## Why this exists

The Artifacts were great, but nothing outside a Claude session could
update them — no CI job can call the Artifact publish tool, and an
Artifact page can't fetch your repo's files at load time either (it's
sandboxed). So "live, auto-updating docs" meant moving the content into
the repo itself, where your own GitHub Actions workflow can rebuild and
redeploy it on every commit, same as any other CI job.

## What's actually automatic vs. what still needs a person

**Fully automatic — parsed straight out of `CHANGELOG.md`, no editing
here required:**
- The Logbook's entries (every `## YYYY-MM-DD — Title` block, its
  `**In one sentence:**` lede, and its `### ` subsections)
- The subsystem status grid (`## Current status`'s table)
- The "blocking a first flight" list (the `### Blocking a first flight`
  table)
- The glossary (`# Glossary`'s table)

Add an entry to `CHANGELOG.md` the normal way — the template and rules are
already at the top of that file — and the Logbook and the status grids on
both the index and Readiness pages update on the next build. Nothing in
`docs-site/` needs to change for a normal changelog entry.

**Needs a person (or Claude, asked to do it) to edit, because it's
judgment rather than parsing:**
- `docs-site/data/stack.yaml` — the Software Stack Reference's component
  inventory. Update it when the repo's actual structure changes: a new
  package, something retired, a status flip from "not wired up" to
  "flight path".
- `docs-site/data/readiness.yaml` — the Flight Readiness Review's audit
  narrative: the verdict, the numbered blockers/safety-gaps/defects, and
  the gates to a first flight test. Update it when a new readiness audit
  happens. (Its subsystem-status grid and blocking list are pulled live
  from `CHANGELOG.md` regardless, so those two bits never go stale even if
  nobody's touched this file in a while.)

Either way: commit it, push it, and the live site updates within a minute
or two. That's the whole point.

## Building locally

```bash
pip install -r docs-site/requirements.txt
python3 docs-site/generate.py            # reads ../CHANGELOG.md, writes dist/
open docs-site/dist/index.html           # or just double-click it
```

`--changelog PATH` and `--out DIR` override the defaults if you're testing
against a different changelog or want the output somewhere else.

## One-time repo setup

1. Commit this `docs-site/` folder and `.github/workflows/docs.yml` to the
   repo (wherever `drone_delivery_hub.html` also lives — the workflow
   assumes `CHANGELOG.md` is at the repo root, matching the rest of the
   monorepo).
2. In the repo's **Settings → Pages**, set **Source** to **GitHub
   Actions**. (If you're not sure whether this is already set, just push —
   the workflow will fail with a clear error on the deploy step if it
   isn't, rather than doing anything silently wrong.)
3. Push. The Actions tab will show the `docs.yml` run; once it's green,
   the site is live at the URL GitHub Pages assigns you (shown in the
   Settings → Pages panel, and in the workflow run's `deploy` job output).
4. Update the three `Docs & Links` cards in `drone_delivery_hub.html` —
   set the `DOCS_BASE` constant near the top of its `<script>` block to
   that Pages URL. It's currently a placeholder.

## Files

```
docs-site/
  generate.py           the whole build — parses CHANGELOG.md, loads the
                         YAML data files, renders the four HTML pages
  requirements.txt       PyYAML, Markdown, Jinja2, MarkupSafe
  templates/
    _style.html           shared CSS (one design system for all 4 pages)
    _header.html           shared nav
    _footer.html           shared "built at / source" footer
    index.html, stack.html, readiness.html, logbook.html
  data/
    stack.yaml              Software Stack Reference source data
    readiness.yaml           Flight Readiness Review source data
  dist/                  build output (gitignored; the workflow builds
                          this fresh every run — nothing here needs to be
                          committed)
```
