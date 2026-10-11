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
#include <fstream>
#include <chrono>
#include <atomic>
#include <android/log.h>
#include "gx_spirv.h"
#include "re4/port/sha256.h"

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
 X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(BindBufferMemory) X(MapMemory) X(UnmapMemory) \
 X(CreateImageView) X(DestroyImageView) X(CreateSampler) X(DestroySampler) X(CmdCopyBufferToImage) X(CmdBlitImage) \
 X(CreateRenderPass) X(DestroyRenderPass) X(CreateFramebuffer) X(DestroyFramebuffer) \
 X(CreateShaderModule) X(DestroyShaderModule) X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) \
 X(CreatePipelineLayout) X(DestroyPipelineLayout) X(CreateGraphicsPipelines) X(DestroyPipeline) \
 X(CreateDescriptorPool) X(DestroyDescriptorPool) X(AllocateDescriptorSets) X(UpdateDescriptorSets) \
 X(CmdBeginRenderPass) X(CmdEndRenderPass) X(CmdBindPipeline) X(CmdBindVertexBuffers) X(CmdBindIndexBuffer) \
 X(CmdBindDescriptorSets) X(CmdPushConstants) X(CmdDrawIndexed)
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
    VkSurfaceTransformFlagBitsKHR surfaceTransform{};
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
    std::unique_ptr<GxScene> scene;
    struct Buffer { VkBuffer buffer{}; VkDeviceMemory memory{}; void *map{}; } vertexBuffer,indexBuffer,sceneReadback,materialBuffer;
    VkDeviceSize materialStride{};
    struct Texture { Target target; VkImageView view{}; VkSampler sampler{}; };
    std::vector<Texture> sceneTextures;
    VkImageView colorView{},depthView{};
    VkRenderPass renderPass{}; VkFramebuffer framebuffer{};
    VkDescriptorSetLayout descriptorLayout{}; VkPipelineLayout pipelineLayout{};
    VkDescriptorPool descriptorPool{}; std::vector<VkDescriptorSet> sets;
    VkPipeline pipelines[15]{};
    u64 geometryFrames{}, nonClearPixels{}, frameMicros{};
    std::string pixelHash;
    std::string sceneReport;
    u32 availableValidationLayers{};
    bool validationEnabled{};
    std::atomic<u32> validationWarnings{},validationErrors{};
    VkDebugUtilsMessengerEXT validationMessenger{};
    PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger{};
    static VKAPI_ATTR VkBool32 VKAPI_CALL validationMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT *data,void *owner) {
        auto &v=*static_cast<Impl*>(owner);
        if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)++v.validationErrors;
        else if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)++v.validationWarnings;
        __android_log_print(ANDROID_LOG_WARN,"RE4Validation","%s",data->pMessage?data->pMessage:"empty validation message");
        return VK_FALSE;
    }
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
        auto enumerateLayers=reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(get(nullptr,"vkEnumerateInstanceLayerProperties"));
        if(!enumerateLayers)throw std::runtime_error("Vulkan layer enumeration missing");
        u32 layerCount=0;ok(enumerateLayers(&layerCount,nullptr),"vkEnumerateInstanceLayerProperties");
        std::vector<VkLayerProperties> layers(layerCount);ok(enumerateLayers(&layerCount,layers.data()),"vkEnumerateInstanceLayerProperties");
        for(const auto &layer:layers)if(std::strcmp(layer.layerName,"VK_LAYER_KHRONOS_validation")==0)++availableValidationLayers;
        std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
        const char *validationLayer="VK_LAYER_KHRONOS_validation";
        auto debug=info<VkDebugUtilsMessengerCreateInfoEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
        debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.pfnUserCallback=validationMessage;debug.pUserData=this;
