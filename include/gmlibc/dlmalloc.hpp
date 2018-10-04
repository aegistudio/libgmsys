#pragma once
/**
 * @file gmlibc/dlmalloc.hpp
 * @brief Doug Lea's Malloc Fine (Heap) Allocator
 * @author Haoran Luo
 *
 * This allocator is based on the work of Doug Lea's malloc allocator, whose 
 * variants ptmalloc2 is used in pthread's malloc and glibc's malloc.
 *
 * The allocator divides a memory allocate request into 4 parts:
 * - FastBin: For memory request who are really small, the last memory returned 
 * will be popped from the bin. (LIFO) All Fast bin chunks are equally sized.
 * - SmallBin: For memory request that are relative small, find the first page 
 * (returned) that fits in with the allocator best and potentially split that
 * page into two parts. (Pages are sorted but not ordered).
 * - LargeBin: For memory request that are relative big, find the best fit chunk, 
 * and potentially split that page into two parts.
 * - PageChunk: For memory request that are of page size, directly allocate the 
 * page with the page allocator.
 *
 * All chunks posses this structure.
 * +----------------------+
 * | PreviousChunkSize    |
 * +--------------+---+---+
 * | ChunkSize    | M | P | (M = Allocated with page allocator, P = Previous In Use)
 * +--------------+---+---+
 * | PreviousChunkPointer | <-- malloc() and free() with such pointer.
 * +----------------------+
 * | NextChunkPointer     | (Circular link list pointing to next free page).
 * +----------------------+
 * | PreviousSizePointer  | 
 * +----------------------+
 * | NextSizePointer      | (Only used in large bin and head node, to point to the 
 * +----------------------+ chunk strip of next size).
 *
 * The page chunk threshold will be (pageSize - 2 * (sizeType)), this ensure at most
 * one low page will grow every malloc call.
 */

template<typename dlInfo, typename pageAllocatorType>
struct GmOsFineAllocatorDlMalloc {
	// Forward the information definition.
	typedef typename dlInfo::allocateSizeType allocateSizeType;
	typedef typename dlInfo::chunkSizeType chunkSizeType;
	typedef typename dlInfo::addressType addressType;
	typedef typename dlInfo::pfnType pfnType;
	typedef typename pageAllocatorType::pageType pageType;
	typedef typename dlInfo::orderType orderType;
	
	/// When the chunk is small, maintaining the node.
	struct GmOsChunkNodeSmall {
		/// The pointers of the doubly link node.
		GmOsChunkNodeSmall *previous, *next;
		
