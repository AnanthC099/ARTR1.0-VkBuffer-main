#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// ------------------------------------------------------------------------
// Debug Callback
// ------------------------------------------------------------------------
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT ms,
    VkDebugUtilsMessageTypeFlagsEXT mt,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    fprintf(gFILE,"VALIDATION: %s\n",pCallbackData->pMessage);
    return VK_FALSE;
}

// ------------------------------------------------------------------------
// Debug Messenger Creation/Destruction
// ------------------------------------------------------------------------
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

// ------------------------------------------------------------------------
// Toggle Fullscreen
// ------------------------------------------------------------------------
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

// ------------------------------------------------------------------------
// Window Procedure
// ------------------------------------------------------------------------
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
            fprintf(gFILE, "LOG: Window gained focus.\n"); // ADDED LOG
            break;
        case WM_KILLFOCUS: 
            gbActive=FALSE; 
            fprintf(gFILE, "LOG: Window lost focus.\n"); // ADDED LOG
            break;
        case WM_SIZE:
            if(bInitialized){
                winWidth=LOWORD(lParam); 
                winHeight=HIWORD(lParam);
                fprintf(gFILE,"LOG: WM_SIZE -> resize(%d, %d)\n", winWidth, winHeight); // ADDED LOG
                if(winWidth && winHeight){
                    extern void resize(int,int);
                    resize(winWidth,winHeight);
                }
            }
            break;
        case WM_KEYDOWN:
            if(wParam==VK_ESCAPE){
                fprintf(gFILE,"LOG: ESC key pressed, destroying window.\n"); // ADDED LOG
                fclose(gFILE);gFILE=NULL; 
                DestroyWindow(hwnd);
            }
            break;
        case WM_CHAR:
            if(wParam=='f' || wParam=='F'){
                fprintf(gFILE,"LOG: Toggle fullscreen.\n"); // ADDED LOG
                ToggleFullscreen(); 
                gbFullscreen=!gbFullscreen;
            }
            break;
        case WM_CLOSE:
            fprintf(gFILE,"LOG: WM_CLOSE received, destroying window.\n"); // ADDED LOG
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            fprintf(gFILE,"LOG: WM_DESTROY received, PostQuitMessage.\n"); // ADDED LOG
            PostQuitMessage(0);
            break;
    }
    return DefWindowProc(hwnd,iMsg,wParam,lParam);
}

// ------------------------------------------------------------------------
// Vulkan: Instance/Extensions/Validation Layers
// ------------------------------------------------------------------------
static uint32_t instExtCount=0; 
static const char* instExts[4];
static uint32_t instLayerCount=0; 
static const char* instLayers[1];

