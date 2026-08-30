#Requires -Version 5.1
<#
.SYNOPSIS
    Files GitHub issues from a schema-defined markdown file, using the GitHub CLI (gh).
    See ISSUE_MARKDOWN_SCHEMA.md for the expected input format.

.DESCRIPTION
    Generic, repo-agnostic, content-agnostic. Point it at any markdown file that
    follows the schema and it creates one GitHub issue per entry. No editing of this
    script is needed to run a future analysis -- just produce a new markdown file
    that follows the schema.

    Prereqs:
      1. Install gh: https://cli.github.com/  (winget install --id GitHub.cli)
      2. Authenticate: gh auth login

.PARAMETER MarkdownPath
    Path to the schema-formatted markdown file (see ISSUE_MARKDOWN_SCHEMA.md).

.PARAMETER Repo
    Optional. Overrides the `repo:` value in the markdown file's front matter.

.PARAMETER DryRun
    Print what would be created without actually creating anything.

.PARAMETER DelaySeconds
    Seconds to sleep between issue creations (default 1, be polite to the API on
    large batches).

.EXAMPLE
    .\file_github_issues.ps1 -MarkdownPath .\cgull-issues-backlog.md -DryRun

.EXAMPLE
    .\file_github_issues.ps1 -MarkdownPath .\cgull-issues-backlog.md

.EXAMPLE
    .\file_github_issues.ps1 -MarkdownPath .\findings.md -Repo someorg/somerepo
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$MarkdownPath,

    [string]$Repo,

    [switch]$DryRun,

    [int]$DelaySeconds = 1
)

$ErrorActionPreference = "Stop"

