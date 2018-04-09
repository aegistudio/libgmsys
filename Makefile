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
gba: bin/gbacrt0.o bin/gba.a

# The stub ROM header for GBA cartridge.
bin/gbacrt0.o: src/gbacrt0.S
	$(MACH_AS) $< -o $@

# The object files in GBA cartridges.
bin/bios_gba.o: src/bios_gba.c
	$(MACH_CC) -O3 -c $< -o $@

# The compiled library in GBA flavour.
bin/gba.a: bin/bios_gba.o
	$(MACH_AR) -rcs $@ $<

clean:
	rm bin/*
