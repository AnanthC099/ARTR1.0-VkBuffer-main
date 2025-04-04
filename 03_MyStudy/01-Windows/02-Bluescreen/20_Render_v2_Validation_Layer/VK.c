#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <math.h>
#include <string.h>

#include "VK.h"  // Your custom header as needed
#define LOG_FILE (char*)"Log.txt"

// Ensure we can use Win32-specific extensions
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

// Vulkan library
#pragma comment(lib, "vulkan-1.lib")

// Macros
#define WIN_WIDTH  800
#define WIN_HEIGHT 600

// Global variables and forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

const char* gpszAppName = "ARTR";

HWND  ghwnd       = NULL;
BOOL  gbActive    = FALSE;
DWORD dwStyle     = 0;
WINDOWPLACEMENT wpPrev;
BOOL  gbFullscreen = FALSE;

// Logging
FILE* gFILE = NULL;

// -- Instance extensions:
uint32_t enabledInstanceExtensionsCount = 0;
/*
   We potentially enable 3 extensions:
   1. VK_KHR_SURFACE_EXTENSION_NAME
   2. VK_KHR_WIN32_SURFACE_EXTENSION_NAME
   3. VK_EXT_debug_utils (optional, if available)
*/
const char* enabledInstanceExtensionNames_array[3];

// -- Instance layers (for validation):
uint32_t enabledLayerCount = 0;
/*
   Typically just "VK_LAYER_KHRONOS_validation", if found
*/
const char* enabledLayerNames_array[1];

// Vulkan Instance
VkInstance vkInstance = VK_NULL_HANDLE;

// Debug messenger (for validation output)
VkDebugUtilsMessengerEXT gDebugUtilsMessenger = VK_NULL_HANDLE;

// Presentation Surface
VkSurfaceKHR vkSurfaceKHR = VK_NULL_HANDLE;

// Physical Device
VkPhysicalDevice vkPhysicalDevice_selected = VK_NULL_HANDLE;
uint32_t         graphicsQuequeFamilyIndex_selected = UINT32_MAX;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;

// We store all enumerated physical devices so we can print device info:
uint32_t          physicalDeviceCount = 0;
VkPhysicalDevice* vkPhysicalDevice_array = NULL;

// Device extensions
uint32_t   enabledDeviceExtensionsCount = 0;
/*
   For swapchains, you need VK_KHR_SWAPCHAIN_EXTENSION_NAME
*/
const char* enabledDeviceExtensionNames_array[1];

// Logical Device + Queue
VkDevice vkDevice       = VK_NULL_HANDLE;
VkQueue vkQueue         = VK_NULL_HANDLE;

// Swapchain
VkSwapchainKHR vkSwapchainKHR = VK_NULL_HANDLE;
VkFormat       vkFormat_color  = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR vkColorSpaceKHR = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
VkExtent2D     vkExtent2D_SwapChain;
uint32_t       swapchainImageCount = UINT32_MAX;
VkImage*       swapChainImage_array = NULL;
VkImageView*   swapChainImageView_array = NULL;

// Command Pool & Buffers
VkCommandPool    vkCommandPool          = VK_NULL_HANDLE;
VkCommandBuffer* vkCommandBuffer_array  = NULL;

// RenderPass
VkRenderPass     vkRenderPass    = VK_NULL_HANDLE;

// Framebuffers
VkFramebuffer*   vkFramebuffer_array    = NULL;

// Sync objects
VkSemaphore      vkSemaphore_BackBuffer    = VK_NULL_HANDLE;
VkSemaphore      vkSemaphore_RenderComplete = VK_NULL_HANDLE;
VkFence*         vkFence_array             = NULL;

// Clear color
VkClearColorValue vkClearColorValue;

// Initialization check
BOOL     bInitialized      = FALSE;
uint32_t currentImageIndex = UINT32_MAX;

// Function Declarations
VkResult initialize(void);
void     uninitialize(void);
VkResult display(void);
void     update(void);
void     resize(int width, int height);
void     ToggleFullscreen(void);

// ------------- Validation Layer / Debug Messenger Support ------------- //

// Prototype to dynamically load vkCreateDebugUtilsMessengerEXT
static PFN_vkCreateDebugUtilsMessengerEXT  pfnCreateDebugUtilsMessengerEXT  = NULL;
static PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroyDebugUtilsMessengerEXT = NULL;

/**
 * The debug callback used by Vulkan validation layers whenever there is
 * a message to report (warning, error, info, etc.).
 */
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT        messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    // Print to your file or Visual Studio console or anywhere you like
    fprintf(gFILE,
        "Validation Layer: %s (Severity: 0x%x, Type: 0x%x)\n",
         pCallbackData->pMessage, messageSeverity, messageType);

    // Return VK_FALSE indicates that layer should not abort the call that triggered the validation message.
    return VK_FALSE;
}

/**
 * Loads the function pointers needed for setting up a debug messenger.
 * Call this AFTER vkCreateInstance but BEFORE using them.
 */