try {
    [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
} catch {
    # Ignore if console encoding cannot be changed in host environment
}

# ---------------------------------------------------------------------------
# UI & Formatting Helpers
# ---------------------------------------------------------------------------

function Format-ProgressBar([int]$Value, [int]$Total, [int]$Width = 24) {
    if ($Total -le 0) { return ("$([char]0x2591)" * $Width) + "   0.0%" }
    $ratio = [Math]::Min(1.0, [Math]::Max(0.0, ($Value / $Total)))
    $filled = [int][Math]::Round($ratio * $Width)
    $empty = $Width - $filled
    $pct = ($ratio * 100).ToString("F1").PadLeft(5)
    return ("$([char]0x2588)" * $filled) + ("$([char]0x2591)" * $empty) + " $pct%"
}

function Show-HeaderBanner([string]$TargetRepo, [string]$Path, [bool]$IsDryRun, [int]$IssueCount, [hashtable]$LabelStats) {
    Write-Host ""
    Write-Host ("=" * 78) -ForegroundColor Cyan
    Write-Host "               GITHUB ISSUE BATCH PUBLISHER" -ForegroundColor Cyan
    Write-Host ("=" * 78) -ForegroundColor Cyan
    Write-Host "  Target Repo  : " -NoNewline -ForegroundColor Gray
    Write-Host "$TargetRepo" -ForegroundColor White
    Write-Host "  Source File  : " -NoNewline -ForegroundColor Gray
    Write-Host "$Path" -ForegroundColor White
    Write-Host "  Mode         : " -NoNewline -ForegroundColor Gray
    if ($IsDryRun) {
        Write-Host "[DRY RUN] (Simulation -- no issues will be created)" -ForegroundColor Yellow
    } else {
        Write-Host "[LIVE] (Publishing issues directly to GitHub)" -ForegroundColor Green
    }
    Write-Host "  Issues Found : " -NoNewline -ForegroundColor Gray
    Write-Host "$IssueCount" -ForegroundColor White
    if ($LabelStats -and $LabelStats.Count -gt 0) {
        $labelSummary = ($LabelStats.Keys | Sort-Object | ForEach-Object { "$_ ($($LabelStats[$_]))" }) -join ', '
        Write-Host "  Labels Found : " -NoNewline -ForegroundColor Gray
        Write-Host "$($LabelStats.Count) distinct ($labelSummary)" -ForegroundColor DarkCyan
    }
    Write-Host ("-" * 78) -ForegroundColor DarkGray
    Write-Host ""
}

function Show-IssueCard {
    param(
        [int]$Index,
        [int]$Total,
        [string]$Title,
        [string]$Labels,
        [string]$Status,
        [string]$Url = "",
        [string]$ErrorMsg = "",
        [string]$Body = "",
        [bool]$IsDryRun = $false
    )

    $idxStr = "[$($Index.ToString().PadLeft(2, '0'))/$($Total.ToString().PadLeft(2, '0'))]"
    $lineColor = if ($IsDryRun) { "Yellow" } else { "Cyan" }
    Write-Host "$idxStr " -NoNewline -ForegroundColor $lineColor
    Write-Host ("-" * (78 - $idxStr.Length - 1)) -ForegroundColor DarkGray

    Write-Host "  Title  : " -NoNewline -ForegroundColor Gray
    Write-Host "$Title" -ForegroundColor White

    if (-not [string]::IsNullOrWhiteSpace($Labels)) {
        Write-Host "  Labels : " -NoNewline -ForegroundColor Gray
        $tagList = ($Labels -split ',' | ForEach-Object { "[$($_.Trim())]" }) -join ' '
        Write-Host "$tagList" -ForegroundColor Magenta
    }

    Write-Host "  Status : " -NoNewline -ForegroundColor Gray
    switch ($Status) {
        "CREATED" {
            Write-Host "$([char]0x2714) [CREATED]" -ForegroundColor Green
            if ($Url) {
                Write-Host "  URL    : " -NoNewline -ForegroundColor Gray
                Write-Host "$Url" -ForegroundColor Cyan
            }
        }
        "SKIPPED" {
            Write-Host "$([char]0x23ED) [SKIPPED]" -ForegroundColor Yellow -NoNewline
            Write-Host " (Already exists in repository)" -ForegroundColor DarkGray
        }
        "DRYRUN" {
            Write-Host "[DRY-RUN]" -ForegroundColor Yellow -NoNewline
            Write-Host " (Validated successfully)" -ForegroundColor DarkGray
            if ($Body) {
                Write-Host "  Body   :" -ForegroundColor Gray
                foreach ($line in ($Body -split "`n")) {
                    Write-Host "    $line" -ForegroundColor DarkGray
                }
            }
        }
        "FAILED" {
            Write-Host "$([char]0x2718) [FAILED]" -ForegroundColor Red
            if ($ErrorMsg) {
                Write-Host "  Error  : " -NoNewline -ForegroundColor Red
                Write-Host "$ErrorMsg" -ForegroundColor Red
            }
        }
    }
    Write-Host ""
}

function Show-SummaryCard {
    param(
        [string]$TargetRepo,
        [bool]$IsDryRun,
        [int]$Total,
        [int]$Created,
        [int]$Skipped,
        [int]$Failed,
        [hashtable]$LabelStats,
        [timespan]$Elapsed
    )

    Write-Host ""
    Write-Host ("=" * 78) -ForegroundColor Cyan
    Write-Host "                            BATCH EXECUTION SUMMARY" -ForegroundColor Cyan
    Write-Host ("=" * 78) -ForegroundColor Cyan
    Write-Host "  Target Repo  : " -NoNewline -ForegroundColor Gray
    Write-Host "$TargetRepo" -ForegroundColor White
    Write-Host "  Mode         : " -NoNewline -ForegroundColor Gray
    if ($IsDryRun) {
        Write-Host "DRY RUN (Simulation)" -ForegroundColor Yellow
    } else {
        Write-Host "LIVE PUBLISH" -ForegroundColor Green
    }
    Write-Host "  Duration     : " -NoNewline -ForegroundColor Gray
    Write-Host "$($Elapsed.ToString('hh\:mm\:ss'))" -ForegroundColor White
    Write-Host ("-" * 78) -ForegroundColor DarkGray

    Write-Host "  STATISTICS" -ForegroundColor Cyan
    Write-Host "  Total Issues : " -NoNewline -ForegroundColor Gray
    Write-Host "$($Total.ToString().PadLeft(4))" -ForegroundColor White

    $cLabel = if ($IsDryRun) { "Simulated    : " } else { "Created      : " }
    Write-Host "  $cLabel" -NoNewline -ForegroundColor Gray
    Write-Host "$($Created.ToString().PadLeft(4))  " -NoNewline -ForegroundColor Green
    Write-Host (Format-ProgressBar -Value $Created -Total $Total) -ForegroundColor Green

    Write-Host "  Skipped      : " -NoNewline -ForegroundColor Gray
    Write-Host "$($Skipped.ToString().PadLeft(4))  " -NoNewline -ForegroundColor Yellow
    Write-Host (Format-ProgressBar -Value $Skipped -Total $Total) -ForegroundColor Yellow

    Write-Host "  Failed       : " -NoNewline -ForegroundColor Gray
    $failColor = if ($Failed -gt 0) { "Red" } else { "DarkGray" }
    Write-Host "$($Failed.ToString().PadLeft(4))  " -NoNewline -ForegroundColor $failColor
    Write-Host (Format-ProgressBar -Value $Failed -Total $Total) -ForegroundColor $failColor

    if ($LabelStats -and $LabelStats.Count -gt 0) {
        Write-Host ("-" * 78) -ForegroundColor DarkGray
        Write-Host "  LABEL BREAKDOWN" -ForegroundColor Cyan
        foreach ($key in ($LabelStats.Keys | Sort-Object)) {
            Write-Host "    $([char]0x2022) $($key.PadRight(18)) : " -NoNewline -ForegroundColor Gray
            Write-Host "$($LabelStats[$key]) issue(s)" -ForegroundColor DarkCyan
        }
    }

    Write-Host ("=" * 78) -ForegroundColor Cyan
    if ($Failed -eq 0) {
        if ($IsDryRun) {
            Write-Host "  $([char]0x2714) Dry run completed successfully! All issues validated." -ForegroundColor Green
        } else {
            Write-Host "  $([char]0x2714) Batch publishing completed successfully!" -ForegroundColor Green
        }
    } else {
        Write-Host "  $([char]0x2718) Completed with $Failed error(s). Please review logs above." -ForegroundColor Red
    }
    Write-Host ("=" * 78) -ForegroundColor Cyan
    Write-Host ""
}

# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

function Read-IssueMarkdown {
    <#
    Parses a schema-formatted markdown file into a front-matter hashtable and
    an array of issue objects (Title, Labels, Body). See ISSUE_MARKDOWN_SCHEMA.md.
    #>
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path $Path)) {
        throw "Markdown file not found: $Path"
    }

    $raw = Get-Content -Path $Path -Raw -Encoding UTF8
    # Normalize line endings so parsing is consistent regardless of how the
    # file was authored/saved.
    $raw = $raw -replace "`r`n", "`n"

    # --- Front matter ---
    $frontMatter = @{}
    $body = $raw
    if ($raw -match '(?s)^---\s*\n(.*?)\n---\s*\n(.*)$') {
        $fmBlock = $Matches[1]
        $body = $Matches[2]
        foreach ($line in ($fmBlock -split "`n")) {
            if ($line -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.*)$') {
                $frontMatter[$Matches[1].Trim()] = $Matches[2].Trim()
            }
        }
    } else {
        throw "Markdown file is missing required front matter (a '---' ... '---' block at the top with at least 'repo: owner/name'). See ISSUE_MARKDOWN_SCHEMA.md."
    }

    if (-not $frontMatter.ContainsKey('repo') -or [string]::IsNullOrWhiteSpace($frontMatter['repo'])) {
        throw "Front matter is missing required 'repo: owner/name' key. See ISSUE_MARKDOWN_SCHEMA.md."
    }

    # --- Issues: split on top-level '## ' headings ---
    # Using -split with a capturing regex keeps the heading text so we don't
    # need a second pass to re-extract titles.
    $chunks = [System.Text.RegularExpressions.Regex]::Split($body, '(?m)^## (.+?)\s*$')
    # Split on a capturing group yields: [pre-text, title1, body1, title2, body2, ...]
    # pre-text (chunks[0]) should be blank/whitespace-only; ignore it.

    $issues = @()
    for ($i = 1; $i -lt $chunks.Count; $i += 2) {
        $title = $chunks[$i].Trim()
        $title = $title -replace '[\u2013\u2014]', '-'
        $rest = $chunks[$i + 1]

        $labels = ""
        # Labels line, if present, is the first non-blank line right after the heading.
        if ($rest -match '(?s)^\s*\n*Labels:\s*(.*?)\s*\n(.*)$') {
            $labels = $Matches[1].Trim()
            $rest = $Matches[2]
        } elseif ($rest -match '(?s)^\s*Labels:\s*(.*?)\s*\n(.*)$') {
            $labels = $Matches[1].Trim()
            $rest = $Matches[2]
        }

        $bodyText = $rest.Trim()
        $bodyText = $bodyText -replace '[\u2013\u2014]', '-'

        if ([string]::IsNullOrWhiteSpace($title)) {
            continue
        }

        $issues += [PSCustomObject]@{
            Title  = $title
            Labels = $labels
            Body   = $bodyText
        }
    }

    if ($issues.Count -eq 0) {
        throw "No issues found in '$Path'. Each issue needs a top-level '## Title' heading. See ISSUE_MARKDOWN_SCHEMA.md."
    }

    return [PSCustomObject]@{
        FrontMatter = $frontMatter
        Issues      = $issues
    }
}

