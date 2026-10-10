#define VK_USE_PLATFORM_ANDROID_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "vulkan_bootstrap.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace re4::port {
namespace {
void ok(VkResult r,const char *operation) {
    if(r!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": VkResult="+std::to_string(r));
}
template<class T> T info(VkStructureType type) { T v{}; v.sType=type; return v; }
}
struct VulkanBootstrap::Impl {
#define VK_INSTANCE_FUNCTIONS(X) \
 X(DestroyInstance) X(CreateAndroidSurfaceKHR) X(DestroySurfaceKHR) \
 X(EnumeratePhysicalDevices) X(GetPhysicalDeviceQueueFamilyProperties) X(GetPhysicalDeviceSurfaceSupportKHR) \
 X(GetPhysicalDeviceProperties) X(GetPhysicalDeviceFeatures) X(GetPhysicalDeviceMemoryProperties) \
 X(GetPhysicalDeviceSurfaceCapabilitiesKHR) X(GetPhysicalDeviceSurfaceFormatsKHR) X(GetPhysicalDeviceFormatProperties) \
 X(CreateDevice)
#define VK_DEVICE_FUNCTIONS(X) \
 X(DestroyDevice) X(GetDeviceQueue) X(DeviceWaitIdle) X(CreateSwapchainKHR) X(DestroySwapchainKHR) \
 X(GetSwapchainImagesKHR) X(AcquireNextImageKHR) X(QueuePresentKHR) X(CreateCommandPool) X(DestroyCommandPool) \
 X(AllocateCommandBuffers) X(ResetCommandBuffer) X(BeginCommandBuffer) X(EndCommandBuffer) X(CmdPipelineBarrier) \
 X(CmdClearColorImage) X(CmdClearDepthStencilImage) X(CmdCopyImageToBuffer) X(QueueSubmit) X(QueueWaitIdle) \
 X(CreateSemaphore) X(DestroySemaphore) X(CreateFence) X(DestroyFence) X(WaitForFences) X(ResetFences) \
 X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindImageMemory) \
 X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(BindBufferMemory) X(MapMemory) X(UnmapMemory)
#define DECLARE(name) PFN_vk##name name{};
    DECLARE(CreateInstance)
    VK_INSTANCE_FUNCTIONS(DECLARE)
    VK_DEVICE_FUNCTIONS(DECLARE)
