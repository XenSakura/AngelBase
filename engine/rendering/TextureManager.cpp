module;
#include <vulkan/vulkan.hpp>
#include "stb_image.h"
export module TextureManager;
import std;
import VulkanContext;
import FileLoaderSystem;


//TODO: have this happen async
namespace Rendering
{
    export class TextureManager
    {
    public:
        TextureManager() = delete;
        TextureManager(const Vulkan::Context& context)
            :m_context(context)
        {
        }

        void loadAllPaths()
        {
            //read json for all relevant file paths
        }

        void loadTexture(const char* path)
        {
            
        }

        void UploadTexturesToVRAM()
        {
            
        }

        
    private:
        
        struct ImageInfo
        {
            unsigned char* data;
            int width, height, channels;
            
        };
        std::vector<ImageInfo> unstaged_images;
        const Vulkan::Context& m_context;
    };
}