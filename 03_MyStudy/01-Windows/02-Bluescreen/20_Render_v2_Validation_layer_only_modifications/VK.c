#include <stdio.h>        
#include <stdlib.h>   
#include <windows.h>  
#include <math.h>  

#include "VK.h"         
#define LOG_FILE (char*)"Log.txt" 

//Vulkan related header files
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h> 

#define WIN_WIDTH 800
#define WIN_HEIGHT 600

//Vulkan related libraries
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

// -------------------------------------------------------
// 1) Declare global validation layer arrays
// -------------------------------------------------------
static const char* gszRequestedInstanceLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};
static const uint32_t gRequestedInstanceLayerCount = 1;

// In modern Vulkan, device layers are deprecated in most drivers.
// If you still want to request them, you can do so similarly:
static const char* gszRequestedDeviceLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};
static const uint32_t gRequestedDeviceLayerCount = 1;

// -------------------------------------------------------
// Vulkan Globals
// -------------------------------------------------------

//Instance extension related variables
uint32_t enabledInstanceExtensionsCount = 0;
const char* enabledInstanceExtensionNames_array[2];

// If the validation layer is found and we want to enable it:
static uint32_t gEnabledInstanceLayerCount = 0;
static const char* gppEnabledInstanceLayerNames[1];

//Vulkan Instance
VkInstance vkInstance = VK_NULL_HANDLE;

//Vulkan Presentation Surface
VkSurfaceKHR vkSurfaceKHR = VK_NULL_HANDLE;

//Vulkan Physical device related global variables
VkPhysicalDevice vkPhysicalDevice_selected = VK_NULL_HANDLE;
uint32_t graphicsQuequeFamilyIndex_selected = UINT32_MAX;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties; 

uint32_t physicalDeviceCount = 0;
VkPhysicalDevice* vkPhysicalDevice_array = NULL; 

//Device extension related variables
uint32_t enabledDeviceExtensionsCount = 0;
const char* enabledDeviceExtensionNames_array[1];

// If the device validation layer is found and we want to enable it:
static uint32_t gEnabledDeviceLayerCount = 0;
static const char* gppEnabledDeviceLayerNames[1];

//Vulkan Device
VkDevice vkDevice = VK_NULL_HANDLE;

//Device Queue
VkQueue vkQueue = VK_NULL_HANDLE;

//Color Format & Color Space
VkFormat vkFormat_color = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR vkColorSpaceKHR = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

//Presentation Mode
VkPresentModeKHR vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;

//SwapChain
int winWidth = WIN_WIDTH;
int winHeight = WIN_HEIGHT;
VkSwapchainKHR vkSwapchainKHR = VK_NULL_HANDLE;
VkExtent2D vkExtent2D_SwapChain;

//Swapchain images and Image views
uint32_t swapchainImageCount = UINT32_MAX;
VkImage* swapChainImage_array = NULL;
VkImageView* swapChainImageView_array = NULL;

//Command Pool
VkCommandPool vkCommandPool = VK_NULL_HANDLE;

//Command Buffers
VkCommandBuffer* vkCommandBuffer_array = NULL;

//RenderPass
VkRenderPass vkRenderPass = VK_NULL_HANDLE;

//Framebuffers
VkFramebuffer* vkFramebuffer_array = NULL;

//Fences and Semaphores
VkSemaphore vkSemaphore_BackBuffer = VK_NULL_HANDLE;
VkSemaphore vkSemaphore_RenderComplete = VK_NULL_HANDLE;
VkFence* vkFence_array = NULL;

//Clear color
VkClearColorValue vkClearColorValue;

//Initialization complete?
BOOL bInitialized = FALSE;
uint32_t currentImageIndex = UINT32_MAX;

// Function declarations (forward):
VkResult initialize(void);
void uninitialize(void);
VkResult display(void);
void update(void);
void ToggleFullscreen(void);
void resize(int, int);

// Validation helper declarations
static VkBool32 CheckInstanceLayerSupport(void);
static VkBool32 CheckDeviceLayerSupport(void);

// Entry-Point Function
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

    // Log File
    gFILE = fopen(LOG_FILE, "w");
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

    // WNDCLASSEX Init
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

    // Register WNDCLASSEX
    RegisterClassEx(&wndclass);

    // Create Window
    hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
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

    // Initialization
    vkResult = initialize();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "WinMain(): initialize() function failed\n");
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

    // Game Loop
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

    // Uninitialization
    uninitialize(); 
    return ((int)msg.wParam);
}

// CALLBACK Function
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    switch (iMsg)
    {
    case WM_CREATE:
        memset((void*)&wpPrev, 0 , sizeof(WINDOWPLACEMENT));
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
    else {
        SetWindowPlacement(ghwnd, &wpPrev);
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(ghwnd, HWND_TOP, 
                     0, 0, 0, 0, 
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    }
}

// -------------------------------------------------------------------------
//                          VALIDATION LAYER SUPPORT
// -------------------------------------------------------------------------
static VkBool32 CheckInstanceLayerSupport(void)
{
    // 1) Enumerate available instance layers
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    if (layerCount == 0)
    {
        fprintf(gFILE, "No instance layers found on this system.\n");
        return VK_FALSE;
    }

    VkLayerProperties* layerProps = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layerProps);

    // 2) Check if VK_LAYER_KHRONOS_validation is present
    VkBool32 validationLayerFound = VK_FALSE;
    for (uint32_t i = 0; i < layerCount; i++)
    {
        // Compare name of each enumerated layer with "VK_LAYER_KHRONOS_validation"
        if (strcmp(layerProps[i].layerName, gszRequestedInstanceLayers[0]) == 0)
        {
            validationLayerFound = VK_TRUE;
            break;
        }
    }

    if (validationLayerFound == VK_TRUE)
    {
        fprintf(gFILE, "Instance validation layer %s found.\n", gszRequestedInstanceLayers[0]);
    }
    else
    {
        fprintf(gFILE, "Instance validation layer %s NOT found.\n", gszRequestedInstanceLayers[0]);
    }

    free(layerProps);
    return validationLayerFound;
}

