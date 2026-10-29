/*
 * Loader test payload for the Mi 5 (gemini) shim: stands in for xnu at the
 * payload entry point, prints what loader.c handed over on the panel, checks
 * the filler segment the host wrote, then resets. MMU off, x0 = boot_args.
 *
 * Built by shim/build-gemini.sh BUILD_STUB=1 and wrapped with
 * tools/mkstub-payload.py (PHYS=0x100000000 MEM=0x5eb50000).
 */
#include <stdint.h>
#include "console.h"

__asm__(".section .text.head, \"ax\"\n"
	".globl _head\n"
	"_head:\n"
	"	adr	x1, _stack_top\n"
	"	mov	sp, x1\n"
	"	b	stub_main\n");

struct boot_args {	/* loader.c, Revision/Version 2 */
	uint16_t Revision, Version;
	uint64_t virtBase, physBase, memSize, topOfKernelData;
	uint64_t v_baseAddr, v_display, v_rowBytes, v_width, v_height, v_depth;
	uint32_t machineType;
	uint64_t deviceTreeP;
	uint32_t deviceTreeLength;
	char CommandLine[608];
};

#define FILL_OFF	0x100000UL	/* mkstub-payload.py FILL_OFF */

static uint64_t now(void)
{
	uint64_t t;
	__asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(t));
	return t;
}

static void delay_ms(unsigned ms)
{
	uint64_t f, end;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
	end = now() + f / 1000 * ms;
	while (now() < end)
		;
}

void __attribute__((noreturn)) stub_main(volatile struct boot_args *ba)
{
	char line[72];
	int i;

	con_clear(0xff003040);
	con_str("STUB", "payload reached");
	con_hex("BA", (uintptr_t)ba);
	con_hex("REV", ((uint64_t)ba->Revision << 16) | ba->Version);
	con_hex("PHYS", ba->physBase);
	con_hex("MEM", ba->memSize);
	con_hex("TOP", ba->topOfKernelData);
	con_hex("ADT", ba->deviceTreeP);
	con_hex("ADTLEN", ba->deviceTreeLength);
	con_hex("FB", ba->v_baseAddr);
	con_hex("FBDEPTH", ba->v_depth);

	/* cmdline "stub sum=<hex> len=<hex>": the filler's expected checksum */
	for (i = 0; i < 63 && ba->CommandLine[i]; i++)
		line[i] = ba->CommandLine[i];
	line[i] = 0;

	uint64_t len = 0, sum = 0;
	for (const char *p = line; *p; p++)
		if (p[0] == 'l' && p[1] == 'e' && p[2] == 'n' && p[3] == '=') {
			for (p += 4; *p && *p != ' '; p++)
				len = len << 4 | (uint64_t)(*p <= '9' ? *p - '0' : *p - 'a' + 10);
			break;
		}
	for (uint64_t off = 0; off < len; off += 8)
		sum = sum * 31 + *(volatile uint64_t *)(ba->physBase + FILL_OFF + off);

	con_hex("FILLEN", len);
	con_hex("GOTSUM", sum);
	con_str("STUB", "done, PSCI reset");
	con_flush();
	delay_ms(40000);	/* distinct from the shim's 30 s and the 90 s
				 * watchdog, so the host can tell what ran */

	register uint64_t x0 __asm__("x0") = 0x84000009UL;	/* SYSTEM_RESET */
	__asm__ volatile("dsb sy; smc #0" : "+r"(x0) : : "memory");
	for (;;)
		__asm__ volatile("wfe");
}
