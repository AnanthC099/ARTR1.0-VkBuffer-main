#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <fstream>
#include <vector>
#include <array>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"  // For glm::ortho

// If you have a custom 'VK.h' for resource defines, include if needed:
// #include "VK.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#pragma comment(lib, "vulkan-1.lib")

// Global Function Declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// For convenience
#define WIN_WIDTH  800
#define WIN_HEIGHT 600

// Global Variables
const char* gpszAppName = "Vulkan_Ortho_Triangle";

HWND ghwnd = NULL;
BOOL gbActive = FALSE;
DWORD dwStyle = 0;
WINDOWPLACEMENT wpPrev{ sizeof(WINDOWPLACEMENT) };
BOOL gbFullscreen = FALSE;

FILE* gFILE = nullptr;

bool gEnableValidation = false; // If you want validation, set true

// Debug messenger
static VkDebugUtilsMessengerEXT gDebugUtilsMessenger = VK_NULL_HANDLE;

// For your callback
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT        messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    fprintf(gFILE, "VALIDATION LAYER: %s\n", pCallbackData->pMessage);
    fflush(gFILE);
    return VK_FALSE;
}

// Helper to create/destroy debug messenger
static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger)
{
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
        return func(instance, pCreateInfo, pAllocator, pMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}
static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
        func(instance, messenger, pAllocator);
}

// Forward Declarations
VkResult initialize(void);
void uninitialize(void);
VkResult display(void);
void update(void);

// Vulkan Globals
VkInstance          vkInstance           = VK_NULL_HANDLE;
VkSurfaceKHR        vkSurfaceKHR         = VK_NULL_HANDLE;
VkPhysicalDevice    vkPhysicalDevice_sel = VK_NULL_HANDLE;
VkDevice            vkDevice             = VK_NULL_HANDLE;
VkQueue             vkQueue              = VK_NULL_HANDLE;
uint32_t            graphicsQueueIndex   = UINT32_MAX;

uint32_t            physicalDeviceCount  = 0;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties{};

VkFormat            vkFormat_color       = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR     vkColorSpaceKHR      = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR    vkPresentModeKHR     = VK_PRESENT_MODE_FIFO_KHR;

VkSwapchainKHR      vkSwapchainKHR       = VK_NULL_HANDLE;
uint32_t            swapchainImageCount  = 0;
VkImage*            swapChainImage_array = nullptr;
VkImageView*        swapChainImageView_array = nullptr;

VkRenderPass        vkRenderPass         = VK_NULL_HANDLE;
VkExtent2D          vkExtent2D_SwapChain{ WIN_WIDTH, WIN_HEIGHT };

VkCommandPool       vkCommandPool        = VK_NULL_HANDLE;
VkCommandBuffer*    vkCommandBuffer_array= nullptr;
VkFramebuffer*      vkFramebuffer_array  = nullptr;

// Per-frame sync
#define MAX_FRAMES_IN_FLIGHT 2
VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
VkFence     inFlightFences[MAX_FRAMES_IN_FLIGHT];
uint32_t    currentFrame = 0;

// For clarity in the commands
uint32_t    currentImageIndex = UINT32_MAX;

// For clearing the screen to (0, 0, 1, 1)
VkClearColorValue vkClearColorValue{};

// Window dimension
int  winWidth  = WIN_WIDTH;
int  winHeight = WIN_HEIGHT;
BOOL bInitialized = FALSE;

// ============================================================================
// ADDITIONAL Globals for Triangle + Orthographic
// ============================================================================
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

static std::vector<Vertex> gTriangleVertices =
{
    //   position      ,    color
    {{  0.0f,  -0.5f }, { 1.0f, 0.0f, 0.0f }},  // bottom center - RED
    {{  0.5f,   0.5f }, { 0.0f, 1.0f, 0.0f }},  // right top - GREEN
    {{ -0.5f,   0.5f }, { 0.0f, 0.0f, 1.0f }},  // left top - BLUE
};

struct UniformBufferObject {
    glm::mat4 proj;
};

// Pipeline data
VkPipelineLayout      gPipelineLayout       = VK_NULL_HANDLE;
VkPipeline            gGraphicsPipeline     = VK_NULL_HANDLE;
VkDescriptorSetLayout gDescriptorSetLayout  = VK_NULL_HANDLE;
VkDescriptorPool      gDescriptorPool       = VK_NULL_HANDLE;
VkDescriptorSet       gDescriptorSet        = VK_NULL_HANDLE;

// Buffers
VkBuffer       gVertexBuffer       = VK_NULL_HANDLE;
VkDeviceMemory gVertexBufferMemory = VK_NULL_HANDLE;

