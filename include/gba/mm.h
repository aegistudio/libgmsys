#pragma once
/**
 * @file gba/mm.h
 * @brief Working RAM Memory Management
 * @author Haoran Luo
 *
 * Defines default memory management inside working memory. 
 * The memory management includes page allocation,
 * dynamic allocation and slab allocation. You can define 
 * your own memory allocation method.
 *
 * All symbols are defined weak, but the underlying implementation
 * will be strongly coupled. So if you want to define your own 
 * implementation, you'll have to rewrite ALL these symbols.
 */
 
// Begin of making c symbol.
#ifdef __cplusplus
extern "C" {
#endif
 
/// The order type of gba. Which is a relative small value 
/// and will always be mapped to a power-of-2 value.
typedef unsigned char __gba_order_t;
typedef unsigned int __gba_size_t;
typedef void* __gba_page_t;
typedef void* __gba_chunk_t;
typedef unsigned char __gba_bool_t;

/// The eye-candy for defining allocator handles in some region.
typedef struct { int data[15]; } __gba_page_allocator_t;
typedef struct { int data[30]; } __gba_malloc_allocator_t;

/// Could be used to define symbol's trait.
#ifndef __gba_mmqualifier
#define __gba_mmqualifier
#endif

/**
 * @brief Initialize the gba page memory allocation.
 *
 * The specified memory and size defines the region to initialize
 * the page allocation system into. Upon success, the passed 
 * pointer will be cached so that other allocation methods 
 * could use it. It is recommended to initialize the allocator 
 * on user stack or internal working ram region.
 *
 * @param region the pointer to region to initialize allocator.
 * @return whetehr allocation has succeed. Or whether the page
 * allocator has already been initialized.
 */
__gba_bool_t __gba_pageinit(__gba_page_allocator_t* allocator) __gba_mmqualifier;
 
/**
 * @brief Check whether the page allocator has initialized.
 * If initialized, return true, else false will be returned.
 *
 * @return whether the page allocation system has allocated, 
 * if so, return true, else return false.
 */
__gba_bool_t __gba_pagehasinit() __gba_mmqualifier;

/**
 * @brief Allocate memory in page unit.
 *
 * The page size is defined by the underlying implementaion,
 * and the default implementaion for page allocation specifies
 * the page size to be 2048.
 *
 * @param pageOrder request to allocate (pageSize << pageOrder)
 * byte of memory.
 * @return the allocated page if success, or nullptr if failed.
 */
__gba_page_t __gba_pagealloc(__gba_order_t pageOrder) __gba_mmqualifier;

/**
 * @brief Deallocate memory in page unit.
 *
 * @param page the allocated page via page alloc method.
 * @param pageOrder the order of page while invoking the page 
 * allocate method. 
 */
void __gba_pagefree(__gba_page_t page, __gba_order_t pageOrder) __gba_mmqualifier;

/**
 * @brief Initialize the dynamic allocation system.
 *
 * This function require page allocator to be initialized priorly.
 * If not initialized, false will be returned.
 */
__gba_bool_t __gba_mallocinit(__gba_malloc_allocator_t* allocator) __gba_mmqualifier;

/**
 * @brief Check whether the dynamic allocation system has initialized.
 */
__gba_bool_t __gba_mallochasinit() __gba_mmqualifier;

/**
 * @brief Allocate memory as chunk.
 *
 * @param chunkSize request to allocate (chunkSize) byte of memory.
 * @return the allocated page if success, or nullptr if failed.
 */
__gba_chunk_t __gba_malloc(__gba_size_t chunkSize) __gba_mmqualifier;

/**
 * @brief Deallocate memory from chunk.
 *
 * @param chunk the allocated chunk via chunk alloc method.
 */
void __gba_free(__gba_chunk_t chunk) __gba_mmqualifier;

// End of enforcing c symbol.
#ifdef __cplusplus
}
#endif