static void LoadDebugUtilsFunctionPointers(VkInstance instance)
{
    pfnCreateDebugUtilsMessengerEXT = 
        (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    pfnDestroyDebugUtilsMessengerEXT = 
        (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
}

/**
 * Creates the VkDebugUtilsMessengerEXT object used to capture validation-layer
 * callbacks and direct them to `debugCallback`.
 */
static VkResult SetupDebugMessenger(VkInstance instance)
{
    if (!pfnCreateDebugUtilsMessengerEXT) 
    {
        // If the extension wasn't found or function pointers couldn't be loaded,
        // just skip. Not necessarily an error, but validation won't happen.
        return VK_SUCCESS; 
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT   |
         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
         // Optionally enable info or verbose:
         // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT  |
         // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
         0;
    createInfo.messageType =
         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
         VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData       = NULL;

    return pfnCreateDebugUtilsMessengerEXT(instance, &createInfo, NULL, &gDebugUtilsMessenger);
}

// ----------------------------------------------------------------------- //
//                           Code Begins Here
// ----------------------------------------------------------------------- //

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
    // Local variables
    WNDCLASSEX wndclass;
    HWND       hwnd;
    MSG        msg;
    TCHAR      szAppName[256];
    int        iResult = 0;

    int SW = GetSystemMetrics(SM_CXSCREEN);
    int SH = GetSystemMetrics(SM_CYSCREEN);
    int xCoordinate = ((SW / 2) - (WIN_WIDTH / 2));
    int yCoordinate = ((SH / 2) - (WIN_HEIGHT / 2));

    BOOL    bDone    = FALSE;
    VkResult vkResult = VK_SUCCESS;

    // Open Log File
    gFILE = fopen(LOG_FILE, "w");
    if (!gFILE) {
        MessageBox(NULL, TEXT("Cannot open log file!"), TEXT("Error"), MB_OK | MB_ICONERROR);
        exit(0);
    }
    fprintf(gFILE, "WinMain()-> Program started successfully\n");

    wsprintf(szAppName, TEXT("%s"), gpszAppName);

    // WNDCLASSEX Initialization
    wndclass.cbSize        = sizeof(WNDCLASSEX);
    wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.hInstance     = hInstance;
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
    wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpszClassName = szAppName;
    wndclass.lpszMenuName  = NULL;
    wndclass.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));

    RegisterClassEx(&wndclass);

    // Create Window
    hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        szAppName,
        TEXT("Vulkan + Validation Layers"),
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

    // Initialize Vulkan
    vkResult = initialize();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "WinMain(): initialize() failed\n");
        DestroyWindow(hwnd);
        hwnd = NULL;
    } else {
        fprintf(gFILE, "WinMain(): initialize() succeeded\n");
    }

    ShowWindow(hwnd, iCmdShow);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    // Game loop
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

    // Uninitialize
    uninitialize();
    return((int)msg.wParam);
}

// ----------------------------------------------------------------------- //
//                       Windows Procedure
// ----------------------------------------------------------------------- //

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    void resize(int, int);

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
        resize(LOWORD(lParam), HIWORD(lParam));
        break;

    case WM_KEYDOWN:
        switch (LOWORD(wParam)) {
        case VK_ESCAPE:
            fprintf(gFILE, "WndProc() VK_ESCAPE-> Program ended.\n");
            fclose(gFILE);
            gFILE = NULL;
            DestroyWindow(hwnd);
            break;
        }
        break;

    case WM_CHAR:
        switch (LOWORD(wParam)) {
        case 'F':
        case 'f':
            if (gbFullscreen == FALSE) {
                ToggleFullscreen();
                gbFullscreen = TRUE;
                fprintf(gFILE, "WndProc() WM_CHAR(F)-> Fullscreen.\n");
            } else {
                ToggleFullscreen();
                gbFullscreen = FALSE;
                fprintf(gFILE, "WndProc() WM_CHAR(F)-> Windowed.\n");
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
    }

    return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}

// ----------------------------------------------------------------------- //
//                           Helper Functions
// ----------------------------------------------------------------------- //

void ToggleFullscreen(void)
{
    MONITORINFO mi = { sizeof(MONITORINFO) };

    if (gbFullscreen == FALSE) {
        dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
        if (dwStyle & WS_OVERLAPPEDWINDOW) {
            if (GetWindowPlacement(ghwnd, &wpPrev) &&
                GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi)) {
                SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(ghwnd,
                             HWND_TOP,
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
        SetWindowPos(ghwnd,
                     HWND_TOP,
                     0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    }
}

// For completeness
void resize(int width, int height)
{
    // Typically, you'd recreate your swapchain here if it is already initialized.
    // For simplicity, we do nothing special in this sample.
}

void update(void)
{
    // Update any animations or scene logic
}

// ----------------------------------------------------------------------- //
//      Vulkan-Related Functions (with Validation Layer Added)
// ----------------------------------------------------------------------- //

//
// Forward-declarations for all the helper functions in the initialization process.
//
static VkResult FillInstanceExtensionNames(void);
static VkResult FillInstanceLayerNames(void);
static VkResult CreateVulkanInstance(void);
static VkResult GetSupportedSurface(void);
static VkResult GetPhysicalDevice(void);
static VkResult PrintVulkanInfo(void);
static VkResult FillDeviceExtensionNames(void);
static VkResult CreateVulKanDevice(void);
static void    GetDeviceQueque(void);
static VkResult CreateSwapChain(VkBool32 vsync);
static VkResult CreateImagesAndImageViews(void);
static VkResult CreateCommandPool(void);
static VkResult CreateCommandBuffers(void);
static VkResult CreateRenderPass(void);
static VkResult CreateFramebuffers(void);
static VkResult CreateSemaphores(void);
static VkResult CreateFences(void);
static VkResult buildCommandBuffers(void);

// Main initialize()
VkResult initialize(void)
{
    VkResult vkResult = VK_SUCCESS;

    vkResult = CreateVulkanInstance();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): CreateVulkanInstance() failed\n");
        return vkResult;
    }

    // Load function pointers for debug messenger & create if the extension is available
    LoadDebugUtilsFunctionPointers(vkInstance);
    SetupDebugMessenger(vkInstance);

    vkResult = GetSupportedSurface();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): GetSupportedSurface() failed\n");
        return vkResult;
    }

    vkResult = GetPhysicalDevice();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): GetPhysicalDevice() failed\n");
        return vkResult;
    }

    vkResult = PrintVulkanInfo();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): PrintVulkanInfo() failed\n");
        return vkResult;
    }

    vkResult = CreateVulKanDevice();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): CreateVulKanDevice() failed\n");
        return vkResult;
    }

    GetDeviceQueque();

    vkResult = CreateSwapChain(VK_FALSE);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): CreateSwapChain() failed\n");
        // Return some error
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkResult = CreateImagesAndImageViews();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): CreateImagesAndImageViews() failed\n");
        return vkResult;
    }

    vkResult = CreateCommandPool();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): CreateCommandPool() failed\n");
        return vkResult;
    }

    vkResult = CreateCommandBuffers();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): CreateCommandBuffers() failed\n");
        return vkResult;
    }

    vkResult = CreateRenderPass();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): CreateRenderPass() failed\n");
        return vkResult;
    }

    vkResult = CreateFramebuffers();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): CreateFramebuffers() failed\n");
        return vkResult;
    }

    vkResult = CreateSemaphores();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): CreateSemaphores() failed\n");
        return vkResult;
    }

    vkResult = CreateFences();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): CreateFences() failed\n");
        return vkResult;
    }

    memset((void*)&vkClearColorValue, 0, sizeof(VkClearColorValue));
    vkClearColorValue.float32[0] = 0.0f;
    vkClearColorValue.float32[1] = 0.0f;
    vkClearColorValue.float32[2] = 1.0f;
    vkClearColorValue.float32[3] = 1.0f;

    vkResult = buildCommandBuffers();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "initialize(): buildCommandBuffers() failed\n");
        return vkResult;
    }

    bInitialized = TRUE;
    fprintf(gFILE, "********** initialize() complete **********\n");
    return vkResult;
}

