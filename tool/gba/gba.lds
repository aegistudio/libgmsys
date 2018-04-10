/**
 * Game Machine System Library - Linker Script for GBA 
 * (GameBoy Advanced)
 * @author Haoran Luo
 *
 * This script is part of the libgmsys, which helps resolving 
 * some library defined symbols so that the code could access
 * proper location in memory.
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
ENTRY(__gba_rom_start)

SECTIONS
{
	/** Section of compiled executable code. */
	.rom ABSOLUTE(0x08000000) : {
		*(.romhdr)
		*(.text)
		*(.text.*)
		*(.rodata)
		*(.rodata.*)
	}

	/** Sections that should be loaded on the slow wram. */
	.wram.text ABSOLUTE(0x02000000) : {
		*(.wram.text)
	}
	.wram.data ABSOLUTE(0x02000000 + SIZEOF(.wram.text)) : {
		*(.data)
		*(.data.*)
		*(.wram.data)
		*(.bss)
		*(.bss.*)
	}

	/** Sections that should be loaded on the fast wram. */
	.fastwram.text ABSOLUTE(0x03000000) : {
		*(.fastwram.text)
	}
	.fastwram.data ABSOLUTE(0x03000000 + SIZEOF(.wram.text)): {
		*(.fastwram.data)
	}

	/** Section of GBA internal memory / symbols. */
	.gba 0 (NOLOAD) : {
		/** The keypad control mapped memory. */
		__gba_keypad_status     = 0x04000130;
		__gba_keypad_control    = 0x04000132;

		/** The interrupt mapped memory. */
		__gba_interrupt_handler = 0x03007ffc;
		__gba_interrupt_check   = 0x03007ff8;
		__gba_interrupt_master  = 0x04000208;
		__gba_interrupt_enabled = 0x04000200;
		__gba_interrupt_flag    = 0x04000202;

		/** The video control mapped memory. */
		__gba_video_control	= 0x04000000;
		__gba_video_status      = 0x04000004;
	}

	/** Section that would be discarded on linking. */
	/DISCARD/ : {
		*(.eh_frame)
		*(.comment)
		*(.ARM.*)
	}
}