static VkBool32 CheckDeviceLayerSupport(void)
{
    // Modern Vulkan typically ignores device layers,
    // but if you want to be thorough, you can check here:
    uint32_t layerCount = 0;
    vkEnumerateDeviceLayerProperties(vkPhysicalDevice_selected, &layerCount, NULL);
    if (layerCount == 0)
    {
        fprintf(gFILE, "No device layers found.\n");
        return VK_FALSE;
    }

    VkLayerProperties* layerProps = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * layerCount);
    vkEnumerateDeviceLayerProperties(vkPhysicalDevice_selected, &layerCount, layerProps);

    VkBool32 validationLayerFound = VK_FALSE;
    for (uint32_t i = 0; i < layerCount; i++)
    {
        if (strcmp(layerProps[i].layerName, gszRequestedDeviceLayers[0]) == 0)
        {
            validationLayerFound = VK_TRUE;
            break;
        }
    }

    free(layerProps);
    return validationLayerFound;
}

// -------------------------------------------------------------------------
//                               INIT / RESIZE / DISPLAY 
// -------------------------------------------------------------------------
VkResult initialize(void)
{
    VkResult CreateVulkanInstance(void);
    VkResult GetSupportedSurface(void);
    VkResult GetPhysicalDevice(void);
    VkResult PrintVulkanInfo(void);
    VkResult CreateVulKanDevice(void);
    void GetDeviceQueque(void);
    VkResult CreateSwapChain(VkBool32);
    VkResult CreateImagesAndImageViews(void);
    VkResult CreateCommandPool(void);
    VkResult CreateCommandBuffers(void);
    VkResult CreateRenderPass(void);
    VkResult CreateFramebuffers(void);
    VkResult CreateSemaphores(void);
    VkResult CreateFences(void);
    VkResult buildCommandBuffers(void);

    VkResult vkResult = VK_SUCCESS;

    vkResult = CreateVulkanInstance();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): CreateVulkanInstance() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateVulkanInstance() succedded\n");
    }

    vkResult = GetSupportedSurface();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): GetSupportedSurface() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): GetSupportedSurface() succedded\n");
    }

    vkResult = GetPhysicalDevice();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): GetPhysicalDevice() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): GetPhysicalDevice() succedded\n");
    }

    vkResult = PrintVulkanInfo();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): PrintVulkanInfo() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): PrintVulkanInfo() succedded\n");
    }

    vkResult = CreateVulKanDevice();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): CreateVulKanDevice() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateVulKanDevice() succedded\n");
    }

    GetDeviceQueque();

    vkResult = CreateSwapChain(VK_FALSE);
    if (vkResult != VK_SUCCESS)
    {
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gFILE, "initialize(): CreateSwapChain() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateSwapChain() succedded\n");
    }

    vkResult = CreateImagesAndImageViews();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): CreateImagesAndImageViews() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateImagesAndImageViews() succedded with SwapChain Image count as %d\n", swapchainImageCount);
    }

    vkResult = CreateCommandPool();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): CreateCommandPool() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateCommandPool() succedded\n");
    }

    vkResult = CreateCommandBuffers();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): CreateCommandBuffers() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateCommandBuffers() succedded\n");
    }

    vkResult = CreateRenderPass();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): CreateRenderPass() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateRenderPass() succedded\n");
    }

    vkResult = CreateFramebuffers();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): CreateFramebuffers() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateFramebuffers() succedded\n");
    }

    vkResult = CreateSemaphores();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): CreateSemaphores() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateSemaphores() succedded\n");
    }

    vkResult = CreateFences();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): CreateFences() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): CreateFences() succedded\n");
    }

    memset((void*)&vkClearColorValue, 0, sizeof(VkClearColorValue));
    vkClearColorValue.float32[0] = 0.0f;
    vkClearColorValue.float32[1] = 0.0f;
    vkClearColorValue.float32[2] = 1.0f;
    vkClearColorValue.float32[3] = 1.0f;

    vkResult = buildCommandBuffers();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "initialize(): buildCommandBuffers() function failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "initialize(): buildCommandBuffers() succedded\n");
    }

    bInitialized = TRUE;
    fprintf(gFILE, "************************* End of initialize ******************************\n");
    return vkResult;
}

void resize(int width, int height)
{
    // Code for handling resize if needed
}

VkResult display(void)
{
    VkResult vkResult = VK_SUCCESS;

    if(bInitialized == FALSE)
    {
        fprintf(gFILE, "display(): initialization not completed yet\n");
        return (VkResult)VK_FALSE;
    }

    vkResult = vkAcquireNextImageKHR(vkDevice, vkSwapchainKHR, UINT64_MAX, vkSemaphore_BackBuffer, VK_NULL_HANDLE, &currentImageIndex);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "display(): vkAcquireNextImageKHR() failed with error code %d\n", vkResult);
        return vkResult;
    }

    vkResult = vkWaitForFences(vkDevice, 1, &vkFence_array[currentImageIndex], VK_TRUE, UINT64_MAX);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "display(): vkWaitForFences() failed\n");
        return vkResult;
    }

    vkResult = vkResetFences(vkDevice, 1, &vkFence_array[currentImageIndex]);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "display(): vkResetFences() failed with error code %d\n", vkResult);
        return vkResult;
    }

    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo vkSubmitInfo;
    memset((void*)&vkSubmitInfo, 0, sizeof(VkSubmitInfo));
    vkSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vkSubmitInfo.pNext                = NULL;
    vkSubmitInfo.pWaitDstStageMask    = &waitDstStageMask;
    vkSubmitInfo.waitSemaphoreCount   = 1;
    vkSubmitInfo.pWaitSemaphores      = &vkSemaphore_BackBuffer;
    vkSubmitInfo.commandBufferCount   = 1;
    vkSubmitInfo.pCommandBuffers      = &vkCommandBuffer_array[currentImageIndex];
    vkSubmitInfo.signalSemaphoreCount = 1;
    vkSubmitInfo.pSignalSemaphores    = &vkSemaphore_RenderComplete;

    vkResult = vkQueueSubmit(vkQueue, 1, &vkSubmitInfo, vkFence_array[currentImageIndex]);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "display(): vkQueueSubmit() failed with error code %d\n", vkResult);
        return vkResult;
    }

    VkPresentInfoKHR vkPresentInfoKHR;
    memset((void*)&vkPresentInfoKHR, 0, sizeof(VkPresentInfoKHR));
    vkPresentInfoKHR.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    vkPresentInfoKHR.pNext              = NULL;
    vkPresentInfoKHR.swapchainCount     = 1;
    vkPresentInfoKHR.pSwapchains        = &vkSwapchainKHR;
    vkPresentInfoKHR.pImageIndices      = &currentImageIndex;
    vkPresentInfoKHR.waitSemaphoreCount = 1;
    vkPresentInfoKHR.pWaitSemaphores    = &vkSemaphore_RenderComplete;
    vkPresentInfoKHR.pResults           = NULL;

    vkResult = vkQueuePresentKHR(vkQueue, &vkPresentInfoKHR);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "display(): vkQueuePresentKHR() failed with error code %d\n", vkResult);
        return vkResult;
    }

    return vkResult;
}