VkResult display(void)
{
    if (!bInitialized) {
        return (VkResult)VK_FALSE;
    }

    VkResult vkResult;

    // Acquire index of next swapChainImage
    vkResult = vkAcquireNextImageKHR(vkDevice, vkSwapchainKHR, UINT64_MAX,
                                     vkSemaphore_BackBuffer, VK_NULL_HANDLE,
                                     &currentImageIndex);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "display(): vkAcquireNextImageKHR() failed with %d\n", vkResult);
        return vkResult;
    }

    // Wait on the fence for this swapchain image
    vkResult = vkWaitForFences(vkDevice, 1, &vkFence_array[currentImageIndex], VK_TRUE, UINT64_MAX);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "display(): vkWaitForFences() failed\n");
        return vkResult;
    }

    // Reset the fence so we can use it again next frame
    vkResult = vkResetFences(vkDevice, 1, &vkFence_array[currentImageIndex]);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "display(): vkResetFences() failed\n");
        return vkResult;
    }

    // Pipeline stage we wait on
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    // Submit info
    VkSubmitInfo vkSubmitInfo;
    memset(&vkSubmitInfo, 0, sizeof(vkSubmitInfo));
    vkSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vkSubmitInfo.waitSemaphoreCount   = 1;
    vkSubmitInfo.pWaitSemaphores      = &vkSemaphore_BackBuffer;
    vkSubmitInfo.pWaitDstStageMask    = &waitDstStageMask;
    vkSubmitInfo.commandBufferCount   = 1;
    vkSubmitInfo.pCommandBuffers      = &vkCommandBuffer_array[currentImageIndex];
    vkSubmitInfo.signalSemaphoreCount = 1;
    vkSubmitInfo.pSignalSemaphores    = &vkSemaphore_RenderComplete;

    vkResult = vkQueueSubmit(vkQueue, 1, &vkSubmitInfo, vkFence_array[currentImageIndex]);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "display(): vkQueueSubmit() failed with %d\n", vkResult);
        return vkResult;
    }

    // Present
    VkPresentInfoKHR vkPresentInfoKHR;
    memset(&vkPresentInfoKHR, 0, sizeof(vkPresentInfoKHR));
    vkPresentInfoKHR.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    vkPresentInfoKHR.swapchainCount     = 1;
    vkPresentInfoKHR.pSwapchains        = &vkSwapchainKHR;
    vkPresentInfoKHR.pImageIndices      = &currentImageIndex;
    vkPresentInfoKHR.waitSemaphoreCount = 1;
    vkPresentInfoKHR.pWaitSemaphores    = &vkSemaphore_RenderComplete;

    vkResult = vkQueuePresentKHR(vkQueue, &vkPresentInfoKHR);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "display(): vkQueuePresentKHR() failed with %d\n", vkResult);
        return vkResult;
    }

    return vkResult;
}

