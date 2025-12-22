# Add core source files to the engine target
target_sources(AngelBase PRIVATE
    VulkanContext.cpp
    WindowManager.cpp
    IRenderer.cpp
    RenderTargetManager.cpp
    VulkanCommand.cpp
    VulkanDescriptors.cpp
    VulkanExtensions.cpp
    VulkanSwapchain.cpp
)

# Make this folder available for includes
target_include_directories(AngelBase PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