void update(void)
{
    // Code
}

// -------------------------------------------------------------------------
//                UNINITIALIZE (including window destruction)
// -------------------------------------------------------------------------
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

        // Destroy fences
        for(uint32_t i = 0; i< swapchainImageCount; i++)
        {
            vkDestroyFence(vkDevice, vkFence_array[i], NULL);
            fprintf(gFILE, "uninitialize(): vkFence_array[%d] is freed\n", i);
        }
        if(vkFence_array)
        {
            free(vkFence_array);
            vkFence_array = NULL;
            fprintf(gFILE, "uninitialize(): vkFence_array is freed\n");
        }

        // Destroy semaphores
        if(vkSemaphore_RenderComplete)
        {
            vkDestroySemaphore(vkDevice, vkSemaphore_RenderComplete, NULL);
            vkSemaphore_RenderComplete = VK_NULL_HANDLE;
            fprintf(gFILE, "uninitialize(): vkSemaphore_RenderComplete is freed\n");
        }
        if(vkSemaphore_BackBuffer)
        {
            vkDestroySemaphore(vkDevice, vkSemaphore_BackBuffer, NULL);
            vkSemaphore_BackBuffer = VK_NULL_HANDLE;
            fprintf(gFILE, "uninitialize(): vkSemaphore_BackBuffer is freed\n");
        }

        // Destroy framebuffers
        for(uint32_t i =0; i < swapchainImageCount; i++)
        {
            vkDestroyFramebuffer(vkDevice, vkFramebuffer_array[i], NULL);
            fprintf(gFILE, "uninitialize(): vkDestroyFramebuffer() is done\n");
        }
        if(vkFramebuffer_array)
        {
            free(vkFramebuffer_array);
            vkFramebuffer_array = NULL;
            fprintf(gFILE, "uninitialize(): vkFramebuffer_array is freed\n");
        }

        // Destroy renderpass
        if(vkRenderPass)
        {
            vkDestroyRenderPass(vkDevice, vkRenderPass, NULL);
            vkRenderPass = VK_NULL_HANDLE;
            fprintf(gFILE, "uninitialize(): vkDestroyRenderPass() is done\n");
        }

        // Free command buffers
        for(uint32_t i =0; i < swapchainImageCount; i++)
        {
            vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &vkCommandBuffer_array[i]);
            fprintf(gFILE, "uninitialize(): vkFreeCommandBuffers() is done\n");
        }
        if(vkCommandBuffer_array)
        {
            free(vkCommandBuffer_array);
            vkCommandBuffer_array = NULL;
            fprintf(gFILE, "uninitialize(): vkCommandBuffer_array is freed\n");
        }

        // Destroy command pool
        if(vkCommandPool)
        {
            vkDestroyCommandPool(vkDevice, vkCommandPool, NULL);
            vkCommandPool = VK_NULL_HANDLE;
            fprintf(gFILE, "uninitialize(): vkDestroyCommandPool() is done\n");
        }

        // Destroy image views
        for(uint32_t i =0; i < swapchainImageCount; i++)
        {
            vkDestroyImageView(vkDevice, swapChainImageView_array[i], NULL);
            fprintf(gFILE, "uninitialize(): vkDestroyImageView() is done\n");
        }
        if(swapChainImageView_array)
        {
            free(swapChainImageView_array);
            swapChainImageView_array = NULL;
            fprintf(gFILE, "uninitialize(): swapChainImageView_array is freed\n");
        }
        // Destroy images array
        if(swapChainImage_array)
        {
            free(swapChainImage_array);
            swapChainImage_array = NULL;
            fprintf(gFILE, "uninitialize(): swapChainImage_array is freed\n");
        }

        // Destroy swapchain
        vkDestroySwapchainKHR(vkDevice, vkSwapchainKHR, NULL);
        vkSwapchainKHR = VK_NULL_HANDLE;
        fprintf(gFILE, "uninitialize(): vkDestroySwapchainKHR() is done\n");

        vkDestroyDevice(vkDevice, NULL);
        vkDevice = VK_NULL_HANDLE;
        fprintf(gFILE, "uninitialize(): vkDestroyDevice() is done\n");
    }

    if(vkSurfaceKHR)
    {
        vkDestroySurfaceKHR(vkInstance, vkSurfaceKHR, NULL);
        vkSurfaceKHR = VK_NULL_HANDLE;
        fprintf(gFILE, "uninitialize(): vkDestroySurfaceKHR() sucedded\n");
    }

    if(vkInstance)
    {
        vkDestroyInstance(vkInstance, NULL);
        vkInstance = VK_NULL_HANDLE;
        fprintf(gFILE, "uninitialize(): vkDestroyInstance() sucedded\n");
    }

    if (gFILE)
    {
        fprintf(gFILE, "uninitialize()-> Program ended successfully.\n");
        fclose(gFILE);
        gFILE = NULL;
    }
}

// -------------------------------------------------------------------------
//                      VULKAN: Create Instance (with Layers)
// -------------------------------------------------------------------------
VkResult CreateVulkanInstance(void)
{
    VkResult FillInstanceExtensionNames(void);
    VkResult vkResult = VK_SUCCESS;

    vkResult = FillInstanceExtensionNames();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateVulkanInstance(): FillInstanceExtensionNames() function failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateVulkanInstance(): FillInstanceExtensionNames() succedded\n");
    }

    // ---------------------------------
    // Check if our validation layer is available
    // ---------------------------------
    VkBool32 validationAvailable = CheckInstanceLayerSupport();
    if (validationAvailable == VK_TRUE)
    {
        gEnabledInstanceLayerCount = 1; // enable
        gppEnabledInstanceLayerNames[0] = gszRequestedInstanceLayers[0];
    }
    else
    {
        gEnabledInstanceLayerCount = 0; // fallback
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

    // Set extension info
    vkInstanceCreateInfo.enabledExtensionCount   = enabledInstanceExtensionsCount;
    vkInstanceCreateInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;

    // Finally, set the layers (if available)
    vkInstanceCreateInfo.enabledLayerCount   = gEnabledInstanceLayerCount;
    vkInstanceCreateInfo.ppEnabledLayerNames = gppEnabledInstanceLayerNames;

    vkResult = vkCreateInstance(&vkInstanceCreateInfo, NULL, &vkInstance);
    if (vkResult == VK_ERROR_INCOMPATIBLE_DRIVER)
    {
        fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() failed due to incompatible driver.\n");
        return vkResult;
    }
    else if (vkResult == VK_ERROR_EXTENSION_NOT_PRESENT)
    {
        fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() failed due to extension not present.\n");
        return vkResult;
    }
    else if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() failed with error code %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() succedded\n");
    }

    return vkResult;
}

