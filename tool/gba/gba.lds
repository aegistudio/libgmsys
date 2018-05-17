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

	/** Sections that should be loaded on the external wram. */
	.ewram.text ABSOLUTE(0x02000000) : {
		*(.ewram.text)
	}
	__gba_ewram_text_size = ((SIZEOF(.ewram.text) + 3) | 3) - 3;
	.ewram.data ABSOLUTE(0x02000000 + __gba_ewram_text_size) : {
		*(.data)
		*(.data.*)
		*(.ewram.data)
		*(.bss)
		*(.bss.*)
	}

	/** Sections that should be loaded on the internal wram. */
	.iwram.text ABSOLUTE(0x03000000) : {
		*(.iwram.text)
	}
	__gba_iwram_text_size = ((SIZEOF(.iwram.text) + 3) | 3) - 3;
	.iwram.data ABSOLUTE(0x03000000 + __gba_iwram_text_size): {
		*(.iwram.data)
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
		__gba_video_vcounter    = 0x04000006;

		/** The sprite control mapped memory. */
		__gba_sprite_attributes = 0x07000000;
	}

	/** Section that would be discarded on linking. */
	/DISCARD/ : {
		*(.eh_frame)
		*(.comment)
		*(.ARM.*)
	}
}
