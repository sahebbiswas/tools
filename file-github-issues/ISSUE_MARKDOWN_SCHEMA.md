# GitHub Issue Batch — Markdown Schema

A single `.md` file describing a batch of GitHub issues to file. Feed this file to
`file_github_issues.ps1` and it creates one issue per entry.

## Structure

```markdown
---
repo: owner/repo-name
---

## Title of first issue
Labels: bug, security, priority-p1

Body of the issue, in normal markdown. Can span multiple paragraphs,
code fences, bullet lists, etc.

## Title of second issue
Labels: enhancement, priority-p2

Body of the second issue...
```

## Rules

1. **Front matter** (required): the file must start with a `---` line, one or more
   `key: value` lines, then a closing `---` line.
   - `repo` (required): `owner/repo-name` to file issues against. Can be overridden
     per-run with the script's `-Repo` parameter without editing the file.

2. **Issues**: every top-level `## ` (H2) heading starts a new issue. The heading text
   (with `## ` stripped) becomes the issue **title**.

3. **Labels line** (optional): the line immediately after the `## ` heading, if it
   starts with `Labels:` (case-insensitive), is parsed as a comma-separated label list
   and is not included in the issue body. If omitted, the issue is created with no
   labels.

4. **Body**: everything after the heading (and the `Labels:` line, if present) up to
   the next top-level `## ` heading or end of file. Leading/trailing blank lines are
   trimmed. Use `###` or deeper for any sub-headings *inside* an issue body — a `##`
   inside a body will incorrectly be parsed as a new issue boundary.

5. **Label conventions** (not enforced by the script, just a suggestion so future
   batches stay consistent): `bug` / `enhancement` / `tech-debt` for type,
   `security` as a cross-cutting tag, `priority-p1`..`priority-p4` for priority.
   The script auto-creates any label it hasn't seen in the target repo yet.

## Minimal example

```markdown
---
repo: octocat/hello-world
---

## Off-by-one error in pagination
Labels: bug, priority-p2

`page_size + 1` items are returned instead of `page_size`. Repro: call
`/items?page_size=10` and count the response array length.

**Fix:** change `<=` to `<` in the loop bound at `paginate.go:42`.
```
