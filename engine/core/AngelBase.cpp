#include <compare>
#include <imgui.h>

import VulkanRenderer;
import ServiceLocator;
import FileLoaderSystem;
import std;

class Engine
{
public:
    std::shared_ptr<Rendering::Vulkan::VulkanRenderer> renderer;
    std::shared_ptr<AngelBase::Core::FileLoaderSystem> fileLoaderSystem;
};

int main()
{
    Engine e;
    e.renderer = std::make_shared<Rendering::Vulkan::VulkanRenderer>();
    ServiceLocator::Instance()->RegisterSystem(e.renderer);
    e.fileLoaderSystem = std::make_shared<AngelBase::Core::FileLoaderSystem>();
    ServiceLocator::Instance()->RegisterSystem(e.fileLoaderSystem);
    e.renderer.get()->initialize(2560, 1440);
    e.renderer.get()->Render();
    e.renderer.get()->shutdown();
    
}
