/**
 * @file gbamm.cpp
 * @brief Implementation for gba memory management.
 * @author Haoran Luo
 *
 * Implementation for the gba/mm.h defined in the include directory. See 
 * the header file for usage and documentation details.
 */
#define __gba_mmqualifier __attribute__((weak))
#include "gba/mm.h"
#include "gmlibc/buddy.hpp"
#include "gmlibc/dlmalloc.hpp"
#include <new>
#define TRUE  1
#define FALSE 0

/// @brief Forward the definition of external working RAM's size.
extern "C" int __gba_ewram_size;

/// @brief The generic type information to be used with working RAM.
struct __gba_ewram_info {
	// Buddy allocator part.
	/// Forward the definition of order.
	typedef __gba_order_t orderType;
	
	/// Maximum page order allowed to allocate.
	static constexpr orderType maxPageOrder = 6;
	
	/// The page frame number's type definition.
	typedef unsigned char pfnType;
	
	/// How many bytes does should the bitmap in the buddy system 
	/// allocator occupies.
	static constexpr orderType bitmapTotalSize = 32;
	
	/// The offsets of bitmap for each page order.
	static const pfnType bitmapOrderOffset[maxPageOrder];
	
	/// The shift for a page. Defaultly set to 2048 (1 << 11) bytes.
	static constexpr orderType pageSizeShift = 11;
	
	/// The address type used in the gba's addressing. Should always
	/// be of word size (4 bytes).
	typedef int addressType;
	static_assert(sizeof(void*) == sizeof(int), "Unexpected building "
		"architecture, please validate your building parameters!");
	
	/// Retrieve the size of area in the working memory region.
	static pfnType initialPageFrame() noexcept {
		return (__gba_ewram_size + (1 << pageSizeShift) - 1) >> pageSizeShift;
	}
	
	/// Total number of page frames in working memory.
	static pfnType totalPageFrame() noexcept {
			return 128 - initialPageFrame();
	}
	
	/// The first available page frame for dynamic page allocation.
	static addressType firstPageAddress() {
		return reinterpret_cast<addressType>(0x02000000) 
				+ (initialPageFrame() << pageSizeShift);
	}
	
	/// The page address when it is null value.
	static constexpr addressType nullPageAddress = 0;
    
	/// Shrink page whenever it is possible. (For high page break using buddy).
	static constexpr bool deftHighBreakShrink = true;
	
	// Fine allocator part.
	/// Forward the definition of dynamic allocate size type.
	typedef __gba_size_t allocateSizeType;
	
	/// The definition of each chunk size type.
	typedef unsigned short chunkSizeType;
	
	/// The 8 - 63 byte's allocation request will be passed into fast bin
	/// request.
	static constexpr orderType fastbinMaxOrder = 6;
	
	/// The 64 - 511 byte's allocation request will be passed into small
	/// bin's allocation request. And the 512 - 2039's allocation request
	/// will be passed to large bin's request.
	static constexpr orderType smallbinMaxOrder = 9;
	
	/// Returned when fails to allocate chunk.
	static constexpr addressType nullChunkAddress = 0;
	
	// The memory clearing part.
	static void memzero(char* memory, __gba_size_t size) noexcept {
		// @XXX This function is just a stub, please complete it with gba 
		// specific library function.
		volatile char* vmemory = memory;
		//__gba_memzero(memory, size);
		for(__gba_size_t i = 0; i < size; ++ i) vmemory[i] = 0;
	}
	
	// We can safely assume all pointer values are 0 in our application.
	template<typename pointerType> static void memzptr(pointerType* pointer, 
		const pointerType& zvalue, __gba_size_t numPointer) noexcept {
		
		// __gba_memzero(pointer, numPointer * sizeof(pointerType));
		memzero((char*)pointer, numPointer * sizeof(pointerType));
	}
};

const unsigned char __gba_ewram_info::bitmapOrderOffset[maxPageOrder] __attribute__((weak)) = {0, 128, 64, 32, 16, 8};

// Forward the allocator definitions.
typedef GmOsPageAllocatorBuddy<__gba_ewram_info> pageAllocatorType;
static_assert(sizeof(pageAllocatorType) <= sizeof(__gba_page_allocator_t),
	"The size of page allocator does not fit in with its underlying object.");
typedef GmOsFineAllocatorDlMalloc<__gba_ewram_info, pageAllocatorType> fineAllocatorType;
static_assert(sizeof(fineAllocatorType) <= sizeof(__gba_malloc_allocator_t),
	"The size of malloc allocator does not fit in with its underlying object.");

// The caching pointers.
pageAllocatorType* pageAllocator __attribute__((section(".iwram.data"), weak)) = nullptr;
fineAllocatorType* fineAllocator __attribute__((section(".iwram.data"), weak)) = nullptr;

// Perform page allocator initialization.
__gba_bool_t __gba_pageinit(__gba_page_allocator_t* region) {
	if(pageAllocator != nullptr) return TRUE;
	new ((unsigned char*)region) pageAllocatorType();
	pageAllocator = reinterpret_cast<pageAllocatorType*>(region);
	return TRUE;
}

// Check whether page allocator has initialized.
__gba_bool_t __gba_pagehasinit() {
	return (pageAllocator != nullptr)? TRUE : FALSE;
}

// Allocate page for certain order.
__gba_page_t __gba_pagealloc(__gba_order_t pageOrder) {
	if(!__gba_pagehasinit()) return nullptr;
	return reinterpret_cast<__gba_page_t>(
		pageAllocator -> allocateHighPage(pageOrder));
}

// Deallocate page for certain order.
void __gba_pagefree(__gba_page_t page, __gba_order_t pageOrder) {
	if(!__gba_pagehasinit()) return;
	pageAllocator -> freeHighPage(reinterpret_cast<
		pageAllocatorType::pageType>(page), pageOrder);
}

// Perform malloc allocator initialization.
__gba_bool_t __gba_mallocinit(__gba_malloc_allocator_t* region) {
	if(fineAllocator != nullptr) return TRUE;
	new ((unsigned char*) region) fineAllocatorType(*pageAllocator);
	fineAllocator = reinterpret_cast<fineAllocatorType*>(region);
	return TRUE;
}

// Check whether fine allocator has initialized.
__gba_bool_t __gba_mallochasinit() {
	return (fineAllocator != nullptr)? TRUE : FALSE;
}

// Allocate chunk for certain size.
__gba_chunk_t __gba_malloc(__gba_size_t chunkSize) {
	if(!__gba_mallochasinit()) return nullptr;
	if(chunkSize <= 0) return nullptr;
	return fineAllocator -> allocate(chunkSize);
}

// Free chunk for certain size.
void __gba_free(__gba_chunk_t chunk) {
	if(!__gba_mallochasinit()) return;
	if(chunk == nullptr) return;
	fineAllocator -> deallocate(chunk);
}