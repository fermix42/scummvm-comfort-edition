// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual CRT format used by the vendored ImGui memory editor.
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <limits>

int main() {
	static_assert(sizeof(size_t) == 8, "This audit targets the Release x64 CRT");
	size_t address = 0;
	assert(sscanf("FFFFFFFFFFFFFFFF", "%IX", &address) == 1);
	assert(address == std::numeric_limits<size_t>::max());
	assert(sscanf("100000000", "%IX", &address) == 1);
	assert(address == size_t(0x100000000ULL));
	assert(sscanf("0", "%IX", &address) == 1 && address == 0);
	assert(sscanf("invalid", "%IX", &address) == 0);
	puts("PASS: native-width hex format preserves 64-bit values and rejects invalid input");
}
