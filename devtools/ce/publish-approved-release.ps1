# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Candidate,
    [Parameter(Mandatory)][string]$SourceCommit,
    [Parameter(Mandatory)][string]$Tag,
    [Parameter(Mandatory)][string]$ZipSha256,
    [Parameter(Mandatory)][string]$ExeSha256,
    [Parameter(Mandatory)][string]$Notes,
    [string]$Repository = 'fermix42/scummvm-comfort-edition',
    [switch]$Publish
)
$ErrorActionPreference = 'Stop'
$ghApplicationPath = (Get-Command gh.exe -CommandType Application -ErrorAction Stop).Source
function Invoke-CeGitHubCli {
    $result = & $ghApplicationPath @args
    if ($LASTEXITCODE) { throw "gh failed: $($args -join ' ')" }
    return $result
}
function Assert-Hash($Path, $Expected) {
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Expected.ToLowerInvariant()) {
        throw "SHA256 mismatch: $Path"
    }
}
if ($Repository -ne 'fermix42/scummvm-comfort-edition') { throw 'Unexpected repository' }
if ($Tag -notmatch '^v\d+\.\d+\.\d+$' -or $SourceCommit -notmatch '^[0-9a-f]{40}$') { throw 'Invalid tag/source identity' }
if ($ZipSha256 -notmatch '^[0-9a-f]{64}$' -or $ExeSha256 -notmatch '^[0-9a-f]{64}$') { throw 'Invalid approved hashes' }
$Candidate = (Resolve-Path -LiteralPath $Candidate).Path
$Notes = (Resolve-Path -LiteralPath $Notes).Path
$metadata = Get-Content -LiteralPath (Join-Path $Candidate 'release-artifacts.json') -Raw | ConvertFrom-Json
if ($metadata.source_commit -ne $SourceCommit -or $metadata.proposed_tag -ne $Tag -or
    $metadata.zip_sha256 -ne $ZipSha256 -or $metadata.exe_sha256 -ne $ExeSha256) { throw 'Candidate identity differs from approval' }
$zipName = "scummvm-comfort-edition-$($Tag.Substring(1))-windows-x64.zip"
if ($metadata.zip -ne $zipName) { throw 'Unexpected ZIP name' }
$zip = Join-Path $Candidate $zipName
$checksum = "$zip.sha256"
Assert-Hash $zip $ZipSha256
if ((Get-Content -LiteralPath $checksum -Raw).Trim() -ne "$ZipSha256  $zipName") { throw 'Checksum sidecar differs from approval' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $executables = @($archive.Entries | Where-Object { $_.FullName -match '/scummvm\.exe$' })
    if ($executables.Count -ne 1) { throw 'Expected one executable' }
    $stream = $executables[0].Open()
    $hash = [Security.Cryptography.SHA256]::Create()
    try { $actual = [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $stream.Dispose(); $hash.Dispose() }
    if ($actual -ne $ExeSha256) { throw 'Archived executable differs from approval' }
} finally { $archive.Dispose() }
$body = [IO.File]::ReadAllText($Notes)
foreach ($required in @($Tag,$SourceCommit,$ZipSha256,$ExeSha256)) {
    if (-not $body.Contains($required)) { throw "Notes omit identity: $required" }
}
Write-Output 'Local candidate identity verified; archive unchanged.'
if (-not $Publish) { Write-Output 'Check only: no network mutations.'; return }

# The old tag contains the old workflow. A branch-only edit cannot neutralize it.
$legacy = Invoke-CeGitHubCli api "repos/$Repository/actions/workflows/374228258" | ConvertFrom-Json
if ($legacy.path -ne '.github/workflows/windows-release.yml' -or $legacy.state -ne 'disabled_manually') {
    throw 'Permanently retire legacy workflow 374228258 before pushing any release tag; see PUBLICATION.md'
}
$ref = Invoke-CeGitHubCli api "repos/$Repository/git/ref/tags/$Tag" | ConvertFrom-Json
if ($ref.object.type -ne 'tag') { throw 'Release requires an existing annotated tag' }
$tagObject = Invoke-CeGitHubCli api "repos/$Repository/git/tags/$($ref.object.sha)" | ConvertFrom-Json
if ($tagObject.object.type -ne 'commit' -or $tagObject.object.sha -ne $SourceCommit) { throw 'Remote tag targets a different source' }

# List rather than treating any failed GET as an absent release.
$releases = Invoke-CeGitHubCli api --paginate "repos/$Repository/releases?per_page=100" --jq '.[] | {id,tag_name} | @json'
$matching = @($releases | ForEach-Object { $_ | ConvertFrom-Json } | Where-Object tag_name -eq $Tag)
if ($matching.Count -gt 1) { throw 'Duplicate release identity' }
if ($matching.Count -eq 0) {
    Invoke-CeGitHubCli release create $Tag --repo $Repository --draft --verify-tag --title "ScummVM Comfort Edition $Tag" --notes-file $Notes | Out-Null
}
# Drafts must be queried by ID/view, not the public tag endpoint.
$release = Invoke-CeGitHubCli release view $Tag --repo $Repository --json 'id,tagName,isDraft,body,assets' | ConvertFrom-Json
if ($release.tagName -ne $Tag -or $release.body.Replace("`r`n","`n").TrimEnd() -ne $body.Replace("`r`n","`n").TrimEnd()) {
    throw 'Existing release notes/tag differ; refusing to edit'
}
$files = @($zip,$checksum,$Notes)
$names = @($files | ForEach-Object { Split-Path -Leaf $_ })
if (@($names | Select-Object -Unique).Count -ne 3) { throw 'Asset names collide' }
if (@($release.assets | Where-Object { $_.name -notin $names }).Count) { throw 'Unexpected existing assets' }
$temp = Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    foreach ($file in $files) {
        $name = Split-Path -Leaf $file
        if (@($release.assets | Where-Object name -eq $name).Count -eq 0) {
            if (-not $release.isDraft) { throw 'Published release is incomplete; refusing mutation' }
            Invoke-CeGitHubCli release upload $Tag $file --repo $Repository | Out-Null
        }
        Invoke-CeGitHubCli release download $Tag --repo $Repository --pattern $name --dir $temp | Out-Null
        Assert-Hash (Join-Path $temp $name) (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    }
    # Draft assets, notes and source have now been verified; only now publish.
    if ($release.isDraft) { Invoke-CeGitHubCli release edit $Tag --repo $Repository --draft=false | Out-Null }
    $public = Invoke-CeGitHubCli api "repos/$Repository/releases/tags/$Tag" | ConvertFrom-Json
    if ($public.draft -or $public.tag_name -ne $Tag -or $public.body.Replace("`r`n","`n").TrimEnd() -ne $body.Replace("`r`n","`n").TrimEnd()) { throw 'Public release identity mismatch' }
    $publicZip = @($public.assets | Where-Object name -eq $zipName)
    if ($publicZip.Count -ne 1) { throw 'Missing public ZIP' }
    Invoke-WebRequest -Uri $publicZip[0].browser_download_url -OutFile (Join-Path $temp 'published.zip')
    Assert-Hash (Join-Path $temp 'published.zip') $ZipSha256
    Write-Output "Published and download-verified: $($public.html_url)"
} finally {
    # Delete only the single owned GUID directory created above.
    if ((Split-Path -Parent $temp) -eq [IO.Path]::GetTempPath().TrimEnd('\')) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