static VkResult FillInstanceExtensions(void)
{
    fprintf(gFILE,"LOG: FillInstanceExtensions() Start\n"); // ADDED LOG

    uint32_t count=0; 
    vkEnumerateInstanceExtensionProperties(NULL,&count,NULL);
    if(!count) return VK_ERROR_INITIALIZATION_FAILED;

    VkExtensionProperties* arr=(VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*count);
    vkEnumerateInstanceExtensionProperties(NULL,&count,arr);

    VkBool32 foundSurface=VK_FALSE,foundWin32=VK_FALSE;
    for(uint32_t i=0;i<count;i++){
        //fprintf(gFILE,"LOG: Available Inst Ext: %s\n", arr[i].extensionName); // optional to log all
        if(!strcmp(arr[i].extensionName,VK_KHR_SURFACE_EXTENSION_NAME)) foundSurface=VK_TRUE;
        if(!strcmp(arr[i].extensionName,VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) foundWin32=VK_TRUE;
    }
    free(arr);

    if(!foundSurface||!foundWin32) {
        fprintf(gFILE,"ERROR: Required instance extensions not found!\n"); // ADDED LOG
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    instExtCount=0;
    instExts[instExtCount++]=VK_KHR_SURFACE_EXTENSION_NAME;
    instExts[instExtCount++]=VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

    fprintf(gFILE,"LOG: FillInstanceExtensions() Successful\n"); // ADDED LOG
    return VK_SUCCESS;
}

static void TryAddDebugExt(void)
{
    fprintf(gFILE,"LOG: TryAddDebugExt()\n"); // ADDED LOG
    uint32_t c=0; 
    vkEnumerateInstanceExtensionProperties(NULL,&c,NULL);
    if(!c) return;
    VkExtensionProperties* arr=(VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*c);
    vkEnumerateInstanceExtensionProperties(NULL,&c,arr);
    for(uint32_t i=0;i<c;i++){
        if(!strcmp(arr[i].extensionName,VK_EXT_DEBUG_UTILS_EXTENSION_NAME)){
            fprintf(gFILE,"LOG: Found VK_EXT_DEBUG_UTILS_EXTENSION_NAME\n"); // ADDED LOG
            instExts[instExtCount++]=VK_EXT_DEBUG_UTILS_EXTENSION_NAME; 
            break;
        }
    }
    free(arr);
}

static void TryAddValidationLayer(void)
{
    fprintf(gFILE,"LOG: TryAddValidationLayer()\n"); // ADDED LOG
    uint32_t c=0; 
    vkEnumerateInstanceLayerProperties(&c,NULL);
    if(!c) return;
    VkLayerProperties* arr=(VkLayerProperties*)malloc(sizeof(VkLayerProperties)*c);
    vkEnumerateInstanceLayerProperties(&c,arr);
    for(uint32_t i=0;i<c;i++){
        if(!strcmp(arr[i].layerName,"VK_LAYER_KHRONOS_validation")){
            instLayers[0]="VK_LAYER_KHRONOS_validation"; 
            instLayerCount=1;
            fprintf(gFILE,"LOG: Found VK_LAYER_KHRONOS_validation\n"); // ADDED LOG
            break;
        }
    }
    free(arr);
}

static VkResult CreateVulkanInstance(void)
{
    fprintf(gFILE,"LOG: CreateVulkanInstance() Start\n"); // ADDED LOG
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
        fprintf(gFILE,"ERROR: vkCreateInstance() Failed!\n"); // ADDED LOG
        return r;
    }

    // If validation layer is active, create debug messenger
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
        fprintf(gFILE,"LOG: Debug Messenger Created.\n"); // ADDED LOG
    }

    fprintf(gFILE,"LOG: CreateVulkanInstance() Successful\n"); // ADDED LOG
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// Surface Creation
// ------------------------------------------------------------------------
static VkResult CreateSurface(void)
{
    fprintf(gFILE,"LOG: CreateSurface() Start\n"); // ADDED LOG

    VkWin32SurfaceCreateInfoKHR ci={VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    ci.hinstance=(HINSTANCE)GetWindowLongPtr(ghwnd,GWLP_HINSTANCE);
    ci.hwnd=ghwnd;
    VkResult result = vkCreateWin32SurfaceKHR(vkInstance,&ci,NULL,&vkSurfaceKHR);

    if (result == VK_SUCCESS)
        fprintf(gFILE,"LOG: Surface creation successful.\n"); // ADDED LOG
    else
        fprintf(gFILE,"ERROR: Surface creation failed.\n"); // ADDED LOG

    return result;
}

// ------------------------------------------------------------------------
// Pick Physical Device
// ------------------------------------------------------------------------
static VkResult PickPhysicalDevice(void)
{
    fprintf(gFILE,"LOG: PickPhysicalDevice() Start\n"); // ADDED LOG
    uint32_t c=0; 
    vkEnumeratePhysicalDevices(vkInstance,&c,NULL);
    if(!c) return VK_ERROR_INITIALIZATION_FAILED;

    VkPhysicalDevice* devs=(VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice)*c);
    vkEnumeratePhysicalDevices(vkInstance,&c,devs);

    int chosen=-1;
    for(uint32_t i=0;i<c;i++){
        uint32_t qc=0; 
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i],&qc,NULL);
        if(!qc) continue;

        VkQueueFamilyProperties* props=(VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties)*qc);
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i],&qc,props);

        VkBool32 found=VK_FALSE;
        for(uint32_t f=0;f<qc;f++){
            VkBool32 sup=VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(devs[i],f,vkSurfaceKHR,&sup);

            if((props[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) && sup){
                vkPhysicalDevice_selected=devs[i];
                graphicsQuequeFamilyIndex_selected=f; 
                found=VK_TRUE;
                fprintf(gFILE,"LOG: Physical Device chosen at index %u, queue family %u\n", i, f); // ADDED LOG
                break;
            }
        }
        free(props);
        if(found){ chosen=i; break; }
    }
    free(devs);

    if(chosen<0) {
        fprintf(gFILE,"ERROR: No suitable physical device found.\n"); // ADDED LOG
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_selected,&vkPhysicalDeviceMemoryProperties);
    fprintf(gFILE,"LOG: PickPhysicalDevice() Successful\n"); // ADDED LOG
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// Check Device Extensions
// ------------------------------------------------------------------------
static const char* devExts[2]; 
static uint32_t devExtCount=0;

