# Makefile - Game Machine System Library Builder
# @author Haoran Luo

SHELL := /bin/bash

# This file is used to pre-compile some library or toolchain
# so that consequent development could be made on.
# Before invoking the Makefile, the GMSYS_ROOT should be set,
# or unexpected behavior will be expected.

# The native compiler of the operating system you are running
# under. These compilers are used to generate toolchain's
# executables.
NATIVE_CPP=g++
NATIVE_CC=gcc
NATIVE_LD=ld
NATIVE_AS=as

# The machine compiler which will compile the library into
# archive or object code. And could be linked with the user
# program later.
MACH_CPP=gmsys-g++
MACH_CC=gmsys-gcc
MACH_LD=gmsys-ld
MACH_AS=gmsys-as
MACH_AR=gmsys-ar

# The target for building library and tool chain for GBA 
# (GameBoy Advanced).
gba: bin/gbacrt0.o bin/gba.a bin/gmsys-gbarom

# The stub ROM header for GBA cartridge.
bin/gbacrt0.o: src/gbacrt0.S
	$(MACH_AS) $< -o $@

# The AEABI implementaiton for GBA cartridge.
bin/gbaaeabi.o: src/gbaaeabi.S
	$(MACH_AS) $< -o $@
	
# The special ROM loader for GBA. The BFD library is used.
bin/gmsys-gbarom: src/gbaromld.cpp
	$(NATIVE_CPP) -O3 $< -o $@ -lbfd -std=c++11

# The object files in GBA cartridges.
bin/gbabios.o: src/gbabios.c
	$(MACH_CC) -O3 -c $< -o $@

# The memory management library for gba.
# The file is built in thumb mode to reduce code size, please compile with
# '-mthumb-interwork' when building your user code and link with it.
bin/gbamm.o: src/gbamm.cpp
	$(MACH_CPP) -c -mthumb -O3 $< -o $@ -std=c++11 -nostdlib -fno-exceptions
	
# The compiled library in GBA flavour.
bin/gba.a: bin/gbabios.o bin/gbamm.o bin/gbaaeabi.o
	$(MACH_AR) -rcs $@ $^

clean:
	rm bin/*
