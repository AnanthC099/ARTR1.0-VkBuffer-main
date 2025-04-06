#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#include "VK.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define WIN_WIDTH 800
#define WIN_HEIGHT 600

#pragma comment(lib, "vulkan-1.lib")

// Global Function Declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

const char* gpszAppName = "ARTR";

HWND ghwnd = NULL;
BOOL gbActive = FALSE;
DWORD dwStyle = 0;
WINDOWPLACEMENT wpPrev;
BOOL gbFullscreen = FALSE;

FILE* gFILE = NULL;

// ***** NEW / CHANGED FOR OPTIONAL VALIDATION *****
// Global flag to enable/disable validation layers + debug messenger.
bool gEnableValidation = false; // Set to 'false' to disable validation layers/messenger.

// --- VALIDATION DEBUG MESSENGER --- //
static VkDebugUtilsMessengerEXT gDebugUtilsMessenger = VK_NULL_HANDLE;
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    fprintf(gFILE, "VALIDATION: %s\n", pCallbackData->pMessage);
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
    if (func != NULL)
        return func(instance, pCreateInfo, pAllocator, pMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL)
        func(instance, messenger, pAllocator);
}

// --- INSTANCE EXTENSIONS and Layers --- //
uint32_t enabledInstanceExtensionsCount = 0;
const char* enabledInstanceExtensionNames_array[4];

static uint32_t enabledInstanceLayerCount = 0;
static const char* enabledInstanceLayerNames_array[1];

VkInstance vkInstance = VK_NULL_HANDLE;
VkSurfaceKHR vkSurfaceKHR = VK_NULL_HANDLE;

VkPhysicalDevice vkPhysicalDevice_selected = VK_NULL_HANDLE;
uint32_t graphicsQuequeFamilyIndex_selected = UINT32_MAX;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;

uint32_t physicalDeviceCount = 0;
VkPhysicalDevice *vkPhysicalDevice_array = NULL;

uint32_t enabledDeviceExtensionsCount = 0;
const char* enabledDeviceExtensionNames_array[1];

VkDevice vkDevice = VK_NULL_HANDLE;
VkQueue vkQueue = VK_NULL_HANDLE;

VkFormat vkFormat_color = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR vkColorSpaceKHR = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;

int winWidth = WIN_WIDTH;
int winHeight = WIN_HEIGHT;

VkSwapchainKHR vkSwapchainKHR = VK_NULL_HANDLE;
VkExtent2D vkExtent2D_SwapChain;

uint32_t swapchainImageCount = UINT32_MAX;
VkImage *swapChainImage_array = NULL;
VkImageView *swapChainImageView_array = NULL;

VkCommandPool vkCommandPool = VK_NULL_HANDLE;
VkCommandBuffer *vkCommandBuffer_array = NULL;
VkRenderPass vkRenderPass = VK_NULL_HANDLE;
VkFramebuffer *vkFramebuffer_array = NULL;

// --- PER-FRAME SYNCHRONIZATION --- //
#define MAX_FRAMES_IN_FLIGHT 2
VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
uint32_t currentFrame = 0;

VkClearColorValue vkClearColorValue;
BOOL bInitialized = FALSE;
uint32_t currentImageIndex = UINT32_MAX;

// Forward Declarations
VkResult initialize(void);
void uninitialize(void);
VkResult display(void);
void update(void);

// *** NEW/CHANGED FOR RESIZE ***
// Function to clean up swapchain-dependent objects (but not the device itself).
static void cleanupSwapChain(void)
{
    // Wait until no pending operations
    vkDeviceWaitIdle(vkDevice);

    // Destroy Framebuffers
    if (vkFramebuffer_array)
    {
        for (uint32_t i = 0; i < swapchainImageCount; i++)
        {
            if (vkFramebuffer_array[i] != VK_NULL_HANDLE)
                vkDestroyFramebuffer(vkDevice, vkFramebuffer_array[i], NULL);
        }
        free(vkFramebuffer_array);
        vkFramebuffer_array = NULL;
    }

    // Destroy Render Pass
    if (vkRenderPass)
    {
        vkDestroyRenderPass(vkDevice, vkRenderPass, NULL);
        vkRenderPass = VK_NULL_HANDLE;
    }

    // Destroy Command Buffers
    if (vkCommandBuffer_array)
    {
        for (uint32_t i = 0; i < swapchainImageCount; i++)
        {
            vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &vkCommandBuffer_array[i]);
        }
        free(vkCommandBuffer_array);
        vkCommandBuffer_array = NULL;
    }

    // Destroy Command Pool
    if (vkCommandPool)
    {
        vkDestroyCommandPool(vkDevice, vkCommandPool, NULL);
        vkCommandPool = VK_NULL_HANDLE;
    }

    // Destroy Image Views
    if (swapChainImageView_array)
    {
        for (uint32_t i = 0; i < swapchainImageCount; i++)
        {
            if (swapChainImageView_array[i] != VK_NULL_HANDLE)
                vkDestroyImageView(vkDevice, swapChainImageView_array[i], NULL);
        }
        free(swapChainImageView_array);
        swapChainImageView_array = NULL;
    }

    // Free swapchain images array
    if (swapChainImage_array)
    {
        free(swapChainImage_array);
        swapChainImage_array = NULL;
    }

    // Destroy Swapchain
    if (vkSwapchainKHR)
    {
        vkDestroySwapchainKHR(vkDevice, vkSwapchainKHR, NULL);
        vkSwapchainKHR = VK_NULL_HANDLE;
    }
}

