#pragma once
/**
 * gba/bios.h - BIOS function definition for GBA.
 * @author Haoran Luo
 *
 * Defines BIOS function that could be used when the
 * targeting machine is GBA (GameBoy Advanced). Some
 * BIOS function is not available in and they will 
 * not be defined.
 */

#define __bios_thumb_svcid_cpufastset 0x0C
#define __bios_arm_svcid_cpufastset 0x0C0000

#include "combios.h"
