/*
 * Xiaomi Mi 5 (gemini) - first hardware test shim.
 *
 * Proves, in one boot, the three things B1 needs:
 *   1. the boot chain aboot -> lk2nd -> fastboot boot -> arm64 Image -> us,
 *      entered at EL1 with the MMU off and x0 = DTB (same contract as ABL);
 *   2. the framebuffer the bootloader left running is really at 0x83401000
 *      (lk2nd's cont-splash dump: VIG_0 SRC0_ADDR, stride 3240, RGB888);
 *   3. the JDI R63452 panel is in COMMAND mode, so the frame only appears
 *      after MDP CTL0 START (0x90201c) is written.
 *
 * Nothing is written to storage. After 30 s it resets through PSCI so the
 * phone returns to the bootloader by itself.
 */
#include <stdint.h>
#include "console.h"
#include "loader.h"

#define PSCI_SYSTEM_RESET	0x84000009UL
/* Where lk2nd drops the DTB (ABOOT_FORCE_TAGS_ADDR = DDR_START + 0x2500000);
 * also used as the appended-payload end bound, like the Pixel build does. */
#define TAGS_ADDR		0x82500000UL

/* APSS watchdog, left running by aboot. Re-arm it before jumping into the
 * payload so a hang in the kernel/payload resets the phone by itself instead
 * of needing a 15 s power-button press. XNU's own wdt driver takes it over. */
#define APSS_WDT		0x9830000UL
#define WDT_RST			0x04
#define WDT_EN			0x08
#define WDT_BARK		0x10
#define WDT_BITE		0x14
#define WDT_SECS		90

extern char _end[];

static inline uint32_t rd32(uintptr_t a)
{
	return *(volatile uint32_t *)a;
}

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

static void __attribute__((noreturn)) psci_reset(void)
{
	register uint64_t x0 __asm__("x0") = PSCI_SYSTEM_RESET;
	__asm__ volatile("dsb sy; smc #0" : "+r"(x0) : : "memory");
	for (;;)
		__asm__ volatile("wfe");
}

static void wdt_arm(unsigned secs)
{
	*(volatile uint32_t *)(APSS_WDT + WDT_BARK) = secs * 32768;
	*(volatile uint32_t *)(APSS_WDT + WDT_BITE) = secs * 32768;
	*(volatile uint32_t *)(APSS_WDT + WDT_RST) = 1;
	__asm__ volatile("dsb sy" ::: "memory");
	*(volatile uint32_t *)(APSS_WDT + WDT_EN) = 1;
	__asm__ volatile("dsb sy" ::: "memory");
}

void shim_main(uint64_t dtb)
{
	con_clear(0xff102030);
	con_str("GEMINI", "test shim running");
	con_hex("DTB", dtb);
	con_hex("CTL_TOP", rd32(0x902014));
	con_hex("FB_SRC0", rd32(0x83401000));
	con_hex("FB_IMG", rd32(0x905004));
	con_hex("FB_STRIDE", rd32(0x905024));
	con_str("PANEL", "cmd mode, kick 0x90201c");
	con_hex("PAYLOAD", (uintptr_t)_end);
	con_flush();

	/* A hang from here on must not need a power-button press. */
	wdt_arm(WDT_SECS);

	/* Payload in the ramdisk slot (lk2nd/ABL fill linux,initrd-start/end). */
	const char *why = loader_boot(dtb);
	con_str("LOADER", why);
	con_flush();

	/* Fallback: a SWEETXNU blob appended at _end (the Pixel layout). */
	why = loader_boot_rd((uintptr_t)_end, TAGS_ADDR);
	con_str("LOADER2", why);
	con_flush();
	delay_ms(30000);
	psci_reset();
}

void secondary_main(uint64_t idx)
{
	(void)idx;
	for (;;)
		__asm__ volatile("wfe");
}

void exc_handler(uint64_t slot, uint64_t *regs)
{
	uint64_t esr, elr, far;
	(void)regs;
	__asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
	__asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
	__asm__ volatile("mrs %0, far_el1" : "=r"(far));
	con_clear(0xff400000);
	con_str("GEMINI", "EXCEPTION");
	con_hex("SLOT", slot);
	con_hex("ESR", esr);
	con_hex("ELR", elr);
	con_hex("FAR", far);
	con_flush();
	delay_ms(20000);
	psci_reset();
}
