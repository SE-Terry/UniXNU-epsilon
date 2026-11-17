// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <libfdt.h>
typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef uintptr_t UINTN;
#define STATIC static
#define VOID void
#define CONST const
#define FdtSubnodeOffset fdt_subnode_offset
#define FdtGetProp fdt_getprop
#include "EpsilonFdtProbe.h"

static void fixture(void *buf, int room, uint64_t start, uint64_t end, int width) {
    assert(fdt_create_empty_tree(buf, room) == 0);
    int chosen = fdt_add_subnode(buf, 0, "chosen");
    assert(chosen >= 0);
    if (width == 8) {
        assert(fdt_setprop_u64(buf, chosen, "linux,initrd-start", start) == 0);
        assert(fdt_setprop_u64(buf, chosen, "linux,initrd-end", end) == 0);
    } else if (width == 4) {
        assert(fdt_setprop_u32(buf, chosen, "linux,initrd-start", start) == 0);
        assert(fdt_setprop_u32(buf, chosen, "linux,initrd-end", end) == 0);
    } else if (width == 2) {
        assert(fdt_setprop_u32(buf, chosen, "linux,initrd-start", start) == 0);
        assert(fdt_setprop(buf, chosen, "linux,initrd-end", "x", 2) == 0);
    }
    assert(fdt_pack(buf) == 0);
}

int main(int argc, char **argv) {
    _Alignas(8) unsigned char buf[4096] = {0};
    const UINT64 base = (UINTN)buf;
    const UINT64 end = base + sizeof(buf);
    EPSILON_FDT_PROBE_RESULT result;
    fixture(buf, 512, base + 512, end, 8);
    result = EpsilonInspectFdt(base, base, end);
    assert(result.Status == EPSILON_FDT_BOUNDS_OK && result.InitrdStart == base + 512 && result.InitrdEnd == end);
    assert(result.TotalSize == fdt_totalsize(buf));
    assert(EpsilonInspectFdt(0, base, end).Status == EPSILON_FDT_ADDRESS);
    assert(EpsilonInspectFdt(base + 1, base, end).Status == EPSILON_FDT_ADDRESS);
    assert(EpsilonInspectFdt(end - 32, base, end).Status == EPSILON_FDT_ADDRESS);
    assert(EpsilonInspectFdt(base, base, base + 48).Status == EPSILON_FDT_INVALID);
    assert(EpsilonInspectFdt(base, end, base).Status == EPSILON_FDT_ADDRESS);
    fdt_set_magic(buf, 0);
    assert(EpsilonInspectFdt(base, base, end).Status == EPSILON_FDT_INVALID);
    fixture(buf, 512, base + 512, end, 8);
    fdt_set_size_dt_struct(buf, 4);
    assert(EpsilonInspectFdt(base, base, end).Status == EPSILON_FDT_INVALID);
    fixture(buf, 512, base + 512, end + 1, 8);
    assert(EpsilonInspectFdt(base, base, end).Status == EPSILON_FDT_INITRD_RANGE);
    fixture(buf, 512, base + 512, base + 512, 8);
    assert(EpsilonInspectFdt(base, base, end).Status == EPSILON_FDT_INITRD_RANGE);
    fixture(buf, 512, base + 8, base + 512, 8);
    assert(EpsilonInspectFdt(base, base, end).Status == EPSILON_FDT_INITRD_RANGE);
    fixture(buf, 512, 0, 0, 0);
    assert(EpsilonInspectFdt(base, base, end).Status == EPSILON_FDT_INITRD_PROPS);
    fixture(buf, 512, 0, 0, 2);
    assert(EpsilonInspectFdt(base, base, end).Status == EPSILON_FDT_INITRD_PROPS);
    fixture(buf, 512, 0x84B84000, 0x856FEE3E, 4);
    result = EpsilonInspectFdt(base, base, end);
    assert(result.Status == EPSILON_FDT_INITRD_RANGE);
    assert(result.InitrdStart == 0x84B84000 && result.InitrdEnd == 0x856FEE3E);
    puts("Bounded FDT/read window and initrd interval checks passed");

    if (argc == 2) {
        static _Alignas(8) unsigned char captured[EPSILON_FDT_SIZE_LIMIT];
        FILE *input = fopen(argv[1], "rb");
        assert(input != NULL);
        size_t size = fread(captured, 1, sizeof(captured), input);
        assert(feof(input) && !ferror(input) && fclose(input) == 0);
        const UINT64 blob = (UINTN)captured;
        result = EpsilonInspectFdt(blob, blob, blob + size);
        // Physical initrd bounds are outside the host buffer, but still decoded.
        assert(result.Status == EPSILON_FDT_INITRD_RANGE);
        assert(result.TotalSize == size);
        assert(result.InitrdStart == 0x84B84000 && result.InitrdEnd == 0x856FEE3E);
        puts("Android audit FDT parses and reproduces both saved initrd bounds");
    }
}
