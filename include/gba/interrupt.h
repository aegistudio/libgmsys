#pragma once
/**
 * gba/interrupt.h - Interrupt Register Definition.
 * @author Haoran Luo
 *
 * Defines structure of each interrupt register, and 
 * symbol for accessing those registers. Please notice
 * that the symbol of those register should be resolved
 * on the linking stage with specific linker script.
 *
 * @see http://problemkaputt.de/gbatek.htm#gbainterruptcontrol
 */

// Set the memory location alignment to just one.
#pragma pack(push)
#pragma pack(1)

// Avoid name mangling when compiled in C++ source.
#ifdef __cplusplus
extern "C" {
#endif

/**
 * The mask of the gba interrupt when we arrange it as a bit 
 * map. Will be useful when we would like to perform bitmap
 * operation on such registers.
 */
enum __gba_interrupt_mask_t {
	im_none			= 0,
	im_vblank		= 1 << 0,
	im_hblank		= 1 << 1,
	im_vcounter		= 1 << 2,
	im_timer0		= 1 << 3,
	im_timer1		= 1 << 4,
	im_timer2		= 1 << 5,
	im_timer3		= 1 << 6,
	im_serial		= 1 << 7,
	im_dma0			= 1 << 8,
	im_dma1			= 1 << 9,
	im_dma2			= 1 << 10,
	im_dma3			= 1 << 11,
	im_keypad		= 1 << 12,
	im_gamepak		= 1 << 13,
	im_all			= 1 << 14 - 1
};

typedef union {
	struct {
		// Interrupted when the scanline just come into 
		// the vertical blank region.
		unsigned short vblank 	: 1;
		
		// Interrupted when the scanline just come into
		// the horizontal blank region.
		unsigned short hblank 	: 1;
		
		// Interrupted when the scanline counter matches
		// the number set to interrupt.
		unsigned short vcounter	: 1;
		
		// Interrupted when specific timer overflow.
		unsigned short timer0	: 1;
		unsigned short timer1	: 1;
		unsigned short timer2	: 1;
		unsigned short timer3	: 1;
		
		// Interrupted when the serial communication generates
		// an interrupt signal.
		unsigned short serial	: 1;
		
		// Interrupted when DMA transfer is finished.
		unsigned short dma0		: 1;
		unsigned short dma1		: 1;
		unsigned short dma2		: 1;
		unsigned short dma3		: 1;
		
		// Interrupted when the keypad is pressed.
		unsigned short keypad	: 1;
		
		// Interrupted when the gamepak or external source 
		// generate such interrupt.
		unsigned short gamepak	: 1;
	} source;
	unsigned short mask : 14;
} __gba_interrupt_t;

/**
 * The memory locations of the interrupt master enabled register,
 * interrupt enabled register and interrupt flag register. 
 * Please refer the GBATEK document for their usage.
 */
extern volatile int __gba_interrupt_master;
extern volatile __gba_interrupt_t __gba_interrupt_enabled;
extern volatile __gba_interrupt_t __gba_interrupt_flag;

/**
 * The function pointer to the user's interrupt handler. You could
 * change this value at bootstrap to register your handler.
 * The handler is entered under ARM mode and you should switch to
 * thumb mode manually if required.
 */
extern void (*__gba_interrupt_handler)();

// End of avoid name mangling when compiled in C++ source.
#ifdef __cplusplus
}

// Perform some static assertion (of c++11) to ensure the size of
// the specified registers.
static_assert(sizeof(__gba_interrupt_t) == 2,
	"The structure of GBA keypad should occupy only 2 bytes.");
#endif

// Restore the memory alignment.
#pragma pack(pop)