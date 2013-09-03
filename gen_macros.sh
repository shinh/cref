#!/bin/sh

set -e

includes="
-include assert.h
-include ctype.h
-include errno.h
-include fenv.h
-include inttypes.h
-include limits.h
-include math.h
-include setjmp.h
-include signal.h
-include stdarg.h
-include stdbool.h
-include stddef.h
-include stdint.h
-include stdio.h
-include stdlib.h
-include string.h
-include time.h
-include unistd.h
"

g++ -dM -xc++ /dev/null -E `echo $includes` > libc-2.17-x64.txt
g++ -m32 -dM -xc++ /dev/null -E `echo $includes` > libc-2.17-i686.txt
g++ -mx32 -dM -xc++ /dev/null -E `echo $includes` > libc-2.17-x32.txt
$NACL_SDK_ROOT/toolchain/linux_x86_glibc/x86_64-nacl/bin/g++ -m64 -dM -xc++ /dev/null -E `echo $includes` > libc-2.9-nacl-x64.txt
$NACL_SDK_ROOT/toolchain/linux_x86_glibc/x86_64-nacl/bin/g++ -m32 -dM -xc++ /dev/null -E `echo $includes` > libc-2.9-nacl-i686.txt
