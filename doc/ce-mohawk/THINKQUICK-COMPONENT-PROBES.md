# Original native component dispatch and CE constructor boundaries

49 original-only cases qualify twice against the pinned CarmenTQ.exe. Each fresh
process stops before application startup and runs the selected original CRT,
filesystem and synchronous-I/O initialization from mohawk_v5_native_io.py. The
original class constructor and numeric dispatcher execute with engine context
zero and an explicit component ID of 17. These cases exercise methods that do
not need a running page or engine context. They do not replace native methods.

Every family (aaDatabase, aaDirectory, aaQSelector, aaUserHistory, aaStringLib,
datafile, encryptor, font, clock) is checked against its recovered constructor
vtable. Common methods -3/-4 return initial refcount zero and ID 17. Unknown
positive method 999 returns zero except clock, which returns -1. Short/integer
result tags are preserved in receipts rather than coerced into one type claim.

Additional representative results:

| Family | Native observations |
| --- | --- |
| aaDatabase | SetPaths copies two owned directory strings, returns short zero |
| aaDirectory | SetPath accepts the owned validation directory, returns short zero |
| aaQSelector | setNumLocations(4) returns short 4 |
| aaUserHistory | setLevel(3), getLevel() both return short 3 |
| aaStringLib | upper/lower of aBc 19! yield ABC 19!/abc 19!; missing argument returns integer zero |
| datafile | close on unopened object returns short zero; open with missing argument returns short -1 |
| encryptor | default key zero; set/get 123; encrypt 123456 with key 123 then decrypt ciphertext returns 123456; ciphertext differs from input |
| font | default get returns integer zero; missing set argument returns integer zero |
| clock | seconds, day, month, year, weekday, year-day, daylight field values are checked against recovered calendar ranges |

The original list constructor, append/copy, value constructors and dispatchers
build arguments and returns. Clock observations intentionally remain variable;
matching a range is not a fixed-time or timezone equivalence claim. Native
objects are process-owned and reclaimed when the debugger terminates each run;
this suite does not claim component destructor/shutdown qualification. Owned-path
setters create no additional files: only the copied executable exists before
the harness writes its receipt. File/path/storage side effects remain a separate
gate. No database index, history save or datafile persistence is inferred here.

Nine separate CE constructor probes use the unchanged baseline executable and
native CLI's owned roots. Numeric original type IDs are used (not unsupported
name-to-integer coercion). Datafile 1001 and font 1004 construct successfully;
clock 1003, aaDatabase 1005, aaDirectory 1006, aaQSelector 1008, aaStringLib 1010,
aaUserHistory 1011 and encryptor 1012 terminate with the exact unknown-LBX-type
diagnostic for that ID. The generated arithmetic predicate only checks that
construction completed; it does not test native component method equivalence.
These CE boundary cases and the 49 direct original calls are not byte-identical
paired language probes and do not increase the cumulative paired totals.

Reproduce original cases with probe_mohawk_v5_components.py --original <pinned
EXE> --root <fresh owned directory>. Reproduce CE constructor observations with
probe_mohawk_v5_component_boundary.py --ce <baseline EXE> --extras <engine-data>
--game-data <owned data containing Outline.txt> --root <fresh owned directory>.
Every original guard must pass; every CE outcome must match its specific expected
constructor boundary. Full receipts, source/executable hashes, fixture hashes,
result cells, diagnostics and repeat observations are in the JSON companion.

Step 3 remains active. Component construction/basic dispatch is now observed;
representative persistent storage, database/selector graph behavior, lifecycle,
packed/operators, OS-error and aggregate reproduction work remain. Stop before
Step 4 and keep PR #295 a draft. No engine behavior or installation was changed.