VkResult FillInstanceExtensionNames(void)
{
    VkResult vkResult = VK_SUCCESS;
    uint32_t instanceExtensionCount = 0;

    vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, NULL);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): vkEnumerateInstanceExtensionProperties() first call failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): first call succeeded, found %u instance extensions\n", instanceExtensionCount);
    }

    VkExtensionProperties* vkExtensionProperties_array =
        (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * instanceExtensionCount);
    vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, vkExtensionProperties_array);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): second call to vkEnumerateInstanceExtensionProperties() failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): second call succeeded\n");
    }

    char** instanceExtensionNames_array = 
        (char**)malloc(sizeof(char*) * instanceExtensionCount);
    for (uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        instanceExtensionNames_array[i] = (char*)malloc(strlen(vkExtensionProperties_array[i].extensionName) + 1);
        strcpy(instanceExtensionNames_array[i], vkExtensionProperties_array[i].extensionName);
        fprintf(gFILE, "FillInstanceExtensionNames(): Found Vulkan Instance Extension = %s\n",
                instanceExtensionNames_array[i]);
    }

    free(vkExtensionProperties_array);
    vkExtensionProperties_array = NULL;

    VkBool32 vulkanSurfaceExtensionFound     = VK_FALSE;
    VkBool32 vulkanWin32SurfaceExtensionFound= VK_FALSE;

    for (uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        if (strcmp(instanceExtensionNames_array[i], VK_KHR_SURFACE_EXTENSION_NAME) == 0)
        {
            vulkanSurfaceExtensionFound = VK_TRUE;
            enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
        }
        if (strcmp(instanceExtensionNames_array[i], VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0)
        {
            vulkanWin32SurfaceExtensionFound = VK_TRUE;
            enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
        }
    }

    for (uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        free(instanceExtensionNames_array[i]);
    }
    free(instanceExtensionNames_array);

    if (vulkanSurfaceExtensionFound == VK_FALSE)
    {
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_SURFACE_EXTENSION_NAME not found\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_SURFACE_EXTENSION_NAME found\n");
    }

    if (vulkanWin32SurfaceExtensionFound == VK_FALSE)
    {
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_WIN32_SURFACE_EXTENSION_NAME not found\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_WIN32_SURFACE_EXTENSION_NAME found\n");
    }

    for (uint32_t i = 0; i < enabledInstanceExtensionsCount; i++)
    {
        fprintf(gFILE, "FillInstanceExtensionNames(): Enabled Vulkan Instance Extension = %s\n",
                enabledInstanceExtensionNames_array[i]);
    }

    return vkResult;
}

VkResult GetSupportedSurface(void)
{
    VkResult vkResult = VK_SUCCESS;
    VkWin32SurfaceCreateInfoKHR vkWin32SurfaceCreateInfoKHR;
    memset((void*)&vkWin32SurfaceCreateInfoKHR, 0, sizeof(VkWin32SurfaceCreateInfoKHR));

    vkWin32SurfaceCreateInfoKHR.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    vkWin32SurfaceCreateInfoKHR.pNext     = NULL;
    vkWin32SurfaceCreateInfoKHR.flags     = 0;
    vkWin32SurfaceCreateInfoKHR.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    vkWin32SurfaceCreateInfoKHR.hwnd      = ghwnd;

    vkResult = vkCreateWin32SurfaceKHR(vkInstance, &vkWin32SurfaceCreateInfoKHR, NULL, &vkSurfaceKHR);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "GetSupportedSurface(): vkCreateWin32SurfaceKHR() failed with error code %d\n", vkResult);
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
        fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() first call failed with code %d\n", vkResult);
        return vkResult;
    }
    else if (physicalDeviceCount == 0)
    {
        fprintf(gFILE, "GetPhysicalDevice(): 0 physical devices found.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() first call succedded\n");
    }

    vkPhysicalDevice_array = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, vkPhysicalDevice_array);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() second call failed with %d\n", vkResult);
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() second call succedded\n");
    }

    VkBool32 bFound = VK_FALSE;
    for(uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        uint32_t queueCount = UINT32_MAX;
        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &queueCount, NULL);

        VkQueueFamilyProperties* vkQueueFamilyProperties_array =
            (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties)*queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &queueCount, vkQueueFamilyProperties_array);

        VkBool32* isQuequeSurfaceSupported_array = 
            (VkBool32*)malloc(sizeof(VkBool32)*queueCount);

        for(uint32_t j=0; j<queueCount; j++)
        {
            vkResult = vkGetPhysicalDeviceSurfaceSupportKHR(
                vkPhysicalDevice_array[i], j, vkSurfaceKHR,
                &isQuequeSurfaceSupported_array[j]);
        }

        for(uint32_t j=0; j<queueCount; j++)
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
        {
            break;
        }
    }

    if(bFound == VK_TRUE)
    {
        fprintf(gFILE, "GetPhysicalDevice(): selected required physical device w/ graphics.\n");
    }
    else
    {
        fprintf(gFILE, "GetPhysicalDevice(): no suitable device found.\n");
        if(vkPhysicalDevice_array)
        {
            free(vkPhysicalDevice_array);
            vkPhysicalDevice_array = NULL;
        }
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }

    memset((void*)&vkPhysicalDeviceMemoryProperties, 0, sizeof(VkPhysicalDeviceMemoryProperties));
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_selected, &vkPhysicalDeviceMemoryProperties);

    VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures;
    memset((void*)&vkPhysicalDeviceFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
    vkGetPhysicalDeviceFeatures(vkPhysicalDevice_selected, &vkPhysicalDeviceFeatures);

    if(vkPhysicalDeviceFeatures.tessellationShader)
    {
        fprintf(gFILE, "GetPhysicalDevice(): device supports tessellation shader.\n");
    }
    else
    {
        fprintf(gFILE, "GetPhysicalDevice(): device does not support tessellation shader.\n");
    }

    if(vkPhysicalDeviceFeatures.geometryShader)
    {
        fprintf(gFILE, "GetPhysicalDevice(): device supports geometry shader.\n");
    }
    else
    {
        fprintf(gFILE, "GetPhysicalDevice(): device does not support geometry shader.\n");
    }

    return vkResult;
}