// *** NEW/CHANGED FOR RESIZE ***
// We re-create all swapchain resources here:
static VkResult recreateSwapChain(void)
{
    VkResult vkResult = VK_SUCCESS;

    // In case window is minimized:
    if (winWidth == 0 || winHeight == 0)
        return VK_SUCCESS;

    // Cleanup old swapchain
    cleanupSwapChain();

    // Create swap chain again
    // (We need the command pool again, so re-create in the same order as in initialize)
    
    vkResult = CreateSwapChain(VK_FALSE);
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateImagesAndImageViews();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateCommandPool();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateCommandBuffers();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateRenderPass();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateFramebuffers();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = buildCommandBuffers();
    if (vkResult != VK_SUCCESS) return vkResult;

    return VK_SUCCESS;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
    WNDCLASSEX wndclass;
    HWND hwnd;
    MSG msg;
    TCHAR szAppName[256];
    int iResult = 0;

    int SW = GetSystemMetrics(SM_CXSCREEN);
    int SH = GetSystemMetrics(SM_CYSCREEN);
    int xCoordinate = ((SW / 2) - (WIN_WIDTH / 2));
    int yCoordinate = ((SH / 2) - (WIN_HEIGHT / 2));

    BOOL bDone = FALSE;
    VkResult vkResult = VK_SUCCESS;

    gFILE = fopen("Log.txt", "w");
    if (!gFILE)
    {
        MessageBox(NULL, TEXT("Program cannot open log file!"), TEXT("Error"), MB_OK | MB_ICONERROR);
        exit(0);
    }
    else
    {
        fprintf(gFILE, "WinMain()-> Program started successfully\n");
    }

    wsprintf(szAppName, TEXT("%s"), gpszAppName);

    wndclass.cbSize = sizeof(WNDCLASSEX);
    wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.lpfnWndProc = WndProc;
    wndclass.hInstance = hInstance;
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpszClassName = szAppName;
    wndclass.lpszMenuName = NULL;
    wndclass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));

    RegisterClassEx(&wndclass);

    hwnd = CreateWindowEx(WS_EX_APPWINDOW,
                          szAppName,
                          TEXT("05_PhysicalDevice"),
                          WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
                          xCoordinate,
                          yCoordinate,
                          WIN_WIDTH,
                          WIN_HEIGHT,
                          NULL,
                          NULL,
                          hInstance,
                          NULL);

    ghwnd = hwnd;

    vkResult = initialize();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "WinMain(): initialize()  function failed\n");
        DestroyWindow(hwnd);
        hwnd = NULL;
    }
    else
    {
        fprintf(gFILE, "WinMain(): initialize() succedded\n");
    }

    ShowWindow(hwnd, iCmdShow);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    while (bDone == FALSE)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                bDone = TRUE;
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            if (gbActive == TRUE)
            {
                display();
                update();
            }
        }
    }

    uninitialize();
    return ((int)msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    void ToggleFullscreen(void);
    void resize(int, int);

    switch (iMsg)
    {
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
                resize(LOWORD(lParam), HIWORD(lParam));
            }
            break;

        case WM_KEYDOWN:
            switch (LOWORD(wParam))
            {
                case VK_ESCAPE:
                    fprintf(gFILE, "WndProc() VK_ESCAPE-> Program ended successfully.\n");
                    fclose(gFILE);
                    gFILE = NULL;
                    DestroyWindow(hwnd);
                    break;
            }
            break;

        case WM_CHAR:
            switch (LOWORD(wParam))
            {
                case 'F':
                case 'f':
                    if (gbFullscreen == FALSE)
                    {
                        ToggleFullscreen();
                        gbFullscreen = TRUE;
                        fprintf(gFILE, "WndProc() WM_CHAR(F key)-> Program entered Fullscreen.\n");
                    }
                    else
                    {
                        ToggleFullscreen();
                        gbFullscreen = FALSE;
                        fprintf(gFILE, "WndProc() WM_CHAR(F key)-> Program ended Fullscreen.\n");
                    }
                    break;
            }
            break;

        case WM_RBUTTONDOWN:
            DestroyWindow(hwnd);
            break;

        case WM_CLOSE:
            uninitialize();
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            break;
    }

    return (DefWindowProc(hwnd, iMsg, wParam, lParam));
}

void ToggleFullscreen(void)
{
    MONITORINFO mi = { sizeof(MONITORINFO) };

    if (gbFullscreen == FALSE)
    {
        dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
        if (dwStyle & WS_OVERLAPPEDWINDOW)
        {
            if (GetWindowPlacement(ghwnd, &wpPrev) &&
                GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi))
            {
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
    }
    else
    {
        SetWindowPlacement(ghwnd, &wpPrev);
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    }
}

/////////////////////////////////////////////////////////////////
// --- INSTANCE LAYER AND EXTENSION SETUP (modified) --- //
/////////////////////////////////////////////////////////////////

// ***** NEW / CHANGED FOR OPTIONAL VALIDATION *****
// Only enable VK_LAYER_KHRONOS_validation if gEnableValidation == true
static VkResult FillInstanceLayerNames(void)
{
    VkResult vkResult = VK_SUCCESS;

    // Default to zero
    enabledInstanceLayerCount = 0;

    // If we are NOT enabling validation, skip all checks
    if (!gEnableValidation)
    {
        fprintf(gFILE, "FillInstanceLayerNames(): Validation disabled; skipping.\n");
        return VK_SUCCESS;
    }

    uint32_t instanceLayerCount = 0;
    vkResult = vkEnumerateInstanceLayerProperties(&instanceLayerCount, NULL);
    if (vkResult != VK_SUCCESS || instanceLayerCount == 0)
    {
        fprintf(gFILE, "FillInstanceLayerNames(): Could not get Instance Layer count\n");
        return vkResult;
    }

    VkLayerProperties* layerProps = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * instanceLayerCount);
    if (!layerProps) return VK_ERROR_OUT_OF_HOST_MEMORY;

    vkResult = vkEnumerateInstanceLayerProperties(&instanceLayerCount, layerProps);
    if (vkResult != VK_SUCCESS)
    {
        free(layerProps);
        return vkResult;
    }

    VkBool32 validationFound = VK_FALSE;
    for (uint32_t i = 0; i < instanceLayerCount; i++)
    {
        if (strcmp(layerProps[i].layerName, "VK_LAYER_KHRONOS_validation") == 0)
        {
            validationFound = VK_TRUE;
            break;
        }
    }

    if (validationFound == VK_TRUE)
    {
        enabledInstanceLayerNames_array[0] = "VK_LAYER_KHRONOS_validation";
        enabledInstanceLayerCount = 1;
        fprintf(gFILE, "FillInstanceLayerNames(): Found and enabling VK_LAYER_KHRONOS_validation\n");
    }
    else
    {
        fprintf(gFILE, "FillInstanceLayerNames(): VK_LAYER_KHRONOS_validation not found.\n");
        // You could treat this as error, or just continue without it.
        // return VK_ERROR_LAYER_NOT_PRESENT;
    }

    free(layerProps);
    return VK_SUCCESS;
}

