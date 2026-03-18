module;
#include "MPMCQueue.h"
#include <windows.h>
#include "atomic"
export module FileLoaderSystem;

import std;
import ServiceLocator;
import Atomics;

class TextureManager;

namespace AngelBase::Core
{
    
    export enum class AsyncFilePriority : uint32_t
    {
        Critical = 0,
        High = 1,
        Normal = 2,
        Low = 3
    };
    
    export enum class AsyncFileResult :uint16_t
    {
        Pending = 0,
        Success = 1,
        Failed = 2
    };
    
    export using AsyncFileHandle = FILE*;
        
    export using AsyncFileBuffer = uint8_t*;

    export struct AsyncRequestHandle
    {
        const char * path;
        AsyncFileHandle handle;
        AsyncFileBuffer buffer;
        size_t buffer_size;
        size_t actual_size;
        AsyncFilePriority priority;
        void (*callback) (AsyncRequestHandle);
        Atomics::Counter dependent_on;
        std::shared_ptr<std::atomic<AsyncFileResult>> result;
    };

    // Handles multiple requests from multiple threads at once, and outputs loaded memory one at a time
    export class FileLoaderSystem : public ISystem
    {
    public:
        
        FileLoaderSystem()
            :workers(8), criticalRequests(100), highRequests(100), normalRequests(100), lowRequests(100)
            
        {

            for (auto& worker : workers)
            {
                worker = std::thread(&FileLoaderSystem::ProcessLoadRequests, this);
            }
        }
        
        ~FileLoaderSystem()
        {
            _shutdown.store(true, std::memory_order_release);
            for (auto& worker :workers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
        }
        
        /**
         * Asynchronous load request -- public Facing API
         * @param path path to file
         * @param buffer buffer to store results
         * @param buffer_size max buffer size you want to allow
         * @param priority priority of this load request
         * @param c Counter that async file request is dependent on
         * @param callback function to call when it's done
         * @return 
         */
        [[nodiscard]] AsyncRequestHandle asyncReadFile(const char * path, AsyncFileBuffer buffer, size_t buffer_size, AsyncFilePriority priority, Atomics::Counter& c,void(*callback)(AsyncRequestHandle))
        {
            c.increment();
            
            AsyncRequestHandle request;
            request.path = path;
            request.handle = nullptr;
            request.buffer = buffer;
            request.buffer_size = buffer_size;
            request.actual_size = 0;
            request.priority = priority;
            request.dependent_on = c;
            request.result = std::make_shared<std::atomic<AsyncFileResult>>(AsyncFileResult::Pending);
            request.callback = callback;
            
            switch (priority)
            {
            case AsyncFilePriority::Critical:
                criticalRequests.push(request);
                break;
            case AsyncFilePriority::High:
                highRequests.push(request);
                break;
            case AsyncFilePriority::Normal:
                normalRequests.push(request);
                break;
            case AsyncFilePriority::Low:
                lowRequests.push(request);
                break;
            }
            return request;
        }
        
        /**
         * fence function to wait for the load request to finish
         * @param handle 
         */
        static void asyncWaitComplete(const AsyncRequestHandle& handle)
        {
            handle.dependent_on.wait_for_zero();
        }
        
    protected:
        
        friend class TextureManager;
        
        /**
         * Used to attempt to open the file for reading
         * @param path_name name of the path to open
         * @return Returns a FILE* as a AsyncFileHandle, or nullptr if it fails. Check!
         */
        static [[nodiscard]] AsyncFileHandle asyncOpen(const char * path_name)
        {
            AsyncFileHandle handle = nullptr;
            errno_t error = fopen_s(&handle, path_name, "rb"); 
            if (error != 0)
            {
                char buffer[512];
                strerror_s(buffer, error);
                std::cerr << "asyncOpen failed: " << buffer << "\n";
            }
            return handle;
        }
        
    private:
        
        static void Load(AsyncRequestHandle&& handle)
        {
            handle.dependent_on.increment();
            handle.handle = asyncOpen(handle.path);
            if (!handle.handle)
            {
                handle.result.get()->store(AsyncFileResult::Failed);
                handle.callback(handle);
                return;
            }
            handle.actual_size = fread_s(handle.buffer, handle.buffer_size, 1, handle.buffer_size, handle.handle);
            if (ferror(handle.handle))
            {
                handle.result.get()->store(AsyncFileResult::Failed);
                handle.callback(handle);
                fclose(handle.handle);
                return;
            }
            handle.callback(handle);
            fclose(handle.handle);
            handle.result.get()->store(AsyncFileResult::Success);
            handle.dependent_on.decrement();
        }
        
        
        void ProcessLoadRequests()
        {
            while (!_shutdown)
            {
                AsyncRequestHandle req;
                if (criticalRequests.try_pop(req) ||
                    highRequests.try_pop(req)     ||
                    normalRequests.try_pop(req)   ||
                    lowRequests.try_pop(req))
                {
                    Load(std::move(req));
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        }
        
        std::vector<std::thread> workers;
        std::atomic<bool> _shutdown = false;
        rigtorp::mpmc::Queue<AsyncRequestHandle> criticalRequests;
        rigtorp::mpmc::Queue<AsyncRequestHandle> highRequests;
        rigtorp::mpmc::Queue<AsyncRequestHandle> normalRequests;
        rigtorp::mpmc::Queue<AsyncRequestHandle> lowRequests;
        unsigned int coreId = 7;
    };
}