#!/bin/bash
# Build the Mi 5 (gemini) shim and wrap it in a v0 boot.img.
# Usage: shim/build-gemini.sh STOCK_BOOT.img OUT.img [PAYLOAD]
#   PAYLOAD        appended to the Image at _end (SWEETXNU blob)
#   BUILD_STUB=1   also build shim/out/gemini/stub.bin (loader test payload)
# Produces shim/out/gemini/{shim.elf,Image[,stub.bin]} and OUT.img.
set -euo pipefail
stock=$(realpath "$1")
out=$(realpath -m "$2")
payload=${3:-}
cd "$(dirname "$0")"
X=aarch64-linux-gnu-
CFLAGS="-O2 -ffreestanding -nostdlib -fno-pic -mgeneral-regs-only -mstrict-align -fno-stack-protector -Wall -Wextra -DGEMINI"
mkdir -p out/gemini

for f in start.S gemini.c console.c loader.c fdt.c; do
	${X}gcc $CFLAGS -c $f -o out/gemini/${f%.*}.o
done
${X}gcc $CFLAGS -static -T shim.ld -Wl,--build-id=none -Wl,--no-warn-rwx-segments -Wl,-N \
	out/gemini/start.o out/gemini/gemini.o out/gemini/console.o \
	out/gemini/loader.o out/gemini/fdt.o -o out/gemini/shim.elf
${X}objcopy -O binary out/gemini/shim.elf out/gemini/Image
size=$(${X}nm out/gemini/shim.elf | awk '$3=="_end"{print $1}')
truncate -s $((16#$size)) out/gemini/Image
if [ -n "$payload" ]; then
	cat "$payload" >> out/gemini/Image
	echo "payload: $(stat -c%s "$payload") bytes appended"
fi
echo "shim: $(stat -c%s out/gemini/Image) bytes"

if [ "${BUILD_STUB:-0}" = 1 ]; then
	${X}gcc $CFLAGS -c gemini_stub.c -o out/gemini/gemini_stub.o
	${X}gcc $CFLAGS -c console.c -o out/gemini/console_stub.o
	${X}gcc $CFLAGS -static -T shim.ld -Wl,--build-id=none -Wl,--no-warn-rwx-segments -Wl,-N \
		out/gemini/gemini_stub.o out/gemini/console_stub.o -o out/gemini/stub.elf
	${X}objcopy -O binary out/gemini/stub.elf out/gemini/stub.bin
	size=$(${X}nm out/gemini/stub.elf | awk '$3=="_end"{print $1}')
	truncate -s $((16#$size)) out/gemini/stub.bin
	echo "stub: $(stat -c%s out/gemini/stub.bin) bytes"
fi

# the repo is checked out with CRLF: strip \r and run the helper (it also
# embeds a python heredoc, which breaks on CRLF just the same)
tr -d '\r' < ../tools/mkboot-v0.sh > out/gemini/mkboot-v0.sh
bash out/gemini/mkboot-v0.sh "$stock" out/gemini/Image "$out"
