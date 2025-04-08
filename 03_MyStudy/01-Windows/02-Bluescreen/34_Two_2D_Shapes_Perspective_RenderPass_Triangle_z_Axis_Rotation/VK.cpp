/*
    Vulkan_RenderPass_WithLayoutTransitions_RotateTriangle.cpp

    A complete, self-contained example using a standard Vulkan render pass +
    framebuffers, with explicit layout transitions for swapchain images.

    CHANGES for rotating ONLY the left triangle 360 degrees continuously:
    - We replaced std::vector<Vertex> with a fixed-size C array (gVertices[9]).
    - The first 3 elements (indices 0..2) represent the triangle; 
      we update them in RotateTriangleCPU() each frame.
    - The remaining 6 elements (indices 3..8) are the square; we never modify them.
    - A global angle gTriangleAngleDegrees is incremented, and we do the CPU-based rotation
      around the triangle’s centroid each frame.

    CHANGES from the original:
    - Minimally adapted to use C-like arrays and simpler patterns where possible.
    - The rest of the Vulkan logic is unchanged.
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <fstream>
#include <vector> // (still used for enumerating layers, devices, etc.)
#include <array>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#pragma comment(lib, "vulkan-1.lib")

// Global Function Declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

#define WIN_WIDTH  800
#define WIN_HEIGHT 600

// Global Variables
const char* gpszAppName = "Vulkan_RenderPass_LayoutTransitions";

HWND  ghwnd          = NULL;
BOOL  gbActive       = FALSE;
DWORD dwStyle        = 0;
WINDOWPLACEMENT wpPrev{ sizeof(WINDOWPLACEMENT) };
BOOL  gbFullscreen   = FALSE;

FILE* gFILE          = nullptr;
bool gEnableValidation = true;  // set to true if you want validation layers

// Debug messenger
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

// Forward Declarations
VkResult initialize(void);
void     uninitialize(void);
VkResult display(void);
void     update(void);
VkResult recreateSwapChain(void);
void     ToggleFullscreen(void);

// Vulkan Globals
VkInstance          vkInstance            = VK_NULL_HANDLE;
VkSurfaceKHR        vkSurfaceKHR          = VK_NULL_HANDLE;
VkPhysicalDevice    vkPhysicalDevice_sel  = VK_NULL_HANDLE;
VkDevice            vkDevice              = VK_NULL_HANDLE;
VkQueue             vkQueue               = VK_NULL_HANDLE;
uint32_t            graphicsQueueIndex    = UINT32_MAX;

uint32_t            physicalDeviceCount   = 0;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties{};

VkFormat            vkFormat_color        = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR     vkColorSpaceKHR       = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR    vkPresentModeKHR      = VK_PRESENT_MODE_FIFO_KHR;

VkSwapchainKHR      vkSwapchainKHR        = VK_NULL_HANDLE;
uint32_t            swapchainImageCount   = 0;
VkImage*            swapChainImage_array  = nullptr;
VkImageView*        swapChainImageView_array = nullptr;

VkExtent2D          vkExtent2D_SwapChain{ WIN_WIDTH, WIN_HEIGHT };

VkCommandPool       vkCommandPool         = VK_NULL_HANDLE;
VkCommandBuffer*    vkCommandBuffer_array = nullptr;

// Per-frame sync
#define MAX_FRAMES_IN_FLIGHT 2
VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
VkFence     inFlightFences[MAX_FRAMES_IN_FLIGHT];
uint32_t    currentFrame = 0;
uint32_t    currentImageIndex = UINT32_MAX;

// For clearing the screen
VkClearColorValue vkClearColorValue{};

// Window dimension
int  winWidth  = WIN_WIDTH;
int  winHeight = WIN_HEIGHT;
BOOL bInitialized = FALSE;

// === New: Render pass + framebuffers ===
VkRenderPass      gRenderPass = VK_NULL_HANDLE;
VkFramebuffer*    gFramebuffers = nullptr;

// ====================== Vertex + Uniform Data =======================

// A simple struct for vertex data:
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

// We have 9 vertices total: first 3 (indices 0..2) for the LEFT TRIANGLE,
// next 6 (indices 3..8) for the RIGHT SQUARE (2 triangles).
// We'll keep them in a static array for easier C-style updates.
static const int  gVertexCount = 9;
static Vertex     gVertices[9] =
{
    // Left Triangle (indices 0..2)
    { {-0.75f, -0.5f}, {1.0f, 0.0f, 0.0f} },  // Red
    { {-0.25f, -0.5f}, {0.0f, 1.0f, 0.0f} },  // Green
    { {-0.50f,  0.5f}, {0.0f, 0.0f, 1.0f} },  // Blue

    // Right Square (indices 3..8) = 6 vertices
    { { 0.25f, -0.5f}, {1.0f, 0.0f, 0.0f} },  // Yellow
    { { 0.75f, -0.5f}, {0.0f, 1.0f, 0.0f} },  // Yellow
    { { 0.25f,  0.5f}, {0.0f, 0.0f, 1.0f} },  // Yellow

    { { 0.75f, -0.5f}, {1.0f, 0.0f, 0.0f} },  // Yellow
    { { 0.75f,  0.5f}, {0.0f, 1.0f, 0.0f} },  // Yellow
    { { 0.25f,  0.5f}, {0.0f, 0.0f, 1.0f} },  // Yellow
};

// Simple uniform struct for the MVP
struct UniformBufferObject {
    glm::mat4 mvp;
};

VkPipelineLayout      gPipelineLayout      = VK_NULL_HANDLE;
VkPipeline            gGraphicsPipeline    = VK_NULL_HANDLE;
VkDescriptorSetLayout gDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorPool      gDescriptorPool      = VK_NULL_HANDLE;
VkDescriptorSet       gDescriptorSet       = VK_NULL_HANDLE;

VkBuffer       gVertexBuffer        = VK_NULL_HANDLE;
VkDeviceMemory gVertexBufferMemory  = VK_NULL_HANDLE;
VkBuffer       gUniformBuffer       = VK_NULL_HANDLE;
VkDeviceMemory gUniformBufferMemory = VK_NULL_HANDLE;

// --------------------------------------------------------------------
// NEW: data for rotating triangle (first 3 vertices only).
// We'll store the original positions and a global angle in degrees.
// --------------------------------------------------------------------
static glm::vec2 gTrianglePosOriginal[3] = {
    glm::vec2(-0.75f, -0.5f),
    glm::vec2(-0.25f, -0.5f),
    glm::vec2(-0.50f,  0.50f),
};

static float gTriangleAngleDegrees = 0.0f;

// Prototypes of newly added function
static void RotateTriangleCPU(float angleDegrees);

// ============================================================================
// Windows Entry (WinMain)
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
    WNDCLASSEX wndclass;
    memset(&wndclass, 0, sizeof(WNDCLASSEX));
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

    RegisterClassEx(&wndclass);
    fprintf(gFILE, "[LOG] Window class registered.\n");
    fflush(gFILE);

    // Position window
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int xCoord  = (screenW - WIN_WIDTH ) / 2;
    int yCoord  = (screenH - WIN_HEIGHT) / 2;

    ghwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        gpszAppName,
        TEXT("Vulkan Render Pass + Layout Transitions (Rotate Triangle)"),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
        xCoord,
        yCoord,
        WIN_WIDTH,
        WIN_HEIGHT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!ghwnd) {
        fprintf(gFILE, "Cannot create window.\n");
        fflush(gFILE);
        return 0;
    }
    fprintf(gFILE, "[LOG] Window created successfully.\n");
    fflush(gFILE);

    // Initialize Vulkan
    VkResult vkResult = initialize();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize() failed with VkResult = %d.\n", vkResult);
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
    MSG msg;
    memset(&msg, 0, sizeof(MSG));

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
                update();
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
            int width  = LOWORD(lParam);
            int height = HIWORD(lParam);

            winWidth  = width;
            winHeight = height;

            if (width > 0 && height > 0) {
                fprintf(gFILE, "[LOG] WM_SIZE -> Recreating swap chain for new size (%d x %d)\n", width, height);
                fflush(gFILE);
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
                fprintf(gFILE, "[LOG] Entered Fullscreen.\n");
            } else {
                ToggleFullscreen();
                gbFullscreen = FALSE;
                fprintf(gFILE, "[LOG] Exited Fullscreen.\n");
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
    MONITORINFO mi;
    memset(&mi, 0, sizeof(MONITORINFO));
    mi.cbSize = sizeof(MONITORINFO);

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
// Vulkan Initialization
// ============================================================================
static uint32_t enabledInstanceLayerCount = 0;
static const char* enabledInstanceLayerNames_array[1];

static VkResult FillInstanceLayerNames()
{
    fprintf(gFILE, "[LOG] Checking for validation layers...\n");
    if (!gEnableValidation) {
        fprintf(gFILE, "[LOG] Validation disabled. Skipping layer checks.\n");
        return VK_SUCCESS;
    }

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    fprintf(gFILE, "[LOG] Found %u instance layers.\n", layerCount);
    if (layerCount == 0) {
        fprintf(gFILE, "[ERROR] No instance layers found.\n");
        fflush(gFILE);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    bool foundValidation = false;
    for (auto& lp : layers) {
        fprintf(gFILE, "   Available layer: %s\n", lp.layerName);
        if (strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            foundValidation = true;
        }
    }

    if (foundValidation) {
        enabledInstanceLayerNames_array[0] = "VK_LAYER_KHRONOS_validation";
        enabledInstanceLayerCount = 1;
        fprintf(gFILE, "[LOG] Using VK_LAYER_KHRONOS_validation.\n");
    } else {
        fprintf(gFILE, "[WARN] Validation layer not found.\n");
    }

    fflush(gFILE);
    return VK_SUCCESS;
}

static uint32_t enabledInstanceExtensionsCount = 0;
static const char* enabledInstanceExtensionNames_array[3];

static void AddDebugUtilsExtensionIfPresent()
{
    if (!gEnableValidation) return;

    fprintf(gFILE, "[LOG] Checking for VK_EXT_debug_utils extension...\n");

    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    fprintf(gFILE, "[LOG] Found %u instance extensions.\n", extCount);
    if (extCount == 0) return;

    std::vector<VkExtensionProperties> props(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, props.data());

    for (auto& ep : props) {
        if (strcmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
            enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            fprintf(gFILE, "[LOG] Enabling %s extension.\n", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            return;
        }
    }
    fprintf(gFILE, "[WARN] VK_EXT_debug_utils not found.\n");
}

static VkResult FillInstanceExtensionNames()
{
    fprintf(gFILE, "[LOG] Checking for required instance extensions...\n");

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    fprintf(gFILE, "[LOG] Found %u instance extensions available.\n", extensionCount);
    if (extensionCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    bool foundSurface      = false;
    bool foundWin32Surface = false;

    {
        std::vector<VkExtensionProperties> exts(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, exts.data());
        for (auto& e : exts) {
            if (strcmp(e.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0)
                foundSurface = true;
            if (strcmp(e.extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0)
                foundWin32Surface = true;
        }
    }

    if (!foundSurface || !foundWin32Surface) {
        fprintf(gFILE, "[ERROR] Required instance extension(s) not found.\n");
        fflush(gFILE);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    enabledInstanceExtensionsCount = 0;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

    // Optionally add debug utils
    AddDebugUtilsExtensionIfPresent();

    fprintf(gFILE, "[LOG] Final instance extensions:\n");
    for (uint32_t i = 0; i < enabledInstanceExtensionsCount; i++) {
        fprintf(gFILE, "   -> %s\n", enabledInstanceExtensionNames_array[i]);
    }

    return VK_SUCCESS;
}

static VkResult CreateVulkanInstance()
{
    fprintf(gFILE, "[LOG] --- CreateVulkanInstance() ---\n");
    VkResult vkResult;

    vkResult = FillInstanceExtensionNames();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = FillInstanceLayerNames();
    if (vkResult != VK_SUCCESS) return vkResult;

    VkApplicationInfo appInfo;
    memset(&appInfo, 0, sizeof(VkApplicationInfo));
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = gpszAppName;
    appInfo.applicationVersion = 1;
    appInfo.pEngineName        = gpszAppName;
    appInfo.engineVersion      = 1;
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkInstanceCreateInfo));
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = enabledInstanceExtensionsCount;
    createInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;
    createInfo.enabledLayerCount       = enabledInstanceLayerCount;
    createInfo.ppEnabledLayerNames     = enabledInstanceLayerNames_array;

    fprintf(gFILE, "[LOG] Calling vkCreateInstance()...\n");
    vkResult = vkCreateInstance(&createInfo, nullptr, &vkInstance);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] vkCreateInstance() failed\n");
        return vkResult;
    }
    fprintf(gFILE, "[LOG] vkCreateInstance() succeeded.\n");

    // If validation is enabled, create debug messenger
    if (gEnableValidation && (enabledInstanceLayerCount > 0)) {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        memset(&debugCreateInfo, 0, sizeof(debugCreateInfo));
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

        fprintf(gFILE, "[LOG] Creating debug utils messenger...\n");
        if (CreateDebugUtilsMessengerEXT(vkInstance, &debugCreateInfo, nullptr, &gDebugUtilsMessenger) != VK_SUCCESS) {
            fprintf(gFILE, "[WARN] Could not create Debug Utils Messenger.\n");
            gDebugUtilsMessenger = VK_NULL_HANDLE;
        } else {
            fprintf(gFILE, "[LOG] Debug Utils Messenger created.\n");
        }
    }
    fflush(gFILE);
    return VK_SUCCESS;
}

static VkResult CreateSurfaceWin32()
{
    fprintf(gFILE, "[LOG] --- CreateSurfaceWin32() ---\n");
    VkWin32SurfaceCreateInfoKHR createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    createInfo.hwnd      = ghwnd;

    VkResult vkResult = vkCreateWin32SurfaceKHR(vkInstance, &createInfo, nullptr, &vkSurfaceKHR);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] vkCreateWin32SurfaceKHR() failed.\n");
        return vkResult;
    }
    fprintf(gFILE, "[LOG] vkCreateWin32SurfaceKHR() succeeded.\n");
    fflush(gFILE);
    return vkResult;
}

static VkResult SelectPhysicalDevice()
{
    fprintf(gFILE, "[LOG] --- SelectPhysicalDevice() ---\n");

    VkResult vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr);
    if (vkResult != VK_SUCCESS || physicalDeviceCount == 0) {
        fprintf(gFILE, "[ERROR] No physical devices found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    fprintf(gFILE, "[LOG] Found %u physical devices.\n", physicalDeviceCount);
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, physicalDevices.data());
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] vkEnumeratePhysicalDevices failed.\n");
        return vkResult;
    }

    bool found = false;
    for (auto pd : physicalDevices) {
        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfProps(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, qfProps.data());

        std::vector<VkBool32> canPresent(queueCount);
        for (uint32_t i = 0; i < queueCount; i++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, vkSurfaceKHR, &canPresent[i]);
        }

        // Attempt to find a queue that has GRAPHICS + canPresent
        for (uint32_t i = 0; i < queueCount; i++) {
            if ((qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent[i]) {
                vkPhysicalDevice_sel = pd;
                graphicsQueueIndex   = i;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) {
        fprintf(gFILE, "[ERROR] Failed to find a suitable GPU with Graphics+Present.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Get memory properties
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_sel, &vkPhysicalDeviceMemoryProperties);
    fprintf(gFILE, "[LOG] Physical device selected at queue family index = %u.\n", graphicsQueueIndex);
    fflush(gFILE);
    return VK_SUCCESS;
}

static uint32_t enabledDeviceExtensionsCount = 0;
static const char* enabledDeviceExtensionNames_array[1];

static VkResult FillDeviceExtensionNames()
{
    fprintf(gFILE, "[LOG] Checking device extension properties...\n");
    // We need swapchain extension
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel, nullptr, &extCount, nullptr);
    fprintf(gFILE, "[LOG] Found %u device extensions.\n", extCount);

    if (extCount == 0) {
        fprintf(gFILE, "[ERROR] No device extensions found.\n");
        fflush(gFILE);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel, nullptr, &extCount, exts.data());

    bool foundSwapchain = false;
    for (auto &e : exts) {
        // Log each extension
        fprintf(gFILE, "   Device extension: %s\n", e.extensionName);
        if (strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            foundSwapchain = true;
        }
    }

    if (!foundSwapchain) {
        fprintf(gFILE, "[ERROR] Device does not have VK_KHR_swapchain.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    enabledDeviceExtensionsCount = 0;
    enabledDeviceExtensionNames_array[enabledDeviceExtensionsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    fprintf(gFILE, "[LOG] Enabling device extension: VK_KHR_swapchain\n");
    fflush(gFILE);

    return VK_SUCCESS;
}

static VkResult CreateLogicalDeviceAndQueue()
{
    fprintf(gFILE, "[LOG] --- CreateLogicalDeviceAndQueue() ---\n");
    VkResult vkResult;

    vkResult = FillDeviceExtensionNames();
    if (vkResult != VK_SUCCESS) return vkResult;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo;
    memset(&queueInfo, 0, sizeof(queueInfo));
    queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = graphicsQueueIndex;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = 1;
    createInfo.pQueueCreateInfos       = &queueInfo;
    createInfo.enabledExtensionCount   = enabledDeviceExtensionsCount;
    createInfo.ppEnabledExtensionNames = enabledDeviceExtensionNames_array;
    createInfo.enabledLayerCount       = 0; // layers used at instance-level only

    fprintf(gFILE, "[LOG] Calling vkCreateDevice()...\n");
    vkResult = vkCreateDevice(vkPhysicalDevice_sel, &createInfo, nullptr, &vkDevice);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] vkCreateDevice() failed.\n");
        return vkResult;
    }
    fprintf(gFILE, "[LOG] vkCreateDevice() succeeded.\n");

    vkGetDeviceQueue(vkDevice, graphicsQueueIndex, 0, &vkQueue);
    fprintf(gFILE, "[LOG] Retrieved device queue at index 0.\n");
    fflush(gFILE);
    return VK_SUCCESS;
}

static VkResult getSurfaceFormatAndColorSpace()
{
    fprintf(gFILE, "[LOG] Checking surface formats...\n");
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &formatCount, nullptr);
    fprintf(gFILE, "[LOG] Found %u surface formats.\n", formatCount);

    if (formatCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &formatCount, formats.data());

    if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        vkFormat_color   = VK_FORMAT_B8G8R8A8_UNORM;
        vkColorSpaceKHR  = formats[0].colorSpace;
        fprintf(gFILE, "[LOG] Using fallback VK_FORMAT_B8G8R8A8_UNORM.\n");
    } else {
        vkFormat_color   = formats[0].format;
        vkColorSpaceKHR  = formats[0].colorSpace;
        fprintf(gFILE, "[LOG] Using surface format: %d  colorSpace: %d\n",
                (int)vkFormat_color, (int)vkColorSpaceKHR);
    }
    fflush(gFILE);
    return VK_SUCCESS;
}

static VkResult getPresentMode()
{
    fprintf(gFILE, "[LOG] Checking present modes...\n");
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &presentModeCount, nullptr);
    fprintf(gFILE, "[LOG] Found %u present modes.\n", presentModeCount);
    if (presentModeCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &presentModeCount, presentModes.data());

    vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR; // V-Sync by default
    for (auto pm : presentModes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
            fprintf(gFILE, "[LOG] Chosen present mode: MAILBOX.\n");
            return VK_SUCCESS;
        }
    }
    fprintf(gFILE, "[LOG] Chosen present mode: FIFO (default).\n");
    return VK_SUCCESS;
}

VkResult CreateSwapChain(VkBool32 vsync = VK_FALSE)
{
    fprintf(gFILE, "[LOG] --- CreateSwapChain() ---\n");
    VkResult vkResult = getSurfaceFormatAndColorSpace();
    if (vkResult != VK_SUCCESS) return vkResult;

    VkSurfaceCapabilitiesKHR surfaceCaps;
    memset(&surfaceCaps, 0, sizeof(surfaceCaps));
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &surfaceCaps);

    uint32_t desiredImageCount = surfaceCaps.minImageCount + 1;
    if ((surfaceCaps.maxImageCount > 0) && (desiredImageCount > surfaceCaps.maxImageCount))
        desiredImageCount = surfaceCaps.maxImageCount;

    if (surfaceCaps.currentExtent.width != UINT32_MAX) {
        vkExtent2D_SwapChain = surfaceCaps.currentExtent;
    } else {
        vkExtent2D_SwapChain.width  = (uint32_t)winWidth;
        vkExtent2D_SwapChain.height = (uint32_t)winHeight;
        if(vkExtent2D_SwapChain.width < surfaceCaps.minImageExtent.width)
            vkExtent2D_SwapChain.width = surfaceCaps.minImageExtent.width;
        else if(vkExtent2D_SwapChain.width > surfaceCaps.maxImageExtent.width)
            vkExtent2D_SwapChain.width = surfaceCaps.maxImageExtent.width;

        if(vkExtent2D_SwapChain.height < surfaceCaps.minImageExtent.height)
            vkExtent2D_SwapChain.height = surfaceCaps.minImageExtent.height;
        else if(vkExtent2D_SwapChain.height > surfaceCaps.maxImageExtent.height)
            vkExtent2D_SwapChain.height = surfaceCaps.maxImageExtent.height;
    }

    vkResult = getPresentMode();
    if (vkResult != VK_SUCCESS) return vkResult;

    fprintf(gFILE, "[LOG] Creating swapchain with imageCount=%u, size=(%u x %u)\n",
            desiredImageCount, vkExtent2D_SwapChain.width, vkExtent2D_SwapChain.height);

    VkSwapchainCreateInfoKHR swapInfo;
    memset(&swapInfo, 0, sizeof(swapInfo));
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
        fprintf(gFILE, "[ERROR] vkCreateSwapchainKHR() failed.\n");
        return vkResult;
    }
    fprintf(gFILE, "[LOG] vkCreateSwapchainKHR() succeeded.\n");
    fflush(gFILE);
    return VK_SUCCESS;
}

VkResult CreateImagesAndImageViews()
{
    fprintf(gFILE, "[LOG] --- CreateImagesAndImageViews() ---\n");
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, nullptr);
    fprintf(gFILE, "[LOG] swapchainImageCount = %u\n", swapchainImageCount);
    if (swapchainImageCount == 0) {
        fprintf(gFILE, "[ERROR] swapchainImageCount=0.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    swapChainImage_array = (VkImage*)malloc(sizeof(VkImage) * swapchainImageCount);
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapChainImage_array);

    swapChainImageView_array = (VkImageView*)malloc(sizeof(VkImageView) * swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageViewCreateInfo viewInfo;
        memset(&viewInfo, 0, sizeof(viewInfo));
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

        if (vkCreateImageView(vkDevice, &viewInfo, nullptr, &swapChainImageView_array[i]) != VK_SUCCESS) {
            fprintf(gFILE, "[ERROR] Could not create image view for index %u.\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        fprintf(gFILE, "[LOG] Created image view %u.\n", i);
    }
    fflush(gFILE);
    return VK_SUCCESS;
}

VkResult CreateCommandPool()
{
    fprintf(gFILE, "[LOG] --- CreateCommandPool() ---\n");
    VkCommandPoolCreateInfo poolInfo;
    memset(&poolInfo, 0, sizeof(poolInfo));
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueIndex;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &vkCommandPool) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] vkCreateCommandPool() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "[LOG] Command pool created.\n");
    fflush(gFILE);
    return VK_SUCCESS;
}

VkResult CreateCommandBuffers()
{
    fprintf(gFILE, "[LOG] --- CreateCommandBuffers() ---\n");
    vkCommandBuffer_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * swapchainImageCount);

    VkCommandBufferAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = vkCommandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = swapchainImageCount;

    if (vkAllocateCommandBuffers(vkDevice, &allocInfo, vkCommandBuffer_array) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] vkAllocateCommandBuffers() failed.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "[LOG] Command buffers allocated (%u).\n", swapchainImageCount);
    fflush(gFILE);
    return VK_SUCCESS;
}

VkResult CreateSemaphores()
{
    fprintf(gFILE, "[LOG] --- CreateSemaphores() ---\n");
    VkSemaphoreCreateInfo semInfo;
    memset(&semInfo, 0, sizeof(semInfo));
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vkDevice, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vkDevice, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            fprintf(gFILE, "[ERROR] Failed to create semaphores for frame %d.\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        fprintf(gFILE, "[LOG] Semaphores created for frame index=%d.\n", i);
    }
    fflush(gFILE);
    return VK_SUCCESS;
}

VkResult CreateFences()
{
    fprintf(gFILE, "[LOG] --- CreateFences() ---\n");
    VkFenceCreateInfo fenceInfo;
    memset(&fenceInfo, 0, sizeof(fenceInfo));
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(vkDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            fprintf(gFILE, "[ERROR] Failed to create fence for frame %d.\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        fprintf(gFILE, "[LOG] Fence created for frame index=%d (signaled).\n", i);
    }
    fflush(gFILE);
    return VK_SUCCESS;
}

// ============================================================================
// Create a standard Render Pass
// ============================================================================
VkResult CreateRenderPass()
{
    fprintf(gFILE, "[LOG] --- CreateRenderPass() ---\n");

    VkAttachmentDescription colorAttachment;
    memset(&colorAttachment, 0, sizeof(colorAttachment));
    colorAttachment.format         = vkFormat_color;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;   // we will clear
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;  // store result
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef;
    memset(&colorAttachmentRef, 0, sizeof(colorAttachmentRef));
    colorAttachmentRef.attachment = 0; // index 0 in the pAttachments array
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass;
    memset(&subpass, 0, sizeof(subpass));
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;

    VkSubpassDependency dependency;
    memset(&dependency, 0, sizeof(dependency));
    dependency.srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass      = 0;
    dependency.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask   = 0;
    dependency.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo;
    memset(&rpInfo, 0, sizeof(rpInfo));
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount   = 1;
    rpInfo.pAttachments      = &colorAttachment;
    rpInfo.subpassCount      = 1;
    rpInfo.pSubpasses        = &subpass;
    rpInfo.dependencyCount   = 1;
    rpInfo.pDependencies     = &dependency;

    if (vkCreateRenderPass(vkDevice, &rpInfo, nullptr, &gRenderPass) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to create render pass.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "[LOG] Render pass created.\n");
    fflush(gFILE);
    return VK_SUCCESS;
}

// Create one framebuffer for each swapchain image
VkResult CreateFramebuffers()
{
    fprintf(gFILE, "[LOG] --- CreateFramebuffers() ---\n");

    gFramebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * swapchainImageCount);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageView attachments[1] = { swapChainImageView_array[i] };

        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = gRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = attachments;
        fbInfo.width           = vkExtent2D_SwapChain.width;
        fbInfo.height          = vkExtent2D_SwapChain.height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(vkDevice, &fbInfo, nullptr, &gFramebuffers[i]) != VK_SUCCESS) {
            fprintf(gFILE, "[ERROR] Failed to create framebuffer for index %u.\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        fprintf(gFILE, "[LOG] Created framebuffer %u.\n", i);
    }
    fflush(gFILE);
    return VK_SUCCESS;
}

// ============================================================================
// Buffers for vertices & uniforms
// ============================================================================
static uint32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    fprintf(gFILE, "[LOG] FindMemoryTypeIndex() -> typeFilter=0x%X, properties=0x%X\n",
            typeFilter, properties);
    for (uint32_t i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            fprintf(gFILE, "[LOG] Found suitable memory type at index=%u\n", i);
            return i;
        }
    }
    fprintf(gFILE, "[ERROR] FindMemoryTypeIndex: Could not find a suitable memory type.\n");
    fflush(gFILE);
    return UINT32_MAX;
}

VkResult CreateVertexBuffer()
{
    fprintf(gFILE, "[LOG] --- CreateVertexBuffer() ---\n");
    // We have 9 vertices total in gVertices.
    VkDeviceSize bufferSize = sizeof(gVertices);

    VkBufferCreateInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = bufferSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &gVertexBuffer) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to create vertex buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryRequirements memReq;
    memset(&memReq, 0, sizeof(memReq));
    vkGetBufferMemoryRequirements(vkDevice, gVertexBuffer, &memReq);
    fprintf(gFILE, "[LOG] Vertex buffer mem requirements: size=%llu, alignment=%llu, memTypeBits=0x%X\n",
            (unsigned long long)memReq.size, (unsigned long long)memReq.alignment, memReq.memoryTypeBits);

    VkMemoryAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeIndex(
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        fprintf(gFILE, "[ERROR] Could not find suitable memory type for vertex buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &gVertexBufferMemory) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to allocate vertex buffer memory.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkBindBufferMemory(vkDevice, gVertexBuffer, gVertexBufferMemory, 0);

    // Initial copy of vertex data
    void* data;
    vkMapMemory(vkDevice, gVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, gVertices, (size_t)bufferSize);
    vkUnmapMemory(vkDevice, gVertexBufferMemory);

    fprintf(gFILE, "[LOG] Vertex buffer created & data copied (9 vertices).\n");
    fflush(gFILE);
    return VK_SUCCESS;
}

VkResult CreateUniformBuffer()
{
    fprintf(gFILE, "[LOG] --- CreateUniformBuffer() ---\n");
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    VkBufferCreateInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = bufferSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &gUniformBuffer) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to create uniform buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryRequirements memReq;
    memset(&memReq, 0, sizeof(memReq));
    vkGetBufferMemoryRequirements(vkDevice, gUniformBuffer, &memReq);
    fprintf(gFILE, "[LOG] Uniform buffer mem requirements: size=%llu, alignment=%llu, memTypeBits=0x%X\n",
            (unsigned long long)memReq.size, (unsigned long long)memReq.alignment, memReq.memoryTypeBits);

    VkMemoryAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeIndex(
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        fprintf(gFILE, "[ERROR] Could not find suitable memory for uniform buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &gUniformBufferMemory) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to allocate memory for uniform buffer.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkBindBufferMemory(vkDevice, gUniformBuffer, gUniformBufferMemory, 0);

    fprintf(gFILE, "[LOG] Uniform buffer created (size=%llu bytes).\n", (unsigned long long)bufferSize);
    fflush(gFILE);
    return VK_SUCCESS;
}

// Descriptor set layout + pool + set
VkResult CreateDescriptorSetLayout()
{
    fprintf(gFILE, "[LOG] --- CreateDescriptorSetLayout() ---\n");
    VkDescriptorSetLayoutBinding uboLayoutBinding;
    memset(&uboLayoutBinding, 0, sizeof(uboLayoutBinding));
    uboLayoutBinding.binding            = 0;
    uboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount    = 1;
    uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    memset(&layoutInfo, 0, sizeof(layoutInfo));
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(vkDevice, &layoutInfo, nullptr, &gDescriptorSetLayout) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to create descriptor set layout.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "[LOG] Descriptor set layout created.\n");
    fflush(gFILE);
    return VK_SUCCESS;
}

VkResult CreateDescriptorPool()
{
    fprintf(gFILE, "[LOG] --- CreateDescriptorPool() ---\n");
    VkDescriptorPoolSize poolSize;
    memset(&poolSize, 0, sizeof(poolSize));
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo;
    memset(&poolInfo, 0, sizeof(poolInfo));
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(vkDevice, &poolInfo, nullptr, &gDescriptorPool) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to create descriptor pool.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "[LOG] Descriptor pool created.\n");
    fflush(gFILE);
    return VK_SUCCESS;
}

VkResult CreateDescriptorSet()
{
    fprintf(gFILE, "[LOG] --- CreateDescriptorSet() ---\n");
    VkDescriptorSetAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = gDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &gDescriptorSetLayout;

    if (vkAllocateDescriptorSets(vkDevice, &allocInfo, &gDescriptorSet) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to allocate descriptor set.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDescriptorBufferInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.buffer = gUniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(UniformBufferObject);

    VkWriteDescriptorSet descriptorWrite;
    memset(&descriptorWrite, 0, sizeof(descriptorWrite));
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = gDescriptorSet;
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo     = &bufferInfo;

    vkUpdateDescriptorSets(vkDevice, 1, &descriptorWrite, 0, nullptr);
    fprintf(gFILE, "[LOG] Descriptor set updated.\n");
    fflush(gFILE);
    return VK_SUCCESS;
}

// Helpers to load SPIR-V
static std::vector<char> ReadFile(const std::string& filename)
{
    fprintf(gFILE, "[LOG] Reading SPIR-V from file: %s\n", filename.c_str());
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        fprintf(gFILE, "[ERROR] Failed to open file: %s\n", filename.c_str());
        fflush(gFILE);
        return {};
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    fprintf(gFILE, "[LOG] Loaded %zu bytes from %s\n", fileSize, filename.c_str());
    return buffer;
}

VkShaderModule CreateShaderModule(const std::vector<char>& code)
{
    if (code.empty()) {
        fprintf(gFILE, "[ERROR] Shader code is empty.\n");
        fflush(gFILE);
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(vkDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to create shader module.\n");
        fflush(gFILE);
        return VK_NULL_HANDLE;
    }
    fprintf(gFILE, "[LOG] Shader module created (size=%zu bytes).\n", code.size());
    return shaderModule;
}

// Create Graphics Pipeline referencing the Render Pass
VkVertexInputBindingDescription GetVertexBindingDescription()
{
    VkVertexInputBindingDescription bindingDesc;
    memset(&bindingDesc, 0, sizeof(bindingDesc));
    bindingDesc.binding   = 0;
    bindingDesc.stride    = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDesc;
}

static void GetVertexAttributeDescriptions(VkVertexInputAttributeDescription outAttrs[2])
{
    // position
    outAttrs[0].binding  = 0;
    outAttrs[0].location = 0;
    outAttrs[0].format   = VK_FORMAT_R32G32_SFLOAT;
    outAttrs[0].offset   = offsetof(Vertex, pos);

    // color
    outAttrs[1].binding  = 0;
    outAttrs[1].location = 1;
    outAttrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    outAttrs[1].offset   = offsetof(Vertex, color);
}

VkResult CreateGraphicsPipeline()
{
    fprintf(gFILE, "[LOG] --- CreateGraphicsPipeline() ---\n");
    // Load SPIR-V
    auto vertCode = ReadFile("vert_shader.spv");
    auto fragCode = ReadFile("frag_shader.spv");

    VkShaderModule vertModule = CreateShaderModule(vertCode);
    VkShaderModule fragModule = CreateShaderModule(fragCode);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        fprintf(gFILE, "[ERROR] Could not create shader modules.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPipelineShaderStageCreateInfo vertStageInfo;
    memset(&vertStageInfo, 0, sizeof(vertStageInfo));
    vertStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertModule;
    vertStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo;
    memset(&fragStageInfo, 0, sizeof(fragStageInfo));
    fragStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragModule;
    fragStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[2] = { vertStageInfo, fragStageInfo };

    VkVertexInputBindingDescription bindingDesc = GetVertexBindingDescription();
    VkVertexInputAttributeDescription attributeDescs[2];
    GetVertexAttributeDescriptions(attributeDescs);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    memset(&vertexInputInfo, 0, sizeof(vertexInputInfo));
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    memset(&inputAssembly, 0, sizeof(inputAssembly));
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport;
    memset(&viewport, 0, sizeof(viewport));
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (float)vkExtent2D_SwapChain.width;
    viewport.height   = (float)vkExtent2D_SwapChain.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    memset(&scissor, 0, sizeof(scissor));
    scissor.offset    = {0,0};
    scissor.extent    = vkExtent2D_SwapChain;

    VkPipelineViewportStateCreateInfo viewportState;
    memset(&viewportState, 0, sizeof(viewportState));
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer;
    memset(&rasterizer, 0, sizeof(rasterizer));
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling;
    memset(&multisampling, 0, sizeof(multisampling));
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    memset(&colorBlendAttachment, 0, sizeof(colorBlendAttachment));
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending;
    memset(&colorBlending, 0, sizeof(colorBlending));
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &gDescriptorSetLayout;

    if (vkCreatePipelineLayout(vkDevice, &pipelineLayoutInfo, nullptr, &gPipelineLayout) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to create pipeline layout.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "[LOG] Pipeline layout created.\n");

    // Now we reference the render pass we created
    VkGraphicsPipelineCreateInfo pipelineInfo;
    memset(&pipelineInfo, 0, sizeof(pipelineInfo));
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
    pipelineInfo.renderPass          = gRenderPass;
    pipelineInfo.subpass             = 0;

    fprintf(gFILE, "[LOG] Creating graphics pipeline...\n");
    if (vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gGraphicsPipeline) != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] Failed to create graphics pipeline.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fprintf(gFILE, "[LOG] Graphics pipeline created.\n");

    vkDestroyShaderModule(vkDevice, fragModule, nullptr);
    vkDestroyShaderModule(vkDevice, vertModule, nullptr);
    fprintf(gFILE, "[LOG] Destroyed temporary shader modules.\n");

    fflush(gFILE);
    return VK_SUCCESS;
}

// ============================================================================
// buildCommandBuffers() with layout transitions
// ============================================================================
VkResult buildCommandBuffers()
{
    fprintf(gFILE, "[LOG] --- buildCommandBuffers() ---\n");
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        if (vkResetCommandBuffer(vkCommandBuffer_array[i], 0) != VK_SUCCESS) {
            fprintf(gFILE, "[ERROR] Failed to reset command buffer at index %u.\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        VkCommandBufferBeginInfo beginInfo;
        memset(&beginInfo, 0, sizeof(beginInfo));
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(vkCommandBuffer_array[i], &beginInfo);

        // Transition (UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL)
        {
            VkImageMemoryBarrier barrier;
            memset(&barrier, 0, sizeof(barrier));
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

        // Begin render pass
        VkClearValue clearValue;
        memset(&clearValue, 0, sizeof(clearValue));
        clearValue.color = vkClearColorValue;

        VkRenderPassBeginInfo rpBegin;
        memset(&rpBegin, 0, sizeof(rpBegin));
        rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass        = gRenderPass;
        rpBegin.framebuffer       = gFramebuffers[i];
        rpBegin.renderArea.offset = {0,0};
        rpBegin.renderArea.extent = vkExtent2D_SwapChain;
        rpBegin.clearValueCount   = 1;
        rpBegin.pClearValues      = &clearValue;

        vkCmdBeginRenderPass(vkCommandBuffer_array[i], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline + descriptor sets + vertex buffer
        vkCmdBindPipeline(
            vkCommandBuffer_array[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            gGraphicsPipeline
        );

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

        VkBuffer vb[1] = { gVertexBuffer };
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(vkCommandBuffer_array[i], 0, 1, vb, offsets);

        // Draw the left triangle (3 vertices)
        vkCmdDraw(vkCommandBuffer_array[i],
                  3, // vertexCount
                  1, // instanceCount
                  0, // firstVertex
                  0  // firstInstance
        );

        // Draw the right square (6 vertices, starting at index=3)
        vkCmdDraw(vkCommandBuffer_array[i],
                  6, // vertexCount
                  1, // instanceCount
                  3, // firstVertex
                  0  // firstInstance
        );

        vkCmdEndRenderPass(vkCommandBuffer_array[i]);

        // Transition (COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR)
        {
            VkImageMemoryBarrier barrier2;
            memset(&barrier2, 0, sizeof(barrier2));
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
            barrier2.dstAccessMask                   = 0; // For presentation

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
    fprintf(gFILE, "[LOG] Command buffers built with layout transitions.\n");
    fflush(gFILE);
    return VK_SUCCESS;
}

// cleanupSwapChain
void cleanupSwapChain()
{
    fprintf(gFILE, "[LOG] --- cleanupSwapChain() ---\n");
    vkDeviceWaitIdle(vkDevice);

    if (vkCommandBuffer_array) {
        fprintf(gFILE, "[LOG] Freeing command buffers.\n");
        vkFreeCommandBuffers(vkDevice, vkCommandPool, swapchainImageCount, vkCommandBuffer_array);
        free(vkCommandBuffer_array);
        vkCommandBuffer_array = nullptr;
    }
    if (gFramebuffers) {
        fprintf(gFILE, "[LOG] Destroying framebuffers.\n");
        for (uint32_t i = 0; i < swapchainImageCount; i++) {
            if (gFramebuffers[i]) {
                vkDestroyFramebuffer(vkDevice, gFramebuffers[i], nullptr);
            }
        }
        free(gFramebuffers);
        gFramebuffers = nullptr;
    }
    if (vkCommandPool) {
        fprintf(gFILE, "[LOG] Destroying command pool.\n");
        vkDestroyCommandPool(vkDevice, vkCommandPool, nullptr);
        vkCommandPool = VK_NULL_HANDLE;
    }
    if (swapChainImageView_array) {
        fprintf(gFILE, "[LOG] Destroying swapchain image views...\n");
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
        fprintf(gFILE, "[LOG] Destroying swapchain.\n");
        vkDestroySwapchainKHR(vkDevice, vkSwapchainKHR, nullptr);
        vkSwapchainKHR = VK_NULL_HANDLE;
    }
    if (gRenderPass) {
        fprintf(gFILE, "[LOG] Destroying render pass.\n");
        vkDestroyRenderPass(vkDevice, gRenderPass, nullptr);
        gRenderPass = VK_NULL_HANDLE;
    }
    fflush(gFILE);
}

VkResult recreateSwapChain()
{
    fprintf(gFILE, "[LOG] --- recreateSwapChain() ---\n");
    if (winWidth == 0 || winHeight == 0) {
        fprintf(gFILE, "[LOG] Window minimized, skipping swapchain recreation.\n");
        return VK_SUCCESS;
    }

    cleanupSwapChain();

    VkResult vkRes;
    vkRes = CreateSwapChain(VK_FALSE);       if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateImagesAndImageViews();     if (vkRes != VK_SUCCESS) return vkRes;

    // Recreate the render pass and framebuffers for the new swapchain size
    vkRes = CreateRenderPass();              if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateFramebuffers();            if (vkRes != VK_SUCCESS) return vkRes;

    vkRes = CreateCommandPool();             if (vkRes != VK_SUCCESS) return vkRes;
    vkRes = CreateCommandBuffers();          if (vkRes != VK_SUCCESS) return vkRes;

    if (gGraphicsPipeline) {
        fprintf(gFILE, "[LOG] Destroying old graphics pipeline.\n");
        vkDestroyPipeline(vkDevice, gGraphicsPipeline, nullptr);
        gGraphicsPipeline = VK_NULL_HANDLE;
    }
    if (gPipelineLayout) {
        fprintf(gFILE, "[LOG] Destroying old pipeline layout.\n");
        vkDestroyPipelineLayout(vkDevice, gPipelineLayout, nullptr);
        gPipelineLayout = VK_NULL_HANDLE;
    }
    vkRes = CreateGraphicsPipeline();        if (vkRes != VK_SUCCESS) return vkRes;

    vkRes = buildCommandBuffers();           if (vkRes != VK_SUCCESS) return vkRes;

    return VK_SUCCESS;
}

VkResult initialize(void)
{
    fprintf(gFILE, "[LOG] === Entering initialize() ===\n");
    fflush(gFILE);

    VkResult vkResult = VK_SUCCESS;

    vkResult = CreateVulkanInstance();         if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateSurfaceWin32();           if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = SelectPhysicalDevice();         if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateLogicalDeviceAndQueue();  if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateSwapChain(VK_FALSE);      if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateImagesAndImageViews();    if (vkResult != VK_SUCCESS) return vkResult;

    // Create a standard render pass & framebuffers:
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
    vkClearColorValue.float32[0] = 0.5f;
    vkClearColorValue.float32[1] = 0.5f;
    vkClearColorValue.float32[2] = 0.5f;
    vkClearColorValue.float32[3] = 1.0f;
    fprintf(gFILE, "[LOG] Clear color set to (0.5, 0.5, 0.5, 1.0).\n");

    vkResult = buildCommandBuffers();          if (vkResult != VK_SUCCESS) return vkResult;

    bInitialized = TRUE;
    fprintf(gFILE, "[LOG] === initialize() completed ===\n");
    fflush(gFILE);
    return VK_SUCCESS;
}

// display()
VkResult display(void)
{
    if (!bInitialized) return VK_SUCCESS;

    // Wait on the fence for the current frame
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
        fprintf(gFILE, "[LOG] vkAcquireNextImageKHR -> VK_ERROR_OUT_OF_DATE_KHR, calling recreateSwapChain()\n");
        recreateSwapChain();
        return VK_SUCCESS;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(gFILE, "[ERROR] vkAcquireNextImageKHR failed: %d.\n", result);
        return result;
    }

    VkPipelineStageFlags waitStages[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &imageAvailableSemaphores[currentFrame];
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &vkCommandBuffer_array[currentImageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &renderFinishedSemaphores[currentFrame];

    // Submit the queue
    vkQueueSubmit(vkQueue, 1, &submitInfo, inFlightFences[currentFrame]);

    VkPresentInfoKHR presentInfo;
    memset(&presentInfo, 0, sizeof(presentInfo));
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinishedSemaphores[currentFrame];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &vkSwapchainKHR;
    presentInfo.pImageIndices      = &currentImageIndex;

    result = vkQueuePresentKHR(vkQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        fprintf(gFILE, "[LOG] vkQueuePresentKHR -> OutOfDate/Suboptimal, calling recreateSwapChain()\n");
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] vkQueuePresentKHR failed: %d.\n", result);
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

// --------------------------------------------------------------------
// Update the CPU-based rotation for the triangle & then the MVP uniform
// --------------------------------------------------------------------
static void RotateTriangleCPU(float angleDegrees)
{
    float angleRad = glm::radians(angleDegrees);

    // 1. Compute centroid (average x,y)
    float cx = 0.0f, cy = 0.0f;
    for(int i = 0; i < 3; i++) {
        cx += gTrianglePosOriginal[i].x;
        cy += gTrianglePosOriginal[i].y;
    }
    cx /= 3.0f;
    cy /= 3.0f;

    // 2. For each of the 3 triangle vertices, compute rotated position about centroid
    for(int i = 0; i < 3; i++)
    {
        float x  = gTrianglePosOriginal[i].x;
        float y  = gTrianglePosOriginal[i].y;
        float dx = x - cx;
        float dy = y - cy;

        // 2D rotation
        float rx = dx * cosf(angleRad) - dy * sinf(angleRad);
        float ry = dx * sinf(angleRad) + dy * cosf(angleRad);

        // Overwrite the actual gVertices array
        gVertices[i].pos.x = cx + rx;
        gVertices[i].pos.y = cy + ry;
    }

    // 3. Update GPU vertex buffer with the entire gVertices array (9 vertices)
    VkDeviceSize bufferSize = sizeof(gVertices);

    void* mappedPtr = NULL;
    vkMapMemory(vkDevice, gVertexBufferMemory, 0, bufferSize, 0, &mappedPtr);
    memcpy(mappedPtr, gVertices, (size_t)bufferSize);
    vkUnmapMemory(vkDevice, gVertexBufferMemory);
}

// UpdateUniformBuffer
void UpdateUniformBuffer()
{
    UniformBufferObject ubo;
    memset(&ubo, 0, sizeof(ubo));

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view  = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));

    float aspect    = (float)winWidth / (float)winHeight;
    glm::mat4 proj  = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // Reintroduce Y-flip typical for Vulkan
    proj[1][1] *= -1.0f;

    ubo.mvp = proj * view * model;

    void* data;
    vkMapMemory(vkDevice, gUniformBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(vkDevice, gUniformBufferMemory);
}

// update()
void update(void)
{
    // 1. Update rotation angle: 0 -> 360 degrees continuously
    gTriangleAngleDegrees += 1.0f;  // speed in degrees/frame
    if(gTriangleAngleDegrees >= 360.0f)
        gTriangleAngleDegrees -= 360.0f;

    // 2. Rotate only the left triangle in CPU memory
    RotateTriangleCPU(gTriangleAngleDegrees);

    // 3. Update the uniform buffer (MVP) used by both shapes
    UpdateUniformBuffer();
}

// uninitialize()
void uninitialize(void)
{
    fprintf(gFILE, "[LOG] Entering uninitialize()...\n");
    fflush(gFILE);

    if (gbFullscreen) {
        ToggleFullscreen();
        gbFullscreen = FALSE;
    }

    if (vkDevice) {
        fprintf(gFILE, "[LOG] Device wait idle...\n");
        vkDeviceWaitIdle(vkDevice);

        // Destroy sync objects
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (inFlightFences[i]) {
                fprintf(gFILE, "[LOG] Destroying fence (frame %d).\n", i);
                vkDestroyFence(vkDevice, inFlightFences[i], nullptr);
                inFlightFences[i] = VK_NULL_HANDLE;
            }
            if (renderFinishedSemaphores[i]) {
                fprintf(gFILE, "[LOG] Destroying renderFinishedSemaphore (frame %d).\n", i);
                vkDestroySemaphore(vkDevice, renderFinishedSemaphores[i], nullptr);
                renderFinishedSemaphores[i] = VK_NULL_HANDLE;
            }
            if (imageAvailableSemaphores[i]) {
                fprintf(gFILE, "[LOG] Destroying imageAvailableSemaphore (frame %d).\n", i);
                vkDestroySemaphore(vkDevice, imageAvailableSemaphores[i], nullptr);
                imageAvailableSemaphores[i] = VK_NULL_HANDLE;
            }
        }

        // Cleanup swapchain resources
        cleanupSwapChain();

        if (gGraphicsPipeline) {
            fprintf(gFILE, "[LOG] Destroying graphics pipeline.\n");
            vkDestroyPipeline(vkDevice, gGraphicsPipeline, nullptr);
            gGraphicsPipeline = VK_NULL_HANDLE;
        }
        if (gPipelineLayout) {
            fprintf(gFILE, "[LOG] Destroying pipeline layout.\n");
            vkDestroyPipelineLayout(vkDevice, gPipelineLayout, nullptr);
            gPipelineLayout = VK_NULL_HANDLE;
        }
        if (gDescriptorPool) {
            fprintf(gFILE, "[LOG] Destroying descriptor pool.\n");
            vkDestroyDescriptorPool(vkDevice, gDescriptorPool, nullptr);
            gDescriptorPool = VK_NULL_HANDLE;
        }
        if (gDescriptorSetLayout) {
            fprintf(gFILE, "[LOG] Destroying descriptor set layout.\n");
            vkDestroyDescriptorSetLayout(vkDevice, gDescriptorSetLayout, nullptr);
            gDescriptorSetLayout = VK_NULL_HANDLE;
        }
        if (gVertexBuffer) {
            fprintf(gFILE, "[LOG] Destroying vertex buffer.\n");
            vkDestroyBuffer(vkDevice, gVertexBuffer, nullptr);
            gVertexBuffer = VK_NULL_HANDLE;
        }
        if (gVertexBufferMemory) {
            fprintf(gFILE, "[LOG] Freeing vertex buffer memory.\n");
            vkFreeMemory(vkDevice, gVertexBufferMemory, nullptr);
            gVertexBufferMemory = VK_NULL_HANDLE;
        }
        if (gUniformBuffer) {
            fprintf(gFILE, "[LOG] Destroying uniform buffer.\n");
            vkDestroyBuffer(vkDevice, gUniformBuffer, nullptr);
            gUniformBuffer = VK_NULL_HANDLE;
        }
        if (gUniformBufferMemory) {
            fprintf(gFILE, "[LOG] Freeing uniform buffer memory.\n");
            vkFreeMemory(vkDevice, gUniformBufferMemory, nullptr);
            gUniformBufferMemory = VK_NULL_HANDLE;
        }

        fprintf(gFILE, "[LOG] Destroying logical device.\n");
        vkDestroyDevice(vkDevice, nullptr);
        vkDevice = VK_NULL_HANDLE;
    }

    if (gDebugUtilsMessenger) {
        fprintf(gFILE, "[LOG] Destroying debug utils messenger.\n");
        DestroyDebugUtilsMessengerEXT(vkInstance, gDebugUtilsMessenger, nullptr);
        gDebugUtilsMessenger = VK_NULL_HANDLE;
    }
    if (vkSurfaceKHR) {
        fprintf(gFILE, "[LOG] Destroying surface.\n");
        vkDestroySurfaceKHR(vkInstance, vkSurfaceKHR, nullptr);
        vkSurfaceKHR = VK_NULL_HANDLE;
    }
    if (vkInstance) {
        fprintf(gFILE, "[LOG] Destroying instance.\n");
        vkDestroyInstance(vkInstance, nullptr);
        vkInstance = VK_NULL_HANDLE;
    }

    if (ghwnd) {
        fprintf(gFILE, "[LOG] Destroying window.\n");
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    if (gFILE) {
        fprintf(gFILE, "uninitialize() completed. Exiting...\n");
        fclose(gFILE);
        gFILE = nullptr;
    }
}