// ***** NEW / CHANGED FOR OPTIONAL VALIDATION *****
// Only add the debug utils extension if gEnableValidation == true
static void AddDebugUtilsExtensionIfPresent(void)
{
    // If not enabling validation, skip
    if (!gEnableValidation)
        return;

    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extCount, NULL);
    if (extCount == 0) return;

    VkExtensionProperties *props = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*extCount);
    if(!props) return;

    if(vkEnumerateInstanceExtensionProperties(NULL, &extCount, props) != VK_SUCCESS)
    {
        free(props);
        return;
    }

    for(uint32_t i=0; i<extCount; i++)
    {
        if(strcmp(props[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
        {
            enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            fprintf(gFILE, "AddDebugUtilsExtensionIfPresent(): Found and enabling %s\n",
                    VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            break;
        }
    }

    free(props);
}

VkResult FillInstanceExtensionNames(void)
{
    VkResult vkResult = VK_SUCCESS;

    uint32_t instanceExtensionCount = 0;
    vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, NULL);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): First call to vkEnumerateInstanceExtensionProperties() failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): count = %u\n", instanceExtensionCount);
    }

    if(instanceExtensionCount == 0)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): No instance extensions found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkExtensionProperties* vkExtensionProperties_array =
        (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * instanceExtensionCount);
    if(!vkExtensionProperties_array) return VK_ERROR_OUT_OF_HOST_MEMORY;

    vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, vkExtensionProperties_array);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): second call to vkEnumerateInstanceExtensionProperties() failed\n");
        free(vkExtensionProperties_array);
        return vkResult;
    }

    for (uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        fprintf(gFILE, "Instance Extension: %s\n", vkExtensionProperties_array[i].extensionName);
    }

    VkBool32 vulkanSurfaceExtensionFound = VK_FALSE;
    VkBool32 vulkanWin32SurfaceExtensionFound = VK_FALSE;

    for (uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        if (strcmp(vkExtensionProperties_array[i].extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0)
        {
            vulkanSurfaceExtensionFound = VK_TRUE;
        }
        if (strcmp(vkExtensionProperties_array[i].extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0)
        {
            vulkanWin32SurfaceExtensionFound = VK_TRUE;
        }
    }

    enabledInstanceExtensionsCount = 0;
    if (vulkanSurfaceExtensionFound)
    {
        enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
        fprintf(gFILE, "Enabling: %s\n", VK_KHR_SURFACE_EXTENSION_NAME);
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): %s not found\n", VK_KHR_SURFACE_EXTENSION_NAME);
        free(vkExtensionProperties_array);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (vulkanWin32SurfaceExtensionFound)
    {
        enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
        fprintf(gFILE, "Enabling: %s\n", VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): %s not found\n", VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        free(vkExtensionProperties_array);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    free(vkExtensionProperties_array);
    AddDebugUtilsExtensionIfPresent();

    return VK_SUCCESS;
}

// ***** NEW / CHANGED FOR OPTIONAL VALIDATION *****
// We only create the debug messenger if gEnableValidation == true and we have the debug utils extension
VkResult CreateVulkanInstance(void)
{
    VkResult vkResult = VK_SUCCESS;

    vkResult = FillInstanceExtensionNames();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateVulkanInstance(): FillInstanceExtensionNames() failed\n");
        return vkResult;
    }

    vkResult = FillInstanceLayerNames();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateVulkanInstance(): FillInstanceLayerNames() failed\n");
        return vkResult;
    }

    VkApplicationInfo vkApplicationInfo;
    memset((void*)&vkApplicationInfo, 0, sizeof(VkApplicationInfo));
    vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vkApplicationInfo.pNext = NULL;
    vkApplicationInfo.pApplicationName = gpszAppName;
    vkApplicationInfo.applicationVersion = 1;
    vkApplicationInfo.pEngineName = gpszAppName;
    vkApplicationInfo.engineVersion = 1;
    vkApplicationInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo vkInstanceCreateInfo;
    memset((void*)&vkInstanceCreateInfo, 0, sizeof(VkInstanceCreateInfo));
    vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkInstanceCreateInfo.pNext = NULL;
    vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;

    vkInstanceCreateInfo.enabledExtensionCount = enabledInstanceExtensionsCount;
    vkInstanceCreateInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;

    vkInstanceCreateInfo.enabledLayerCount = enabledInstanceLayerCount;
    vkInstanceCreateInfo.ppEnabledLayerNames = enabledInstanceLayerNames_array;

    vkResult = vkCreateInstance(&vkInstanceCreateInfo, NULL, &vkInstance);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() failed, code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() succeeded\n");
    }

    // If validation is enabled and we have a validation layer enabled,
    // attempt to create the debug utils messenger.
    if(gEnableValidation && (enabledInstanceLayerCount > 0))
    {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        memset(&debugCreateInfo, 0, sizeof(debugCreateInfo));
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = DebugCallback;
        debugCreateInfo.pUserData = NULL;

        vkResult = CreateDebugUtilsMessengerEXT(vkInstance, &debugCreateInfo, NULL, &gDebugUtilsMessenger);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateVulkanInstance(): Failed to create DebugUtilsMessengerEXT\n");
            gDebugUtilsMessenger = VK_NULL_HANDLE;
        }
        else
        {
            fprintf(gFILE, "CreateVulkanInstance(): DebugUtilsMessengerEXT created\n");
        }
    }

    return vkResult;
}

VkResult GetSupportedSurface(void)
{
    VkResult vkResult = VK_SUCCESS;
    VkWin32SurfaceCreateInfoKHR vkWin32SurfaceCreateInfoKHR;
    memset((void*)&vkWin32SurfaceCreateInfoKHR, 0, sizeof(VkWin32SurfaceCreateInfoKHR));
    vkWin32SurfaceCreateInfoKHR.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    vkWin32SurfaceCreateInfoKHR.pNext = NULL;
    vkWin32SurfaceCreateInfoKHR.flags = 0;
    vkWin32SurfaceCreateInfoKHR.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    vkWin32SurfaceCreateInfoKHR.hwnd = ghwnd;

    vkResult = vkCreateWin32SurfaceKHR(vkInstance, &vkWin32SurfaceCreateInfoKHR, NULL, &vkSurfaceKHR);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "GetSupportedSurface(): vkCreateWin32SurfaceKHR() failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "GetSupportedSurface(): vkCreateWin32SurfaceKHR() succedded\n");
    }

    return vkResult;
}

