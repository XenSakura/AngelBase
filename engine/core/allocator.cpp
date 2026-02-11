module;
#include <cstdint>
#include <cassert>
#include <stddef.h>
#include <cstring>
export module allocator;
import std;

namespace AngelBase::Allocator
{
    template <typename T>
    concept Allocator = requires(T alloc, size_t size, size_t alignment, void* ptr)
    {
        { alloc.Allocate(size, alignment) } -> std::same_as<void*>;
        { alloc.Deallocate(ptr) } -> std::same_as<void>;
        { alloc.Reset() } -> std::same_as<void>;
        { alloc.GetUsedMemory() } -> std::same_as<size_t>;
        { alloc.GetTotalMemory() } -> std::same_as<size_t>;
    };
    static constexpr unsigned char DEAD_MEMORY = 0xDD;
    static constexpr unsigned char CLEAN_MEMORY = 0xCD;

    /**
     * Calculate how many padding bytes are needed to align an address.
     * 
     * Alignment requirements ensure data sits at addresses that are multiples
     * of its alignment value. For example:
     * - 8-byte alignment means address must be divisible by 8 (0, 8, 16, 24...)
     * - 16-byte alignment means address must be divisible by 16 (0, 16, 32, 48...)
     * 
     * @param address - The current memory address (as integer)
     * @param alignment - Required alignment (must be power of 2: 1, 2, 4, 8, 16, etc.)
     * @return Number of bytes to add to reach next aligned address
     * 
     * Example: address=13, alignment=8
     *   - 13 % 8 = 5 (we're 5 bytes past the last aligned address)
     *   - Need to skip 3 more bytes to reach 16 (next multiple of 8)
     *   - Returns 3
     */
    inline size_t calculate_padding(uintptr_t address, size_t alignment)
    {
        // Find how far past the last aligned boundary we are
        size_t remainder = address % alignment;
    
        // If already aligned (remainder is 0), no padding needed
        if (remainder == 0)
        {
            return 0;
        }
    
        // Otherwise, padding = bytes needed to reach next alignment boundary
        // Example: if remainder is 5 and alignment is 8, padding is 8-5=3
        size_t padding = alignment - remainder;
    
        return padding;
    }

    /**
     * Overload that accepts a pointer instead of raw address.
     * Converts pointer to integer address and calculates padding.
     */
    inline size_t calculate_padding(void* ptr, size_t alignment)
    {
        uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
        return calculate_padding(address, alignment);
    }
    
    namespace Generic
    {
        // default 200 items and 512 bytes per allocation
        /**
         * Used for uncertain allocations within the \b engine. Test
         * @tparam size_of_pool 
         * @tparam size_of_block 
         */
        export template <size_t size_of_pool = 102400, size_t size_of_block = 512>
        class PoolAllocator
        {
            static_assert(size_of_block % alignof(std::max_align_t) == 0,
                          "PoolAllocator: size_of_block must be a multiple of max_align_t (typically 8 or 16).");
        private:
            struct GenericObject
            {
                GenericObject* next;
                GenericObject* prev;
                //by default 512 bytes-- leave room to keep track of next free memory
                char data[size_of_block];
            };
            constexpr size_t block_size = size_of_block;
            constexpr size_t pool_size = size_of_pool;
            constexpr size_t total_objects = size_of_pool / size_of_block;
            GenericObject* poolMemory;
            // no memory leaks-- ensure all is  freed
            GenericObject* allocatedList;
            size_t allocated_size = 0;
            GenericObject* freeList;
            size_t free_size = 0;
        public:
            
            PoolAllocator()
            {
                free_size = pool_size / block_size;
                poolMemory = new GenericObject[free_size];
                freeList = poolMemory;
                allocatedList = nullptr;

                reinitialize_pointers();
            }

            ~PoolAllocator()
            {
                Reset();
                delete[] poolMemory;
            }
        