# ---------------------------------------------------------------------------
# GitHub interaction
# ---------------------------------------------------------------------------

function Test-GhInstalled {
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
        Write-Host ""
        Write-Host ("=" * 78) -ForegroundColor Red
        Write-Host "  $([char]0x2718) GitHub CLI 'gh' is not installed or not on PATH." -ForegroundColor Red
        Write-Host "    Install via: winget install --id GitHub.cli" -ForegroundColor Yellow
        Write-Host "    Or visit:    https://cli.github.com/" -ForegroundColor Yellow
        Write-Host ("=" * 78) -ForegroundColor Red
        Write-Host ""
        exit 1
    }
}

function Test-GhAuthenticated {
    param([switch]$SkipCheck)
    if ($SkipCheck) { return }
    $null = & gh auth status 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host ("=" * 78) -ForegroundColor Red
        Write-Host "  $([char]0x2718) GitHub CLI is not authenticated." -ForegroundColor Red
        Write-Host "    Run 'gh auth login' to authenticate with GitHub." -ForegroundColor Yellow
        Write-Host ("=" * 78) -ForegroundColor Red
        Write-Host ""
        exit 1
    }
}

function Get-DistinctLabels {
    param([Parameter(Mandatory)][array]$Issues)
    $all = @()
    foreach ($issue in $Issues) {
        if (-not [string]::IsNullOrWhiteSpace($issue.Labels)) {
            $all += ($issue.Labels -split ',') | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }
        }
    }
    return ($all | Select-Object -Unique)
}

