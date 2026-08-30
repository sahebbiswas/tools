# Tools

A small collection of Windows-oriented utility scripts.

## Contents

- [`colors3.py`](#colors3py) — ANSI true-color output helpers for Python console programs.
- [`exif_fix.py`](#exif_fixpy) — create JPEG copies with updated capture time and GPS metadata.
- [`file-github-issues`](#file-github-issues) — create a batch of GitHub issues from a Markdown file.

## `colors3.py`

Import this module to use named true-color print helpers or a colored gradient
banner in a Windows console that supports ANSI escape sequences.

```python
from colors3 import c_green, c_red, gradient

c_green("Completed", end="\n")
c_red("Something went wrong", end="\n")
gradient("My utility")
```

Available helpers are `c_red`, `c_green`, `c_cyan`, `c_yellow`, `c_gray`,
`c_white`, `c_magenta`, and `c_light_gray`. Each accepts `message` and an
`end` argument, just like `print`.

This module uses the Windows console API, so it is intended for Windows rather
than macOS or Linux.

## `exif_fix.py`

Creates JPEG copies of an image or a directory of images, assigning a capture
timestamp and GPS coordinates. Output files are written to an `out/` directory
in the current working directory, named from their assigned timestamps.

### Requirements

Use Python 3 and install the required packages:

```powershell
py -m pip install Pillow piexif GPSPhoto
```

### Usage

```powershell
py .\exif_fix.py <image-or-directory> "YYYY:MM:DD HH:MM:SS" "latitude,longitude" [--time minutes]
```

For example, to process the images in `photos` one minute apart:

```powershell
py .\exif_fix.py .\photos "2011:07:31 11:11:11" "22.577832,88.4007126" --time 1
```

The first image receives the supplied date and time; subsequent images are
offset by `--time` minutes (one minute by default). Supported input extensions
are `.jpg`, `.jpeg`, and `.png`; generated files are JPEGs.

## `file-github-issues`

[`file_github_issues.ps1`](file-github-issues/file_github_issues.ps1) reads a
schema-formatted Markdown file and creates one GitHub issue for each `##`
heading. It can also add any labels referenced by that Markdown and skips issue
titles already present in the target repository.

See [`ISSUE_MARKDOWN_SCHEMA.md`](file-github-issues/ISSUE_MARKDOWN_SCHEMA.md)
for the complete input format.

### Prerequisites

1. Install the [GitHub CLI](https://cli.github.com/):

   ```powershell
   winget install --id GitHub.cli
   ```

2. Authenticate it for the GitHub account that can create issues in the target
   repository:

   ```powershell
   gh auth login
   ```

3. Allow local PowerShell scripts to run. This is commonly needed when Windows
   reports that script execution is disabled. The recommended one-time setting
   affects only your user account:

   ```powershell
   Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
   ```

   Confirm the prompt with `Y`, then close and reopen PowerShell. To allow the
   script only in the current PowerShell window, without changing your saved
   policy, use:

   ```powershell
   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
   ```

   If the script was downloaded from the internet and remains blocked, run:

   ```powershell
   Unblock-File .\file-github-issues\file_github_issues.ps1
   ```

   Do not use an unrestricted, machine-wide execution policy just to run this
   utility.

### Markdown input

Create a file such as `issues.md`:

```markdown
---
repo: owner/repository
---

## Correct pagination boundary
Labels: bug, priority-p2

The endpoint returns one item more than its requested page size.

## Add CSV export
Labels: enhancement

Export search results as CSV.
```

Use `###` or deeper for headings inside an issue body. Every top-level `##`
heading starts another issue.

### Run it

Start with a dry run to validate the input without creating issues:

```powershell
.\file-github-issues\file_github_issues.ps1 -MarkdownPath .\issues.md -DryRun
```

Publish the batch after reviewing the dry-run output:

```powershell
.\file-github-issues\file_github_issues.ps1 -MarkdownPath .\issues.md
```

Override the Markdown file's `repo:` value when needed:

```powershell
.\file-github-issues\file_github_issues.ps1 -MarkdownPath .\issues.md -Repo other-owner/other-repo
```

The script requires PowerShell 5.1 or later. Its `-DelaySeconds` parameter
accepts the desired number of seconds between issue creations (default: `1`).

## License

Licensed under the [GNU GPL v3.0](LICENSE).
