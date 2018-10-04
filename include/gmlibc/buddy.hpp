#pragma once
/**
 * @file gmlibc/buddy.hpp
 * @brief Buddy System Page Allocator (Template)
 * @author Haoran Luo
 *
 * This file defines the page allocator based on buddy system, however it is modifed 
 * to fullfil low page and high page allocation requirement. Low page (aka. heap) 
 * allocation requires continguous page, while high page (aka. slab) allocation does
 * not. This allocator is modified to fulfill both allocation requirement as much
 * as possible.
 */

/**
 * The concept of a buddy page allocator's information. The information is hardcoded
 * for each architecture, and will generate concrete class utilizing template 
 * metaprogramming of c++.
 *
 * concept buddyInfo {
 *     // The type of the order.
 *     typedef <orderType> orderType;
 *
 *     // The maximum order of the buddy allocator.
 *     static constexpr orderType maxPageOrder;
 *
 *     // The type of the page frame number.
 *     typedef <pageSizeType> pfnType;
 *
 *     // The size of bitmap that will be used by the allocator.
 *     static constexpr pfnType bitmapTotalSize;
 *
 *     // The offset of each order in bitmap.
 *     static pfnType bitmapOrderOffset[maxPageOrder];
 *
 *     // The page size of the allocator, in the unit of shift.
 *     static constexpr orderType pageSizeShift;
 *
 *     // The type of the physical address type using as integer.
 *     typedef <addressType> addressType;
 *
 *     // The total count of allocatable page frame count.
 *     static pfnType totalPageFrame() noexcept;
 *
 *     // Retrieve the start address of the first page, it might be static and calcualte
 *     // from other external symbols.
 *     static addressType firstPageAddress() noexcept;
 *
 *     // The pointer indicating the null pages.
 *     static const addressType nullPageAddress;
 *
 *     // Helper for setting a continguous memory to zero.
 *     static void memzero(char* memory, size_t size) noexcept;
 *
 *     // Helper for setting a continguous void points to zero, however it is in struct form.
 *     template<typename pointerType>
 *     static void memzptr(pointerType* memory, pointerType zero, size_t numPointers) noexcept;
 *
 *     // Should the allocator perform a high break point shrinking immediately if a 
 *     // high page on top is deallocated. This process might be time consuming and is 
 *     // recommended to turn off if the application is always expect to use not so 
 *     // much page.
 *     static const bool deftHighBreakShrink;
 * };
 */

template<typename buddyInfo>
struct GmOsPageAllocatorBuddy {
	/// Forward template types ahead.
	typedef typename buddyInfo::orderType orderType;
	typedef typename buddyInfo::pfnType pfnType;
	typedef typename buddyInfo::addressType addressType;
	
	/// Represents a buddy page in the memory.
	union GmOsPageBuddy {
		/// The link list nodes in the managed body.
		struct {
			/// Constrain for the free pages.
			/// Assert *prev = this.
			/// Assert next -> prev = &next;
			GmOsPageBuddy *next, **prev;
		} freePage;
	
		/// Padding of the page.
		char padding[1 << buddyInfo::pageSizeShift];		
	};
	static_assert(sizeof(GmOsPageBuddy) == 1 << buddyInfo::pageSizeShift,
			"Invalid page size shift was given.");
	
	/// Forward the definition of the page type.
	typedef GmOsPageBuddy* pageType;
	
	/// Calculate the page frame number of a page. Please notice the page counting is 
	/// reversed, which means the page at the start is totalPageFrame - 1, and the 
	/// page at the end is 0.
	static pfnType pageFrameFor(const pageType page) noexcept {
		pfnType reversePageFrame = (reinterpret_cast<addressType>(page) 
				- buddyInfo::firstPageAddress()) >> buddyInfo::pageSizeShift;
		return buddyInfo::totalPageFrame() - 1 - reversePageFrame;
	}
	
	/// Calculate the page address from a frame.
	static pageType pageFrameFrom(pfnType pfn) noexcept {
		return reinterpret_cast<pageType>(((buddyInfo::totalPageFrame() - 1 - pfn) 
			<< buddyInfo::pageSizeShift) + buddyInfo::firstPageAddress());
	}
	
	/// Calculate offset and index from page frame number.
	static inline void indexFrom(pfnType pfn, orderType order, pfnType& index, pfnType& offset) noexcept {
		pfnType pfnIndex = pfn >> order;
		index  = (buddyInfo::bitmapOrderOffset[order] + pfnIndex) >> 3;
		offset = (buddyInfo::bitmapOrderOffset[order] + pfnIndex) & 0x07;
	}
	
	/// Unlink a page from the free list. Unsetting bitmap's bit is not included here.
	static inline void unlinkPage(pageType page) noexcept {
		*(page -> freePage.prev) = page -> freePage.next;
		if(page -> freePage.next != buddyInfo::nullPageAddress) 
			page -> freePage.next -> freePage.prev = page -> freePage.prev;
	}
	