VkResult GetPhysicalDevice(void)
{
    VkResult vkResult = VK_SUCCESS;
    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, NULL);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() first call failed %d\n", vkResult);
        return vkResult;
    }
    else if (physicalDeviceCount == 0)
    {
        fprintf(gFILE, "GetPhysicalDevice(): No physical devices found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkPhysicalDevice_array = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, vkPhysicalDevice_array);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "GetPhysicalDevice(): second call failed %d\n", vkResult);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkBool32 bFound = VK_FALSE;
    for(uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        uint32_t quequeCount = UINT32_MAX;
        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &quequeCount, NULL);
        VkQueueFamilyProperties *vkQueueFamilyProperties_array =
            (VkQueueFamilyProperties*) malloc(sizeof(VkQueueFamilyProperties)*quequeCount);
        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &quequeCount, vkQueueFamilyProperties_array);

        VkBool32 *isQuequeSurfaceSupported_array =
            (VkBool32*) malloc(sizeof(VkBool32)*quequeCount);

        for(uint32_t j = 0; j < quequeCount; j++)
        {
            vkResult = vkGetPhysicalDeviceSurfaceSupportKHR(
                vkPhysicalDevice_array[i], j, vkSurfaceKHR, &isQuequeSurfaceSupported_array[j]);
        }

        for(uint32_t j = 0; j < quequeCount; j++)
        {
            if(vkQueueFamilyProperties_array[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                if(isQuequeSurfaceSupported_array[j] == VK_TRUE)
                {
                    vkPhysicalDevice_selected = vkPhysicalDevice_array[i];
                    graphicsQuequeFamilyIndex_selected = j;
                    bFound = VK_TRUE;
                    break;
                }
            }
        }

        free(isQuequeSurfaceSupported_array);
        free(vkQueueFamilyProperties_array);

        if(bFound == VK_TRUE)
            break;
    }

    if(bFound == VK_FALSE)
    {
        fprintf(gFILE, "GetPhysicalDevice(): failed to find suitable device.\n");
        free(vkPhysicalDevice_array);
        vkPhysicalDevice_array = NULL;
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    memset((void*)&vkPhysicalDeviceMemoryProperties, 0, sizeof(vkPhysicalDeviceMemoryProperties));
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_selected, &vkPhysicalDeviceMemoryProperties);

    VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures;
    memset((void*)&vkPhysicalDeviceFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
    vkGetPhysicalDeviceFeatures(vkPhysicalDevice_selected, &vkPhysicalDeviceFeatures);

    if(vkPhysicalDeviceFeatures.tessellationShader)
        fprintf(gFILE, "Tessellation shader supported.\n");
    else
        fprintf(gFILE, "Tessellation shader NOT supported.\n");

    if(vkPhysicalDeviceFeatures.geometryShader)
        fprintf(gFILE, "Geometry shader supported.\n");
    else
        fprintf(gFILE, "Geometry shader NOT supported.\n");

    return vkResult;
}

VkResult PrintVulkanInfo(void)
{
    VkResult vkResult = VK_SUCCESS;
    fprintf(gFILE, "********** PrintVulkanInfo **********\n");

    for(uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        VkPhysicalDeviceProperties vkPhysicalDeviceProperties;
        memset((void*)&vkPhysicalDeviceProperties, 0, sizeof(VkPhysicalDeviceProperties));
        vkGetPhysicalDeviceProperties(vkPhysicalDevice_array[i], &vkPhysicalDeviceProperties);

        uint32_t majorVersion = VK_API_VERSION_MAJOR(vkPhysicalDeviceProperties.apiVersion);
        uint32_t minorVersion = VK_API_VERSION_MINOR(vkPhysicalDeviceProperties.apiVersion);
        uint32_t patchVersion = VK_API_VERSION_PATCH(vkPhysicalDeviceProperties.apiVersion);
        fprintf(gFILE,"apiVersion = %d.%d.%d\n", majorVersion, minorVersion, patchVersion);

        fprintf(gFILE,"deviceName = %s\n", vkPhysicalDeviceProperties.deviceName);

        switch(vkPhysicalDeviceProperties.deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                fprintf(gFILE,"deviceType = Integrated GPU\n");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                fprintf(gFILE,"deviceType = Discrete GPU\n");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                fprintf(gFILE,"deviceType = Virtual GPU\n");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                fprintf(gFILE,"deviceType = CPU\n");
                break;
            default:
                fprintf(gFILE,"deviceType = UNKNOWN\n");
                break;
        }

        fprintf(gFILE,"vendorID = 0x%04x\n", vkPhysicalDeviceProperties.vendorID);
        fprintf(gFILE,"deviceID = 0x%04x\n", vkPhysicalDeviceProperties.deviceID);
    }

    if(vkPhysicalDevice_array)
    {
        free(vkPhysicalDevice_array);
        vkPhysicalDevice_array = NULL;
        fprintf(gFILE, "PrintVulkanInfo(): Freed vkPhysicalDevice_array\n");
    }

    return vkResult;
}

VkResult FillDeviceExtensionNames(void)
{
    VkResult vkResult = VK_SUCCESS;
    uint32_t deviceExtensionCount = 0;
    vkResult = vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected, NULL, &deviceExtensionCount, NULL);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "FillDeviceExtensionNames(): First call failed\n");
        return vkResult;
    }

    VkExtensionProperties* vkExtensionProperties_array =
        (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*deviceExtensionCount);

    vkResult = vkEnumerateDeviceExtensionProperties(
        vkPhysicalDevice_selected, NULL, &deviceExtensionCount, vkExtensionProperties_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "FillDeviceExtensionNames(): second call failed\n");
        free(vkExtensionProperties_array);
        return vkResult;
    }

    VkBool32 vulkanSwapchainExtensionFound = VK_FALSE;
    for(uint32_t i=0; i<deviceExtensionCount; i++)
    {
        fprintf(gFILE, "Device Extension: %s\n", vkExtensionProperties_array[i].extensionName);
        if(strcmp(vkExtensionProperties_array[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            vulkanSwapchainExtensionFound = VK_TRUE;
        }
    }

    enabledDeviceExtensionsCount = 0;
    if(vulkanSwapchainExtensionFound)
    {
        enabledDeviceExtensionNames_array[enabledDeviceExtensionsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        fprintf(gFILE, "FillDeviceExtensionNames(): enabling VK_KHR_swapchain\n");
    }
    else
    {
        free(vkExtensionProperties_array);
        fprintf(gFILE, "FillDeviceExtensionNames(): No swapchain extension found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    free(vkExtensionProperties_array);
    return vkResult;
}

VkResult CreateVulKanDevice(void)
{
    VkResult vkResult = VK_SUCCESS;
    vkResult = FillDeviceExtensionNames();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateVulKanDevice(): FillDeviceExtensionNames() failed\n");
        return vkResult;
    }

    float queuePriorities[1] = {1.0f};
    VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo;
    memset(&vkDeviceQueueCreateInfo, 0, sizeof(VkDeviceQueueCreateInfo));
    vkDeviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    vkDeviceQueueCreateInfo.queueFamilyIndex = graphicsQuequeFamilyIndex_selected;
    vkDeviceQueueCreateInfo.queueCount = 1;
    vkDeviceQueueCreateInfo.pQueuePriorities = queuePriorities;

    VkDeviceCreateInfo vkDeviceCreateInfo;
    memset(&vkDeviceCreateInfo, 0, sizeof(VkDeviceCreateInfo));
    vkDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    vkDeviceCreateInfo.enabledExtensionCount = enabledDeviceExtensionsCount;
    vkDeviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensionNames_array;
    vkDeviceCreateInfo.queueCreateInfoCount = 1;
    vkDeviceCreateInfo.pQueueCreateInfos = &vkDeviceQueueCreateInfo;
    vkDeviceCreateInfo.pEnabledFeatures = NULL;

    // ***** NEW / CHANGED FOR OPTIONAL VALIDATION *****
    // If we want device-level validation layers, we would specify them here.
    // For now, we just do 0, but you could do similarly:
    // if (gEnableValidation) { ... } etc.
    vkDeviceCreateInfo.enabledLayerCount = 0;
    vkDeviceCreateInfo.ppEnabledLayerNames = NULL;

    vkResult = vkCreateDevice(vkPhysicalDevice_selected, &vkDeviceCreateInfo, NULL, &vkDevice);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateVulKanDevice(): vkCreateDevice() failed\n");
        return vkResult;
    }

    return vkResult;
}

void GetDeviceQueque(void)
{
    vkGetDeviceQueue(vkDevice, graphicsQuequeFamilyIndex_selected, 0, &vkQueue);
    if(vkQueue == VK_NULL_HANDLE)
    {
        fprintf(gFILE, "GetDeviceQueque(): vkGetDeviceQueue() returned NULL\n");
        return;
    }
    fprintf(gFILE, "GetDeviceQueque(): acquired device queue\n");
}

VkResult getPhysicalDeviceSurfaceFormatAndColorSpace(void)
{
    VkResult vkResult = VK_SUCCESS;
    uint32_t FormatCount = 0;

    vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_selected, vkSurfaceKHR, &FormatCount, NULL);
    if(vkResult != VK_SUCCESS || FormatCount == 0)
    {
        fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): first call fail or zero count.\n");
        return (vkResult == VK_SUCCESS) ? VK_ERROR_INITIALIZATION_FAILED : vkResult;
    }

    VkSurfaceFormatKHR *vkSurfaceFormatKHR_array =
        (VkSurfaceFormatKHR*)malloc(FormatCount * sizeof(VkSurfaceFormatKHR));
    vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        vkPhysicalDevice_selected, vkSurfaceKHR, &FormatCount, vkSurfaceFormatKHR_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): second call failed\n");
        free(vkSurfaceFormatKHR_array);
        return vkResult;
    }

    if((1 == FormatCount) && (vkSurfaceFormatKHR_array[0].format == VK_FORMAT_UNDEFINED))
    {
        vkFormat_color = VK_FORMAT_B8G8R8A8_UNORM;
    }
    else
    {
        vkFormat_color = vkSurfaceFormatKHR_array[0].format;
    }
    vkColorSpaceKHR = vkSurfaceFormatKHR_array[0].colorSpace;

    free(vkSurfaceFormatKHR_array);
    return vkResult;
}