//
// Full Cleanup
//
void uninitialize(void)
{
    if (gbFullscreen == TRUE) {
        ToggleFullscreen();
        gbFullscreen = FALSE;
    }

    // Destroy the window if still around
    if (ghwnd) {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    // Begin Vulkan resource cleanup
    if (vkDevice) {
        vkDeviceWaitIdle(vkDevice);

        // Destroy fences
        if (vkFence_array) {
            for (uint32_t i = 0; i < swapchainImageCount; i++) {
                vkDestroyFence(vkDevice, vkFence_array[i], NULL);
            }
            free(vkFence_array);
            vkFence_array = NULL;
        }

        // Destroy semaphores
        if (vkSemaphore_RenderComplete) {
            vkDestroySemaphore(vkDevice, vkSemaphore_RenderComplete, NULL);
            vkSemaphore_RenderComplete = VK_NULL_HANDLE;
        }
        if (vkSemaphore_BackBuffer) {
            vkDestroySemaphore(vkDevice, vkSemaphore_BackBuffer, NULL);
            vkSemaphore_BackBuffer = VK_NULL_HANDLE;
        }

        // Destroy framebuffers
        if (vkFramebuffer_array) {
            for (uint32_t i = 0; i < swapchainImageCount; i++) {
                vkDestroyFramebuffer(vkDevice, vkFramebuffer_array[i], NULL);
            }
            free(vkFramebuffer_array);
            vkFramebuffer_array = NULL;
        }

        // Destroy renderpass
        if (vkRenderPass) {
            vkDestroyRenderPass(vkDevice, vkRenderPass, NULL);
            vkRenderPass = VK_NULL_HANDLE;
        }

        // Free command buffers
        if (vkCommandBuffer_array) {
            for (uint32_t i = 0; i < swapchainImageCount; i++) {
                vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &vkCommandBuffer_array[i]);
            }
            free(vkCommandBuffer_array);
            vkCommandBuffer_array = NULL;
        }

        // Destroy command pool
        if (vkCommandPool) {
            vkDestroyCommandPool(vkDevice, vkCommandPool, NULL);
            vkCommandPool = VK_NULL_HANDLE;
        }

        // Destroy image views
        if (swapChainImageView_array) {
            for (uint32_t i = 0; i < swapchainImageCount; i++) {
                vkDestroyImageView(vkDevice, swapChainImageView_array[i], NULL);
            }
            free(swapChainImageView_array);
            swapChainImageView_array = NULL;
        }

        // Free image array (swapchain images are not destroyed with vkDestroyImage because they are owned by swapchain)
        if (swapChainImage_array) {
            free(swapChainImage_array);
            swapChainImage_array = NULL;
        }

        // Destroy swapchain
        if (vkSwapchainKHR) {
            vkDestroySwapchainKHR(vkDevice, vkSwapchainKHR, NULL);
            vkSwapchainKHR = VK_NULL_HANDLE;
        }

        // Destroy device
        vkDestroyDevice(vkDevice, NULL);
        vkDevice = VK_NULL_HANDLE;
    }

    // Destroy surface
    if (vkSurfaceKHR) {
        vkDestroySurfaceKHR(vkInstance, vkSurfaceKHR, NULL);
        vkSurfaceKHR = VK_NULL_HANDLE;
    }

    // Destroy debug messenger if loaded
    if (gDebugUtilsMessenger && pfnDestroyDebugUtilsMessengerEXT) {
        pfnDestroyDebugUtilsMessengerEXT(vkInstance, gDebugUtilsMessenger, NULL);
        gDebugUtilsMessenger = VK_NULL_HANDLE;
    }

    // Destroy Vulkan instance
    if (vkInstance) {
        vkDestroyInstance(vkInstance, NULL);
        vkInstance = VK_NULL_HANDLE;
    }

    // Close file
    if (gFILE) {
        fprintf(gFILE, "uninitialize()-> Program ended successfully.\n");
        fclose(gFILE);
        gFILE = NULL;
    }
}

// ----------------------------------------------------------------------- //
//                    Validation Layer: Helpers
// ----------------------------------------------------------------------- //

/**
 * Enumerates all available layers, prints them, and if `VK_LAYER_KHRONOS_validation`
 * is found, adds it to our global enabledLayerNames_array.
 */
