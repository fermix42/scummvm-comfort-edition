# ThinkQuick: operator failures, packed values and OS retry behavior

November 26, 2026. Final static recovery checkpoint for step 2 of CE #1.
All addresses refer to the pinned original CarmenTQ.exe, SHA-256
219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966.
The evidence extractor hashes these regions and decodes their branch tables.
At the step-2 checkpoint no original-runtime execution, CE implementation or
compatibility acceptance was claimed. See the step-3 erratum below for subsequent
function observations. Earlier open questions in these three areas are superseded here.

## Operator and error contracts

The binary handler pops the right operand to local -0x30, then the left to
-0x24 (`0x404179`). Its secondary opcode table is now extracted in the manifest.
The following are observed paths, including their missing guards:

| Token | Operation and result | Failure behavior |
| --- | --- | --- |
| 0x14, 0x11, 0x0e | Addition, subtraction, multiplication through double conversion; push real | No separate numeric-type or overflow VM error in these handlers; conversion fallback applies |
| 0x21 | Real division | Convert divisor to double; x87 compare with 0; equality-bit set raises VM error 4 before division |
| 0x32 | Signed integer division, pushed as real | Same double zero check, then truncate both operands through low-32-bit integer conversion and execute IDIV; no second zero/overflow check |
| 0x34 | Signed integer remainder, pushed as real | Convert both operands to int32 and execute IDIV, use EDX; no VM zero check here |
| 0x0b | String concatenation through each operand's string conversion | No arithmetic/type rejection; allocator/CRT failures are separate |

Thus integer division by 0.5 passes the initial zero test but its converted
integer divisor is zero. Remainder by zero and INT_MIN / -1 also reach processor
IDIV faults. These are unguarded processor paths, not VM error 4 or a guaranteed
zero result. No controlled crash outcome is asserted. Invalid numeric strings
convert to zero and can trigger the real-division zero check. The x87 equality
bit is also set for unordered comparisons; with masked invalid exceptions, an
unordered divisor takes that check's error branch. Do not replace this with an
assumption that every NaN behaves like an ordinary nonzero number.

Assignment and compound assignment (`0x40448a`) require an actual tag-15
reference; otherwise VM error 14 is raised before writing. The four numeric
compound paths use double conversion, including a zero check for division.
Concatenating assignment converts both to strings. Increment/decrement
(`0x404096`, `0x40f050`, `0x40f0b2`) also require a reference; they calculate
converted value +/-1 and choose int32 when the result equals its truncated
int32 reconstruction, otherwise real. Prefix pushes a copy after mutation;
postfix pushes a copy before mutation. No separate range guard precedes the
conversion. Negation uses the same int32-versus-real reconstruction rule.

Comparison helpers `0x405d70..0x405ef0` test x87 status bits, not a three-way
integer ordering API: equality tests 0x40, inequality its inverse; the other
four predicates test bit 1 or mask 0x41 and their inverses. The value comparison
routine computes right minus left for numeric pairs. NaN/unordered behavior
must follow those exact predicates and the floating-point mask state. Logical
operators convert to double and test zero; operands have already executed.
No type-error check is added by these operator paths.

Indexing first resolves a list; an assignable non-list receiver can be replaced
with a new list, while an unassignable receiver raises error 15. Numeric list
indices use signed-short conversion and grow to a requested positive index.
The element helper (`0x416a83`) returns null outside 1..count, which is then
passed onward for reference construction; this is not a universal bounds error
or a promise of a safe value for all consumers. Item property targeting raises
error 12 when no item resolves. These distinctions supplement the prior typed
VM-error catch and nontransactional unwind findings.

## Packed values and their consumers

| Tag | Observed payload/role | Conversion and lifetime |
| --- | --- | --- |
| 5 | Two signed shorts: point | Inline four-byte copy; string `%d,%d` |
| 6 | Four signed shorts: rectangle | Inline eight-byte copy; string `%d,%d,%d,%d` |
| 8 | Short Boolean representation | String TRUE when short is nonzero, otherwise FALSE; numeric conversion reads the signed-short payload |
| 11 | Four bytes: key value | Inline copy; key input events construct this tag; byte 2 is character, byte 0 virtual key, byte 1 modifier encoding, byte 3 repeat/scan-derived auxiliary value |

Tags 9 and 10 have no specialized string/geometry/key conversion in the recovered
tables and no owned cleanup. They are reserved/unhandled in these consumers;
no invented language role is required. Point and rectangle are not recognized as ordinary numeric tags by the numeric
predicate. Boolean numeric conversion follows the signed-short branch; the
earlier claim of a zero fallback was disproved by step-3 original-code probes. Geometry
conversion and numeric conversion are different interfaces.

Point conversion (`0x41052a`) accepts tag 5 directly, rectangle's first pair,
item position via its rectangle, references recursively, numeric values repeated
in both coordinates, or a string parsed with `%hd %1[,] %hd%n`. Rectangle
conversion (`0x4101a9`) accepts tag 6 directly, point repeated into both corners,
item rectangle at +0x1c, references, numeric values repeated four times, or
`%hd %1[,] %hd %1[,] %hd %1[,] %hd%n`. Parsing requires the exact conversion
count and no extra non-whitespace token; parse failure/unsupported source yields
zero geometry. Short overflow parsing remains a CRT-dependent runtime case,
not an interpreter range error. Neither conversion normalizes corner order.

Key conversion (`0x4100a2`) starts with four zero bytes. Key values copy all four;
references recurse; integers/reals place the converted low byte in byte 0;
strings place their first byte in byte 2. Unsupported sources leave zero.
Numeric conversion of tag 11 uses byte 2 when nonzero, otherwise byte 0;
**toInt does not share that special key path** and defaults to zero for tag 11.
String conversion uses `%c` when byte 2 is nonzero, otherwise rearranges packed
bytes through `0x40df30` and formats `%08lX`. It is not an item/group target.

