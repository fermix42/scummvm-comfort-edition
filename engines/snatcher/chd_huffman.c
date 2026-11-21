/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Keep the pinned dependency unchanged and its warning exception local in every
 * CE build, including detection-only builds that do not enable this engine. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4456)
#endif
#include "libchdr/src/libchdr_huffman.c"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
