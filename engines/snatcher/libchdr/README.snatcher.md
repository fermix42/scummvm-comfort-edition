# Pinned CHD container dependency

Source: rtissera/libchdr, tag v0.3.0, exact commit
`93d8c239ff0d4e8d7722985992649fce12d2463b`.
This is an independently licensed container/codec dependency, not Snatcher port
source, game code, or a Sega CD execution/hardware implementation.

Only required C decoder sources/headers and their embedded notices are retained.
No assembly, tests, benchmark, shared-library/export, install or executable code
is built. Upstream source bytes are unchanged. `SOURCE-MANIFEST.json` records each
selected upstream source hash. Local CMake and this notice inventory are additions.
The stripped upstream Zstandard package omitted LICENSE; its exact BSD text was
obtained from https://raw.githubusercontent.com/facebook/zstd/v1.5.7/LICENSE.

Selected licenses are permissive and compatible with the surrounding GPLv3-or-
later engine: libchdr BSD-3-Clause (retain root and Aaron Giles notices), LZMA SDK
public domain, miniz embedded MIT/Unlicense portions, Zstandard BSD-3-Clause
(choose BSD rather than GPLv2), and dr_flac MIT No Attribution (choose MIT-0
rather than the Unlicense alternative). Miniz's directory label3.1.1 and header
banner3.1.0 differ; the pinned libchdr commit and exact source hashes identify
the selected bytes. No unsupported claim of a matching separate miniz tag is made.

NOTICES.txt collects the selected grants for eventual binary notice inclusion;
the exact original embedded notices remain in every source. No publication or
binary redistribution is performed by this task. A future packaging workflow
must include those notices in its materials.

The local CMake static target is `snatcher-chdr`; its configuration disables miniz
archive/compression/stdio/time APIs, retains CHD raw sectors, subcode and block
CRC checks, and uses pure-C LZMA. C++ language requirements are unchanged.
Only Windows x64 MSVC was built/tested here; other-platform portability is NOT RUN.
The adapter restricts supported inputs to bounded parentless CHDv5 CD images with
2448-byte units and the verified stored-VAUDIO-pregap track layout. No whole-image
precache is used. Calls are single-owner-thread; no concurrent handle access is
promised. CE supplies the compressed stream through bounded64-bit callbacks.

The exposed projected2352-byte stream removes per-track four-unit padding and
converts AUDIO sample pairs from CHD big endian to the verified extracted-BIN
little endian. Raw readUnit preserves all2448 original bytes. Stored pregaps stay
in the projected disc stream; resource-level CDDA requests exclude them explicitly.
