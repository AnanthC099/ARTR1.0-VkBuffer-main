// ============================================================================
//  Vk.cpp  (Vulkan sample code rendering a 3D Pyramid with Depth Buffer)
// ============================================================================
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <fstream>
#include <vector>
#include <array>

#include "vmath.h" // using vmath

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

#define WIN_WIDTH  800
#define WIN_HEIGHT 600

const char* gpszAppName = "Vulkan_3D_Pyramid";
HWND  ghwnd = NULL;
BOOL  gbActive = FALSE;
DWORD dwStyle = 0;
WINDOWPLACEMENT wpPrev{ sizeof(WINDOWPLACEMENT) };
BOOL  gbFullscreen = FALSE;

FILE* gFILE = nullptr;
bool gEnableValidation = true; 

static VkDebugUtilsMessengerEXT gDebugUtilsMessenger = VK_NULL_HANDLE;
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData)
{
    fprintf(gFILE, "VALIDATION LAYER: %s\n", pCallbackData->pMessage);
    fflush(gFILE);
    return VK_FALSE;
}
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
    VkInstance              instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
        func(instance, messenger, pAllocator);
}

VkResult initialize(void);
void     uninitialize(void);
VkResult display(void);
void     update(void);
VkResult recreateSwapChain(void);
void     ToggleFullscreen(void);

VkInstance          vkInstance = VK_NULL_HANDLE;
VkSurfaceKHR        vkSurfaceKHR = VK_NULL_HANDLE;
VkPhysicalDevice    vkPhysicalDevice_sel = VK_NULL_HANDLE;
VkDevice            vkDevice = VK_NULL_HANDLE;
VkQueue             vkQueue = VK_NULL_HANDLE;
uint32_t            graphicsQueueIndex = UINT32_MAX;

uint32_t            physicalDeviceCount = 0;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties{};

VkFormat            vkFormat_color = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR     vkColorSpaceKHR = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR    vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;

VkSwapchainKHR      vkSwapchainKHR = VK_NULL_HANDLE;
uint32_t            swapchainImageCount = 0;
VkImage*            swapChainImage_array = nullptr;
VkImageView*        swapChainImageView_array = nullptr;

VkExtent2D          vkExtent2D_SwapChain{ WIN_WIDTH, WIN_HEIGHT };

VkCommandPool       vkCommandPool = VK_NULL_HANDLE;
VkCommandBuffer*    vkCommandBuffer_array = nullptr;

#define MAX_FRAMES_IN_FLIGHT 2
VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
VkFence     inFlightFences[MAX_FRAMES_IN_FLIGHT];
uint32_t    currentFrame = 0;
uint32_t    currentImageIndex = UINT32_MAX;

VkClearColorValue vkClearColorValue{};

int  winWidth  = WIN_WIDTH;
int  winHeight = WIN_HEIGHT;
BOOL bInitialized = FALSE;

VkRenderPass      gRenderPass = VK_NULL_HANDLE;
VkFramebuffer*    gFramebuffers = nullptr;

struct Vertex {
    vmath::vec3 pos;
    vmath::vec3 color;
};
static std::vector<Vertex> gVertices =
{
    // Pyramid with a square base
    // Top vertex
    {{ 0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f, 0.0f }},  // top is red

    // Base corners
    {{-1.0f,  0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }},  // green
    {{ 1.0f,  0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f }},  // blue
    {{ 1.0f,  0.0f,  1.0f }, { 1.0f, 1.0f, 0.0f }},  // yellow
    {{-1.0f,  0.0f,  1.0f }, { 1.0f, 0.0f, 1.0f }},  // magenta
};

