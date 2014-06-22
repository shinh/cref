#!/bin/sh

set -ex

./dump_debug_info /usr/lib/debug/lib/x86_64-linux-gnu/libc-2.18.so > libc-2.18-x64.json
./dump_debug_info /usr/lib/debug/lib/i386-linux-gnu/libc-2.18.so > libc-2.18-i686.json
./dump_debug_info /usr/tmp/eglibc-2.17/build-tree/amd64-x32/libc.so > libc-2.17-x32.json
./dump_debug_info $NACL_SDK_ROOT/toolchain/linux_x86_glibc/x86_64-nacl/lib64/libc-2.9.so > libc-2.9-nacl-x64.json
./dump_debug_info $NACL_SDK_ROOT/toolchain/linux_x86_glibc/x86_64-nacl/lib32/libc-2.9.so > libc-2.9-nacl-i686.json
./gen_sizeof.rb > sizeof.html
