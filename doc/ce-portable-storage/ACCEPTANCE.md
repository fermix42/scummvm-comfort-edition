# Permanent CE storage acceptance

The owner accepted the November 8, 2026 strict portable-storage implementation
as functionally complete. Implementation commit: `ea764d2d39006dc01d941dcb7f67b1ad4c30d4f7`.
The separate clean-storage acceptance/evidence history (`351e81d58fe`,
`386fa035cb4`) remains preserved; it is not merged into the Gooka integration.
The accepted implementation already belongs to this branch's ancestry.

All CE-managed normal persistent state belongs under the actual executable's
directory tree. No portable flag or marker is required. External managed CLI
paths are rejected, legacy external INI storage values resolve locally, and
unwritable local storage raises an error. Never introduce AppData, Documents or
profile fallback to bypass a path or permission error. Source game assets may
remain external read-only references; no existing profile data is auto-imported.

Use a short writable directory such as `D:\Games\ScummVM CE`. Existing
Windows/C-runtime limits can reject deeply nested paths and long save sidecars.
This limitation is retained; storage policy is not weakened to work around it.

Gooka must use the same save/filesystem interfaces. The Windows build and
release workflows retain their packaged-executable storage regression gates.
See `STRICT-STORAGE.md`, `STRICT-RESULTS.md` and the CE v0.1.4 readiness report
for scope, actual evidence, Windows/driver-resource distinctions and remaining
limits. Functional acceptance does not authorize a push, tag, merge or release.
