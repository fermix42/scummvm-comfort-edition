# Nox M1 online verification

Approved registration: game `nox-archaist`, variant `apple2-v119`, source engine
and game ID `nox`, display `Nox Archaist`, variant title
`Apple II — Base Game v119`. These match the existing website's kebab-case API
identifiers and separate source metadata fields. No achievement definitions exist.

Website registration is prepared in `database/seeders/NoxArchaistSeeder.php` in
`D:\Codex Projects\CE Achievements`, also called by DatabaseSeeder. It has not been
deployed. Do not apply these commands to production or a personal account database.

## Verified local API contract

From that website checkout, with PHP 8.5:

```powershell
$env:APP_ENV='testing'
$env:DB_CONNECTION='sqlite'
$env:DB_DATABASE=':memory:'
$env:DB_URL=''
& 'C:\Users\aaron\.config\herd-lite\bin\php.exe' vendor/bin/pest tests/Feature/NoxPlaythroughApiTest.php tests/Feature/GamePagesTest.php tests/Feature/PlaythroughApiTest.php --compact
```

27 tests and 195 assertions passed on October 15, 2026. Dedicated non-admin
factory users and random tokens exist only inside the isolated tests. The tests
cover zero-award registration, supported-game catalog/detail routes, session
creation/replay, same-slot save metadata/reload metadata, and account separation.
They do not exercise CE's HTTP transport or restore serialized game state.
`/games` remains the existing achievement-only listing; the supported-game
catalog is `/scummvm-ce/catalog`.

## Remaining end-to-end procedure

1. Provision an isolated local/staging website database and server. Confirm its
   connection points to separate test data before running migrations or seeders.
   Run `php artisan migrate` and
   `php artisan db:seed --class=NoxArchaistSeeder` there. Verify the supported
   catalog and `/games/nox-archaist`; confirm the game has zero achievement rows.
2. Register a dedicated non-admin user on that environment. Mint its own CE Link
   credential at `/settings/ce-link`. Keep credentials in a private local CE
   configuration only; never copy tokens/passwords into source, argv, reports,
   screenshots, Git, or this document. Revoke the token when testing is finished.
3. Use a separate CE config/save directory. Set
   `ce_achievements_api_base_url` to the test server's `/api/v1` URL. Configure the
   dedicated token through existing CE Link settings and test the link. Do not
   reuse the personal desktop CE configuration or change the production endpoint.
4. Launch detected base v119 through the CE launcher; create Play Session A and
   synchronize. Verify the server associates the approved game and variant.
   Repeat with Session B; retain distinct client and server session identities.
5. Once engine save integration is complete, save different playable positions
   into logical slot 1 in A and B. Cold-start CE, load each, and verify its own
   position and original session identity. Synchronize the create/reload metadata;
   the website must retain two distinct session/save rows. Retry queued requests
   and verify replay updates the existing records instead of duplicating them.
6. Confirm no achievement events/definitions were fabricated, no cross-account
   access is accepted, and original HDV hashes remain unchanged. Record executable
   hash, redacted endpoint/account role, HTTP outcomes, session/save counts, and
   gameplay evidence. Record failures honestly; do not publish credentials.

Current transport status: **VERIFIED** on the executable and evidence below. User supplied `http://hermesbox:8123/` and
authorized a new dedicated non-admin account. Host/environment/URL verified;
approved game/variant registered using existing application models, without
deploying server code or migrating its database. Dedicated credentials are in
the private temporary directory recorded in the canonical progress document,
outside source and artifacts. HTTP Link and Nox detail returned 200.

The first actual CE run (artifact `ce-m1-hermesbox/20261015-115107`) exposed a
client request race: local session B received A's server identity. Independent
database audit failed even though HTTP requests and local cold saves passed.
Preserve this evidence. Build 44465 passed and its correction passed actual CE
sync/cold saves plus independent server audit at `ce-m1-hermesbox/20261015-120650`.
The stronger run `ce-m1-hermesbox/20261015-120800` also resent the original
immutable events after resetting only local acknowledgement state; event IDs,
payloads and signatures stayed unchanged, all events synchronized, and the
independent database audit still found exactly two sessions and one save each.
Both audits verify labels, hashes, reloaded metadata and zero achievements.
Executable SHA256: `6e390d3cd5c2a7955a7d286cb40f5bf9cec4a27256d2946ea8d31585cdddef1a`.
Repeat `test_gameplay.py --private-config <private ce-private.ini>` with the
normal executable/game/artifact arguments, then run `verify_hermesbox.py` with
the dedicated SSH identity, private directory and new `--ce-evidence` directory.
Both client and server checks must pass for future changed binaries as well.
Final M1 transport evidence: `ce-m1-final-hermesbox/20261015-124104`, executable
SHA256 `81a51beec3a1044812ba53a5c2166e12ceaa7d021b4f4563e6f521c89600fb44`.
Client sync/immutable replay and independent server audit both pass. The same
binary passed actual launcher, chooser, host lifecycle and killed-write recovery
in the separate final artifact sets linked from the current progress document.

After final verification, the temporary test credential was revoked through its
exact dedicated account/name/token hash. The non-admin test account and test
history remain; private password/configuration remain outside source/reports.
The retained CE config no longer authenticates. Future authorized live testing
must configure a fresh credential (or explicitly reprovision the same dedicated
credential using `provision_hermesbox.py --private` and its existing private
directory). Never activate it for production. No production deployment or
production account modification is authorized. M1 is VERIFIED; stop before M2.