// We expand that into triangles (top + base corners):
static std::vector<Vertex> gPyramidVertices()
{
    std::vector<Vertex> out;
    // side1: top(0), v0(1), v1(2)
    out.push_back(gVertices[0]); // top
    out.push_back(gVertices[1]); // v0
    out.push_back(gVertices[2]); // v1

    // side2: top(0), v1(2), v2(3)
    out.push_back(gVertices[0]);
    out.push_back(gVertices[2]);
    out.push_back(gVertices[3]);

    // side3: top(0), v2(3), v3(4)
    out.push_back(gVertices[0]);
    out.push_back(gVertices[3]);
    out.push_back(gVertices[4]);

    // side4: top(0), v3(4), v0(1)
    out.push_back(gVertices[0]);
    out.push_back(gVertices[4]);
    out.push_back(gVertices[1]);

    // No base triangles here, so the bottom is open

    return out;
}

static std::vector<Vertex> gFinalVertices = gPyramidVertices();

struct UniformBufferObject {
    vmath::mat4 mvp;
};

VkBuffer       gUniformBuffer = VK_NULL_HANDLE;
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
    gFILE = fopen("Log.txt", "w");
    if (!gFILE) {
        MessageBox(NULL, TEXT("Cannot open Log file."), TEXT("Error"), MB_OK | MB_ICONERROR);
        exit(0);
    }

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

    ghwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        gpszAppName,
        TEXT("Vulkan 3D Pyramid"),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
        xCoord, yCoord,
        WIN_WIDTH, WIN_HEIGHT,
        NULL, NULL, hInstance, NULL
    );
    if (!ghwnd) {
        return 0;
    }

    VkResult vkResult = initialize();
    if (vkResult != VK_SUCCESS) {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    ShowWindow(ghwnd, iCmdShow);
    UpdateWindow(ghwnd);
    SetForegroundWindow(ghwnd);
    SetFocus(ghwnd);

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
            if (gbActive == TRUE) {
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
            int width  = LOWORD(lParam);
            int height = HIWORD(lParam);
            winWidth  = width;
            winHeight = height;
            if (width > 0 && height > 0) {
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
        SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    }
}

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
    bool foundValidation = false;
    for (auto& lp : layers) {
        if (strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            foundValidation = true;
        }
    }
    if (foundValidation) {
        enabledInstanceLayerNames_array[0] = "VK_LAYER_KHRONOS_validation";
        enabledInstanceLayerCount = 1;
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
    bool foundSurface = false;
    bool foundWin32Surface = false;
    std::vector<VkExtensionProperties> exts(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, exts.data());
    for (auto& e : exts) {
        if (strcmp(e.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) foundSurface = true;
        if (strcmp(e.extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0) foundWin32Surface = true;
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
    if (vkResult != VK_SUCCESS) return vkResult;
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
            gDebugUtilsMessenger = VK_NULL_HANDLE;
        }
    }
    return VK_SUCCESS;
}
static VkResult CreateSurfaceWin32()
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    createInfo.hwnd      = ghwnd;
    VkResult vkResult = vkCreateWin32SurfaceKHR(vkInstance, &createInfo, nullptr, &vkSurfaceKHR);
    return vkResult;
}
static VkResult SelectPhysicalDevice()
{
    VkResult vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr);
    if (vkResult != VK_SUCCESS || physicalDeviceCount == 0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, physicalDevices.data());
    if (vkResult != VK_SUCCESS) return vkResult;
    bool foundSuitableDevice = false;
    for (uint32_t deviceIndex = 0; deviceIndex < physicalDeviceCount; deviceIndex++)
    {
        VkPhysicalDevice pd = physicalDevices[deviceIndex];
        VkPhysicalDeviceProperties deviceProps{};
        vkGetPhysicalDeviceProperties(pd, &deviceProps);
        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfProps(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, qfProps.data());
        std::vector<VkBool32> canPresent(queueCount, VK_FALSE);
        for (uint32_t i = 0; i < queueCount; i++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, vkSurfaceKHR, &canPresent[i]);
        }
        for (uint32_t i = 0; i < queueCount; i++)
        {
            if ((qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (canPresent[i] == VK_TRUE))
            {
                vkPhysicalDevice_sel = pd;
                graphicsQueueIndex   = i;
                foundSuitableDevice  = true;
                break;
            }
        }
        if (foundSuitableDevice) break;
    }
    if (!foundSuitableDevice) return VK_ERROR_INITIALIZATION_FAILED;
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
        if (strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            foundSwapchain = true;
        }
    }
    if (!foundSwapchain) return VK_ERROR_INITIALIZATION_FAILED;
    enabledDeviceExtensionsCount = 0;
    enabledDeviceExtensionNames_array[enabledDeviceExtensionsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    return VK_SUCCESS;
}
static VkResult CreateLogicalDeviceAndQueue()
{
    VkResult vkResult = FillDeviceExtensionNames();
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
    vkResult = vkCreateDevice(vkPhysicalDevice_sel, &createInfo, nullptr, &vkDevice);
    if (vkResult != VK_SUCCESS) return vkResult;
    vkGetDeviceQueue(vkDevice, graphicsQueueIndex, 0, &vkQueue);
    return VK_SUCCESS;
}
static VkResult getSurfaceFormatAndColorSpace()
{
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &formatCount, nullptr);
    if (formatCount == 0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &formatCount, formats.data());
    if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        vkFormat_color  = VK_FORMAT_B8G8R8A8_UNORM;
        vkColorSpaceKHR = formats[0].colorSpace;
    } else {
        vkFormat_color  = formats[0].format;
        vkColorSpaceKHR = formats[0].colorSpace;
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
    vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
    for (auto pm : presentModes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    return VK_SUCCESS;
}
VkResult CreateSwapChain(VkBool32 vsync = VK_FALSE)
{
    VkResult vkResult = getSurfaceFormatAndColorSpace();
    if (vkResult != VK_SUCCESS) return vkResult;
    VkSurfaceCapabilitiesKHR surfaceCaps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &surfaceCaps);
    uint32_t desiredImageCount = surfaceCaps.minImageCount + 1;
    if ((surfaceCaps.maxImageCount > 0) && (desiredImageCount > surfaceCaps.maxImageCount))
        desiredImageCount = surfaceCaps.maxImageCount;
    if (surfaceCaps.currentExtent.width != UINT32_MAX) {
        vkExtent2D_SwapChain = surfaceCaps.currentExtent;
    } else {
        vkExtent2D_SwapChain.width  = std::max(surfaceCaps.minImageExtent.width,
                                        std::min(surfaceCaps.maxImageExtent.width,  (uint32_t)winWidth));
        vkExtent2D_SwapChain.height = std::max(surfaceCaps.minImageExtent.height,
                                        std::min(surfaceCaps.maxImageExtent.height, (uint32_t)winHeight));
    }
    vkResult = getPresentMode();
    if (vkResult != VK_SUCCESS) return vkResult;
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
    return vkResult;
}
VkResult CreateImagesAndImageViews()
{
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, nullptr);
    if (swapchainImageCount == 0) return VK_ERROR_INITIALIZATION_FAILED;
    swapChainImage_array = (VkImage*)malloc(sizeof(VkImage)*swapchainImageCount);
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapChainImage_array);
    swapChainImageView_array = (VkImageView*)malloc(sizeof(VkImageView)*swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = swapChainImage_array[i];
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = vkFormat_color;
        viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(vkDevice, &viewInfo, nullptr, &swapChainImageView_array[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}
VkResult CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueIndex;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &vkCommandPool) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
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
    if (vkAllocateCommandBuffers(vkDevice, &allocInfo, vkCommandBuffer_array) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    return VK_SUCCESS;
}
VkResult CreateSemaphores()
{
    VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vkDevice, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vkDevice, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}
VkResult CreateFences()
{
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(vkDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}
VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                             VkImageTiling tiling,
                             VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice_sel, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    return VK_FORMAT_UNDEFINED;
}
VkFormat findDepthFormat()
{
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}
VkResult CreateDepthResources()
{
    gDepthFormat = findDepthFormat();
    if (gDepthFormat == VK_FORMAT_UNDEFINED)
        return VK_ERROR_INITIALIZATION_FAILED;
    gDepthImages.resize(swapchainImageCount);
    gDepthImagesMemory.resize(swapchainImageCount);
    gDepthImagesView.resize(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = vkExtent2D_SwapChain.width;
        imageInfo.extent.height = vkExtent2D_SwapChain.height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = gDepthFormat;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(vkDevice, &imageInfo, nullptr, &gDepthImages[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(vkDevice, gDepthImages[i], &memReq);
        auto FindMemoryTypeIndex = [&](uint32_t typeFilter, VkMemoryPropertyFlags properties)->uint32_t {
            for (uint32_t j = 0; j < vkPhysicalDeviceMemoryProperties.memoryTypeCount; j++) {
                if ((typeFilter & (1 << j)) &&
                    (vkPhysicalDeviceMemoryProperties.memoryTypes[j].propertyFlags & properties) == properties)
                    return j;
            }
            return UINT32_MAX;
        };
        VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocInfo.allocationSize  = memReq.size;
        allocInfo.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (allocInfo.memoryTypeIndex == UINT32_MAX)
            return VK_ERROR_INITIALIZATION_FAILED;
        if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &gDepthImagesMemory[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
        vkBindImageMemory(vkDevice, gDepthImages[i], gDepthImagesMemory[i], 0);
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = gDepthImages[i];
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = gDepthFormat;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(vkDevice, &viewInfo, nullptr, &gDepthImagesView[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}
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
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = gDepthFormat;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = (uint32_t)attachments.size();
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;
    if (vkCreateRenderPass(vkDevice, &rpInfo, nullptr, &gRenderPass) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    return VK_SUCCESS;
}
VkResult CreateFramebuffers()
{
    gFramebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageView_array[i],
            gDepthImagesView[i]
        };
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = gRenderPass;
        fbInfo.attachmentCount = (uint32_t)attachments.size();
        fbInfo.pAttachments    = attachments.data();
        fbInfo.width           = vkExtent2D_SwapChain.width;
        fbInfo.height          = vkExtent2D_SwapChain.height;
        fbInfo.layers          = 1;
        if (vkCreateFramebuffer(vkDevice, &fbInfo, nullptr, &gFramebuffers[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}
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
    VkDeviceSize bufferSize = sizeof(Vertex) * gFinalVertices.size();
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = bufferSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &gVertexBuffer) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gVertexBuffer, &memReq);
    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeIndex(
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (allocInfo.memoryTypeIndex == UINT32_MAX)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &gVertexBufferMemory) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    vkBindBufferMemory(vkDevice, gVertexBuffer, gVertexBufferMemory, 0);
    void* data;
    vkMapMemory(vkDevice, gVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, gFinalVertices.data(), (size_t)bufferSize);
    vkUnmapMemory(vkDevice, gVertexBufferMemory);
    return VK_SUCCESS;
}
VkResult CreateUniformBuffer()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = bufferSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &gUniformBuffer) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gUniformBuffer, &memReq);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeIndex(
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (allocInfo.memoryTypeIndex == UINT32_MAX)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &gUniformBufferMemory) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
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
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboLayoutBinding;
    if (vkCreateDescriptorSetLayout(vkDevice, &layoutInfo, nullptr, &gDescriptorSetLayout) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
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
    if (vkCreateDescriptorPool(vkDevice, &poolInfo, nullptr, &gDescriptorPool) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    return VK_SUCCESS;
}
VkResult CreateDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = gDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &gDescriptorSetLayout;
    if (vkAllocateDescriptorSets(vkDevice, &allocInfo, &gDescriptorSet) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = gUniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(UniformBufferObject);
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = gDescriptorSet;
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo     = &bufferInfo;
    vkUpdateDescriptorSets(vkDevice, 1, &descriptorWrite, 0, nullptr);
    return VK_SUCCESS;
}
std::vector<char> ReadFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
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
    if (vkCreateShaderModule(vkDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return shaderModule;
}
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
    attributes[0].binding  = 0;
    attributes[0].location = 0;
    attributes[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset   = offsetof(Vertex, pos);
    attributes[1].binding  = 0;
    attributes[1].location = 1;
    attributes[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset   = offsetof(Vertex, color);
    return attributes;
}
VkResult CreateGraphicsPipeline()
{
    auto vertCode = ReadFile("vert_shader.spv");
    auto fragCode = ReadFile("frag_shader.spv");
    VkShaderModule vertModule = CreateShaderModule(vertCode);
    VkShaderModule fragModule = CreateShaderModule(fragCode);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;
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
    auto bindingDesc    = GetVertexBindingDescription();
    auto attributeDescs = GetVertexAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescs.size();
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescs.data();
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (float)vkExtent2D_SwapChain.width;
    viewport.height   = (float)vkExtent2D_SwapChain.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = vkExtent2D_SwapChain;
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode                = VK_CULL_MODE_NONE;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth               = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorBlendAttachment;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &gDescriptorSetLayout;
    if (vkCreatePipelineLayout(vkDevice, &pipelineLayoutInfo, nullptr, &gPipelineLayout) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.layout              = gPipelineLayout;
    pipelineInfo.renderPass          = gRenderPass;
    pipelineInfo.subpass             = 0;
    if (vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gGraphicsPipeline) != VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    vkDestroyShaderModule(vkDevice, fragModule, nullptr);
    vkDestroyShaderModule(vkDevice, vertModule, nullptr);
    return VK_SUCCESS;
}
VkResult buildCommandBuffers()
{
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        vkResetCommandBuffer(vkCommandBuffer_array[i], 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(vkCommandBuffer_array[i], &beginInfo);
        {
            VkImageMemoryBarrier barrier{};
            barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.image                           = swapChainImage_array[i];
            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = 1;
            barrier.srcAccessMask                   = 0;
            barrier.dstAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(
                vkCommandBuffer_array[i],
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        }
        {
            VkImageMemoryBarrier depthBarrier{};
            depthBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
            depthBarrier.newLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.image                           = gDepthImages[i];
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
                0,
                0, nullptr,
                0, nullptr,
                1, &depthBarrier
            );
        }
        VkClearValue clearValues[2];
        clearValues[0].color                = vkClearColorValue;
        clearValues[1].depthStencil.depth   = 1.0f;
        clearValues[1].depthStencil.stencil = 0;
        VkRenderPassBeginInfo rpBegin{};
        rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass        = gRenderPass;
        rpBegin.framebuffer       = gFramebuffers[i];
        rpBegin.renderArea.offset = {0,0};
        rpBegin.renderArea.extent = vkExtent2D_SwapChain;
        rpBegin.clearValueCount   = 2;
        rpBegin.pClearValues      = clearValues;
        vkCmdBeginRenderPass(vkCommandBuffer_array[i], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(vkCommandBuffer_array[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gGraphicsPipeline);
        VkBuffer vb[] = { gVertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(vkCommandBuffer_array[i], 0, 1, vb, offsets);
        vkCmdBindDescriptorSets(
            vkCommandBuffer_array[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            gPipelineLayout,
            0, 1, &gDescriptorSet,
            0, nullptr
        );
        vkCmdDraw(vkCommandBuffer_array[i], (uint32_t)gFinalVertices.size(), 1, 0, 0);
        vkCmdEndRenderPass(vkCommandBuffer_array[i]);
        {
            VkImageMemoryBarrier barrier2{};
            barrier2.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
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
            barrier2.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier2.dstAccessMask                   = 0;
            vkCmdPipelineBarrier(
                vkCommandBuffer_array[i],
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier2
            );
        }
        vkEndCommandBuffer(vkCommandBuffer_array[i]);
    }
    return VK_SUCCESS;
}
void cleanupDepthResources()
{
    for (uint32_t i = 0; i < gDepthImages.size(); i++) {
        if (gDepthImagesView[i])    vkDestroyImageView(vkDevice, gDepthImagesView[i], nullptr);
        if (gDepthImages[i])        vkDestroyImage(vkDevice, gDepthImages[i], nullptr);
        if (gDepthImagesMemory[i])  vkFreeMemory(vkDevice, gDepthImagesMemory[i], nullptr);
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
    VkResult vkRes;
    vkRes = CreateSwapChain(VK_FALSE);         if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateImagesAndImageViews();       if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateDepthResources();            if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateRenderPass();                if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateFramebuffers();              if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateCommandPool();               if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateCommandBuffers();            if (vkRes != VK_SUCCESS) return vkRes;
    if (gGraphicsPipeline) {
        vkDestroyPipeline(vkDevice, gGraphicsPipeline, nullptr);
        gGraphicsPipeline = VK_NULL_HANDLE;
    }
    if (gPipelineLayout) {
        vkDestroyPipelineLayout(vkDevice, gPipelineLayout, nullptr);
        gPipelineLayout = VK_NULL_HANDLE;
    }
    vkRes = CreateGraphicsPipeline();          if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = buildCommandBuffers();             if (vkRes != VK_SUCCESS) return vkRes;
    return VK_SUCCESS;
}
VkResult initialize(void)
{
    VkResult vkResult;
    vkResult = CreateVulkanInstance();         if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateSurfaceWin32();           if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = SelectPhysicalDevice();         if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateLogicalDeviceAndQueue();  if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateSwapChain(VK_FALSE);      if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateImagesAndImageViews();    if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateDepthResources();         if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateRenderPass();             if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateFramebuffers();           if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateCommandPool();            if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateCommandBuffers();         if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateDescriptorSetLayout();    if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateUniformBuffer();          if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateDescriptorPool();         if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateDescriptorSet();          if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateVertexBuffer();           if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateGraphicsPipeline();       if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateSemaphores();             if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateFences();                 if (vkResult != VK_SUCCESS) return vkResult;
    memset(&vkClearColorValue, 0, sizeof(vkClearColorValue));
    vkClearColorValue.float32[0] = 0.2f;
    vkClearColorValue.float32[1] = 0.2f;
    vkClearColorValue.float32[2] = 0.2f;
    vkClearColorValue.float32[3] = 1.0f;
    vkResult = buildCommandBuffers();          if (vkResult != VK_SUCCESS) return vkResult;
    bInitialized = TRUE;
    return VK_SUCCESS;
}
void update(void)
{
    static float angle = 0.0f;
    angle += 0.5f;
    if (angle >= 360.0f) angle -= 360.0f;
    UniformBufferObject ubo{};
    vmath::mat4 model = vmath::mat4::identity();
    vmath::mat4 view  = vmath::mat4::identity();
    float aspect = (winHeight == 0) ? 1.0f : (float)winWidth / (float)winHeight;
    vmath::mat4 proj  = vmath::perspective(45.0f, aspect, 0.1f, 100.0f);
    proj[1][1] *= -1.0f;
    vmath::mat4 T = vmath::translate(0.0f, 0.0f, -5.0f);
    vmath::mat4 R = vmath::rotate(angle, 1.0f, 0.0f, 0.0f);
    model = T * R;
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
    VkResult result = vkAcquireNextImageKHR(
        vkDevice,
        vkSwapchainKHR,
        UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE,
        &currentImageIndex
    );
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return VK_SUCCESS;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
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
    }
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}
void uninitialize(void)
{
    if (gbFullscreen) {
        ToggleFullscreen();
        gbFullscreen = FALSE;
    }
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