VkBuffer       gUniformBuffer       = VK_NULL_HANDLE;
VkDeviceMemory gUniformBufferMemory = VK_NULL_HANDLE;

// ============================================================================
// Windows Entry
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
    // Create/Open log file
    gFILE = fopen("Log.txt", "w");
    if (!gFILE) {
        MessageBox(NULL, TEXT("Cannot open Log file."), TEXT("Error"), MB_OK | MB_ICONERROR);
        exit(0);
    }
    fprintf(gFILE, "Program started.\n");
    fflush(gFILE);

    // Window class
    WNDCLASSEX wndclass{};
    wndclass.cbSize        = sizeof(WNDCLASSEX);
    wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.hInstance     = hInstance;
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpszClassName = gpszAppName;
    wndclass.lpszMenuName  = NULL;
    // If you have an icon resource, you can load it:
    // wndclass.hIcon       = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
    // wndclass.hIconSm     = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));

    RegisterClassEx(&wndclass);

    // Position window
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int xCoord  = (screenW - WIN_WIDTH ) / 2;
    int yCoord  = (screenH - WIN_HEIGHT) / 2;

    ghwnd = CreateWindowEx(WS_EX_APPWINDOW,
                           gpszAppName,
                           TEXT("Vulkan + glm Orthographic Triangle"),
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
                           xCoord,
                           yCoord,
                           WIN_WIDTH,
                           WIN_HEIGHT,
                           NULL,
                           NULL,
                           hInstance,
                           NULL);

    if (!ghwnd) {
        fprintf(gFILE, "Cannot create window.\n");
        fflush(gFILE);
        return 0;
    }

    // Initialize Vulkan
    VkResult vkResult = initialize();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize() failed.\n");
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    } else {
        fprintf(gFILE, "initialize() succeeded.\n");
    }

    ShowWindow(ghwnd, iCmdShow);
    UpdateWindow(ghwnd);
    SetForegroundWindow(ghwnd);
    SetFocus(ghwnd);

    BOOL bDone = FALSE;
    MSG msg{};
    while (bDone == FALSE) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                bDone = TRUE;
            else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            if (gbActive == TRUE) {
                display();
                update(); // We update our uniform buffer each frame
            }
        }
    }

    uninitialize();
    return (int)msg.wParam;
}

// Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    switch (iMsg) {
    case WM_CREATE:
        memset((void*)&wpPrev, 0, sizeof(WINDOWPLACEMENT));
        wpPrev.length = sizeof(WINDOWPLACEMENT);
        break;

    case WM_SETFOCUS:
        gbActive = TRUE;
        break;

    case WM_KILLFOCUS:
        gbActive = FALSE;
        break;

    case WM_SIZE:
        if (bInitialized == TRUE) {
            // The 'resize' function is below
            int width  = LOWORD(lParam);
            int height = HIWORD(lParam);

            winWidth  = width;
            winHeight = height;

            // Recreate swapchain if not minimized
            if (width > 0 && height > 0) {
                // We'll do a helper function
                extern VkResult recreateSwapChain(void);
                recreateSwapChain();
            }
        }
        break;

    case WM_KEYDOWN:
        switch (LOWORD(wParam)) {
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_CHAR:
        switch (LOWORD(wParam)) {
        case 'F':
        case 'f':
            if (!gbFullscreen) {
                // Toggle function below
                extern void ToggleFullscreen(void);
                ToggleFullscreen();
                gbFullscreen = TRUE;
                fprintf(gFILE, "Entered Fullscreen.\n");
            } else {
                extern void ToggleFullscreen(void);
                ToggleFullscreen();
                gbFullscreen = FALSE;
                fprintf(gFILE, "Exited Fullscreen.\n");
            }
            fflush(gFILE);
            break;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        break;
    }
    return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

// ToggleFullscreen
void ToggleFullscreen(void)
{
    MONITORINFO mi{ sizeof(MONITORINFO) };
    if (!gbFullscreen) {
        dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
        if (dwStyle & WS_OVERLAPPEDWINDOW) {
            if (GetWindowPlacement(ghwnd, &wpPrev) &&
                GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi)) {
                SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(ghwnd, HWND_TOP,
                    mi.rcMonitor.left,
                    mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOZORDER | SWP_FRAMECHANGED);
            }
        }
        ShowCursor(FALSE);
    } else {
        SetWindowPlacement(ghwnd, &wpPrev);
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    }
}

// ============================================================================
// Vulkan Initialization Steps
// ============================================================================

// 1) Fill/Check Instance layers
static uint32_t enabledInstanceLayerCount = 0;
static const char* enabledInstanceLayerNames_array[1];