VkResult getPhysicalDevicePresentMode(void)
{
    VkResult vkResult = VK_SUCCESS;
    uint32_t presentModeCount = 0;

    vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_selected, vkSurfaceKHR, &presentModeCount, NULL);
    if(vkResult != VK_SUCCESS || presentModeCount == 0)
    {
        fprintf(gFILE, "getPhysicalDevicePresentMode(): fail or zero count.\n");
        return (vkResult == VK_SUCCESS) ? VK_ERROR_INITIALIZATION_FAILED : vkResult;
    }

    VkPresentModeKHR *vkPresentModeKHR_array =
        (VkPresentModeKHR*)malloc(presentModeCount * sizeof(VkPresentModeKHR));
    vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysicalDevice_selected, vkSurfaceKHR, &presentModeCount, vkPresentModeKHR_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "getPhysicalDevicePresentMode(): second call failed.\n");
        free(vkPresentModeKHR_array);
        return vkResult;
    }

    vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
    for(uint32_t i = 0; i < presentModeCount; i++)
    {
        if(vkPresentModeKHR_array[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    fprintf(gFILE, "PresentMode used: %d\n", vkPresentModeKHR);

    free(vkPresentModeKHR_array);
    return vkResult;
}

VkResult CreateSwapChain(VkBool32 vsync)
{
    VkResult vkResult = VK_SUCCESS;

    vkResult = getPhysicalDeviceSurfaceFormatAndColorSpace();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateSwapChain(): getPhysicalDeviceSurfaceFormatAndColorSpace() failed.\n");
        return vkResult;
    }

    VkSurfaceCapabilitiesKHR vkSurfaceCapabilitiesKHR;
    memset((void*)&vkSurfaceCapabilitiesKHR, 0, sizeof(VkSurfaceCapabilitiesKHR));
    vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        vkPhysicalDevice_selected, vkSurfaceKHR, &vkSurfaceCapabilitiesKHR);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateSwapChain(): vkGetPhysicalDeviceSurfaceCapabilitiesKHR() failed.\n");
        return vkResult;
    }

    uint32_t testingNumerOfSwapChainImages = vkSurfaceCapabilitiesKHR.minImageCount + 1;
    uint32_t desiredNumerOfSwapChainImages = 0;

    if((vkSurfaceCapabilitiesKHR.maxImageCount > 0) &&
       (vkSurfaceCapabilitiesKHR.maxImageCount < testingNumerOfSwapChainImages))
    {
        desiredNumerOfSwapChainImages = vkSurfaceCapabilitiesKHR.maxImageCount;
    }
    else
    {
        desiredNumerOfSwapChainImages = vkSurfaceCapabilitiesKHR.minImageCount;
    }

    if(vkSurfaceCapabilitiesKHR.currentExtent.width != UINT32_MAX)
    {
        vkExtent2D_SwapChain.width  = vkSurfaceCapabilitiesKHR.currentExtent.width;
        vkExtent2D_SwapChain.height = vkSurfaceCapabilitiesKHR.currentExtent.height;
    }
    else
    {
        vkExtent2D_SwapChain.width  = winWidth;
        vkExtent2D_SwapChain.height = winHeight;
        vkExtent2D_SwapChain.width  = max(vkSurfaceCapabilitiesKHR.minImageExtent.width,
                                          min(vkSurfaceCapabilitiesKHR.maxImageExtent.width, vkExtent2D_SwapChain.width));
        vkExtent2D_SwapChain.height = max(vkSurfaceCapabilitiesKHR.minImageExtent.height,
                                          min(vkSurfaceCapabilitiesKHR.maxImageExtent.height, vkExtent2D_SwapChain.height));
    }

    VkImageUsageFlags vkImageUsageFlagBits = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkSurfaceTransformFlagBitsKHR vkSurfaceTransformFlagBitsKHR;
    if(vkSurfaceCapabilitiesKHR.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        vkSurfaceTransformFlagBitsKHR = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
        vkSurfaceTransformFlagBitsKHR = vkSurfaceCapabilitiesKHR.currentTransform;

    vkResult = getPhysicalDevicePresentMode();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateSwapChain(): getPhysicalDevicePresentMode() failed.\n");
        return vkResult;
    }

    VkSwapchainCreateInfoKHR vkSwapchainCreateInfoKHR;
    memset((void*)&vkSwapchainCreateInfoKHR, 0, sizeof(VkSwapchainCreateInfoKHR));
    vkSwapchainCreateInfoKHR.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    vkSwapchainCreateInfoKHR.surface = vkSurfaceKHR;
    vkSwapchainCreateInfoKHR.minImageCount = desiredNumerOfSwapChainImages;
    vkSwapchainCreateInfoKHR.imageFormat = vkFormat_color;
    vkSwapchainCreateInfoKHR.imageColorSpace = vkColorSpaceKHR;
    vkSwapchainCreateInfoKHR.imageExtent.width = vkExtent2D_SwapChain.width;
    vkSwapchainCreateInfoKHR.imageExtent.height = vkExtent2D_SwapChain.height;
    vkSwapchainCreateInfoKHR.imageUsage = vkImageUsageFlagBits;
    vkSwapchainCreateInfoKHR.preTransform = vkSurfaceTransformFlagBitsKHR;
    vkSwapchainCreateInfoKHR.imageArrayLayers = 1;
    vkSwapchainCreateInfoKHR.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkSwapchainCreateInfoKHR.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    vkSwapchainCreateInfoKHR.presentMode = vkPresentModeKHR;
    vkSwapchainCreateInfoKHR.clipped = VK_TRUE;
    vkSwapchainCreateInfoKHR.oldSwapchain = VK_NULL_HANDLE;

    vkResult = vkCreateSwapchainKHR(vkDevice, &vkSwapchainCreateInfoKHR, NULL, &vkSwapchainKHR);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateSwapChain(): vkCreateSwapchainKHR() failed.\n");
        return vkResult;
    }

    fprintf(gFILE, "CreateSwapChain(): vkCreateSwapchainKHR() succeeded.\n");
    return vkResult;
}

