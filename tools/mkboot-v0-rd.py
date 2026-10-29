#!/usr/bin/env python3
"""Wrap a raw arm64 Image as the kernel of a v0 boot.img and put a payload in
the ramdisk slot (the Mi 5 variant: lk2nd/ABL then passes linux,initrd-start/end
and the shim's loader_boot() finds it there).

The stock DTBs stay appended to the kernel, exactly like mkboot-v0.sh.

  mkboot-v0-rd.py STOCK_BOOT.img Image RAMDISK OUT.img
"""
import gzip, hashlib, struct, sys

stock, image, ramdisk, out = sys.argv[1:5]
b = open(stock, 'rb').read()
assert b[:8] == b'ANDROID!'
ksize, _, rsize, _, ssize, _, _, page, hv = struct.unpack_from('<9I', b, 8)
assert hv == 0, 'only v0 headers'
up = lambda n: (n + page - 1) // page * page

kernel = b[page:page + ksize]
dtbs = kernel[kernel.index(b'\xd0\x0d\xfe\xed'):]	# stock appended DTBs
k = gzip.compress(open(image, 'rb').read(), 9, mtime=0) + dtbs
r = open(ramdisk, 'rb').read()

h = bytearray(b[:page])
struct.pack_into('<I', h, 8, len(k))
struct.pack_into('<I', h, 16, len(r))
sha = hashlib.sha1()
for part in (k, r):
	sha.update(part)
	sha.update(struct.pack('<I', len(part)))
h[576:608] = sha.digest().ljust(32, b'\0')

pad = lambda d: d + b'\0' * (up(len(d)) - len(d))
open(out, 'wb').write(bytes(h) + pad(k) + pad(r))
print(f'{out}: kernel {len(k)} (gz {len(k) - len(dtbs)} + {len(dtbs)} dtb), ramdisk {len(r)}')