VkResult PrintVulkanInfo(void)
{
    VkResult vkResult = VK_SUCCESS;

    fprintf(gFILE, "************************* Shree Ganesha******************************\n");

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
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                fprintf(gFILE,"deviceType = Other\n");
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

// -------------------------------------------------------------------------
//             DEVICE EXTENSIONS + (Optional) Validation Layers
// -------------------------------------------------------------------------
VkResult FillDeviceExtensionNames(void)
{
    VkResult vkResult = VK_SUCCESS;
    uint32_t deviceExtensionCount = 0;

    vkResult = vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected, NULL, &deviceExtensionCount, NULL);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "FillDeviceExtensionNames(): first call to vkEnumerateDeviceExtensionProperties() failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "FillDeviceExtensionNames(): found %u device extensions.\n", deviceExtensionCount);
    }

    VkExtensionProperties* vkExtensionProperties_array =
        (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*deviceExtensionCount);

    vkResult = vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected, NULL, &deviceExtensionCount,
                                                    vkExtensionProperties_array);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "FillDeviceExtensionNames(): second call to vkEnumerateDeviceExtensionProperties() failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "FillDeviceExtensionNames(): second call succeeded.\n");
    }

    char** deviceExtensionNames_array = (char**)malloc(sizeof(char*) * deviceExtensionCount);

    for(uint32_t i=0; i<deviceExtensionCount; i++)
    {
        deviceExtensionNames_array[i] =
            (char*)malloc(strlen(vkExtensionProperties_array[i].extensionName)+1);
        strcpy(deviceExtensionNames_array[i], vkExtensionProperties_array[i].extensionName);
        fprintf(gFILE, "FillDeviceExtensionNames(): Vulkan Device Extension = %s\n", 
                deviceExtensionNames_array[i]);
    }

    VkBool32 vulkanSwapchainExtensionFound = VK_FALSE;
    for(uint32_t i=0; i<deviceExtensionCount; i++)
    {
        if (strcmp(deviceExtensionNames_array[i], VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            vulkanSwapchainExtensionFound = VK_TRUE;
            enabledDeviceExtensionsCount = 1;
            enabledDeviceExtensionNames_array[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            break;
        }
    }

    for(uint32_t i=0; i<deviceExtensionCount; i++)
    {
        free(deviceExtensionNames_array[i]);
    }
    free(deviceExtensionNames_array);
    free(vkExtensionProperties_array);

    if(vulkanSwapchainExtensionFound == VK_FALSE)
    {
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gFILE, "FillDeviceExtensionNames(): VK_KHR_SWAPCHAIN_EXTENSION_NAME not found.\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "FillDeviceExtensionNames(): VK_KHR_SWAPCHAIN_EXTENSION_NAME found.\n");
    }

    for(uint32_t i=0; i<enabledDeviceExtensionsCount; i++)
    {
        fprintf(gFILE, "FillDeviceExtensionNames(): Enabled Device Extension = %s\n",
                enabledDeviceExtensionNames_array[i]);
    }

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
    else
    {
        fprintf(gFILE, "CreateVulKanDevice(): FillDeviceExtensionNames() succedded\n");
    }

    // ---------------------------------
    // (Optional) Check for device validation layer
    // ---------------------------------
    VkBool32 devValidationAvailable = CheckDeviceLayerSupport();
    if (devValidationAvailable == VK_TRUE)
    {
        gEnabledDeviceLayerCount = gRequestedDeviceLayerCount;
        gppEnabledDeviceLayerNames[0] = gszRequestedDeviceLayers[0];
        fprintf(gFILE, "Device validation layer %s found.\n", gszRequestedDeviceLayers[0]);
    }
    else
    {
        gEnabledDeviceLayerCount = 0;
        fprintf(gFILE, "Device validation layer %s NOT found. Ignoring.\n", gszRequestedDeviceLayers[0]);
    }

    float queuePriorities[1] = {1.0f};
    VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo;
    memset(&vkDeviceQueueCreateInfo, 0, sizeof(VkDeviceQueueCreateInfo));
    vkDeviceQueueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    vkDeviceQueueCreateInfo.pNext            = NULL;
    vkDeviceQueueCreateInfo.flags            = 0;
    vkDeviceQueueCreateInfo.queueFamilyIndex = graphicsQuequeFamilyIndex_selected;
    vkDeviceQueueCreateInfo.queueCount       = 1;
    vkDeviceQueueCreateInfo.pQueuePriorities = queuePriorities;

    VkDeviceCreateInfo vkDeviceCreateInfo;
    memset(&vkDeviceCreateInfo, 0, sizeof(VkDeviceCreateInfo));
    vkDeviceCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    vkDeviceCreateInfo.pNext                   = NULL;
    vkDeviceCreateInfo.flags                   = 0;
    vkDeviceCreateInfo.enabledExtensionCount   = enabledDeviceExtensionsCount;
    vkDeviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensionNames_array;

    // For older Vulkan versions, ignoring device-level layers might still be fine,
    // but if you want them:
    vkDeviceCreateInfo.enabledLayerCount   = gEnabledDeviceLayerCount;
    vkDeviceCreateInfo.ppEnabledLayerNames = (gEnabledDeviceLayerCount > 0) ?
                                             gppEnabledDeviceLayerNames : NULL;

    vkDeviceCreateInfo.pEnabledFeatures        = NULL;
    vkDeviceCreateInfo.queueCreateInfoCount    = 1;
    vkDeviceCreateInfo.pQueueCreateInfos       = &vkDeviceQueueCreateInfo;

    vkResult = vkCreateDevice(vkPhysicalDevice_selected, &vkDeviceCreateInfo, NULL, &vkDevice);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateVulKanDevice(): vkCreateDevice() failed.\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateVulKanDevice(): vkCreateDevice() succedded\n");
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
    else
    {
        fprintf(gFILE, "GetDeviceQueque(): vkGetDeviceQueue() succedded\n");
    }
}

// -------------------------------------------------------------------------
//                            SWAPCHAIN CREATION
// -------------------------------------------------------------------------
VkResult getPhysicalDeviceSurfaceFormatAndColorSpace(void)
{
    VkResult vkResult = VK_SUCCESS;
    uint32_t FormatCount = 0;
    vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_selected, vkSurfaceKHR, &FormatCount, NULL);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): first call failed\n");
        return vkResult;
    }
    else if(FormatCount == 0)
    {
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): FormatCount=0\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): first call succedded\n");
    }

    VkSurfaceFormatKHR* vkSurfaceFormatKHR_array = 
        (VkSurfaceFormatKHR*)malloc(FormatCount*sizeof(VkSurfaceFormatKHR));
    vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_selected, vkSurfaceKHR,
                                                    &FormatCount, vkSurfaceFormatKHR_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): second call failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): second call succedded\n");
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
    vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_selected,
                    vkSurfaceKHR, &presentModeCount, NULL);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "getPhysicalDevicePresentMode(): first call failed\n");
        return vkResult;
    }
    else if(presentModeCount==0)
    {
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gFILE, "getPhysicalDevicePresentMode(): presentModeCount=0\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "getPhysicalDevicePresentMode(): first call succedded\n");
    }

    VkPresentModeKHR* vkPresentModeKHR_array = 
        (VkPresentModeKHR*)malloc(presentModeCount*sizeof(VkPresentModeKHR));
    vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_selected,
                    vkSurfaceKHR, &presentModeCount, vkPresentModeKHR_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "getPhysicalDevicePresentMode(): second call failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "getPhysicalDevicePresentMode(): second call succedded\n");
    }

    vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
    for(uint32_t i=0; i<presentModeCount; i++)
    {
        if(vkPresentModeKHR_array[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    fprintf(gFILE, "getPhysicalDevicePresentMode(): vkPresentModeKHR = %d\n", vkPresentModeKHR);

    free(vkPresentModeKHR_array);
    return vkResult;
}

VkResult CreateSwapChain(VkBool32 vsync)
{
    VkResult vkResult = getPhysicalDeviceSurfaceFormatAndColorSpace();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateSwapChain(): getPhysicalDeviceSurfaceFormatAndColorSpace() failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateSwapChain(): getPhysicalDeviceSurfaceFormatAndColorSpace() succedded\n");
    }

    VkSurfaceCapabilitiesKHR vkSurfaceCapabilitiesKHR;
    memset((void*)&vkSurfaceCapabilitiesKHR, 0, sizeof(VkSurfaceCapabilitiesKHR));
    vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_selected, vkSurfaceKHR,
                                                         &vkSurfaceCapabilitiesKHR);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateSwapChain(): vkGetPhysicalDeviceSurfaceCapabilitiesKHR() failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateSwapChain(): vkGetPhysicalDeviceSurfaceCapabilitiesKHR() succedded\n");
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
        vkExtent2D_SwapChain.width  = (uint32_t)winWidth;
        vkExtent2D_SwapChain.height = (uint32_t)winHeight;

        if(vkExtent2D_SwapChain.width < vkSurfaceCapabilitiesKHR.minImageExtent.width)
            vkExtent2D_SwapChain.width = vkSurfaceCapabilitiesKHR.minImageExtent.width;
        else if(vkExtent2D_SwapChain.width > vkSurfaceCapabilitiesKHR.maxImageExtent.width)
            vkExtent2D_SwapChain.width = vkSurfaceCapabilitiesKHR.maxImageExtent.width;

        if(vkExtent2D_SwapChain.height < vkSurfaceCapabilitiesKHR.minImageExtent.height)
            vkExtent2D_SwapChain.height = vkSurfaceCapabilitiesKHR.minImageExtent.height;
        else if(vkExtent2D_SwapChain.height > vkSurfaceCapabilitiesKHR.maxImageExtent.height)
            vkExtent2D_SwapChain.height = vkSurfaceCapabilitiesKHR.maxImageExtent.height;
    }

    VkSurfaceTransformFlagBitsKHR vkSurfaceTransformFlagBitsKHR;
    if(vkSurfaceCapabilitiesKHR.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        vkSurfaceTransformFlagBitsKHR = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        vkSurfaceTransformFlagBitsKHR = vkSurfaceCapabilitiesKHR.currentTransform;
    }

    vkResult = getPhysicalDevicePresentMode();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateSwapChain(): getPhysicalDevicePresentMode() failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateSwapChain(): getPhysicalDevicePresentMode() succedded\n");
    }

    VkSwapchainCreateInfoKHR vkSwapchainCreateInfoKHR;
    memset((void*)&vkSwapchainCreateInfoKHR, 0, sizeof(VkSwapchainCreateInfoKHR));
    vkSwapchainCreateInfoKHR.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    vkSwapchainCreateInfoKHR.pNext            = NULL;
    vkSwapchainCreateInfoKHR.flags            = 0;
    vkSwapchainCreateInfoKHR.surface          = vkSurfaceKHR;
    vkSwapchainCreateInfoKHR.minImageCount    = desiredNumerOfSwapChainImages;
    vkSwapchainCreateInfoKHR.imageFormat      = vkFormat_color;
    vkSwapchainCreateInfoKHR.imageColorSpace  = vkColorSpaceKHR;
    vkSwapchainCreateInfoKHR.imageExtent.width  = vkExtent2D_SwapChain.width;
    vkSwapchainCreateInfoKHR.imageExtent.height = vkExtent2D_SwapChain.height;
    vkSwapchainCreateInfoKHR.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    vkSwapchainCreateInfoKHR.preTransform     = vkSurfaceTransformFlagBitsKHR;
    vkSwapchainCreateInfoKHR.imageArrayLayers = 1;
    vkSwapchainCreateInfoKHR.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkSwapchainCreateInfoKHR.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    vkSwapchainCreateInfoKHR.presentMode      = vkPresentModeKHR;
    vkSwapchainCreateInfoKHR.clipped          = VK_TRUE;
    vkSwapchainCreateInfoKHR.oldSwapchain     = VK_NULL_HANDLE;

    vkResult = vkCreateSwapchainKHR(vkDevice, &vkSwapchainCreateInfoKHR, NULL, &vkSwapchainKHR);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateSwapChain(): vkCreateSwapchainKHR() failed\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateSwapChain(): vkCreateSwapchainKHR() succedded\n");
    }

    return vkResult;
}

