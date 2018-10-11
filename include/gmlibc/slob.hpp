#pragma once
/**
 * @file gmlibc/slob.hpp
 * @brief Simple List-Of-Blocks Allocator
 * @author Haoran Luo
 *
 * This allocator is a fix-sized allocator, and suits users' need for 
 * object oriented allocation.
 *
 * The slob descriptor manages a series of slob frames. A slob frame 
 * maintain data for this slob and the data behind.
 *
 * Slob Descriptor                     Slob Frame
 * +--------------+                    +-----------------+
 * | FullFrame    |                    | FrameMagic      |
 * +--------------+                    +-----------------+
 * | PartialFrame | -----------------> | FrameUsed       |
 * +--------------+                    +-----------------+
 * | FreeFrame    | <-- At most one.   | FrameTop        |
 * +--------------+                    +-----------------+
 * | Slob Info    |                    | FrameFree       |
 * +--------------+                    +-----------------+
 *                                     | Slobs...        |
 *                                     +-----------------+
 * 
 * When an allocation request comes, the allocator first pick up a
 * partial frame, request allocation from it. If there's no partial
 * frame currently, the free frame will be promoted to partial frame 
 * and perform allocation. If there's no free frame, a new free frame
 * will be allocated.
 *
 * When a slob object is freed. We will search by validating whether
 * the page header magic check succeed. If succeed, the slob will be 
 * returned to this slob frame, and its slob frame will be promoted
 * or demoted. If the page frame is demoted and there's already some 
 * existing page frame. The lower address page frame will be deallocated,
 * and the higher address page frame will take the place.
 *
 * How large is the page frame to allocate purely depends on slob 
 * information.
 *
 * (The slob info is privately inherited so that empty object optimization
 * could be easily performed.)
 */

template<typename slobInfo, typename pageAllocatorType, typename slobRuntimeInfo>
struct GmOsFineAllocatorSlob : private slobRuntimeInfo {
	typedef typename slobInfo::addressType addressType;
	typedef typename slobInfo::objectNumberType objectNumberType;
	typedef typename slobInfo::orderType orderType;
	typedef typename pageAllocatorType::pageType pageType;
	
	/// The slob frame header that is used to manage slob data.
	struct GmOsFineChunkSlob {
		/// The magic word used in chunk header check.
		addressType magic;
		
		/// The runtime-info defined frame type data.
		addressType frameType;
		
		/// The number of chunks that is in use in the slob.
		objectNumberType used;
		
		/// The top of the chunk, which is always unused.
		objectNumberType top;
		
		/// The first object that is in unused state.
		objectNumberType freeHead;
		
		/// The pointer to previous and next slob frame.
		GmOsFineChunkSlob **previous, *next;
		
		/// The slob objects goes here.
		addressType slobs[1];
		
		/// The header size of the slob frame.
		static constexpr addressType slobHeaderSize = 
			reinterpret_cast<addressType>(&(((GmOsFineChunkSlob*)0x00) -> slobs));
		
		/// Calculate the magic word of the remained.
		addressType expectedMagic(const slobRuntimeInfo& rti) const noexcept {
			addressType thisAddress = reinterpret_cast<addressType>(this);
			return thisAddress ^ rti.magicForType(frameType) 
				^ (used | (top << 13) | (freeHead << 26));
		}
		
		/// Synchronize the current page magic.
		inline void synchronizeMagic(const slobRuntimeInfo& rti) noexcept {
			magic = expectedMagic(rti);
		}
		
		/// Determine whether current chunk is the slob frame header.
		bool isSlobHeader(const slobRuntimeInfo& rti) const noexcept {
			if(!rti.isValidFrameType(frameType)) return false;
			return (magic ^ expectedMagic(rti)) == 0;
		}
		
		/// Determine full state of the slob frame.
		bool full(const slobRuntimeInfo& rti) const noexcept {
			return rti.numObjects(slobHeaderSize, frameType) == used;
		}
		
		/// Determine empty state of the slob frame.
		bool empty(const slobRuntimeInfo& rti) const noexcept {
			return used == 0;
		}
		
