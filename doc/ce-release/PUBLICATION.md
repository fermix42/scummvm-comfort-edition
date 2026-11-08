# Publishing an accepted CE candidate

Preparation, acceptance and publication are separate operations. Build and seal
the candidate with the existing tooling, then obtain owner acceptance of the
source commit, ZIP SHA256 and archived executable SHA256. Publication never
compiles, repackages or repeats gameplay acceptance. Retain the approved ZIP,
its `.sha256`, `release-artifacts.json`, acceptance evidence and final notes.

## One-time retirement of the legacy publisher

The old `.github/workflows/windows-release.yml` runs on `push.tags: v*` and
independently builds and publishes. Its definition remains in historical source
commits, including the approved v0.1.4 source. Editing a newer branch alone does
not prevent that old definition from running on a tag push.

Before pushing **any release tag**, an authorized repository administrator must
permanently disable the legacy workflow ID **374228258** in
`fermix42/scummvm-comfort-edition`. Use GitHub Actions → Windows x64 release →
Disable workflow, or:

```powershell
gh api --method PUT repos/fermix42/scummvm-comfort-edition/actions/workflows/374228258/disable
gh api repos/fermix42/scummvm-comfort-edition/actions/workflows/374228258 --jq '{id,path,state}'
```

Require path `.github/workflows/windows-release.yml` and state
`disabled_manually`. **Never restore/re-enable this retired workflow.** This is
a permanent migration, not a temporary publication workaround. Do not use a
skip-CI annotation or token-event suppression as the safety mechanism. Before
disabling, inspect for any already-running legacy publisher; cancel it with
explicit authorization before proceeding. No retirement, cancellation or remote
mutation was performed by the local infrastructure-fix task.

The old workflow file remains as a harmless manual retirement notice. The
replacement `.github/workflows/windows-candidate.yml` has a distinct path,
manual `workflow_dispatch` only, read-only contents permission and artifact
upload for review; it has no release-publication step. Install these changes on
the development branch when separately authorized. GitHub normally exposes manual
dispatch after the workflow reaches the default branch; no merge is authorized
implicitly. Local candidate preparation remains available regardless.

## Authorized publication

1. Obtain explicit owner publication authorization. Confirm the repository,
   clean branch, source ancestry, candidate hashes and documentation-only changes.
2. Complete and verify the permanent legacy retirement above **before tag push**.
   Push the completed development branch normally; do not force-push.
3. Create an annotated version tag at the accepted source commit, not the later
   infrastructure/documentation HEAD. Push that tag normally. If it already
   exists, verify its peeled commit and annotation; never replace it. For v0.1.4
   the accepted target is `3a6a4412ed2d728b4e4f88b52f2a97d0b259ba1f`.
4. Prepare final publication notes from the approved draft outside the immutable
   ZIP. Replace candidate/authorization-pending wording; retain limitations,
   source and both hashes. Include tested behavior, official-install isolation,
   installation-directory preservation on upgrade and short writable paths.
5. With authenticated `gh` on PATH (existing GitHub CLI; repository contents write
   access required), invoke `devtools/ce/publish-approved-release.ps1`. Supply
   `-Candidate`, `-SourceCommit`, `-Tag`, `-ZipSha256`, `-ExeSha256`, and `-Notes`.
   Without `-Publish`, it performs local identity/integrity checks only. With
   `-Publish`, it verifies permanent retirement and the existing remote annotated
   tag, creates/reuses a draft, uploads only missing matching assets, downloads
   and compares all three assets, checks notes, then publishes and downloads the
   public ZIP to verify the approved checksum. GitHub supplies source archives
   automatically from the tag; do not upload substitute source archives.
6. Record the public URL, source/tag, branch HEAD, asset names/sizes, downloaded
   SHA256 and Actions state. No compilation or ZIP generation is involved.

The three assets are the unchanged ZIP, its original checksum sidecar and final
release-notes Markdown file. No overwriting flags, asset deletion, tag replacement
or automatic notes edits are used. On interruption, rerun with exactly the same
inputs: matching assets are downloaded and checked, absent draft assets are
uploaded, and a complete already-public release is verified without mutation.
Conflicting notes/assets/tags or an incomplete public release cause a hard stop.
Publication requires repository contents write access; legacy retirement requires
Actions workflow-management access. Never print authentication tokens.

## v0.1.4 identity

- ZIP `scummvm-comfort-edition-0.1.4-windows-x64.zip`:
  `9993dcc02e43dd9c4bb4231c933d4d0c4d9a574a6340704ccaf5649525af494d`.
- Archived executable:
  `9600932ff4da2e490b3ae62e904909da9075f45405bb26b73315c88dfa159850`.
- Accepted source `3a6a4412ed2d728b4e4f88b52f2a97d0b259ba1f`;
  readiness documentation `a935413668fd07b00885745e5e6b6430ce8c773c`.

Acceptance remains final. Gooka is experimental; full completion, intro/audio
and save-dialog navigation remain incompletely verified. Preserve the complete
writable installation directory when upgrading. Independent bit-identical
executable rebuilding remains unverified. Infrastructure corrections do not
change any of these acceptance boundaries or the approved artifact.

## Local infrastructure verification (November 8, 2026)

PowerShell parsing and the check-only publisher passed against the approved
candidate, including hashing its archived executable. An incorrect approved hash
was rejected before network use. Four release-mechanics tests passed, covering
fixture archive determinism/non-overwrite, CE version identity and static
publication/workflow guards. The first sandboxed run could not access its
temporary fixture directories; the same four tests passed with normal temporary
directory access. No compilation, gameplay/storage acceptance, candidate rebuild,
network publication or workflow-configuration mutation occurred. Draft resumption
and remote upload verification are implemented and inspected; live publication
remains for the separately authorized operation.

The release is prepared locally, but **not safe to tag yet**: permanent remote
retirement of workflow 374228258 remains an explicit prerequisite. This task did
not change GitHub configuration or push the local corrections.