            template <typename T>
            T* allocate()
            {
                static_assert(sizeof(T) <= size_of_block,
                              "PoolAllocator: requested type is too large for the block size.");
                //if no more
                if (freeList == nullptr) return nullptr;
                //get the block
                GenericObject* block = freeList;
                // set the head to the next free block
                freeList = freeList->next;
                // set free list checking
                free_size--;
            
                //set head previous to none
                if (freeList != nullptr) freeList->prev = nullptr;
                //for tracking 
                allocated_size++;

                //update allocated tracker

                //grab the head of allocated list
                GenericObject* head = allocatedList;

                //make the block new head-- set the next to the old head
                block->next = head;
                //set the previous to none
                block->prev = nullptr;
                // old head now points to new head
                if (head != nullptr) head->prev = block;
                //now the head is the block
                allocatedList = block;

#ifdef _DEBUG
                memset(block->data, CLEAN_MEMORY, size_of_block);
#endif
                return reinterpret_cast<T*>(block->data);
            }

            template <typename T>
            void deallocate(T* ptr)
            {
                assert(ptr != nullptr && "Attempted to deallocate a null pointer!");
                GenericObject* end = &poolMemory[total_objects];

                assert(ptr <= end && ptr >= poolMemory && "Attempted to free memory not owned by this pool");
                GenericObject* block = reinterpret_cast<GenericObject*>(reinterpret_cast<char*>(ptr) - offsetof(GenericObject, data));
#ifdef _DEBUG
                // Verify we're not double-freeing by checking if memory is already dead
                if (block->data[0] != DEAD_MEMORY)
                {
                    assert(!false && "Double free detected - memory already marked as dead!");
                }
#endif
                
                // if head of the allocated list set head to next
                if (block->prev == nullptr) allocatedList = block->next;
            
                //remove from allocated tracker
                allocated_size--;
                //get prev -- will be null if block is head
                GenericObject* prev = block->prev;
                // get next -- will be null if block is tail
                GenericObject* next = block->next;

                //if not head, link the prev to next
                if (prev != nullptr) prev->next = next;
                //if not tail, link the next to previous
                if (next != nullptr) next->prev = prev;

#ifdef _DEBUG
                // Fill with dead memory pattern - use-after-free will be obvious
                memset(block->data, DEAD_MEMORY, size_of_block);
#endif

                //move block to the start
                block->prev = nullptr;
                block->next = freeList;
                //set previous head's previous tracker to the block
                if (freeList != nullptr) freeList->prev = block;
                freeList = block;
                free_size++;
            }

            //take away allocations manually, reset 
            void Reset()
            {
#ifdef _DEBUG
                // Mark all memory as dead on reset
                for (size_t i = 0; i < total_objects; i++)
                {
                    memset(poolMemory[i].data, DEAD_MEMORY, size_of_block);
                }
#endif
                freeList = poolMemory;
                allocatedList = nullptr;

                reinitialize_pointers();
            
            }
        private:
            void reinitialize_pointers()
            {
                freeList[0].prev = nullptr;
                for (unsigned int i = 0; i < free_size - 1; i++)
                {
                    freeList[i].next = &freeList[i + 1];
                    freeList[i + 1].prev = &freeList[i];
                }
                freeList[free_size - 1].next = nullptr;
            }
        };

        //lightweight array
        export template <typename T>
        struct TypedView
        {
            T* elements;
            size_t count;

            //if created we think someone's going to use it 
            constexpr TypedView() :elements(nullptr), count(0){}
            
            TypedView(T* elements, size_t count) : elements(elements), count(count) {}

            T& operator [](size_t idx)
            {
                assert(idx < count && "index out of bounds");
                return elements[idx];
            }

            const T& operator[](int idx) const
            {
                assert(idx < count && "index out of bounds");
                return elements[idx];
            }

            T* begin() {return elements;}
            T* end() {return elements + count;}

            const T* begin() const {return elements;}
            const T* end() const {return elements + count;}

            bool empty() const {return count == 0;}

            TypedView<T> SubsetOf(size_t begin, size_t end)
            {
                assert(begin + end <= count && "Subset is out of bounds of the Component view.");
                return ComponentView<T>(elements + begin, end);
            }
        };

        /**
         * if we know the type and we need a lot of it
         * @tparam T 
         * @tparam pool_elements_count 
         */
        export template <typename T, size_t pool_elements_count>
        class TypedPoolAllocator
        {
        private:
            T* elements;
            size_t total_count = pool_elements_count;
            size_t count;
            T* allocated;
        public:
            TypedPoolAllocator()
            {
                count = 0;
                elements = new T[total_count];
            }