		/// Attempt to allocate next object from the slob frame.
		void* allocateFromFrame(const slobRuntimeInfo& rti) noexcept {
			if(freeHead == 0) {
				// Attempt to increase top from the frame.
				if(full(rti)) return nullptr;
				void* result = rti.offsetForObject(slobs, top);
				++ top; ++ used;
				synchronizeMagic(rti);
				return result;
			}
			else {
				// Directly pop the most recently freed object.
				void* result = rti.offsetForObject(slobs, freeHead - 1);
				objectNumberType nextSlob = *((objectNumberType*)result);
				freeHead = nextSlob; ++ used;
				synchronizeMagic(rti);
				return result;
			}
		}
		
		/// Attempt return the object to the slob frame.
		bool deallocateToFrame(const slobRuntimeInfo& rti, void* memory) noexcept {
			// Make sure the deallocating object is valid.
			objectNumberType* newFreeHead = (objectNumberType*)memory;
			objectNumberType memoryIndex = rti.offsetFromObject(slobs, memory);
			if(memoryIndex >= rti.numObjects(slobHeaderSize, frameType)) return false;
			else if(memoryIndex < 0) return false;
			else if(used <= 0) return false;
			
			// Perform deallocation.
			*newFreeHead = freeHead;
			freeHead = memoryIndex + 1; -- used;
			synchronizeMagic(rti);
			return true;
		}
		
		/// Remove the frame from current list.
		void removeFromList() noexcept {
			if(previous != nullptr) *previous = next;
			if(next != nullptr) next -> previous = previous;
			previous = nullptr; next = nullptr;
		}
		
		/// Insert the object into a list.
		void insertIntoList(GmOsFineChunkSlob** list) noexcept {
			previous = list; next = *list;
			if(*list != nullptr) (*list) -> previous = &next;
			*list = this;
		}
	};
	
	// The head of each slob frames.
	pageAllocatorType& pageAllocator;
	GmOsFineChunkSlob *full, *partial, *sfree;
	
	GmOsFineAllocatorSlob(pageAllocatorType& pageAllocator, const slobRuntimeInfo& rti): 
		slobRuntimeInfo(rti), pageAllocator(pageAllocator), 
		full(nullptr), partial(nullptr), sfree(nullptr) {}
	
	/// Allocate new object.
	void* allocate() noexcept {
		// Ensure that there's some partial list for allocation.
		if(partial == nullptr) {
			if(sfree != nullptr) {
				// Promote a free slob frame to the partial.
				GmOsFineChunkSlob* popped = sfree;
				popped -> removeFromList();
				popped -> insertIntoList(&partial);
			}
			else {
				// Initialize a new slob frame.
				addressType frameType = slobRuntimeInfo::nextPageType();
				orderType order = slobRuntimeInfo::pageOrderOf(frameType);
				GmOsFineChunkSlob* newSlobFrame = reinterpret_cast<
					GmOsFineChunkSlob*>(pageAllocator.allocateHighPage(order));
				if(newSlobFrame == (GmOsFineChunkSlob*)slobInfo::nullPageAddress) return nullptr;
				newSlobFrame -> frameType = frameType;
				newSlobFrame -> used = newSlobFrame -> top = newSlobFrame -> freeHead = 0;
				newSlobFrame -> insertIntoList(&partial);
				newSlobFrame -> synchronizeMagic(*this);
			}
		}
		
		// Allocate new object from the top partial frame.
		void* result = partial -> allocateFromFrame(*this);
		if(result == nullptr) return nullptr;
		
		// Update the partial frame status.
		if(partial -> full(*this)) {
			GmOsFineChunkSlob* promoted = partial;
			partial -> removeFromList();
			partial -> insertIntoList(&full);
		}
		
		// Notify that one object has been created.
		slobRuntimeInfo::objectCreated();
		return result;
	}
	