VkResult CreateImagesAndImageViews(void)
{
    VkResult vkResult = VK_SUCCESS;

    vkResult = vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, NULL);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateImagesAndImageViews(): first call vkGetSwapchainImagesKHR() failed.\n");
        return vkResult;
    }
    else if(swapchainImageCount == 0)
    {
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gFILE, "CreateImagesAndImageViews(): swapchainImageCount=0\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateImagesAndImageViews(): first call success => count=%d\n", swapchainImageCount);
    }

    swapChainImage_array = (VkImage*)malloc(sizeof(VkImage)*swapchainImageCount);
    vkResult = vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapChainImage_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateImagesAndImageViews(): second call vkGetSwapchainImagesKHR() failed.\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateImagesAndImageViews(): second call success => count=%d\n", swapchainImageCount);
    }

    swapChainImageView_array = (VkImageView*)malloc(sizeof(VkImageView)*swapchainImageCount);

    VkImageViewCreateInfo vkImageViewCreateInfo;
    memset((void*)&vkImageViewCreateInfo, 0, sizeof(VkImageViewCreateInfo));
    vkImageViewCreateInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vkImageViewCreateInfo.pNext            = NULL;
    vkImageViewCreateInfo.flags            = 0;
    vkImageViewCreateInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    vkImageViewCreateInfo.format           = vkFormat_color;
    vkImageViewCreateInfo.components.r     = VK_COMPONENT_SWIZZLE_R;
    vkImageViewCreateInfo.components.g     = VK_COMPONENT_SWIZZLE_G;
    vkImageViewCreateInfo.components.b     = VK_COMPONENT_SWIZZLE_B;
    vkImageViewCreateInfo.components.a     = VK_COMPONENT_SWIZZLE_A;
    vkImageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    vkImageViewCreateInfo.subresourceRange.levelCount     = 1;
    vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    vkImageViewCreateInfo.subresourceRange.layerCount     = 1;

    for(uint32_t i=0; i<swapchainImageCount; i++)
    {
        vkImageViewCreateInfo.image = swapChainImage_array[i];
        vkResult = vkCreateImageView(vkDevice, &vkImageViewCreateInfo, NULL, &swapChainImageView_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateImagesAndImageViews(): vkCreateImageView() failed at i=%d\n", i);
            return vkResult;
        }
        else
        {
            fprintf(gFILE, "CreateImagesAndImageViews(): vkCreateImageView() success at i=%d\n", i);
        }
    }

    return vkResult;
}