            TypedPoolAllocator(const TypedPoolAllocator&) = delete;
            TypedPoolAllocator& operator=(const TypedPoolAllocator&) = delete;
            
            ~TypedPoolAllocator()
            {
                delete[] elements;
            }

            TypedPoolAllocator(TypedPoolAllocator&& other) noexcept
                :elements(other.elements), count(other.count), total_count(other.total_count)
            {
                other.elements = nullptr;
                other.count = 0;
                other.total_count = 0;
            }

            TypedView<T> allocate(size_t size)
            {
                assert(count + size <= total_count && "Allocation is too big for pool.");
                TypedView<T> view(elements + count, size);
                count += size;
                return view;
            }

            TypedView<T> getPool()
            {
                return ComponentView<T>(elements, count);
            }

            void clear()
            {
                count = 0;
            }

            size_t capacity() const {return total_count;}
            size_t size() const {return count;}
            size_t available() const {return total_count - count;}
        };
        
        
    }

    namespace Atomic
    {
     
    }
    
    
    // STL allocator
    export template <typename T>
    class allocator
    {
    public:
        using value_type = T;
        
        allocator() noexcept = default;
        

        template<typename U>
        constexpr allocator(const allocator<U>&) noexcept {}

        virtual T* allocate(std::size_t n) = 0;

        virtual void deallocate(T* p, std::size_t n) noexcept = 0;
    private:
    };

    export class IAllocator {
    public:
        virtual ~IAllocator() = default;
    
        virtual void* Allocate(size_t size, size_t alignment = 16) = 0;
        virtual void Deallocate(void* ptr) = 0;
        virtual void Reset() = 0;
    
        virtual size_t GetUsedMemory() const = 0;
        virtual size_t GetTotalMemory() const = 0;
    
        const char* GetName() const { return m_name; }
    
    protected:
        IAllocator(const char* name) : m_name(name) {}
        const char* m_name;
    };

    //used for RAII types
    export template <typename T>
    class STLAllocator : allocator<T>
    {
    public:
        using value_type = T;
        
        
        STLAllocator() noexcept = default;
        

        template<typename U>
        constexpr STLAllocator(const STLAllocator<U>&) noexcept {}

        T* allocate(std::size_t n) override
        {
            if (n > (std::size_t(-1) / sizeof(T)))
            {
                throw std::bad_alloc{};
            }
            if (auto p = static_cast<T*>(::operator new(sizeof(T) * n)))
            {
                return p;
            }
            throw std::bad_alloc{};
        }

        void deallocate(T* p, std::size_t n) noexcept override
        {
            ::operator delete(p);
        }
    private:
    };

    /**
     * Memory in bytes
     * @tparam size 
     */
    export template <size_t size>
    class ArenaAllocator
    {
        char* memory;
        size_t remaining_memory;
        char* current;
        
        struct WrapperBase
        {
            WrapperBase* next;
            WrapperBase* prev;
        };
    
        
        template <typename T>
        struct TWrapper : WrapperBase
        {
            T object;  
        };
    
        WrapperBase* allocatedList;  
        

        size_t allocatedCount;
    public:
        ArenaAllocator() noexcept
        {
            memory = new char[size];
            remaining_memory = size;
            current = memory;
            allocatedList = nullptr;
            allocatedCount = 0;
        }
        ~ArenaAllocator()
        {
            delete[] memory;
        }

        template <typename T>
        T* allocate()
        {
            size_t wrapper_size = sizeof(TWrapper<T>);
            size_t alignment = alignof(TWrapper<T>);
    
            // Calculate padding needed to align current pointer (not memory!)
            size_t padding = calculate_padding(current, alignment);
            size_t total_size = padding + wrapper_size;
    
            assert(remaining_memory >= total_size && "Not enough memory in pool");

            // Apply padding to align current pointer
            current += padding;
    
            // Place TWrapper in arena (now properly aligned)
            TWrapper<T>* wrapper = reinterpret_cast<TWrapper<T>*>(current);
            wrapper->memory = &(wrapper->object);  // Point to the object member
            wrapper->next = allocatedList;
            wrapper->prev = nullptr;

            if (allocatedList)
                allocatedList->prev = wrapper;

            // Set to head of new list
            allocatedList = wrapper;
    
            // Update tracking
            remaining_memory -= total_size;
    
            // Move cursor past the wrapper
            current += wrapper_size;
    
            return &(wrapper->object);
        }