static VkResult FillInstanceLayerNames(void)
{
    uint32_t layerCount = 0;
    VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    if (result != VK_SUCCESS) {
        fprintf(gFILE, "FillInstanceLayerNames(): Could not get layer count\n");
        return result;
    }

    if (layerCount == 0) {
        fprintf(gFILE, "FillInstanceLayerNames(): No instance layers available!\n");
        // Not necessarily an error: we can proceed without validation.
        return VK_SUCCESS;
    }

    VkLayerProperties* layerProps = (VkLayerProperties*) malloc(sizeof(VkLayerProperties) * layerCount);
    if (!layerProps) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    result = vkEnumerateInstanceLayerProperties(&layerCount, layerProps);
    if (result != VK_SUCCESS) {
        free(layerProps);
        fprintf(gFILE, "FillInstanceLayerNames(): Could not enumerate layers\n");
        return result;
    }

    // Print them and look for "VK_LAYER_KHRONOS_validation"
    VkBool32 foundValidation = VK_FALSE;
    for (uint32_t i = 0; i < layerCount; i++) {
        fprintf(gFILE, "Available Layer: %s\n", layerProps[i].layerName);
        if (strcmp(layerProps[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            foundValidation = VK_TRUE;
        }
    }

    if (foundValidation) {
        enabledLayerNames_array[enabledLayerCount++] = "VK_LAYER_KHRONOS_validation";
        fprintf(gFILE, "Validation layer \"VK_LAYER_KHRONOS_validation\" found and enabled.\n");
    } else {
        fprintf(gFILE, "Validation layer VK_LAYER_KHRONOS_validation not found. Proceeding without validation.\n");
    }

    free(layerProps);
    return VK_SUCCESS;
}

// ----------------------------------------------------------------------- //
//                    Instance Extension: Helpers
// ----------------------------------------------------------------------- //

static VkResult FillInstanceExtensionNames(void)
{
    uint32_t instanceExtensionCount = 0;
    VkResult vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, NULL);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "FillInstanceExtensionNames(): Cannot enumerate instance extensions.\n");
        return vkResult;
    }

    if (instanceExtensionCount == 0) {
        fprintf(gFILE, "FillInstanceExtensionNames(): No instance extensions found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkExtensionProperties* extProps =
        (VkExtensionProperties*) malloc(sizeof(VkExtensionProperties) * instanceExtensionCount);
    if (!extProps) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, extProps);
    if (vkResult != VK_SUCCESS) {
        free(extProps);
        fprintf(gFILE, "FillInstanceExtensionNames(): vkEnumerateInstanceExtensionProperties() failed.\n");
        return vkResult;
    }

    // We'll look for 3 possible extensions:
    // 1) VK_KHR_surface
    // 2) VK_KHR_win32_surface
    // 3) VK_EXT_debug_utils (for validation output)

    VkBool32 foundSurface      = VK_FALSE;
    VkBool32 foundWin32Surface = VK_FALSE;
    VkBool32 foundDebugUtils   = VK_FALSE;

    for (uint32_t i = 0; i < instanceExtensionCount; i++) {
        fprintf(gFILE, "Instance Extension: %s\n", extProps[i].extensionName);

        if (!strcmp(extProps[i].extensionName, VK_KHR_SURFACE_EXTENSION_NAME)) {
            foundSurface = VK_TRUE;
        } else if (!strcmp(extProps[i].extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
            foundWin32Surface = VK_TRUE;
        } else if (!strcmp(extProps[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            foundDebugUtils = VK_TRUE;
        }
    }

    free(extProps);

    // Now enable them if found
    if (!foundSurface) {
        fprintf(gFILE, "Required extension VK_KHR_SURFACE_EXTENSION_NAME not found.\n");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_SURFACE_EXTENSION_NAME;

    if (!foundWin32Surface) {
        fprintf(gFILE, "Required extension VK_KHR_WIN32_SURFACE_EXTENSION_NAME not found.\n");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

    if (foundDebugUtils) {
        enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        fprintf(gFILE, "VK_EXT_debug_utils extension found and enabled.\n");
    }

    return VK_SUCCESS;
}

// ----------------------------------------------------------------------- //
//                  Create VkInstance (with validation)
// ----------------------------------------------------------------------- //

static VkResult CreateVulkanInstance(void)
{
    VkResult vkResult;

    // Gather instance extensions
    vkResult = FillInstanceExtensionNames();
    if (vkResult != VK_SUCCESS) {
        return vkResult;
    }

    // Gather validation layers if available
    vkResult = FillInstanceLayerNames();
    if (vkResult != VK_SUCCESS) {
        return vkResult;
    }

    // Application info
    VkApplicationInfo appInfo;
    memset(&appInfo, 0, sizeof(appInfo));
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext              = NULL;
    appInfo.pApplicationName   = gpszAppName;
    appInfo.applicationVersion = 1;
    appInfo.pEngineName        = gpszAppName;
    appInfo.engineVersion      = 1;
    // If your SDK is Vulkan 1.3 or 1.2, set accordingly. Example:
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    // Instance create info
    VkInstanceCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext            = NULL;
    createInfo.pApplicationInfo = &appInfo;
    // Extensions
    createInfo.enabledExtensionCount   = enabledInstanceExtensionsCount;
    createInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;
    // Layers
    createInfo.enabledLayerCount       = enabledLayerCount;
    createInfo.ppEnabledLayerNames     = enabledLayerNames_array;

    // Create the instance
    vkResult = vkCreateInstance(&createInfo, NULL, &vkInstance);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() failed. Code: %d\n", vkResult);
        return vkResult;
    }

    fprintf(gFILE, "Vulkan Instance created successfully.\n");
    return VK_SUCCESS;
}

// ----------------------------------------------------------------------- //
//                 Rest of your Vulkan Setup code
// ----------------------------------------------------------------------- //

static VkResult GetSupportedSurface(void)
{
    VkResult vkResult = VK_SUCCESS;

    VkWin32SurfaceCreateInfoKHR createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext     = NULL;
    createInfo.flags     = 0;
    createInfo.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    createInfo.hwnd      = ghwnd;

    vkResult = vkCreateWin32SurfaceKHR(vkInstance, &createInfo, NULL, &vkSurfaceKHR);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "GetSupportedSurface(): vkCreateWin32SurfaceKHR failed. Code: %d\n", vkResult);
        return vkResult;
    }

    fprintf(gFILE, "Presentation Surface created.\n");
    return vkResult;
}

static VkResult GetPhysicalDevice(void)
{
    VkResult vkResult;

    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, NULL);
    if (vkResult != VK_SUCCESS || physicalDeviceCount == 0) {
        fprintf(gFILE, "GetPhysicalDevice(): No physical devices found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkPhysicalDevice_array = (VkPhysicalDevice*) malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, vkPhysicalDevice_array);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "GetPhysicalDevice(): Unable to enumerate physical devices.\n");
        free(vkPhysicalDevice_array);
        vkPhysicalDevice_array = NULL;
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // We want a device that supports graphics + present on the same queue
    VkBool32 found = VK_FALSE;
    for (uint32_t i = 0; i < physicalDeviceCount && !found; i++) {
        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &queueCount, NULL);
        if (queueCount == 0) continue;

        VkQueueFamilyProperties* queueProps = 
            (VkQueueFamilyProperties*) malloc(sizeof(VkQueueFamilyProperties) * queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &queueCount, queueProps);

        VkBool32* isQueueSurfaceSupported = 
            (VkBool32*) malloc(sizeof(VkBool32) * queueCount);
        for (uint32_t j = 0; j < queueCount; j++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice_array[i], j, vkSurfaceKHR, &isQueueSurfaceSupported[j]);
        }

        for (uint32_t j = 0; j < queueCount; j++) {
            if ((queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                (isQueueSurfaceSupported[j] == VK_TRUE)) {
                vkPhysicalDevice_selected = vkPhysicalDevice_array[i];
                graphicsQuequeFamilyIndex_selected = j;
                found = VK_TRUE;
                break;
            }
        }

        free(isQueueSurfaceSupported);
        free(queueProps);
    }

    if (!found) {
        fprintf(gFILE, "GetPhysicalDevice(): Could not find suitable GPU with graphics + present.\n");
        free(vkPhysicalDevice_array);
        vkPhysicalDevice_array = NULL;
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Fill memory properties of selected device
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_selected, &vkPhysicalDeviceMemoryProperties);

    // We keep vkPhysicalDevice_array for PrintVulkanInfo(), then free it there.
    return VK_SUCCESS;
}

static VkResult PrintVulkanInfo(void)
{
    // Print info for all enumerated physical devices
    for (uint32_t i = 0; i < physicalDeviceCount; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(vkPhysicalDevice_array[i], &props);

        uint32_t major = VK_VERSION_MAJOR(props.apiVersion);
        uint32_t minor = VK_VERSION_MINOR(props.apiVersion);
        uint32_t patch = VK_VERSION_PATCH(props.apiVersion);

        fprintf(gFILE, "Device %u:\n", i);
        fprintf(gFILE, "  apiVersion  = %u.%u.%u\n", major, minor, patch);
        fprintf(gFILE, "  deviceName  = %s\n", props.deviceName);
        fprintf(gFILE, "  vendorID    = 0x%04x\n", props.vendorID);
        fprintf(gFILE, "  deviceID    = 0x%04x\n", props.deviceID);

        switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            fprintf(gFILE, "  deviceType  = Integrated GPU\n");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            fprintf(gFILE, "  deviceType  = Discrete GPU\n");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            fprintf(gFILE, "  deviceType  = Virtual GPU\n");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            fprintf(gFILE, "  deviceType  = CPU\n");
            break;
        default:
            fprintf(gFILE, "  deviceType  = Other\n");
            break;
        }
    }

    // Now we can free the global array
    if (vkPhysicalDevice_array) {
        free(vkPhysicalDevice_array);
        vkPhysicalDevice_array = NULL;
    }
    return VK_SUCCESS;
}

static VkResult FillDeviceExtensionNames(void)
{
    uint32_t deviceExtensionCount = 0;
    VkResult vkResult = vkEnumerateDeviceExtensionProperties(
        vkPhysicalDevice_selected, NULL, &deviceExtensionCount, NULL);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "FillDeviceExtensionNames(): Could not get device extension count.\n");
        return vkResult;
    }

    if (deviceExtensionCount == 0) {
        fprintf(gFILE, "FillDeviceExtensionNames(): No device extensions found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkExtensionProperties* extProps = 
        (VkExtensionProperties*) malloc(sizeof(VkExtensionProperties) * deviceExtensionCount);

    vkResult = vkEnumerateDeviceExtensionProperties(
        vkPhysicalDevice_selected, NULL, &deviceExtensionCount, extProps);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "FillDeviceExtensionNames(): Could not enumerate device extensions.\n");
        free(extProps);
        return vkResult;
    }

    VkBool32 foundSwapchain = VK_FALSE;
    for (uint32_t i = 0; i < deviceExtensionCount; i++) {
        fprintf(gFILE, "Device Extension: %s\n", extProps[i].extensionName);
        if (!strcmp(extProps[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            foundSwapchain = VK_TRUE;
        }
    }
    if (foundSwapchain) {
        enabledDeviceExtensionNames_array[enabledDeviceExtensionsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        fprintf(gFILE, "Found VK_KHR_swapchain extension.\n");
    } else {
        fprintf(gFILE, "Could not find VK_KHR_swapchain extension.\n");
        free(extProps);
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    free(extProps);
    return VK_SUCCESS;
}

static VkResult CreateVulKanDevice(void)
{
    VkResult vkResult = FillDeviceExtensionNames();
    if (vkResult != VK_SUCCESS) {
        return vkResult;
    }

    float queuePriority[] = { 1.0f };
    VkDeviceQueueCreateInfo queueCreateInfo;
    memset(&queueCreateInfo, 0, sizeof(queueCreateInfo));
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQuequeFamilyIndex_selected;
    queueCreateInfo.queueCount       = 1;
    queueCreateInfo.pQueuePriorities = queuePriority;

    VkDeviceCreateInfo deviceCreateInfo;
    memset(&deviceCreateInfo, 0, sizeof(deviceCreateInfo));
    deviceCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount    = 1;
    deviceCreateInfo.pQueueCreateInfos       = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount   = enabledDeviceExtensionsCount;
    deviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensionNames_array;
    // If we want the device-level validation layers too (less common nowadays),
    // we could set deviceCreateInfo.enabledLayerCount = enabledLayerCount; ...
    deviceCreateInfo.enabledLayerCount       = 0;
    deviceCreateInfo.ppEnabledLayerNames     = NULL;
    deviceCreateInfo.pEnabledFeatures        = NULL; // We could specify advanced features here

    vkResult = vkCreateDevice(vkPhysicalDevice_selected, &deviceCreateInfo, NULL, &vkDevice);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateVulKanDevice(): vkCreateDevice failed.\n");
        return vkResult;
    }

    fprintf(gFILE, "Logical Device created.\n");
    return VK_SUCCESS;
}

static void GetDeviceQueque(void)
{
    vkGetDeviceQueue(vkDevice, graphicsQuequeFamilyIndex_selected, 0, &vkQueue);
    if (!vkQueue) {
        fprintf(gFILE, "GetDeviceQueue(): Invalid queue handle.\n");
    } else {
        fprintf(gFILE, "GetDeviceQueue(): Queue acquired.\n");
    }
}

// Helper for CreateSwapChain
static VkResult getPhysicalDeviceSurfaceFormatAndColorSpace(void)
{
    uint32_t formatCount = 0;
    VkResult vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        vkPhysicalDevice_selected, vkSurfaceKHR, &formatCount, NULL);
    if (vkResult != VK_SUCCESS || formatCount == 0) {
        fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): No surface formats.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSurfaceFormatKHR* surfFormats =
        (VkSurfaceFormatKHR*) malloc(sizeof(VkSurfaceFormatKHR) * formatCount);
    vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        vkPhysicalDevice_selected, vkSurfaceKHR, &formatCount, surfFormats);
    if (vkResult != VK_SUCCESS) {
        free(surfFormats);
        return vkResult;
    }

    if ((formatCount == 1) && (surfFormats[0].format == VK_FORMAT_UNDEFINED)) {
        vkFormat_color = VK_FORMAT_B8G8R8A8_UNORM;
        vkColorSpaceKHR = surfFormats[0].colorSpace;
    } else {
        vkFormat_color     = surfFormats[0].format;
        vkColorSpaceKHR    = surfFormats[0].colorSpace;
    }

    free(surfFormats);
    return VK_SUCCESS;
}

