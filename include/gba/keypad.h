#pragma once
/**
 * gba/keypad.h - Keypad I/O Register Definition.
 * @author Haoran Luo
 *
 * Defines structure of each keypad I/O register, and 
 * symbol for accessing those registers. Please notice
 * that the symbol of those register should be resolved
 * on the linking stage with specific linker script.
 *
 * @see http://problemkaputt.de/gbatek.htm#gbakeypadinput
 */

// Set the memory location alignment to just one.
#pragma pack(push)
#pragma pack(1)

// Avoid name mangling when compiled in C++ source.
#ifdef __cplusplus
extern "C" {
#endif

/**
 * The mask of the gba keypad when we arrange it as a bit 
 * map. Will be useful when we would like to perform bitmap
 * operation on such registers.
 */
enum __gba_keypad_mask_t {
	km_none		= 0,
	km_a		= 1 << 0,
	km_b		= 1 << 1,
	km_select	= 1 << 2,
	km_start	= 1 << 3,
	km_right	= 1 << 4,
	km_left		= 1 << 5,
	km_up		= 1 << 6,
	km_down		= 1 << 7,
	km_r		= 1 << 8,
	km_l		= 1 << 9,
	km_all		= 1 << 10 - 1
};

/**
 * This structure depicts the I/O register layout of a 
 * GBA keypad. Which remains the same for the keypad
 * status register and keypad control register.
 */
typedef union {
	struct {
		unsigned short a 		: 1;	// Button A
		unsigned short b 		: 1;	// Button B
		unsigned short select	: 1;	// Select
		unsigned short start	: 1;	// Start
		unsigned short right	: 1;	// Right
		unsigned short left		: 1;	// Left
		unsigned short up		: 1;	// Up
		unsigned short down		: 1;	// Down
		unsigned short r		: 1;	// Button R
		unsigned short l		: 1;	// Button L
	} button;
	unsigned short mask : 10;
} __gba_keypad_t;

/**
 * This structure depicts the layout of a keypad control
 * register. The lower bits stores the keypad mask and 
 * the higher bit stores the interrupt control bits.
 */
typedef struct {
	// The bits indicating the button that is clicked.
	unsigned short buttons			: 10;
	
	// These bits are remained zero and will not be used.
	unsigned short unused			: 4;
	
	// 0 = Disabled, 1 = Enabled
	unsigned short irq_enabled		: 1;
	
	// 0 = Triggered when one button is clicked.
	// 1 = Triggered when all button masked is clicked.
	unsigned short irq_condition	: 1;
} __gba_keypad_intr_t;

/**
 * The memory locations of the keypad status register and the keypad
 * interrupt control register. Please notice that the keypad status
 * register will never be writable.
 */
extern const volatile __gba_keypad_t __gba_keypad_status;
extern volatile __gba_keypad_intr_t __gba_keypad_control;

// End of avoid name mangling when compiled in C++ source.
#ifdef __cplusplus
}

// Perform some static assertion (of c++11) to ensure the size of
// the specified registers.
static_assert(sizeof(__gba_keypad_t) == 2,
	"The structure of GBA keypad should occupy only 2 bytes.");
static_assert(sizeof(__gba_keypad_intr_t) == 2,
	"The register of GBA keypad interrupt should occupy only 2 bytes.");
#endif

// Restore the memory alignment.
#pragma pack(pop)