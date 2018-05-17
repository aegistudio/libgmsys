#pragma once
/**
 * gba/sprite.h - Sprite definition for GBA.
 * @author Haoran Luo
 *
 * Defines structure of each sprite registers, and 
 * symbol for accessing those registers. Please notice
 * that the symbol of those register should be resolved
 * on the linking stage with specific linker script.
 *
 * @see http://problemkaputt.de/gbatek.htm#lcdobjoverview
 */

// Set the memory location alignment to just one.
#pragma pack(push)
#pragma pack(1)

// Avoid name mangling when compiled in C++ source.
#ifdef __cplusplus
extern "C" {
#endif

// Defines the sprite flag used attribute 0 of each sprite.
enum __gba_sprite_flag {
	oamflg_normal        = 0,
	oamflg_effect        = 1,
	oamflg_disabled      = 2,
	oamflg_effect_double = 3
};

// Defines the sprite mode used attribute 0 of each sprite.
enum __gba_sprite_mode {
	oammode_normal          = 0,
	oammode_semitransparent = 1,
	oammode_objwindow       = 2,
	oammode_prohibited      = 3
};

// Defines the shape of each gba object.
enum __gba_sprite_shape {
	oamshape_square          = 0,
	oamshape_rect_horizontal = 1,
	oamshape_rect_vertical   = 2,
	oamshape_prohibited      = 3
};

// Defines the sprite transform flag.
enum __gba_sprite_transform {
	oamtrans_none            = 0,
	oamtrans_horizontal_flip = 1 << 3,
	oamtrans_vertical_flip   = 1 << 4
};

// Defines the sprite attribute's structure.
// See the OAM (Object Attribute Memory) topic on GBATEK for details:
// @see http://problemkaputt.de/gbatek.htm#lcdobjoamattributes
typedef union {
	// Accessing the register as bit fields.
	struct {
		/** Attribute 0 */
		// The sprite's Y coord on screen (start from top).
		unsigned short y                 : 8;

		// The flag indicates whether the affine transform should
		// be applied to this sprite.
		unsigned short flag              : 2;

		// The mode indicates how to display the sprite.
		unsigned short mode              : 2;

		// Indicates should mosaic effect be applied to this sprite.
		unsigned short mosaic            : 1;

		// Indicates which palette does this sprite uses.
		// (0 = 16 color 16 palette bank, 1 = 256 color unique bank)
		unsigned short palette256        : 1;

		// Indicates the shape of each sprite.
		unsigned short shape             : 2;

		/** Attribute 1 */
		// The sprite's X coord on screen (start from left).
		unsigned short x                 : 9;

		// Selects the rotation scale.
		unsigned short transform         : 5;

		// Indicates the object size depending on shape.
		unsigned short size              : 2;

		/** Attribute 2 */
		// Indicates the starting tile's index.
		unsigned short tile              : 10;

		// Indicates the priority relative to BGs.
		unsigned short priority          : 2;

		// Indicates the palette bank to be selected.
		unsigned short palette           : 4;

		/** Attribute 3 */
		// The padding bit fields that is reserved for effect.
		unsigned short effect;
	} bits;

	// Accessing the register as half word.
	struct {
		unsigned short attr0;
		unsigned short attr1;
		unsigned short attr2;
		unsigned short effect;
	} halfwords;
} __gba_sprite_attribute_t;

// The register locations for video registers.
static const int __gba_sprite_maxattributes = 128;
extern volatile __gba_sprite_attribute_t __gba_sprite_attributes[__gba_sprite_maxattributes];

// End of avoid name mangling when compiled in C++ source.
#ifdef __cplusplus
}

// Perform some static assertion (of c++11) to ensure the 
// size of the specified registers.
static_assert(sizeof(__gba_sprite_attribute_t) == 8, 
	"Each sprite attribute should occupy exactly 4 halfwords.");
#endif

// Restore the memory alignment.
#pragma pack(pop)