	/// Deallocate an object.
	void deallocate(void* object) noexcept {
		if(object == nullptr) return;
		
		// Determine which slab frame contains this object.
		addressType frameSize = (1 << slobInfo::pageSizeShift);
		addressType firstPageAddress = slobInfo::firstPageAddress();
		addressType frameAddress = (((reinterpret_cast<addressType>(object)
				- firstPageAddress) | (frameSize - 1)) ^ (frameSize - 1)) + firstPageAddress;
		while(frameAddress >= firstPageAddress) {
			if(reinterpret_cast<GmOsFineChunkSlob*>(frameAddress) -> isSlobHeader(*this)) break;
			frameAddress -= frameSize;
		}
		if(frameAddress < firstPageAddress) return;
		
		// Perform deallocation.
		GmOsFineChunkSlob* frame = reinterpret_cast<GmOsFineChunkSlob*>(frameAddress);
		if(!frame -> deallocateToFrame(*this, object)) return;
		
		/// Check whether demotion should be performed.
		if(frame -> empty(*this)) {
			if(slobInfo::deftSlobDeallocate) {
				// Perform deallocation directly.
				pageAllocator.freeHighPage(reinterpret_cast<pageType>(frame),
							slobRuntimeInfo::pageOrderOf(frame -> frameType));
			}
			else {
				// Perform demotion instead.
				GmOsFineChunkSlob* demoted = frame;
				demoted -> removeFromList();
				if(sfree == nullptr) {
					// Just replace the free list header.
					demoted -> insertIntoList(&sfree);
				}
				else {
					// One should be erased and one should take the place.
					if(	reinterpret_cast<addressType>(demoted) > 
						reinterpret_cast<addressType>(sfree)) {
						pageAllocator.freeHighPage(reinterpret_cast<pageType>(sfree), 
							slobRuntimeInfo::pageOrderOf(sfree -> frameType));
						sfree = nullptr;
						demoted -> insertIntoList(&sfree);
					}
					else {
						pageAllocator.freeHighPage(reinterpret_cast<pageType>(demoted),
							slobRuntimeInfo::pageOrderOf(demoted -> frameType));
					}
				}
			}
		}
		
		// Notify that one object has been destroyed.
		slobRuntimeInfo::objectDestroyed();
	}
};

/// @brief the runtime info that naively allocate one more page no matter how many objects
/// has been allocated.
template<typename slobInfo, typename sizeType>
struct GmOsSlobRuntimeNormalSized {
	/// @brief The object size, which must be aligned, otherwise unexpected problem will 
	/// occur while accessing words.
	sizeType objectSize;
	
	// Forward type definitions.
	typedef typename slobInfo::orderType orderType;
	typedef typename slobInfo::addressType addressType;
	typedef typename slobInfo::objectNumberType objectNumberType;
	
	/// Just single page will be accepted, and the page type is always deadbeef.
	static addressType nextPageType() noexcept { return 0xdeadbeef; }
	static bool isValidFrameType(addressType frameType) noexcept { return frameType == 0xdeadbeef; }
	
	/// The magic word will also always be cafebabe.
	static addressType magicForType(addressType frameType) noexcept { return 0xcafebabe; }
	
	// Perform calculation based on every objects' size.
	addressType numObjects(addressType slobHeaderSize, addressType frameType) const noexcept {
		addressType pageSize = (1 << slobInfo::pageSizeShift);
		return (pageSize - slobHeaderSize) / objectSize;
	}
	
	void* offsetForObject(void* slobPointer, objectNumberType objectNumber) const noexcept {
		return reinterpret_cast<void*>(reinterpret_cast<addressType>(slobPointer) 
				+ (objectNumber * objectSize));
	}
	
	objectNumberType offsetFromObject(void* slobPointer, void* objectPointer) const noexcept {
		return (reinterpret_cast<addressType>(objectPointer) - 
			reinterpret_cast<addressType>(slobPointer)) / objectSize;
	}
	
	static orderType pageOrderOf(addressType frameType) noexcept { return 0; }
	static void objectCreated() noexcept {}
	static void objectDestroyed() noexcept {}
};