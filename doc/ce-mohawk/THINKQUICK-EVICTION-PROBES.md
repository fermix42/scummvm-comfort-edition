# Original disk-cache pressure and storage-lock probes

Four original-only cases execute the real generated-MHK cold-read pipeline,
original purge collector 0x457403(request=4096, force=0), and original cache purge
callback 0x45bba6 registered by 0x483dc8. This invokes the pressure collector
directly without exhausting host RAM. It does not emulate a failed allocation.
The current-thread shell and explicit original synchronous-I/O fallback are the
same adaptations documented in THINKQUICK-DISK-CACHE-PROBES.md. There is no
substitute allocator, purge, file-read, cache-result or language-result callback.

The fixture BCOD has 49 bytes. The original SDK discardability setter 0x483c5d
sets or clears bit 0x8000. We explicitly set policy here; these observations do
not claim every authored BCOD is discardable by default.

| Case | First purge freed / remaining bytes | Follow-up |
| --- | --- | --- |
| Retain count one, unlocked/discardable | 49 / 0 | Original load restores exact 49 bytes from disk |
| Retain count zero, unlocked/discardable | 49 / 0 | Original load restores exact bytes |
| Retain count one, locked/discardable | 0 / 49 | Unlock, purge -> 49 / 0; exact reload |
| Retain count one, unlocked/non-discardable | 0 / 49 | Exact bytes remain available |

Logical resource retention therefore does not itself pin discardable storage.
An active memory lock or non-discardability protects it in this bounded clean
fixture. Original size query 0x483cbc distinguishes purged storage (zero bytes)
from a surviving allocated handle; handle-validity query alone cannot do so.
Both released resource and closed archive return zero. Every archive retains its
original SHA-256. No dirty-resource writeback, concurrent purge or allocation
failure is claimed.

All four cases qualify twice with identical normalized results, exact executable,
source, archive and expression identities in THINKQUICK-EVICTION-OBSERVATIONS.json.
The seven prior resident-cache/destructor/cross-resource controls also pass under
the current transport. Native string reads are now explicit opt-in for destructor
value observations, avoiding accidental interpretation of numeric status words
as string pointers. No engine source changes or builds occurred.

Reproduce with `python devtools/ce/probe_mohawk_v5_eviction.py --original <pinned
CarmenTQ.exe> --root <new owned directory>`. Each case launches and terminates its
own original process before application startup. Receipts fail qualification if
initialization, purge count, size, payload, teardown, VM error or archive integrity
checks disagree. These four original-only cases have no matching CE eviction
observations and do not increase paired totals (25 matches, 10 mismatches,
27 unqualified across the separately recorded 62 paired cases).

Step 3 remains active. Component/storage, remaining VM/lifecycle and OS-error
coverage, applicable CE observations and aggregate published-source replay remain
required. Stop before Step 4; leave PR #295 a draft.