static VkResult CheckDevExtensions(void)
{
    fprintf(gFILE,"LOG: CheckDevExtensions() Start\n"); // ADDED LOG
    uint32_t c=0;
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected,NULL,&c,NULL);
    if(!c) return VK_ERROR_INITIALIZATION_FAILED;

    VkExtensionProperties* arr=(VkExtensionProperties*)malloc(sizeof(VkExtensionProperties)*c);
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected,NULL,&c,arr);

    VkBool32 haveSwap=VK_FALSE,haveDyn=VK_FALSE;
    for(uint32_t i=0;i<c;i++){
        //fprintf(gFILE,"LOG: Device Ext: %s\n", arr[i].extensionName); // optional to log all
        if(!strcmp(arr[i].extensionName,VK_KHR_SWAPCHAIN_EXTENSION_NAME)) haveSwap=VK_TRUE;
        if(!strcmp(arr[i].extensionName,"VK_KHR_dynamic_rendering")) haveDyn=VK_TRUE;
    }
    free(arr); 
    devExtCount=0;

    if(!haveSwap) {
        fprintf(gFILE,"ERROR: VK_KHR_swapchain extension not found.\n"); // ADDED LOG
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    devExts[devExtCount++]=VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    if(haveDyn) {
        fprintf(gFILE,"LOG: Found VK_KHR_dynamic_rendering extension.\n"); // ADDED LOG
        devExts[devExtCount++]="VK_KHR_dynamic_rendering";
    }

    fprintf(gFILE,"LOG: CheckDevExtensions() Successful\n"); // ADDED LOG
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// Load Dynamic Rendering Functions
// ------------------------------------------------------------------------
static void LoadDynamicRenderingFunctions(void)
{
    fprintf(gFILE,"LOG: LoadDynamicRenderingFunctions()\n"); // ADDED LOG
    pfnCmdBeginRenderingKHR=(PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(vkDevice,"vkCmdBeginRenderingKHR");
    pfnCmdEndRenderingKHR  =(PFN_vkCmdEndRenderingKHR)  vkGetDeviceProcAddr(vkDevice,"vkCmdEndRenderingKHR");
}

// ------------------------------------------------------------------------
// Create Logical Device and Queue
// ------------------------------------------------------------------------
static VkResult CreateDeviceQueue(void)
{
    fprintf(gFILE,"LOG: CreateDeviceQueue() Start\n"); // ADDED LOG
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
        fprintf(gFILE,"ERROR: vkCreateDevice() Failed!\n"); // ADDED LOG
        return r;
    }
    vkGetDeviceQueue(vkDevice,graphicsQuequeFamilyIndex_selected,0,&vkQueue);
    LoadDynamicRenderingFunctions();

    fprintf(gFILE,"LOG: CreateDeviceQueue() Successful\n"); // ADDED LOG
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// Choose Swap Format
// ------------------------------------------------------------------------
static VkResult ChooseSwapFormat(void)
{
    fprintf(gFILE,"LOG: ChooseSwapFormat() Start\n"); // ADDED LOG
    uint32_t c=0; 
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_selected,vkSurfaceKHR,&c,NULL);
    if(!c) return VK_ERROR_INITIALIZATION_FAILED;

    VkSurfaceFormatKHR* arr=(VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR)*c);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_selected,vkSurfaceKHR,&c,arr);

    if(c==1 && arr[0].format==VK_FORMAT_UNDEFINED)
        vkFormat_color=VK_FORMAT_B8G8R8A8_UNORM;
    else
        vkFormat_color=arr[0].format;

    vkColorSpaceKHR=arr[0].colorSpace;
    free(arr);
    fprintf(gFILE,"LOG: ChooseSwapFormat() -> Chosen format %d\n", vkFormat_color); // ADDED LOG
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// Choose Present Mode
// ------------------------------------------------------------------------
static VkResult ChoosePresentMode(void)
{
    fprintf(gFILE,"LOG: ChoosePresentMode() Start\n"); // ADDED LOG
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
    fprintf(gFILE,"LOG: Chosen Present Mode = %d\n", vkPresentModeKHR); // ADDED LOG
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// Create Swapchain
// ------------------------------------------------------------------------
static VkResult CreateSwapchain(void)
{
    fprintf(gFILE,"LOG: CreateSwapchain() Start\n"); // ADDED LOG
    ChooseSwapFormat(); 
    ChoosePresentMode();

    VkSurfaceCapabilitiesKHR cap; 
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        vkPhysicalDevice_selected,vkSurfaceKHR,&cap);

    uint32_t desired=cap.minImageCount+1;
    if(cap.maxImageCount>0 && desired>cap.maxImageCount) desired=cap.maxImageCount;

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
        fprintf(gFILE,"LOG: vkCreateSwapchainKHR() Successful.\n"); // ADDED LOG
    else
        fprintf(gFILE,"ERROR: vkCreateSwapchainKHR() Failed.\n"); // ADDED LOG

    return result;
}

