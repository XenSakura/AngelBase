module;

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
export module vkhelper;
import std;
import <cstring>;
namespace rendering
{
    namespace vkhelper
    {
        //we need to abstract information-- stuff that fundamentally changes how the game engine works
        // should be hidden-- stuff that would be nice to allow higher level graphics programmers
        // to change options and such should be exposed through functions

        //we build these classes to hide implementation details and expose the ones that we want to allow others to use
        export class InstanceBuilder
        {
        public:
            InstanceBuilder()
            {
                appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                appInfo.pApplicationName = "application";
                appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
                appInfo.pEngineName = "KitsuneEngine";
                appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
                // ONLY low level programmers should be allowed to touch this
                appInfo.apiVersion = VK_API_VERSION_1_4;
                
                createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                createInfo.pApplicationInfo = &appInfo;
                
                //TODO FOR RELEASE: Check for hardware incompatibilities for other instance extensions
                uint32_t glfwExtensionCount = 0;
                const char ** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
                instanceExtensions = std::vector(glfwExtensions, glfwExtensions + glfwExtensionCount);
                instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                
                createInfo.ppEnabledExtensionNames = instanceExtensions.data();
                createInfo.enabledExtensionCount = instanceExtensions.size();

                instance = VK_NULL_HANDLE;
            }

            ~InstanceBuilder() = default;
            
            void SetEngineVersion(const uint32_t a, const uint32_t b, const uint32_t c)
            {
                appInfo.engineVersion = VK_MAKE_VERSION(a, b, c);
            }
            // NO COPY
            void SetApplicationName(const std::string& name)
            {
                appInfo.pApplicationName = name.c_str();
            }
            
            void SetApplicationVersion(const uint32_t a, const uint32_t b, const uint32_t c)
            {
                appInfo.applicationVersion = VK_MAKE_VERSION(a, b, c);
            }
            
            void SetVKAPIVersion(uint32_t majorVersion, uint32_t minorVersion)
            {
                if (minorVersion > 4 || majorVersion > 1)
                    throw (std::runtime_error("Invalid Vulkan API version, or a new version has been released. "
                                              "Check the Instance Builder in vkhelper.cpp or the Vulkan Specification."));
                // We shouldn't be using a variant-- keep variant to 0
                // we also should keep patch number to 0-- that is required by Vulkan Spec
                appInfo.apiVersion = VK_MAKE_API_VERSION(0, majorVersion, minorVersion, 0);
            }

            void EnableDebugValidationLayers(bool value)
            {
#ifdef _DEBUG
                if (value == true)
                {
                    uint32_t layerCount = 0;
                    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
                    std::vector<VkLayerProperties> availableLayers(layerCount);
                    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

                    for (auto const& layer : validationLayers)
                    {
                        bool currentLayerFound = false;
                        for (const auto& a : availableLayers)
                        {
                            if (strcmp(layer, a.layerName) == 0)
                            {
                                currentLayerFound = true;
                                break;
                            }
                        }
                        if (currentLayerFound == false)
                        {
                            throw std::runtime_error("Failed to find a validation layer in the instance.");
                        }
                    }
                    createInfo.enabledLayerCount = validationLayers.size();
                    createInfo.ppEnabledLayerNames = validationLayers.data();
                }
#endif
            }

            void AddInstanceExtensionLayer(const char* ExtensionLayer)
            {
                instanceExtensions.push_back(ExtensionLayer);
                createInfo.ppEnabledExtensionNames = instanceExtensions.data();
                createInfo.enabledExtensionCount = instanceExtensions.size();
            }
            
            VkInstance build()
            {
                //two line error checking
                VkResult e =  vkCreateInstance(&createInfo, nullptr, &instance);
                if (e != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan instance");
                return instance;
            }
        private:
            VkInstance instance;
            VkApplicationInfo appInfo = {};
            VkInstanceCreateInfo createInfo = {};
            std::vector<const char*> instanceExtensions;
            std::vector<const char*> validationLayers =
                {
                "VK_LAYER_KHRONOS_validation"
                };
        };
    }
}