	/// Add a page to certain order of free list.
	static inline void linkPage(pageType* listHead, pageType page) noexcept {
		page -> freePage.prev = listHead;
		page -> freePage.next = *listHead;
		if(*listHead != buddyInfo::nullPageAddress)
			(*listHead) -> freePage.prev = &(page -> freePage.next);
		*listHead = page;
	}
	
	/// Set the bitmap by index and offset.
	inline void bitmapSet(pfnType index, pfnType offset) noexcept {
		bitmap[index] |= (1 << offset);
	}
	
	/// Clear the bitmap by index and offset.
	inline void bitmapClear(pfnType index, pfnType offset) noexcept {
		char bit = (1 << offset);
		bitmap[index] = (bitmap[index] | bit) ^ bit;
	}
	
	/// Check whether a bitmap bit is set.
	inline bool bitmapHas(pfnType index, pfnType offset) const noexcept {
		return (bitmap[index] & (1 << offset)) != 0;
	}
	
	/// The break point of the heap / low page allocation.
	pfnType lpbrk;
	
	/// The break point of the slab / high page allocation.
	pfnType hpbrk;
	
	/// The list of free pages in different orders. Please notice the page of higher 
	/// address always come earlier in the free page list. Should all be initially
	/// null page pointer.
	pageType freePageList[buddyInfo::maxPageOrder];
	
	/// The bitmap recording the pages status. If a page is inside free list, it will be 
	/// marked 1. This field MUST be initially 0, and it should be cleared somewhere.
	char bitmap[buddyInfo::bitmapTotalSize];
	static_assert(sizeof(char) == 1, "Invalid char type on building platform.");
	
	/// Perform high page shrinking, which will attempt to lookup the high page break 
	/// point and attempt to shrink.
	void shrinkHighPage() noexcept {
		bool hasPageShrinked;
		do {
			hasPageShrinked = false;
			
			// Scan every possible order number.
			for(orderType order = 0; (order < buddyInfo::maxPageOrder) 
					&& ((1 << order) <= hpbrk); ++ order) {

				// Calculate page frame information.
				pfnType pfn = hpbrk - (1 << order);
				pfnType index, offset;
				indexFrom(pfn, order, index, offset);
				
				// Check whether it is a page to shrink.
				if(bitmapHas(index, offset)) {
					// Remove the page from free list and unmask the bitmap.
					pageType page = pageFrameFrom(pfn);
					unlinkPage(page);
					bitmapClear(index, offset);
					
					// Update the high page break value.
					hpbrk = pfn;
					hasPageShrinked = true;
				}
				
				// Immediately start next loop if so.
				if(hasPageShrinked) break;
			}
		} while(hasPageShrinked);
	}
	
	/// Return a high page back to the allocator.
	void freeHighPage(pageType page, orderType order) noexcept {
		if(page == (pageType)buddyInfo::nullPageAddress) return;
		pfnType pfnCurrent = pageFrameFor(page);
		
		// Perform iterative merging of buddy page algorithm. Please notice that the 
		// page is currently not inside a free list (However its buddy will be).
		for(; order < buddyInfo::maxPageOrder - 1; ++ order) {
			// Retrieve the buddy of current page frame number.
			pfnType pfnBuddy = pfnCurrent ^ (1 << order);
			pfnType buddyIndex, buddyOffset;
			indexFrom(pfnBuddy, order, buddyIndex, buddyOffset);
			
			// The buddy of current page is free too. Unlink the buddy from free list 
			// and make a new buddy index.
			if(bitmapHas(buddyIndex, buddyOffset)) {
				// Remove the buddy page from its free list.
				pageType buddyPage = pageFrameFrom(pfnBuddy);
				unlinkPage(buddyPage);
				bitmapClear(buddyIndex, buddyOffset);
				
				// Update the current page frame number.
				pfnCurrent = pfnCurrent < pfnBuddy? pfnCurrent : pfnBuddy;
			}
			else break;	// End up with no more buddy to merge.
		}
		
		// The page is top page, so just perform shrinking.
		if(pfnCurrent + (1 << order) == hpbrk) {
			hpbrk = pfnCurrent;
			if(buddyInfo::deftHighBreakShrink) shrinkHighPage();
		}
		
		// Add specified page to corresponding free list.
		else {
			pageType currentPage = pageFrameFrom(pfnCurrent);
			
			// Retrieve the current page frame number.
			pfnType currentIndex, currentOffset;
			indexFrom(pfnCurrent, order, currentIndex, currentOffset);
			
			// Add the page to corresponding free list.
			bitmapSet(currentIndex, currentOffset);
			linkPage(&freePageList[order], currentPage);
		}
	}
	