VkResult CreateCommandPool()
{
    VkResult vkResult = VK_SUCCESS;
    VkCommandPoolCreateInfo vkCommandPoolCreateInfo;
    memset((void*)&vkCommandPoolCreateInfo, 0, sizeof(VkCommandPoolCreateInfo));
    vkCommandPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    vkCommandPoolCreateInfo.pNext            = NULL;
    vkCommandPoolCreateInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCommandPoolCreateInfo.queueFamilyIndex = graphicsQuequeFamilyIndex_selected;

    vkResult = vkCreateCommandPool(vkDevice, &vkCommandPoolCreateInfo, NULL, &vkCommandPool);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateCommandPool(): vkCreateCommandPool() failed.\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateCommandPool(): vkCreateCommandPool() success.\n");
    }

    return vkResult;
}

VkResult CreateCommandBuffers(void)
{
    VkResult vkResult = VK_SUCCESS;
    VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo;
    memset((void*)&vkCommandBufferAllocateInfo, 0, sizeof(VkCommandBufferAllocateInfo));
    vkCommandBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    vkCommandBufferAllocateInfo.pNext              = NULL;
    vkCommandBufferAllocateInfo.commandPool        = vkCommandPool;
    vkCommandBufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkCommandBufferAllocateInfo.commandBufferCount = 1;

    vkCommandBuffer_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*swapchainImageCount);

    for(uint32_t i=0; i<swapchainImageCount; i++)
    {
        vkResult = vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, &vkCommandBuffer_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateCommandBuffers(): vkAllocateCommandBuffers() failed at i=%d\n", i);
            return vkResult;
        }
        else
        {
            fprintf(gFILE, "CreateCommandBuffers(): success at i=%d\n", i);
        }
    }
    return vkResult;
}

