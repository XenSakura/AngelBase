module;

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

export module vkwindow;
import std;

namespace rendering
{
	//custom wrapper since I can't use smart pointers for this
	struct Window
	{
		GLFWwindow* _window;
		Window()
		{
			_window = glfwCreateWindow(1700, 900, "Vulkan Engine", nullptr, nullptr);
		}
		~Window()
		{
			glfwDestroyWindow(_window);
		}
	};


	//every time the engine needs to make a different window, one of each needs to be remade
	export struct vkwindow
	{
		//handle vulkan stuff in a separate function from this
		vkwindow()
		{
			//TODO: Hard coded values: need to be serialized from JSON
			_extent = {1700, 900};
			window = std::make_unique<Window>();
		}
		//TODO: Window Resizing
		void ResizeWindow(VkExtent2D extent)
		{
			
			_extent = extent;
		}
		
		~vkwindow()
		{
			
		}
		//window we'll be using to initialize things
		std::unique_ptr<Window> window;
		//TODO: Make a builder for selecting relevant settings

		//Used for both swapchain and the window
		VkExtent2D _extent;
		VkSurfaceKHR _surface;
		//should I bundle all of these under a vulkan swapchain thingy?
		//TODO: Make swapchain builder
		VkSwapchainKHR _swapchain;
		VkFormat _swapchainImageFormat;
		std::vector<VkImage> _swapchainImages;
		std::vector<VkImageView> _swapchainImageViews;
	};

	export class WindowManager
	{
	public:
		WindowManager()
		{
			//calls VulkanWindow constructor
			_window = std::make_unique<vkwindow>();
		}
		~WindowManager()
		{
			
		}

		
		
	private:
		//TODO: for now, only one window. Will look into having multiple
		std::unique_ptr<vkwindow> _window;
		//std::vector<VulkanWindow> _windows;
	};
	
}
