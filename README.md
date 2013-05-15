linux-syscall-hooker
====================

A Linux kernel module that locates the system call table in memory and hooks uname.
http://www.elliotbradbury.com/linux-syscall-hooking-interrupt-descriptor-table/

Prerequisites
-------------

- An x86 installation - does not work on x64.
- Kernel headers (`apt-get install linux-headers-$(uname -r)`)

Compiling and Running
---------------------

Note: This module unprotects and overwrites portions of kernel memory space. I recommend testing it in a VM.

1. `git clone https://github.com/ebradbury/linux-syscall-hooker.git`
2. `cd linux-syscall-hooker`
3. `make`
4. `insmod ./my_module.ko`
5. `uname`

If it worked, the output of `uname` should be "All ur base r belong to us". If it didn't work, check `dmesg` for details.