static VkResult getPhysicalDevicePresentMode(void)
{
    uint32_t presentModeCount = 0;
    VkResult vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysicalDevice_selected, vkSurfaceKHR, &presentModeCount, NULL);
    if (vkResult != VK_SUCCESS || presentModeCount == 0) {
        fprintf(gFILE, "getPhysicalDevicePresentMode(): No present modes.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPresentModeKHR* presentModes =
        (VkPresentModeKHR*) malloc(sizeof(VkPresentModeKHR) * presentModeCount);

    vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysicalDevice_selected, vkSurfaceKHR, &presentModeCount, presentModes);
    if (vkResult != VK_SUCCESS) {
        free(presentModes);
        return vkResult;
    }

    // Default
    vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }

    free(presentModes);
    fprintf(gFILE, "Chosen present mode = %d\n", vkPresentModeKHR);
    return VK_SUCCESS;
}

static VkResult CreateSwapChain(VkBool32 vsync)
{
    VkResult vkResult;

    vkResult = getPhysicalDeviceSurfaceFormatAndColorSpace();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateSwapChain(): getPhysicalDeviceSurfaceFormatAndColorSpace() failed.\n");
        return vkResult;
    }

    VkSurfaceCapabilitiesKHR surfaceCaps;
    memset(&surfaceCaps, 0, sizeof(surfaceCaps));
    vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        vkPhysicalDevice_selected, vkSurfaceKHR, &surfaceCaps);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateSwapChain(): vkGetPhysicalDeviceSurfaceCapabilitiesKHR() failed.\n");
        return vkResult;
    }

    uint32_t desiredNumberOfSwapChainImages = surfaceCaps.minImageCount + 1;
    if ((surfaceCaps.maxImageCount > 0) && (desiredNumberOfSwapChainImages > surfaceCaps.maxImageCount)) {
        desiredNumberOfSwapChainImages = surfaceCaps.maxImageCount;
    }

    // Decide swapchain extent
    if (surfaceCaps.currentExtent.width != UINT32_MAX) {
        vkExtent2D_SwapChain = surfaceCaps.currentExtent;
    } else {
        vkExtent2D_SwapChain.width  = max(surfaceCaps.minImageExtent.width, 
                                          min(surfaceCaps.maxImageExtent.width, WIN_WIDTH));
        vkExtent2D_SwapChain.height = max(surfaceCaps.minImageExtent.height, 
                                          min(surfaceCaps.maxImageExtent.height, WIN_HEIGHT));
    }

    VkSurfaceTransformFlagBitsKHR transform;
    if (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        transform = surfaceCaps.currentTransform;
    }

    vkResult = getPhysicalDevicePresentMode();
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateSwapChain(): getPhysicalDevicePresentMode() failed.\n");
        return vkResult;
    }

    VkSwapchainCreateInfoKHR swapchainInfo;
    memset(&swapchainInfo, 0, sizeof(swapchainInfo));
    swapchainInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface          = vkSurfaceKHR;
    swapchainInfo.minImageCount    = desiredNumberOfSwapChainImages;
    swapchainInfo.imageFormat      = vkFormat_color;
    swapchainInfo.imageColorSpace  = vkColorSpaceKHR;
    swapchainInfo.imageExtent      = vkExtent2D_SwapChain;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform     = transform;
    swapchainInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode      = vkPresentModeKHR;
    swapchainInfo.clipped          = VK_TRUE;
    swapchainInfo.oldSwapchain     = VK_NULL_HANDLE;

    vkResult = vkCreateSwapchainKHR(vkDevice, &swapchainInfo, NULL, &vkSwapchainKHR);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateSwapChain(): vkCreateSwapchainKHR() failed.\n");
        return vkResult;
    }

    fprintf(gFILE, "Swapchain created.\n");
    return VK_SUCCESS;
}

