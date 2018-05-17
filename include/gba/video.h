#pragma once
/**
 * gba/video.h - Video definition for GBA.
 * @author Haoran Luo
 *
 * Defines structure of each video registers, and 
 * symbol for accessing those registers. Please notice
 * that the symbol of those register should be resolved
 * on the linking stage with specific linker script.
 *
 * @see http://problemkaputt.de/gbatek.htm#gbalcdvideocontroller
 */

// Set the memory location alignment to just one.
#pragma pack(push)
#pragma pack(1)

// Avoid name mangling when compiled in C++ source.
#ifdef __cplusplus
extern "C" {
#endif

// Defines the video control register's structure.
// See the LCD Video Controller topic on GBATEK for details:
// @see http://problemkaputt.de/gbatek.htm#lcdiodisplaycontrol
typedef union {
	// Accessing the register as bit fields.
	struct {
		// The BG mode of the video (0 to 5). 
		unsigned short mode             : 3;

		// The CGB mode (could only set by bios opcodes).
		unsigned short cgb              : 1;

		// The current displaying frame (0 or 1).
		unsigned short frame            : 1;

		// Allow access to OAM while H-Blank.
		unsigned short hblank_oamaccess : 1;

		// Object character mapping mode (0 for two
		// dimensional, and 1 for one dimensional).
		unsigned short obj_mapmode      : 1;

		// Allow fast access to VRAM, palatte and OAM.
		unsigned short forced_blank     : 1;

		// Whether corresponding BG display is visible.
		unsigned short bg0_visible      : 1;
		unsigned short bg1_visible      : 1;
		unsigned short bg2_visible      : 1;
		unsigned short bg3_visible      : 1;
		unsigned short obj_visible      : 1;

		// The display flags for windows.
		unsigned short win0_display     : 1;
		unsigned short win1_display     : 1;
		unsigned short obj_display      : 1;
	} bits;

	// Accessing the register as half word.
	unsigned short halfword;
} __gba_video_control_t;

// Defines the video status register's structure.
typedef union {
	// Accessing the register as bit fields.
	struct {
		// The read only fields indicating current video status.
		unsigned short vblank               : 1;
		unsigned short hblank               : 1;
		unsigned short vcounter             : 1;

		// The flags set so that interrupt could be triggered.
		unsigned short vblank_irq_enabled   : 1;
		unsigned short hblank_irq_enabled   : 1;
		unsigned short vcounter_irq_enabled : 1;

		unsigned short unused               : 2;

		// Indicates the vertical counter to match.
		unsigned short vcounter_target      : 8;
	} bits;

	// Accessing the register as half word.
	unsigned short halfword;
} __gba_video_status_t;

// The register locations for video registers.
extern volatile __gba_video_control_t __gba_video_control;
extern volatile __gba_video_status_t __gba_video_status;
extern volatile unsigned short __gba_video_vcounter;

// End of avoid name mangling when compiled in C++ source.
#ifdef __cplusplus
}

// Perform some static assertion (of c++11) to ensure the 
// size of the specified registers.
static_assert(sizeof(__gba_video_control_t) == 2,
	"The structure of GBA video control should occupy only 2 bytes.");
static_assert(sizeof(__gba_video_status_t) == 2,
	"The structure of GBA video status should occupy only 2 bytes.");
#endif

// Restore the memory alignment.
#pragma pack(pop)
