module;
#include <cstdint>

export module Atomics;
import std;
namespace Atomics
{
    
    export class Counter
    {
    private:
        struct CounterInternal
        {
            std::atomic<unsigned int> counter{0};
            std::atomic<unsigned int> ref_counter{1};
        };
    
        CounterInternal* internal;
    public:
        // Default constructor - allocates new internal state
        Counter() : internal(new CounterInternal()) {}

        //when copying, increment usages
        Counter(const Counter& rhs) :internal(rhs.internal)
        {
            internal->ref_counter.fetch_add(1, std::memory_order_relaxed);
        };
    
        // Copy assignment operator
        Counter& operator=(const Counter& rhs)
        {
            if (this != &rhs)
            {
                decrement_ref_count_cleanup();
                internal = rhs.internal;
                internal->ref_counter.fetch_add(1, std::memory_order_relaxed);
            }
        
            return *this;
        };

        /**
         * Used for r-values (temporary values)-- rhs is destroyed
         * @param rhs -- counter to be moved
         */
        Counter(Counter&& rhs) noexcept
        {
            internal = rhs.internal;
            internal->ref_counter.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * Used for r-values (temporary values)-- rhs is destroyed
         * @param rhs 
         * @return returns the moved Counter
         */
        Counter& operator=(Counter&& rhs) noexcept
        {
            if (this != &rhs)
            {
                //we have to decrement 
                decrement_ref_count_cleanup();
                internal = rhs.internal;
                internal->ref_counter.fetch_add(1, std::memory_order_relaxed);
            }
        
            return *this;
        }
    
        // Destructor
        ~Counter()
        {
            //1 less ref count for the atomic counter 
            decrement_ref_count_cleanup();
        }

        /**
         * increments counter 
         * @return returns the original counter value
         */
        uint32_t increment() {
            if (internal) {
                return internal->counter.fetch_add(1, std::memory_order_relaxed);
            }
            return 0;
        }

        /**
         * decrements counter
         * @return returns the original counter value before decrementing 
         */
        uint32_t decrement() {
            if (internal) {
                return internal->counter.fetch_sub(1, std::memory_order_acquire);
            }
            return 0;
        }

        /**
         * gets current value atomically
         * @return 
         */
        uint32_t get() const {
            return internal ? internal->counter.load(std::memory_order_relaxed) : 0;
        }


        /**
         * acts as a fence to prevent moving until counter is 0 for all threads
         */
        void wait_for_zero() const
        {
            if (!internal) return;
            while (internal->counter.load(std::memory_order_acquire) > 0) {
                std::this_thread::yield();
            }
        }

    private:
        /**
         * decrement reference counter and see if we need to clean up
         */
        void decrement_ref_count_cleanup() {
            if (internal) {
                // Use release-acquire semantics for proper synchronization
                if (internal->ref_counter.fetch_sub(1, std::memory_order_release) == 1)
                {
                    //we need to make sure we have full control of this counter before deleting
                    std::atomic_thread_fence(std::memory_order_acquire);
                    delete internal;
                }
                //if we're not the last one to hold this counter we just decrement 
            }
        }
    };
 

}