	/// Allocate a high page from the allocator.
	pageType allocateHighPage(orderType order) noexcept {
		if(order >= buddyInfo::maxPageOrder) 	// Cannot allocate.
			return (pageType)buddyInfo::nullPageAddress;
		
		// Remove an available page from the free list.
		if(freePageList[order] != (pageType)buddyInfo::nullPageAddress) {
			pageType resultPage = freePageList[order];
			
			// Unset bitmap bit.
			pfnType pfnResult = pageFrameFor(resultPage);
			pfnType resultIndex, resultOffset;
			indexFrom(pfnResult, order, resultIndex, resultOffset);
			
			// Unlink current page.
			bitmapClear(resultIndex, resultOffset);
			unlinkPage(resultPage);
			
			return resultPage;
		}
		else {
			// Increase up to the available order.
			orderType availableOrder = order + 1;
			for(; availableOrder < buddyInfo::maxPageOrder && 
				freePageList[order] == (pageType)buddyInfo::nullPageAddress; 
				++ availableOrder);
			
			if(availableOrder < buddyInfo::maxPageOrder) {
				// Luckily we've found one, remove the page from free list first.
				pageType victimPage = freePageList[availableOrder];
				pfnType pfnVictim = pageFrameFor(victimPage);
				pfnType victimIndex, victimOffset;
				indexFrom(pfnVictim, availableOrder, victimIndex, victimOffset);
				bitmapClear(victimIndex, victimOffset);
				unlinkPage(victimPage);
				
				// Recursively split the page down, till the order.
				do {
					-- availableOrder;
					
					// Calculate the page to split.
					pfnType pfnSplit = pfnVictim + (1 << availableOrder);
					pfnType splitIndex, splitOffset;
					indexFrom(pfnSplit, availableOrder, splitIndex, splitOffset);
					pageType splitPage = pageFrameFrom(pfnSplit);
					
					// Add the splitted page to the free list.
					bitmapSet(splitIndex, splitOffset);
					linkPage(&freePageList[availableOrder], splitPage);
				} while(availableOrder != order);
				
				// The splitted page will be returned.
				return victimPage;
			}
			else {
				// We have to increase the hpbrk, by find the last availabe page.
				pfnType pfnNew = ((hpbrk + ((1 << order) - 1)) >> order) << order;
				pfnType newHpbrk = pfnNew + (1 << order);
				
				// Check whether more page can be allocated.
				if(buddyInfo::totalPageFrame() < lpbrk + newHpbrk) 
					return (pageType)buddyInfo::nullPageAddress;
				
				// Add some free page between the old hpbrk and the new page frame.
				pfnType pfnSplit = pfnNew;
				for(orderType orderSplit = order; orderSplit > 0; -- orderSplit) {
					pfnType pfnDecrement = (1 << (orderSplit - 1));
					if(pfnSplit < hpbrk + pfnDecrement) continue;
					pfnSplit = pfnSplit - pfnDecrement;
					
					// Calculate the page to split.
					pfnType splitIndex, splitOffset;
					indexFrom(pfnSplit, orderSplit - 1, splitIndex, splitOffset);
					pageType splitPage = pageFrameFrom(pfnSplit);
					
					// Add the splitted page to the free list.
					bitmapSet(splitIndex, splitOffset);
					linkPage(&freePageList[orderSplit - 1], splitPage);
				}
				
				// Update the high break and return.
				hpbrk = newHpbrk;
				return pageFrameFrom(pfnNew);
			}
		}
	}
	
	/// Retrieve the current low break point top page.
	pageType lowPageBreak() const noexcept {
		if(lpbrk == 0) return (pageType)buddyInfo::nullPageAddress;
		else return reinterpret_cast<pageType>(((lpbrk - 1) 
			<< buddyInfo::pageSizeShift) + buddyInfo::firstPageAddress());
	}
	
	/// Increase the low page break point from the allocator. If the page increment 
	/// has succeed, the true will be returned and low break point will be changed, 
	/// otherwise false will be returned and nothing is changed.
	bool allocateLowPage(pfnType pageCount) noexcept {
		pfnType newLpbrk = lpbrk + pageCount;
		
		if(buddyInfo::totalPageFrame() < newLpbrk + hpbrk) return false;
		lpbrk = newLpbrk; return true;
	};
	
	/// Decrease the low page break point from the allocator.
	bool freeLowPage(pfnType numFree) noexcept {
		if(lpbrk >= numFree) lpbrk = lpbrk - numFree;
		else lpbrk = 0;
	}
	
	/// Initialize the buddy info structure.
	GmOsPageAllocatorBuddy() noexcept: lpbrk(0), hpbrk(0) {
		buddyInfo::memzptr(freePageList, 
			(pageType)buddyInfo::nullPageAddress, buddyInfo::maxPageOrder);
		buddyInfo::memzero(bitmap, buddyInfo::bitmapTotalSize);
	}
};