#include <compare>

import VulkanRenderer;
import ServiceLocator;
int main()
{

    Rendering::Vulkan::VulkanRenderer r;
    //ServiceLocator::Instance()->RegisterSystem();
    r.initialize(2560, 1440);
    r.Render();
    r.shutdown();
    
}