static VkResult FillInstanceLayerNames()
{
    if (!gEnableValidation) {
        fprintf(gFILE, "Validation disabled, skipping instance layer checks.\n");
        return VK_SUCCESS;
    }

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    if (layerCount == 0) {
        fprintf(gFILE, "No instance layers found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    bool foundValidation = false;
    for (auto& lp : layers) {
        if (strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            foundValidation = true;
            break;
        }
    }

    if (foundValidation) {
        enabledInstanceLayerNames_array[0] = "VK_LAYER_KHRONOS_validation";
        enabledInstanceLayerCount = 1;
        fprintf(gFILE, "Using VK_LAYER_KHRONOS_validation.\n");
    } else {
        fprintf(gFILE, "Validation layer not found.\n");
        // You can return an error or just continue without it.
    }
    return VK_SUCCESS;
}

// 2) Fill instance extension names
static uint32_t enabledInstanceExtensionsCount = 0;
static const char* enabledInstanceExtensionNames_array[4];

static void AddDebugUtilsExtensionIfPresent()
{
    if (!gEnableValidation)
        return;

    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    if (extCount == 0) return;

    std::vector<VkExtensionProperties> props(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, props.data());

    for (auto& ep : props) {
        if (strcmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
            enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            fprintf(gFILE, "Enabling: %s\n", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            break;
        }
    }
}

static VkResult FillInstanceExtensionNames()
{
    // We definitely need the surface/Win32 surface
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (extensionCount == 0) {
        fprintf(gFILE, "No instance extensions found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::vector<VkExtensionProperties> exts(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, exts.data());

    bool foundSurface       = false;
    bool foundWin32Surface  = false;
    for (auto& e : exts) {
        if (strcmp(e.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0)
            foundSurface = true;
        if (strcmp(e.extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0)
            foundWin32Surface = true;
    }

    if (!foundSurface || !foundWin32Surface) {
        fprintf(gFILE, "Required extension(s) not found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    enabledInstanceExtensionsCount = 0;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

    // Optionally add debug utils
    AddDebugUtilsExtensionIfPresent();

    return VK_SUCCESS;
}

// 3) Create the Vulkan instance
static VkResult CreateVulkanInstance()
{
    VkResult vkResult;

    vkResult = FillInstanceExtensionNames();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = FillInstanceLayerNames();
    if (vkResult != VK_SUCCESS) return vkResult;

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = gpszAppName;
    appInfo.applicationVersion = 1;
    appInfo.pEngineName        = gpszAppName;
    appInfo.engineVersion      = 1;
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = enabledInstanceExtensionsCount;
    createInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;
    createInfo.enabledLayerCount       = enabledInstanceLayerCount;
    createInfo.ppEnabledLayerNames     = enabledInstanceLayerNames_array;

    vkResult = vkCreateInstance(&createInfo, nullptr, &vkInstance);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "vkCreateInstance() failed\n");
        return vkResult;
    }

    // If validation is enabled, create debug messenger
    if (gEnableValidation && (enabledInstanceLayerCount > 0)) {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        debugCreateInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = DebugCallback;

        if (CreateDebugUtilsMessengerEXT(vkInstance, &debugCreateInfo, nullptr, &gDebugUtilsMessenger) != VK_SUCCESS) {
            fprintf(gFILE, "Could not create Debug Utils Messenger.\n");
            gDebugUtilsMessenger = VK_NULL_HANDLE;
        }
    }
    return VK_SUCCESS;
}

// 4) Create Win32 surface
static VkResult CreateSurfaceWin32()
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    createInfo.hwnd      = ghwnd;

    VkResult vkResult = vkCreateWin32SurfaceKHR(vkInstance, &createInfo, nullptr, &vkSurfaceKHR);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "vkCreateWin32SurfaceKHR() failed.\n");
    }
    return vkResult;
}

// 5) Enumerate physical devices + select a suitable one
static VkResult SelectPhysicalDevice()
{
    VkResult vkResult;

    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr);
    if (vkResult != VK_SUCCESS || physicalDeviceCount == 0) {
        fprintf(gFILE, "No physical devices found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, physicalDevices.data());
    if (vkResult != VK_SUCCESS) {
        return vkResult;
    }

    bool found = false;
    for (auto pd : physicalDevices) {
        // Check queue families
        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfProps(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, qfProps.data());

        std::vector<VkBool32> canPresent(queueCount);
        for (uint32_t i = 0; i < queueCount; i++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, vkSurfaceKHR, &canPresent[i]);
        }

        for (uint32_t i = 0; i < queueCount; i++) {
            if ((qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent[i]) {
                // Found a suitable device
                vkPhysicalDevice_sel = pd;
                graphicsQueueIndex   = i;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) {
        fprintf(gFILE, "Failed to find a suitable GPU with Graphics + Present.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_sel, &vkPhysicalDeviceMemoryProperties);
    return VK_SUCCESS;
}

// 6) Create logical device + queue
static uint32_t enabledDeviceExtensionsCount = 0;
static const char* enabledDeviceExtensionNames_array[1];

static VkResult FillDeviceExtensionNames()
{
    // We need swapchain extension
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel, nullptr, &extCount, nullptr);
    if (extCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel, nullptr, &extCount, exts.data());

    bool foundSwapchain = false;
    for (auto& e : exts) {
        if (strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            foundSwapchain = true;
        }
    }

    if (!foundSwapchain) {
        fprintf(gFILE, "Device does not have VK_KHR_swapchain.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    enabledDeviceExtensionsCount = 0;
    enabledDeviceExtensionNames_array[enabledDeviceExtensionsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    fprintf(gFILE, "Enabling device extension: VK_KHR_swapchain.\n");

    return VK_SUCCESS;
}

static VkResult CreateLogicalDeviceAndQueue()
{
    VkResult vkResult;

    vkResult = FillDeviceExtensionNames();
    if (vkResult != VK_SUCCESS) return vkResult;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = graphicsQueueIndex;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = 1;
    createInfo.pQueueCreateInfos       = &queueInfo;
    createInfo.enabledExtensionCount   = enabledDeviceExtensionsCount;
    createInfo.ppEnabledExtensionNames = enabledDeviceExtensionNames_array;
    // We don't enable any device-level validation layers here:
    createInfo.enabledLayerCount       = 0;

    vkResult = vkCreateDevice(vkPhysicalDevice_sel, &createInfo, nullptr, &vkDevice);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "vkCreateDevice() failed.\n");
        return vkResult;
    }

    // Retrieve the queue
    vkGetDeviceQueue(vkDevice, graphicsQueueIndex, 0, &vkQueue);
    return VK_SUCCESS;
}

// 7) Create Swapchain
static VkResult getSurfaceFormatAndColorSpace()
{
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &formatCount, nullptr);
    if (formatCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &formatCount, formats.data());

    if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        vkFormat_color = VK_FORMAT_B8G8R8A8_UNORM;
        vkColorSpaceKHR= formats[0].colorSpace;
    } else {
        // Just choose the first
        vkFormat_color = formats[0].format;
        vkColorSpaceKHR= formats[0].colorSpace;
    }
    return VK_SUCCESS;
}

static VkResult getPresentMode()
{
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &presentModeCount, nullptr);
    if (presentModeCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &presentModeCount, presentModes.data());

    // Default to FIFO
    vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
    for (auto pm : presentModes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    fprintf(gFILE, "Present Mode: %d\n", vkPresentModeKHR);
    return VK_SUCCESS;
}

VkResult CreateSwapChain(VkBool32 vsync = VK_FALSE)
{
    VkResult vkResult = getSurfaceFormatAndColorSpace();
    if (vkResult != VK_SUCCESS) return vkResult;

    VkSurfaceCapabilitiesKHR surfaceCaps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &surfaceCaps);

    uint32_t desiredImageCount = surfaceCaps.minImageCount + 1;
    if (surfaceCaps.maxImageCount > 0 && desiredImageCount > surfaceCaps.maxImageCount)
        desiredImageCount = surfaceCaps.maxImageCount;

    // If currentExtent is not undefined, use it:
    if (surfaceCaps.currentExtent.width != UINT32_MAX) {
        vkExtent2D_SwapChain = surfaceCaps.currentExtent;
    } else {
        vkExtent2D_SwapChain.width  = std::max(surfaceCaps.minImageExtent.width,
                                        std::min(surfaceCaps.maxImageExtent.width,  (uint32_t)winWidth));
        vkExtent2D_SwapChain.height = std::max(surfaceCaps.minImageExtent.height,
                                        std::min(surfaceCaps.maxImageExtent.height, (uint32_t)winHeight));
    }

    VkResult pmRes = getPresentMode();
    if (pmRes != VK_SUCCESS) return pmRes;

    VkSwapchainCreateInfoKHR swapInfo{};
    swapInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapInfo.surface          = vkSurfaceKHR;
    swapInfo.minImageCount    = desiredImageCount;
    swapInfo.imageFormat      = vkFormat_color;
    swapInfo.imageColorSpace  = vkColorSpaceKHR;
    swapInfo.imageExtent      = vkExtent2D_SwapChain;
    swapInfo.imageArrayLayers = 1;
    swapInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.preTransform     = surfaceCaps.currentTransform;
    swapInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapInfo.presentMode      = vkPresentModeKHR;
    swapInfo.clipped          = VK_TRUE;
    swapInfo.oldSwapchain     = VK_NULL_HANDLE;

    vkResult = vkCreateSwapchainKHR(vkDevice, &swapInfo, nullptr, &vkSwapchainKHR);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "vkCreateSwapchainKHR() failed.\n");
        return vkResult;
    }

    return VK_SUCCESS;
}

// 8) Create swapchain images + views
VkResult CreateImagesAndImageViews()
{
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, nullptr);
    if (swapchainImageCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    swapChainImage_array = (VkImage*)malloc(sizeof(VkImage)*swapchainImageCount);
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapChainImage_array);

    swapChainImageView_array = (VkImageView*)malloc(sizeof(VkImageView)*swapchainImageCount);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image      = swapChainImage_array[i];
        viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format     = vkFormat_color;
        viewInfo.components = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        };
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(vkDevice, &viewInfo, nullptr, &swapChainImageView_array[i]) != VK_SUCCESS) {
            fprintf(gFILE, "Could not create image view.\n");
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}

// 9) Create Command Pool + Buffers
VkResult CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueIndex;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &vkCommandPool) != VK_SUCCESS) {
        fprintf(gFILE, "vkCreateCommandPool() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkResult CreateCommandBuffers()
{
    vkCommandBuffer_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*swapchainImageCount);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = vkCommandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = swapchainImageCount;

    if (vkAllocateCommandBuffers(vkDevice, &allocInfo, vkCommandBuffer_array) != VK_SUCCESS) {
        fprintf(gFILE, "vkAllocateCommandBuffers() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

// 10) Create Render Pass
VkResult CreateRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = vkFormat_color;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount  = 1;
    rpInfo.pAttachments     = &colorAttachment;
    rpInfo.subpassCount     = 1;
    rpInfo.pSubpasses       = &subpass;

    if (vkCreateRenderPass(vkDevice, &rpInfo, nullptr, &vkRenderPass) != VK_SUCCESS) {
        fprintf(gFILE, "vkCreateRenderPass() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

// 11) Create Framebuffers
VkResult CreateFramebuffers()
{
    vkFramebuffer_array = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*swapchainImageCount);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageView attachments[] = { swapChainImageView_array[i] };

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = vkRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = attachments;
        fbInfo.width           = vkExtent2D_SwapChain.width;
        fbInfo.height          = vkExtent2D_SwapChain.height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(vkDevice, &fbInfo, nullptr, &vkFramebuffer_array[i]) != VK_SUCCESS) {
            fprintf(gFILE, "vkCreateFramebuffer() failed.\n");
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}

// 12) Create Semaphores + Fences
VkResult CreateSemaphores()
{
    VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vkDevice, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vkDevice, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            fprintf(gFILE, "Failed to create semaphores.\n");
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}

VkResult CreateFences()
{
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(vkDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            fprintf(gFILE, "Failed to create fence.\n");
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}

// ============================================================================
// ADDITION: Create Vertex Buffer
// ============================================================================
static uint32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    for (uint32_t i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

VkResult CreateVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(Vertex) * gTriangleVertices.size();

    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = bufferSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &gVertexBuffer) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create vertex buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Memory requirements
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gVertexBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        fprintf(gFILE, "Could not find suitable memory type for vertex buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &gVertexBufferMemory) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to allocate vertex buffer memory.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    vkBindBufferMemory(vkDevice, gVertexBuffer, gVertexBufferMemory, 0);

    // Copy data
    void* data;
    vkMapMemory(vkDevice, gVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, gTriangleVertices.data(), (size_t)bufferSize);
    vkUnmapMemory(vkDevice, gVertexBufferMemory);

    return VK_SUCCESS;
}

// ============================================================================
// ADDITION: Create Uniform Buffer + Descriptor Set
// ============================================================================
VkResult CreateUniformBuffer()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = bufferSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &gUniformBuffer) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create uniform buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gUniformBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        fprintf(gFILE, "Could not find suitable memory for uniform buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &gUniformBufferMemory) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to allocate memory for uniform buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkBindBufferMemory(vkDevice, gUniformBuffer, gUniformBufferMemory, 0);

    return VK_SUCCESS;
}

VkResult CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding            = 0;
    uboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount    = 1;
    uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(vkDevice, &layoutInfo, nullptr, &gDescriptorSetLayout) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create descriptor set layout.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

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

    if (vkCreateDescriptorPool(vkDevice, &poolInfo, nullptr, &gDescriptorPool) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create descriptor pool.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkResult CreateDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = gDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &gDescriptorSetLayout;

    if (vkAllocateDescriptorSets(vkDevice, &allocInfo, &gDescriptorSet) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to allocate descriptor set.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = gUniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(UniformBufferObject);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet           = gDescriptorSet;
    descriptorWrite.dstBinding       = 0;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount  = 1;
    descriptorWrite.pBufferInfo      = &bufferInfo;

    vkUpdateDescriptorSets(vkDevice, 1, &descriptorWrite, 0, nullptr);

    return VK_SUCCESS;
}

// ============================================================================
// Helpers to load SPIR-V
// ============================================================================
std::vector<char> ReadFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        fprintf(gFILE, "Error: failed to open file %s\n", filename.c_str());
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

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(vkDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create shader module.\n");
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

// ============================================================================
// Create the Graphics Pipeline
// ============================================================================
VkVertexInputBindingDescription GetVertexBindingDescription()
{
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding   = 0;
    bindingDesc.stride    = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDesc;
}

std::array<VkVertexInputAttributeDescription, 2> GetVertexAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 2> attributes{};

    // Position (vec2)
    attributes[0].binding  = 0;
    attributes[0].location = 0;
    attributes[0].format   = VK_FORMAT_R32G32_SFLOAT;  // vec2
    attributes[0].offset   = offsetof(Vertex, pos);

    // Color (vec3)
    attributes[1].binding  = 0;
    attributes[1].location = 1;
    attributes[1].format   = VK_FORMAT_R32G32B32_SFLOAT;  // vec3
    attributes[1].offset   = offsetof(Vertex, color);

    return attributes;
}

VkResult CreateGraphicsPipeline()
{
    // You must have vert_shader.spv and frag_shader.spv in your folder
    auto vertCode = ReadFile("vert_shader.spv");
    auto fragCode = ReadFile("frag_shader.spv");

    VkShaderModule vertModule = CreateShaderModule(vertCode);
    VkShaderModule fragModule = CreateShaderModule(fragCode);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        fprintf(gFILE, "Shader modules not created.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertModule;
    vertStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragModule;
    fragStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

    // Vertex input
    auto bindingDesc    = GetVertexBindingDescription();
    auto attributeDescs = GetVertexAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescs.size();
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescs.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (float)vkExtent2D_SwapChain.width;
    viewport.height   = (float)vkExtent2D_SwapChain.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = vkExtent2D_SwapChain;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Multisample
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blend
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &gDescriptorSetLayout;

    if (vkCreatePipelineLayout(vkDevice, &pipelineLayoutInfo, nullptr, &gPipelineLayout) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create pipeline layout.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Final pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.layout              = gPipelineLayout;
    pipelineInfo.renderPass          = vkRenderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gGraphicsPipeline) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create graphics pipeline.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkDestroyShaderModule(vkDevice, fragModule, nullptr);
    vkDestroyShaderModule(vkDevice, vertModule, nullptr);

    return VK_SUCCESS;
}

// ============================================================================
// buildCommandBuffers() - we draw the triangle here
// ============================================================================
VkResult buildCommandBuffers()
{
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        if (vkResetCommandBuffer(vkCommandBuffer_array[i], 0) != VK_SUCCESS) {
            fprintf(gFILE, "Failed to reset command buffer.\n");
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(vkCommandBuffer_array[i], &beginInfo);

        VkClearValue clearColor{};
        clearColor.color = vkClearColorValue;

        VkRenderPassBeginInfo rpBeginInfo{};
        rpBeginInfo.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBeginInfo.renderPass       = vkRenderPass;
        rpBeginInfo.framebuffer      = vkFramebuffer_array[i];
        rpBeginInfo.renderArea.offset= {0,0};
        rpBeginInfo.renderArea.extent= vkExtent2D_SwapChain;
        rpBeginInfo.clearValueCount  = 1;
        rpBeginInfo.pClearValues     = &clearColor;

        vkCmdBeginRenderPass(vkCommandBuffer_array[i], &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline
        vkCmdBindPipeline(vkCommandBuffer_array[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gGraphicsPipeline);

        // Bind descriptor set (uniform buffer)
        vkCmdBindDescriptorSets(
            vkCommandBuffer_array[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            gPipelineLayout,
            0,
            1,
            &gDescriptorSet,
            0,
            nullptr
        );

        // Bind vertex buffer
        VkBuffer vb[] = { gVertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(vkCommandBuffer_array[i], 0, 1, vb, offsets);

        // Draw 3 vertices
        vkCmdDraw(vkCommandBuffer_array[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(vkCommandBuffer_array[i]);
        vkEndCommandBuffer(vkCommandBuffer_array[i]);
    }
    return VK_SUCCESS;
}

// ============================================================================
// RE-CREATE SWAPCHAIN if window is resized
// ============================================================================
void cleanupSwapChain()
{
    vkDeviceWaitIdle(vkDevice);

    if (vkFramebuffer_array) {
        for (uint32_t i = 0; i < swapchainImageCount; i++) {
            if (vkFramebuffer_array[i]) {
                vkDestroyFramebuffer(vkDevice, vkFramebuffer_array[i], nullptr);
            }
        }
        free(vkFramebuffer_array);
        vkFramebuffer_array = nullptr;
    }
    if (vkRenderPass) {
        vkDestroyRenderPass(vkDevice, vkRenderPass, nullptr);
        vkRenderPass = VK_NULL_HANDLE;
    }

    if (vkCommandBuffer_array) {
        vkFreeCommandBuffers(vkDevice, vkCommandPool, swapchainImageCount, vkCommandBuffer_array);
        free(vkCommandBuffer_array);
        vkCommandBuffer_array = nullptr;
    }
    if (vkCommandPool) {
        vkDestroyCommandPool(vkDevice, vkCommandPool, nullptr);
        vkCommandPool = VK_NULL_HANDLE;
    }
    if (swapChainImageView_array) {
        for (uint32_t i = 0; i < swapchainImageCount; i++) {
            if (swapChainImageView_array[i]) {
                vkDestroyImageView(vkDevice, swapChainImageView_array[i], nullptr);
            }
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
}

VkResult recreateSwapChain()
{
    if (winWidth == 0 || winHeight == 0) {
        return VK_SUCCESS;
    }

    cleanupSwapChain();

    VkResult vkRes;
    vkRes = CreateSwapChain(VK_FALSE);       if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateImagesAndImageViews();     if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateCommandPool();             if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateCommandBuffers();          if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateRenderPass();              if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateFramebuffers();            if (vkRes != VK_SUCCESS) return vkRes;

    // Pipeline depends on swapchain dimensions, so we recreate it:
    if (gGraphicsPipeline) {
        vkDestroyPipeline(vkDevice, gGraphicsPipeline, nullptr);
        gGraphicsPipeline = VK_NULL_HANDLE;
    }
    if (gPipelineLayout) {
        vkDestroyPipelineLayout(vkDevice, gPipelineLayout, nullptr);
        gPipelineLayout = VK_NULL_HANDLE;
    }
    vkRes = CreateGraphicsPipeline();        if (vkRes != VK_SUCCESS) return vkRes;

    vkRes = buildCommandBuffers();           if (vkRes != VK_SUCCESS) return vkRes;

    return VK_SUCCESS;
}

// ============================================================================
// Initialize
// ============================================================================
VkResult initialize(void)
{
    VkResult vkResult = VK_SUCCESS;

    vkResult = CreateVulkanInstance();      if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateSurfaceWin32();        if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = SelectPhysicalDevice();      if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateLogicalDeviceAndQueue(); if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateSwapChain(VK_FALSE);   if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateImagesAndImageViews(); if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateCommandPool();         if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateCommandBuffers();      if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateRenderPass();          if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateFramebuffers();        if (vkResult != VK_SUCCESS) return vkResult;

    // Now create descriptors, buffers, pipeline, etc.
    vkResult = CreateDescriptorSetLayout(); if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateUniformBuffer();       if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateDescriptorPool();      if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateDescriptorSet();       if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateVertexBuffer();        if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateGraphicsPipeline();    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateSemaphores();          if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateFences();              if (vkResult != VK_SUCCESS) return vkResult;

    memset(&vkClearColorValue, 0, sizeof(vkClearColorValue));
    vkClearColorValue.float32[0] = 0.5f; // Red
    vkClearColorValue.float32[1] = 0.5f; // Green
    vkClearColorValue.float32[2] = 0.5f; // Blue
    vkClearColorValue.float32[3] = 1.0f; // Alpha

    // Build the draw commands
    vkResult = buildCommandBuffers();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "buildCommandBuffers() failed.\n");
        return vkResult;
    }

    bInitialized = TRUE;
    return VK_SUCCESS;
}

// ============================================================================
// Render
// ============================================================================
VkResult display(void)
{
    if (!bInitialized) return VK_SUCCESS;

    vkWaitForFences(vkDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(vkDevice, 1, &inFlightFences[currentFrame]);

    VkResult result = vkAcquireNextImageKHR(
        vkDevice, vkSwapchainKHR, UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE,
        &currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return VK_SUCCESS;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(gFILE, "vkAcquireNextImageKHR failed.\n");
        return result;
    }

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &imageAvailableSemaphores[currentFrame];
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &vkCommandBuffer_array[currentImageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &renderFinishedSemaphores[currentFrame];

    vkQueueSubmit(vkQueue, 1, &submitInfo, inFlightFences[currentFrame]);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinishedSemaphores[currentFrame];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &vkSwapchainKHR;
    presentInfo.pImageIndices      = &currentImageIndex;

    result = vkQueuePresentKHR(vkQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        fprintf(gFILE, "vkQueuePresentKHR failed.\n");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

// ============================================================================
// Update -> we update our uniform buffer with an orthographic matrix
// ============================================================================
void UpdateUniformBuffer()
{
    UniformBufferObject ubo{};

    // Simple orthographic projection from -1..1 in X, and -1..1 in Y,
    // adjusted by aspect ratio so the triangle doesn't look stretched.
    float aspect = (float)winWidth / (float)winHeight;
    // left, right, bottom, top, zNear, zFar
    // We'll do -1..1 for X, and -1..1 for Y, but scaled by aspect in Y:
    ubo.proj = glm::ortho(-1.0f, 1.0f, -1.0f/aspect, 1.0f/aspect, -1.0f, 1.0f);

    // If your triangle is upside down, you can flip Y:
    // ubo.proj[1][1] *= -1.0f;  // Sometimes needed in Vulkan.

    // Copy to GPU
    void* data;
    vkMapMemory(vkDevice, gUniformBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(vkDevice, gUniformBufferMemory);
}

void update(void)
{
    UpdateUniformBuffer();
}

// ============================================================================
// Uninitialize
// ============================================================================
void uninitialize(void)
{
    if (gbFullscreen) {
        ToggleFullscreen();
        gbFullscreen = FALSE;
    }

    if (vkDevice) {
        vkDeviceWaitIdle(vkDevice);

        // Cleanup
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (inFlightFences[i]) {
                vkDestroyFence(vkDevice, inFlightFences[i], nullptr);
                inFlightFences[i] = VK_NULL_HANDLE;
            }
            if (renderFinishedSemaphores[i]) {
                vkDestroySemaphore(vkDevice, renderFinishedSemaphores[i], nullptr);
                renderFinishedSemaphores[i] = VK_NULL_HANDLE;
            }
            if (imageAvailableSemaphores[i]) {
                vkDestroySemaphore(vkDevice, imageAvailableSemaphores[i], nullptr);
                imageAvailableSemaphores[i] = VK_NULL_HANDLE;
            }
        }

        cleanupSwapChain();

        // Destroy pipeline
        if (gGraphicsPipeline) {
            vkDestroyPipeline(vkDevice, gGraphicsPipeline, nullptr);
            gGraphicsPipeline = VK_NULL_HANDLE;
        }
        if (gPipelineLayout) {
            vkDestroyPipelineLayout(vkDevice, gPipelineLayout, nullptr);
            gPipelineLayout = VK_NULL_HANDLE;
        }
        if (gDescriptorPool) {
            vkDestroyDescriptorPool(vkDevice, gDescriptorPool, nullptr);
            gDescriptorPool = VK_NULL_HANDLE;
        }
        if (gDescriptorSetLayout) {
            vkDestroyDescriptorSetLayout(vkDevice, gDescriptorSetLayout, nullptr);
            gDescriptorSetLayout = VK_NULL_HANDLE;
        }

        if (gVertexBuffer) {
            vkDestroyBuffer(vkDevice, gVertexBuffer, nullptr);
            gVertexBuffer = VK_NULL_HANDLE;
        }
        if (gVertexBufferMemory) {
            vkFreeMemory(vkDevice, gVertexBufferMemory, nullptr);
            gVertexBufferMemory = VK_NULL_HANDLE;
        }
        if (gUniformBuffer) {
            vkDestroyBuffer(vkDevice, gUniformBuffer, nullptr);
            gUniformBuffer = VK_NULL_HANDLE;
        }
        if (gUniformBufferMemory) {
            vkFreeMemory(vkDevice, gUniformBufferMemory, nullptr);
            gUniformBufferMemory = VK_NULL_HANDLE;
        }

        vkDestroyDevice(vkDevice, nullptr);
        vkDevice = VK_NULL_HANDLE;
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
        fprintf(gFILE, "uninitialize() completed. Exiting...\n");
        fclose(gFILE);
        gFILE = nullptr;
    }
}