VkResult CreateImagesAndImageViews(void)
{
    VkResult vkResult = VK_SUCCESS;
    vkResult = vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, NULL);
    if (vkResult != VK_SUCCESS || swapchainImageCount == 0)
    {
        fprintf(gFILE, "CreateImagesAndImageViews(): swapchainImageCount == 0 or failed.\n");
        return (vkResult == VK_SUCCESS)? VK_ERROR_INITIALIZATION_FAILED : vkResult;
    }

    swapChainImage_array = (VkImage*)malloc(sizeof(VkImage) * swapchainImageCount);
    vkResult = vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapChainImage_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateImagesAndImageViews(): second call failed.\n");
        return vkResult;
    }

    swapChainImageView_array = (VkImageView*)malloc(sizeof(VkImageView)*swapchainImageCount);

    VkImageViewCreateInfo vkImageViewCreateInfo;
    memset(&vkImageViewCreateInfo, 0, sizeof(VkImageViewCreateInfo));
    vkImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vkImageViewCreateInfo.format = vkFormat_color;
    vkImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    vkImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    vkImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    vkImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    vkImageViewCreateInfo.subresourceRange.levelCount = 1;
    vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    vkImageViewCreateInfo.subresourceRange.layerCount = 1;

    for(uint32_t i = 0; i < swapchainImageCount; i++)
    {
        vkImageViewCreateInfo.image = swapChainImage_array[i];
        vkResult = vkCreateImageView(vkDevice, &vkImageViewCreateInfo, NULL, &swapChainImageView_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateImagesAndImageViews(): vkCreateImageView failed at i=%u.\n", i);
            return vkResult;
        }
    }

    return vkResult;
}

VkResult CreateCommandPool()
{
    VkResult vkResult = VK_SUCCESS;
    VkCommandPoolCreateInfo vkCommandPoolCreateInfo;
    memset((void*)&vkCommandPoolCreateInfo, 0, sizeof(VkCommandPoolCreateInfo));
    vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCommandPoolCreateInfo.queueFamilyIndex = graphicsQuequeFamilyIndex_selected;

    vkResult = vkCreateCommandPool(vkDevice, &vkCommandPoolCreateInfo, NULL, &vkCommandPool);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateCommandPool(): vkCreateCommandPool() failed.\n");
        return vkResult;
    }

    return vkResult;
}