static VkResult CreateImagesAndImageViews(void)
{
    VkResult vkResult;

    vkResult = vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, NULL);
    if (vkResult != VK_SUCCESS || swapchainImageCount == 0) {
        fprintf(gFILE, "CreateImagesAndImageViews(): No swapchain images.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    swapChainImage_array = (VkImage*) malloc(sizeof(VkImage) * swapchainImageCount);
    vkResult = vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapChainImage_array);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateImagesAndImageViews(): Could not get swapchain images.\n");
        return vkResult;
    }

    swapChainImageView_array = (VkImageView*) malloc(sizeof(VkImageView) * swapchainImageCount);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageViewCreateInfo viewInfo;
        memset(&viewInfo, 0, sizeof(viewInfo));
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = swapChainImage_array[i];
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = vkFormat_color;
        viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
        viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
        viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
        viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        vkResult = vkCreateImageView(vkDevice, &viewInfo, NULL, &swapChainImageView_array[i]);
        if (vkResult != VK_SUCCESS) {
            fprintf(gFILE, "CreateImagesAndImageViews(): vkCreateImageView failed at index %d\n", i);
            return vkResult;
        }
    }

    fprintf(gFILE, "Swapchain images and image-views created.\n");
    return VK_SUCCESS;
}

static VkResult CreateCommandPool(void)
{
    VkCommandPoolCreateInfo poolInfo;
    memset(&poolInfo, 0, sizeof(poolInfo));
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQuequeFamilyIndex_selected;

    VkResult vkResult = vkCreateCommandPool(vkDevice, &poolInfo, NULL, &vkCommandPool);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateCommandPool(): vkCreateCommandPool() failed.\n");
        return vkResult;
    }

    fprintf(gFILE, "Command Pool created.\n");
    return VK_SUCCESS;
}

