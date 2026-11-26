# Original filesystem mapping and callback policy

37 original-only cases qualify twice. Selected original CRT/filesystem boot and
synchronous-I/O fallback are explicit, as in the disk-cache report. Each case
runs in a fresh copied original process before normal application entry. Only
the copied executable exists in the owned root before its receipt is written.
There are no file mutation, UI reachability or native-operation retry claims.

18 mapping cases call original 0x454991 and retain its low16-bit status. They
cover raw2,3,5,19,21,32,33,38,39,112,80,183,12345,34,107,108,15,16. Six adapter
cases call 0x4548ef with no callback, a controlled callback returning0 or1,
policy0/1 and negative suppression. Twelve category cases call original error
handler0x454102 for missing/path/access/readonly/not-ready/sharing/short-I/O/disk
full/existing-file/unknown controls. The application-suppression case calls the
actual installed callback0x42db7e with application flag0x49ac38 set and observes
its immediate zero return without claiming the later dialog branch.

| Policy/control | Result |
| --- | --- |
| No callback, policy1 | Stop0 |
| No callback, policy0 | Continue1 |
| Callback returns0, policy1 | Stop0; exactly one call |
| Callback returns1, policy1 | Continue1; exactly one call |
| Callback returns0, policy0 | Continue1; exactly one call |
| Negative suppression, even with callback1/policy0 | Stop0; callback never called |

Callback fixtures are explicitly observational test inputs. They capture actual
category and passed policy and return a fixed decision; they do not replace the
original mapping, category or policy implementations. They are stdcall, matching
the original callback ABI. Policy and suppression writes execute after filesystem
initialization; writing them only into remote memory before running the boot
stub would be overwritten by original initialization and is not qualified setup.

Missing file2, path3, access5, sharing32, short read38, disk full112 and existing
file183 do not call the callback in these category tests. Error19 passes category4,
21/34 category3,108 category2, and unknown12345 category1. A callback decision1
returns continuation without installing a final mapped failure status; the stop
paths retain the exact mapped status. No retry attempt loop is inferred merely
from a callback decision.

## Static mapping correction

The prior static report incorrectly assigned raw Win32 error21 to fallback
0x2846/category1. Controlled execution returns **0x284c/category3**. Reinspection
shows the decrement chain0x4549c4..0x4549d4 leaves zero flags for21, jumps to
0x4549fa, then takes its conditional jump to0x454a36. The earlier branch reading
missed those flags across the unconditional jump. Raw34 and107 independently
exercise the same mapped result. This corrects research documentation, not an
engine implementation. Region hashes remain unchanged.

Reproduce with probe_mohawk_v5_os_errors.py --original <pinned CarmenTQ.exe>
--root <new owned directory>. Native result, exact callback counts/category/policy,
original initialization, VM errors and native status must all qualify. The JSON
companion retains executable/source identities and both complete runs.

These37 original-only cases do not increase paired totals. Actual native-operation
repeat/cancel, short-I/O wrapper outcomes and real original Retry/Cancel dialog
reachability/screenshots remain required. Matching CE behavior, lifecycle and
storage graph gaps and final aggregate replay remain in the Step3 audit.
Stop before Step4; keep PR295 a draft. No engine/build/installation change.