VkResult CreateCommandBuffers(void)
{
    VkResult vkResult = VK_SUCCESS;
    VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo;
    memset(&vkCommandBufferAllocateInfo, 0, sizeof(VkCommandBufferAllocateInfo));
    vkCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    vkCommandBufferAllocateInfo.commandPool = vkCommandPool;
    vkCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkCommandBufferAllocateInfo.commandBufferCount = 1;

    vkCommandBuffer_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*swapchainImageCount);

    for(uint32_t i = 0; i < swapchainImageCount; i++)
    {
        vkResult = vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, &vkCommandBuffer_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateCommandBuffers(): vkAllocateCommandBuffers() fail at i=%u.\n", i);
            return vkResult;
        }
    }

    return vkResult;
}

VkResult CreateRenderPass(void)
{
    VkResult vkResult = VK_SUCCESS;

    VkAttachmentDescription vkAttachmentDescription_array[1];
    memset(vkAttachmentDescription_array, 0, sizeof(vkAttachmentDescription_array));

    vkAttachmentDescription_array[0].format = vkFormat_color;
    vkAttachmentDescription_array[0].samples = VK_SAMPLE_COUNT_1_BIT;
    vkAttachmentDescription_array[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    vkAttachmentDescription_array[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    vkAttachmentDescription_array[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    vkAttachmentDescription_array[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    vkAttachmentDescription_array[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkAttachmentDescription_array[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference vkAttachmentReference;
    memset(&vkAttachmentReference, 0, sizeof(VkAttachmentReference));
    vkAttachmentReference.attachment = 0;
    vkAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription vkSubpassDescription;
    memset(&vkSubpassDescription, 0, sizeof(VkSubpassDescription));
    vkSubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkSubpassDescription.colorAttachmentCount = 1;
    vkSubpassDescription.pColorAttachments = &vkAttachmentReference;

    VkRenderPassCreateInfo vkRenderPassCreateInfo;
    memset(&vkRenderPassCreateInfo, 0, sizeof(VkRenderPassCreateInfo));
    vkRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    vkRenderPassCreateInfo.attachmentCount = 1;
    vkRenderPassCreateInfo.pAttachments = vkAttachmentDescription_array;
    vkRenderPassCreateInfo.subpassCount = 1;
    vkRenderPassCreateInfo.pSubpasses = &vkSubpassDescription;

    vkResult = vkCreateRenderPass(vkDevice, &vkRenderPassCreateInfo, NULL, &vkRenderPass);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateRenderPass(): vkCreateRenderPass() failed\n");
        return vkResult;
    }

    return vkResult;
}

VkResult CreateFramebuffers(void)
{
    VkResult vkResult = VK_SUCCESS;
    VkImageView vkImageView_attachment_array[1];
    VkFramebufferCreateInfo vkFramebufferCreateInfo;
    memset((void*)&vkFramebufferCreateInfo, 0, sizeof(VkFramebufferCreateInfo));
    vkFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    vkFramebufferCreateInfo.renderPass = vkRenderPass;
    vkFramebufferCreateInfo.attachmentCount = 1;
    vkFramebufferCreateInfo.width = vkExtent2D_SwapChain.width;
    vkFramebufferCreateInfo.height = vkExtent2D_SwapChain.height;
    vkFramebufferCreateInfo.layers = 1;

    vkFramebuffer_array = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*swapchainImageCount);

    for(uint32_t i = 0; i < swapchainImageCount; i++)
    {
        vkImageView_attachment_array[0] = swapChainImageView_array[i];
        vkFramebufferCreateInfo.pAttachments = vkImageView_attachment_array;

        vkResult = vkCreateFramebuffer(vkDevice, &vkFramebufferCreateInfo, NULL, &vkFramebuffer_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateFramebuffers(): vkCreateFramebuffer() failed at i=%u.\n", i);
            return vkResult;
        }
    }

    return vkResult;
}

VkResult CreateSemaphores(void)
{
    VkResult vkResult = VK_SUCCESS;
    VkSemaphoreCreateInfo vkSemaphoreCreateInfo;
    memset(&vkSemaphoreCreateInfo, 0, sizeof(VkSemaphoreCreateInfo));
    vkSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkResult = vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, NULL, &imageAvailableSemaphores[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateSemaphores(): vkCreateSemaphore() fail for imageAvailableSemaphores[%d].\n", i);
            return vkResult;
        }
        vkResult = vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, NULL, &renderFinishedSemaphores[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateSemaphores(): vkCreateSemaphore() fail for renderFinishedSemaphores[%d].\n", i);
            return vkResult;
        }
    }
    return vkResult;
}

VkResult CreateFences(void)
{
    VkResult vkResult = VK_SUCCESS;
    VkFenceCreateInfo vkFenceCreateInfo;
    memset(&vkFenceCreateInfo, 0, sizeof(VkFenceCreateInfo));
    vkFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkResult = vkCreateFence(vkDevice, &vkFenceCreateInfo, NULL, &inFlightFences[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateFences(): vkCreateFence() failed for inFlightFences[%d].\n", i);
            return vkResult;
        }
    }
    return vkResult;
}

VkResult buildCommandBuffers(void)
{
    VkResult vkResult = VK_SUCCESS;

    for(uint32_t i = 0; i < swapchainImageCount; i++)
    {
        vkResult = vkResetCommandBuffer(vkCommandBuffer_array[i], 0);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "buildCommandBuffers(): vkResetCommandBuffer() fail i=%u.\n", i);
            return vkResult;
        }

        VkCommandBufferBeginInfo vkCommandBufferBeginInfo;
        memset(&vkCommandBufferBeginInfo, 0, sizeof(VkCommandBufferBeginInfo));
        vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkResult = vkBeginCommandBuffer(vkCommandBuffer_array[i], &vkCommandBufferBeginInfo);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "buildCommandBuffers(): vkBeginCommandBuffer() fail i=%u.\n", i);
            return vkResult;
        }

        VkClearValue vkClearValue_array[1];
        vkClearValue_array[0].color = vkClearColorValue;

        VkRenderPassBeginInfo vkRenderPassBeginInfo;
        memset(&vkRenderPassBeginInfo, 0, sizeof(VkRenderPassBeginInfo));
        vkRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vkRenderPassBeginInfo.renderPass = vkRenderPass;
        vkRenderPassBeginInfo.framebuffer = vkFramebuffer_array[i];
        vkRenderPassBeginInfo.renderArea.offset.x = 0;
        vkRenderPassBeginInfo.renderArea.offset.y = 0;
        vkRenderPassBeginInfo.renderArea.extent = vkExtent2D_SwapChain;
        vkRenderPassBeginInfo.clearValueCount = 1;
        vkRenderPassBeginInfo.pClearValues = vkClearValue_array;

        vkCmdBeginRenderPass(vkCommandBuffer_array[i], &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(vkCommandBuffer_array[i]);

        vkResult = vkEndCommandBuffer(vkCommandBuffer_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "buildCommandBuffers(): vkEndCommandBuffer() fail i=%u.\n", i);
            return vkResult;
        }
    }

    return vkResult;
}