        template <typename T>
        void deallocate(T* pointer)
        {
            assert (pointer != nullptr && "Invalid memory to free");
            
        }
    private:
    };
    // temp memory for the frame only-- should never use with persistent memory
    export template <size_t Size>
    class FrameAllocator
    {
        void* m_start;
        void* m_current;
        size_t m_used;
        size_t m_size;
        std::string_view m_name = "FrameAllocator";

#ifdef _DEBUG
        
        
        struct AllocationInfo
        {
            void* address;              // Where it was allocated
            size_t size;                // Size of the type
            size_t alignment;           // Alignment requirement
            size_t count;               // Number of elements (n)
            size_t total_bytes;         // Total bytes including padding
            size_t padding;             // Bytes of padding added
            const char* type_name;      // Type name from typeid
            size_t allocation_id;
        };
        size_t m_allocation_counter = 0;
        std::vector<AllocationInfo> m_allocations;
#endif
    public:
        explicit FrameAllocator()
            :m_used(0),
             m_size(Size)
        {
            m_start = ::operator new(Size);
            m_current = m_start;
        }
        /**
         * DO NOT USE WITH PERSISTENT MEMORY-- this memory only exists for this frame
         * @tparam T 
         * @param n 
         * @return 
         */
        template<typename T>
        T* allocate(std::size_t n = 1)
        {
            size_t obj_size = sizeof(T);
            size_t alignment = alignof(T);
            
            uintptr_t current_addr = reinterpret_cast<uintptr_t>(m_current);
            uintptr_t mask = alignment - 1;
            uintptr_t misalignment = current_addr & mask;
            size_t padding = 0;
            if (misalignment != 0)
            {
                padding = alignment - misalignment;
            }
            
            size_t totalOffset = padding + (obj_size * n);
            std::cout << "Total offset: " << totalOffset << "\n";

            
            char* start_char = static_cast<char*>(m_start);
            char* current_char = static_cast<char*>(m_current);
            
            void* printed_address_end = start_char + m_size;
            void* printed_address_attempted = current_char + totalOffset;
            std::cout << "Compare: " << printed_address_end << " and " << printed_address_attempted << "\n";
            
            // if we're trying to allocate past where we're allowed to
            if ((start_char + m_size) < (current_char + totalOffset))
            {
                std::cerr << "FrameAllocator is full!" << "\n";
                throw std::bad_alloc{};
            }
            
            void* return_value = reinterpret_cast<void*>(current_addr + padding);
            m_current = current_char + totalOffset; 
            m_used += totalOffset; 

            T* return_value_ptr = reinterpret_cast<T*>(return_value);
            *return_value_ptr = T();

#ifdef _DEBUG
            m_allocations.push_back({
                return_value_ptr,           // address
                obj_size,                   // size
                alignment,                  // alignment
                n,                          // count
                totalOffset,                // total_bytes
                padding,                    // padding
                typeid(T).name(),           // type_name
                m_allocation_counter++      // allocation_id
            });         
#endif

            memset(return_value_ptr, CLEAN_MEMORY, sizeof(T));
            
            return return_value_ptr;
        }

        void Reset()
        {
            memset(m_start, DEAD_MEMORY, m_used);
            m_current = m_start;
            m_used = 0;
#ifdef _DEBUG
            m_allocations.clear();
            m_allocation_counter = 0;
#endif
        }

        size_t GetUsed() const { return m_used; }
    };

    
    

    export enum class MemoryType
    {
        Stack,
        Pool,
        Slab
    };
    
    export class GeneralAllocator 
    {
        const unsigned int FRAMES = 2;
        
    public:
        GeneralAllocator()
        {
            // make an allocator for engine STL needs and systems
        }

        struct MemoryPoolInfo
        {
            const char* name;
            unsigned int budget;
            MemoryType type;
            
        };

        void SetMemoryPoolsAndBudgets(const std::vector<MemoryPoolInfo>& pool_info)
        {
            
        }

    private:
        std::vector<MemoryPoolInfo> pools_and_info;
        
    };
}