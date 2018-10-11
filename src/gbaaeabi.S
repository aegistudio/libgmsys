# gbaaeabi.S - Common ARM EABI implementation for GBA Cartridge
# @author Haoran Luo

# Should be load into the ewram memory in order to speed up the calculation.
.section .ewram.text

# Perform unsigned integer division.
.global __aeabi_uidiv
__aeabi_uidiv:
	push {r1, r3}
	swi 0x060000
	pop {r1, r3}
	bx lr

# Perform unsigned integer division-modulus.
.global __aeabi_idivmod
__aeabi_idivmod:
	push {r3}
	swi 0x060000
	pop {r3}
	bx lr