static VkResult CreateCommandBuffers(void)
{
    vkCommandBuffer_array = (VkCommandBuffer*) malloc(sizeof(VkCommandBuffer) * swapchainImageCount);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkCommandBufferAllocateInfo allocInfo;
        memset(&allocInfo, 0, sizeof(allocInfo));
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = vkCommandPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkResult vkResult = vkAllocateCommandBuffers(vkDevice, &allocInfo, &vkCommandBuffer_array[i]);
        if (vkResult != VK_SUCCESS) {
            fprintf(gFILE, "CreateCommandBuffers(): vkAllocateCommandBuffers() failed at %d\n", i);
            return vkResult;
        }
    }

    fprintf(gFILE, "Command Buffers allocated.\n");
    return VK_SUCCESS;
}

static VkResult CreateRenderPass(void)
{
    // One color attachment
    VkAttachmentDescription attachmentDesc;
    memset(&attachmentDesc, 0, sizeof(attachmentDesc));
    attachmentDesc.format         = vkFormat_color;
    attachmentDesc.samples        = VK_SAMPLE_COUNT_1_BIT;
    attachmentDesc.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDesc.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDesc.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDesc.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef;
    memset(&colorRef, 0, sizeof(colorRef));
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass;
    memset(&subpass, 0, sizeof(subpass));
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;

    VkRenderPassCreateInfo renderPassInfo;
    memset(&renderPassInfo, 0, sizeof(renderPassInfo));
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &attachmentDesc;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;

    VkResult vkResult = vkCreateRenderPass(vkDevice, &renderPassInfo, NULL, &vkRenderPass);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateRenderPass(): vkCreateRenderPass() failed.\n");
        return vkResult;
    }

    fprintf(gFILE, "RenderPass created.\n");
    return VK_SUCCESS;
}

static VkResult CreateFramebuffers(void)
{
    vkFramebuffer_array = (VkFramebuffer*) malloc(sizeof(VkFramebuffer) * swapchainImageCount);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageView attachments[1] = { swapChainImageView_array[i] };

        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = vkRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = attachments;
        fbInfo.width           = vkExtent2D_SwapChain.width;
        fbInfo.height          = vkExtent2D_SwapChain.height;
        fbInfo.layers          = 1;

        VkResult vkResult = vkCreateFramebuffer(vkDevice, &fbInfo, NULL, &vkFramebuffer_array[i]);
        if (vkResult != VK_SUCCESS) {
            fprintf(gFILE, "CreateFramebuffers(): vkCreateFramebuffer() failed at %d\n", i);
            return vkResult;
        }
    }

    fprintf(gFILE, "Framebuffers created.\n");
    return VK_SUCCESS;
}

static VkResult CreateSemaphores(void)
{
    VkSemaphoreCreateInfo semInfo;
    memset(&semInfo, 0, sizeof(semInfo));
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkResult vkResult = vkCreateSemaphore(vkDevice, &semInfo, NULL, &vkSemaphore_BackBuffer);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateSemaphores(): vkCreateSemaphore failed for BackBuffer.\n");
        return vkResult;
    }

    vkResult = vkCreateSemaphore(vkDevice, &semInfo, NULL, &vkSemaphore_RenderComplete);
    if (vkResult != VK_SUCCESS) {
        fprintf(gFILE, "CreateSemaphores(): vkCreateSemaphore failed for RenderComplete.\n");
        return vkResult;
    }

    fprintf(gFILE, "Semaphores created.\n");
    return VK_SUCCESS;
}

static VkResult CreateFences(void)
{
    vkFence_array = (VkFence*) malloc(sizeof(VkFence) * swapchainImageCount);

    VkFenceCreateInfo fenceInfo;
    memset(&fenceInfo, 0, sizeof(fenceInfo));
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Start signaled so that we can use it on the first frame
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkResult vkResult = vkCreateFence(vkDevice, &fenceInfo, NULL, &vkFence_array[i]);
        if (vkResult != VK_SUCCESS) {
            fprintf(gFILE, "CreateFences(): vkCreateFence() failed at i = %d\n", i);
            return vkResult;
        }
    }

    fprintf(gFILE, "Fences created.\n");
    return VK_SUCCESS;
}

static VkResult buildCommandBuffers(void)
{
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkResult vkResult = vkResetCommandBuffer(vkCommandBuffer_array[i], 0);
        if (vkResult != VK_SUCCESS) {
            fprintf(gFILE, "buildCommandBuffers(): vkResetCommandBuffer failed at i=%d\n", i);
            return vkResult;
        }

        VkCommandBufferBeginInfo beginInfo;
        memset(&beginInfo, 0, sizeof(beginInfo));
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkResult = vkBeginCommandBuffer(vkCommandBuffer_array[i], &beginInfo);
        if (vkResult != VK_SUCCESS) {
            fprintf(gFILE, "buildCommandBuffers(): vkBeginCommandBuffer failed at i=%d\n", i);
            return vkResult;
        }

        VkClearValue clearValue;
        clearValue.color = vkClearColorValue;

        VkRenderPassBeginInfo rpBegin;
        memset(&rpBegin, 0, sizeof(rpBegin));
        rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass        = vkRenderPass;
        rpBegin.framebuffer       = vkFramebuffer_array[i];
        rpBegin.renderArea.offset.x = 0;
        rpBegin.renderArea.offset.y = 0;
        rpBegin.renderArea.extent.width  = vkExtent2D_SwapChain.width;
        rpBegin.renderArea.extent.height = vkExtent2D_SwapChain.height;
        rpBegin.clearValueCount   = 1;
        rpBegin.pClearValues      = &clearValue;

        vkCmdBeginRenderPass(vkCommandBuffer_array[i], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        // Here you would bind pipeline and issue draw calls, etc.

        vkCmdEndRenderPass(vkCommandBuffer_array[i]);

        vkResult = vkEndCommandBuffer(vkCommandBuffer_array[i]);
        if (vkResult != VK_SUCCESS) {
            fprintf(gFILE, "buildCommandBuffers(): vkEndCommandBuffer failed at i=%d\n", i);
            return vkResult;
        }
    }

    fprintf(gFILE, "Command Buffers recorded.\n");
    return VK_SUCCESS;
}
