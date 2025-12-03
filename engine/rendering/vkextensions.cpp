module;
#include <vulkan/vulkan.hpp>
#ifdef _DEBUG
#define DEBUG_ONLY(code) code
#else
#define DEBUG_ONLY(code)
#endif

export module vkextensions;

import std;


namespace rendering
{
    namespace vkextensions
    {
        static bool has_debug_utils = false;
        
        export static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                                   vk::DebugUtilsMessageTypeFlagsEXT             message_types,
                                                   vk::DebugUtilsMessengerCallbackDataEXT const *callback_data,
                                                   void                                         *user_data)
        {
            std::ostringstream msg;
            msg << callback_data->messageIdNumber << " Validation Layer: ";
        
            if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
            {
                std::cerr << msg.str() << "Error: " << callback_data->pMessageIdName 
                          << ": " << callback_data->pMessage << std::endl;
            }
            else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
            {
                std::cerr << msg.str() << "Warning: " << callback_data->pMessageIdName 
                          << ": " << callback_data->pMessage << std::endl;
            }
            else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
            {
                std::cout << msg.str() << "Information: " << callback_data->pMessageIdName 
                          << ": " << callback_data->pMessage << std::endl;
            }
            else if (message_types & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
            {
                std::cout << msg.str() << "Performance warning: " << callback_data->pMessageIdName 
                          << ": " << callback_data->pMessage << std::endl;
            }
            else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose)
            {
                std::cout << msg.str() << "Verbose: " << callback_data->pMessageIdName 
                          << ": " << callback_data->pMessage << std::endl;
            }
            return false;
        }

        //  @brief: Checks to see we have necessary extensions
        export bool validate_extensions(const std::vector<const char *>            &required,
                                                  const std::vector<vk::ExtensionProperties> &available)
        {
            return std::ranges::all_of(required,
            [&available](auto const &extension_name) {
                bool found = std::ranges::any_of(
                available, [&extension_name](auto const &ep) { return strcmp(ep.extensionName, extension_name) == 0; });
                if (!found)
                {
                    // Output an error message for the missing extension
                    std::cerr << "Error: Required extension not found: " +  extension_name;
                }
                return found;
            });
        }

        // @brief: Adding instance extensions for debug messenger
        export void AddInstanceExtensions(std::vector<const char*>& required_instance_extensions)
        {
            std::vector<vk::ExtensionProperties> available_instance_extensions = vk::enumerateInstanceExtensionProperties();

    #ifdef _DEBUG
            has_debug_utils = std::ranges::any_of(
            available_instance_extensions,
            [](auto const &ep) { return strncmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME, strlen(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) == 0; });
            if (has_debug_utils)
            {
                required_instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
            else
            {
                std::cerr << std::string(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) +  " is not available; disabling debug utils messenger";
            }
    #endif

            if (!validate_extensions(required_instance_extensions, available_instance_extensions))
            {
                throw std::runtime_error("Required instance extensions are missing.");
            }
        }

        //@brief: Adding instance layers for validation layers
        export void AddInstanceLayers(std::vector<const char*> &requested_instance_layers)
        {
    #ifdef _DEBUG
            char const *validationLayer = "VK_LAYER_KHRONOS_validation";

            std::vector<vk::LayerProperties> supported_instance_layers = vk::enumerateInstanceLayerProperties();

            if (std::ranges::any_of(supported_instance_layers, [&validationLayer](auto const &lp) { return strcmp(lp.layerName, validationLayer) == 0; }))
            {
                requested_instance_layers.push_back(validationLayer);
                std::cerr << "Enabled Validation Layer " + std::string(validationLayer);
            }
            else
            {
                std::cerr << "Validation Layer " +  std::string(validationLayer) + " is not available";
            }
    #endif
        }

        //@brief: Adding the debug messenger to instance info
        export static std::optional<vk::DebugUtilsMessengerCreateInfoEXT> CreateDebugMessengerInfo()
        {
#ifdef _DEBUG
            if (has_debug_utils)
            {
                vk::DebugUtilsMessengerCreateInfoEXT info;
                info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | 
                                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
                info.messageType     = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | 
                                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
                info.pfnUserCallback = debug_callback;
                return info;
            }
#endif
            return std::nullopt;
        }
    }
}