$KnownLabelColors = @{
    "bug"         = "d73a4a"
    "enhancement" = "a2eeef"
    "tech-debt"   = "fbca04"
    "security"    = "b60205"
    "priority-p1" = "e11d21"
    "priority-p2" = "eb6420"
    "priority-p3" = "fbca04"
    "priority-p4" = "cccccc"
}

function Get-LabelColor {
    param([Parameter(Mandatory)][string]$Name)
    if ($KnownLabelColors.ContainsKey($Name)) {
        return $KnownLabelColors[$Name]
    }
    $hash = [System.Security.Cryptography.MD5]::Create().ComputeHash(
        [System.Text.Encoding]::UTF8.GetBytes($Name)
    )
    return ([BitConverter]::ToString($hash) -replace '-', '').Substring(0, 6).ToLower()
}

function Add-MissingLabels {
    param(
        [Parameter(Mandatory)][string]$TargetRepo,
        [Parameter(Mandatory)][array]$LabelNames
    )
    foreach ($name in $LabelNames) {
        $color = Get-LabelColor -Name $name
        try {
            & gh label create $name --repo $TargetRepo --color $color --force 2>&1 | Out-Null
        } catch {
            # Ignore -- label may already exist or token may lack permission
        }
    }
}

function New-Issue {
    param(
        [Parameter(Mandatory)][string]$TargetRepo,
        [Parameter(Mandatory)][string]$Title,
        [Parameter(Mandatory)][string]$Body,
        [string]$Labels,
        [switch]$IsDryRun,
        [int]$SleepSeconds = 1,
        [int]$CurrentIndex = 1,
        [int]$TotalCount = 1
    )

    if ($IsDryRun) {
        Show-IssueCard -Index $CurrentIndex -Total $TotalCount -Title $Title -Labels $Labels `
            -Status "DRYRUN" -Body $Body -IsDryRun $true
        return [PSCustomObject]@{
            Status = "CREATED"
            Url    = ""
            Error  = ""
        }
    }

    $tmpFile = [System.IO.Path]::GetTempFileName()
    try {
        Set-Content -Path $tmpFile -Value $Body -Encoding UTF8
        $ghArgs = @("issue", "create", "--repo", $TargetRepo, "--title", $Title, "--body-file", $tmpFile)
        if (-not [string]::IsNullOrWhiteSpace($Labels)) {
            $cleanedLabels = ($Labels -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }) -join ','
            if ($cleanedLabels) {
                $ghArgs += @("--label", $cleanedLabels)
            }
        }
        $result = & gh @ghArgs
        if ($LASTEXITCODE -ne 0) {
            $errMsg = "gh exit code: $LASTEXITCODE"
            Show-IssueCard -Index $CurrentIndex -Total $TotalCount -Title $Title -Labels $Labels `
                -Status "FAILED" -ErrorMsg $errMsg
            return [PSCustomObject]@{
                Status = "FAILED"
                Url    = ""
                Error  = $errMsg
            }
        } else {
            $url = ($result -split "`n" | Where-Object { $_ -match '^https?://' } | Select-Object -First 1)
            if (-not $url) { $url = ($result -join ' ').Trim() }
            Show-IssueCard -Index $CurrentIndex -Total $TotalCount -Title $Title -Labels $Labels `
                -Status "CREATED" -Url $url
            return [PSCustomObject]@{
                Status = "CREATED"
                Url    = $url
                Error  = ""
            }
        }
    } catch {
        $errMsg = $_.Exception.Message
        Show-IssueCard -Index $CurrentIndex -Total $TotalCount -Title $Title -Labels $Labels `
            -Status "FAILED" -ErrorMsg $errMsg
        return [PSCustomObject]@{
            Status = "FAILED"
            Url    = ""
            Error  = $errMsg
        }
    } finally {
        Remove-Item $tmpFile -ErrorAction SilentlyContinue
    }

    if ($SleepSeconds -gt 0) {
        Start-Sleep -Seconds $SleepSeconds
    }
}

