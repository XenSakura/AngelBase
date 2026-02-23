module;
#include <cstdint>
#include <cassert>
#include <stddef.h>
#include <cstring>
export module allocator;
import std;

namespace AngelBase::Allocator
{
    static constexpr unsigned char DEAD_MEMORY = 0xDD;
    static constexpr unsigned char CLEAN_MEMORY = 0xCD;


    
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

            TypedView<T> allocate(size_t size = 1)
            {
                assert(count + size <= total_count && "Allocation is too big for pool.");
                TypedView<T> view(elements + count, size);
                count += size;
                return view;
            }

            void deallocate(TypedView<T> view)
            {
                size_t size = view.count;
                
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
    
    // temp memory for the frame only-- should never use with persistent memory
    export template <size_t Size>
    class ArenaAllocator
    {
        void* m_start;
        void* m_current;
        size_t m_used;
        size_t m_size;
        std::string_view m_name = "ArenaAllocator";

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
        explicit ArenaAllocator()
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
            
            char* start_char = static_cast<char*>(m_start);
            char* current_char = static_cast<char*>(m_current);
            
            
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
    
}