VkResult CreateRenderPass(void)
{
    VkResult vkResult = VK_SUCCESS;

    VkAttachmentDescription vkAttachmentDescription_array[1];
    memset(vkAttachmentDescription_array, 0, sizeof(vkAttachmentDescription_array));

    vkAttachmentDescription_array[0].flags          = 0;
    vkAttachmentDescription_array[0].format         = vkFormat_color;
    vkAttachmentDescription_array[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    vkAttachmentDescription_array[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    vkAttachmentDescription_array[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    vkAttachmentDescription_array[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    vkAttachmentDescription_array[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    vkAttachmentDescription_array[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    vkAttachmentDescription_array[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference vkAttachmentReference;
    memset(&vkAttachmentReference, 0, sizeof(vkAttachmentReference));
    vkAttachmentReference.attachment = 0; 
    vkAttachmentReference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription vkSubpassDescription;
    memset(&vkSubpassDescription, 0, sizeof(vkSubpassDescription));
    vkSubpassDescription.flags                   = 0;
    vkSubpassDescription.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkSubpassDescription.inputAttachmentCount    = 0;
    vkSubpassDescription.pInputAttachments       = NULL;
    vkSubpassDescription.colorAttachmentCount    = 1;
    vkSubpassDescription.pColorAttachments       = &vkAttachmentReference;
    vkSubpassDescription.pResolveAttachments     = NULL;
    vkSubpassDescription.pDepthStencilAttachment = NULL;
    vkSubpassDescription.preserveAttachmentCount = 0;
    vkSubpassDescription.pPreserveAttachments    = NULL;

    VkRenderPassCreateInfo vkRenderPassCreateInfo;
    memset(&vkRenderPassCreateInfo, 0, sizeof(vkRenderPassCreateInfo));
    vkRenderPassCreateInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    vkRenderPassCreateInfo.pNext           = NULL;
    vkRenderPassCreateInfo.flags           = 0;
    vkRenderPassCreateInfo.attachmentCount = 1;
    vkRenderPassCreateInfo.pAttachments    = vkAttachmentDescription_array;
    vkRenderPassCreateInfo.subpassCount    = 1;
    vkRenderPassCreateInfo.pSubpasses     = &vkSubpassDescription;
    vkRenderPassCreateInfo.dependencyCount= 0;
    vkRenderPassCreateInfo.pDependencies  = NULL;

    vkResult = vkCreateRenderPass(vkDevice, &vkRenderPassCreateInfo, NULL, &vkRenderPass);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateRenderPass(): vkCreateRenderPass() failed.\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateRenderPass(): vkCreateRenderPass() success.\n");
    }

    return vkResult;
}

VkResult CreateFramebuffers(void)
{
    VkResult vkResult = VK_SUCCESS;

    VkImageView vkImageView_attachment_array[1];
    memset((void*)vkImageView_attachment_array, 0, sizeof(vkImageView_attachment_array));

    VkFramebufferCreateInfo vkFramebufferCreateInfo;
    memset((void*)&vkFramebufferCreateInfo, 0, sizeof(vkFramebufferCreateInfo));
    vkFramebufferCreateInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    vkFramebufferCreateInfo.pNext           = NULL;
    vkFramebufferCreateInfo.flags           = 0;
    vkFramebufferCreateInfo.renderPass      = vkRenderPass;
    vkFramebufferCreateInfo.attachmentCount = 1;
    vkFramebufferCreateInfo.pAttachments    = vkImageView_attachment_array;
    vkFramebufferCreateInfo.width           = vkExtent2D_SwapChain.width;
    vkFramebufferCreateInfo.height          = vkExtent2D_SwapChain.height;
    vkFramebufferCreateInfo.layers          = 1;

    vkFramebuffer_array = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*swapchainImageCount);

    for(uint32_t i=0; i<swapchainImageCount; i++)
    {
        vkImageView_attachment_array[0] = swapChainImageView_array[i];
        vkResult = vkCreateFramebuffer(vkDevice, &vkFramebufferCreateInfo, NULL, &vkFramebuffer_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateFramebuffers(): vkCreateFramebuffer() failed at i=%d\n", i);
            return vkResult;
        }
        else
        {
            fprintf(gFILE, "CreateFramebuffers(): success at i=%d\n", i);
        }
    }

    return vkResult;
}

VkResult CreateSemaphores(void)
{
    VkResult vkResult = VK_SUCCESS;
    VkSemaphoreCreateInfo vkSemaphoreCreateInfo;
    memset((void*)&vkSemaphoreCreateInfo, 0, sizeof(VkSemaphoreCreateInfo));
    vkSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkSemaphoreCreateInfo.pNext = NULL;
    vkSemaphoreCreateInfo.flags = 0;

    vkResult = vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, NULL, &vkSemaphore_BackBuffer);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateSemaphores(): vkCreateSemaphore() failed for BackBuffer.\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateSemaphores(): vkCreateSemaphore() success for BackBuffer.\n");
    }

    vkResult = vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, NULL, &vkSemaphore_RenderComplete);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gFILE, "CreateSemaphores(): vkCreateSemaphore() failed for RenderComplete.\n");
        return vkResult;
    }
    else
    {
        fprintf(gFILE, "CreateSemaphores(): vkCreateSemaphore() success for RenderComplete.\n");
    }

    return vkResult;
}

VkResult CreateFences(void)
{
    VkResult vkResult = VK_SUCCESS;
    VkFenceCreateInfo vkFenceCreateInfo;
    memset((void*)&vkFenceCreateInfo, 0, sizeof(VkFenceCreateInfo));
    vkFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkFenceCreateInfo.pNext = NULL;
    vkFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkFence_array = (VkFence*)malloc(sizeof(VkFence)*swapchainImageCount);

    for(uint32_t i=0; i<swapchainImageCount; i++)
    {
        vkResult = vkCreateFence(vkDevice, &vkFenceCreateInfo, NULL, &vkFence_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "CreateFences(): vkCreateFence() failed at i=%d\n", i);
            return vkResult;
        }
        else
        {
            fprintf(gFILE, "CreateFences(): success at i=%d\n", i);
        }
    }

    return vkResult;
}

VkResult buildCommandBuffers(void)
{
    VkResult vkResult = VK_SUCCESS;

    for(uint32_t i=0; i<swapchainImageCount; i++)
    {
        vkResult = vkResetCommandBuffer(vkCommandBuffer_array[i], 0);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "buildCommandBuffers(): vkResetCommandBuffer() failed at i=%d\n", i);
            return vkResult;
        }

        VkCommandBufferBeginInfo vkCommandBufferBeginInfo;
        memset((void*)&vkCommandBufferBeginInfo, 0, sizeof(VkCommandBufferBeginInfo));
        vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkCommandBufferBeginInfo.pNext = NULL;
        vkCommandBufferBeginInfo.flags = 0;
        vkCommandBufferBeginInfo.pInheritanceInfo = NULL;

        vkResult = vkBeginCommandBuffer(vkCommandBuffer_array[i], &vkCommandBufferBeginInfo);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "buildCommandBuffers(): vkBeginCommandBuffer() failed at i=%d\n", i);
            return vkResult;
        }

        VkClearValue vkClearValue_array[1];
        memset((void*)vkClearValue_array, 0, sizeof(vkClearValue_array));
        vkClearValue_array[0].color = vkClearColorValue;

        VkRenderPassBeginInfo vkRenderPassBeginInfo;
        memset((void*)&vkRenderPassBeginInfo, 0, sizeof(VkRenderPassBeginInfo));
        vkRenderPassBeginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vkRenderPassBeginInfo.pNext             = NULL;
        vkRenderPassBeginInfo.renderPass        = vkRenderPass;
        vkRenderPassBeginInfo.renderArea.offset.x = 0;
        vkRenderPassBeginInfo.renderArea.offset.y = 0;
        vkRenderPassBeginInfo.renderArea.extent.width  = vkExtent2D_SwapChain.width;
        vkRenderPassBeginInfo.renderArea.extent.height = vkExtent2D_SwapChain.height;
        vkRenderPassBeginInfo.clearValueCount   = 1;
        vkRenderPassBeginInfo.pClearValues      = vkClearValue_array;
        vkRenderPassBeginInfo.framebuffer       = vkFramebuffer_array[i];

        vkCmdBeginRenderPass(vkCommandBuffer_array[i], &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Here would go drawing commands if any

        vkCmdEndRenderPass(vkCommandBuffer_array[i]);

        vkResult = vkEndCommandBuffer(vkCommandBuffer_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gFILE, "buildCommandBuffers(): vkEndCommandBuffer() failed at i=%d\n", i);
            return vkResult;
        }
    }
    return vkResult;
}
