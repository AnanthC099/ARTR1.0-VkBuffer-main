#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR

#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// ----- ADDED CODE: Include GLM (for orthographic matrix) -----
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#pragma comment(lib, "vulkan-1.lib")

#define MYICON 101
#define WIN_WIDTH 800
#define WIN_HEIGHT 600
#define MAX_FRAMES_IN_FLIGHT 2

LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
HWND ghwnd=NULL;
BOOL gbActive=FALSE;
DWORD dwStyle=0;
WINDOWPLACEMENT wpPrev;
BOOL gbFullscreen=FALSE;
FILE* gFILE=NULL;
const char* gpszAppName="VulkanApp";

bool gEnableValidationLayers = true;

static VkDebugUtilsMessengerEXT gDebugUtilsMessenger=VK_NULL_HANDLE;
static PFN_vkCmdBeginRenderingKHR pfnCmdBeginRenderingKHR=NULL;
static PFN_vkCmdEndRenderingKHR   pfnCmdEndRenderingKHR=NULL;

VkInstance vkInstance=VK_NULL_HANDLE;
VkSurfaceKHR vkSurfaceKHR=VK_NULL_HANDLE;
VkPhysicalDevice vkPhysicalDevice_selected=VK_NULL_HANDLE;
VkDevice vkDevice=VK_NULL_HANDLE;
VkQueue vkQueue=VK_NULL_HANDLE;
uint32_t graphicsQuequeFamilyIndex_selected=UINT32_MAX;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;
VkSwapchainKHR vkSwapchainKHR=VK_NULL_HANDLE;
VkFormat vkFormat_color=VK_FORMAT_UNDEFINED;
VkColorSpaceKHR vkColorSpaceKHR=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR vkPresentModeKHR=VK_PRESENT_MODE_FIFO_KHR;
VkExtent2D vkExtent2D_SwapChain;
uint32_t swapchainImageCount=0;
VkImage* swapChainImage_array=NULL;
VkImageView* swapChainImageView_array=NULL;
VkCommandPool vkCommandPool=VK_NULL_HANDLE;
VkCommandBuffer* vkCommandBuffer_array=NULL;
VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];
uint32_t currentFrame=0;
uint32_t currentImageIndex=UINT32_MAX;
BOOL bInitialized=FALSE;
VkClearColorValue vkClearColorValue;
int winWidth=WIN_WIDTH,winHeight=WIN_HEIGHT;

// ----- ADDED CODE: Pipeline / Vertex Buffer Globals -----
static VkPipelineLayout gPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       gPipeline       = VK_NULL_HANDLE;

// A simple 2D triangle (3 vertices)
static const float gTriangleVertices[] =
{
    //  x,     y
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f
};