#ifndef NDEBUG
        if(availableValidationLayers){
            auto enumerate=reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(get(nullptr,"vkEnumerateInstanceExtensionProperties"));
            u32 count=0;ok(enumerate(nullptr,&count,nullptr),"validation extensions");std::vector<VkExtensionProperties> available(count);ok(enumerate(nullptr,&count,available.data()),"validation extensions");
            for(auto &extension:available)if(std::strcmp(extension.extensionName,VK_EXT_DEBUG_UTILS_EXTENSION_NAME)==0)validationEnabled=true;
            if(validationEnabled)extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
#endif
        auto application=info<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
        application.pApplicationName="RE4 original GX bootstrap"; application.apiVersion=VK_API_VERSION_1_0;
        auto create=info<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
        create.pApplicationInfo=&application; create.enabledExtensionCount=u32(extensions.size()); create.ppEnabledExtensionNames=extensions.data();
        if(validationEnabled){create.enabledLayerCount=1;create.ppEnabledLayerNames=&validationLayer;create.pNext=&debug;}
        ok(CreateInstance(&create,nullptr,&instance),"vkCreateInstance");
        if(validationEnabled){
            auto make=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(get(instance,"vkCreateDebugUtilsMessengerEXT"));
            destroyMessenger=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(get(instance,"vkDestroyDebugUtilsMessengerEXT"));
            if(!make||!destroyMessenger)throw std::runtime_error("Vulkan validation messenger missing");
            ok(make(instance,&debug,nullptr,&validationMessenger),"vkCreateDebugUtilsMessengerEXT");
        }
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
        VkFormatProperties blitProperties{};GetPhysicalDeviceFormatProperties(physical,format->format,&blitProperties);
        const VkFormatFeatureFlags blitRequired=VK_FORMAT_FEATURE_BLIT_SRC_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if((blitProperties.optimalTilingFeatures&blitRequired)!=blitRequired)
            throw std::runtime_error("Surface format lacks verified filtered EFB blit support");
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
        // EFB pixels are already in the app's orientation. Ask the compositor to
        // handle native-panel rotation, rather than falsely claiming prerotation.
        if(!(caps.supportedTransforms&VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR))
            throw std::runtime_error("Surface requires GX presentation prerotation; identity transform unsupported");
        surfaceTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        sc.preTransform=surfaceTransform; sc.compositeAlpha=compositeAlpha;
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
            clearScene();
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
        if(validationMessenger && destroyMessenger)destroyMessenger(instance,validationMessenger,nullptr);
        if(instance && DestroyInstance) DestroyInstance(instance,nullptr);
        if(library) dlclose(library);
        if(window) ANativeWindow_release(window);
    }
    void destroyBuffer(Buffer &b) noexcept {
        if(b.map) UnmapMemory(device,b.memory);
        if(b.buffer) DestroyBuffer(device,b.buffer,nullptr);
        if(b.memory) FreeMemory(device,b.memory,nullptr);
        b={};
    }
    void clearScene() noexcept {
        // Caller waits for the queue before any scene resource can be retired.
        for(auto &p:pipelines){if(p)DestroyPipeline(device,p,nullptr);p={};}
        if(framebuffer)DestroyFramebuffer(device,framebuffer,nullptr);framebuffer={};
        if(renderPass)DestroyRenderPass(device,renderPass,nullptr);renderPass={};
        if(pipelineLayout)DestroyPipelineLayout(device,pipelineLayout,nullptr);pipelineLayout={};
        if(descriptorPool)DestroyDescriptorPool(device,descriptorPool,nullptr);descriptorPool={};sets.clear();
        if(descriptorLayout)DestroyDescriptorSetLayout(device,descriptorLayout,nullptr);descriptorLayout={};
        for(auto &t:sceneTextures){if(t.sampler)DestroySampler(device,t.sampler,nullptr);if(t.view)DestroyImageView(device,t.view,nullptr);if(t.target.image)DestroyImage(device,t.target.image,nullptr);if(t.target.memory)FreeMemory(device,t.target.memory,nullptr);}sceneTextures.clear();
        if(colorView)DestroyImageView(device,colorView,nullptr);colorView={};
        if(depthView)DestroyImageView(device,depthView,nullptr);depthView={};
        destroyBuffer(vertexBuffer);destroyBuffer(indexBuffer);destroyBuffer(sceneReadback);destroyBuffer(materialBuffer);scene.reset();
        sceneReport.clear();pixelHash.clear();geometryFrames=nonClearPixels=frameMicros=0;
    }
    void buffer(Buffer &b,VkDeviceSize bytes,VkBufferUsageFlags usage,const void *data=nullptr) {
        auto ci=info<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);ci.size=bytes;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        ok(CreateBuffer(device,&ci,nullptr,&b.buffer),"vkCreateBuffer/GX");VkMemoryRequirements req{};GetBufferMemoryRequirements(device,b.buffer,&req);
        auto ma=info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);ma.allocationSize=req.size;ma.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        ok(AllocateMemory(device,&ma,nullptr,&b.memory),"vkAllocateMemory/GX buffer");ok(BindBufferMemory(device,b.buffer,b.memory,0),"vkBindBufferMemory/GX");ok(MapMemory(device,b.memory,0,bytes,0,&b.map),"vkMapMemory/GX");if(data)std::memcpy(b.map,data,static_cast<std::size_t>(bytes));
    }
    VkImageView view(VkImage image,VkFormat format,VkImageAspectFlags aspect,u32 levels=1) {
        auto ci=info<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);ci.image=image;ci.viewType=VK_IMAGE_VIEW_TYPE_2D;ci.format=format;ci.subresourceRange={aspect,0,levels,0,1};VkImageView v{};ok(CreateImageView(device,&ci,nullptr,&v),"vkCreateImageView/GX");return v;
    }
    void beginCommands() {
        ok(ResetCommandBuffer(command,0),"vkResetCommandBuffer/GX");auto ci=info<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);ci.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;ok(BeginCommandBuffer(command,&ci),"vkBeginCommandBuffer/GX");
    }
    void finishUpload() {
        ok(EndCommandBuffer(command),"vkEndCommandBuffer/GX upload");auto s=info<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);s.commandBufferCount=1;s.pCommandBuffers=&command;ok(ResetFences(device,1,&fence),"vkResetFences/upload");ok(QueueSubmit(queue,1,&s,fence),"vkQueueSubmit/upload");ok(WaitForFences(device,1,&fence,VK_TRUE,10000000000ULL),"vkWaitForFences/upload");
    }
    void texture(Texture &gpu,const GxTexture &t) {
        VkFormatProperties fp{};GetPhysicalDeviceFormatProperties(physical,VK_FORMAT_R8G8B8A8_UNORM,&fp);
        if(!(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))throw std::runtime_error("RGBA8 linear sampling unsupported");
        auto ci=info<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_R8G8B8A8_UNORM;ci.extent={t.levels[0].width,t.levels[0].height,1};ci.mipLevels=u32(t.levels.size());ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        ok(CreateImage(device,&ci,nullptr,&gpu.target.image),"vkCreateImage/GX texture");VkMemoryRequirements req{};GetImageMemoryRequirements(device,gpu.target.image,&req);auto ma=info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);ma.allocationSize=req.size;ma.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);ok(AllocateMemory(device,&ma,nullptr,&gpu.target.memory),"vkAllocateMemory/texture");ok(BindImageMemory(device,gpu.target.image,gpu.target.memory,0),"vkBindImageMemory/texture");
        gpu.view=view(gpu.target.image,ci.format,VK_IMAGE_ASPECT_COLOR_BIT,ci.mipLevels);
        auto sc=info<VkSamplerCreateInfo>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);sc.magFilter=t.magFilter?VK_FILTER_LINEAR:VK_FILTER_NEAREST;sc.minFilter=(t.minFilter==1||t.minFilter==4||t.minFilter==5)?VK_FILTER_LINEAR:VK_FILTER_NEAREST;sc.mipmapMode=(t.minFilter==3||t.minFilter==5)?VK_SAMPLER_MIPMAP_MODE_LINEAR:VK_SAMPLER_MIPMAP_MODE_NEAREST;
        auto address=[](u32 x){return x==0?VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:x==1?VK_SAMPLER_ADDRESS_MODE_REPEAT:VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;};sc.addressModeU=address(t.wrapS);sc.addressModeV=address(t.wrapT);sc.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sc.mipLodBias=t.bias;sc.minLod=float(t.minLod);sc.maxLod=t.minFilter<2?0.f:float(t.maxLod);sc.maxAnisotropy=1;
        if(std::fabs(sc.mipLodBias)>properties.limits.maxSamplerLodBias)throw std::runtime_error("original GX LOD bias exceeds device capability");ok(CreateSampler(device,&sc,nullptr,&gpu.sampler),"vkCreateSampler/GX");
        std::vector<u8> pixels;std::vector<VkBufferImageCopy> copies;
        for(u32 i=0;i<t.levels.size();++i){const auto &m=t.levels[i];VkBufferImageCopy c{};c.bufferOffset=pixels.size();c.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,i,0,1};c.imageExtent={m.width,m.height,1};copies.push_back(c);pixels.insert(pixels.end(),m.rgba.begin(),m.rgba.end());}
        Buffer staging;
        try{buffer(staging,pixels.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,pixels.data());beginCommands();auto b=info<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=gpu.target.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,ci.mipLevels,0,1};CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);CmdCopyBufferToImage(command,staging.buffer,gpu.target.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,u32(copies.size()),copies.data());b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);finishUpload();destroyBuffer(staging);}catch(...){DeviceWaitIdle(device);destroyBuffer(staging);throw;}
    }
    VkShaderModule shader(const u32 *data,std::size_t bytes) {
        auto ci=info<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);ci.codeSize=bytes;ci.pCode=data;VkShaderModule result{};ok(CreateShaderModule(device,&ci,nullptr,&result),"vkCreateShaderModule/GX");return result;
    }
    void loadScene(GxScene data) {
        if(!data.materialLightingReady)throw std::invalid_argument("GX geometry has no completed original camera/material/light state");
        ok(DeviceWaitIdle(device),"vkDeviceWaitIdle/scene replacement");clearScene();
        try {
            if(data.draws.empty()||data.vertices.empty()||data.indices.empty())throw std::invalid_argument("empty original geometry scene");scene=std::make_unique<GxScene>(std::move(data));
            sceneReport=scene->report(); // Immutable source/hash evidence; do not hash 16 MB each display frame.
            static_assert(sizeof(GxTevStage)==112 && offsetof(GxMaterialState,meta)==1856 && offsetof(GxMaterialState,lights)==1952 && offsetof(GxMaterialState,texgens)==2592);
            if(scene->materials.size()!=scene->draws.size())throw std::invalid_argument("GX draw/material state count");
            auto alignment=std::max<VkDeviceSize>(16,properties.limits.minUniformBufferOffsetAlignment);
            materialStride=(sizeof(GxMaterialState)+alignment-1)/alignment*alignment;
            if(sizeof(GxMaterialState)>properties.limits.maxUniformBufferRange)throw std::runtime_error("GX material exceeds uniform buffer range");
            buffer(materialBuffer,materialStride*scene->materials.size(),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            for(u32 i=0;i<scene->materials.size();++i)std::memcpy(static_cast<u8*>(materialBuffer.map)+materialStride*i,&scene->materials[i],sizeof(GxMaterialState));
            buffer(vertexBuffer,scene->vertices.size()*sizeof(GxVertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,scene->vertices.data());buffer(indexBuffer,scene->indices.size()*4,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,scene->indices.data());buffer(sceneReadback,512*448*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            sceneTextures.resize(scene->textures.size());for(u32 i=0;i<sceneTextures.size();++i)texture(sceneTextures[i],scene->textures[i]);
            colorView=view(color.image,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT);depthView=view(depth.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT);
            VkAttachmentDescription attachments[2]{};attachments[0].format=VK_FORMAT_R8G8B8A8_UNORM;attachments[1].format=VK_FORMAT_D32_SFLOAT;for(auto &a:attachments){a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;}attachments[0].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            VkAttachmentReference colorRef{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depthRef{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&colorRef;sub.pDepthStencilAttachment=&depthRef;
            VkSubpassDependency deps[2]{};deps[0].srcSubpass=VK_SUBPASS_EXTERNAL;deps[0].dstSubpass=0;deps[0].srcStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;deps[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;deps[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;deps[1].srcSubpass=0;deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;deps[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;deps[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;deps[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;deps[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            auto rc=info<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);rc.attachmentCount=2;rc.pAttachments=attachments;rc.subpassCount=1;rc.pSubpasses=&sub;rc.dependencyCount=2;rc.pDependencies=deps;ok(CreateRenderPass(device,&rc,nullptr,&renderPass),"vkCreateRenderPass/GX");
            VkImageView views[]={colorView,depthView};auto fc=info<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);fc.renderPass=renderPass;fc.attachmentCount=2;fc.pAttachments=views;fc.width=512;fc.height=448;fc.layers=1;ok(CreateFramebuffer(device,&fc,nullptr,&framebuffer),"vkCreateFramebuffer/GX");
            VkDescriptorSetLayoutBinding bindings[9]{};for(u32 i=0;i<9;++i){bindings[i].binding=i;bindings[i].descriptorType=i==8?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;bindings[i].descriptorCount=1;bindings[i].stageFlags=i==8?VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_FRAGMENT_BIT;}auto dl=info<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);dl.bindingCount=9;dl.pBindings=bindings;ok(CreateDescriptorSetLayout(device,&dl,nullptr,&descriptorLayout),"vkCreateDescriptorSetLayout/GX");
            VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,128};auto pl=info<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);pl.setLayoutCount=1;pl.pSetLayouts=&descriptorLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;ok(CreatePipelineLayout(device,&pl,nullptr,&pipelineLayout),"vkCreatePipelineLayout/GX");
            VkDescriptorPoolSize poolSizes[]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,u32(scene->draws.size())*8},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,u32(scene->draws.size())}};auto dp=info<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);dp.maxSets=u32(scene->draws.size());dp.poolSizeCount=2;dp.pPoolSizes=poolSizes;ok(CreateDescriptorPool(device,&dp,nullptr,&descriptorPool),"vkCreateDescriptorPool/GX");std::vector<VkDescriptorSetLayout> layouts(scene->draws.size(),descriptorLayout);sets.resize(scene->draws.size());auto alloc=info<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);alloc.descriptorPool=descriptorPool;alloc.descriptorSetCount=u32(sets.size());alloc.pSetLayouts=layouts.data();ok(AllocateDescriptorSets(device,&alloc,sets.data()),"vkAllocateDescriptorSets/GX");
            for(u32 i=0;i<sets.size();++i){auto &d=scene->draws[i];if(d.cull>2||d.blendMode>4)throw std::invalid_argument("GPU draw pipeline state");VkDescriptorImageInfo im[8]{};VkWriteDescriptorSet writes[9]{};for(u32 k=0;k<8;++k){auto id=scene->materials[i].textures[k];if(id>=sceneTextures.size())throw std::invalid_argument("GPU material texture reference");auto &t=sceneTextures[id];im[k]={t.sampler,t.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};writes[k]=info<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);writes[k].dstSet=sets[i];writes[k].dstBinding=k;writes[k].descriptorCount=1;writes[k].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[k].pImageInfo=&im[k];}VkDescriptorBufferInfo uniform{materialBuffer.buffer,materialStride*i,sizeof(GxMaterialState)};writes[8]=info<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);writes[8].dstSet=sets[i];writes[8].dstBinding=8;writes[8].descriptorCount=1;writes[8].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;writes[8].pBufferInfo=&uniform;UpdateDescriptorSets(device,9,writes,0,nullptr);}
            VkShaderModule vs{},fs{};
            try{vs=shader(gxVertexSpirv,sizeof(gxVertexSpirv));fs=shader(gxFragmentSpirv,sizeof(gxFragmentSpirv));VkPipelineShaderStageCreateInfo stages[2]={info<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO),info<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO)};stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vs;stages[0].pName="main";stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fs;stages[1].pName="main";
                VkVertexInputBindingDescription binding{0,sizeof(GxVertex),VK_VERTEX_INPUT_RATE_VERTEX};VkVertexInputAttributeDescription attributes[]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(GxVertex,position)},{1,0,VK_FORMAT_R32G32_SFLOAT,offsetof(GxVertex,uv)},{2,0,VK_FORMAT_R8G8B8A8_UNORM,offsetof(GxVertex,color)},{3,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(GxVertex,normal)},{4,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(GxVertex,rawNormal)}};auto vi=info<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&binding;vi.vertexAttributeDescriptionCount=5;vi.pVertexAttributeDescriptions=attributes;
                auto ia=info<VkPipelineInputAssemblyStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;VkViewport viewport{0,0,512,448,0,1};VkRect2D scissor{{0,0},{512,448}};auto vp=info<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);vp.viewportCount=1;vp.pViewports=&viewport;vp.scissorCount=1;vp.pScissors=&scissor;auto rs=info<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);rs.polygonMode=VK_POLYGON_MODE_FILL;
                // GX API FRONT/BACK are swapped by GXGeometry.c before the BP
                // register. With the Vulkan Y correction, API front is clockwise.
                rs.frontFace=VK_FRONT_FACE_CLOCKWISE;rs.lineWidth=1;
                auto ms=info<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;auto ds=info<VkPipelineDepthStencilStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);ds.depthTestEnable=ds.depthWriteEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
                VkPipelineColorBlendAttachmentState ba{};ba.blendEnable=VK_TRUE;ba.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;ba.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.colorBlendOp=VK_BLEND_OP_ADD;ba.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;ba.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.alphaBlendOp=VK_BLEND_OP_ADD;ba.colorWriteMask=15;auto bs=info<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);bs.attachmentCount=1;bs.pAttachments=&ba;
                auto pc=info<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);pc.stageCount=2;pc.pStages=stages;pc.pVertexInputState=&vi;pc.pInputAssemblyState=&ia;pc.pViewportState=&vp;pc.pRasterizationState=&rs;pc.pMultisampleState=&ms;pc.pDepthStencilState=&ds;pc.pColorBlendState=&bs;pc.layout=pipelineLayout;pc.renderPass=renderPass;
                for(u32 blend=0;blend<5;++blend){
                    // Exact factor rows of commonModelTrans's original bl[5][4].
                    const VkBlendFactor src[]={VK_BLEND_FACTOR_SRC_ALPHA,VK_BLEND_FACTOR_SRC_ALPHA,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_DST_COLOR,VK_BLEND_FACTOR_ONE};
                    const VkBlendFactor dst[]={VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ZERO};
                    ba.blendEnable=blend==4?VK_FALSE:VK_TRUE;ba.srcColorBlendFactor=ba.srcAlphaBlendFactor=src[blend];ba.dstColorBlendFactor=ba.dstAlphaBlendFactor=dst[blend];
                    for(u32 i=0;i<3;++i){rs.cullMode=i==0?VK_CULL_MODE_NONE:i==1?VK_CULL_MODE_FRONT_BIT:VK_CULL_MODE_BACK_BIT;ok(CreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pc,nullptr,&pipelines[blend*3+i]),"vkCreateGraphicsPipelines/GX");}
                }
                DestroyShaderModule(device,fs,nullptr);DestroyShaderModule(device,vs,nullptr);
            }catch(...){if(fs)DestroyShaderModule(device,fs,nullptr);if(vs)DestroyShaderModule(device,vs,nullptr);throw;}
        }catch(...){DeviceWaitIdle(device);clearScene();throw;}
    }
    void presentScene() {
        if(!scene)throw std::logic_error("original geometry scene not loaded");auto started=std::chrono::steady_clock::now();u32 image=0;auto result=AcquireNextImageKHR(device,swap,1000000000ULL,acquired,VK_NULL_HANDLE,&image);
        if(result==VK_ERROR_OUT_OF_DATE_KHR){ok(DeviceWaitIdle(device),"vkDeviceWaitIdle/geometry resize");DestroySwapchainKHR(device,swap,nullptr);swap={};createSwapchain();result=AcquireNextImageKHR(device,swap,1000000000ULL,acquired,VK_NULL_HANDLE,&image);}if(result!=VK_SUBOPTIMAL_KHR)ok(result,"vkAcquireNextImageKHR/geometry");beginCommands();
        VkClearValue clear[2]{};clear[0].color={{0,0,0,1}};clear[1].depthStencil={1,0};auto rp=info<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);rp.renderPass=renderPass;rp.framebuffer=framebuffer;rp.renderArea={{0,0},{512,448}};rp.clearValueCount=2;rp.pClearValues=clear;CmdBeginRenderPass(command,&rp,VK_SUBPASS_CONTENTS_INLINE);VkDeviceSize offset=0;CmdBindVertexBuffers(command,0,1,&vertexBuffer.buffer,&offset);CmdBindIndexBuffer(command,indexBuffer.buffer,0,VK_INDEX_TYPE_UINT32);
        struct Push{std::array<float,16> vp;float view[3][4];float reserved[4];} push{};static_assert(sizeof(Push)==128);push.vp=scene->viewProjection;std::memcpy(push.view,scene->camera.view,sizeof(push.view));
        for(auto i:scene->renderOrder){if(i>=scene->draws.size())throw std::invalid_argument("GPU OT draw reference");auto &d=scene->draws[i];CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[d.blendMode*3+d.cull]);CmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&sets[i],0,nullptr);CmdPushConstants(command,pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);CmdDrawIndexed(command,d.indexCount,1,d.firstIndex,0,0);}CmdEndRenderPass(command);
        VkBufferImageCopy read{};read.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};read.imageExtent={512,448,1};CmdCopyImageToBuffer(command,color.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,sceneReadback.buffer,1,&read);auto host=info<VkBufferMemoryBarrier>(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=sceneReadback.buffer;host.size=VK_WHOLE_SIZE;CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        barrier(images[image],VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkClearColorValue black{{0,0,0,1}};CmdClearColorImage(command,images[image],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&black,1,&range);
        // Transfer write -> write dependency: the blit covers only the centered VI rectangle.
        barrier(images[image],VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
        u32 width=std::min(extent.width,extent.height*4/3),height=width*3/4;VkImageBlit blit{};blit.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};blit.srcOffsets[1]={512,448,1};blit.dstSubresource=blit.srcSubresource;auto x=int((extent.width-width)/2),y=int((extent.height-height)/2);blit.dstOffsets[0]={x,y,0};blit.dstOffsets[1]={x+int(width),y+int(height),1};CmdBlitImage(command,color.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,images[image],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);barrier(images[image],VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,VK_ACCESS_TRANSFER_WRITE_BIT,0);
        ok(EndCommandBuffer(command),"vkEndCommandBuffer/geometry");auto submit=info<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&acquired;submit.pWaitDstStageMask=&stage;submit.commandBufferCount=1;submit.pCommandBuffers=&command;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&finished;ok(ResetFences(device,1,&fence),"vkResetFences/geometry");ok(QueueSubmit(queue,1,&submit,fence),"vkQueueSubmit/geometry");auto p=info<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);p.waitSemaphoreCount=1;p.pWaitSemaphores=&finished;p.swapchainCount=1;p.pSwapchains=&swap;p.pImageIndices=&image;result=QueuePresentKHR(queue,&p);if(result!=VK_SUBOPTIMAL_KHR)ok(result,"vkQueuePresentKHR/geometry");ok(WaitForFences(device,1,&fence,VK_TRUE,10000000000ULL),"vkWaitForFences/geometry");ok(QueueWaitIdle(queue),"vkQueueWaitIdle/geometry");
        const auto *bytes=static_cast<const u8*>(sceneReadback.map);nonClearPixels=0;for(u32 i=0;i<512*448;++i)if(bytes[i*4]||bytes[i*4+1]||bytes[i*4+2])++nonClearPixels;if(!nonClearPixels)throw std::runtime_error("geometry GPU frame contains no non-clear pixels");pixelHash=sha256(bytes,512*448*4);++geometryFrames;frameMicros=u64(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());
    }
};
VulkanBootstrap::VulkanBootstrap(ANativeWindow *window):impl_(std::make_unique<Impl>(window)) {}
VulkanBootstrap::~VulkanBootstrap()=default;
void VulkanBootstrap::present(const GraphicsClear &c,bool b) { impl_->present(c,b); }
void VulkanBootstrap::loadScene(GxScene s){impl_->loadScene(std::move(s));}
void VulkanBootstrap::presentScene(){impl_->presentScene();}
void VulkanBootstrap::saveSceneReadback(const std::string &path) const {
    if(!impl_->geometryFrames)throw std::logic_error("no completed geometry readback");std::ofstream out(path,std::ios::binary);out.write(static_cast<const char*>(impl_->sceneReadback.map),512*448*4);if(!out)throw std::runtime_error("cannot save GPU RGBA readback");
}
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
        << "],\"last_depth\":" << v.lastDepth
        << ",\"geometry_frames\":"<<v.geometryFrames<<",\"non_clear_pixels\":"<<v.nonClearPixels
        << ",\"surface_transform\":"<<v.surfaceTransform
        << ",\"rgba_sha256\":"<<std::quoted(v.pixelHash)<<",\"last_frame_cpu_us\":"<<v.frameMicros
        << ",\"available_khronos_validation_layers\":"<<v.availableValidationLayers
        << ",\"validation_enabled\":"<<(v.validationEnabled?"true":"false")
        << ",\"validation_warnings\":"<<v.validationWarnings.load()
        << ",\"validation_errors\":"<<v.validationErrors.load();
    if(v.scene)out<<",\"scene\":"<<v.sceneReport;out<<'}';return out.str();
}
} // namespace re4::port
