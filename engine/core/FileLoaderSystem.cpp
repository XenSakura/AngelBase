module;
#include "MPMCQueue.h"
#include <windows.h>
export module FileLoaderSystem;
import std;
import allocator;
//TODO: Make async
namespace AngelBase::Core
{
    // Handles multiple requests from multiple threads at once, and outputs loaded memory one at a time
    export class FileLoaderSystem
    {
    public:
        struct LoadRequest
        {
            std::string filepath;
            std::atomic<bool>* completed;
            std::vector<unsigned char>* result_binary;
        };
        
        FileLoaderSystem()
            :requests(1000)
        {

            for (auto& worker : workers)
            {
                workers.emplace_back(ProcessLoadRequests);
            }
        }

        /**
         * This request MUST happen
         * @param req 
         */
        void submitLoadRequests(LoadRequest& req)
        {
            requests.push(req);
        }

        /**
         * this request doesn't need to happen urgently, fail if it doesn't happen
         * @param req 
         */
        bool try_submitLoadRequests(LoadRequest& req)
        {
            return requests.try_push(req);
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
        
        static std::vector<unsigned char> Load(const std::string& filepath)
        {
            std::ifstream file(filepath, std::ios::binary);
            if (!file.is_open())
            {
                return {};
            }

            file.seekg(0, std::ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);
    
            std::vector<unsigned char> buffer(fileSize);
            file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
            file.close();
    
            return buffer;
        }
    private:
        std::vector<std::thread> workers;

        std::atomic<bool> _shutdown = false;
        rigtorp::mpmc::Queue<LoadRequest> requests;
        
        void ProcessLoadRequests()
        {
            DWORD_PTR mask = 1ULL << coreId;
            SetThreadAffinityMask(GetCurrentThread(), mask);
            
            while (!_shutdown.load(std::memory_order_acquire))
            {
                LoadRequest request;
                
                // Try to pop a request (non-blocking with timeout simulation)
                if (requests.try_pop(request))
                {
                    // Perform the I/O operation
                    auto data = Load(request.filepath);
                    
                    // Store result
                    if (request.result_binary)
                    {
                        *request.result_binary = std::move(data);
                    }
                    
                    // Signal completion
                    if (request.completed)
                    {
                        request.completed->store(true, std::memory_order_release);
                    }
                }
                else
                {
                    // No work available, yield to avoid busy-waiting
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        }
        unsigned int coreId = 7;
    };
}