#undef DECLARE
    void *library{};
    PFN_vkGetInstanceProcAddr get{};
    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    VkQueue queue{};
    u32 family{};
    VkSwapchainKHR swap{};
    VkExtent2D extent{};
    VkCompositeAlphaFlagBitsKHR compositeAlpha{};
    std::vector<VkImage> images;
    VkCommandPool pool{};
    VkCommandBuffer command{};
    VkSemaphore acquired{},finished{};
    VkFence fence{};
    struct Target { VkImage image{}; VkDeviceMemory memory{}; } color,depth;
    VkBuffer readback{};
    VkDeviceMemory readMemory{};
    void *mapped{};
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    u64 presents{}, verified{};
    std::array<u8,4> lastPixel{};
    float lastDepth{};
    ANativeWindow *window{};
    Impl(ANativeWindow *w):window(w) {
        ANativeWindow_acquire(w);
        try { initialize(); } catch(...) { cleanup(); throw; }
    }
    ~Impl() { cleanup(); }
    u32 memoryType(u32 mask,VkMemoryPropertyFlags required) {
        for(u32 i=0;i<memoryProperties.memoryTypeCount;++i)
            if((mask&(1u<<i)) && (memoryProperties.memoryTypes[i].propertyFlags&required)==required) return i;
        throw std::runtime_error("Vulkan bootstrap memory type unsupported");
    }
    void initialize() {
        library=dlopen("libvulkan.so",RTLD_NOW|RTLD_LOCAL);
        if(!library) throw std::runtime_error("Android Vulkan loader unavailable (requires API24 or newer)");
        get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library,"vkGetInstanceProcAddr"));
        if(!get) throw std::runtime_error("Vulkan entry point missing");
        CreateInstance=reinterpret_cast<PFN_vkCreateInstance>(get(nullptr,"vkCreateInstance"));
        if(!CreateInstance) throw std::runtime_error("Vulkan vkCreateInstance missing");
        const char *extensions[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
        auto application=info<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
        application.pApplicationName="RE4 original GX bootstrap"; application.apiVersion=VK_API_VERSION_1_0;
        auto create=info<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
        create.pApplicationInfo=&application; create.enabledExtensionCount=2; create.ppEnabledExtensionNames=extensions;
        ok(CreateInstance(&create,nullptr,&instance),"vkCreateInstance");
#define LOAD(name) name=reinterpret_cast<PFN_vk##name>(get(instance,"vk" #name)); if(!name) throw std::runtime_error("Vulkan function missing: vk" #name);
        VK_INSTANCE_FUNCTIONS(LOAD)
#undef LOAD
        auto android=info<VkAndroidSurfaceCreateInfoKHR>(VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR);
        android.window=window;
        ok(CreateAndroidSurfaceKHR(instance,&android,nullptr,&surface),"vkCreateAndroidSurfaceKHR");
        u32 count=0; ok(EnumeratePhysicalDevices(instance,&count,nullptr),"vkEnumeratePhysicalDevices");
        std::vector<VkPhysicalDevice> devices(count); ok(EnumeratePhysicalDevices(instance,&count,devices.data()),"vkEnumeratePhysicalDevices");
        for(auto d:devices) {
            u32 n=0; GetPhysicalDeviceQueueFamilyProperties(d,&n,nullptr);
            std::vector<VkQueueFamilyProperties> families(n); GetPhysicalDeviceQueueFamilyProperties(d,&n,families.data());
            for(u32 i=0;i<n;++i) {
                VkBool32 supported=VK_FALSE; ok(GetPhysicalDeviceSurfaceSupportKHR(d,i,surface,&supported),"vkGetPhysicalDeviceSurfaceSupportKHR");
                if(supported && (families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)) { physical=d; family=i; break; }
            }
            if(physical) break;
        }
        if(!physical) throw std::runtime_error("No graphics/presentation queue for Android surface");
        GetPhysicalDeviceProperties(physical,&properties); GetPhysicalDeviceFeatures(physical,&features);
        GetPhysicalDeviceMemoryProperties(physical,&memoryProperties);
        float priority=1;
        auto q=info<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
        q.queueFamilyIndex=family; q.queueCount=1; q.pQueuePriorities=&priority;
        VkPhysicalDeviceFeatures enabled{}; enabled.samplerAnisotropy=features.samplerAnisotropy;
        auto dc=info<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        const char *swapExtension=VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        dc.queueCreateInfoCount=1; dc.pQueueCreateInfos=&q; dc.enabledExtensionCount=1;
        dc.ppEnabledExtensionNames=&swapExtension; dc.pEnabledFeatures=&enabled;
        ok(CreateDevice(physical,&dc,nullptr,&device),"vkCreateDevice");
        // Device entry points must come from vkGetDeviceProcAddr, not a different ICD.
        auto getDevice=reinterpret_cast<PFN_vkGetDeviceProcAddr>(get(instance,"vkGetDeviceProcAddr"));
        if(!getDevice) throw std::runtime_error("vkGetDeviceProcAddr missing");
#define DEVICE(name) name=reinterpret_cast<PFN_vk##name>(getDevice(device,"vk" #name)); if(!name) throw std::runtime_error("Vulkan device function missing: vk" #name);
        VK_DEVICE_FUNCTIONS(DEVICE)
#undef DEVICE
        GetDeviceQueue(device,family,0,&queue);
        createSwapchain();
        auto pc=info<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        pc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pc.queueFamilyIndex=family;
        ok(CreateCommandPool(device,&pc,nullptr,&pool),"vkCreateCommandPool");
        auto ca=info<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        ca.commandPool=pool; ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ca.commandBufferCount=1;
        ok(AllocateCommandBuffers(device,&ca,&command),"vkAllocateCommandBuffers");
        auto sem=info<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
        ok(CreateSemaphore(device,&sem,nullptr,&acquired),"vkCreateSemaphore");
        ok(CreateSemaphore(device,&sem,nullptr,&finished),"vkCreateSemaphore");
        auto fc=info<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        ok(CreateFence(device,&fc,nullptr,&fence),"vkCreateFence");
        makeTarget(color,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        makeTarget(depth,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        auto bc=info<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        bc.size=8; bc.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT; bc.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        ok(CreateBuffer(device,&bc,nullptr,&readback),"vkCreateBuffer");
        VkMemoryRequirements req{}; GetBufferMemoryRequirements(device,readback,&req);
        auto ma=info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        ma.allocationSize=req.size; ma.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        ok(AllocateMemory(device,&ma,nullptr,&readMemory),"vkAllocateMemory/readback");
        ok(BindBufferMemory(device,readback,readMemory,0),"vkBindBufferMemory");
        ok(MapMemory(device,readMemory,0,8,0,&mapped),"vkMapMemory");
    }
    void createSwapchain() {
        VkSurfaceCapabilitiesKHR caps{}; ok(GetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps),"vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        if(!(caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)) throw std::runtime_error("Surface lacks transfer clear usage");
        // Android SurfaceView may expose INHERIT only. Every presented pixel has
        // alpha=1, so every supported composition mode preserves opaque VI output.
        compositeAlpha={};
        for(auto mode:{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                       VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
            if(caps.supportedCompositeAlpha&mode) { compositeAlpha=mode; break; }
        if(!compositeAlpha) throw std::runtime_error("Surface lacks a supported composition mode");
        u32 count=0; ok(GetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,nullptr),"vkGetPhysicalDeviceSurfaceFormatsKHR");
        std::vector<VkSurfaceFormatKHR> formats(count); ok(GetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,formats.data()),"vkGetPhysicalDeviceSurfaceFormatsKHR");
        auto format=std::find_if(formats.begin(),formats.end(),[](auto f){return f.format==VK_FORMAT_R8G8B8A8_UNORM && f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;});
        if(format==formats.end()) throw std::runtime_error("No verified RGBA8 UNORM Android surface format");
        extent=caps.currentExtent;
        if(extent.width==UINT32_MAX) {
            extent.width=std::clamp(u32(ANativeWindow_getWidth(window)),caps.minImageExtent.width,caps.maxImageExtent.width);
            extent.height=std::clamp(u32(ANativeWindow_getHeight(window)),caps.minImageExtent.height,caps.maxImageExtent.height);
        }
        if(!extent.width || !extent.height) throw std::runtime_error("Android surface is suspended");
        auto sc=info<VkSwapchainCreateInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
        sc.surface=surface; sc.minImageCount=caps.minImageCount+1;
        if(caps.maxImageCount) sc.minImageCount=std::min(sc.minImageCount,caps.maxImageCount);
        sc.imageFormat=format->format; sc.imageColorSpace=format->colorSpace; sc.imageExtent=extent; sc.imageArrayLayers=1;
        sc.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT; sc.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
        sc.preTransform=caps.currentTransform; sc.compositeAlpha=compositeAlpha;
        sc.presentMode=VK_PRESENT_MODE_FIFO_KHR; sc.clipped=VK_TRUE;
        ok(CreateSwapchainKHR(device,&sc,nullptr,&swap),"vkCreateSwapchainKHR");
        ok(GetSwapchainImagesKHR(device,swap,&count,nullptr),"vkGetSwapchainImagesKHR"); images.resize(count);
        ok(GetSwapchainImagesKHR(device,swap,&count,images.data()),"vkGetSwapchainImagesKHR");
    }
    void makeTarget(Target &target,VkFormat format,VkImageUsageFlags usage) {
        VkFormatProperties fp{}; GetPhysicalDeviceFormatProperties(physical,format,&fp);
        const auto required=usage==VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT?VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT:VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if(!(fp.optimalTilingFeatures&required)) throw std::runtime_error("Required EFB color/depth format unsupported");
        auto ci=info<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        // Render_init -> ScreenReSize(512,448), src/game/main_sub.cpp:123.
        ci.imageType=VK_IMAGE_TYPE_2D; ci.format=format; ci.extent={512,448,1}; ci.mipLevels=1; ci.arrayLayers=1;
        ci.samples=VK_SAMPLE_COUNT_1_BIT; ci.tiling=VK_IMAGE_TILING_OPTIMAL;
        ci.usage=usage|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE; ci.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        ok(CreateImage(device,&ci,nullptr,&target.image),"vkCreateImage/EFB");
        VkMemoryRequirements req{}; GetImageMemoryRequirements(device,target.image,&req);
        auto ma=info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        ma.allocationSize=req.size; ma.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        ok(AllocateMemory(device,&ma,nullptr,&target.memory),"vkAllocateMemory/EFB");
        ok(BindImageMemory(device,target.image,target.memory,0),"vkBindImageMemory/EFB");
    }
    void barrier(VkImage image,VkImageAspectFlags aspect,VkImageLayout oldLayout,VkImageLayout next,VkAccessFlags src,VkAccessFlags dst) {
        auto b=info<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
        b.oldLayout=oldLayout; b.newLayout=next; b.srcAccessMask=src; b.dstAccessMask=dst;
        b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.image=image; b.subresourceRange={aspect,0,1,0,1};
        CmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
    }
    void present(const GraphicsClear &clear,bool black) {
        u32 index=0;
        auto acquiredResult=AcquireNextImageKHR(device,swap,1000000000ULL,acquired,VK_NULL_HANDLE,&index);
        if(acquiredResult==VK_ERROR_OUT_OF_DATE_KHR) {
            ok(DeviceWaitIdle(device),"vkDeviceWaitIdle/recreate"); DestroySwapchainKHR(device,swap,nullptr); swap={}; createSwapchain();
            acquiredResult=AcquireNextImageKHR(device,swap,1000000000ULL,acquired,VK_NULL_HANDLE,&index);
        }
        if(acquiredResult!=VK_SUBOPTIMAL_KHR) ok(acquiredResult,"vkAcquireNextImageKHR");
        ok(ResetCommandBuffer(command,0),"vkResetCommandBuffer");
        auto begin=info<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; ok(BeginCommandBuffer(command,&begin),"vkBeginCommandBuffer");
        VkClearColorValue colorValue{}; for(u32 i=0;i<4;++i) colorValue.float32[i]=float(clear.rgba[i])/255.f;
        VkImageSubresourceRange cr{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1},dr{VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
        barrier(color.image,cr.aspectMask,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        barrier(depth.image,dr.aspectMask,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        CmdClearColorImage(command,color.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&colorValue,1,&cr);
        VkClearDepthStencilValue dv{float(clear.depth)/float(0xffffff),0};
        CmdClearDepthStencilImage(command,depth.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&dv,1,&dr);
        barrier(color.image,cr.aspectMask,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        barrier(depth.image,dr.aspectMask,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={1,1,1};
        CmdCopyImageToBuffer(command,color.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&copy);
        copy.bufferOffset=4; copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
        CmdCopyImageToBuffer(command,depth.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&copy);
        auto host=info<VkBufferMemoryBarrier>(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        host.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        host.buffer=readback; host.size=8;
        CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        barrier(images[index],cr.aspectMask,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        auto displayColor=colorValue; if(black) displayColor={{0,0,0,1}}; else displayColor.float32[3]=1;
        CmdClearColorImage(command,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&displayColor,1,&cr);
        barrier(images[index],cr.aspectMask,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,VK_ACCESS_TRANSFER_WRITE_BIT,0);
        ok(EndCommandBuffer(command),"vkEndCommandBuffer");
        auto submit=info<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
        submit.waitSemaphoreCount=1; submit.pWaitSemaphores=&acquired; submit.pWaitDstStageMask=&stage;
        submit.commandBufferCount=1; submit.pCommandBuffers=&command; submit.signalSemaphoreCount=1; submit.pSignalSemaphores=&finished;
        ok(ResetFences(device,1,&fence),"vkResetFences"); ok(QueueSubmit(queue,1,&submit,fence),"vkQueueSubmit");
        auto p=info<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
        p.waitSemaphoreCount=1; p.pWaitSemaphores=&finished; p.swapchainCount=1; p.pSwapchains=&swap; p.pImageIndices=&index;
        auto result=QueuePresentKHR(queue,&p);
        if(result!=VK_SUBOPTIMAL_KHR && result!=VK_ERROR_OUT_OF_DATE_KHR) ok(result,"vkQueuePresentKHR");
        // Deliberately serialized bootstrap: proves ownership and readback. Future
        // GX draw rendering requires pipelined per-image fences, not this diagnostic path.
        ok(WaitForFences(device,1,&fence,VK_TRUE,1000000000ULL),"vkWaitForFences/readback");
        ok(QueueWaitIdle(queue),"vkQueueWaitIdle/presentation semaphore");
        std::memcpy(lastPixel.data(),mapped,4); std::memcpy(&lastDepth,static_cast<u8*>(mapped)+4,4);
        if(lastPixel!=clear.rgba || std::fabs(lastDepth-dv.depth)>1e-6f) throw std::runtime_error("GPU EFB clear readback differs from original GX BP state");
        ++verified;
        // A completed clear is not a successful display presentation. Keep the
        // GraphicsPresenter contract truthful if the surface changed meanwhile.
        if(result==VK_ERROR_OUT_OF_DATE_KHR)
            throw std::runtime_error("Vulkan surface changed during presentation; recreate Surface");
        ++presents;
    }
    void cleanup() noexcept {
        if(device) {
            if(DeviceWaitIdle) DeviceWaitIdle(device);
            if(mapped && UnmapMemory) UnmapMemory(device,readMemory);
            if(readback && DestroyBuffer) DestroyBuffer(device,readback,nullptr);
            if(readMemory && FreeMemory) FreeMemory(device,readMemory,nullptr);
            for(auto *t:{&color,&depth}) {
                if(t->image && DestroyImage) DestroyImage(device,t->image,nullptr);
                if(t->memory && FreeMemory) FreeMemory(device,t->memory,nullptr);
            }
            if(fence && DestroyFence) DestroyFence(device,fence,nullptr);
            if(acquired && DestroySemaphore) DestroySemaphore(device,acquired,nullptr);
            if(finished && DestroySemaphore) DestroySemaphore(device,finished,nullptr);
            if(pool && DestroyCommandPool) DestroyCommandPool(device,pool,nullptr);
            if(swap && DestroySwapchainKHR) DestroySwapchainKHR(device,swap,nullptr);
            if(DestroyDevice) DestroyDevice(device,nullptr);
        }
        if(surface && DestroySurfaceKHR) DestroySurfaceKHR(instance,surface,nullptr);
        if(instance && DestroyInstance) DestroyInstance(instance,nullptr);
        if(library) dlclose(library);
        if(window) ANativeWindow_release(window);
    }
};
VulkanBootstrap::VulkanBootstrap(ANativeWindow *window):impl_(std::make_unique<Impl>(window)) {}
VulkanBootstrap::~VulkanBootstrap()=default;
void VulkanBootstrap::present(const GraphicsClear &c,bool b) { impl_->present(c,b); }
std::string VulkanBootstrap::report() const {
    const auto &v=*impl_; std::ostringstream out;
    out << "{\"pointer_bits\":" << sizeof(void*)*8 << ",\"gpu\":" << std::quoted(v.properties.deviceName)
        << ",\"api\":" << v.properties.apiVersion << ",\"presents\":" << v.presents << ",\"verified_gpu_clears\":" << v.verified
        << ",\"surface\":[" << v.extent.width << ',' << v.extent.height << "],\"efb\":[512,448],\"composite_alpha\":" << v.compositeAlpha
        << ",\"sample_mask\":" << (v.properties.limits.framebufferColorSampleCounts&v.properties.limits.framebufferDepthSampleCounts)
        << ",\"anisotropy_supported\":" << (v.features.samplerAnisotropy?"true":"false")
        << ",\"max_anisotropy\":" << v.properties.limits.maxSamplerAnisotropy
        << ",\"active_samples\":1,\"active_anisotropy\":1,\"dof\":false,\"last_rgba\":["
        << unsigned(v.lastPixel[0]) << ',' << unsigned(v.lastPixel[1]) << ',' << unsigned(v.lastPixel[2]) << ',' << unsigned(v.lastPixel[3])
        << "],\"last_depth\":" << v.lastDepth << '}'; return out.str();
}
} // namespace re4::port
