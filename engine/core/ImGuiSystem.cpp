module;
#include "imgui.h"
export module ImGuiSystem;

import std;
import ServiceLocator;
import VulkanRenderer;
namespace AngelBase::Core::Debug
{
    enum RenderingBackend
    {
        eVulkan,
        eDX12,
        eOpenGL
    };

    export struct IImGuiFunctions
    {
        virtual ~IImGuiFunctions() = default;
        virtual void init() = 0;
        virtual void cleanup() = 0;
        virtual void beginFrame() = 0;
        virtual void endFrame() = 0;
        virtual void render() = 0;
    };
    
    export class ImGuiSystem
    {
    public:
        ImGuiSystem(const RenderingBackend& render_api)
        {
            switch (render_api)
            {
            case RenderingBackend::eVulkan:
                {
                    
                }
                break;
                case RenderingBackend::eDX12:
                break;
                case RenderingBackend::eOpenGL:
                break;
            }
            
            
        }
    private:
        std::unique_ptr<IImGuiFunctions> functions;
    };
}

