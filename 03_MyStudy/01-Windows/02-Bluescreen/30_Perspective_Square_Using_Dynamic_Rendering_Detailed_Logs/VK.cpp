/*
    Vulkan_Ortho_Square_DynamicRendering.cpp
    -----------------------------------------
    A complete, self-contained example rendering a colored square with Vulkan,
    using:
      - Dynamic Rendering (no traditional VkRenderPass)
      - Layout transitions from UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
      - GLFW-style perspective for demonstration (glm::perspective)
      - Win32 surface creation
      - Minimal error checks

    Required SPIR-V shaders in the same directory:
      vert_shader.spv
      frag_shader.spv

    Compile & link:
      cl /std:c++17 /EHsc Vulkan_Ortho_Square_DynamicRendering.cpp /link vulkan-1.lib
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <fstream>
#include <array>
#include <iostream>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#pragma comment(lib, "vulkan-1.lib")

// ---------------------------------------------------------
// Constants / Globals
// ---------------------------------------------------------
static const char* gAppName = "Vulkan_Ortho_Square_DynamicRendering";
#define WIN_WIDTH  800
#define WIN_HEIGHT 600

HWND  ghwnd           = NULL;
BOOL  gbActive        = FALSE;
DWORD gdwStyle        = 0;
WINDOWPLACEMENT gwpPrev { sizeof(WINDOWPLACEMENT) };
BOOL  gbFullscreen    = FALSE;

FILE* gfpLog          = nullptr;
bool  gEnableValidation = true;  // Set to true to enable validation layers

// For the debug messenger (validation layer callback)
static VkDebugUtilsMessengerEXT gDebugUtilsMessenger = VK_NULL_HANDLE;

// Vulkan instance / devices / etc.
VkInstance              gInstance               = VK_NULL_HANDLE;
VkSurfaceKHR            gSurface                = VK_NULL_HANDLE;
VkPhysicalDevice        gPhysicalDevice         = VK_NULL_HANDLE;
VkDevice                gDevice                 = VK_NULL_HANDLE;
VkQueue                 gGraphicsQueue          = VK_NULL_HANDLE;
uint32_t                gGraphicsQueueIndex     = UINT32_MAX;
VkPhysicalDeviceMemoryProperties gMemProperties {};

// Swapchain
VkSwapchainKHR          gSwapchain             = VK_NULL_HANDLE;
VkFormat                gSwapchainFormat       = VK_FORMAT_UNDEFINED;
VkExtent2D              gSwapchainExtent       = {WIN_WIDTH, WIN_HEIGHT};
VkColorSpaceKHR         gColorSpace            = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR        gPresentMode           = VK_PRESENT_MODE_FIFO_KHR;

uint32_t                gSwapchainImageCount   = 0;
std::vector<VkImage>    gSwapchainImages;
std::vector<VkImageView> gSwapchainImageViews;

// Command buffers / pool
VkCommandPool           gCommandPool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> gCommandBuffers;

// Synchronization
static const int        MAX_FRAMES_IN_FLIGHT = 2;
VkSemaphore             gImageAvailableSem[MAX_FRAMES_IN_FLIGHT];
VkSemaphore             gRenderFinishedSem[MAX_FRAMES_IN_FLIGHT];
VkFence                 gInFlightFences[MAX_FRAMES_IN_FLIGHT];
uint32_t                gCurrentFrame    = 0;
uint32_t                gCurrentImageIdx = 0;

// For clearing the background
VkClearColorValue       gClearColorValue { 0.5f, 0.5f, 0.5f, 1.0f };

// For tracking window size
int                     gWindowWidth  = WIN_WIDTH;
int                     gWindowHeight = WIN_HEIGHT;
bool                    gInitialized  = false;

// ---------------------------------------------------------
//  Structures for geometry + UBO
// ---------------------------------------------------------
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

// A square made of 2 triangles (6 vertices total)
static std::vector<Vertex> gSquareVertices = {
    // 1st triangle
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // bottom-left (RED)
    {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // bottom-right (GREEN)
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // top-right (BLUE)

    // 2nd triangle
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // top-right (BLUE)
    {{-0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}}, // top-left (YELLOW)
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // bottom-left (RED)
};

struct UniformBufferObject {
    glm::mat4 mvp;
};

// Buffers
VkBuffer       gVertexBuffer        = VK_NULL_HANDLE;
VkDeviceMemory gVertexBufferMemory  = VK_NULL_HANDLE;

VkBuffer       gUniformBuffer       = VK_NULL_HANDLE;
VkDeviceMemory gUniformBufferMemory = VK_NULL_HANDLE;

// Descriptor set / pipeline
VkDescriptorSetLayout gDescSetLayout = VK_NULL_HANDLE;
VkDescriptorPool      gDescPool      = VK_NULL_HANDLE;
VkDescriptorSet       gDescSet       = VK_NULL_HANDLE;

VkPipelineLayout      gPipelineLayout = VK_NULL_HANDLE;
VkPipeline            gGraphicsPipeline = VK_NULL_HANDLE;

// ---------------------------------------------------------
// Forward Declarations
// ---------------------------------------------------------
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

VkResult InitializeVulkan();
VkResult CreateVulkanInstance();
VkResult CreateWin32Surface();
VkResult SelectPhysicalDevice();
VkResult CreateLogicalDevice();
VkResult CreateSwapchain();
VkResult CreateSwapchainImageViews();
VkResult CreateCommandPool();
VkResult CreateCommandBuffers();
VkResult CreateSyncObjects();
VkResult CreateVertexBuffer();
VkResult CreateUniformBuffer();
VkResult CreateDescriptorSetLayout();
VkResult CreateDescriptorPool();
VkResult CreateDescriptorSet();
VkResult CreateGraphicsPipeline();
VkResult BuildCommandBuffers();

VkResult RecreateSwapchain();
void     CleanupSwapchain();

void UpdateUniformBuffer();
void ToggleFullscreen();
void Uninitialize();
VkResult RenderFrame();

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData)
{
    fprintf(gfpLog, "VALIDATION LAYER: %s\n", pCallbackData->pMessage);
    fflush(gfpLog);
    return VK_FALSE; // do not stop validation
}

// Helper to create/destroy debug messenger
static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance                                  instance,
    const VkDebugUtilsMessengerCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugUtilsMessengerEXT*                   pMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance               instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, messenger, pAllocator);
    }
}

// ---------------------------------------------------------
//  WinMain
// ---------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine, int iCmdShow)
{
    gfpLog = fopen("Log.txt", "w");
    if (!gfpLog) {
        MessageBox(NULL, TEXT("Cannot open log file."), TEXT("Error"), MB_OK | MB_ICONERROR);
        return 0;
    }
    fprintf(gfpLog, "Program started.\n");
    fflush(gfpLog);

    // Register window class
    WNDCLASSEX wcex{};
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.lpfnWndProc   = WndProc;
    wcex.hInstance     = hInstance;
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = gAppName;
    RegisterClassEx(&wcex);

    // Calculate window position
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - WIN_WIDTH ) / 2;
    int y = (screenH - WIN_HEIGHT) / 2;

    ghwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        gAppName,
        TEXT("Vulkan + Dynamic Rendering (Square)"),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, WIN_WIDTH, WIN_HEIGHT,
        NULL, NULL,
        hInstance, NULL
    );

    if(!ghwnd) {
        fprintf(gfpLog, "Failed to create Window.\n");
        fflush(gfpLog);
        return 0;
    }

    // Initialize Vulkan
    VkResult result = InitializeVulkan();
    if (result != VK_SUCCESS) {
        fprintf(gfpLog, "InitializeVulkan() failed. VkResult = %d\n", result);
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    ShowWindow(ghwnd, iCmdShow);
    UpdateWindow(ghwnd);
    SetForegroundWindow(ghwnd);
    SetFocus(ghwnd);

    // Message loop
    MSG msg{};
    BOOL bDone = FALSE;
    while (!bDone) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                bDone = TRUE;
            else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            if (gbActive) {
                RenderFrame();
                UpdateUniformBuffer();
            }
        }
    }

    Uninitialize();
    return (int)msg.wParam;
}

// ---------------------------------------------------------
//  Window Procedure
// ---------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    switch (iMsg) {
    case WM_CREATE:
        memset((void*)&gwpPrev, 0, sizeof(WINDOWPLACEMENT));
        gwpPrev.length = sizeof(WINDOWPLACEMENT);
        break;

    case WM_SETFOCUS:
        gbActive = TRUE;
        break;

    case WM_KILLFOCUS:
        gbActive = FALSE;
        break;

    case WM_SIZE:
        if (gInitialized && (wParam != SIZE_MINIMIZED)) {
            gWindowWidth  = LOWORD(lParam);
            gWindowHeight = HIWORD(lParam);
            if(gWindowWidth > 0 && gWindowHeight > 0) {
                RecreateSwapchain();
            }
        }
        break;

    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_CHAR:
        switch (LOWORD(wParam)) {
        case 'f':
        case 'F':
            if (!gbFullscreen) {
                ToggleFullscreen();
                gbFullscreen = TRUE;
            } else {
                ToggleFullscreen();
                gbFullscreen = FALSE;
            }
            break;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

// ---------------------------------------------------------
//  ToggleFullscreen
// ---------------------------------------------------------
void ToggleFullscreen()
{
    MONITORINFO mi { sizeof(MONITORINFO) };
    if (!gbFullscreen) {
        gdwStyle = GetWindowLong(ghwnd, GWL_STYLE);
        if (gdwStyle & WS_OVERLAPPEDWINDOW) {
            if (GetWindowPlacement(ghwnd, &gwpPrev) &&
                GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi)) {
                SetWindowLong(ghwnd, GWL_STYLE, gdwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(
                    ghwnd, HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOZORDER | SWP_FRAMECHANGED
                );
            }
        }
        ShowCursor(FALSE);
    } else {
        SetWindowLong(ghwnd, GWL_STYLE, gdwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(ghwnd, &gwpPrev);
        SetWindowPos(
            ghwnd, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED
        );
        ShowCursor(TRUE);
    }
}

// ---------------------------------------------------------
//  Vulkan Initialization
// ---------------------------------------------------------
VkResult InitializeVulkan()
{
    VkResult res = CreateVulkanInstance();
    if (res != VK_SUCCESS) return res;

    res = CreateWin32Surface();
    if (res != VK_SUCCESS) return res;

    res = SelectPhysicalDevice();
    if (res != VK_SUCCESS) return res;

    res = CreateLogicalDevice();
    if (res != VK_SUCCESS) return res;

    res = CreateSwapchain();
    if (res != VK_SUCCESS) return res;

    res = CreateSwapchainImageViews();
    if (res != VK_SUCCESS) return res;

    res = CreateCommandPool();
    if (res != VK_SUCCESS) return res;

    res = CreateCommandBuffers();
    if (res != VK_SUCCESS) return res;

    // Descriptors / Buffers
    res = CreateDescriptorSetLayout(); if (res != VK_SUCCESS) return res;
    res = CreateUniformBuffer();       if (res != VK_SUCCESS) return res;
    res = CreateDescriptorPool();      if (res != VK_SUCCESS) return res;
    res = CreateDescriptorSet();       if (res != VK_SUCCESS) return res;
    res = CreateVertexBuffer();        if (res != VK_SUCCESS) return res;

    // Pipeline
    res = CreateGraphicsPipeline();
    if (res != VK_SUCCESS) return res;

    // Sync
    res = CreateSyncObjects();
    if (res != VK_SUCCESS) return res;

    // Build command buffers
    res = BuildCommandBuffers();
    if (res != VK_SUCCESS) return res;

    gInitialized = true;
    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Create Vulkan Instance
// ---------------------------------------------------------
VkResult CreateVulkanInstance()
{
    // 1) Pick instance extensions
    uint32_t instExtCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, nullptr);
    std::vector<VkExtensionProperties> extProps(instExtCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, extProps.data());

    bool foundSurface      = false;
    bool foundWin32Surface = false;
    bool foundDebugUtils   = false;

    for (auto &ep : extProps) {
        if (!strcmp(ep.extensionName, VK_KHR_SURFACE_EXTENSION_NAME)) {
            foundSurface = true;
        } else if (!strcmp(ep.extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
            foundWin32Surface = true;
        } else if (!strcmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            foundDebugUtils = true;
        }
    }

    std::vector<const char*> instanceExtensions;
    if (foundSurface)      instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    if (foundWin32Surface) instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    if (gEnableValidation && foundDebugUtils) {
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // 2) Pick instance layers
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layerProps(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data());

    bool haveValidationLayer = false;
    for (auto &lp : layerProps) {
        if(!strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation")) {
            haveValidationLayer = true;
            break;
        }
    }

    std::vector<const char*> instanceLayers;
    if (gEnableValidation && haveValidationLayer) {
        instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    // 3) Create Instance
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = gAppName;
    appInfo.applicationVersion = 1;
    appInfo.pEngineName        = "NoEngine";
    appInfo.engineVersion      = 1;
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = (uint32_t)instanceExtensions.size();
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();
    createInfo.enabledLayerCount       = (uint32_t)instanceLayers.size();
    createInfo.ppEnabledLayerNames     = instanceLayers.data();

    VkResult res = vkCreateInstance(&createInfo, nullptr, &gInstance);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "vkCreateInstance() failed.\n");
        return res;
    }

    // 4) Create Debug Messenger if requested
    if (gEnableValidation && haveValidationLayer && foundDebugUtils) {
        VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo{};
        dbgCreateInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgCreateInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgCreateInfo.pfnUserCallback = DebugCallback;

        if (CreateDebugUtilsMessengerEXT(gInstance, &dbgCreateInfo, nullptr, &gDebugUtilsMessenger) != VK_SUCCESS) {
            fprintf(gfpLog, "Could not create debug messenger.\n");
            gDebugUtilsMessenger = VK_NULL_HANDLE;
        }
    }
    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Create Win32 Surface
// ---------------------------------------------------------
VkResult CreateWin32Surface()
{
    VkWin32SurfaceCreateInfoKHR sci{};
    sci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    sci.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    sci.hwnd      = ghwnd;
    VkResult res = vkCreateWin32SurfaceKHR(gInstance, &sci, nullptr, &gSurface);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "vkCreateWin32SurfaceKHR() failed.\n");
    }
    return res;
}

// ---------------------------------------------------------
//  Select Physical Device
// ---------------------------------------------------------
VkResult SelectPhysicalDevice()
{
    uint32_t physCount = 0;
    vkEnumeratePhysicalDevices(gInstance, &physCount, nullptr);
    if (physCount == 0) {
        fprintf(gfpLog, "No physical devices found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::vector<VkPhysicalDevice> phys(physCount);
    vkEnumeratePhysicalDevices(gInstance, &physCount, phys.data());

    bool found = false;
    for (auto &pd : phys) {
        // Check for a queue family that supports graphics & present
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qfProps.data());

        std::vector<VkBool32> canPresent(qCount);
        for (uint32_t i = 0; i < qCount; i++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, gSurface, &canPresent[i]);
        }

        for (uint32_t i = 0; i < qCount; i++) {
            if ((qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent[i]) {
                gPhysicalDevice     = pd;
                gGraphicsQueueIndex = i;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) {
        fprintf(gfpLog, "No suitable GPU with graphics+present found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    vkGetPhysicalDeviceMemoryProperties(gPhysicalDevice, &gMemProperties);
    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Create Logical Device + Queue
// ---------------------------------------------------------
VkResult CreateLogicalDevice()
{
    // We want VK_KHR_swapchain and possibly VK_KHR_dynamic_rendering
    uint32_t devExtCount = 0;
    vkEnumerateDeviceExtensionProperties(gPhysicalDevice, nullptr, &devExtCount, nullptr);
    std::vector<VkExtensionProperties> devExtProps(devExtCount);
    vkEnumerateDeviceExtensionProperties(gPhysicalDevice, nullptr, &devExtCount, devExtProps.data());

    bool haveSwapchain    = false;
    bool haveDynRendering = false;
    for (auto &dep : devExtProps) {
        if (!strcmp(dep.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            haveSwapchain = true;
        }
        if (!strcmp(dep.extensionName, "VK_KHR_dynamic_rendering")) {
            haveDynRendering = true;
        }
    }
    if (!haveSwapchain) {
        fprintf(gfpLog, "Device extension VK_KHR_swapchain not found.\n");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    std::vector<const char*> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (haveDynRendering) {
        deviceExtensions.push_back("VK_KHR_dynamic_rendering");
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = gGraphicsQueueIndex;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &queuePriority;

    // If we rely on the extension with Vulkan < 1.3
    VkPhysicalDeviceDynamicRenderingFeatures dynFeatures{};
    dynFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynFeatures.dynamicRendering = haveDynRendering ? VK_TRUE : VK_FALSE;

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext                   = &dynFeatures;
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = (uint32_t)deviceExtensions.size();
    dci.ppEnabledExtensionNames = deviceExtensions.data();

    VkResult res = vkCreateDevice(gPhysicalDevice, &dci, nullptr, &gDevice);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "vkCreateDevice() failed.\n");
        return res;
    }

    vkGetDeviceQueue(gDevice, gGraphicsQueueIndex, 0, &gGraphicsQueue);
    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Create Swapchain
// ---------------------------------------------------------
VkResult CreateSwapchain()
{
    // 1) We need a surface format
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gPhysicalDevice, gSurface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> sfmts(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gPhysicalDevice, gSurface, &formatCount, sfmts.data());

    if (formatCount == 1 && sfmts[0].format == VK_FORMAT_UNDEFINED) {
        gSwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
        gColorSpace      = sfmts[0].colorSpace;
    } else {
        // just pick the first
        gSwapchainFormat = sfmts[0].format;
        gColorSpace      = sfmts[0].colorSpace;
    }

    // 2) We need present mode
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gPhysicalDevice, gSurface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> pmodes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gPhysicalDevice, gSurface, &presentModeCount, pmodes.data());
    gPresentMode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed
    for (auto &pm : pmodes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            gPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }

    // 3) We need extent
    VkSurfaceCapabilitiesKHR surfCaps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gPhysicalDevice, gSurface, &surfCaps);

    if(surfCaps.currentExtent.width != UINT32_MAX) {
        gSwapchainExtent = surfCaps.currentExtent;
    } else {
        // clamp manually
        gSwapchainExtent.width  = std::max(surfCaps.minImageExtent.width,
                                    std::min(surfCaps.maxImageExtent.width,  (uint32_t)gWindowWidth));
        gSwapchainExtent.height = std::max(surfCaps.minImageExtent.height,
                                    std::min(surfCaps.maxImageExtent.height, (uint32_t)gWindowHeight));
    }

    uint32_t desiredImgCount = surfCaps.minImageCount + 1;
    if (surfCaps.maxImageCount > 0 && desiredImgCount > surfCaps.maxImageCount) {
        desiredImgCount = surfCaps.maxImageCount;
    }

    // 4) Create the swapchain
    VkSwapchainCreateInfoKHR sci{};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = gSurface;
    sci.minImageCount    = desiredImgCount;
    sci.imageFormat      = gSwapchainFormat;
    sci.imageColorSpace  = gColorSpace;
    sci.imageExtent      = gSwapchainExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform     = surfCaps.currentTransform;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = gPresentMode;
    sci.clipped          = VK_TRUE;

    VkResult res = vkCreateSwapchainKHR(gDevice, &sci, nullptr, &gSwapchain);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "vkCreateSwapchainKHR() failed.\n");
        return res;
    }
    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Create Swapchain Image Views
// ---------------------------------------------------------
VkResult CreateSwapchainImageViews()
{
    vkGetSwapchainImagesKHR(gDevice, gSwapchain, &gSwapchainImageCount, nullptr);
    gSwapchainImages.resize(gSwapchainImageCount);
    vkGetSwapchainImagesKHR(gDevice, gSwapchain, &gSwapchainImageCount, gSwapchainImages.data());

    gSwapchainImageViews.resize(gSwapchainImageCount);

    for (uint32_t i = 0; i < gSwapchainImageCount; i++) {
        VkImageViewCreateInfo ivci{};
        ivci.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image            = gSwapchainImages[i];
        ivci.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format           = gSwapchainFormat;
        ivci.components.r     = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.g     = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.b     = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.a     = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel   = 0;
        ivci.subresourceRange.levelCount     = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount     = 1;

        VkResult res = vkCreateImageView(gDevice, &ivci, nullptr, &gSwapchainImageViews[i]);
        if (res != VK_SUCCESS) {
            fprintf(gfpLog, "Failed to create image view idx %d\n", i);
            return res;
        }
    }
    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Create Command Pool
// ---------------------------------------------------------
VkResult CreateCommandPool()
{
    VkCommandPoolCreateInfo cpci{};
    cpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = gGraphicsQueueIndex;
    cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult res = vkCreateCommandPool(gDevice, &cpci, nullptr, &gCommandPool);
    if(res != VK_SUCCESS) {
        fprintf(gfpLog, "vkCreateCommandPool() failed.\n");
    }
    return res;
}

// ---------------------------------------------------------
//  Create Command Buffers
// ---------------------------------------------------------
VkResult CreateCommandBuffers()
{
    gCommandBuffers.resize(gSwapchainImageCount);

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool        = gCommandPool;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = (uint32_t)gCommandBuffers.size();

    VkResult res = vkAllocateCommandBuffers(gDevice, &cbai, gCommandBuffers.data());
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "vkAllocateCommandBuffers() failed.\n");
    }
    return res;
}

// ---------------------------------------------------------
//  Create Sync Objects
// ---------------------------------------------------------
VkResult CreateSyncObjects()
{
    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(gDevice, &sci, nullptr, &gImageAvailableSem[i]) != VK_SUCCESS ||
            vkCreateSemaphore(gDevice, &sci, nullptr, &gRenderFinishedSem[i]) != VK_SUCCESS ||
            vkCreateFence(gDevice, &fci, nullptr, &gInFlightFences[i]) != VK_SUCCESS)
        {
            fprintf(gfpLog, "Failed to create sync objects for frame %d.\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Create Vertex Buffer
// ---------------------------------------------------------
uint32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags flags)
{
    for (uint32_t i = 0; i < gMemProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (gMemProperties.memoryTypes[i].propertyFlags & flags) == flags)
        {
            return i;
        }
    }
    fprintf(gfpLog, "FindMemoryTypeIndex() failed. No suitable memory.\n");
    return UINT32_MAX;
}

VkResult CreateVertexBuffer()
{
    VkDeviceSize sizeBytes = sizeof(Vertex) * gSquareVertices.size();

    VkBufferCreateInfo bci{};
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = sizeBytes;
    bci.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(gDevice, &bci, nullptr, &gVertexBuffer);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "Failed to create vertex buffer.\n");
        return res;
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(gDevice, gVertexBuffer, &memReq);

    uint32_t memTypeIdx = FindMemoryTypeIndex(
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (memTypeIdx == UINT32_MAX) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = memTypeIdx;

    res = vkAllocateMemory(gDevice, &mai, nullptr, &gVertexBufferMemory);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "Failed to allocate memory for vertex buffer.\n");
        return res;
    }

    vkBindBufferMemory(gDevice, gVertexBuffer, gVertexBufferMemory, 0);

    // Copy data
    void* data;
    vkMapMemory(gDevice, gVertexBufferMemory, 0, sizeBytes, 0, &data);
    memcpy(data, gSquareVertices.data(), (size_t)sizeBytes);
    vkUnmapMemory(gDevice, gVertexBufferMemory);

    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Create Uniform Buffer
// ---------------------------------------------------------
VkResult CreateUniformBuffer()
{
    VkDeviceSize sizeBytes = sizeof(UniformBufferObject);

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = sizeBytes;
    bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VkResult res = vkCreateBuffer(gDevice, &bci, nullptr, &gUniformBuffer);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "Failed to create uniform buffer.\n");
        return res;
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(gDevice, gUniformBuffer, &memReq);

    uint32_t memTypeIdx = FindMemoryTypeIndex(
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (memTypeIdx == UINT32_MAX) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = memTypeIdx;

    res = vkAllocateMemory(gDevice, &mai, nullptr, &gUniformBufferMemory);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "Failed to allocate memory for uniform buffer.\n");
        return res;
    }

    vkBindBufferMemory(gDevice, gUniformBuffer, gUniformBufferMemory, 0);
    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Create Descriptor Set Layout
// ---------------------------------------------------------
VkResult CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding         = 0;
    uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboBinding;

    VkResult res = vkCreateDescriptorSetLayout(gDevice, &layoutInfo, nullptr, &gDescSetLayout);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "Failed to create descriptor set layout.\n");
    }
    return res;
}

// ---------------------------------------------------------
//  Create Descriptor Pool
// ---------------------------------------------------------
VkResult CreateDescriptorPool()
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    VkResult res = vkCreateDescriptorPool(gDevice, &poolInfo, nullptr, &gDescPool);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "Failed to create descriptor pool.\n");
    }
    return res;
}

// ---------------------------------------------------------
//  Create Descriptor Set
// ---------------------------------------------------------
VkResult CreateDescriptorSet()
{
    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool     = gDescPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts        = &gDescSetLayout;

    VkResult res = vkAllocateDescriptorSets(gDevice, &dsai, &gDescSet);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "Failed to allocate descriptor set.\n");
        return res;
    }

    VkDescriptorBufferInfo dbi{};
    dbi.buffer = gUniformBuffer;
    dbi.offset = 0;
    dbi.range  = sizeof(UniformBufferObject);

    VkWriteDescriptorSet wds{};
    wds.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wds.dstSet          = gDescSet;
    wds.dstBinding      = 0;
    wds.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    wds.descriptorCount = 1;
    wds.pBufferInfo     = &dbi;

    vkUpdateDescriptorSets(gDevice, 1, &wds, 0, nullptr);
    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Create Graphics Pipeline (Dynamic Rendering style)
// ---------------------------------------------------------
std::vector<char> ReadFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        fprintf(gfpLog, "Failed to open file: %s\n", filename.c_str());
        return {};
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule CreateShaderModule(const std::vector<char>& code)
{
    if (code.empty()) return VK_NULL_HANDLE;

    VkShaderModuleCreateInfo smci{};
    smci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = code.size();
    smci.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(gDevice, &smci, nullptr, &shaderModule) != VK_SUCCESS) {
        fprintf(gfpLog, "Failed to create shader module.\n");
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

VkResult CreateGraphicsPipeline()
{
    // Read SPIR-V
    auto vertCode = ReadFile("vert_shader.spv");
    auto fragCode = ReadFile("frag_shader.spv");
    VkShaderModule vertModule = CreateShaderModule(vertCode);
    VkShaderModule fragModule = CreateShaderModule(fragCode);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        fprintf(gfpLog, "Could not create shader modules.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPipelineShaderStageCreateInfo vs{};
    vs.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vs.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vs.module = vertModule;
    vs.pName  = "main";

    VkPipelineShaderStageCreateInfo fs{};
    fs.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fs.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs.module = fragModule;
    fs.pName  = "main";

    VkPipelineShaderStageCreateInfo stages[] = { vs, fs };

    // Vertex input
    VkVertexInputBindingDescription bindDesc{};
    bindDesc.binding   = 0;
    bindDesc.stride    = sizeof(Vertex);
    bindDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attribs{};
    attribs[0].binding  = 0;
    attribs[0].location = 0;
    attribs[0].format   = VK_FORMAT_R32G32_SFLOAT;
    attribs[0].offset   = offsetof(Vertex, pos);

    attribs[1].binding  = 0;
    attribs[1].location = 1;
    attribs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attribs[1].offset   = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vici{};
    vici.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vici.vertexBindingDescriptionCount   = 1;
    vici.pVertexBindingDescriptions      = &bindDesc;
    vici.vertexAttributeDescriptionCount = (uint32_t)attribs.size();
    vici.pVertexAttributeDescriptions    = attribs.data();

    VkPipelineInputAssemblyStateCreateInfo iaci{};
    iaci.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Viewport/scissor (we can set it up for the swapchain extent)
    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)gSwapchainExtent.width;
    vp.height   = (float)gSwapchainExtent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0,0};
    scissor.extent = gSwapchainExtent;

    VkPipelineViewportStateCreateInfo vpci{};
    vpci.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpci.viewportCount = 1;
    vpci.pViewports    = &vp;
    vpci.scissorCount  = 1;
    vpci.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rsci{};
    rsci.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsci.polygonMode             = VK_POLYGON_MODE_FILL;
    rsci.cullMode                = VK_CULL_MODE_NONE;
    rsci.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsci.lineWidth               = 1.0f;

    VkPipelineMultisampleStateCreateInfo msci{};
    msci.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cbAttach{};
    cbAttach.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbAttach.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cbci{};
    cbci.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbci.attachmentCount = 1;
    cbci.pAttachments    = &cbAttach;

    // Pipeline layout
    VkPipelineLayoutCreateInfo plci{};
    plci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts    = &gDescSetLayout;

    VkResult res = vkCreatePipelineLayout(gDevice, &plci, nullptr, &gPipelineLayout);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "Failed to create pipeline layout.\n");
        return res;
    }

    // Dynamic rendering info
    VkPipelineRenderingCreateInfo prci{};
    prci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    prci.colorAttachmentCount = 1;
    VkFormat colorFormat = gSwapchainFormat;
    prci.pColorAttachmentFormats = &colorFormat;

    VkGraphicsPipelineCreateInfo gpci{};
    gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.pNext               = &prci;
    gpci.stageCount          = 2;
    gpci.pStages             = stages;
    gpci.pVertexInputState   = &vici;
    gpci.pInputAssemblyState = &iaci;
    gpci.pViewportState      = &vpci;
    gpci.pRasterizationState = &rsci;
    gpci.pMultisampleState   = &msci;
    gpci.pColorBlendState    = &cbci;
    gpci.layout              = gPipelineLayout;

    res = vkCreateGraphicsPipelines(gDevice, VK_NULL_HANDLE, 1, &gpci, nullptr, &gGraphicsPipeline);
    if (res != VK_SUCCESS) {
        fprintf(gfpLog, "Failed to create graphics pipeline.\n");
    }

    // Cleanup temp modules
    vkDestroyShaderModule(gDevice, fragModule, nullptr);
    vkDestroyShaderModule(gDevice, vertModule, nullptr);

    return res;
}

// ---------------------------------------------------------
//  Build Command Buffers (with layout transitions)
// ---------------------------------------------------------
VkResult BuildCommandBuffers()
{
    for (uint32_t i = 0; i < gSwapchainImageCount; i++) {
        vkResetCommandBuffer(gCommandBuffers[i], 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(gCommandBuffers[i], &beginInfo);

        //------------------------------------------------------
        // Barrier: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
        //------------------------------------------------------
        {
            VkImageMemoryBarrier barrier{};
            barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.image                           = gSwapchainImages[i];
            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = 1;

            // No writes before
            barrier.srcAccessMask = 0;
            // We want to write color
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(
                gCommandBuffers[i],
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        }

        //------------------------------------------------------
        // Dynamic Rendering begin
        //------------------------------------------------------
        VkClearValue clearValue{};
        clearValue.color = gClearColorValue;

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView   = gSwapchainImageViews[i];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue  = clearValue;

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset    = {0,0};
        renderingInfo.renderArea.extent    = gSwapchainExtent;
        renderingInfo.layerCount           = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments    = &colorAttachment;

        vkCmdBeginRendering(gCommandBuffers[i], &renderingInfo);

        // Bind pipeline
        vkCmdBindPipeline(gCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gGraphicsPipeline);

        // Bind descriptor set (for UBO)
        vkCmdBindDescriptorSets(gCommandBuffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            gPipelineLayout,
            0, 1, &gDescSet,
            0, nullptr);

        // Bind vertex buffer
        VkBuffer vertexBuffers[] = { gVertexBuffer };
        VkDeviceSize offsets[]   = { 0 };
        vkCmdBindVertexBuffers(gCommandBuffers[i], 0, 1, vertexBuffers, offsets);

        // Draw the 6 vertices of the square
        vkCmdDraw(gCommandBuffers[i], 6, 1, 0, 0);

        // End dynamic rendering
        vkCmdEndRendering(gCommandBuffers[i]);

        //------------------------------------------------------
        // Barrier: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
        //------------------------------------------------------
        {
            VkImageMemoryBarrier barrier{};
            barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.image                           = gSwapchainImages[i];
            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = 1;

            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = 0; // no more reads/writes needed

            vkCmdPipelineBarrier(
                gCommandBuffers[i],
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        }

        vkEndCommandBuffer(gCommandBuffers[i]);
    }

    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Recreate Swapchain
// ---------------------------------------------------------
VkResult RecreateSwapchain()
{
    vkDeviceWaitIdle(gDevice);

    CleanupSwapchain(); // destroy old

    VkResult res = CreateSwapchain();                  if (res != VK_SUCCESS) return res;
    res = CreateSwapchainImageViews();                 if (res != VK_SUCCESS) return res;
    res = CreateCommandPool();                         if (res != VK_SUCCESS) return res;
    res = CreateCommandBuffers();                      if (res != VK_SUCCESS) return res;

    // Rebuild pipeline (since it depends on swapchain size)
    if (gGraphicsPipeline) {
        vkDestroyPipeline(gDevice, gGraphicsPipeline, nullptr);
        gGraphicsPipeline = VK_NULL_HANDLE;
    }
    if (gPipelineLayout) {
        vkDestroyPipelineLayout(gDevice, gPipelineLayout, nullptr);
        gPipelineLayout = VK_NULL_HANDLE;
    }
    res = CreateGraphicsPipeline();                    if (res != VK_SUCCESS) return res;

    res = BuildCommandBuffers();                       if (res != VK_SUCCESS) return res;

    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  Cleanup Swapchain
// ---------------------------------------------------------
void CleanupSwapchain()
{
    if (!gDevice) return;

    // Command buffers
    if (!gCommandBuffers.empty()) {
        vkFreeCommandBuffers(gDevice, gCommandPool, (uint32_t)gCommandBuffers.size(), gCommandBuffers.data());
        gCommandBuffers.clear();
    }

    if (gCommandPool) {
        vkDestroyCommandPool(gDevice, gCommandPool, nullptr);
        gCommandPool = VK_NULL_HANDLE;
    }

    for (auto &iv : gSwapchainImageViews) {
        vkDestroyImageView(gDevice, iv, nullptr);
    }
    gSwapchainImageViews.clear();

    if (gSwapchain) {
        vkDestroySwapchainKHR(gDevice, gSwapchain, nullptr);
        gSwapchain = VK_NULL_HANDLE;
    }
}

// ---------------------------------------------------------
//  RenderFrame
// ---------------------------------------------------------
VkResult RenderFrame()
{
    vkWaitForFences(gDevice, 1, &gInFlightFences[gCurrentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(gDevice, 1, &gInFlightFences[gCurrentFrame]);

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(
        gDevice, gSwapchain,
        UINT64_MAX,
        gImageAvailableSem[gCurrentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        RecreateSwapchain();
        return VK_SUCCESS;
    } else if (result != VK_SUCCESS) {
        fprintf(gfpLog, "vkAcquireNextImageKHR failed: %d\n", result);
        return result;
    }
    gCurrentImageIdx = imageIndex;

    // Submit
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &gImageAvailableSem[gCurrentFrame];
    si.pWaitDstStageMask    = waitStages;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &gCommandBuffers[gCurrentImageIdx];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &gRenderFinishedSem[gCurrentFrame];

    vkQueueSubmit(gGraphicsQueue, 1, &si, gInFlightFences[gCurrentFrame]);

    // Present
    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &gRenderFinishedSem[gCurrentFrame];
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &gSwapchain;
    pi.pImageIndices      = &gCurrentImageIdx;

    result = vkQueuePresentKHR(gGraphicsQueue, &pi);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        RecreateSwapchain();
    } else if (result != VK_SUCCESS) {
        fprintf(gfpLog, "vkQueuePresentKHR failed: %d\n", result);
    }

    gCurrentFrame = (gCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

// ---------------------------------------------------------
//  UpdateUniformBuffer
// ---------------------------------------------------------
void UpdateUniformBuffer()
{
    // Basic MVP
    UniformBufferObject ubo{};

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view  = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -2.0f));
    float aspect     = (float)gWindowWidth / (float)gWindowHeight;
    glm::mat4 proj  = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    // flip Y for Vulkan
    proj[1][1] *= -1.0f;

    ubo.mvp = proj * view * model;

    // Copy to uniform buffer
    void* data;
    vkMapMemory(gDevice, gUniformBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(gDevice, gUniformBufferMemory);
}

// ---------------------------------------------------------
//  Uninitialize
// ---------------------------------------------------------
void Uninitialize()
{
    if (gbFullscreen) {
        ToggleFullscreen();
        gbFullscreen = FALSE;
    }

    if (gDevice) {
        vkDeviceWaitIdle(gDevice);

        // Destroy sync
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (gInFlightFences[i]) {
                vkDestroyFence(gDevice, gInFlightFences[i], nullptr);
                gInFlightFences[i] = VK_NULL_HANDLE;
            }
            if (gRenderFinishedSem[i]) {
                vkDestroySemaphore(gDevice, gRenderFinishedSem[i], nullptr);
                gRenderFinishedSem[i] = VK_NULL_HANDLE;
            }
            if (gImageAvailableSem[i]) {
                vkDestroySemaphore(gDevice, gImageAvailableSem[i], nullptr);
                gImageAvailableSem[i] = VK_NULL_HANDLE;
            }
        }

        CleanupSwapchain();

        if (gGraphicsPipeline) {
            vkDestroyPipeline(gDevice, gGraphicsPipeline, nullptr);
            gGraphicsPipeline = VK_NULL_HANDLE;
        }
        if (gPipelineLayout) {
            vkDestroyPipelineLayout(gDevice, gPipelineLayout, nullptr);
            gPipelineLayout = VK_NULL_HANDLE;
        }
        if (gDescPool) {
            vkDestroyDescriptorPool(gDevice, gDescPool, nullptr);
            gDescPool = VK_NULL_HANDLE;
        }
        if (gDescSetLayout) {
            vkDestroyDescriptorSetLayout(gDevice, gDescSetLayout, nullptr);
            gDescSetLayout = VK_NULL_HANDLE;
        }
        if (gVertexBuffer) {
            vkDestroyBuffer(gDevice, gVertexBuffer, nullptr);
            gVertexBuffer = VK_NULL_HANDLE;
        }
        if (gVertexBufferMemory) {
            vkFreeMemory(gDevice, gVertexBufferMemory, nullptr);
            gVertexBufferMemory = VK_NULL_HANDLE;
        }
        if (gUniformBuffer) {
            vkDestroyBuffer(gDevice, gUniformBuffer, nullptr);
            gUniformBuffer = VK_NULL_HANDLE;
        }
        if (gUniformBufferMemory) {
            vkFreeMemory(gDevice, gUniformBufferMemory, nullptr);
            gUniformBufferMemory = VK_NULL_HANDLE;
        }

        vkDestroyDevice(gDevice, nullptr);
        gDevice = VK_NULL_HANDLE;
    }

    if (gDebugUtilsMessenger) {
        DestroyDebugUtilsMessengerEXT(gInstance, gDebugUtilsMessenger, nullptr);
        gDebugUtilsMessenger = VK_NULL_HANDLE;
    }

    if (gSurface) {
        vkDestroySurfaceKHR(gInstance, gSurface, nullptr);
        gSurface = VK_NULL_HANDLE;
    }

    if (gInstance) {
        vkDestroyInstance(gInstance, nullptr);
        gInstance = VK_NULL_HANDLE;
    }

    if (ghwnd) {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    if (gfpLog) {
        fprintf(gfpLog, "Uninitialize completed. Exiting.\n");
        fclose(gfpLog);
        gfpLog = nullptr;
    }
}

