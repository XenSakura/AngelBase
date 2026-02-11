module;

#include <GLFW/glfw3.h>
export module IRenderer;

import std;
import WindowManager;
import VulkanContext;
//the only file that should be accessed outside of rendering
class MeshData;
class MaterialProperties;
class TextureDesc;
class Vec3;
class DrawCommand;
class Camera;
class RenderPassDesc;
class Vec4;



export class IRenderer
{
public:

    
    virtual ~IRenderer() = default;
    
    // ===== Initialization =====
    virtual bool initialize(uint32_t width, uint32_t height) = 0;
    virtual void shutdown() = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
    
   
    // ===== Frame Rendering =====
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;
    
    virtual void begin_render_pass(const RenderPassDesc& desc) = 0;
    virtual void end_render_pass() = 0;
    
    virtual void set_camera(const Camera& camera) = 0;
    
    // Submit draws - can be called from multiple threads during frame
    // Implementation should use lock-free queues or thread-local buffers
    virtual void submit_draw(const DrawCommand& cmd) = 0;
    virtual void submit_draws(std::span<const DrawCommand> commands) = 0;
    
    // ===== Immediate Mode (for debug/UI) =====
    virtual void draw_line(Vec3 start, Vec3 end, Vec4 color) = 0;
    virtual void draw_box(Vec3 min, Vec3 max, Vec4 color) = 0;
    virtual void draw_sphere(Vec3 center, float radius, Vec4 color) = 0;
    
    // ===== Query =====
    virtual uint32_t get_frame_index() const = 0;
    virtual void wait_idle() = 0;

protected:
    GLFWwindow* window;
    
};