		/// Unlink the node from its context, assuming the node is in small bin.
		inline void unlinkChunk() noexcept {
			if(previous != (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress) previous -> next = next;
			if(next != (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress) next -> previous = previous;
			next = previous = (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress;
		}
		
		/// Insert a node right after the node.
		inline void insertSmallAfter(GmOsChunkNodeSmall* small) noexcept {
			small -> previous = this;
			small -> next = next;
			if(next != (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress) next -> previous = small;
			next = small;
		}
		
		/// Insert a node right before the node.
		inline void insertSmallBefore(GmOsChunkNodeSmall* small) noexcept {
			small -> previous = previous;
			small -> next = this;
			if(previous != (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress) previous -> next = small;
			previous = small;
		}
	};
	
	/// When the chunk is large, maintaining the node.
	struct GmOsChunkNodeLarge : public GmOsChunkNodeSmall {
		/// Only the first node of a size will possess this field, other node will just 
		/// have either node set to null address.
		GmOsChunkNodeLarge *previousSize, *nextSize;
		
		/// Unlink the node from its context, assuming the node is in large bin.
		inline void unlinkChunk() noexcept {
			// Perform peer node checking.
			if(	nextSize == GmOsChunkNodeSmall::next || 
				GmOsChunkNodeSmall::next == (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress) {
				// There's no peer node after this node.
				if(previousSize != (GmOsChunkNodeLarge*)dlInfo::nullChunkAddress) 
					previousSize -> nextSize = nextSize;
				
				if(nextSize != (GmOsChunkNodeLarge*)dlInfo::nullChunkAddress) 
					nextSize -> previousSize = previousSize;				
			}
			else {
				// Elevate the peer node of current node.
				((GmOsChunkNodeLarge*)GmOsChunkNodeSmall::next) -> previousSize = previousSize;
				((GmOsChunkNodeLarge*)GmOsChunkNodeSmall::next) -> nextSize = nextSize;
			}
			
			// Unlink the size node.
			previousSize = nextSize = (GmOsChunkNodeLarge*)dlInfo::nullChunkAddress;
			
			
			// Perform normal unlinking just like the small nodes.
			GmOsChunkNodeSmall::unlinkChunk();
		}
	};
	
	/// Definition for a real chunk allocated.
	struct GmOsFineChunkDlMalloc {
		/// The size ledger of previous chunk, undefined if the previous chunk is not in use.
		chunkSizeType previousSize;

		/// The size of current chunk, the lower bits are preserved. for other use.
		chunkSizeType chunkSize;
		
		/// The chunk node union, depending on which bin is the node inside.
		union {
			/// When the node is inside fast, small or unsorted bin, this node will 
			/// be made available.
			GmOsChunkNodeSmall small;
			
			/// When the node is inside large bin, this node will be made available.
			GmOsChunkNodeLarge large;
			
			/// Returned by the allocator as the memory.
			char memory[1];
		} payload;
		
		/// The const expressions.
		static constexpr chunkSizeType bitPreviousInUse = 0x01;
		static constexpr chunkSizeType bitPageAllocated = 0x02;
		static constexpr addressType payloadOffset = reinterpret_cast<addressType>(
				&((const GmOsFineChunkDlMalloc*)0x00) -> payload);
		
		/// Retrieve the in-use status of previous chunk.
		inline bool previousInUse() const noexcept 
			{ return (chunkSize & bitPreviousInUse) == bitPreviousInUse; }
			
		/// Retrieve the chunk status of current chunk.
		inline bool isPageAllocated() const noexcept 
			{ return (chunkSize & bitPageAllocated) == bitPageAllocated; }
		
		/// Set the lower bit flags.
		inline void setFlag(chunkSizeType flag) noexcept 
			{ chunkSize = chunkSize | flag; }
			
		/// Clear the lower bit flag.
		inline void clearFlag(chunkSizeType flag) noexcept 
			{ chunkSize = (chunkSize | flag) ^ flag; }
		
		/// Retrieve the size of current chunk.
		inline chunkSizeType size() const noexcept 
			{ return ((chunkSize | 0x03) ^ 0x03); }
			
		/// Estimate the size of a chunk.
		static inline chunkSizeType physicalSize(chunkSizeType sz) noexcept
			{ return sz + payloadOffset; }
		
		/// Retrieve the physical size of current chunk.
		inline chunkSizeType physicalSize() const noexcept
			{ return physicalSize(size()); }
		
		/// Forward to fetch next physical chunk.
		inline GmOsFineChunkDlMalloc* nextPhysicalChunk() const noexcept 
			{ return reinterpret_cast<GmOsFineChunkDlMalloc*>(
				reinterpret_cast<addressType>(this) + physicalSize()); }
				
		/// Retrieve whether current chunk is in use.
		inline bool currentInUse() const noexcept 
			{	return nextPhysicalChunk() -> previousInUse(); }
			
		/// Update the page size without erasing the status of the low bits.
		inline void updateSize(chunkSizeType newSize) noexcept 
			{	chunkSize = ((chunkSize & 0x03) | ((newSize | 0x03) ^ 0x03));	}
			
		/// Retrieve the previous physical chunk of current chunk.
		inline GmOsFineChunkDlMalloc* previousPhysicalChunk() const noexcept {
			return reinterpret_cast<GmOsFineChunkDlMalloc*>(
				reinterpret_cast<addressType>(this) - physicalSize(previousSize));
		}
		
		/// See whether a chunk suits the size requirement of large chunk.
		inline bool isLargeChunkSize() const noexcept {
			chunkSizeType sizeValue = size();
			return (sizeValue >= (1 << dlInfo::smallbinMaxOrder)) 
				&& (sizeValue <  (1 << dlInfo::pageSizeShift));
		}
	};
	typedef GmOsFineChunkDlMalloc* chunkType;
	
	/// Retrieve the chunk corresponding to a void (user) pointer.
	static inline chunkType chunkOf(void* memory) noexcept {
		if(memory == (dlInfo::nullChunkAddress)) 
			return (chunkType)dlInfo::nullChunkAddress;
		return reinterpret_cast<chunkType>(reinterpret_cast<addressType>
			(memory) - GmOsFineChunkDlMalloc::payloadOffset);
	}
	
	/// The page allocator which the fine allocator relies on.
	pageAllocatorType& pageAllocator;
	
	/// Pointing to the top chunk, which ought to be initialized before any allocation.
	chunkType topChunk;
	
	/// The fastbin, small bin, large bin and unsorted bin.
	/// Please notice that 0, 1, 2 of the fast bin is not used. Depending on the system
	/// word length, the 3 might also be not used.
	GmOsChunkNodeSmall fast[dlInfo::fastbinMaxOrder];
	GmOsChunkNodeSmall small[dlInfo::smallbinMaxOrder - dlInfo::fastbinMaxOrder];
	GmOsChunkNodeLarge large[dlInfo::pageSizeShift - dlInfo::smallbinMaxOrder];
	GmOsChunkNodeSmall unsorted;
	
	/// Constructor for the malloc class.
	GmOsFineAllocatorDlMalloc(pageAllocatorType& pageAllocator) noexcept: 
		pageAllocator(pageAllocator), 
		topChunk((chunkType)dlInfo::nullChunkAddress) {
		
		// Make the fast bin a stack.
		for(orderType i = 0; i < dlInfo::fastbinMaxOrder; ++ i)
			fast[i].previous = fast[i].next = (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress;
		
		// Make both of them a circular link list (queue).
		for(orderType i = dlInfo::fastbinMaxOrder; i < dlInfo::smallbinMaxOrder; ++ i)
			small[i - dlInfo::fastbinMaxOrder].previous = small[i - dlInfo::fastbinMaxOrder].next 
				= (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress;
		for(orderType i = dlInfo::smallbinMaxOrder; i < dlInfo::pageSizeShift; ++ i)
			large[i - dlInfo::smallbinMaxOrder].previous 
				= large[i - dlInfo::smallbinMaxOrder].next 
				= large[i - dlInfo::smallbinMaxOrder].previousSize
				= large[i - dlInfo::smallbinMaxOrder].nextSize
				= (GmOsChunkNodeLarge*)dlInfo::nullChunkAddress;
				
		// Make the unsorted bin a stack.
		unsorted.previous = unsorted.next = (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress;
	}
	
	/// Allocate top chunk on request.
	bool topChunkInitialize() noexcept {
		/// The top chunk is already available under such case.
		if(topChunk != (chunkType)dlInfo::nullChunkAddress) return true;
		
		/// Attempt to allocate a top chunk.
		if(!pageAllocator.allocateLowPage(1)) return false;
		
		/// Initialize top chunk (of one page size).
		topChunk = reinterpret_cast<chunkType>(pageAllocator.lowPageBreak());
		topChunk -> chunkSize = (1 << dlInfo::pageSizeShift) - (sizeof(chunkSizeType) << 1);
		topChunk -> setFlag(GmOsFineChunkDlMalloc::bitPreviousInUse);
		return true;
	}
	
	/// Increase the top chunk by a page on request. If the increment can process,
	/// true will be returned, otherwise false will be returned and the allocator 
	/// should return null chunk (indicating allocation failure) under such situation.
	bool increaseTopChunk() noexcept {
		if(!topChunkInitialize()) return false;
		
		/// Update the chunk size by one page.
		if(!pageAllocator.allocateLowPage(1)) return false;
		topChunk -> updateSize(topChunk -> size() + (1 << dlInfo::pageSizeShift));
		return true;
	}
	
	/// Shrink the top chunk after deallocation besides top chunk. Under which case the 
	/// top chunk will go across one or more page size.
	void shrinkTopChunk() noexcept {
		if(!topChunkInitialize()) return;	// No need to shrink this case.
		
		/// Get the last un-shrinkable word's address and page frame.
		addressType chunkSizeAddress = reinterpret_cast<addressType>(&(topChunk -> chunkSize));
		pfnType pfnChunkSize = chunkSizeAddress >> dlInfo::pageSizeShift;
		pfnType pfnLowBreak = reinterpret_cast<addressType>(
			pageAllocator.lowPageBreak()) >> dlInfo::pageSizeShift;
		
		// Return the page to the page allocator and shrink the size.
		pageAllocator.freeLowPage(pfnLowBreak - pfnChunkSize);
		topChunk -> updateSize(topChunk -> size() - 
			((pfnLowBreak - pfnChunkSize) << dlInfo::pageSizeShift));
	}
		
	/// Insert the chunk into proper bin. The chunk is assumed not to link with any bin node, but will be 
	/// linked after arranging. (Will always find some where to insert the chunk).
	void arrangeChunk(chunkType chunk) noexcept {
		chunkSizeType size = chunk -> size();
		
		// Judge by the size.
		if(size >= sizeof(GmOsChunkNodeSmall)) {
			
			// Insert the chunk into the fast bin.
			if(size < (1 << dlInfo::fastbinMaxOrder)) {
				orderType fastOrder = 2;
				for(; ((1 << fastOrder) < sizeof(GmOsChunkNodeSmall)) 
							|| (1 << (fastOrder + 1)) < size; ++ fastOrder);
				fast[fastOrder].insertSmallAfter(&(chunk -> payload.small));
				return;
			}
			
			// Insert the chunk into small bin.
			else if(size < (1 << dlInfo::smallbinMaxOrder)) {
				orderType smallOrder = dlInfo::fastbinMaxOrder;
				for(; (1 << (smallOrder + 1)) < size; ++ smallOrder);
				GmOsChunkNodeSmall* nodePrevious = &small[smallOrder - dlInfo::fastbinMaxOrder];
				GmOsChunkNodeSmall* nodeCurrent = nodePrevious -> next;
				
				// See whether the chunk might be inserted in the center.
				while(nodeCurrent != (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress) {
					chunkType chunkCurrent = chunkOf(nodeCurrent);
					
					// Insert the chunk right before this chunk.
					if(chunkCurrent -> size() >= size) {
						nodeCurrent -> insertSmallBefore(&(chunk -> payload.small));
						return;
					}
					
					// Just go forward.
					else {
						nodePrevious = nodeCurrent;
						nodeCurrent = nodeCurrent -> next;
					}
				}
					
				// The chunk will be inserted at last.
				nodePrevious -> insertSmallAfter(&(chunk -> payload.small));
				return;
			}
			
			// Insert the chunk into large bin.
			else if(size < (1 << dlInfo::pageSizeShift)) {
				orderType largeOrder = dlInfo::smallbinMaxOrder;
				for(; (1 << (largeOrder + 1)) < size; ++ largeOrder);
				GmOsChunkNodeLarge* nodePrevious = &large[largeOrder - dlInfo::smallbinMaxOrder];
				GmOsChunkNodeLarge* nodeCurrent = nodePrevious -> nextSize;
				GmOsChunkNodeLarge* nodeChunk = &(chunk -> payload.large);
				
				// See whether the chunk might be inserted in the center.
				while(nodeCurrent != (GmOsChunkNodeLarge*)dlInfo::nullChunkAddress) {
					chunkType chunkCurrent = chunkOf(nodeCurrent);
					
					// Insert the chunk as a standalone chunk of this chunk.
					if(chunkCurrent -> size() > size) {
						nodeCurrent -> insertSmallBefore(nodeChunk);
						chunk -> payload.large.nextSize = nodeCurrent;
						chunk -> payload.large.previousSize = nodePrevious;
						nodePrevious -> nextSize = nodeChunk;
						nodeCurrent -> previousSize = nodeChunk;
						return;
					}
					
					// Insert the chunk as a buddy of this chunk.
					else if(chunkCurrent -> size() == size) {
						nodeCurrent -> insertSmallAfter(nodeChunk);
						nodeChunk -> nextSize = nodeChunk -> previousSize 
							= (GmOsChunkNodeLarge*)dlInfo::nullChunkAddress;
						return;
					}
					
					// Just go forward.
					else {
						nodePrevious = nodeCurrent;
						nodeCurrent = nodeCurrent -> nextSize;
					}
				}
				
				// The chunk will be inserted at last.
				nodePrevious -> insertSmallAfter(nodeChunk);
				nodeChunk -> previousSize = nodePrevious;
				nodePrevious -> nextSize = nodeChunk;
				nodeChunk -> nextSize = (GmOsChunkNodeLarge*)dlInfo::nullChunkAddress;
				return;
			}
		}
		
		// Well, we have to insert this chunk back to unsorted bin.
		unsorted.insertSmallAfter(&(chunk -> payload.small));
	}
	
	/// Safely unlink a chunk based on its size trait.
	static inline void safelyUnlinkChunk(chunkType chunk) noexcept {
		if(chunk -> isLargeChunkSize()) 
			chunk -> payload.large.unlinkChunk();
		else chunk -> payload.small.unlinkChunk();
	}
	
	/// Split some bytes of the chunk. Please notice that the chunk should already have been taken off
	/// the free list. (You can safely ensure the allocator size should be larger than the chunk size).
	void* splitUseChunk(chunkType chunk, allocateSizeType size) noexcept {
		//  Current V
		// (Before) | Previous | ChunkSize | -------------------------------------| ChunkSize    |
		// (After)  | Previous | Size      | XXXXXXX | Size | RemainedSize | -----| RemainedSize |
		//  Current A                    SplittedOff A
		// Where: Size + RemainedSize + 2 * sizeof(chunkSizeType) == ChunkSize.
		chunkSizeType availableSize = chunk -> size() - size;
		availableSize = (availableSize | 0x03) ^ 0x03;	// Perform size floor alignment.
		
		// The remained size fits in with the least size of the split off limit.
		if(GmOsFineChunkDlMalloc::physicalSize(sizeof(GmOsChunkNodeSmall)) <= availableSize) {
			chunkSizeType remainedSize = 0;
			
			// We can safely make it a small or large chunk, or even more.
			if(availableSize >= GmOsFineChunkDlMalloc::physicalSize(1 << dlInfo::fastbinMaxOrder))
				remainedSize = remainedSize - (sizeof(chunkSizeType) << 1);
			
			// We will have to search for a safe size that we can make it a chunk.
			else {
				remainedSize = 1 << (dlInfo::fastbinMaxOrder - 1);
				while(GmOsFineChunkDlMalloc::physicalSize(remainedSize) > availableSize 
					&& remainedSize > 0) remainedSize >>= 1;
			}
			
			// Perform chunk splitting.
			if(remainedSize > 0) {
				// Locate the splitted chunk.
				chunkType nextChunk = chunk -> nextPhysicalChunk();
				nextChunk -> previousSize = remainedSize;
				chunkType splittedChunk = nextChunk -> previousPhysicalChunk();
				
				// Prepare data for the splitted chunk.
				splittedChunk -> chunkSize = remainedSize;
				chunkSizeType updatedSize = chunk -> size() - splittedChunk -> physicalSize();
				splittedChunk -> previousSize = updatedSize;
				chunk -> updateSize(updatedSize);
				
				// Insert the splitted chunk back to the allocator.
				arrangeChunk(splittedChunk);
			}
		}
		
		// Return the current chunk to the user.
		chunk -> nextPhysicalChunk() -> setFlag(GmOsFineChunkDlMalloc::bitPreviousInUse);
		return chunk -> payload.memory;
	}
	
	/// Coalsce chunk backward, until the first chunk whose previous chunk is in use was hit.
	/// All chunks other than the specified chunk will be unlinked from their free list upon 
	/// coalsce, with their size updated. The size chunk specified will also be updated meantime.
	/// If no chunk before could be coalsced, the null chunk will be returned.
	/// (Please notice the control bits are omitted).
	///          Empty V                    BeforeCurrent V                        Current V
	/// (Before)       | Previous | ChunkSize   | --------| ChunkSize | ChunkSize' | ----- | ChunkSize'  |
	/// (After)        | Previous | ChunkSize'' | ---------------------------------------- | ChunkSize'' |
	///  BeforeCurrent A                                                           Current A
	/// Where, ChunkSize'' = ChunkSize + ChunkSize' + 2 * sizeof(chunkSizeType).
	static chunkType coalsceChunkBefore(chunkType chunk) noexcept {
		// No node will be unlinked under such situation.
		if(chunk -> previousInUse()) return (chunkType)dlInfo::nullChunkAddress;
		
		// Search for the very node whose previous is in use.
		chunkType result = chunk -> previousPhysicalChunk();
		while(!result -> previousInUse()) {
			// Remove the chunk from its link context.
			safelyUnlinkChunk(result);
			
			// Update the corresponding sizes.
			chunkType newResult = result -> previousPhysicalChunk();
			chunkSizeType newResultSize = result -> previousSize + result -> physicalSize();
			newResult -> updateSize(newResultSize);
			chunk -> previousSize = newResultSize;
			
			// Continue to search forward.
			result = newResult;
		}
		
		// Don't forget to unlink the result.
		safelyUnlinkChunk(result);
		
		// Return the final result.
		return result;
	}
	
	/// Coalsce chunks forward, until the first chunk which is in use was hit.
	/// The chunk may never be the top chunk. Because if there's a chunk that is before the top 
	/// chunk and returned to the allocator, the coalsce operation will erase such chunk.
	/// The coalsed chunk after will be automatically unlinked from the free list.
	/// (Please notice the control bits are omitted).
	///        Current V                     AfterCurrent V
	/// (Before)       | Previous | ChunkSize   | --------| ChunkSize | ChunkSize' | ----- | ChunkSize'  |
	/// (After)        | Previous | ChunkSize'' | ---------------------------------------- | ChunkSize'' |
	///        Current A                                                      AfterCurrent A
	/// Where, ChunkSize'' = ChunkSize + ChunkSize' + 2 * sizeof(chunkSizeType).
	static inline void coalsceChunkAfter(chunkType chunk) noexcept {
		chunkType visitingChunk = chunk -> nextPhysicalChunk();
		while(!visitingChunk -> currentInUse()) {
			// assert(visitingChunk != topChunk, "Invariant violation in dlmalloc allocator!");
			// Unlink the visiting node.
			safelyUnlinkChunk(visitingChunk);
			
			// Update the size in all possible positions.
			chunkSizeType newChunkSize = visitingChunk -> previousSize + visitingChunk -> physicalSize();
			chunkType nextVisitingChunk = visitingChunk -> nextPhysicalChunk();
			chunk -> updateSize(newChunkSize);
			nextVisitingChunk -> previousSize = newChunkSize;
			
			// Update the visiting cursor.
			visitingChunk = nextVisitingChunk;
		}
	}
	
	/// Coalsce chunks into a bigger one. The final coalscing result will be relinked. The chunk 
	/// being coalsced is assumed to be inside the unsorted bin.
	static inline chunkType coalsceChunkUnsorted(chunkType chunk) noexcept {
		chunkType result = chunk;
		chunkType coalscedPrevious = coalsceChunkBefore(chunk);
		
		// Some chunk before the specified chunk was coalsced.
		if(coalscedPrevious != (chunkType)dlInfo::nullChunkAddress) {
			result = coalscedPrevious;
			
			// Replace the chunk with the coalsced result chunk.
			result -> payload.small.next = chunk -> payload.small.next;
			chunkOf(chunk -> payload.small.next) -> payload.small.previous = &(result -> payload.small);
			result -> payload.small.previous = chunk -> payload.small.previous;
			chunkOf(chunk -> payload.small.previous) -> payload.small.next = &(result -> payload.small);
			chunk -> payload.small.next = chunk -> payload.small.previous = 
				(GmOsChunkNodeSmall*)dlInfo::nullChunkAddress;
		}
		
		// Coalsce the result forward.
		coalsceChunkAfter(result);
		
		return result;
	}
	
	/// Attempt to allocate a chunk. If no chunk can be allocated, the null address will 
	/// be returned.
	void* allocate(allocateSizeType size) noexcept {
		// Round up the size.
		if(size < sizeof(GmOsChunkNodeSmall)) size = sizeof(GmOsChunkNodeSmall);
		else size = ((size + 0x03) | 0x03) ^ 0x03;
		
		// Eliminate impossible allocation.
		if(size >= ((dlInfo::totalPageFrame) << dlInfo::pageSizeShift)) return nullptr;
		
		// Judge whether the allocation level is page level.
		allocateSizeType physicalSize = GmOsFineChunkDlMalloc::physicalSize(size);
		if(physicalSize > (1 << dlInfo::pageSizeShift)) {
			// Calculate the page order.
			pfnType pfnSize = (physicalSize + 
				((1 << dlInfo::pageSizeShift) - 1)) >> dlInfo::pageSizeShift;
			orderType orderSize = 0;
			for(; (1 << orderSize) < pfnSize; ++ orderSize);
			
			// Attempt to allocate high page.
			pageType page = pageAllocator.allocateHighPage(orderSize);
			if(page == dlInfo::nullPageAddress) return nullptr;
			
			// Initialize page chunk header now.
			chunkType chunk = reinterpret_cast<chunkType>(page);
			chunk -> updateSize(orderSize << 2);
			chunk -> setFlag(GmOsFineChunkDlMalloc::bitPageAllocated);
			return chunk -> payload.memory;
		}
		
		// The allocation is chunk level, perform the allocation.
		else {
			if(!topChunkInitialize()) return nullptr;
			
			// Search in the fast bin for a potential chunk.
			// The bin will be directly allocated once a bin of fitting is found. Regardless
			// of internal fragment. This will surely increase the efficiency.
			if(size < (1 << dlInfo::fastbinMaxOrder)) {
				// Find the start fast order to allocate.
				orderType fastOrder = 2;
				for(; ((1 << fastOrder) < sizeof(GmOsChunkNodeSmall)) 
						|| (1 << fastOrder) > size; ++ fastOrder);
				
				// Search for a chunk in the fast bin.
				for(; fastOrder < dlInfo::fastbinMaxOrder; ++ fastOrder)
					if(fast[fastOrder].next != (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress) {
						
						// Get that chunk directly and return it to user.
						chunkType chunk = chunkOf(fast[fastOrder].next);
						chunk -> payload.small.unlinkChunk();
						chunk -> nextPhysicalChunk() -> setFlag(GmOsFineChunkDlMalloc::bitPreviousInUse);
						return chunk -> payload.memory;
					}
			}
			
			// Search in the small bin for a potential chunk.
			if(size < (1 << dlInfo::smallbinMaxOrder)) {
				// Find the start small order to allocate.
				orderType smallOrder = dlInfo::fastbinMaxOrder;
				for(; (1 << (smallOrder + 1)) > size; ++ smallOrder);
				
				// Search for a small chunk in the bin.
				for(; smallOrder < dlInfo::smallbinMaxOrder; ++ smallOrder) {
					orderType order = smallOrder - dlInfo::fastbinMaxOrder;
					
					// Find the best fit bin, and return the splitted chunk.
					chunkType current = chunkOf(small[order].next);
					for(; current != (chunkType)dlInfo::nullChunkAddress; 
						current = chunkOf(current -> payload.small.next)) {
						if(current -> size() >= size) {
							current -> payload.small.unlinkChunk();
							return splitUseChunk(current, size);
						}
					}
				}
			}
			
			// Search in the fast bin for a potential chunk.
			{
				// Find the start large order to allocate.
				orderType largeOrder = dlInfo::smallbinMaxOrder;
				for(; (1 << (largeOrder + 1)) > size; ++ largeOrder);
				
				// Search for a large chunk in the bin.
				for(; largeOrder < dlInfo::pageSizeShift; ++ largeOrder) {
					orderType order = largeOrder - dlInfo::smallbinMaxOrder;
					
					// Find the best fit bin, and return the splitted chunk.
					chunkType current = chunkOf(large[order].nextSize);
					for(; current != (chunkType)dlInfo::nullChunkAddress; 
							current = chunkOf(current -> payload.large.nextSize)) {

						if(current -> size() >= size) {
							// Always use the next chunk of current if possible.
							if(current -> payload.large.next != current -> payload.large.nextSize) {
								chunkType nextChunk = chunkOf(current -> payload.large.next);
								nextChunk -> payload.large.unlinkChunk();
								return splitUseChunk(nextChunk, size);
							}
							
							// Well, we have to use this chunk.
							else {
								current -> payload.large.unlinkChunk();
								return splitUseChunk(current, size);
							}
						}
					}
				}
			}
			
			// The chunk will be returned if it is selected.
			chunkType selectedChunk = (chunkType)dlInfo::nullChunkAddress;
			
			// Find in the unsorted bin and coalse the chunk if possible.
			if(unsorted.next != (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress) {
				for(chunkType chunk = chunkOf(unsorted.next);
					chunk != (chunkType)dlInfo::nullChunkAddress; ) {
					
					// Attempt to coalsce chunk. Please notice the coalscing process does not 
					// remove the chunk from the current list.
					chunkType coalsced = coalsceChunkUnsorted(chunk);
					
					// Now fetch the next node of the coalsced chunk.
					chunkType nextChunk = chunkOf(coalsced -> payload.small.next);
					coalsced -> payload.small.unlinkChunk();
					
					if(coalsced -> size() >= size) {
						if(selectedChunk == (chunkType)dlInfo::nullChunkAddress) 
							selectedChunk = coalsced;
						else if(selectedChunk -> size() > coalsced -> size()) {
							arrangeChunk(selectedChunk);
							selectedChunk = coalsced;
						}
					}
					
					// Insert the coalsced chunk into a proper list, and forward to next chunk.
					else arrangeChunk(coalsced);
					
					chunk = nextChunk;
				}
			}
			
			// Return the result if the coalsced chunk is suitable.
			if(selectedChunk != (chunkType)dlInfo::nullChunkAddress) {
				safelyUnlinkChunk(selectedChunk);
				return splitUseChunk(selectedChunk, size);
			}
			
			// Cannot allocate the page from free list, now split off the top chunk.
			// TopChunk V
			// (Before) | Previous | TopChunkSize | -------------------------------------|
			// (After)  | Previous | Size         | XXXXXXX | Size | RemainedSize | -----|
			//                                     TopChunk A
			// As the returned chunk will be in use, so the previous in use bit of the 
			// new top chunk will always be set to true.
			{ 
				if(physicalSize > topChunk -> size()) increaseTopChunk();
				chunkSizeType remainedSize = topChunk -> size() - physicalSize;
				topChunk -> updateSize(size);
				chunkType returnedChunk = topChunk;
				
				topChunk = topChunk -> nextPhysicalChunk();
				topChunk -> previousSize = size;
				topChunk -> updateSize(remainedSize);
				topChunk -> setFlag(GmOsFineChunkDlMalloc::bitPreviousInUse);
				return returnedChunk -> payload.memory;
			}
		}
	}
	
	/// Return a block of memory back to the allocator.
	void deallocate(void* memory) noexcept {
		if(memory == nullptr) return;
		chunkType chunk = chunkOf(memory);
		
		/// Return the chunk in page back to the allocator.
		if(chunk -> isPageAllocated()) {
			orderType pageOrder = (chunk -> size()) >> 2;
			pageAllocator.freeHighPage(reinterpret_cast<pageType>(chunk), pageOrder);
		}
		else {
			if(!topChunkInitialize()) return;
			
			// Reset the chunk data to avoid potential data violation.
			chunk -> payload.small.next = chunk -> payload.small.previous
				= (GmOsChunkNodeSmall*)dlInfo::nullChunkAddress;
			if(chunk -> isLargeChunkSize())
				chunk -> payload.large.nextSize = chunk -> payload.large.previousSize
					= (GmOsChunkNodeLarge*)dlInfo::nullChunkAddress;
			
			// Mark the current chunk as not allocated.
			chunk -> nextPhysicalChunk() -> clearFlag(GmOsFineChunkDlMalloc::bitPreviousInUse);
			
			// Put the chunk inside the unsorted bin.
			unsorted.insertSmallAfter(&(chunk -> payload.small));
			
			// Trigger top chunk shrink if adjacent.
			if(!topChunk -> previousInUse()) {
				chunkType coalscedChunk = coalsceChunkBefore(topChunk);
				if(coalscedChunk != (chunkType)dlInfo::nullChunkAddress) {
					coalscedChunk -> updateSize(coalscedChunk -> size() + topChunk -> physicalSize());
					topChunk = coalscedChunk;
					shrinkTopChunk();
				}
			}
		}
	}
};