static VkBuffer       gVertexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory gVertexMemory = VK_NULL_HANDLE;
// --------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT ms,
    VkDebugUtilsMessageTypeFlagsEXT mt,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    fprintf(gFILE,"VALIDATION: %s\n",pCallbackData->pMessage);
    return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger)
{
    PFN_vkCreateDebugUtilsMessengerEXT fn=
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");
    if(fn) return fn(instance,pCreateInfo,pAllocator,pMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,VkDebugUtilsMessengerEXT messenger,const VkAllocationCallbacks* pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT fn=
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT");
    if(fn) fn(instance,messenger,pAllocator);
}

static void ToggleFullscreen(void)
{
    MONITORINFO mi={sizeof(MONITORINFO)};
    if(!gbFullscreen)
    {
        dwStyle=GetWindowLong(ghwnd,GWL_STYLE);
        if(dwStyle & WS_OVERLAPPEDWINDOW)
        {
            if(GetWindowPlacement(ghwnd,&wpPrev) &&
               GetMonitorInfo(MonitorFromWindow(ghwnd,MONITORINFOF_PRIMARY),&mi))
            {
                SetWindowLong(ghwnd,GWL_STYLE,dwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(ghwnd,HWND_TOP,
                    mi.rcMonitor.left,mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOZORDER|SWP_FRAMECHANGED);
            }
        }
        ShowCursor(FALSE);
    }
    else
    {
        SetWindowPlacement(ghwnd,&wpPrev);
        SetWindowLong(ghwnd,GWL_STYLE,dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(ghwnd,HWND_TOP,0,0,0,0,
                     SWP_NOMOVE|SWP_NOSIZE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
    switch(iMsg)
    {
        case WM_CREATE:
            memset(&wpPrev,0,sizeof(WINDOWPLACEMENT));
            wpPrev.length=sizeof(WINDOWPLACEMENT);
            break;
        case WM_SETFOCUS:
            gbActive=TRUE;
            fprintf(gFILE, "LOG: Window gained focus.\n");
            break;
        case WM_KILLFOCUS:
            gbActive=FALSE;
            fprintf(gFILE, "LOG: Window lost focus.\n");
            break;
        case WM_SIZE:
            if(bInitialized){
                winWidth=LOWORD(lParam);
                winHeight=HIWORD(lParam);
                fprintf(gFILE,"LOG: WM_SIZE -> resize(%d, %d)\n", winWidth, winHeight);
                if(winWidth && winHeight){
                    extern void resize(int,int);
                    resize(winWidth,winHeight);
                }
            }
            break;
        case WM_KEYDOWN:
            if(wParam==VK_ESCAPE){
                fprintf(gFILE,"LOG: ESC key pressed, destroying window.\n");
                fclose(gFILE);gFILE=NULL;
                DestroyWindow(hwnd);
            }
            break;
        case WM_CHAR:
            if(wParam=='f' || wParam=='F'){
                fprintf(gFILE,"LOG: Toggle fullscreen.\n");
                ToggleFullscreen();
                gbFullscreen=!gbFullscreen;
            }
            break;
        case WM_CLOSE:
            fprintf(gFILE,"LOG: WM_CLOSE received, destroying window.\n");
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            fprintf(gFILE,"LOG: WM_DESTROY received, PostQuitMessage.\n");
            PostQuitMessage(0);
            break;
    }
    return DefWindowProc(hwnd,iMsg,wParam,lParam);
}

static uint32_t instExtCount=0;
static const char* instExts[4];
static uint32_t instLayerCount=0;
static const char* instLayers[1];

static VkResult FillInstanceExtensions(void)
{
    fprintf(gFILE,"LOG: FillInstanceExtensions() Start\n");

    uint32_t count=0;
    vkEnumerateInstanceExtensionProperties(NULL,&count,NULL);
    if(!count) return VK_ERROR_INITIALIZATION_FAILED;

    VkExtensionProperties* arr=(VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*count);
    vkEnumerateInstanceExtensionProperties(NULL,&count,arr);

    VkBool32 foundSurface=VK_FALSE,foundWin32=VK_FALSE;
    for(uint32_t i=0;i<count;i++){
        if(!strcmp(arr[i].extensionName,VK_KHR_SURFACE_EXTENSION_NAME)) foundSurface=VK_TRUE;
        if(!strcmp(arr[i].extensionName,VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) foundWin32=VK_TRUE;
    }
    free(arr);

    if(!foundSurface||!foundWin32) {
        fprintf(gFILE,"ERROR: Required instance extensions not found!\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    instExtCount=0;
    instExts[instExtCount++]=VK_KHR_SURFACE_EXTENSION_NAME;
    instExts[instExtCount++]=VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

    fprintf(gFILE,"LOG: FillInstanceExtensions() Successful\n");
    return VK_SUCCESS;
}

static void TryAddDebugExt(void)
{
    fprintf(gFILE,"LOG: TryAddDebugExt() Start\n");
    uint32_t c=0;
    vkEnumerateInstanceExtensionProperties(NULL,&c,NULL);
    if(!c) return;
    VkExtensionProperties* arr=(VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*c);
    vkEnumerateInstanceExtensionProperties(NULL,&c,arr);

    for(uint32_t i=0;i<c;i++){
        if(!strcmp(arr[i].extensionName,VK_EXT_DEBUG_UTILS_EXTENSION_NAME)){
            fprintf(gFILE,"LOG: Found VK_EXT_debug_utils, enabling debug extension.\n");
            instExts[instExtCount++]=VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            break;
        }
    }
    free(arr);
}

static void TryAddValidationLayer(void)
{
    fprintf(gFILE,"LOG: TryAddValidationLayer() Start\n");

    // If validation is disabled, do nothing
    if (!gEnableValidationLayers)
    {
        fprintf(gFILE,"LOG: Validation layers disabled by user, skipping.\n");
        return;
    }

    uint32_t c=0;
    vkEnumerateInstanceLayerProperties(&c,NULL);
    if(!c) {
        fprintf(gFILE,"LOG: No instance layers found, skipping.\n");
        return;
    }
    VkLayerProperties* arr=(VkLayerProperties*)malloc(sizeof(VkLayerProperties)*c);
    vkEnumerateInstanceLayerProperties(&c,arr);

    for(uint32_t i=0;i<c;i++){
        if(!strcmp(arr[i].layerName,"VK_LAYER_KHRONOS_validation")){
            instLayers[0]="VK_LAYER_KHRONOS_validation";
            instLayerCount=1;
            fprintf(gFILE,"LOG: Found VK_LAYER_KHRONOS_validation -> enabling validation layer.\n");
            break;
        }
    }
    free(arr);
}

static VkResult CreateVulkanInstance(void)
{
    fprintf(gFILE,"LOG: CreateVulkanInstance() Start\n");
    VkResult r=FillInstanceExtensions();
    if(r!=VK_SUCCESS) return r;

    TryAddDebugExt();
    TryAddValidationLayer();

    VkApplicationInfo ai={VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName=gpszAppName;
    ai.applicationVersion=1;
    ai.pEngineName=gpszAppName;
    ai.engineVersion=1;
    ai.apiVersion=VK_API_VERSION_1_3;

    VkInstanceCreateInfo ici={VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo=&ai;
    ici.enabledExtensionCount=instExtCount;
    ici.ppEnabledExtensionNames=instExts;
    ici.enabledLayerCount=instLayerCount;
    ici.ppEnabledLayerNames=instLayers;

    r=vkCreateInstance(&ici,NULL,&vkInstance);
    if(r!=VK_SUCCESS){
        fprintf(gFILE,"ERROR: vkCreateInstance() Failed!\n");
        return r;
    }

    // Only create debug messenger if validation layers are active
    if(instLayerCount>0){
        VkDebugUtilsMessengerCreateInfoEXT dbg={VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        dbg.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbg.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|
                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|
                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbg.pfnUserCallback=DebugCallback;
        CreateDebugUtilsMessengerEXT(vkInstance,&dbg,NULL,&gDebugUtilsMessenger);
        fprintf(gFILE,"LOG: Debug Messenger Created.\n");
    }

    fprintf(gFILE,"LOG: CreateVulkanInstance() Successful\n");
    return VK_SUCCESS;
}

static VkResult CreateSurface(void)
{
    fprintf(gFILE,"LOG: CreateSurface() Start\n");
    VkWin32SurfaceCreateInfoKHR ci={VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    ci.hinstance=(HINSTANCE)GetWindowLongPtr(ghwnd,GWLP_HINSTANCE);
    ci.hwnd=ghwnd;
    VkResult result = vkCreateWin32SurfaceKHR(vkInstance,&ci,NULL,&vkSurfaceKHR);
    if(result == VK_SUCCESS)
        fprintf(gFILE,"LOG: Surface creation successful.\n");
    else
        fprintf(gFILE,"ERROR: Surface creation failed.\n");
    return result;
}

static VkResult PickPhysicalDevice(void)
{
    fprintf(gFILE,"LOG: PickPhysicalDevice() Start\n");
    uint32_t c=0;
    vkEnumeratePhysicalDevices(vkInstance,&c,NULL);
    if(!c) return VK_ERROR_INITIALIZATION_FAILED;

    VkPhysicalDevice* devs=(VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice)*c);
    vkEnumeratePhysicalDevices(vkInstance,&c,devs);

    int chosen=-1;
    for(uint32_t i=0;i<c;i++){
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devs[i], &props);

        uint32_t qc=0;
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i],&qc,NULL);
        if(!qc) continue;

        VkQueueFamilyProperties* qprops=(VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties)*qc);
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i],&qc,qprops);

        for(uint32_t f=0; f<qc; f++){
            VkBool32 sup=VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(devs[i],f,vkSurfaceKHR,&sup);
            if((qprops[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) && sup){
                vkPhysicalDevice_selected=devs[i];
                graphicsQuequeFamilyIndex_selected=f;
                chosen=i;
                break;
            }
        }
        free(qprops);
        if(chosen >= 0) break;
    }
    free(devs);
    if(chosen<0) return VK_ERROR_INITIALIZATION_FAILED;

    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_selected,&vkPhysicalDeviceMemoryProperties);
    fprintf(gFILE,"LOG: PickPhysicalDevice() Successful\n");
    return VK_SUCCESS;
}

static const char* devExts[2];
static uint32_t devExtCount=0;

static VkResult CheckDevExtensions(void)
{
    fprintf(gFILE,"LOG: CheckDevExtensions() Start\n");
    uint32_t c=0;
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected,NULL,&c,NULL);
    if(!c) return VK_ERROR_INITIALIZATION_FAILED;

    VkExtensionProperties* arr=(VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*c);
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected,NULL,&c,arr);

    VkBool32 haveSwap=VK_FALSE,haveDyn=VK_FALSE;
    for(uint32_t i=0;i<c;i++){
        if(!strcmp(arr[i].extensionName,VK_KHR_SWAPCHAIN_EXTENSION_NAME)) haveSwap=VK_TRUE;
        if(!strcmp(arr[i].extensionName,"VK_KHR_dynamic_rendering")) haveDyn=VK_TRUE;
    }
    free(arr);

    devExtCount=0;
    if(!haveSwap) {
        fprintf(gFILE,"ERROR: VK_KHR_swapchain extension not found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    devExts[devExtCount++]=VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    if(haveDyn) {
        devExts[devExtCount++]="VK_KHR_dynamic_rendering";
    }

    fprintf(gFILE,"LOG: CheckDevExtensions() Successful\n");
    return VK_SUCCESS;
}

static void LoadDynamicRenderingFunctions(void)
{
    pfnCmdBeginRenderingKHR=(PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(vkDevice,"vkCmdBeginRenderingKHR");
    pfnCmdEndRenderingKHR  =(PFN_vkCmdEndRenderingKHR)  vkGetDeviceProcAddr(vkDevice,"vkCmdEndRenderingKHR");
}

static VkResult CreateDeviceQueue(void)
{
    fprintf(gFILE,"LOG: CreateDeviceQueue() Start\n");
    VkResult r=CheckDevExtensions();
    if(r!=VK_SUCCESS) return r;

    float qp=1.0f;
    VkDeviceQueueCreateInfo dqci={VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    dqci.queueFamilyIndex=graphicsQuequeFamilyIndex_selected;
    dqci.queueCount=1;
    dqci.pQueuePriorities=&qp;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR drf={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR};
    drf.dynamicRendering=VK_TRUE;

    VkDeviceCreateInfo dci={VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount=1;
    dci.pQueueCreateInfos=&dqci;
    dci.enabledExtensionCount=devExtCount;
    dci.ppEnabledExtensionNames=devExts;
    dci.pEnabledFeatures=NULL;
    dci.pNext=&drf;

    r=vkCreateDevice(vkPhysicalDevice_selected,&dci,NULL,&vkDevice);
    if(r!=VK_SUCCESS){
        fprintf(gFILE,"ERROR: vkCreateDevice() Failed!\n");
        return r;
    }
    vkGetDeviceQueue(vkDevice,graphicsQuequeFamilyIndex_selected,0,&vkQueue);
    LoadDynamicRenderingFunctions();

    fprintf(gFILE,"LOG: CreateDeviceQueue() Successful\n");
    return VK_SUCCESS;
}

static VkResult ChooseSwapFormat(void)
{
    fprintf(gFILE,"LOG: ChooseSwapFormat() Start\n");
    uint32_t c=0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_selected,vkSurfaceKHR,&c,NULL);
    if(!c) return VK_ERROR_INITIALIZATION_FAILED;

    VkSurfaceFormatKHR* arr=(VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR)*c);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_selected,vkSurfaceKHR,&c,arr);

    if(c==1 && arr[0].format==VK_FORMAT_UNDEFINED){
        vkFormat_color=VK_FORMAT_B8G8R8A8_UNORM;
    } else {
        vkFormat_color=arr[0].format;
    }
    vkColorSpaceKHR=arr[0].colorSpace;
    free(arr);
    return VK_SUCCESS;
}

static VkResult ChoosePresentMode(void)
{
    fprintf(gFILE,"LOG: ChoosePresentMode() Start\n");
    uint32_t c=0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_selected,vkSurfaceKHR,&c,NULL);
    if(!c) return VK_ERROR_INITIALIZATION_FAILED;

    VkPresentModeKHR* arr=(VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR)*c);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_selected,vkSurfaceKHR,&c,arr);

    vkPresentModeKHR=VK_PRESENT_MODE_FIFO_KHR;
    for(uint32_t i=0;i<c;i++){
        if(arr[i]==VK_PRESENT_MODE_MAILBOX_KHR){
            vkPresentModeKHR=VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    free(arr);
    return VK_SUCCESS;
}

static VkResult CreateSwapchain(void)
{
    fprintf(gFILE,"LOG: CreateSwapchain() Start\n");
    ChooseSwapFormat();
    ChoosePresentMode();

    VkSurfaceCapabilitiesKHR cap;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        vkPhysicalDevice_selected,vkSurfaceKHR,&cap);

    uint32_t desired=cap.minImageCount+1;
    if(cap.maxImageCount>0 && desired>cap.maxImageCount)
        desired=cap.maxImageCount;

    if(cap.currentExtent.width!=UINT32_MAX){
        vkExtent2D_SwapChain=cap.currentExtent;
    } else {
        vkExtent2D_SwapChain.width=winWidth;
        vkExtent2D_SwapChain.height=winHeight;
        if(vkExtent2D_SwapChain.width<cap.minImageExtent.width)
            vkExtent2D_SwapChain.width=cap.minImageExtent.width;
        if(vkExtent2D_SwapChain.height<cap.minImageExtent.height)
            vkExtent2D_SwapChain.height=cap.minImageExtent.height;
        if(vkExtent2D_SwapChain.width>cap.maxImageExtent.width)
            vkExtent2D_SwapChain.width=cap.maxImageExtent.width;
        if(vkExtent2D_SwapChain.height>cap.maxImageExtent.height)
            vkExtent2D_SwapChain.height=cap.maxImageExtent.height;
    }

    VkSwapchainCreateInfoKHR sci={VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface=vkSurfaceKHR;
    sci.minImageCount=desired;
    sci.imageFormat=vkFormat_color;
    sci.imageColorSpace=vkColorSpaceKHR;
    sci.imageExtent=vkExtent2D_SwapChain;
    sci.imageArrayLayers=1;
    sci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(cap.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        sci.preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
        sci.preTransform=cap.currentTransform;
    sci.presentMode=vkPresentModeKHR;
    sci.clipped=VK_TRUE;
    sci.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkResult result = vkCreateSwapchainKHR(vkDevice,&sci,NULL,&vkSwapchainKHR);
    if(result == VK_SUCCESS)
        fprintf(gFILE,"LOG: vkCreateSwapchainKHR() Successful.\n");
    else
        fprintf(gFILE,"ERROR: vkCreateSwapchainKHR() Failed.\n");
    return result;
}

static VkResult CreateImagesViews(void)
{
    fprintf(gFILE,"LOG: CreateImagesViews() Start\n");
    vkGetSwapchainImagesKHR(vkDevice,vkSwapchainKHR,&swapchainImageCount,NULL);
    if(!swapchainImageCount) return VK_ERROR_INITIALIZATION_FAILED;

    swapChainImage_array=(VkImage*)malloc(sizeof(VkImage)*swapchainImageCount);
    vkGetSwapchainImagesKHR(vkDevice,vkSwapchainKHR,&swapchainImageCount,swapChainImage_array);

    swapChainImageView_array=(VkImageView*)malloc(sizeof(VkImageView)*swapchainImageCount);

    for(uint32_t i=0;i<swapchainImageCount;i++){
        VkImageViewCreateInfo ivci={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivci.image=swapChainImage_array[i];
        ivci.viewType=VK_IMAGE_VIEW_TYPE_2D;
        ivci.format=vkFormat_color;
        ivci.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel=0;
        ivci.subresourceRange.levelCount=1;
        ivci.subresourceRange.baseArrayLayer=0;
        ivci.subresourceRange.layerCount=1;

        VkResult r=vkCreateImageView(vkDevice,&ivci,NULL,&swapChainImageView_array[i]);
        if(r!=VK_SUCCESS) return r;
    }
    return VK_SUCCESS;
}

static VkResult CreateCommandPoolBuffers(void)
{
    fprintf(gFILE,"LOG: CreateCommandPoolBuffers() Start\n");

    VkCommandPoolCreateInfo cpci={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex=graphicsQuequeFamilyIndex_selected;
    cpci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult r=vkCreateCommandPool(vkDevice,&cpci,NULL,&vkCommandPool);
    if(r!=VK_SUCCESS) return r;

    VkCommandBufferAllocateInfo cbai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool=vkCommandPool;
    cbai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount=1;

    vkCommandBuffer_array=(VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*swapchainImageCount);
    for(uint32_t i=0;i<swapchainImageCount;i++){
        r=vkAllocateCommandBuffers(vkDevice,&cbai,&vkCommandBuffer_array[i]);
        if(r!=VK_SUCCESS) return r;
    }
    return VK_SUCCESS;
}

// ----- ADDED CODE: Helper to load SPIR-V from file into a VkShaderModule -----
static VkShaderModule CreateShaderModule(const char* spvPath)
{
    FILE* fp = fopen(spvPath, "rb");
    if (!fp) {
        fprintf(gFILE, "ERROR: Could not open SPIR-V file: %s\n", spvPath);
        return VK_NULL_HANDLE;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char* buffer = (char*)malloc(size);
    if (!buffer) {
        fclose(fp);
        fprintf(gFILE, "ERROR: Could not allocate memory for SPIR-V buffer.\n");
        return VK_NULL_HANDLE;
    }
    fread(buffer, 1, size, fp);
    fclose(fp);

    VkShaderModuleCreateInfo smci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    smci.codeSize = size;
    smci.pCode    = (const uint32_t*)buffer;

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkResult r = vkCreateShaderModule(vkDevice, &smci, NULL, &shaderModule);
    free(buffer);

    if (r != VK_SUCCESS) {
        fprintf(gFILE, "ERROR: vkCreateShaderModule failed for file %s\n", spvPath);
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

// ----- ADDED CODE: Create a vertex buffer for the triangle -----
static VkResult CreateVertexBuffer(void)
{
    fprintf(gFILE, "LOG: CreateVertexBuffer() Start\n");

    // 1) Create a buffer
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size        = sizeof(gTriangleVertices);
    bci.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult r = vkCreateBuffer(vkDevice, &bci, NULL, &gVertexBuffer);
    if (r != VK_SUCCESS) {
        fprintf(gFILE, "ERROR: vkCreateBuffer() for vertex data failed!\n");
        return r;
    }

    // 2) Memory requirements
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gVertexBuffer, &memReq);

    // 3) Find suitable host‐visible memory type
    uint32_t memTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
        if ((memReq.memoryTypeBits & (1 << i)) &&
            ((vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags &
              (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)))
        {
            memTypeIndex = i;
            break;
        }
    }
    if (memTypeIndex == UINT32_MAX) {
        fprintf(gFILE, "ERROR: Could not find HOST_VISIBLE memory for vertex buffer!\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // 4) Allocate
    VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = memTypeIndex;
    r = vkAllocateMemory(vkDevice, &mai, NULL, &gVertexMemory);
    if (r != VK_SUCCESS) {
        fprintf(gFILE, "ERROR: vkAllocateMemory() for vertex buffer failed!\n");
        return r;
    }

    // 5) Bind
    vkBindBufferMemory(vkDevice, gVertexBuffer, gVertexMemory, 0);

    // 6) Copy
    void* data = NULL;
    vkMapMemory(vkDevice, gVertexMemory, 0, memReq.size, 0, &data);
    memcpy(data, gTriangleVertices, sizeof(gTriangleVertices));
    vkUnmapMemory(vkDevice, gVertexMemory);

    fprintf(gFILE, "LOG: CreateVertexBuffer() successful\n");
    return VK_SUCCESS;
}

// ----- ADDED CODE: Create a Pipeline Layout (with push constants for an ortho mat) -----
static VkResult CreatePipelineLayout(void)
{
    fprintf(gFILE, "LOG: CreatePipelineLayout() Start\n");

    // One push constant range for a mat4 (64 bytes)
    VkPushConstantRange pcRange;
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(float) * 16; // 16 floats => mat4

    VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    plci.setLayoutCount         = 0;   // no descriptor sets
    plci.pSetLayouts            = NULL;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges    = &pcRange;

    VkResult r = vkCreatePipelineLayout(vkDevice, &plci, NULL, &gPipelineLayout);
    if (r != VK_SUCCESS) {
        fprintf(gFILE, "ERROR: vkCreatePipelineLayout failed!\n");
        return r;
    }
    return VK_SUCCESS;
}

// ----- ADDED CODE: Create a Graphics Pipeline using dynamic rendering -----
static VkResult CreateGraphicsPipeline(void)
{
    fprintf(gFILE, "LOG: CreateGraphicsPipeline() Start\n");

    // 1) Load SPIR-V modules (for our example, we have white in the fragment)
    VkShaderModule vertModule = CreateShaderModule("./triangle.vert.spirv");
    VkShaderModule fragModule = CreateShaderModule("./triangle.frag.spirv");
    if (!vertModule || !fragModule)
        return VK_ERROR_INITIALIZATION_FAILED;

    // 2) Shader Stage Infos
    VkPipelineShaderStageCreateInfo vsci = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    vsci.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vsci.module = vertModule;
    vsci.pName  = "main";

    VkPipelineShaderStageCreateInfo fsci = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    fsci.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fsci.module = fragModule;
    fsci.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[2] = { vsci, fsci };

    // 3) Vertex Input State: just 2 floats for position
    VkVertexInputBindingDescription bindingDesc;
    bindingDesc.binding   = 0;
    bindingDesc.stride    = sizeof(float) * 2; // 2D position
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDesc;
    attrDesc.binding  = 0;                       // matches bindingDesc.binding
    attrDesc.location = 0;                       // "layout(location=0)"
    attrDesc.format   = VK_FORMAT_R32G32_SFLOAT; // vec2
    attrDesc.offset   = 0;

    VkPipelineVertexInputStateCreateInfo visci = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    visci.vertexBindingDescriptionCount   = 1;
    visci.pVertexBindingDescriptions      = &bindingDesc;
    visci.vertexAttributeDescriptionCount = 1;
    visci.pVertexAttributeDescriptions    = &attrDesc;

    // 4) Input Assembly
    VkPipelineInputAssemblyStateCreateInfo iasci = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iasci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // 5) Viewport/Scissor State (dynamic)
    VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vpState.viewportCount = 1;
    vpState.scissorCount  = 1;

    // 6) Rasterizer
    VkPipelineRasterizationStateCreateInfo rsci = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsci.polygonMode = VK_POLYGON_MODE_FILL;
    rsci.cullMode    = VK_CULL_MODE_NONE;
    rsci.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    rsci.lineWidth   = 1.0f;

    // 7) Multisample
    VkPipelineMultisampleStateCreateInfo msci = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 8) Color Blend
    VkPipelineColorBlendAttachmentState cbAttach = {0};
    cbAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|
                              VK_COLOR_COMPONENT_G_BIT|
                              VK_COLOR_COMPONENT_B_BIT|
                              VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbci = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbci.attachmentCount = 1;
    cbci.pAttachments    = &cbAttach;

    // 9) Dynamic States
    VkDynamicState dynStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dsci = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dsci.dynamicStateCount = 2;
    dsci.pDynamicStates    = dynStates;

    // 10) Use dynamic rendering => specify color format
    VkPipelineRenderingCreateInfoKHR prci = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
    prci.colorAttachmentCount    = 1;
    prci.pColorAttachmentFormats = &vkFormat_color;

    // 11) Finally, create the pipeline
    VkGraphicsPipelineCreateInfo gpci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gpci.pNext               = &prci; // dynamic rendering
    gpci.stageCount          = 2;
    gpci.pStages             = shaderStages;
    gpci.pVertexInputState   = &visci;
    gpci.pInputAssemblyState = &iasci;
    gpci.pViewportState      = &vpState;
    gpci.pRasterizationState = &rsci;
    gpci.pMultisampleState   = &msci;
    gpci.pColorBlendState    = &cbci;
    gpci.pDynamicState       = &dsci;
    gpci.layout              = gPipelineLayout;

    VkResult r = vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &gpci, NULL, &gPipeline);
    if (r != VK_SUCCESS) {
        fprintf(gFILE, "ERROR: vkCreateGraphicsPipelines() failed!\n");
        return r;
    }

    // 12) Cleanup shader modules
    vkDestroyShaderModule(vkDevice, vertModule, NULL);
    vkDestroyShaderModule(vkDevice, fragModule, NULL);

    fprintf(gFILE, "LOG: CreateGraphicsPipeline() Successful\n");
    return VK_SUCCESS;
}

// ----- MODIFIED: buildCommandBuffers to draw a white triangle -----
static VkResult buildCommandBuffers(void)
{
    fprintf(gFILE,"LOG: buildCommandBuffers() Start\n");
    for(uint32_t i=0; i<swapchainImageCount; i++){
        vkResetCommandBuffer(vkCommandBuffer_array[i],0);

        VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        VkResult r = vkBeginCommandBuffer(vkCommandBuffer_array[i],&bi);
        if(r != VK_SUCCESS) {
            fprintf(gFILE, "ERROR: vkBeginCommandBuffer failed at index %u.\n", i);
            return r;
        }

        // Transition UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
        VkImageMemoryBarrier toColor={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toColor.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        toColor.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toColor.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        toColor.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        toColor.image=swapChainImage_array[i];
        toColor.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        toColor.subresourceRange.baseMipLevel=0;
        toColor.subresourceRange.levelCount=1;
        toColor.subresourceRange.baseArrayLayer=0;
        toColor.subresourceRange.layerCount=1;
        toColor.srcAccessMask=0;
        toColor.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(
            vkCommandBuffer_array[i],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0,NULL,
            0,NULL,
            1,&toColor
        );

        // Begin dynamic rendering
        VkRenderingAttachmentInfoKHR colAtt={VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};
        colAtt.imageView=swapChainImageView_array[i];
        colAtt.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colAtt.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        colAtt.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        colAtt.clearValue.color=vkClearColorValue;

        VkRenderingInfoKHR ri={VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
        ri.renderArea.offset.x=0;
        ri.renderArea.offset.y=0;
        ri.renderArea.extent=vkExtent2D_SwapChain;
        ri.layerCount=1;
        ri.colorAttachmentCount=1;
        ri.pColorAttachments=&colAtt;

        pfnCmdBeginRenderingKHR(vkCommandBuffer_array[i], &ri);

        // ----- NEW: set viewport/scissor (dynamic) -----
        {
            VkViewport vp;
            vp.x = 0.0f; 
            vp.y = 0.0f;
            vp.width    = (float)vkExtent2D_SwapChain.width;
            vp.height   = (float)vkExtent2D_SwapChain.height;
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            vkCmdSetViewport(vkCommandBuffer_array[i], 0, 1, &vp);

            VkRect2D scissor;
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent   = vkExtent2D_SwapChain;
            vkCmdSetScissor(vkCommandBuffer_array[i], 0, 1, &scissor);
        }

        // ----- NEW: bind pipeline -----
        vkCmdBindPipeline(vkCommandBuffer_array[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gPipeline);

        // ----- NEW: push an orthographic matrix via push constants -----
        {
            glm::mat4 ortho = glm::ortho(-1.0f,1.0f, -1.0f,1.0f, -1.0f,1.0f);
            vkCmdPushConstants(
                vkCommandBuffer_array[i],
                gPipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,                     // offset
                sizeof(glm::mat4),     // size
                &ortho[0][0]           // pointer
            );
        }

        // ----- NEW: bind vertex buffer and draw the triangle -----
        {
            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(vkCommandBuffer_array[i], 0, 1, &gVertexBuffer, offsets);
            vkCmdDraw(vkCommandBuffer_array[i], 3, 1, 0, 0);
        }

        // End dynamic rendering
        pfnCmdEndRenderingKHR(vkCommandBuffer_array[i]);

        // Transition COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
        VkImageMemoryBarrier toPresent={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toPresent.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toPresent.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        toPresent.image=swapChainImage_array[i];
        toPresent.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        toPresent.subresourceRange.baseMipLevel=0;
        toPresent.subresourceRange.levelCount=1;
        toPresent.subresourceRange.baseArrayLayer=0;
        toPresent.subresourceRange.layerCount=1;
        toPresent.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toPresent.dstAccessMask=0;

        vkCmdPipelineBarrier(
            vkCommandBuffer_array[i],
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,NULL,
            0,NULL,
            1,&toPresent
        );

        vkEndCommandBuffer(vkCommandBuffer_array[i]);
    }
    return VK_SUCCESS;
}

static VkResult CreateSyncObjects(void)
{
    fprintf(gFILE,"LOG: CreateSyncObjects() Start\n");
    VkSemaphoreCreateInfo sci={VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fci={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags=VK_FENCE_CREATE_SIGNALED_BIT;

    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        if(vkCreateSemaphore(vkDevice,&sci,NULL,&imageAvailableSemaphores[i])!=VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
        if(vkCreateSemaphore(vkDevice,&sci,NULL,&renderFinishedSemaphores[i])!=VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
        if(vkCreateFence(vkDevice,&fci,NULL,&inFlightFences[i])!=VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

static void cleanupSwapchain(void)
{
    fprintf(gFILE,"LOG: cleanupSwapchain() Start\n");
    vkDeviceWaitIdle(vkDevice);

    if(vkCommandBuffer_array){
        for(uint32_t i=0;i<swapchainImageCount;i++){
            if(vkCommandBuffer_array[i]){
                vkFreeCommandBuffers(vkDevice,vkCommandPool,1,&vkCommandBuffer_array[i]);
            }
        }
        free(vkCommandBuffer_array);
        vkCommandBuffer_array=NULL;
    }
    if(vkCommandPool){
        vkDestroyCommandPool(vkDevice,vkCommandPool,NULL);
        vkCommandPool=VK_NULL_HANDLE;
    }
    if(swapChainImageView_array){
        for(uint32_t i=0;i<swapchainImageCount;i++){
            if(swapChainImageView_array[i]){
                vkDestroyImageView(vkDevice,swapChainImageView_array[i],NULL);
            }
        }
        free(swapChainImageView_array);
        swapChainImageView_array=NULL;
    }
    if(swapChainImage_array){
        free(swapChainImage_array);
        swapChainImage_array=NULL;
    }
    if(vkSwapchainKHR){
        vkDestroySwapchainKHR(vkDevice,vkSwapchainKHR,NULL);
        vkSwapchainKHR=VK_NULL_HANDLE;
    }
}

static VkResult recreateSwapchain(void)
{
    fprintf(gFILE,"LOG: recreateSwapchain() Start\n");
    if(winWidth==0||winHeight==0) {
        fprintf(gFILE,"LOG: Window minimized, skip recreate.\n");
        return VK_SUCCESS;
    }
    cleanupSwapchain();

    VkResult r=CreateSwapchain();
    if(r!=VK_SUCCESS) return r;
    r=CreateImagesViews();
    if(r!=VK_SUCCESS) return r;
    r=CreateCommandPoolBuffers();
    if(r!=VK_SUCCESS) return r;
    // Rebuild command buffers so they use new swapchain images
    r=buildCommandBuffers();
    return r;
}

void resize(int w,int h)
{
    fprintf(gFILE,"LOG: resize(%d, %d)\n", w, h);
    winWidth=w;
    winHeight=h;
    if(winWidth && winHeight)
        recreateSwapchain();
}

static VkResult drawFrame(void)
{
    VkResult fenceWait = vkWaitForFences(vkDevice,1,&inFlightFences[currentFrame],VK_TRUE,UINT64_MAX);
    if(fenceWait != VK_SUCCESS) return fenceWait;

    VkResult fenceReset = vkResetFences(vkDevice,1,&inFlightFences[currentFrame]);
    if(fenceReset != VK_SUCCESS) return fenceReset;

    uint32_t imgIndex;
    VkResult r=vkAcquireNextImageKHR(
        vkDevice,
        vkSwapchainKHR,
        UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE,
        &imgIndex);

    if(r==VK_ERROR_OUT_OF_DATE_KHR){
        recreateSwapchain();
        return VK_SUCCESS;
    }
    if(r!=VK_SUCCESS && r!=VK_SUBOPTIMAL_KHR)
        return r;

    VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount=1;
    si.pWaitSemaphores=&imageAvailableSemaphores[currentFrame];
    si.pWaitDstStageMask=&waitStage;
    si.commandBufferCount=1;
    si.pCommandBuffers=&vkCommandBuffer_array[imgIndex];
    si.signalSemaphoreCount=1;
    si.pSignalSemaphores=&renderFinishedSemaphores[currentFrame];

    r=vkQueueSubmit(vkQueue,1,&si,inFlightFences[currentFrame]);
    if(r!=VK_SUCCESS) return r;

    VkPresentInfoKHR pi={VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount=1;
    pi.pWaitSemaphores=&renderFinishedSemaphores[currentFrame];
    pi.swapchainCount=1;
    pi.pSwapchains=&vkSwapchainKHR;
    pi.pImageIndices=&imgIndex;

    r=vkQueuePresentKHR(vkQueue,&pi);
    if(r==VK_ERROR_OUT_OF_DATE_KHR || r==VK_SUBOPTIMAL_KHR){
        recreateSwapchain();
    }
    currentFrame=(currentFrame+1)%MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

VkResult initialize(void)
{
    fprintf(gFILE,"LOG: initialize() Start\n");
    VkResult r=CreateVulkanInstance();
    if(r!=VK_SUCCESS) return r;

    r=CreateSurface();
    if(r!=VK_SUCCESS) return r;

    r=PickPhysicalDevice();
    if(r!=VK_SUCCESS) return r;

    r=CreateDeviceQueue();
    if(r!=VK_SUCCESS) return r;

    // Create swapchain/images
    r=CreateSwapchain();
    if(r!=VK_SUCCESS) return r;
    r=CreateImagesViews();
    if(r!=VK_SUCCESS) return r;

    // Command Pool/Buffers
    r=CreateCommandPoolBuffers();
    if(r!=VK_SUCCESS) return r;

    // ----- ADDED: Create vertex buffer with our triangle data -----
    r=CreateVertexBuffer();
    if(r!=VK_SUCCESS) return r;

    // ----- ADDED: Create pipeline layout + pipeline (dynamic rendering) -----
    r=CreatePipelineLayout();
    if(r!=VK_SUCCESS) return r;

    r=CreateGraphicsPipeline();
    if(r!=VK_SUCCESS) return r;

    // Sync
    r=CreateSyncObjects();
    if(r!=VK_SUCCESS) return r;

    // Clear color => (0,0,1,1) => blue
    memset(&vkClearColorValue,0,sizeof(vkClearColorValue));
    vkClearColorValue.float32[0]=0.0f;
    vkClearColorValue.float32[1]=0.0f;
    vkClearColorValue.float32[2]=1.0f;
    vkClearColorValue.float32[3]=1.0f;

    // Build command buffers (which now draw our triangle)
    r=buildCommandBuffers();
    if(r!=VK_SUCCESS) return r;

    bInitialized=TRUE;
    return VK_SUCCESS;
}

VkResult display(void){
    return drawFrame();
}

void update(void){
    // no-op in this example
}

void uninitialize(void)
{
    fprintf(gFILE,"LOG: uninitialize() Start\n");
    if(gbFullscreen){
        ToggleFullscreen();
        gbFullscreen=FALSE;
    }
    if(vkDevice){
        vkDeviceWaitIdle(vkDevice);

        // Destroy sync objects
        for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
            if(inFlightFences[i])
                vkDestroyFence(vkDevice,inFlightFences[i],NULL);
            if(renderFinishedSemaphores[i])
                vkDestroySemaphore(vkDevice,renderFinishedSemaphores[i],NULL);
            if(imageAvailableSemaphores[i])
                vkDestroySemaphore(vkDevice,imageAvailableSemaphores[i],NULL);
        }

        // ----- ADDED: Destroy pipeline, pipeline layout, vertex buffer, etc. -----
        if(gPipeline) {
            vkDestroyPipeline(vkDevice, gPipeline, NULL);
            gPipeline = VK_NULL_HANDLE;
        }
        if(gPipelineLayout) {
            vkDestroyPipelineLayout(vkDevice, gPipelineLayout, NULL);
            gPipelineLayout = VK_NULL_HANDLE;
        }
        if(gVertexBuffer) {
            vkDestroyBuffer(vkDevice, gVertexBuffer, NULL);
            gVertexBuffer = VK_NULL_HANDLE;
        }
        if(gVertexMemory) {
            vkFreeMemory(vkDevice, gVertexMemory, NULL);
            gVertexMemory = VK_NULL_HANDLE;
        }
        // -------------------------------------------------------------

        cleanupSwapchain();
        vkDestroyDevice(vkDevice,NULL);
    }
    if(gDebugUtilsMessenger) {
        DestroyDebugUtilsMessengerEXT(vkInstance,gDebugUtilsMessenger,NULL);
    }
    if(vkSurfaceKHR) {
        vkDestroySurfaceKHR(vkInstance,vkSurfaceKHR,NULL);
    }
    if(vkInstance) {
        vkDestroyInstance(vkInstance,NULL);
    }
    if(ghwnd){
        DestroyWindow(ghwnd);
        ghwnd=NULL;
    }
    if(gFILE){
        fprintf(gFILE,"Program ended.\n");
        fclose(gFILE);
        gFILE=NULL;
    }
}

int WINAPI WinMain(HINSTANCE hInst,HINSTANCE hPrev,LPSTR lpszCmdLine,int iCmdShow)
{
    WNDCLASSEX wc;
    wc.cbSize=sizeof(WNDCLASSEX);
    wc.style=CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    wc.cbClsExtra=0; wc.cbWndExtra=0;
    wc.lpfnWndProc=WndProc;
    wc.hInstance=hInst;
    wc.hIcon=LoadIcon(hInst,MAKEINTRESOURCE(MYICON));
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName=NULL;
    wc.lpszClassName=gpszAppName;
    wc.hIconSm=LoadIcon(hInst,MAKEINTRESOURCE(MYICON));
    RegisterClassEx(&wc);

    int sw=GetSystemMetrics(SM_CXSCREEN);
    int sh=GetSystemMetrics(SM_CYSCREEN);
    int x=(sw/2)-(WIN_WIDTH/2);
    int y=(sh/2)-(WIN_HEIGHT/2);

    gFILE=fopen("Log.txt","w");
    if(!gFILE){
        MessageBox(NULL,TEXT("Cannot open log file"),TEXT("Error"),MB_OK|MB_ICONERROR);
        return 0;
    }

    fprintf(gFILE,"LOG: Creating Main Window\n");
    ghwnd=CreateWindowEx(
        WS_EX_APPWINDOW,
        gpszAppName,
        TEXT("VulkanApp - White Triangle with Dynamic Rendering"),
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        x,y,
        WIN_WIDTH,WIN_HEIGHT,
        NULL,NULL,
        hInst,NULL);

    VkResult r=initialize();
    if(r!=VK_SUCCESS){
        fprintf(gFILE,"ERROR: initialize() failed with VkResult %d\n", r);
        DestroyWindow(ghwnd);
        ghwnd=NULL;
    }

    ShowWindow(ghwnd,iCmdShow);
    UpdateWindow(ghwnd);
    SetFocus(ghwnd);
    SetForegroundWindow(ghwnd);

    MSG msg;
    BOOL done=FALSE;
    while(!done){
        if(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT)
                done=TRUE;
            else{
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            if(gbActive){
                display();
                update();
            }
        }
    }
    uninitialize();
    return (int)msg.wParam;
}
