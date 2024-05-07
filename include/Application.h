#ifndef APPLICATION_H
#define APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <memory>

#include <stb_image.h>
#include "Camera.h"
#include "Helper.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <stdio.h>          // printf, fprintf
#include <stdlib.h>         // abort

//#define APP_USE_UNLIMITED_FRAME_RATE (This and the following was added by imgui)
#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
#endif

struct QueueFamilyIndices 
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() 
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails 
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Application 
{
public:
    Application(std::string app_name);
    void run();

    // Imgui
    ImGuiIO* io;
    ImGui_ImplVulkanH_Window g_MainWindowData;
    int g_MinImageCount = 2;
    bool g_SwapChainRebuild = false;

    uint32_t WIDTH = 1920;
    uint32_t HEIGHT = 1080;
    std::string APP_NAME;
    const int MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t currentFrame = 0;
    bool framebufferResized = false;

    double currentFrameTime = 0.0;
    double lastFrameTime = 0.0;
    double deltaTime;

    virtual void key_callback_extended(GLFWwindow* window, int key, int scancode, int action, int mods, double deltaTime) = 0;
    virtual void mouse_callback_extended(GLFWwindow* window, int button, int action, int mods, double deltaTime) = 0;
    virtual void cursor_position_callback_extended(GLFWwindow* window, double xpos, double ypos) = 0;

#ifdef APP_USE_VULKAN_DEBUG_REPORT
    inline VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
    {
        (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
        fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
        return VK_FALSE;
    }
#endif // APP_USE_VULKAN_DEBUG_REPORT

protected:
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    std::shared_ptr<Helper> helper;

    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkSurfaceKHR surface;
    uint32_t imageCount;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    VkRenderPass swapChainRenderPass;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkDescriptorPool descriptorPool;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;


#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    void init_vulkan();

    void main_loop();

    virtual void main_loop_extended(uint32_t currentFrame, uint32_t imageIndex) = 0;

    void cleanup();

    virtual void cleanup_extended() = 0;

    void init_window();

    void create_instance();

    bool checkValidationLayerSupport();

    std::vector<const char*> getRequiredExtensions();

    void setupDebugMessenger();

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void pickPhysicalDevice();

    bool isDeviceSuitable(VkPhysicalDevice device);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    void createLogicalDevice();

    void createSurface();

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void createSwapChain();

    void createImageViews();

    void createCommandPool();

    void createCommandBuffers();

    virtual void recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex) = 0;

    void beginCommandBuffer();

    void createSyncObjects();

    void recreateSwapChain();

    void cleanupSwapChain();

    void createSwapChainFramebuffers();

    void createSwapChainRenderPass();

    void createDescriptorPool();

    void createDepthResources();

};

#endif // !APPLICATION_H
