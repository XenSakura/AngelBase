#include <compare>
#include <imgui.h>

import VulkanRenderer;
import ServiceLocator;
import FileLoaderSystem;
import std;

class Engine
{
public:
    Rendering::Vulkan::VulkanRenderer* renderer;
    AngelBase::Core::FileLoaderSystem* fileLoaderSystem;
};

int main()
{
    Engine e;
    e.renderer = new Rendering::Vulkan::VulkanRenderer();
    ServiceLocator::Instance()->RegisterSystem(e.renderer);
    e.fileLoaderSystem = new AngelBase::Core::FileLoaderSystem();
    ServiceLocator::Instance()->RegisterSystem(e.fileLoaderSystem);
    e.renderer->initialize(2560, 1440);
    e.renderer->Render();
    e.renderer->shutdown();
    
}
