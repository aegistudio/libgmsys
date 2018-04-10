#pragma once
/**
 * combios.h - BIOS function common definition
 * @author Haoran Luo
 *
 * Most BIOS function wrapper using inline assembly,
 * so that libraries and users could utilize these them
 * easily and correctly.
 *
 * Whether a BIOS wrapper function could output depends
 * on whether corresponding macro is set. The macro 
 * defines the service number while using SWI instruction.
 *
 * For ARM instruction, the macro should be named as
 * __bios_svcid_arm_*.
 *
 * @see http://problemkaputt.de/gbatek.htm#biosfunctions
 */

// Avoid name mangling in C++.
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __bios_arm_svcid_cpufastset

/**
 * Fast-set function provided by the BIOS. Could be used
 * to copy or fill a region of memory quickly.
 * This function could only be used to copy or fill words.
 *
 * @param[in] sourceAddress the start of the source address
 * which will be refered as data used to copy or fill.
 * @param[in] destinationAddress the start of the destination
 * address which will be filled with the data.
 * @param[in[ wordAmountMode the lower 24-bit indicates the
 * size of word to copy, the 25-th bit indicates whether to
 * copy (when 0) or fill (when 1).
 */
void __bios_arm_cpufastset(
	void* sourceAddress,
	void* destinationAddress,
	int wordAmountMode);

/**
 * Utilize the fast-set function to fill words.
 */
void __bios_arm_cpufastfill(void* destinationAddress, 
	int word, unsigned int numWords);

/**
 * Utilize the fast-set function to copy words.
 */
void __bios_arm_cpufastcopy(void* destinationAddress,
	void* sourceAddress, unsigned int numWords);

#endif

// End of avoid name mangling in C++.
#ifdef __cplusplus
}
#endif
