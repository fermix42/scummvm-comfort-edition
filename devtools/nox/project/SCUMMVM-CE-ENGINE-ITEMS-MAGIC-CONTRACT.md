# Item/service/magic source closure478

Status: IN PROGRESS. Defined before this batch's production changes.
Reuse Inventory, ItemCatalog, EquipmentActions/Ownership, SpellBook, WorldCast,
BattleCast and existing service owners/receipts. Do not rerun accepted service,
audio or campaign suites. The original book42 and cast-ui source proofs remain
scoped evidence; they are not fresh acceptance of a later executable.

## Demonstrated book shortcut mismatch

InventoryWindow's new book prompt says1-9,0 and maps1 to slot0,0 to slot9.
WorldCast and BattleCast both correctly map0..9 directly to slots0..9. Thus a
book readied with key1 casts using key0. Original inventory AC6D selection accepts
176+slot, and original cast9C38-9C42 acceptsB0..B9/AND0F. Both share direct0..9.
Existing prepare_native_book42_m4.py records original numeric shortcut inputs;
no new original oracle or broad equipment rewrite is needed.

1. Ready a book with digit0..9 into that exact global shortcut slot.
2. Show the same0..9 labels in ready and world/tactical Cast interfaces.
3. Retain existing eligibility, requirement bytes, cancellation, unready, slot
   replacement and inventory/party ownership semantics.
4. Save/readied book then prepare Cast at the same numeric shortcut. Preserve
   the other nine global slots, opaque party/inventory fields and game clock.
5. Verify actual InventoryWindow key handling and downstream CastRules lookup;
   not a helper-only test mirroring the changed arithmetic.

## Immutable producer closure to record

Misc definitions end at ID167 before merchant table4100, not arbitrary256
records into adjacent data. Reuse item-use-audit.json (64..167): document,
orb binding and coordinate actions have existing native owners. Book0..63
definitions with ## names are reserved, not evidence of obtainable spells.
WorldCast63 has authored cost0; its lack of mana deduction is not a defect.
Catalog counts and callback producer coverage remain distinct from native
workflow verification. Dynamic rewards/quest callbacks will be reconciled in
script-producer closure; do not promote them based on definition presence.
