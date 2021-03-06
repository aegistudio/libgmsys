/**
 * combios.c - BIOS function common definition
 * @author Haoran Luo
 *
 * Implementation for the combios.h defined in the 
 * include directory. See the header file for usage
 * and documentation details.
 */
#include "combios.h"
#define __val(mac) #mac
#define __tostring(mac) __val(mac)
#define __swi(mac) "swi " __tostring(mac) ";"

#ifdef __bios_arm_svcid_cpufastset

// Implementation for BIOS function cpuFastSet.
asm (	"__bios_arm_cpufastset:"
		__swi(__bios_arm_svcid_cpufastset)
		"bx	lr");

// Implementation for function cpufastfill.
void __bios_arm_cpufastfill(
	void* destinationAddress,
	int word, unsigned int numWords) {

	// This local variable comes into negate a bug when 
	// using GCC ARM compiler with O3 optimization.
	// The optimizer seem not to understand the fact
	// that the first 4 variables are not in stack
	// frame in ARM. And when optimizing this will be
	// a bug. So a volatile variable comes to negate it.
	// @note Haoran Luo
	volatile int stackWord = word;

	__bios_arm_cpufastset((void*)&stackWord,
		destinationAddress,
		numWords | (1 << 24));
}

// Implementation for function cpufastfill.
void __bios_arm_cpufastcopy(
	void* destinationAddress, void* sourceAddress, 
	unsigned int numWords) {

	__bios_arm_cpufastset(sourceAddress,
		destinationAddress, numWords);
}
#endif
