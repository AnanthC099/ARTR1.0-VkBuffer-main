#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <fstream>
#include <vector>
#include <array>

#include "vmath.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

// Windowing
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
#define WIN_WIDTH  800
#define WIN_HEIGHT 600

const char* gpszAppName = "Vulkan_3D_Pyramid";
HWND  ghwnd              = NULL;
BOOL  gbActive           = FALSE;
DWORD dwStyle            = 0;
WINDOWPLACEMENT wpPrev{ sizeof(WINDOWPLACEMENT) };
BOOL  gbFullscreen       = FALSE;

FILE* gFILE             = nullptr;
bool  gEnableValidation = true;

static VkDebugUtilsMessengerEXT gDebugUtilsMessenger = VK_NULL_HANDLE;

// Validation callback
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData)
{
    fprintf(gFILE, "VALIDATION: %s\n", pCallbackData->pMessage);
    fflush(gFILE);
    return VK_FALSE;
}

// Debug messenger creation
static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance                                  instance,
    const VkDebugUtilsMessengerCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugUtilsMessengerEXT*                   pMessenger)
{
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
        return func(instance, pCreateInfo, pAllocator, pMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance                    instance,
    VkDebugUtilsMessengerEXT      messenger,
    const VkAllocationCallbacks*  pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
        func(instance, messenger, pAllocator);
}

// Vulkan handles
VkInstance          vkInstance             = VK_NULL_HANDLE;
VkSurfaceKHR        vkSurfaceKHR           = VK_NULL_HANDLE;
VkPhysicalDevice    vkPhysicalDevice_sel   = VK_NULL_HANDLE;
VkDevice            vkDevice               = VK_NULL_HANDLE;
VkQueue             vkQueue                = VK_NULL_HANDLE;
uint32_t            graphicsQueueIndex     = UINT32_MAX;
uint32_t            physicalDeviceCount    = 0;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties{};

VkFormat       vkFormat_color     = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR vkColorSpaceKHR   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
VkSwapchainKHR   vkSwapchainKHR   = VK_NULL_HANDLE;
uint32_t         swapchainImageCount = 0;
VkImage*         swapChainImage_array    = nullptr;
VkImageView*     swapChainImageView_array= nullptr;
VkExtent2D       vkExtent2D_SwapChain{WIN_WIDTH, WIN_HEIGHT};

VkCommandPool    vkCommandPool       = VK_NULL_HANDLE;
VkCommandBuffer* vkCommandBuffer_array = nullptr;

#define MAX_FRAMES_IN_FLIGHT 2
VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
VkFence     inFlightFences[MAX_FRAMES_IN_FLIGHT];
uint32_t    currentFrame      = 0;
uint32_t    currentImageIndex = UINT32_MAX;

VkClearColorValue vkClearColorValue{};

int  winWidth   = WIN_WIDTH;
int  winHeight  = WIN_HEIGHT;
BOOL bInitialized = FALSE;

VkRenderPass   gRenderPass    = VK_NULL_HANDLE;
VkFramebuffer* gFramebuffers  = nullptr;

// Vertex data
struct Vertex {
    vmath::vec3 pos;
    vmath::vec3 color;
};

static std::vector<Vertex> gVertices =
{
    {{ 0.0f,  1.0f,  0.0f }, {1.0f, 0.0f, 0.0f}},
    {{-1.0f,  0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
    {{ 1.0f,  0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},
    {{ 1.0f,  0.0f,  1.0f}, {1.0f, 1.0f, 0.0f}},
    {{-1.0f,  0.0f,  1.0f}, {1.0f, 0.0f, 1.0f}},
};

static std::vector<Vertex> gPyramidVertices()
{
    std::vector<Vertex> out;
    out.push_back(gVertices[0]); out.push_back(gVertices[1]); out.push_back(gVertices[2]);
    out.push_back(gVertices[0]); out.push_back(gVertices[2]); out.push_back(gVertices[3]);
    out.push_back(gVertices[0]); out.push_back(gVertices[3]); out.push_back(gVertices[4]);
    out.push_back(gVertices[0]); out.push_back(gVertices[4]); out.push_back(gVertices[1]);
    return out;
}

static std::vector<Vertex> gFinalVertices = gPyramidVertices();

struct UniformBufferObject {
    vmath::mat4 mvp;
};

VkBuffer       gUniformBuffer       = VK_NULL_HANDLE;
VkDeviceMemory gUniformBufferMemory = VK_NULL_HANDLE;
VkDescriptorSetLayout gDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorPool      gDescriptorPool      = VK_NULL_HANDLE;
VkDescriptorSet       gDescriptorSet       = VK_NULL_HANDLE;
VkBuffer       gVertexBuffer       = VK_NULL_HANDLE;
VkDeviceMemory gVertexBufferMemory = VK_NULL_HANDLE;
VkPipelineLayout gPipelineLayout   = VK_NULL_HANDLE;
VkPipeline       gGraphicsPipeline = VK_NULL_HANDLE;

static VkFormat gDepthFormat = VK_FORMAT_UNDEFINED;
std::vector<VkImage>        gDepthImages;
std::vector<VkDeviceMemory> gDepthImagesMemory;
std::vector<VkImageView>    gDepthImagesView;

// Prototypes
VkResult initialize(void);
void     uninitialize(void);
VkResult display(void);
void     update(void);
VkResult recreateSwapChain(void);
void     ToggleFullscreen(void);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
    gFILE = fopen("Log.txt", "w");
    if (!gFILE) { MessageBox(NULL, TEXT("Cannot open log file."), TEXT("Error"), MB_OK); exit(0); }

    WNDCLASSEX wndclass{};
    wndclass.cbSize        = sizeof(WNDCLASSEX);
    wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.hInstance     = hInstance;
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpszClassName = gpszAppName;
    RegisterClassEx(&wndclass);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int xCoord  = (screenW - WIN_WIDTH ) / 2;
    int yCoord  = (screenH - WIN_HEIGHT) / 2;

    ghwnd = CreateWindowEx(WS_EX_APPWINDOW,
        gpszAppName,
        TEXT("Vulkan 3D Pyramid"),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
        xCoord, yCoord, WIN_WIDTH, WIN_HEIGHT,
        NULL, NULL, hInstance, NULL);

    if (!ghwnd) return 0;

    if (initialize() != VK_SUCCESS) {
        fprintf(gFILE, "Initialization failed.\n");
        fflush(gFILE);
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    ShowWindow(ghwnd, iCmdShow);
    UpdateWindow(ghwnd);
    SetForegroundWindow(ghwnd);
    SetFocus(ghwnd);

    MSG msg{};
    while (true) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            if (gbActive) {
                display();
                update();
            }
        }
    }
    uninitialize();
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    switch (iMsg) {
    case WM_CREATE:
        memset(&wpPrev, 0, sizeof(WINDOWPLACEMENT));
        wpPrev.length = sizeof(WINDOWPLACEMENT);
        break;
    case WM_SETFOCUS:
        gbActive = TRUE;
        break;
    case WM_KILLFOCUS:
        gbActive = FALSE;
        break;
    case WM_SIZE:
        if (bInitialized) {
            winWidth  = LOWORD(lParam);
            winHeight = HIWORD(lParam);
            if (winWidth > 0 && winHeight > 0) {
                recreateSwapChain();
            }
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        break;
    case WM_CHAR:
        if (wParam == 'f' || wParam == 'F') {
            if (!gbFullscreen) { ToggleFullscreen(); gbFullscreen = TRUE; }
            else { ToggleFullscreen(); gbFullscreen = FALSE; }
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

void ToggleFullscreen(void)
{
    static MONITORINFO mi{ sizeof(MONITORINFO) };
    if (!gbFullscreen) {
        dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
        if (dwStyle & WS_OVERLAPPEDWINDOW) {
            if (GetWindowPlacement(ghwnd, &wpPrev) &&
                GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi)) {
                SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(ghwnd, HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOZORDER | SWP_FRAMECHANGED);
            }
        }
        ShowCursor(FALSE);
    } else {
        SetWindowPlacement(ghwnd, &wpPrev);
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(ghwnd, HWND_TOP,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    }
}

// Helpers for instance creation
static uint32_t enabledInstanceLayerCount = 0;
static const char* enabledInstanceLayerNames_array[1];

static VkResult FillInstanceLayerNames()
{
    if (!gEnableValidation) return VK_SUCCESS;
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    if (layerCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    for (auto& lp : layers) {
        if (strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            enabledInstanceLayerNames_array[0] = "VK_LAYER_KHRONOS_validation";
            enabledInstanceLayerCount          = 1;
            break;
        }
    }
    return VK_SUCCESS;
}

static uint32_t enabledInstanceExtensionsCount = 0;
static const char* enabledInstanceExtensionNames_array[3];

static void AddDebugUtilsExtensionIfPresent()
{
    if (!gEnableValidation) return;
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    if (extCount == 0) return;
    std::vector<VkExtensionProperties> props(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, props.data());
    for (auto& ep : props) {
        if (strcmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
            enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            return;
        }
    }
}

static VkResult FillInstanceExtensionNames()
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (extensionCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    bool foundSurface = false, foundWin32Surface = false;
    std::vector<VkExtensionProperties> exts(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, exts.data());
    for (auto& e : exts) {
        if (!strcmp(e.extensionName, VK_KHR_SURFACE_EXTENSION_NAME)) {
            foundSurface = true;
        }
        if (!strcmp(e.extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
            foundWin32Surface = true;
        }
    }
    if (!foundSurface || !foundWin32Surface) return VK_ERROR_INITIALIZATION_FAILED;

    enabledInstanceExtensionsCount = 0;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
    AddDebugUtilsExtensionIfPresent();
    return VK_SUCCESS;
}

static VkResult CreateVulkanInstance()
{
    if (FillInstanceExtensionNames() != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (FillInstanceLayerNames()      != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = gpszAppName;
    appInfo.applicationVersion = 1;
    appInfo.pEngineName        = gpszAppName;
    appInfo.engineVersion      = 1;
    // Keep Vulkan version as requested
    appInfo.apiVersion         = VK_API_VERSION_1_4;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = enabledInstanceExtensionsCount;
    createInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;
    createInfo.enabledLayerCount       = enabledInstanceLayerCount;
    createInfo.ppEnabledLayerNames     = enabledInstanceLayerNames_array;

    if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (gEnableValidation && (enabledInstanceLayerCount > 0)) {
        VkDebugUtilsMessengerCreateInfoEXT dbgCreate{};
        dbgCreate.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgCreate.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                  | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                  | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgCreate.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                  | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                  | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgCreate.pfnUserCallback = DebugCallback;
        CreateDebugUtilsMessengerEXT(vkInstance, &dbgCreate, nullptr, &gDebugUtilsMessenger);
    }
    return VK_SUCCESS;
}

static VkResult CreateSurfaceWin32()
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    createInfo.hwnd      = ghwnd;
    return vkCreateWin32SurfaceKHR(vkInstance, &createInfo, nullptr, &vkSurfaceKHR);
}

static VkResult SelectPhysicalDevice()
{
    vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr);
    if (physicalDeviceCount == 0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkPhysicalDevice> devices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, devices.data());

    bool found = false;
    for (auto& pd : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);

        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, qfs.data());

        std::vector<VkBool32> canPresent(queueCount);
        for (uint32_t i = 0; i < queueCount; i++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, vkSurfaceKHR, &canPresent[i]);
        }

        for (uint32_t i = 0; i < queueCount; i++) {
            if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent[i]) {
                vkPhysicalDevice_sel = pd;
                graphicsQueueIndex   = i;
                found                = true;
                break;
            }
        }
        if (found) break;
    }
    if (!found) return VK_ERROR_INITIALIZATION_FAILED;

    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_sel, &vkPhysicalDeviceMemoryProperties);
    return VK_SUCCESS;
}

static uint32_t enabledDeviceExtensionsCount = 0;
static const char* enabledDeviceExtensionNames_array[1];

static VkResult FillDeviceExtensionNames()
{
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel, nullptr, &extCount, nullptr);
    if (extCount == 0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel, nullptr, &extCount, exts.data());

    bool foundSwapchain = false;
    for (auto &e : exts) {
        if (!strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            foundSwapchain = true;
            break;
        }
    }
    if (!foundSwapchain) return VK_ERROR_INITIALIZATION_FAILED;

    enabledDeviceExtensionsCount = 0;
    enabledDeviceExtensionNames_array[enabledDeviceExtensionsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    return VK_SUCCESS;
}

static VkResult CreateLogicalDeviceAndQueue()
{
    if (FillDeviceExtensionNames() != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;

    float priority = 1.0f;
    VkDeviceQueueCreateInfo dqci{};
    dqci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    dqci.queueFamilyIndex = graphicsQueueIndex;
    dqci.queueCount       = 1;
    dqci.pQueuePriorities = &priority;

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = 1;
    ci.pQueueCreateInfos       = &dqci;
    ci.enabledExtensionCount   = enabledDeviceExtensionsCount;
    ci.ppEnabledExtensionNames = enabledDeviceExtensionNames_array;
    ci.pEnabledFeatures        = &features;

    if (vkCreateDevice(vkPhysicalDevice_sel, &ci, nullptr, &vkDevice) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    vkGetDeviceQueue(vkDevice, graphicsQueueIndex, 0, &vkQueue);
    return VK_SUCCESS;
}

// Surface format
static VkResult getSurfaceFormatAndColorSpace()
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &count, nullptr);
    if (count == 0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkSurfaceFormatKHR> fmts(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &count, fmts.data());

    if (count == 1 && fmts[0].format == VK_FORMAT_UNDEFINED) {
        vkFormat_color  = VK_FORMAT_B8G8R8A8_UNORM;
        vkColorSpaceKHR = fmts[0].colorSpace;
    } else {
        vkFormat_color  = fmts[0].format;
        vkColorSpaceKHR = fmts[0].colorSpace;
    }
    return VK_SUCCESS;
}

// Present mode
static VkResult getPresentMode()
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &count, nullptr);
    if (count == 0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkPresentModeKHR> pms(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &count, pms.data());

    vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
    for (auto pm : pms) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    return VK_SUCCESS;
}

VkResult CreateSwapChain(VkBool32 /*unused*/ = VK_FALSE)
{
    if (getSurfaceFormatAndColorSpace() != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &caps);

    uint32_t desiredCount = caps.minImageCount + 1;
    if ((caps.maxImageCount > 0) && (desiredCount > caps.maxImageCount))
        desiredCount = caps.maxImageCount;

    if (caps.currentExtent.width != UINT32_MAX) {
        vkExtent2D_SwapChain = caps.currentExtent;
    } else {
        vkExtent2D_SwapChain.width  = (winWidth  < caps.minImageExtent.width)  ? caps.minImageExtent.width  : winWidth;
        vkExtent2D_SwapChain.width  = (vkExtent2D_SwapChain.width > caps.maxImageExtent.width)
                                    ? caps.maxImageExtent.width : vkExtent2D_SwapChain.width;
        vkExtent2D_SwapChain.height = (winHeight < caps.minImageExtent.height) ? caps.minImageExtent.height : winHeight;
        vkExtent2D_SwapChain.height = (vkExtent2D_SwapChain.height > caps.maxImageExtent.height)
                                    ? caps.maxImageExtent.height : vkExtent2D_SwapChain.height;
    }

    if (getPresentMode() != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = vkSurfaceKHR;
    ci.minImageCount    = desiredCount;
    ci.imageFormat      = vkFormat_color;
    ci.imageColorSpace  = vkColorSpaceKHR;
    ci.imageExtent      = vkExtent2D_SwapChain;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = vkPresentModeKHR;
    ci.clipped          = VK_TRUE;

    if (vkCreateSwapchainKHR(vkDevice, &ci, nullptr, &vkSwapchainKHR) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    return VK_SUCCESS;
}

VkResult CreateImagesAndImageViews()
{
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, nullptr);
    if (swapchainImageCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    swapChainImage_array = (VkImage*)malloc(sizeof(VkImage)*swapchainImageCount);
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapChainImage_array);

    swapChainImageView_array = (VkImageView*)malloc(sizeof(VkImageView)*swapchainImageCount);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = swapChainImage_array[i];
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = vkFormat_color;
        vi.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.baseMipLevel   = 0;
        vi.subresourceRange.levelCount     = 1;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(vkDevice, &vi, nullptr, &swapChainImageView_array[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkResult CreateCommandPool()
{
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = graphicsQueueIndex;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    return vkCreateCommandPool(vkDevice, &ci, nullptr, &vkCommandPool);
}

VkResult CreateCommandBuffers()
{
    vkCommandBuffer_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*swapchainImageCount);

    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = vkCommandPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = swapchainImageCount;

    if (vkAllocateCommandBuffers(vkDevice, &ai, vkCommandBuffer_array) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    return VK_SUCCESS;
}

VkResult CreateSemaphores()
{
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vkDevice, &sci, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
        if (vkCreateSemaphore(vkDevice, &sci, nullptr, &renderFinishedSemaphores[i])  != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkResult CreateFences()
{
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(vkDevice, &fci, nullptr, &inFlightFences[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (auto &f : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice_sel, f, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)  return f;
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)return f;
    }
    return VK_FORMAT_UNDEFINED;
}

VkFormat findDepthFormat()
{
    return findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkResult CreateDepthResources()
{
    gDepthFormat = findDepthFormat();
    if (gDepthFormat == VK_FORMAT_UNDEFINED) return VK_ERROR_INITIALIZATION_FAILED;

    gDepthImages.resize(swapchainImageCount);
    gDepthImagesMemory.resize(swapchainImageCount);
    gDepthImagesView.resize(swapchainImageCount);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageCreateInfo ci{};
        ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType     = VK_IMAGE_TYPE_2D;
        ci.extent.width  = vkExtent2D_SwapChain.width;
        ci.extent.height = vkExtent2D_SwapChain.height;
        ci.extent.depth  = 1;
        ci.mipLevels     = 1;
        ci.arrayLayers   = 1;
        ci.format        = gDepthFormat;
        ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ci.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        ci.samples       = VK_SAMPLE_COUNT_1_BIT;
        ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(vkDevice, &ci, nullptr, &gDepthImages[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(vkDevice, gDepthImages[i], &memReq);

        auto FindMemoryTypeIndex = [&](uint32_t typeFilter, VkMemoryPropertyFlags props)->uint32_t {
            for (uint32_t j = 0; j < vkPhysicalDeviceMemoryProperties.memoryTypeCount; j++) {
                if ((typeFilter & (1 << j)) &&
                    (vkPhysicalDeviceMemoryProperties.memoryTypes[j].propertyFlags & props) == props)
                    return j;
            }
            return UINT32_MAX;
        };

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = memReq.size;
        mai.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (mai.memoryTypeIndex == UINT32_MAX) return VK_ERROR_INITIALIZATION_FAILED;
        if (vkAllocateMemory(vkDevice, &mai, nullptr, &gDepthImagesMemory[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;

        vkBindImageMemory(vkDevice, gDepthImages[i], gDepthImagesMemory[i], 0);

        VkImageViewCreateInfo vi{};
        vi.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image                           = gDepthImages[i];
        vi.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        vi.format                          = gDepthFormat;
        vi.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        vi.subresourceRange.baseMipLevel   = 0;
        vi.subresourceRange.levelCount     = 1;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(vkDevice, &vi, nullptr, &gDepthImagesView[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkResult CreateRenderPass()
{
    VkAttachmentDescription colorAtt{};
    colorAtt.format         = vkFormat_color;
    colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAtt{};
    depthAtt.format         = gDepthFormat;
    depthAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAtt.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription,2> atts = {colorAtt, depthAtt};

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = (uint32_t)atts.size();
    rpci.pAttachments    = atts.data();
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    return vkCreateRenderPass(vkDevice, &rpci, nullptr, &gRenderPass);
}

VkResult CreateFramebuffers()
{
    gFramebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        std::array<VkImageView,2> attachments = {swapChainImageView_array[i], gDepthImagesView[i]};
        VkFramebufferCreateInfo fbci{};
        fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass      = gRenderPass;
        fbci.attachmentCount = (uint32_t)attachments.size();
        fbci.pAttachments    = attachments.data();
        fbci.width           = vkExtent2D_SwapChain.width;
        fbci.height          = vkExtent2D_SwapChain.height;
        fbci.layers          = 1;
        if (vkCreateFramebuffer(vkDevice, &fbci, nullptr, &gFramebuffers[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

static uint32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    for (uint32_t i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    return UINT32_MAX;
}

VkResult CreateVertexBuffer()
{
    VkDeviceSize size = sizeof(Vertex) * gFinalVertices.size();

    VkBufferCreateInfo ci{};
    ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size        = size;
    ci.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vkDevice, &ci, nullptr, &gVertexBuffer) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gVertexBuffer, &memReq);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mai.memoryTypeIndex == UINT32_MAX) return VK_ERROR_INITIALIZATION_FAILED;
    if (vkAllocateMemory(vkDevice, &mai, nullptr, &gVertexBufferMemory) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    vkBindBufferMemory(vkDevice, gVertexBuffer, gVertexBufferMemory, 0);

    void* data;
    vkMapMemory(vkDevice, gVertexBufferMemory, 0, size, 0, &data);
    memcpy(data, gFinalVertices.data(), (size_t)size);
    vkUnmapMemory(vkDevice, gVertexBufferMemory);
    return VK_SUCCESS;
}

VkResult CreateUniformBuffer()
{
    VkDeviceSize size = sizeof(UniformBufferObject);

    VkBufferCreateInfo ci{};
    ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size        = size;
    ci.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vkDevice, &ci, nullptr, &gUniformBuffer) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gUniformBuffer, &memReq);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mai.memoryTypeIndex == UINT32_MAX) return VK_ERROR_INITIALIZATION_FAILED;
    if (vkAllocateMemory(vkDevice, &mai, nullptr, &gUniformBufferMemory) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    vkBindBufferMemory(vkDevice, gUniformBuffer, gUniformBufferMemory, 0);
    return VK_SUCCESS;
}

VkResult CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding ubo{};
    ubo.binding         = 0;
    ubo.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 1;
    ci.pBindings    = &ubo;
    return vkCreateDescriptorSetLayout(vkDevice, &ci, nullptr, &gDescriptorSetLayout);
}

VkResult CreateDescriptorPool()
{
    VkDescriptorPoolSize ps{};
    ps.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ps.descriptorCount = 1;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount = 1;
    ci.pPoolSizes    = &ps;
    ci.maxSets       = 1;
    return vkCreateDescriptorPool(vkDevice, &ci, nullptr, &gDescriptorPool);
}

VkResult CreateDescriptorSet()
{
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = gDescriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &gDescriptorSetLayout;

    if (vkAllocateDescriptorSets(vkDevice, &ai, &gDescriptorSet) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkDescriptorBufferInfo bi{};
    bi.buffer = gUniformBuffer;
    bi.offset = 0;
    bi.range  = sizeof(UniformBufferObject);

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = gDescriptorSet;
    w.dstBinding      = 0;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo     = &bi;
    vkUpdateDescriptorSets(vkDevice, 1, &w, 0, nullptr);
    return VK_SUCCESS;
}

std::vector<char> ReadFile(const std::string& fname)
{
    std::ifstream file(fname, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
    size_t sz = (size_t)file.tellg();
    std::vector<char> buf(sz);
    file.seekg(0); file.read(buf.data(), sz); file.close();
    return buf;
}

VkShaderModule CreateShaderModule(const std::vector<char>& code)
{
    if (code.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule sm;
    if (vkCreateShaderModule(vkDevice, &ci, nullptr, &sm) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return sm;
}

VkVertexInputBindingDescription GetVertexBindingDescription()
{
    VkVertexInputBindingDescription bd{};
    bd.binding   = 0;
    bd.stride    = sizeof(Vertex);
    bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bd;
}

std::array<VkVertexInputAttributeDescription,2> GetVertexAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription,2> at{};
    at[0].binding  = 0;
    at[0].location = 0;
    at[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    at[0].offset   = offsetof(Vertex, pos);

    at[1].binding  = 0;
    at[1].location = 1;
    at[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    at[1].offset   = offsetof(Vertex, color);
    return at;
}

VkResult CreateGraphicsPipeline()
{
    auto vertCode = ReadFile("vert_shader.spv");
    auto fragCode = ReadFile("frag_shader.spv");
    VkShaderModule vertModule = CreateShaderModule(vertCode);
    VkShaderModule fragModule = CreateShaderModule(fragCode);
    if (!vertModule || !fragModule) return VK_ERROR_INITIALIZATION_FAILED;

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

    VkPipelineShaderStageCreateInfo stages[] = {vs, fs};

    auto bindingDesc    = GetVertexBindingDescription();
    auto attributeDescs = GetVertexAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount   = 1;
    vi.pVertexBindingDescriptions      = &bindingDesc;
    vi.vertexAttributeDescriptionCount = (uint32_t)attributeDescs.size();
    vi.pVertexAttributeDescriptions    = attributeDescs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.width    = (float)vkExtent2D_SwapChain.width;
    viewport.height   = (float)vkExtent2D_SwapChain.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = vkExtent2D_SwapChain;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.pViewports    = &viewport;
    vp.scissorCount  = 1;
    vp.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    // Disable culling so all faces are visible
    rs.cullMode    = VK_CULL_MODE_NONE; 
    rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cbAtt{};
    cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbAtt;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts    = &gDescriptorSetLayout;

    if (vkCreatePipelineLayout(vkDevice, &plci, nullptr, &gPipelineLayout) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount          = 2;
    pci.pStages             = stages;
    pci.pVertexInputState   = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState      = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState   = &ms;
    pci.pDepthStencilState  = &ds;
    pci.pColorBlendState    = &cb;
    pci.layout              = gPipelineLayout;
    pci.renderPass          = gRenderPass;
    if (vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pci, nullptr, &gGraphicsPipeline) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    vkDestroyShaderModule(vkDevice, fragModule, nullptr);
    vkDestroyShaderModule(vkDevice, vertModule, nullptr);
    return VK_SUCCESS;
}

VkResult buildCommandBuffers()
{
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        vkResetCommandBuffer(vkCommandBuffer_array[i], 0);

        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(vkCommandBuffer_array[i], &bi);

        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = swapChainImage_array[i];
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask       = 0;
        barrier.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(
            vkCommandBuffer_array[i],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier
        );

        VkImageMemoryBarrier depthBarrier{};
        depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depthBarrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        depthBarrier.newLayout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.image               = gDepthImages[i];
        depthBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthBarrier.subresourceRange.baseMipLevel   = 0;
        depthBarrier.subresourceRange.levelCount     = 1;
        depthBarrier.subresourceRange.baseArrayLayer = 0;
        depthBarrier.subresourceRange.layerCount     = 1;
        depthBarrier.srcAccessMask                   = 0;
        depthBarrier.dstAccessMask                   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(
            vkCommandBuffer_array[i],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0, 0, nullptr, 0, nullptr, 1, &depthBarrier
        );

        VkClearValue clearVals[2];
        clearVals[0].color = vkClearColorValue;
        clearVals[1].depthStencil.depth   = 1.0f;
        clearVals[1].depthStencil.stencil = 0;

        VkRenderPassBeginInfo rpbi{};
        rpbi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass        = gRenderPass;
        rpbi.framebuffer       = gFramebuffers[i];
        rpbi.renderArea.offset = {0,0};
        rpbi.renderArea.extent = vkExtent2D_SwapChain;
        rpbi.clearValueCount   = 2;
        rpbi.pClearValues      = clearVals;
        vkCmdBeginRenderPass(vkCommandBuffer_array[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(vkCommandBuffer_array[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gGraphicsPipeline);

        VkBuffer vb[] = {gVertexBuffer};
        VkDeviceSize offs[] = {0};
        vkCmdBindVertexBuffers(vkCommandBuffer_array[i], 0, 1, vb, offs);

        vkCmdBindDescriptorSets(vkCommandBuffer_array[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS, gPipelineLayout,
            0, 1, &gDescriptorSet, 0, nullptr);

        vkCmdDraw(vkCommandBuffer_array[i], (uint32_t)gFinalVertices.size(), 1, 0, 0);

        vkCmdEndRenderPass(vkCommandBuffer_array[i]);

        VkImageMemoryBarrier barrier2{};
        barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier2.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier2.newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier2.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier2.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier2.image                           = swapChainImage_array[i];
        barrier2.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier2.subresourceRange.baseMipLevel   = 0;
        barrier2.subresourceRange.levelCount     = 1;
        barrier2.subresourceRange.baseArrayLayer = 0;
        barrier2.subresourceRange.layerCount     = 1;
        barrier2.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier2.dstAccessMask = 0;

        vkCmdPipelineBarrier(vkCommandBuffer_array[i],
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier2);

        vkEndCommandBuffer(vkCommandBuffer_array[i]);
    }
    return VK_SUCCESS;
}

void cleanupDepthResources()
{
    for (uint32_t i = 0; i < gDepthImages.size(); i++) {
        if (gDepthImagesView[i])   vkDestroyImageView(vkDevice, gDepthImagesView[i], nullptr);
        if (gDepthImages[i])       vkDestroyImage(vkDevice, gDepthImages[i], nullptr);
        if (gDepthImagesMemory[i]) vkFreeMemory(vkDevice, gDepthImagesMemory[i], nullptr);
    }
    gDepthImagesView.clear();
    gDepthImages.clear();
    gDepthImagesMemory.clear();
}

void cleanupSwapChain()
{
    vkDeviceWaitIdle(vkDevice);
    if (vkCommandBuffer_array) {
        vkFreeCommandBuffers(vkDevice, vkCommandPool, swapchainImageCount, vkCommandBuffer_array);
        free(vkCommandBuffer_array);
        vkCommandBuffer_array = nullptr;
    }
    if (gFramebuffers) {
        for (uint32_t i = 0; i < swapchainImageCount; i++) {
            if (gFramebuffers[i]) vkDestroyFramebuffer(vkDevice, gFramebuffers[i], nullptr);
        }
        free(gFramebuffers);
        gFramebuffers = nullptr;
    }
    if (vkCommandPool) {
        vkDestroyCommandPool(vkDevice, vkCommandPool, nullptr);
        vkCommandPool = VK_NULL_HANDLE;
    }
    if (swapChainImageView_array) {
        for (uint32_t i = 0; i < swapchainImageCount; i++) {
            if (swapChainImageView_array[i]) vkDestroyImageView(vkDevice, swapChainImageView_array[i], nullptr);
        }
        free(swapChainImageView_array);
        swapChainImageView_array = nullptr;
    }
    if (swapChainImage_array) {
        free(swapChainImage_array);
        swapChainImage_array = nullptr;
    }
    if (vkSwapchainKHR) {
        vkDestroySwapchainKHR(vkDevice, vkSwapchainKHR, nullptr);
        vkSwapchainKHR = VK_NULL_HANDLE;
    }
    if (gRenderPass) {
        vkDestroyRenderPass(vkDevice, gRenderPass, nullptr);
        gRenderPass = VK_NULL_HANDLE;
    }
    cleanupDepthResources();
}

VkResult recreateSwapChain()
{
    if (winWidth == 0 || winHeight == 0) return VK_SUCCESS;
    cleanupSwapChain();

    if (CreateSwapChain()             != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateImagesAndImageViews()   != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateDepthResources()        != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateRenderPass()            != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateFramebuffers()          != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateCommandPool()           != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateCommandBuffers()        != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;

    if (gGraphicsPipeline) {
        vkDestroyPipeline(vkDevice, gGraphicsPipeline, nullptr);
        gGraphicsPipeline = VK_NULL_HANDLE;
    }
    if (gPipelineLayout) {
        vkDestroyPipelineLayout(vkDevice, gPipelineLayout, nullptr);
        gPipelineLayout = VK_NULL_HANDLE;
    }

    if (CreateGraphicsPipeline() != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (buildCommandBuffers()    != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    return VK_SUCCESS;
}

VkResult initialize(void)
{
    if (CreateVulkanInstance()          != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateSurfaceWin32()            != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (SelectPhysicalDevice()          != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateLogicalDeviceAndQueue()   != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateSwapChain()               != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateImagesAndImageViews()     != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateDepthResources()          != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateRenderPass()              != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateFramebuffers()            != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateCommandPool()             != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateCommandBuffers()          != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateDescriptorSetLayout()     != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateUniformBuffer()           != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateDescriptorPool()          != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateDescriptorSet()           != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateVertexBuffer()            != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateGraphicsPipeline()        != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateSemaphores()              != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateFences()                  != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;

    memset(&vkClearColorValue, 0, sizeof(vkClearColorValue));
    vkClearColorValue.float32[0] = 0.2f;
    vkClearColorValue.float32[1] = 0.2f;
    vkClearColorValue.float32[2] = 0.2f;
    vkClearColorValue.float32[3] = 1.0f;

    if (buildCommandBuffers() != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    bInitialized = TRUE;
    return VK_SUCCESS;
}

void update(void)
{
    static float angle = 0.0f;
    angle += 0.5f; if (angle >= 360.0f) angle -= 360.0f;

    UniformBufferObject ubo{};
    vmath::mat4 model = vmath::mat4::identity();
    vmath::mat4 view  = vmath::mat4::identity();
    float aspect = (winHeight == 0) ? 1.0f : (float)winWidth / (float)winHeight;
    vmath::mat4 proj  = vmath::perspective(45.0f, aspect, 0.1f, 100.0f);
    proj[1][1] *= -1.0f;

    model = vmath::translate(0.0f, 0.0f, -5.0f) * vmath::rotate(angle, 1.0f, 0.0f, 0.0f);
    ubo.mvp = proj * view * model;

    void* data;
    vkMapMemory(vkDevice, gUniformBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(vkDevice, gUniformBufferMemory);
}

VkResult display(void)
{
    if (!bInitialized) return VK_SUCCESS;

    vkWaitForFences(vkDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(vkDevice, 1, &inFlightFences[currentFrame]);

    VkResult result = vkAcquireNextImageKHR(vkDevice, vkSwapchainKHR, UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &currentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapChain(); return VK_SUCCESS; }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return result;
    }

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &imageAvailableSemaphores[currentFrame];
    si.pWaitDstStageMask    = waitStages;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &vkCommandBuffer_array[currentImageIndex];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &renderFinishedSemaphores[currentFrame];
    vkQueueSubmit(vkQueue, 1, &si, inFlightFences[currentFrame]);

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &renderFinishedSemaphores[currentFrame];
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &vkSwapchainKHR;
    pi.pImageIndices      = &currentImageIndex;

    result = vkQueuePresentKHR(vkQueue, &pi);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

void uninitialize(void)
{
    if (gbFullscreen) { ToggleFullscreen(); gbFullscreen = FALSE; }
    if (vkDevice) {
        vkDeviceWaitIdle(vkDevice);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (inFlightFences[i])           vkDestroyFence(vkDevice, inFlightFences[i], nullptr);
            if (renderFinishedSemaphores[i]) vkDestroySemaphore(vkDevice, renderFinishedSemaphores[i], nullptr);
            if (imageAvailableSemaphores[i]) vkDestroySemaphore(vkDevice, imageAvailableSemaphores[i], nullptr);
        }
        cleanupSwapChain();
        if (gGraphicsPipeline)    vkDestroyPipeline(vkDevice, gGraphicsPipeline, nullptr);
        if (gPipelineLayout)      vkDestroyPipelineLayout(vkDevice, gPipelineLayout, nullptr);
        if (gDescriptorPool)      vkDestroyDescriptorPool(vkDevice, gDescriptorPool, nullptr);
        if (gDescriptorSetLayout) vkDestroyDescriptorSetLayout(vkDevice, gDescriptorSetLayout, nullptr);
        if (gUniformBuffer)       vkDestroyBuffer(vkDevice, gUniformBuffer, nullptr);
        if (gUniformBufferMemory) vkFreeMemory(vkDevice, gUniformBufferMemory, nullptr);
        if (gVertexBuffer)        vkDestroyBuffer(vkDevice, gVertexBuffer, nullptr);
        if (gVertexBufferMemory)  vkFreeMemory(vkDevice, gVertexBufferMemory, nullptr);
        vkDestroyDevice(vkDevice, nullptr);
    }

    if (gDebugUtilsMessenger) {
        DestroyDebugUtilsMessengerEXT(vkInstance, gDebugUtilsMessenger, nullptr);
        gDebugUtilsMessenger = VK_NULL_HANDLE;
    }

    if (vkSurfaceKHR) {
        vkDestroySurfaceKHR(vkInstance, vkSurfaceKHR, nullptr);
        vkSurfaceKHR = VK_NULL_HANDLE;
    }
    if (vkInstance) {
        vkDestroyInstance(vkInstance, nullptr);
        vkInstance = VK_NULL_HANDLE;
    }
    if (ghwnd) {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }
    if (gFILE) {
        fclose(gFILE);
        gFILE = nullptr;
    }
}