VkResult initialize(void)
{
    VkResult vkResult = VK_SUCCESS;
    vkResult = CreateVulkanInstance();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = GetSupportedSurface();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = GetPhysicalDevice();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = PrintVulkanInfo();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateVulKanDevice();
    if (vkResult != VK_SUCCESS) return vkResult;

    GetDeviceQueque();

    vkResult = CreateSwapChain(VK_FALSE);
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateImagesAndImageViews();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateCommandPool();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateCommandBuffers();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateRenderPass();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateFramebuffers();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateSemaphores();
    if (vkResult != VK_SUCCESS) return vkResult;

    vkResult = CreateFences();
    if (vkResult != VK_SUCCESS) return vkResult;

    memset((void*)&vkClearColorValue, 0, sizeof(VkClearColorValue));
    vkClearColorValue.float32[0] = 0.0f;
    vkClearColorValue.float32[1] = 0.0f;
    vkClearColorValue.float32[2] = 1.0f;
    vkClearColorValue.float32[3] = 1.0f;

    vkResult = buildCommandBuffers();
    if (vkResult != VK_SUCCESS) return vkResult;

    bInitialized = TRUE;
    fprintf(gFILE, "******* End of initialize ********\n");
    return vkResult;
}

// *** NEW/CHANGED FOR RESIZE ***
void resize(int width, int height)
{
    // Update our global width/height
    winWidth = width;
    winHeight = height;

    // If minimized (either dimension is 0), just skip.
    if (winWidth == 0 || winHeight == 0)
        return;

    // Re-create the swap chain and everything dependent on it
    recreateSwapChain();
}

VkResult display(void)
{
    VkResult vkResult = VK_SUCCESS;
    if(!bInitialized)
    {
        fprintf(gFILE, "display(): not initialized yet.\n");
        return VK_FALSE;
    }

    vkResult = vkWaitForFences(vkDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "display(): vkWaitForFences() failed.\n");
        return vkResult;
    }
    vkResetFences(vkDevice, 1, &inFlightFences[currentFrame]);

    vkResult = vkAcquireNextImageKHR(
        vkDevice, vkSwapchainKHR, UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &currentImageIndex);
    if(vkResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // *** NEW/CHANGED FOR RESIZE ***
        resize(winWidth, winHeight);
        return VK_SUCCESS;
    }
    else if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "display(): vkAcquireNextImageKHR() failed code=%d.\n", vkResult);
        return vkResult;
    }

    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo vkSubmitInfo;
    memset(&vkSubmitInfo, 0, sizeof(VkSubmitInfo));
    vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vkSubmitInfo.waitSemaphoreCount = 1;
    vkSubmitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
    vkSubmitInfo.pWaitDstStageMask = &waitDstStageMask;
    vkSubmitInfo.commandBufferCount = 1;
    vkSubmitInfo.pCommandBuffers = &vkCommandBuffer_array[currentImageIndex];
    vkSubmitInfo.signalSemaphoreCount = 1;
    vkSubmitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

    vkResult = vkQueueSubmit(vkQueue, 1, &vkSubmitInfo, inFlightFences[currentFrame]);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "display(): vkQueueSubmit() failed.\n");
        return vkResult;
    }

    VkPresentInfoKHR vkPresentInfoKHR;
    memset(&vkPresentInfoKHR, 0, sizeof(VkPresentInfoKHR));
    vkPresentInfoKHR.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    vkPresentInfoKHR.swapchainCount = 1;
    vkPresentInfoKHR.pSwapchains = &vkSwapchainKHR;
    vkPresentInfoKHR.pImageIndices = &currentImageIndex;
    vkPresentInfoKHR.waitSemaphoreCount = 1;
    vkPresentInfoKHR.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];

    vkResult = vkQueuePresentKHR(vkQueue, &vkPresentInfoKHR);
    // *** NEW/CHANGED FOR RESIZE ***
    if (vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
    {
        resize(winWidth, winHeight);
        return VK_SUCCESS;
    }
    else if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "display(): vkQueuePresentKHR() failed.\n");
        return vkResult;
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return vkResult;
}

void update(void)
{
    // no-op
}

void uninitialize(void)
{
    if (gbFullscreen == TRUE)
    {
        ToggleFullscreen();
        gbFullscreen = FALSE;
    }

    if (ghwnd)
    {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    if(vkDevice)
    {
        vkDeviceWaitIdle(vkDevice);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if(inFlightFences[i] != VK_NULL_HANDLE)
            {
                vkDestroyFence(vkDevice, inFlightFences[i], NULL);
                inFlightFences[i] = VK_NULL_HANDLE;
            }
        }

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if(renderFinishedSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(vkDevice, renderFinishedSemaphores[i], NULL);
                renderFinishedSemaphores[i] = VK_NULL_HANDLE;
            }
            if(imageAvailableSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(vkDevice, imageAvailableSemaphores[i], NULL);
                imageAvailableSemaphores[i] = VK_NULL_HANDLE;
            }
        }

        cleanupSwapChain();

        vkDestroyDevice(vkDevice, NULL);
        vkDevice = VK_NULL_HANDLE;
    }

    // Destroy debug messenger if it was created
    if(gDebugUtilsMessenger != VK_NULL_HANDLE)
    {
        DestroyDebugUtilsMessengerEXT(vkInstance, gDebugUtilsMessenger, NULL);
        gDebugUtilsMessenger = VK_NULL_HANDLE;
    }

    if(vkSurfaceKHR)
    {
        vkDestroySurfaceKHR(vkInstance, vkSurfaceKHR, NULL);
        vkSurfaceKHR = VK_NULL_HANDLE;
    }

    if(vkInstance)
    {
        vkDestroyInstance(vkInstance, NULL);
        vkInstance = VK_NULL_HANDLE;
    }

    if (gFILE)
    {
        fprintf(gFILE, "uninitialize()-> Program ended successfully.\n");
        fclose(gFILE);
        gFILE = NULL;
    }
}
