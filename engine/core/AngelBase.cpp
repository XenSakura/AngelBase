#include <compare>

import VulkanRenderer;

int main()
{
    Rendering::Vulkan::VulkanRenderer r;
    r.initialize(2560, 1440);
    r.Render();
    r.shutdown();
    
}