# ---------------------------------------------------------------------------
# Main Execution
# ---------------------------------------------------------------------------

$StopWatch = [System.Diagnostics.Stopwatch]::StartNew()

Test-GhInstalled
Test-GhAuthenticated -SkipCheck:$DryRun

$parsed = Read-IssueMarkdown -Path $MarkdownPath

$targetRepo = if ($Repo) { $Repo } else { $parsed.FrontMatter['repo'] }
if ([string]::IsNullOrWhiteSpace($targetRepo)) {
    Write-Error "No target repo resolved. Set 'repo: owner/name' in the markdown front matter or pass -Repo."
    exit 1
}

# Calculate label breakdown stats
$labelStats = @{}
foreach ($issue in $parsed.Issues) {
    if (-not [string]::IsNullOrWhiteSpace($issue.Labels)) {
        foreach ($lbl in ($issue.Labels -split ',')) {
            $cleanLbl = $lbl.Trim()
            if ($cleanLbl) {
                if ($labelStats.ContainsKey($cleanLbl)) {
                    $labelStats[$cleanLbl]++
                } else {
                    $labelStats[$cleanLbl] = 1
                }
            }
        }
    }
}

Show-HeaderBanner -TargetRepo $targetRepo -Path $MarkdownPath -IsDryRun $DryRun `
    -IssueCount $parsed.Issues.Count -LabelStats $labelStats

Write-Host "  $([char]0x2192) Fetching existing issues from '$targetRepo' to prevent duplicates..." -ForegroundColor Cyan
$existingIssues = @()
try {
    $json = & gh issue list --repo $targetRepo --state all --limit 1000 --json title
    if ($LASTEXITCODE -eq 0 -and $json) {
        $parsedJson = $json | ConvertFrom-Json
        if ($parsedJson) {
            $existingIssues = $parsedJson.title
        }
    }
} catch {
    # Ignore errors if remote query fails
}

$existingTitles = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($title in $existingIssues) {
    $null = $existingTitles.Add($title)
}
Write-Host "  $([char]0x2714) Checked remote repository: $($existingTitles.Count) existing issue(s) indexed" -ForegroundColor Green

if (-not $DryRun) {
    $labelNames = Get-DistinctLabels -Issues $parsed.Issues
    if ($labelNames.Count -gt 0) {
        Write-Host "  $([char]0x2192) Synchronizing $($labelNames.Count) label(s) with remote repository..." -ForegroundColor Cyan
        Add-MissingLabels -TargetRepo $targetRepo -LabelNames $labelNames
        Write-Host "  $([char]0x2714) Labels synchronized" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host ("=" * 78) -ForegroundColor Cyan
Write-Host "                         PROCESSING ISSUE BATCH" -ForegroundColor Cyan
Write-Host ("=" * 78) -ForegroundColor Cyan
Write-Host ""

$createdCount = 0
$skippedCount = 0
$failedCount  = 0
$totalCount   = $parsed.Issues.Count
$currentIndex = 0

foreach ($issue in $parsed.Issues) {
    $currentIndex++

    if ($existingTitles.Contains($issue.Title)) {
        Show-IssueCard -Index $currentIndex -Total $totalCount -Title $issue.Title `
            -Labels $issue.Labels -Status "SKIPPED" -IsDryRun $DryRun
        $skippedCount++
        continue
    }

    $res = New-Issue -TargetRepo $targetRepo -Title $issue.Title -Body $issue.Body -Labels $issue.Labels `
        -IsDryRun:$DryRun -SleepSeconds $DelaySeconds -CurrentIndex $currentIndex -TotalCount $totalCount

    if ($res.Status -eq "FAILED") {
        $failedCount++
    } else {
        $createdCount++
    }
}

$StopWatch.Stop()

Show-SummaryCard -TargetRepo $targetRepo -IsDryRun $DryRun -Total $totalCount `
    -Created $createdCount -Skipped $skippedCount -Failed $failedCount `
    -LabelStats $labelStats -Elapsed $StopWatch.Elapsed
