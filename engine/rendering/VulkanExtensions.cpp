module;
#include <vulkan/vulkan.h>
export module VulkanExtensions;

import std;

namespace rendering
{
#ifdef _DEBUG
    const bool enableValidationLayers = true;
#else
    const bool enableValidationLayers = false;
#endif

    //this is its own separate section-- only graphics programmers should touch this
    //can change fundamental engine architecture constraints
    export void PickInstanceExtensions()
    {
        std::pair<uint32_t, std::vector<VkExtensionProperties>> extensions = {};
        vkEnumerateInstanceExtensionProperties(nullptr, &extensions.first, nullptr);
        extensions.second.resize(extensions.first);
        // now we know how many extensions are possible
        vkEnumerateInstanceExtensionProperties(nullptr, &extensions.first, extensions.second.data());
        // this is something we will need to handle when we're testing various hardware
        std::vector<const char *> requiredInstanceExtensions = {};
        //TODO: I really don't feel like writing code for picking out instance extensions right now
        // MacOS users can suffer for all I care
        for (const auto& extension : requiredInstanceExtensions)
        {
			
        }
    }
}