Key comparison (`0x40f7d4`) compares right.character - left.character if both
characters are printable ASCII 0x20..0x7e, ignoring the other bytes. Otherwise
it returns zero when virtual-key and modifier bytes both match, and **one for
either mismatch**, ignoring character/auxiliary bytes. Consequently this fallback
is not an antisymmetric ordering. Generic equality and relational predicates
inherit this behavior when both operands are tag 11.

Windows key construction (`0x44a6c1`) calls keyboard-state/translation APIs and
fills those bytes. Page key delivery (`0x420144`, `0x4202eb`) pushes a tag-11
value into the event argument frame and dispatches events 0x2d/0x2e. Trigger
matching (`0x4203ae`) accepts equal nonzero character bytes, or equal nonzero
virtual keys with equal modifiers; auxiliary byte is ignored. It differs from
value comparison. General key consumers (`0x40a4ae`, `0x40a580`) convert and
return key values; the latter also invokes page key dispatch. None of these
key paths retain an item/native pointer or require object cleanup.

## Interactive filesystem failures

Filesystem operations pass through `0x456488`. The operation virtual method
performs CreateFileA/read/write/etc, stores raw GetLastError at operation +0xc,
and returns via the operation runner. If that raw error is nonzero,
`0x454102` maps it with `0x454991` and decides whether to invoke the filesystem
callback. A nonzero callback result repeats the **whole native operation**;
zero ends retry and preserves the mapped error. This is distinct from script
return cells, and is not transactional or an unconditional retry policy.

Representative exact mappings (raw Win32 number -> internal status):
2 -> 0x2845; 3 -> 0x2847; 5 -> 0x283c; 19 -> 0x284d;
21 -> default 0x2846; 32/33 -> 0x2848; 38 -> 0x283f;
39/112 -> 0x2849; 80/183 -> 0x283e. Unknown errors use 0x2846.
`0x454102` offers callback categories only for internal 0x2846 (category 1),
0x284a (2), 0x284c (3), and 0x284d (4). Other errors return without that dialog.
In particular ordinary missing file/path and access-denied errors do not
universally show a retry dialog. Successful short read/write is separately
rejected with 0x283f by the exact-byte-count wrappers, after native success;
it does not enter the failed-native-operation retry loop.

The callback setter `0x453ef3` returns the prior callback. Application startup
installs `0x42db7e` at `0x4298f5`; filesystem initialization defaults policy
word +0x49b0be to 1. The callback adapter `0x4548ef`:

- returns zero immediately when suppression counter 0x49b0bc is negative;
- passes policy in callback data +4, and accepts a nonzero callback result;
- also permits continuation for callback zero when policy is zero;
- with no callback, returns true only when policy is zero.

The installed application callback immediately returns zero when application
flag 0x49ac38 is nonzero. Otherwise category 1 uses dialog resource 0x3f4 and
category 3 uses 0x3f3, with flags 0x8008 | Boolean(policy). Both return true only
for internal dialog result 8. Other categories show resource 0x3f5 with 0x8004
and return false. Dialog helper `0x451550` calls MessageBoxA and maps native
result 4 (Retry) to internal 8; native 2 (Cancel) maps to internal 1. With default
policy 1 the first two therefore present Retry/Cancel behavior, and only Retry
repeats the operation. With policy zero the adapter itself can permit repeated
attempts even when the callback returns zero. No automatic retry limit exists
in this loop.

A scoped helper temporarily sets callback null (`0x4320f3`) and restores the
saved pointer (`0x4321bb`); cleanup suppresses callbacks using the negative
counter. Missing media/volume recovery also invokes the callback adapter and
can repeat volume discovery (`0x4560d0..0x4561b6`). These contexts explain why
one observed missing-file result cannot establish a global interactive policy.
Modal dialog setup releases/reacquires an application lock and calls window
hooks around MessageBoxA; nested UI activity must be tested in step 3.

Profile flush uses this same open/write machinery. It returns helper status,
while datafile dispatch often returns a separate VM result. Read-only/missing
parent failures must not be presented as a successful persistent save just
because a script result defaults to zero. Original directory choices remain
historical evidence; CE portable-storage ownership is unchanged.

## Step-2 closure and step-3 acceptance boundary

The three remaining static questions now have a caller-to-consumer trace and
pinned branch/format evidence. Together with the earlier VM, lifecycle and
loader/storage supplements, this completes the planned **structural/static
recovery baseline**. It does not assert a complete public v5 specification,
qualification of every possible command, successful original runs or gameplay.
An absent guard is a recovered contract boundary, not a missing safe result to
invent. There is no further implementation in this checkpoint.

Step 3 must establish isolated original runs and compare: real/integer zero
and fractional divisors, remainder and signed overflow, wrong reference/index
operands, NaN/overflow mask behavior, point/rectangle parsing and conversions,
key equality versus trigger matching/modifiers, default and suppressed callbacks,
Retry/Cancel and repeated OS errors, short I/O and cold-restart persistence.
Record observable outcomes and exact environment identities before deciding CE
handling for unsafe original paths. Implementation remains gated on that work.

## Step-3 erratum

The first original-code batch measured tag-8 TRUE as 1.0, contradicting this
document's earlier zero-fallback claim. Reinspection confirms toReal dispatches
tag 8 to 0x40fef4 and toInt to 0x41003c: both read its signed-short payload.
The earlier failing receipt is retained; the corrected contract is tested with
zero, negative, noncanonical 2 and signed-short boundary controls. This is a
research correction, not a CE engine change. See THINKQUICK-COMPARISON.md.