// ------------------------------------------------------------------------
// Create Image Views
// ------------------------------------------------------------------------
static VkResult CreateImagesViews(void)
{
    fprintf(gFILE,"LOG: CreateImagesViews() Start\n"); // ADDED LOG

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
        if(r!=VK_SUCCESS) {
            fprintf(gFILE,"ERROR: vkCreateImageView() Failed for image index %u.\n", i); // ADDED LOG
            return r;
        }
    }

    fprintf(gFILE,"LOG: CreateImagesViews() Successfully created %u Image Views.\n", swapchainImageCount); // ADDED LOG
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// Create Command Pool and Buffers
// ------------------------------------------------------------------------
static VkResult CreateCommandPoolBuffers(void)
{
    fprintf(gFILE,"LOG: CreateCommandPoolBuffers() Start\n"); // ADDED LOG

    VkCommandPoolCreateInfo cpci={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex=graphicsQuequeFamilyIndex_selected;
    cpci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult r=vkCreateCommandPool(vkDevice,&cpci,NULL,&vkCommandPool);
    if(r!=VK_SUCCESS){
        fprintf(gFILE,"ERROR: vkCreateCommandPool() Failed!\n"); // ADDED LOG
        return r;
    }

    VkCommandBufferAllocateInfo cbai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool=vkCommandPool;
    cbai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount=1;

    vkCommandBuffer_array=(VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*swapchainImageCount);
    for(uint32_t i=0;i<swapchainImageCount;i++){
        r=vkAllocateCommandBuffers(vkDevice,&cbai,&vkCommandBuffer_array[i]);
        if(r!=VK_SUCCESS){
            fprintf(gFILE,"ERROR: vkAllocateCommandBuffers() Failed for index %u!\n", i); // ADDED LOG
            return r;
        }
    }

    fprintf(gFILE,"LOG: CreateCommandPoolBuffers() Successful\n"); // ADDED LOG
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// Build Command Buffers
// ------------------------------------------------------------------------
static VkResult buildCommandBuffers(void)
{
    fprintf(gFILE,"LOG: buildCommandBuffers() Start\n"); // ADDED LOG

    for(uint32_t i=0;i<swapchainImageCount;i++){
        vkResetCommandBuffer(vkCommandBuffer_array[i],0);

        VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(vkCommandBuffer_array[i],&bi);

        // Transition to COLOR_ATTACHMENT_OPTIMAL
        VkImageMemoryBarrier toColor={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toColor.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        toColor.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toColor.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        toColor.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        toColor.image=swapChainImage_array[i];
        toColor.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        toColor.subresourceRange.levelCount=1;
        toColor.subresourceRange.layerCount=1;
        toColor.srcAccessMask=0;
        toColor.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(
            vkCommandBuffer_array[i],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,0,NULL,0,NULL,1,&toColor
        );

        // Dynamic Rendering: Clear
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

        pfnCmdBeginRenderingKHR(vkCommandBuffer_array[i],&ri);
        // No draw commands, just clearing
        pfnCmdEndRenderingKHR(vkCommandBuffer_array[i]);

        // Transition to PRESENT_SRC_KHR
        VkImageMemoryBarrier toPresent={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toPresent.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toPresent.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        toPresent.image=swapChainImage_array[i];
        toPresent.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        toPresent.subresourceRange.levelCount=1;
        toPresent.subresourceRange.layerCount=1;
        toPresent.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toPresent.dstAccessMask=0;

        vkCmdPipelineBarrier(
            vkCommandBuffer_array[i],
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,0,NULL,0,NULL,1,&toPresent
        );

        vkEndCommandBuffer(vkCommandBuffer_array[i]);
    }

    fprintf(gFILE,"LOG: buildCommandBuffers() Successful\n"); // ADDED LOG
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// Create Synchronization Objects
// ------------------------------------------------------------------------
static VkResult CreateSyncObjects(void)
{
    fprintf(gFILE,"LOG: CreateSyncObjects() Start\n"); // ADDED LOG
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

    fprintf(gFILE,"LOG: CreateSyncObjects() Successful\n"); // ADDED LOG
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// Cleanup Swapchain
// ------------------------------------------------------------------------
static void cleanupSwapchain(void)
{
    fprintf(gFILE,"LOG: cleanupSwapchain()\n"); // ADDED LOG
    vkDeviceWaitIdle(vkDevice);

    if(vkCommandBuffer_array){
        for(uint32_t i=0;i<swapchainImageCount;i++)
            vkFreeCommandBuffers(vkDevice,vkCommandPool,1,&vkCommandBuffer_array[i]);
        free(vkCommandBuffer_array); 
        vkCommandBuffer_array=NULL;
    }
    if(vkCommandPool){
        vkDestroyCommandPool(vkDevice,vkCommandPool,NULL);
        vkCommandPool=VK_NULL_HANDLE;
    }
    if(swapChainImageView_array){
        for(uint32_t i=0;i<swapchainImageCount;i++)
            vkDestroyImageView(vkDevice,swapChainImageView_array[i],NULL);
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

// ------------------------------------------------------------------------
// Recreate Swapchain
// ------------------------------------------------------------------------
static VkResult recreateSwapchain(void)
{
    fprintf(gFILE,"LOG: recreateSwapchain() Start\n"); // ADDED LOG
    if(winWidth==0||winHeight==0) return VK_SUCCESS;

    cleanupSwapchain();

    VkResult r=CreateSwapchain(); 
    if(r!=VK_SUCCESS) return r;

    r=CreateImagesViews(); 
    if(r!=VK_SUCCESS) return r;

    r=CreateCommandPoolBuffers(); 
    if(r!=VK_SUCCESS) return r;

    return buildCommandBuffers();
}

// ------------------------------------------------------------------------
// Resize
// ------------------------------------------------------------------------
void resize(int w,int h)
{
    fprintf(gFILE,"LOG: resize(%d, %d)\n", w, h); // ADDED LOG
    winWidth=w; 
    winHeight=h;
    if(winWidth && winHeight) 
        recreateSwapchain();
}

// ------------------------------------------------------------------------
// Draw Frame
// ------------------------------------------------------------------------
static VkResult drawFrame(void)
{
    // Wait for the GPU to finish with this frame
    vkWaitForFences(vkDevice,1,&inFlightFences[currentFrame],VK_TRUE,UINT64_MAX);
    vkResetFences(vkDevice,1,&inFlightFences[currentFrame]);

    // Acquire next image
    uint32_t imgIndex;
    VkResult r=vkAcquireNextImageKHR(
        vkDevice,
        vkSwapchainKHR,
        UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE,
        &imgIndex);

    if(r==VK_ERROR_OUT_OF_DATE_KHR){
        fprintf(gFILE,"LOG: vkAcquireNextImageKHR -> VK_ERROR_OUT_OF_DATE_KHR, recreateSwapchain()\n"); // ADDED LOG
        recreateSwapchain();
        return VK_SUCCESS;
    }
    if(r!=VK_SUCCESS && r!=VK_SUBOPTIMAL_KHR) 
        return r;

    // Submit command buffer
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
    if(r!=VK_SUCCESS) {
        fprintf(gFILE,"ERROR: vkQueueSubmit failed!\n"); // ADDED LOG
        return r;
    }

    // Present
    VkPresentInfoKHR pi={VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount=1;
    pi.pWaitSemaphores=&renderFinishedSemaphores[currentFrame];
    pi.swapchainCount=1;
    pi.pSwapchains=&vkSwapchainKHR;
    pi.pImageIndices=&imgIndex;

    r=vkQueuePresentKHR(vkQueue,&pi);
    if(r==VK_ERROR_OUT_OF_DATE_KHR || r==VK_SUBOPTIMAL_KHR){
        fprintf(gFILE,"LOG: vkQueuePresentKHR -> out_of_date or suboptimal, recreateSwapchain()\n"); // ADDED LOG
        recreateSwapchain();
    }

    currentFrame=(currentFrame+1)%MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

// ------------------------------------------------------------------------
// App-level Initialize, Display (drawFrame), Update, Uninitialize
// ------------------------------------------------------------------------
VkResult initialize(void)
{
    fprintf(gFILE,"LOG: initialize() Start\n"); // ADDED LOG
    VkResult r=CreateVulkanInstance(); 
    if(r!=VK_SUCCESS) return r;

    r=CreateSurface(); 
    if(r!=VK_SUCCESS) return r;

    r=PickPhysicalDevice(); 
    if(r!=VK_SUCCESS) return r;

    r=CreateDeviceQueue(); 
    if(r!=VK_SUCCESS) return r;

    r=CreateSwapchain(); 
    if(r!=VK_SUCCESS) return r;

    r=CreateImagesViews(); 
    if(r!=VK_SUCCESS) return r;

    r=CreateCommandPoolBuffers(); 
    if(r!=VK_SUCCESS) return r;

    r=CreateSyncObjects(); 
    if(r!=VK_SUCCESS) return r;

    memset(&vkClearColorValue,0,sizeof(vkClearColorValue));
    vkClearColorValue.float32[0]=0.0f;
    vkClearColorValue.float32[1]=0.0f;
    vkClearColorValue.float32[2]=1.0f;
    vkClearColorValue.float32[3]=1.0f;

    r=buildCommandBuffers(); 
    if(r!=VK_SUCCESS) return r;

    bInitialized=TRUE;
    fprintf(gFILE,"LOG: initialize() Successful\n"); // ADDED LOG
    return VK_SUCCESS;
}

VkResult display(void){ 
    // We only do one call to drawFrame here
    return drawFrame(); 
}

void update(void){
    // Put any dynamic updates here if needed
    // For now, no logs since it's empty
}

void uninitialize(void)
{
    fprintf(gFILE,"LOG: uninitialize() Start\n"); // ADDED LOG
    if(gbFullscreen){
        ToggleFullscreen(); 
        gbFullscreen=FALSE;
    }

    if(vkDevice){
        vkDeviceWaitIdle(vkDevice);

        // Destroy sync objects
        for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
            if(inFlightFences[i]) vkDestroyFence(vkDevice,inFlightFences[i],NULL);
            if(renderFinishedSemaphores[i]) vkDestroySemaphore(vkDevice,renderFinishedSemaphores[i],NULL);
            if(imageAvailableSemaphores[i]) vkDestroySemaphore(vkDevice,imageAvailableSemaphores[i],NULL);
        }

        cleanupSwapchain();
        vkDestroyDevice(vkDevice,NULL);
    }

    if(gDebugUtilsMessenger) 
        DestroyDebugUtilsMessengerEXT(vkInstance,gDebugUtilsMessenger,NULL);

    if(vkSurfaceKHR) 
        vkDestroySurfaceKHR(vkInstance,vkSurfaceKHR,NULL);

    if(vkInstance) 
        vkDestroyInstance(vkInstance,NULL);

    if(ghwnd){
        DestroyWindow(ghwnd); 
        ghwnd=NULL;
    }

    if(gFILE){
        fprintf(gFILE,"Program ended.\n"); 
        fclose(gFILE);
        gFILE=NULL;
    }
    fprintf(gFILE,"LOG: uninitialize() End\n"); // ADDED LOG (will print only if gFILE still open)
}

// ------------------------------------------------------------------------
// WinMain
// ------------------------------------------------------------------------
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

    fprintf(gFILE,"LOG: Creating Main Window\n"); // ADDED LOG
    ghwnd=CreateWindowEx(
        WS_EX_APPWINDOW,
        gpszAppName,
        TEXT("DynamicRenderingNoLinkError - with logs"),
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        x,y,
        WIN_WIDTH,WIN_HEIGHT,
        NULL,NULL,
        hInst,NULL);

    VkResult r=initialize(); 
    if(r!=VK_SUCCESS){
        fprintf(gFILE,"ERROR: initialize() failed with VkResult %d\n", r); // ADDED LOG
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
