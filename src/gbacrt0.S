# gbacrt0.S - Common ROM Header for GBA Cartridge
# @author Haoran Luo

# This assembly just export a minimal crt0.S file for gba, making
# it easy to understand what is happening.
.section .romhdr
.global __gba_rom_start

# The GPIO purpose registers on cartridge.
.global __gba_gpio_data
.global __gba_gpio_direction
.global __gba_gpio_control

# The hard-coded data used to control wram and fast wram upload.
.global __gba_slowwram_address
.global __gba_slowwram_size
.global __gba_fastwram_address
.global __gba_fastwram_size

__gba_rom_start:
	# The cartridge header which will be loaded in the 
	# memory location of 0x08000000, and the BIOS will check
	# the integrity of this part before invoking the first
	# instruction. See cartridge header topic on GBATEK:
	# http://problemkaputt.de/gbatek.htm#gbacartridgeheader
	b               __gba_rom_hdrbody
	.space          188

	# The potential reserved GPIO ports on GBA. Requires
	# cartridges of certain specification. We will still
	# Reserve them here. See GPIO topic on GBATEK:
	# http://problemkaputt.de/gbatek.htm#gbacartioportgpio
	.space          4	                     
__gba_gpio_data:
	.space          2
__gba_gpio_direction:
	.space          2
__gba_gpio_control:
	.space          2
	.space          2

	# The hardcoded variables that should be replaced with
	# the rom making command.
__gba_slowwram_address:
	.word           0
__gba_slowwram_size:
	.word           0
__gba_fastwram_address:
	.word           0
__gba_fastwram_size:
	.word           0

	# The address skipping the headers, GPIO ports and hard
	# coded program uploading code.
__gba_rom_hdrbody:

	# Upload the code to slow wram if required.
__gba_slowwram_upload:
	ldr             r2, =__gba_slowwram_size
	ldr             r2, [r2]
	cmp             r2, #0
	beq             __gba_slowwram_upload_end
	ldr             r1, =0x02000000
	ldr             r0, =__gba_slowwram_address
	ldr             r0, [r0]
	swi             0x0C0000
__gba_slowwram_upload_end:

	# Upload the code to fast wram if required.
__gba_fastwram_upload:
	ldr             r2, =__gba_fastwram_size
	ldr             r2, [r2]
	cmp             r2, #0
	beq             __gba_fastwram_upload_end
	ldr             r1, =0x03000000
	ldr             r0, =__gba_fastwram_address
	ldr             r0, [r0]
	swi             0x0C0000
__gba_fastwram_upload_end:

__gba_rom_invokemain:
	# Will directly invoke the main function after skipping 
	# the header information in the cartridge header.
	bl              main
	
	# If the code return from the main, the machine will Soft
	# Reset, see also the reset topic on GBATEK:
	# http://problemkaputt.de/gbatek.htm#biosresetfunctions
	swi             0x0
