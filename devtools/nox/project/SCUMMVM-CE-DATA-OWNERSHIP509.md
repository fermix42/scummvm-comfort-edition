# Foundational item/map ownership review509

Status: VERIFIED for static known data-owner implementation review only.
Parent045a8ea52bc. No production change, build or executable run.

Current362 engine files match the accepted506b focused source hashes exactly.
The executable remains c322a8188b35fccead5d2dd99302bb75e229937f02b8d66c8d08dce6eb2d72eb.
This identity does not expand506b's focused acceptance.

| Known requirement | Existing complete native ownership | Qualification |
|---|---|---|
| inventory.01 item definitions and six-byte records | ItemCatalog reads original SRTN.INVENTORY and copies all32 definition bytes from the three source table bases1000/1CC0/2C00. Inventory owns all256 six-byte records; import/export preserves every byte including empty-slot suffixes. NewGame imports actual initial records1800..1DFF, saves all1536 bytes, and stages the whole core before load commit. Record mutations such as removal change only their specified fields; party reorder deliberately exchanges owner bits3/4 for every slot, including empty slots. | Known data model is IMPLEMENTED. Equipment/use/commerce behavior and ordinary cold/session are separate requirements; arbitrary corrupt/imported IDs are not expanded supported workflows. No new runtime credit. |
| world.01 map family decode and retained records | MapResource owns all seven source families, variable directory lengths,24-bit extents, compressed sector directories and reverse packets, local48x48 sectors, metadata and1792-byte state blocks. UM76/77 use original uncompressed layout. World retains each changed family/id once and travel selects its current owned records rather than reloading immutable defaults. NWLD saves all records and stages resource/id/extent validation before commit. NewGame seeds Surface0 from GAMEN first1536 bytes separately from Shipwreck7; generic/legacy imports are not silently reseeded. | Known map data/record ownership is IMPLEMENTED. Movement/window/entry scheduling, historical import domain and ordinary cold/session remain their own qualifications. Map97 native changed walls persist directly; no stale field77..79 reload reset is added. |

The typed notice reader also resolves all83 ordinary initial state55 notices
against their actual metadata pointer table, monotone offsets, index and zero
terminator. The single Keep69 state55 record belongs to speech505 instead.
This is a read-only source extent check, not83 gameplay activations. Surface0
uses the actual newgame seed as507 established. No original resource changes.

These two implementation rows become IMPLEMENTED for their enumerated data
requirements. Their runtime verification statuses remain unchanged. No full
inventory/world subsystem or campaign completion is claimed. Legitimate reading
of original data remains an intentional dependency; none of these owners runs
original instructions. Source-format offsets do not become CPU ABI state.
