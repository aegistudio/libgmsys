# gbacrt0.S - Common ROM Header for GBA Cartridge
# @author Haoran Luo

# This assembly just export a minimal crt0.S file for gba, making
# it easy to understand what is happening.
.section .romhdr
.global __gba_rom_start

__gba_rom_start:
	# The cartridge header which will be loaded in the 
	# memory location of 0x08000000, and the BIOS will check
	# the integrity of this part before invoking the first
	# instruction. See cartridge header topic in GBATEK:
	# http://problemkaputt.de/gbatek.htm#gbacartridgeheader
	b __gba_rom_hdrbody
	.space 188
__gba_rom_hdrbody:
	# Will directly invoke the main function after skipping 
	# the header information in the cartridge header.
	bl main
	
	# If the code return from the main, the machine will Soft
	# Reset, see also the reset topic in GBATEK:
	# http://problemkaputt.de/gbatek.htm#biosresetfunctions
	swi 0x0
