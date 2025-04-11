/****************************************************************************
  Complete Vulkan 3D Pyramid Code with Texture (stone.jpg) and Logging
  ===============================================================
  This version includes detailed logs at each step. Use the provided
  vertex.glsl/fragment.glsl (compiled to SPIR-V) for the shaders.
****************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <fstream>
#include <vector>
#include <array>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

// Minimal linear algebra (vmath)
#include "vmath.h"

// STB for loading images
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Window
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
#define WIN_WIDTH  800
#define WIN_HEIGHT 600
const char* gpszAppName = "Vulkan_3D_Pyramid_Texture_Logging";
HWND  ghwnd              = NULL;
BOOL  gbActive           = FALSE;
DWORD dwStyle            = 0;
WINDOWPLACEMENT wpPrev{ sizeof(WINDOWPLACEMENT) };
BOOL  gbFullscreen       = FALSE;
int   winWidth   = WIN_WIDTH;
int   winHeight  = WIN_HEIGHT;
BOOL  bInitialized = FALSE;

// Logging / validation
FILE* gFILE             = nullptr;
bool  gEnableValidation = true;
static VkDebugUtilsMessengerEXT gDebugUtilsMessenger = VK_NULL_HANDLE;
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    fprintf(gFILE, "VALIDATION LAYER MESSAGE: %s\n", pCallbackData->pMessage);
    fflush(gFILE);
    return VK_FALSE;
}
static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger)
{
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) return func(instance, pCreateInfo, pAllocator, pMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}
static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) func(instance, messenger, pAllocator);
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

VkRenderPass   gRenderPass    = VK_NULL_HANDLE;
VkFramebuffer* gFramebuffers  = nullptr;

// Vertex data
struct Vertex {
    vmath::vec3 pos;
    vmath::vec3 color;
    vmath::vec2 texCoord;
};
static std::vector<Vertex> gPyramidVertices()
{
    // 4 triangular faces, each face -> 3 vertices, with texCoords
    std::vector<Vertex> out;

    // Face 1
    out.push_back({{ 0.0f,  1.0f,  0.0f}, {1.f, 0.f, 0.f}, {0.5f, 1.f}});
    out.push_back({{-1.0f,  0.0f, -1.0f}, {0.f, 1.f, 0.f}, {0.0f, 0.f}});
    out.push_back({{ 1.0f,  0.0f, -1.0f}, {0.f, 0.f, 1.f}, {1.0f, 0.f}});

    // Face 2
    out.push_back({{ 0.0f,  1.0f,  0.0f}, {1.f, 0.f, 0.f}, {0.5f, 1.f}});
    out.push_back({{ 1.0f,  0.0f, -1.0f}, {0.f, 0.f, 1.f}, {0.0f, 0.f}});
    out.push_back({{ 1.0f,  0.0f,  1.0f}, {1.f, 1.f, 0.f}, {1.0f, 0.f}});

    // Face 3
    out.push_back({{ 0.0f,  1.0f,  0.0f}, {1.f, 0.f, 0.f}, {0.5f, 1.f}});
    out.push_back({{ 1.0f,  0.0f,  1.0f}, {1.f, 1.f, 0.f}, {0.0f, 0.f}});
    out.push_back({{-1.0f,  0.0f,  1.0f}, {1.f, 0.f, 1.f}, {1.0f, 0.f}});

    // Face 4
    out.push_back({{ 0.0f,  1.0f,  0.0f}, {1.f, 0.f, 0.f}, {0.5f, 1.f}});
    out.push_back({{-1.0f,  0.0f,  1.0f}, {1.f, 0.f, 1.f}, {0.0f, 0.f}});
    out.push_back({{-1.0f,  0.0f, -1.0f}, {0.f, 1.f, 0.f}, {1.0f, 0.f}});

    return out;
}
static std::vector<Vertex> gFinalVertices = gPyramidVertices();

struct UniformBufferObject {
    vmath::mat4 mvp;
};

VkBuffer       gUniformBuffer       = VK_NULL_HANDLE;
VkDeviceMemory gUniformBufferMemory = VK_NULL_HANDLE;

VkBuffer       gVertexBuffer       = VK_NULL_HANDLE;
VkDeviceMemory gVertexBufferMemory = VK_NULL_HANDLE;

VkDescriptorSetLayout gDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorPool      gDescriptorPool      = VK_NULL_HANDLE;
VkDescriptorSet       gDescriptorSet       = VK_NULL_HANDLE;

VkPipelineLayout gPipelineLayout   = VK_NULL_HANDLE;
VkPipeline       gGraphicsPipeline = VK_NULL_HANDLE;

static VkFormat gDepthFormat = VK_FORMAT_UNDEFINED;
std::vector<VkImage>        gDepthImages;
std::vector<VkDeviceMemory> gDepthImagesMemory;
std::vector<VkImageView>    gDepthImagesView;

// Texture resources
VkImage        gTextureImage       = VK_NULL_HANDLE;
VkDeviceMemory gTextureImageMemory = VK_NULL_HANDLE;
VkImageView    gTextureImageView   = VK_NULL_HANDLE;
VkSampler      gTextureSampler     = VK_NULL_HANDLE;

// Forward-declarations
VkResult initialize(void);
void     uninitialize(void);
VkResult display(void);
void     update(void);
VkResult recreateSwapChain(void);
void     ToggleFullscreen(void);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
    gFILE = fopen("Log.txt", "w");
    if (!gFILE) {
        MessageBox(NULL, TEXT("Cannot open log file."), TEXT("Error"), MB_OK);
        exit(0);
    }
    fprintf(gFILE, "Log file created successfully.\n");

    WNDCLASSEX wndclass{};
    wndclass.cbSize        = sizeof(WNDCLASSEX);
    wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.hInstance     = hInstance;
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpszClassName = gpszAppName;
    fprintf(gFILE, "Registering window class...\n");
    RegisterClassEx(&wndclass);
    fprintf(gFILE, "Window class registered.\n");

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int xCoord  = (screenW - WIN_WIDTH ) / 2;
    int yCoord  = (screenH - WIN_HEIGHT) / 2;

    fprintf(gFILE, "Creating window...\n");
    ghwnd = CreateWindowEx(WS_EX_APPWINDOW,
        gpszAppName,
        TEXT("Vulkan 3D Pyramid Texture Logging"),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
        xCoord, yCoord, WIN_WIDTH, WIN_HEIGHT,
        NULL, NULL, hInstance, NULL);

    if (!ghwnd) {
        fprintf(gFILE, "Failed to create window.\n");
        return 0;
    }
    fprintf(gFILE, "Window created successfully.\n");

    fprintf(gFILE, "Starting initialization...\n");
    if (initialize() != VK_SUCCESS) {
        fprintf(gFILE, "Initialization failed.\n");
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }
    fprintf(gFILE, "Initialization successful.\n");

    ShowWindow(ghwnd, iCmdShow);
    UpdateWindow(ghwnd);
    SetForegroundWindow(ghwnd);
    SetFocus(ghwnd);

    MSG msg{};
    fprintf(gFILE, "Entering message loop.\n");
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
    fprintf(gFILE, "Exiting message loop.\n");
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
                fprintf(gFILE, "Window resized to %d x %d. Recreating swapchain...\n", winWidth, winHeight);
                recreateSwapChain();
            }
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        break;
    case WM_CHAR:
        if (wParam == 'f' || wParam == 'F') {
            if (!gbFullscreen) {
                ToggleFullscreen();
                gbFullscreen = TRUE;
                fprintf(gFILE, "Switched to fullscreen.\n");
            } else {
                ToggleFullscreen();
                gbFullscreen = FALSE;
                fprintf(gFILE, "Switched to windowed mode.\n");
            }
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

// Validation + Instance Layers
static uint32_t enabledInstanceLayerCount = 0;
static const char* enabledInstanceLayerNames_array[1];
static VkResult FillInstanceLayerNames()
{
    fprintf(gFILE, "FillInstanceLayerNames() called.\n");
    if (!gEnableValidation) {
        fprintf(gFILE, "Validation disabled, skipping layer fill.\n");
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
    for (auto& lp : layers) {
        if (strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            enabledInstanceLayerNames_array[0] = "VK_LAYER_KHRONOS_validation";
            enabledInstanceLayerCount          = 1;
            fprintf(gFILE, "Enabling: VK_LAYER_KHRONOS_validation\n");
            break;
        }
    }
    return VK_SUCCESS;
}
static uint32_t enabledInstanceExtensionsCount = 0;
static const char* enabledInstanceExtensionNames_array[3];
static void AddDebugUtilsExtensionIfPresent()
{
    fprintf(gFILE, "Attempting to add debug utils extension.\n");
    if (!gEnableValidation) {
        fprintf(gFILE, "Validation disabled, skipping.\n");
        return;
    }
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    if (extCount == 0) {
        fprintf(gFILE, "No instance extensions found at all.\n");
        return;
    }
    std::vector<VkExtensionProperties> props(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, props.data());
    for (auto& ep : props) {
        if (strcmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
            enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            fprintf(gFILE, "Added extension: VK_EXT_debug_utils\n");
            return;
        }
    }
    fprintf(gFILE, "VK_EXT_debug_utils not found.\n");
}
static VkResult FillInstanceExtensionNames()
{
    fprintf(gFILE, "FillInstanceExtensionNames() called.\n");
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (extensionCount == 0) {
        fprintf(gFILE, "No instance extensions available.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    bool foundSurface = false, foundWin32Surface = false;
    std::vector<VkExtensionProperties> exts(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, exts.data());
    for (auto& e : exts) {
        if (!strcmp(e.extensionName, VK_KHR_SURFACE_EXTENSION_NAME))     foundSurface = true;
        if (!strcmp(e.extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) foundWin32Surface = true;
    }
    if (!foundSurface || !foundWin32Surface) {
        fprintf(gFILE, "Required surface extensions not found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    enabledInstanceExtensionsCount = 0;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
    AddDebugUtilsExtensionIfPresent();
    return VK_SUCCESS;
}

// Create Vulkan instance
static VkResult CreateVulkanInstance()
{
    fprintf(gFILE, "=== CreateVulkanInstance() start ===\n");
    if (FillInstanceExtensionNames() != VK_SUCCESS) {
        fprintf(gFILE, "FillInstanceExtensionNames() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (FillInstanceLayerNames() != VK_SUCCESS) {
        fprintf(gFILE, "FillInstanceLayerNames() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = gpszAppName;
    appInfo.applicationVersion = 1;
    appInfo.pEngineName        = gpszAppName;
    appInfo.engineVersion      = 1;
    appInfo.apiVersion         = VK_API_VERSION_1_4; // or 1.4 if your SDK supports it

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = enabledInstanceExtensionsCount;
    createInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;
    createInfo.enabledLayerCount       = enabledInstanceLayerCount;
    createInfo.ppEnabledLayerNames     = enabledInstanceLayerNames_array;

    fprintf(gFILE, "Creating Vulkan instance...\n");
    if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS) {
        fprintf(gFILE, "vkCreateInstance() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Vulkan instance created.\n");

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
        if (CreateDebugUtilsMessengerEXT(vkInstance, &dbgCreate, nullptr, &gDebugUtilsMessenger) == VK_SUCCESS) {
            fprintf(gFILE, "Debug utils messenger created.\n");
        }
    }
    fprintf(gFILE, "=== CreateVulkanInstance() end ===\n");
    return VK_SUCCESS;
}
static VkResult CreateSurfaceWin32()
{
    fprintf(gFILE, "Creating Win32 surface.\n");
    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    ci.hwnd      = ghwnd;
    if (vkCreateWin32SurfaceKHR(vkInstance, &ci, nullptr, &vkSurfaceKHR) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create Win32 surface.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Win32 surface created.\n");
    return VK_SUCCESS;
}
static VkResult SelectPhysicalDevice()
{
    fprintf(gFILE, "Selecting physical device...\n");
    vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr);
    if (physicalDeviceCount == 0) {
        fprintf(gFILE, "No physical devices found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Physical device count = %u\n", physicalDeviceCount);
    std::vector<VkPhysicalDevice> devices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, devices.data());

    bool found = false;
    for (auto& pd : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);
        fprintf(gFILE, "Checking device: %s\n", props.deviceName);

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
                fprintf(gFILE, "Device selected: %s, queueIndex = %u\n", props.deviceName, i);
                vkPhysicalDevice_sel = pd;
                graphicsQueueIndex   = i;
                found = true;
                break;
            }
        }
        if (found) break;
    }
    if (!found) {
        fprintf(gFILE, "No suitable device found with graphics+present.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_sel, &vkPhysicalDeviceMemoryProperties);
    return VK_SUCCESS;
}
static uint32_t enabledDeviceExtensionsCount = 0;
static const char* enabledDeviceExtensionNames_array[1];
static VkResult FillDeviceExtensionNames()
{
    fprintf(gFILE, "Looking for device extensions (swapchain)...\n");
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel, nullptr, &extCount, nullptr);
    if (extCount == 0) {
        fprintf(gFILE, "No device extensions found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel, nullptr, &extCount, exts.data());
    bool foundSwapchain = false;
    for (auto &e : exts) {
        if (!strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            foundSwapchain = true;
            break;
        }
    }
    if (!foundSwapchain) {
        fprintf(gFILE, "Swapchain extension not present on device.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    enabledDeviceExtensionsCount = 0;
    enabledDeviceExtensionNames_array[enabledDeviceExtensionsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    fprintf(gFILE, "Swapchain extension found.\n");
    return VK_SUCCESS;
}

// ***** THIS FUNCTION CHANGED TO ENABLE ANISOTROPY IF SUPPORTED *****
// ------------------------------------------------------------
static VkResult CreateLogicalDeviceAndQueue()
{
    fprintf(gFILE, "Creating logical device.\n");
    if (FillDeviceExtensionNames() != VK_SUCCESS) {
        fprintf(gFILE, "FillDeviceExtensionNames() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Query which features are supported by this physical device
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(vkPhysicalDevice_sel, &supportedFeatures);

    // We'll enable any features we want:
    VkPhysicalDeviceFeatures features{};
    // Enable samplerAnisotropy if device supports it
    if (supportedFeatures.samplerAnisotropy) {
        features.samplerAnisotropy = VK_TRUE;
        fprintf(gFILE, "Enabling anisotropic filtering feature.\n");
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo dqci{};
    dqci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    dqci.queueFamilyIndex = graphicsQueueIndex;
    dqci.queueCount       = 1;
    dqci.pQueuePriorities = &priority;

    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = 1;
    ci.pQueueCreateInfos       = &dqci;
    ci.enabledExtensionCount   = enabledDeviceExtensionsCount;
    ci.ppEnabledExtensionNames = enabledDeviceExtensionNames_array;
    // Use the features struct we populated above
    ci.pEnabledFeatures        = &features;

    if (vkCreateDevice(vkPhysicalDevice_sel, &ci, nullptr, &vkDevice) != VK_SUCCESS) {
        fprintf(gFILE, "vkCreateDevice() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    vkGetDeviceQueue(vkDevice, graphicsQueueIndex, 0, &vkQueue);
    fprintf(gFILE, "Logical device created.\n");
    return VK_SUCCESS;
}
// ------------------------------------------------------------

static VkResult getSurfaceFormatAndColorSpace()
{
    fprintf(gFILE, "Querying surface formats...\n");
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &count, nullptr);
    if (count == 0) {
        fprintf(gFILE, "No surface formats.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::vector<VkSurfaceFormatKHR> fmts(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &count, fmts.data());
    if (count == 1 && fmts[0].format == VK_FORMAT_UNDEFINED) {
        vkFormat_color  = VK_FORMAT_B8G8R8A8_UNORM;
        vkColorSpaceKHR = fmts[0].colorSpace;
    } else {
        vkFormat_color  = fmts[0].format;
        vkColorSpaceKHR = fmts[0].colorSpace;
    }
    fprintf(gFILE, "Surface format: %d, colorSpace: %d\n", vkFormat_color, vkColorSpaceKHR);
    return VK_SUCCESS;
}
static VkResult getPresentMode()
{
    fprintf(gFILE, "Querying present modes...\n");
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &count, nullptr);
    if (count == 0) {
        fprintf(gFILE, "No present modes.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::vector<VkPresentModeKHR> pms(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &count, pms.data());
    vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
    for (auto pm : pms) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    fprintf(gFILE, "Selected present mode: %d\n", vkPresentModeKHR);
    return VK_SUCCESS;
}

VkResult CreateSwapChain(VkBool32 unused = VK_FALSE)
{
    fprintf(gFILE, "=== CreateSwapChain() start ===\n");
    if (getSurfaceFormatAndColorSpace() != VK_SUCCESS) {
        fprintf(gFILE, "getSurfaceFormatAndColorSpace() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &caps);
    uint32_t desiredCount = caps.minImageCount + 1;
    if ((caps.maxImageCount > 0) && (desiredCount > caps.maxImageCount))
        desiredCount = caps.maxImageCount;
    if (caps.currentExtent.width != UINT32_MAX) {
        vkExtent2D_SwapChain = caps.currentExtent;
    } else {
        vkExtent2D_SwapChain.width = (winWidth < caps.minImageExtent.width) ?
            caps.minImageExtent.width : winWidth;
        if (vkExtent2D_SwapChain.width > caps.maxImageExtent.width)
            vkExtent2D_SwapChain.width = caps.maxImageExtent.width;
        vkExtent2D_SwapChain.height = (winHeight < caps.minImageExtent.height) ?
            caps.minImageExtent.height : winHeight;
        if (vkExtent2D_SwapChain.height > caps.maxImageExtent.height)
            vkExtent2D_SwapChain.height = caps.maxImageExtent.height;
    }
    fprintf(gFILE, "Swapchain extent w=%d, h=%d\n", vkExtent2D_SwapChain.width, vkExtent2D_SwapChain.height);

    if (getPresentMode() != VK_SUCCESS) {
        fprintf(gFILE, "getPresentMode() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

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

    if (vkCreateSwapchainKHR(vkDevice, &ci, nullptr, &vkSwapchainKHR) != VK_SUCCESS) {
        fprintf(gFILE, "vkCreateSwapchainKHR() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Swapchain created.\n");
    fprintf(gFILE, "=== CreateSwapChain() end ===\n");
    return VK_SUCCESS;
}
VkResult CreateImagesAndImageViews()
{
    fprintf(gFILE, "Retrieving swapchain images...\n");
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, nullptr);
    if (swapchainImageCount == 0) {
        fprintf(gFILE, "No swapchain images.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    swapChainImage_array = (VkImage*)malloc(sizeof(VkImage)*swapchainImageCount);
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapChainImage_array);
    fprintf(gFILE, "swapchainImageCount = %u\n", swapchainImageCount);

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
        if (vkCreateImageView(vkDevice, &vi, nullptr, &swapChainImageView_array[i]) != VK_SUCCESS) {
            fprintf(gFILE, "Failed to create swapchain image view %u.\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    fprintf(gFILE, "Swapchain image views created.\n");
    return VK_SUCCESS;
}
VkResult CreateCommandPool()
{
    fprintf(gFILE, "Creating command pool.\n");
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = graphicsQueueIndex;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(vkDevice, &ci, nullptr, &vkCommandPool) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create command pool.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Command pool created.\n");
    return VK_SUCCESS;
}
VkResult CreateCommandBuffers()
{
    fprintf(gFILE, "Allocating command buffers (one per swapchain image).\n");
    vkCommandBuffer_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*swapchainImageCount);
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = vkCommandPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = swapchainImageCount;
    if (vkAllocateCommandBuffers(vkDevice, &ai, vkCommandBuffer_array) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to allocate command buffers.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Command buffers allocated.\n");
    return VK_SUCCESS;
}
VkResult CreateSemaphores()
{
    fprintf(gFILE, "Creating semaphores.\n");
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vkDevice, &sci, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vkDevice, &sci, nullptr, &renderFinishedSemaphores[i])  != VK_SUCCESS) {
            fprintf(gFILE, "Failed to create semaphores.\n");
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    fprintf(gFILE, "Semaphores created.\n");
    return VK_SUCCESS;
}
VkResult CreateFences()
{
    fprintf(gFILE, "Creating fences.\n");
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(vkDevice, &fci, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            fprintf(gFILE, "Failed to create fence %d.\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    fprintf(gFILE, "Fences created.\n");
    return VK_SUCCESS;
}

// Depth
VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (auto &f : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice_sel, f, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return f;
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return f;
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
    fprintf(gFILE, "Creating depth resources.\n");
    gDepthFormat = findDepthFormat();
    if (gDepthFormat == VK_FORMAT_UNDEFINED) {
        fprintf(gFILE, "No suitable depth format found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
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
        vkCreateImage(vkDevice, &ci, nullptr, &gDepthImages[i]);
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
        vkAllocateMemory(vkDevice, &mai, nullptr, &gDepthImagesMemory[i]);
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
        vkCreateImageView(vkDevice, &vi, nullptr, &gDepthImagesView[i]);
    }
    fprintf(gFILE, "Depth resources created.\n");
    return VK_SUCCESS;
}

// Render pass
VkResult CreateRenderPass()
{
    fprintf(gFILE, "Creating render pass.\n");
    VkAttachmentDescription colorAtt{};
    colorAtt.format        = vkFormat_color;
    colorAtt.samples       = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAtt{};
    depthAtt.format        = gDepthFormat;
    depthAtt.samples       = VK_SAMPLE_COUNT_1_BIT;
    depthAtt.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAtt.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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

    if (vkCreateRenderPass(vkDevice, &rpci, nullptr, &gRenderPass) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create render pass.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Render pass created.\n");
    return VK_SUCCESS;
}
VkResult CreateFramebuffers()
{
    fprintf(gFILE, "Creating framebuffers.\n");
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
        if (vkCreateFramebuffer(vkDevice, &fbci, nullptr, &gFramebuffers[i]) != VK_SUCCESS) {
            fprintf(gFILE, "Failed to create framebuffer %u.\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    fprintf(gFILE, "Framebuffers created.\n");
    return VK_SUCCESS;
}

// Buffers
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
    fprintf(gFILE, "Creating vertex buffer.\n");
    VkDeviceSize size = sizeof(Vertex) * gFinalVertices.size();
    VkBufferCreateInfo ci{};
    ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size        = size;
    ci.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vkDevice, &ci, nullptr, &gVertexBuffer) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create vertex buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gVertexBuffer, &memReq);
    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(vkDevice, &mai, nullptr, &gVertexBufferMemory) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to allocate vertex buffer memory.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    vkBindBufferMemory(vkDevice, gVertexBuffer, gVertexBufferMemory, 0);

    void* data;
    vkMapMemory(vkDevice, gVertexBufferMemory, 0, size, 0, &data);
    memcpy(data, gFinalVertices.data(), (size_t)size);
    vkUnmapMemory(vkDevice, gVertexBufferMemory);
    fprintf(gFILE, "Vertex buffer created & data copied.\n");
    return VK_SUCCESS;
}
VkResult CreateUniformBuffer()
{
    fprintf(gFILE, "Creating uniform buffer.\n");
    VkDeviceSize size = sizeof(UniformBufferObject);
    VkBufferCreateInfo ci{};
    ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size        = size;
    ci.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vkDevice, &ci, nullptr, &gUniformBuffer) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create uniform buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gUniformBuffer, &memReq);
    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(vkDevice, &mai, nullptr, &gUniformBufferMemory) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to allocate uniform buffer memory.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    vkBindBufferMemory(vkDevice, gUniformBuffer, gUniformBufferMemory, 0);
    fprintf(gFILE, "Uniform buffer created.\n");
    return VK_SUCCESS;
}

// Texture creation
VkBuffer stagingBuffer = VK_NULL_HANDLE;
VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

void createStagingBuffer(VkDeviceSize size)
{
    fprintf(gFILE, "Creating staging buffer of size %llu.\n", (long long unsigned)size);
    VkBufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size  = size;
    ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(vkDevice, &ci, nullptr, &stagingBuffer);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, stagingBuffer, &memReq);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(vkDevice, &mai, nullptr, &stagingMemory);
    vkBindBufferMemory(vkDevice, stagingBuffer, stagingMemory, 0);
    fprintf(gFILE, "Staging buffer created.\n");
}

VkCommandBuffer beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = vkCommandPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(vkDevice, &ai, &cmd);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}
void endSingleTimeCommands(VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;
    vkQueueSubmit(vkQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(vkQueue);
    vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &cmd);
}
void transitionImageLayout(VkImage img, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    fprintf(gFILE, "Transitioning image layout from %d to %d.\n", oldLayout, newLayout);
    VkCommandBuffer cmd = beginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount   = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount   = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;
    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(cmd);
    fprintf(gFILE, "Image layout transition done.\n");
}
void copyBufferToImage(VkBuffer buf, VkImage img, uint32_t width, uint32_t height)
{
    fprintf(gFILE, "Copying buffer to image (%u x %u).\n", width, height);
    VkCommandBuffer cmd = beginSingleTimeCommands();
    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset = {0,0,0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, buf, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(cmd);
    fprintf(gFILE, "Buffer copied to image.\n");
}

VkResult CreateTextureImage()
{
    fprintf(gFILE, "Creating texture image from stone.jpg.\n");
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("stone.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        fprintf(gFILE, "Failed to load stone.jpg\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "stone.jpg loaded: width=%d, height=%d, channels=%d\n", texWidth, texHeight, texChannels);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    createStagingBuffer(imageSize);
    void* data;
    vkMapMemory(vkDevice, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, (size_t)imageSize);
    vkUnmapMemory(vkDevice, stagingMemory);
    stbi_image_free(pixels);

    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.extent.width  = texWidth;
    ci.extent.height = texHeight;
    ci.extent.depth  = 1;
    ci.mipLevels     = 1;
    ci.arrayLayers   = 1;
    ci.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ci.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(vkDevice, &ci, nullptr, &gTextureImage) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create texture image.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(vkDevice, gTextureImage, &memReq);
    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(vkDevice, &mai, nullptr, &gTextureImageMemory) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to allocate texture image memory.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    vkBindImageMemory(vkDevice, gTextureImage, gTextureImageMemory, 0);

    transitionImageLayout(gTextureImage, VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, gTextureImage, texWidth, texHeight);
    transitionImageLayout(gTextureImage, VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(vkDevice, stagingBuffer, nullptr);
    vkFreeMemory(vkDevice, stagingMemory, nullptr);
    stagingBuffer = VK_NULL_HANDLE;
    stagingMemory = VK_NULL_HANDLE;

    fprintf(gFILE, "Texture image created successfully.\n");
    return VK_SUCCESS;
}
VkResult CreateTextureImageView()
{
    fprintf(gFILE, "Creating texture image view.\n");
    VkImageViewCreateInfo vi{};
    vi.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image                           = gTextureImage;
    vi.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    vi.format                          = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.baseMipLevel   = 0;
    vi.subresourceRange.levelCount     = 1;
    vi.subresourceRange.baseArrayLayer = 0;
    vi.subresourceRange.layerCount     = 1;
    if (vkCreateImageView(vkDevice, &vi, nullptr, &gTextureImageView) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create texture image view.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Texture image view created.\n");
    return VK_SUCCESS;
}
VkResult CreateTextureSampler()
{
    fprintf(gFILE, "Creating texture sampler.\n");
    VkSamplerCreateInfo sci{};
    sci.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter               = VK_FILTER_LINEAR;
    sci.minFilter               = VK_FILTER_LINEAR;
    sci.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // The following lines are valid only if "samplerAnisotropy" was enabled:
    sci.anisotropyEnable        = VK_TRUE;     // We want anisotropic filtering
    sci.maxAnisotropy           = 16.f;        // or device limit
    sci.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sci.unnormalizedCoordinates = VK_FALSE;
    sci.compareEnable           = VK_FALSE;
    sci.compareOp               = VK_COMPARE_OP_ALWAYS;
    sci.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(vkDevice, &sci, nullptr, &gTextureSampler) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create sampler.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Texture sampler created.\n");
    return VK_SUCCESS;
}

// Descriptor set layout
VkResult CreateDescriptorSetLayout()
{
    fprintf(gFILE, "Creating descriptor set layout.\n");
    // 2 bindings: 0=UBO, 1=combined sampler
    VkDescriptorSetLayoutBinding ubo{};
    ubo.binding         = 0;
    ubo.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding         = 1;
    samplerBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding,2> bindings = {ubo, samplerBinding};
    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = (uint32_t)bindings.size();
    ci.pBindings    = bindings.data();
    if (vkCreateDescriptorSetLayout(vkDevice, &ci, nullptr, &gDescriptorSetLayout) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create descriptor set layout.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Descriptor set layout created.\n");
    return VK_SUCCESS;
}
VkResult CreateDescriptorPool()
{
    fprintf(gFILE, "Creating descriptor pool.\n");
    std::array<VkDescriptorPoolSize,2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount = (uint32_t)poolSizes.size();
    ci.pPoolSizes    = poolSizes.data();
    ci.maxSets       = 1;

    if (vkCreateDescriptorPool(vkDevice, &ci, nullptr, &gDescriptorPool) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create descriptor pool.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Descriptor pool created.\n");
    return VK_SUCCESS;
}
VkResult CreateDescriptorSet()
{
    fprintf(gFILE, "Allocating descriptor set.\n");
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = gDescriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &gDescriptorSetLayout;
    if (vkAllocateDescriptorSets(vkDevice, &ai, &gDescriptorSet) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to allocate descriptor set.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Writing to descriptor set.\n");
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = gUniformBuffer;
    uboInfo.offset = 0;
    uboInfo.range  = sizeof(UniformBufferObject);

    VkWriteDescriptorSet w1{};
    w1.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w1.dstSet          = gDescriptorSet;
    w1.dstBinding      = 0;
    w1.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w1.descriptorCount = 1;
    w1.pBufferInfo     = &uboInfo;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = gTextureImageView;
    imageInfo.sampler     = gTextureSampler;

    VkWriteDescriptorSet w2{};
    w2.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w2.dstSet          = gDescriptorSet;
    w2.dstBinding      = 1;
    w2.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w2.descriptorCount = 1;
    w2.pImageInfo      = &imageInfo;

    std::array<VkWriteDescriptorSet, 2> writes = { w1, w2 };
    vkUpdateDescriptorSets(vkDevice, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    fprintf(gFILE, "Descriptor set allocated & updated.\n");
    return VK_SUCCESS;
}

// Shaders
std::vector<char> ReadFile(const std::string& fname)
{
    std::ifstream file(fname, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        fprintf(gFILE, "Could not open file: %s\n", fname.c_str());
        return {};
    }
    size_t sz = (size_t)file.tellg();
    std::vector<char> buf(sz);
    file.seekg(0);
    file.read(buf.data(), sz);
    file.close();
    fprintf(gFILE, "Read file: %s (%llu bytes)\n", fname.c_str(), (long long unsigned)sz);
    return buf;
}
VkShaderModule CreateShaderModule(const std::vector<char>& code)
{
    if (code.empty()) {
        fprintf(gFILE, "Shader code is empty, cannot create module.\n");
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule sm;
    if (vkCreateShaderModule(vkDevice, &ci, nullptr, &sm) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create shader module.\n");
        return VK_NULL_HANDLE;
    }
    fprintf(gFILE, "Shader module created.\n");
    return sm;
}

// Pipeline
VkVertexInputBindingDescription GetVertexBindingDescription()
{
    VkVertexInputBindingDescription bd{};
    bd.binding   = 0;
    bd.stride    = sizeof(Vertex);
    bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bd;
}
std::array<VkVertexInputAttributeDescription,3> GetVertexAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription,3> at{};
    // pos -> location=0
    at[0].binding  = 0;
    at[0].location = 0;
    at[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    at[0].offset   = offsetof(Vertex, pos);

    // color -> location=1
    at[1].binding  = 0;
    at[1].location = 1;
    at[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    at[1].offset   = offsetof(Vertex, color);

    // texCoord -> location=2
    at[2].binding  = 0;
    at[2].location = 2;
    at[2].format   = VK_FORMAT_R32G32_SFLOAT;
    at[2].offset   = offsetof(Vertex, texCoord);

    return at;
}
VkResult CreateGraphicsPipeline()
{
    fprintf(gFILE, "Creating graphics pipeline.\n");
    auto vertCode = ReadFile("vert_shader.spv");
    auto fragCode = ReadFile("frag_shader.spv");
    VkShaderModule vertModule = CreateShaderModule(vertCode);
    VkShaderModule fragModule = CreateShaderModule(fragCode);
    if (!vertModule || !fragModule) {
        fprintf(gFILE, "Error in creating shader modules.\n");
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
    cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                           VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT |
                           VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbAtt;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts    = &gDescriptorSetLayout;
    if (vkCreatePipelineLayout(vkDevice, &plci, nullptr, &gPipelineLayout) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create pipeline layout.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

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

    if (vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pci, nullptr, &gGraphicsPipeline) != VK_SUCCESS) {
        fprintf(gFILE, "Failed to create graphics pipeline.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "Graphics pipeline created.\n");

    vkDestroyShaderModule(vkDevice, fragModule, nullptr);
    vkDestroyShaderModule(vkDevice, vertModule, nullptr);
    return VK_SUCCESS;
}

// Build command buffers
VkResult buildCommandBuffers()
{
    fprintf(gFILE, "Building command buffers.\n");
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        vkResetCommandBuffer(vkCommandBuffer_array[i], 0);
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(vkCommandBuffer_array[i], &bi);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
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
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(vkCommandBuffer_array[i],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

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
        depthBarrier.srcAccessMask = 0;
        depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(vkCommandBuffer_array[i],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0, 0, nullptr, 0, nullptr, 1, &depthBarrier);

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
            VK_PIPELINE_BIND_POINT_GRAPHICS, gPipelineLayout, 0, 1, &gDescriptorSet, 0, nullptr);

        vkCmdDraw(vkCommandBuffer_array[i], (uint32_t)gFinalVertices.size(), 1, 0, 0);

        vkCmdEndRenderPass(vkCommandBuffer_array[i]);

        VkImageMemoryBarrier barrier2{};
        barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier2.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier2.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.image               = swapChainImage_array[i];
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
    fprintf(gFILE, "Command buffers built.\n");
    return VK_SUCCESS;
}

// Cleanup swapchain
void cleanupDepthResources()
{
    fprintf(gFILE, "Cleaning up depth resources...\n");
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
    fprintf(gFILE, "Cleaning up existing swapchain...\n");
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

// Recreate swapchain
VkResult recreateSwapChain()
{
    fprintf(gFILE, "=== recreateSwapChain() called ===\n");
    if (winWidth == 0 || winHeight == 0) {
        fprintf(gFILE, "Window minimized, skipping.\n");
        return VK_SUCCESS;
    }
    cleanupSwapChain();
    if (CreateSwapChain()             != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateImagesAndImageViews()   != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateDepthResources()        != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateRenderPass()            != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateFramebuffers()          != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateCommandPool()           != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateCommandBuffers()        != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    // Recreate pipeline if needed
    if (gGraphicsPipeline)    vkDestroyPipeline(vkDevice, gGraphicsPipeline, nullptr);
    gGraphicsPipeline = VK_NULL_HANDLE;
    if (gPipelineLayout)      vkDestroyPipelineLayout(vkDevice, gPipelineLayout, nullptr);
    gPipelineLayout = VK_NULL_HANDLE;
    if (CreateGraphicsPipeline() != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (buildCommandBuffers()    != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    fprintf(gFILE, "Swapchain recreated successfully.\n");
    return VK_SUCCESS;
}

// initialize
VkResult initialize(void)
{
    fprintf(gFILE, "=== initialize() start ===\n");
    if (CreateVulkanInstance()          != VK_SUCCESS) { fprintf(gFILE, "CreateVulkanInstance() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateSurfaceWin32()            != VK_SUCCESS) { fprintf(gFILE, "CreateSurfaceWin32() failed.\n");   return VK_ERROR_INITIALIZATION_FAILED; }
    if (SelectPhysicalDevice()          != VK_SUCCESS) { fprintf(gFILE, "SelectPhysicalDevice() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateLogicalDeviceAndQueue()   != VK_SUCCESS) { fprintf(gFILE, "CreateLogicalDeviceAndQueue() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateSwapChain()               != VK_SUCCESS) { fprintf(gFILE, "CreateSwapChain() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateImagesAndImageViews()     != VK_SUCCESS) { fprintf(gFILE, "CreateImagesAndImageViews() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateDepthResources()          != VK_SUCCESS) { fprintf(gFILE, "CreateDepthResources() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateRenderPass()              != VK_SUCCESS) { fprintf(gFILE, "CreateRenderPass() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateFramebuffers()            != VK_SUCCESS) { fprintf(gFILE, "CreateFramebuffers() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateCommandPool()             != VK_SUCCESS) { fprintf(gFILE, "CreateCommandPool() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateCommandBuffers()          != VK_SUCCESS) { fprintf(gFILE, "CreateCommandBuffers() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateDescriptorSetLayout()     != VK_SUCCESS) { fprintf(gFILE, "CreateDescriptorSetLayout() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateUniformBuffer()           != VK_SUCCESS) { fprintf(gFILE, "CreateUniformBuffer() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }

    // TEXTURE
    if (CreateTextureImage()            != VK_SUCCESS) { fprintf(gFILE, "CreateTextureImage() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateTextureImageView()        != VK_SUCCESS) { fprintf(gFILE, "CreateTextureImageView() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateTextureSampler()          != VK_SUCCESS) { fprintf(gFILE, "CreateTextureSampler() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }

    if (CreateDescriptorPool()          != VK_SUCCESS) { fprintf(gFILE, "CreateDescriptorPool() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateDescriptorSet()           != VK_SUCCESS) { fprintf(gFILE, "CreateDescriptorSet() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateVertexBuffer()            != VK_SUCCESS) { fprintf(gFILE, "CreateVertexBuffer() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateGraphicsPipeline()        != VK_SUCCESS) { fprintf(gFILE, "CreateGraphicsPipeline() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateSemaphores()              != VK_SUCCESS) { fprintf(gFILE, "CreateSemaphores() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }
    if (CreateFences()                  != VK_SUCCESS) { fprintf(gFILE, "CreateFences() failed.\n"); return VK_ERROR_INITIALIZATION_FAILED; }

    memset(&vkClearColorValue, 0, sizeof(vkClearColorValue));
    vkClearColorValue.float32[0] = 0.2f;
    vkClearColorValue.float32[1] = 0.2f;
    vkClearColorValue.float32[2] = 0.2f;
    vkClearColorValue.float32[3] = 1.0f;

    if (buildCommandBuffers() != VK_SUCCESS) {
        fprintf(gFILE, "buildCommandBuffers() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    bInitialized = TRUE;
    fprintf(gFILE, "=== initialize() end ===\n");
    return VK_SUCCESS;
}

// update: rotate
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
    proj[1][1] *= -1.0f; // Vulkan's Y-flip
    model = vmath::translate(0.0f, 0.0f, -5.0f) * vmath::rotate(angle, 1.0f, 0.0f, 0.0f);
    ubo.mvp = proj * view * model;
    void* data;
    vkMapMemory(vkDevice, gUniformBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(vkDevice, gUniformBufferMemory);
    // Optional log:
    // fprintf(gFILE, "Pyramid angle=%f\n", angle);
}

// display
VkResult display(void)
{
    if (!bInitialized) return VK_SUCCESS;
    vkWaitForFences(vkDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(vkDevice, 1, &inFlightFences[currentFrame]);

    VkResult result = vkAcquireNextImageKHR(vkDevice, vkSwapchainKHR, UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE, &currentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        fprintf(gFILE, "Swapchain out of date in display(), recreating.\n");
        recreateSwapChain();
        return VK_SUCCESS;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(gFILE, "vkAcquireNextImageKHR failed, code = %d\n", result);
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
        fprintf(gFILE, "Swapchain out of date or suboptimal in display(), recreating.\n");
        recreateSwapChain();
    }
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

// uninitialize
void uninitialize(void)
{
    fprintf(gFILE, "=== uninitialize() start ===\n");
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

        if (gTextureSampler)      vkDestroySampler(vkDevice, gTextureSampler, nullptr);
        if (gTextureImageView)    vkDestroyImageView(vkDevice, gTextureImageView, nullptr);
        if (gTextureImage)        vkDestroyImage(vkDevice, gTextureImage, nullptr);
        if (gTextureImageMemory)  vkFreeMemory(vkDevice, gTextureImageMemory, nullptr);

        vkDestroyDevice(vkDevice, nullptr);
        fprintf(gFILE, "Logical device destroyed.\n");
    }
    if (gDebugUtilsMessenger) {
        DestroyDebugUtilsMessengerEXT(vkInstance, gDebugUtilsMessenger, nullptr);
        gDebugUtilsMessenger = VK_NULL_HANDLE;
        fprintf(gFILE, "Debug utils messenger destroyed.\n");
    }
    if (vkSurfaceKHR) {
        vkDestroySurfaceKHR(vkInstance, vkSurfaceKHR, nullptr);
        vkSurfaceKHR = VK_NULL_HANDLE;
        fprintf(gFILE, "Surface destroyed.\n");
    }
    if (vkInstance) {
        vkDestroyInstance(vkInstance, nullptr);
        vkInstance = VK_NULL_HANDLE;
        fprintf(gFILE, "Vulkan instance destroyed.\n");
    }
    if (ghwnd) {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }
    if (gFILE) {
        fprintf(gFILE, "=== uninitialize() end ===\n");
        fclose(gFILE);
        gFILE = nullptr;
    }
}
