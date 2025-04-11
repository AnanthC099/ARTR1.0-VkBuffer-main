#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fstream>
#include <vector>
#include <array>

#include "vmath.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

#define WIN_WIDTH  800
#define WIN_HEIGHT 600
#define MAX_FRAMES_IN_FLIGHT 2

const char* gpszAppName = "Vulkan_3D_TwoObjects_Wireframe";
HWND  ghwnd             = NULL;
BOOL  gbActive          = FALSE;
DWORD dwStyle           = 0;
WINDOWPLACEMENT wpPrev{ sizeof(WINDOWPLACEMENT) };
BOOL  gbFullscreen      = FALSE;

FILE* gFILE             = nullptr;
bool  gEnableValidation = true;

static VkDebugUtilsMessengerEXT gDebugUtilsMessenger = VK_NULL_HANDLE;
VkInstance          vkInstance              = VK_NULL_HANDLE;
VkSurfaceKHR        vkSurfaceKHR            = VK_NULL_HANDLE;
VkPhysicalDevice    vkPhysicalDevice_sel    = VK_NULL_HANDLE;
VkDevice            vkDevice                = VK_NULL_HANDLE;
VkQueue             vkQueue                 = VK_NULL_HANDLE;
uint32_t            graphicsQueueIndex      = UINT32_MAX;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties{};

VkFormat            vkFormat_color          = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR     vkColorSpaceKHR         = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR    vkPresentModeKHR        = VK_PRESENT_MODE_FIFO_KHR;

VkSwapchainKHR      vkSwapchainKHR          = VK_NULL_HANDLE;
uint32_t            swapchainImageCount     = 0;
VkImage*            swapChainImage_array    = nullptr;
VkImageView*        swapChainImageView_array= nullptr;
VkExtent2D          vkExtent2D_SwapChain{ WIN_WIDTH, WIN_HEIGHT };

VkCommandPool       vkCommandPool           = VK_NULL_HANDLE;
VkCommandBuffer*    vkCommandBuffer_array   = nullptr;

VkSemaphore         imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
VkSemaphore         renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
VkFence             inFlightFences[MAX_FRAMES_IN_FLIGHT];
uint32_t            currentFrame            = 0;
uint32_t            currentImageIndex       = UINT32_MAX;

VkClearColorValue   vkClearColorValue{};
int                 winWidth                = WIN_WIDTH;
int                 winHeight               = WIN_HEIGHT;
BOOL                bInitialized            = FALSE;

VkRenderPass        gRenderPass             = VK_NULL_HANDLE;
VkFramebuffer*      gFramebuffers           = nullptr;

struct Vertex {
    vmath::vec3 pos;
    vmath::vec3 color;
};

static std::vector<Vertex> CreateCubeVertices() {
    std::vector<Vertex> out;
    out.push_back({ {-1, -1, +1}, {1,0,0} }); out.push_back({ {+1, -1, +1}, {1,0,0} });
    out.push_back({ {+1, +1, +1}, {1,0,0} }); out.push_back({ {+1, +1, +1}, {1,0,0} });
    out.push_back({ {-1, +1, +1}, {1,0,0} }); out.push_back({ {-1, -1, +1}, {1,0,0} });
    out.push_back({ {+1, -1, -1}, {0,1,0} }); out.push_back({ {-1, -1, -1}, {0,1,0} });
    out.push_back({ {-1, +1, -1}, {0,1,0} }); out.push_back({ {-1, +1, -1}, {0,1,0} });
    out.push_back({ {+1, +1, -1}, {0,1,0} }); out.push_back({ {+1, -1, -1}, {0,1,0} });
    out.push_back({ {-1,+1,+1}, {0,0,1} });   out.push_back({ {-1,+1,-1}, {0,0,1} });
    out.push_back({ {-1,-1,-1}, {0,0,1} });   out.push_back({ {-1,-1,-1}, {0,0,1} });
    out.push_back({ {-1,-1,+1}, {0,0,1} });   out.push_back({ {-1,+1,+1}, {0,0,1} });
    out.push_back({ {+1,+1,-1}, {0,1,1} });   out.push_back({ {+1,+1,+1}, {0,1,1} });
    out.push_back({ {+1,-1,+1}, {0,1,1} });   out.push_back({ {+1,-1,+1}, {0,1,1} });
    out.push_back({ {+1,-1,-1}, {0,1,1} });   out.push_back({ {+1,+1,-1}, {0,1,1} });
    out.push_back({ {-1,+1,-1}, {1,0,1} });   out.push_back({ {-1,+1,+1}, {1,0,1} });
    out.push_back({ {+1,+1,+1}, {1,0,1} });   out.push_back({ {+1,+1,+1}, {1,0,1} });
    out.push_back({ {+1,+1,-1}, {1,0,1} });   out.push_back({ {-1,+1,-1}, {1,0,1} });
    out.push_back({ {-1,-1,+1}, {1,1,0} });   out.push_back({ {-1,-1,-1}, {1,1,0} });
    out.push_back({ {+1,-1,-1}, {1,1,0} });   out.push_back({ {+1,-1,-1}, {1,1,0} });
    out.push_back({ {+1,-1,+1}, {1,1,0} });   out.push_back({ {-1,-1,+1}, {1,1,0} });
    return out;
}
static std::vector<Vertex> CreatePyramidVertices() {
    std::vector<Vertex> out;
    Vertex apex { {0,1,0}, {1,0,0} };
    Vertex v0   { {-1,0,-1}, {0,1,0} };
    Vertex v1   { {+1,0,-1}, {0,0,1} };
    Vertex v2   { {+1,0,+1}, {1,1,0} };
    Vertex v3   { {-1,0,+1}, {1,0,1} };
    out.push_back(apex); out.push_back(v0); out.push_back(v1);
    out.push_back(apex); out.push_back(v1); out.push_back(v2);
    out.push_back(apex); out.push_back(v2); out.push_back(v3);
    out.push_back(apex); out.push_back(v3); out.push_back(v0);
    return out;
}
static std::vector<Vertex> gCubeVertices    = CreateCubeVertices();
static std::vector<Vertex> gPyramidVertices = CreatePyramidVertices();

struct UniformBufferObject { vmath::mat4 mvp; };

VkBuffer       gUniformBufferCube             = VK_NULL_HANDLE;
VkDeviceMemory gUniformBufferMemoryCube       = VK_NULL_HANDLE;
VkBuffer       gUniformBufferPyramid          = VK_NULL_HANDLE;
VkDeviceMemory gUniformBufferMemoryPyramid    = VK_NULL_HANDLE;

VkDescriptorSetLayout gDescriptorSetLayout    = VK_NULL_HANDLE;
VkDescriptorPool      gDescriptorPool         = VK_NULL_HANDLE;
VkDescriptorSet       gDescriptorSetCube      = VK_NULL_HANDLE;
VkDescriptorSet       gDescriptorSetPyramid   = VK_NULL_HANDLE;

VkBuffer       gVertexBufferCube             = VK_NULL_HANDLE;
VkDeviceMemory gVertexBufferMemoryCube       = VK_NULL_HANDLE;
VkBuffer       gVertexBufferPyramid          = VK_NULL_HANDLE;
VkDeviceMemory gVertexBufferMemoryPyramid    = VK_NULL_HANDLE;

VkPipelineLayout gPipelineLayout             = VK_NULL_HANDLE;
VkPipeline       gGraphicsPipeline           = VK_NULL_HANDLE;

static VkFormat gDepthFormat                 = VK_FORMAT_UNDEFINED;
std::vector<VkImage>        gDepthImages;
std::vector<VkDeviceMemory> gDepthImagesMemory;
std::vector<VkImageView>    gDepthImagesView;

void ToggleFullscreen(void);

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
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
        if(bInitialized) {
            winWidth  = LOWORD(lParam);
            winHeight = HIWORD(lParam);
            if (winWidth > 0 && winHeight > 0) {
                extern VkResult recreateSwapChain(void);
                recreateSwapChain();
            }
        }
        break;
    case WM_KEYDOWN:
        if(LOWORD(wParam) == VK_ESCAPE) PostQuitMessage(0);
        break;
    case WM_CHAR:
        switch(LOWORD(wParam)) {
        case 'F': case 'f':
            if(!gbFullscreen) { 
                extern void ToggleFullscreen(void);
                ToggleFullscreen(); 
                gbFullscreen=TRUE;
            } else { 
                ToggleFullscreen();
                gbFullscreen=FALSE;
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
    if (func) return func(instance, pCreateInfo, pAllocator, pMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance               instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func) func(instance, messenger, pAllocator);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow) {
    gFILE = fopen("Log.txt", "w");
    if(!gFILE) return 0;

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
        TEXT("Vulkan 3D Two Objects (Wireframe)"),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
        xCoord, yCoord, WIN_WIDTH, WIN_HEIGHT, NULL, NULL, hInstance, NULL
    );
    if(!ghwnd) return 0;

    extern VkResult initialize(void);
    if(initialize() != VK_SUCCESS) {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    ShowWindow(ghwnd, iCmdShow);
    UpdateWindow(ghwnd);
    SetForegroundWindow(ghwnd);
    SetFocus(ghwnd);

    MSG msg{};
    BOOL bDone = FALSE;
    while(!bDone) {
        if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if(msg.message==WM_QUIT) bDone=TRUE;
            else { TranslateMessage(&msg); DispatchMessage(&msg); }
        } else {
            if(gbActive) {
                extern VkResult display(void);
                display();
                extern void update(void);
                update();
            }
        }
    }
    extern void uninitialize(void);
    uninitialize();
    return (int)msg.wParam;
}

void ToggleFullscreen(void) {
    MONITORINFO mi { sizeof(MONITORINFO) };
    if(!gbFullscreen) {
        dwStyle=GetWindowLong(ghwnd,GWL_STYLE);
        if(dwStyle & WS_OVERLAPPEDWINDOW) {
            if(GetWindowPlacement(ghwnd,&wpPrev) &&
               GetMonitorInfo(MonitorFromWindow(ghwnd,MONITORINFOF_PRIMARY),&mi))
            {
                SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(
                    ghwnd, HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOZORDER | SWP_FRAMECHANGED
                );
            }
        }
        ShowCursor(FALSE);
    } else {
        SetWindowPlacement(ghwnd,&wpPrev);
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(
            ghwnd,
            HWND_TOP,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );
        ShowCursor(TRUE);
    }
}

static std::vector<char> ReadFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if(!file.is_open()) return {};
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkResult initialize(void);
void     uninitialize(void);
VkResult display(void);
void     update(void);
VkResult recreateSwapChain(void);

static VkResult CreateVulkanInstance();
static VkResult CreateSurfaceWin32();
static VkResult SelectPhysicalDevice();
static VkResult CreateLogicalDeviceAndQueue();
static VkResult CreateSwapChain(VkBool32 vsync=VK_FALSE);
static VkResult CreateImagesAndImageViews();
static VkResult CreateCommandPool();
static VkResult CreateCommandBuffers();
static VkResult CreateSemaphores();
static VkResult CreateFences();
static VkResult CreateDepthResources();
static VkResult CreateRenderPass();
static VkResult CreateFramebuffers();
static VkResult CreateCubeVertexBuffer();
static VkResult CreatePyramidVertexBuffer();
static VkResult CreateUniformBufferCube();
static VkResult CreateUniformBufferPyramid();
static VkResult CreateDescriptorSetLayout();
static VkResult CreateDescriptorPool();
static VkResult CreateDescriptorSets();
static VkResult CreateGraphicsPipeline();
static VkResult buildCommandBuffers();
static void     cleanupSwapChain();
static void     cleanupDepthResources();

VkResult initialize(void) {
    VkResult vkResult;
    vkResult = CreateVulkanInstance();         if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateSurfaceWin32();           if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = SelectPhysicalDevice();         if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateLogicalDeviceAndQueue();  if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateSwapChain();             if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateImagesAndImageViews();   if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateDepthResources();        if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateRenderPass();            if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateFramebuffers();          if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateCommandPool();           if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateCommandBuffers();        if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateDescriptorSetLayout();   if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateUniformBufferCube();     if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateUniformBufferPyramid();  if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateDescriptorPool();        if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateDescriptorSets();        if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateCubeVertexBuffer();      if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreatePyramidVertexBuffer();   if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateGraphicsPipeline();      if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateSemaphores();           if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = CreateFences();               if (vkResult != VK_SUCCESS) return vkResult;

    memset(&vkClearColorValue,0,sizeof(vkClearColorValue));
    vkClearColorValue.float32[0]=0.1f; 
    vkClearColorValue.float32[1]=0.1f;
    vkClearColorValue.float32[2]=0.1f; 
    vkClearColorValue.float32[3]=1.0f;

    vkResult = buildCommandBuffers(); if (vkResult != VK_SUCCESS) return vkResult;
    bInitialized=TRUE;
    return VK_SUCCESS;
}

void update(void) {
    static float angle=0.0f;
    angle+=0.75f; if(angle>=360.0f) angle-=360.0f;

    {
        UniformBufferObject ubo{};
        vmath::mat4 T = vmath::translate(-1.5f, 0.0f, -6.0f);
        vmath::mat4 R = vmath::rotate(angle, 0.0f,1.0f,0.0f);
        vmath::mat4 model = T*R;
        float aspect = (winHeight==0)?1.0f: (float)winWidth/(float)winHeight;
        vmath::mat4 proj  = vmath::perspective(45.0f, aspect, 0.1f,100.0f);
        proj[1][1]*=-1.0f;
        ubo.mvp = proj * vmath::mat4::identity() * model;
        void* data;
        vkMapMemory(vkDevice, gUniformBufferMemoryCube, 0, sizeof(ubo),0,&data);
        memcpy(data,&ubo,sizeof(ubo));
        vkUnmapMemory(vkDevice, gUniformBufferMemoryCube);
    }
    {
        UniformBufferObject ubo{};
        vmath::mat4 T = vmath::translate(+1.5f, 0.0f, -6.0f);
        vmath::mat4 R = vmath::rotate(angle, 1.0f,0.0f,0.0f);
        vmath::mat4 model = T*R;
        float aspect = (winHeight==0)?1.0f: (float)winWidth/(float)winHeight;
        vmath::mat4 proj  = vmath::perspective(45.0f, aspect, 0.1f,100.0f);
        proj[1][1]*=-1.0f;
        ubo.mvp = proj * vmath::mat4::identity() * model;
        void* data;
        vkMapMemory(vkDevice, gUniformBufferMemoryPyramid,0,sizeof(ubo),0,&data);
        memcpy(data,&ubo,sizeof(ubo));
        vkUnmapMemory(vkDevice, gUniformBufferMemoryPyramid);
    }
}

VkResult display(void) {
    if(!bInitialized) return VK_SUCCESS;
    VkResult r = vkWaitForFences(vkDevice,1,&inFlightFences[currentFrame],VK_TRUE,UINT64_MAX);
    if(r!=VK_SUCCESS) return r;
    vkResetFences(vkDevice,1,&inFlightFences[currentFrame]);

    r = vkAcquireNextImageKHR(vkDevice,vkSwapchainKHR,UINT64_MAX,
        imageAvailableSemaphores[currentFrame],VK_NULL_HANDLE,&currentImageIndex);
    if(r==VK_ERROR_OUT_OF_DATE_KHR){ recreateSwapChain(); return VK_SUCCESS; }
    else if(r!=VK_SUCCESS && r!=VK_SUBOPTIMAL_KHR) return r;

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
    r = vkQueueSubmit(vkQueue,1,&submitInfo,inFlightFences[currentFrame]);
    if(r!=VK_SUCCESS) return r;

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinishedSemaphores[currentFrame];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &vkSwapchainKHR;
    presentInfo.pImageIndices      = &currentImageIndex;
    r = vkQueuePresentKHR(vkQueue,&presentInfo);
    if(r==VK_ERROR_OUT_OF_DATE_KHR||r==VK_SUBOPTIMAL_KHR) recreateSwapChain();

    currentFrame=(currentFrame+1)%MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

VkResult recreateSwapChain(void) {
    if(winWidth==0||winHeight==0) return VK_SUCCESS;
    cleanupSwapChain();
    VkResult r;
    r=CreateSwapChain();           if(r!=VK_SUCCESS) return r;
    r=CreateImagesAndImageViews(); if(r!=VK_SUCCESS) return r;
    r=CreateDepthResources();      if(r!=VK_SUCCESS) return r;
    r=CreateRenderPass();          if(r!=VK_SUCCESS) return r;
    r=CreateFramebuffers();        if(r!=VK_SUCCESS) return r;
    r=CreateCommandPool();         if(r!=VK_SUCCESS) return r;
    r=CreateCommandBuffers();      if(r!=VK_SUCCESS) return r;
    if(gGraphicsPipeline) { vkDestroyPipeline(vkDevice,gGraphicsPipeline,nullptr); gGraphicsPipeline=VK_NULL_HANDLE; }
    if(gPipelineLayout)   { vkDestroyPipelineLayout(vkDevice,gPipelineLayout,nullptr); gPipelineLayout=VK_NULL_HANDLE; }
    r=CreateGraphicsPipeline();    if(r!=VK_SUCCESS) return r;
    r=buildCommandBuffers();       if(r!=VK_SUCCESS) return r;
    return VK_SUCCESS;
}

void uninitialize(void) {
    if(gbFullscreen) { ToggleFullscreen(); gbFullscreen=FALSE; }
    if(vkDevice) {
        vkDeviceWaitIdle(vkDevice);
        for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
            if(inFlightFences[i])           vkDestroyFence(vkDevice,inFlightFences[i],nullptr);
            if(renderFinishedSemaphores[i]) vkDestroySemaphore(vkDevice,renderFinishedSemaphores[i],nullptr);
            if(imageAvailableSemaphores[i]) vkDestroySemaphore(vkDevice,imageAvailableSemaphores[i],nullptr);
        }
        cleanupSwapChain();
        if(gGraphicsPipeline)    vkDestroyPipeline(vkDevice,gGraphicsPipeline,nullptr);
        if(gPipelineLayout)      vkDestroyPipelineLayout(vkDevice,gPipelineLayout,nullptr);
        if(gDescriptorPool)      vkDestroyDescriptorPool(vkDevice,gDescriptorPool,nullptr);
        if(gDescriptorSetLayout) vkDestroyDescriptorSetLayout(vkDevice,gDescriptorSetLayout,nullptr);
        if(gUniformBufferCube)       vkDestroyBuffer(vkDevice,gUniformBufferCube,nullptr);
        if(gUniformBufferMemoryCube) vkFreeMemory(vkDevice,gUniformBufferMemoryCube,nullptr);
        if(gUniformBufferPyramid)       vkDestroyBuffer(vkDevice,gUniformBufferPyramid,nullptr);
        if(gUniformBufferMemoryPyramid) vkFreeMemory(vkDevice,gUniformBufferMemoryPyramid,nullptr);
        if(gVertexBufferCube)        vkDestroyBuffer(vkDevice,gVertexBufferCube,nullptr);
        if(gVertexBufferMemoryCube)  vkFreeMemory(vkDevice,gVertexBufferMemoryCube,nullptr);
        if(gVertexBufferPyramid)     vkDestroyBuffer(vkDevice,gVertexBufferPyramid,nullptr);
        if(gVertexBufferMemoryPyramid) vkFreeMemory(vkDevice,gVertexBufferMemoryPyramid,nullptr);
        vkDestroyDevice(vkDevice,nullptr);
    }
    if(gDebugUtilsMessenger) {
        DestroyDebugUtilsMessengerEXT(vkInstance,gDebugUtilsMessenger,nullptr);
        gDebugUtilsMessenger=VK_NULL_HANDLE;
    }
    if(vkSurfaceKHR) { vkDestroySurfaceKHR(vkInstance,vkSurfaceKHR,nullptr); vkSurfaceKHR=VK_NULL_HANDLE; }
    if(vkInstance)   { vkDestroyInstance(vkInstance,nullptr); vkInstance=VK_NULL_HANDLE; }
    if(ghwnd) { DestroyWindow(ghwnd); ghwnd=nullptr; }
    if(gFILE) { fprintf(gFILE,"Exiting\n"); fclose(gFILE); gFILE=nullptr; }
}

static uint32_t enabledInstanceLayerCount=0;
static const char* enabledInstanceLayerNames_array[1];

static VkResult FillInstanceLayerNames() {
    if(!gEnableValidation) return VK_SUCCESS;
    uint32_t layerCount=0; vkEnumerateInstanceLayerProperties(&layerCount,nullptr);
    if(layerCount==0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount,layers.data());
    bool found=false; for(auto &lp:layers){ if(strcmp(lp.layerName,"VK_LAYER_KHRONOS_validation")==0){ found=true; break;} }
    if(found){ enabledInstanceLayerNames_array[0]="VK_LAYER_KHRONOS_validation"; enabledInstanceLayerCount=1;}
    return VK_SUCCESS;
}

static uint32_t enabledInstanceExtensionsCount=0;
static const char* enabledInstanceExtensionNames_array[3];
static void AddDebugUtilsExtensionIfPresent() {
    if(!gEnableValidation) return;
    uint32_t extCount=0; vkEnumerateInstanceExtensionProperties(nullptr,&extCount,nullptr);
    if(extCount==0) return;
    std::vector<VkExtensionProperties> props(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr,&extCount,props.data());
    for(auto &ep:props) { if(strcmp(ep.extensionName,VK_EXT_DEBUG_UTILS_EXTENSION_NAME)==0){
        enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++]=VK_EXT_DEBUG_UTILS_EXTENSION_NAME; return;
    }}
}
static VkResult FillInstanceExtensionNames() {
    uint32_t extensionCount=0; vkEnumerateInstanceExtensionProperties(nullptr,&extensionCount,nullptr);
    if(extensionCount==0) return VK_ERROR_INITIALIZATION_FAILED;
    bool foundSurface=false; bool foundWin32Surface=false;
    std::vector<VkExtensionProperties> exts(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr,&extensionCount,exts.data());
    for(auto &e: exts){
        if(strcmp(e.extensionName,VK_KHR_SURFACE_EXTENSION_NAME)==0) foundSurface=true;
        if(strcmp(e.extensionName,VK_KHR_WIN32_SURFACE_EXTENSION_NAME)==0) foundWin32Surface=true;
    }
    if(!foundSurface||!foundWin32Surface) return VK_ERROR_INITIALIZATION_FAILED;
    enabledInstanceExtensionsCount=0;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++]=VK_KHR_SURFACE_EXTENSION_NAME;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++]=VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
    AddDebugUtilsExtensionIfPresent();
    return VK_SUCCESS;
}

static VkResult CreateVulkanInstance() {
    VkResult r=FillInstanceExtensionNames(); if(r!=VK_SUCCESS) return r;
    r=FillInstanceLayerNames(); if(r!=VK_SUCCESS) return r;
    VkApplicationInfo appInfo{};
    appInfo.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName=gpszAppName; appInfo.applicationVersion=1;
    appInfo.pEngineName=gpszAppName; appInfo.engineVersion=1;
	appInfo.apiVersion=VK_API_VERSION_1_4;
    VkInstanceCreateInfo createInfo{};
    createInfo.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo=&appInfo;
    createInfo.enabledExtensionCount=enabledInstanceExtensionsCount;
    createInfo.ppEnabledExtensionNames=enabledInstanceExtensionNames_array;
    createInfo.enabledLayerCount=enabledInstanceLayerCount;
    createInfo.ppEnabledLayerNames=enabledInstanceLayerNames_array;
    VkResult res=vkCreateInstance(&createInfo,nullptr,&vkInstance);
    if(res!=VK_SUCCESS) return res;
    if(gEnableValidation && enabledInstanceLayerCount>0){
        VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo{};
        dbgCreateInfo.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgCreateInfo.messageSeverity=
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgCreateInfo.messageType=
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgCreateInfo.pfnUserCallback=DebugCallback;
        CreateDebugUtilsMessengerEXT(vkInstance,&dbgCreateInfo,nullptr,&gDebugUtilsMessenger);
    }
    return VK_SUCCESS;
}
static VkResult CreateSurfaceWin32() {
    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hinstance=(HINSTANCE)GetWindowLongPtr(ghwnd,GWLP_HINSTANCE);
    ci.hwnd=ghwnd;
    return vkCreateWin32SurfaceKHR(vkInstance,&ci,nullptr,&vkSurfaceKHR);
}
static VkResult SelectPhysicalDevice() {
    uint32_t devCount=0; vkEnumeratePhysicalDevices(vkInstance,&devCount,nullptr);
    if(devCount==0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkPhysicalDevice> devs(devCount);
    vkEnumeratePhysicalDevices(vkInstance,&devCount,devs.data());
    bool found=false;
    for(auto &pd: devs){
        uint32_t qCount=0; vkGetPhysicalDeviceQueueFamilyProperties(pd,&qCount,nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd,&qCount,qProps.data());
        std::vector<VkBool32> canPresent(qCount,VK_FALSE);
        for(uint32_t i=0;i<qCount;i++){
            vkGetPhysicalDeviceSurfaceSupportKHR(pd,i,vkSurfaceKHR,&canPresent[i]);
        }
        for(uint32_t i=0;i<qCount;i++){
            if((qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent[i]==VK_TRUE) {
                vkPhysicalDevice_sel=pd; graphicsQueueIndex=i; found=true; break;
            }
        }
        if(found) break;
    }
    if(!found) return VK_ERROR_INITIALIZATION_FAILED;
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_sel,&vkPhysicalDeviceMemoryProperties);
    return VK_SUCCESS;
}
static uint32_t enabledDeviceExtensionsCount=0;
static const char* enabledDeviceExtensionNames_array[1];
static VkResult FillDeviceExtensionNames(){
    uint32_t extCount=0; vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel,nullptr,&extCount,nullptr);
    if(extCount==0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel,nullptr,&extCount,exts.data());
    bool foundSwapchain=false;
    for(auto &e: exts){ if(strcmp(e.extensionName,VK_KHR_SWAPCHAIN_EXTENSION_NAME)==0) {foundSwapchain=true;break;} }
    if(!foundSwapchain) return VK_ERROR_INITIALIZATION_FAILED;
    enabledDeviceExtensionsCount=0;
    enabledDeviceExtensionNames_array[enabledDeviceExtensionsCount++]=VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    return VK_SUCCESS;
}
static VkResult CreateLogicalDeviceAndQueue() {
    VkResult r=FillDeviceExtensionNames(); if(r!=VK_SUCCESS) return r;
    float qp=1.0f;
    VkDeviceQueueCreateInfo qInfo{};
    qInfo.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qInfo.queueFamilyIndex=graphicsQueueIndex; qInfo.queueCount=1; qInfo.pQueuePriorities=&qp;
    VkPhysicalDeviceFeatures supf{}; vkGetPhysicalDeviceFeatures(vkPhysicalDevice_sel,&supf);
    VkPhysicalDeviceFeatures df{}; if(supf.fillModeNonSolid) df.fillModeNonSolid=VK_TRUE;
    VkDeviceCreateInfo ci{};
    ci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount=1; ci.pQueueCreateInfos=&qInfo;
    ci.enabledExtensionCount=enabledDeviceExtensionsCount;
    ci.ppEnabledExtensionNames=enabledDeviceExtensionNames_array;
    ci.pEnabledFeatures=&df;
    r=vkCreateDevice(vkPhysicalDevice_sel,&ci,nullptr,&vkDevice);
    if(r!=VK_SUCCESS) return r;
    vkGetDeviceQueue(vkDevice,graphicsQueueIndex,0,&vkQueue);
    return VK_SUCCESS;
}
static VkResult getSurfaceFormatAndColorSpace() {
    uint32_t count=0; vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel,vkSurfaceKHR,&count,nullptr);
    if(count==0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkSurfaceFormatKHR> fmts(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel,vkSurfaceKHR,&count,fmts.data());
    if(count==1&&fmts[0].format==VK_FORMAT_UNDEFINED){
        vkFormat_color=VK_FORMAT_B8G8R8A8_UNORM; vkColorSpaceKHR=fmts[0].colorSpace;
    } else {
        vkFormat_color=fmts[0].format; vkColorSpaceKHR=fmts[0].colorSpace;
    }
    return VK_SUCCESS;
}
static VkResult getPresentMode() {
    uint32_t count=0; vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel,vkSurfaceKHR,&count,nullptr);
    if(count==0) return VK_ERROR_INITIALIZATION_FAILED;
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel,vkSurfaceKHR,&count,modes.data());
    vkPresentModeKHR=VK_PRESENT_MODE_FIFO_KHR;
    for(auto &m: modes){ if(m==VK_PRESENT_MODE_MAILBOX_KHR){ vkPresentModeKHR=VK_PRESENT_MODE_MAILBOX_KHR; break;} }
    return VK_SUCCESS;
}
VkResult CreateSwapChain(VkBool32 vsync) {
    (void)vsync;
    if(getSurfaceFormatAndColorSpace()!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    VkSurfaceCapabilitiesKHR caps{}; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_sel,vkSurfaceKHR,&caps);
    uint32_t desired=caps.minImageCount+1;
    if(caps.maxImageCount>0 && desired>caps.maxImageCount) desired=caps.maxImageCount;
    if(caps.currentExtent.width != UINT32_MAX) vkExtent2D_SwapChain=caps.currentExtent;
    else {
        vkExtent2D_SwapChain.width =  std::max(caps.minImageExtent.width,
                                      std::min(caps.maxImageExtent.width,(uint32_t)winWidth));
        vkExtent2D_SwapChain.height=  std::max(caps.minImageExtent.height,
                                      std::min(caps.maxImageExtent.height,(uint32_t)winHeight));
    }
    if(getPresentMode()!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    VkSwapchainCreateInfoKHR ci{};
    ci.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface=vkSurfaceKHR; ci.minImageCount=desired;
    ci.imageFormat=vkFormat_color; ci.imageColorSpace=vkColorSpaceKHR;
    ci.imageExtent=vkExtent2D_SwapChain; ci.imageArrayLayers=1;
    ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform=caps.currentTransform; ci.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode=vkPresentModeKHR; ci.clipped=VK_TRUE;
    return vkCreateSwapchainKHR(vkDevice,&ci,nullptr,&vkSwapchainKHR);
}
VkResult CreateImagesAndImageViews() {
    vkGetSwapchainImagesKHR(vkDevice,vkSwapchainKHR,&swapchainImageCount,nullptr);
    if(swapchainImageCount==0) return VK_ERROR_INITIALIZATION_FAILED;
    swapChainImage_array=(VkImage*)malloc(sizeof(VkImage)*swapchainImageCount);
    vkGetSwapchainImagesKHR(vkDevice,vkSwapchainKHR,&swapchainImageCount,swapChainImage_array);
    swapChainImageView_array=(VkImageView*)malloc(sizeof(VkImageView)*swapchainImageCount);
    for(uint32_t i=0;i<swapchainImageCount;i++){
        VkImageViewCreateInfo vi{};
        vi.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image=swapChainImage_array[i];
        vi.viewType=VK_IMAGE_VIEW_TYPE_2D;
        vi.format=vkFormat_color;
        vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1;
        if(vkCreateImageView(vkDevice,&vi,nullptr,&swapChainImageView_array[i])!=VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}
VkResult CreateCommandPool() {
    VkCommandPoolCreateInfo ci{};
    ci.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex=graphicsQueueIndex;
    ci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    return vkCreateCommandPool(vkDevice,&ci,nullptr,&vkCommandPool);
}
VkResult CreateCommandBuffers() {
    vkCommandBuffer_array=(VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*swapchainImageCount);
    VkCommandBufferAllocateInfo ai{};
    ai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool=vkCommandPool;
    ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount=swapchainImageCount;
    return vkAllocateCommandBuffers(vkDevice,&ai,vkCommandBuffer_array);
}
VkResult CreateSemaphores() {
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        if(vkCreateSemaphore(vkDevice,&si,nullptr,&imageAvailableSemaphores[i])!=VK_SUCCESS ||
           vkCreateSemaphore(vkDevice,&si,nullptr,&renderFinishedSemaphores[i])!=VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}
VkResult CreateFences() {
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;
    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        if(vkCreateFence(vkDevice,&fi,nullptr,&inFlightFences[i])!=VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}
static VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates,VkImageTiling tiling,VkFormatFeatureFlags feats){
    for(VkFormat f: candidates){
        VkFormatProperties p; vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice_sel,f,&p);
        if(tiling==VK_IMAGE_TILING_LINEAR && (p.linearTilingFeatures & feats)==feats) return f;
        if(tiling==VK_IMAGE_TILING_OPTIMAL&& (p.optimalTilingFeatures & feats)==feats) return f;
    }
    return VK_FORMAT_UNDEFINED;
}
static VkFormat findDepthFormat() {
    return findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
VkResult CreateDepthResources() {
    gDepthFormat=findDepthFormat();
    if(gDepthFormat==VK_FORMAT_UNDEFINED) return VK_ERROR_INITIALIZATION_FAILED;
    gDepthImages.resize(swapchainImageCount);
    gDepthImagesMemory.resize(swapchainImageCount);
    gDepthImagesView.resize(swapchainImageCount);
    for(uint32_t i=0;i<swapchainImageCount;i++){
        VkImageCreateInfo ii{};
        ii.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ii.imageType=VK_IMAGE_TYPE_2D;
        ii.extent.width=vkExtent2D_SwapChain.width;
        ii.extent.height=vkExtent2D_SwapChain.height;
        ii.extent.depth=1; ii.mipLevels=1; ii.arrayLayers=1;
        ii.format=gDepthFormat; ii.tiling=VK_IMAGE_TILING_OPTIMAL;
        ii.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        ii.samples=VK_SAMPLE_COUNT_1_BIT; ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        if(vkCreateImage(vkDevice,&ii,nullptr,&gDepthImages[i])!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
        VkMemoryRequirements mr; vkGetImageMemoryRequirements(vkDevice,gDepthImages[i],&mr);
        auto FindMemoryTypeIndex=[&](uint32_t typeFilter,VkMemoryPropertyFlags props)->uint32_t {
            for(uint32_t j=0;j<vkPhysicalDeviceMemoryProperties.memoryTypeCount;j++){
                if((typeFilter&(1<<j)) && 
                   (vkPhysicalDeviceMemoryProperties.memoryTypes[j].propertyFlags&props)==props) return j;
            }
            return UINT32_MAX;
        };
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize=mr.size; ai.memoryTypeIndex=FindMemoryTypeIndex(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if(ai.memoryTypeIndex==UINT32_MAX) return VK_ERROR_INITIALIZATION_FAILED;
        if(vkAllocateMemory(vkDevice,&ai,nullptr,&gDepthImagesMemory[i])!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
        vkBindImageMemory(vkDevice,gDepthImages[i],gDepthImagesMemory[i],0);
        VkImageViewCreateInfo vi{};
        vi.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image=gDepthImages[i]; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=gDepthFormat;
        vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT; vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1;
        if(vkCreateImageView(vkDevice,&vi,nullptr,&gDepthImagesView[i])!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}
VkResult CreateRenderPass() {
    VkAttachmentDescription colorA{};
    colorA.format=vkFormat_color; colorA.samples=VK_SAMPLE_COUNT_1_BIT;
    colorA.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; colorA.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    colorA.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    colorA.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorRef{};
    colorRef.attachment=0; colorRef.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentDescription depthA{};
    depthA.format=gDepthFormat; depthA.samples=VK_SAMPLE_COUNT_1_BIT;
    depthA.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; depthA.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthA.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    depthA.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthRef{};
    depthRef.attachment=1; depthRef.layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subp{};
    subp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    subp.colorAttachmentCount=1; subp.pColorAttachments=&colorRef;
    subp.pDepthStencilAttachment=&depthRef;
    VkSubpassDependency dep{};
    dep.srcSubpass=VK_SUBPASS_EXTERNAL; dep.dstSubpass=0;
    dep.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dep.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    std::array<VkAttachmentDescription,2> attachments={colorA,depthA};
    VkRenderPassCreateInfo rpci{};
    rpci.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount=(uint32_t)attachments.size();
    rpci.pAttachments=attachments.data();
    rpci.subpassCount=1; rpci.pSubpasses=&subp;
    rpci.dependencyCount=1; rpci.pDependencies=&dep;
    return vkCreateRenderPass(vkDevice,&rpci,nullptr,&gRenderPass);
}
VkResult CreateFramebuffers() {
    gFramebuffers=(VkFramebuffer*)malloc(sizeof(VkFramebuffer)*swapchainImageCount);
    for(uint32_t i=0;i<swapchainImageCount;i++){
        std::array<VkImageView,2> atts={ swapChainImageView_array[i], gDepthImagesView[i] };
        VkFramebufferCreateInfo fci{};
        fci.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass=gRenderPass;
        fci.attachmentCount=(uint32_t)atts.size();
        fci.pAttachments=atts.data();
        fci.width=vkExtent2D_SwapChain.width;
        fci.height=vkExtent2D_SwapChain.height;
        fci.layers=1;
        if(vkCreateFramebuffer(vkDevice,&fci,nullptr,&gFramebuffers[i])!=VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}
static void cleanupDepthResources(){
    for(uint32_t i=0;i<gDepthImages.size();i++){
        if(gDepthImagesView[i])   vkDestroyImageView(vkDevice,gDepthImagesView[i],nullptr);
        if(gDepthImages[i])       vkDestroyImage(vkDevice,gDepthImages[i],nullptr);
        if(gDepthImagesMemory[i]) vkFreeMemory(vkDevice,gDepthImagesMemory[i],nullptr);
    }
    gDepthImagesView.clear(); gDepthImages.clear(); gDepthImagesMemory.clear();
}
static void cleanupSwapChain(){
    vkDeviceWaitIdle(vkDevice);
    if(vkCommandBuffer_array){
        vkFreeCommandBuffers(vkDevice,vkCommandPool,swapchainImageCount,vkCommandBuffer_array);
        free(vkCommandBuffer_array); vkCommandBuffer_array=nullptr;
    }
    if(gFramebuffers){
        for(uint32_t i=0;i<swapchainImageCount;i++){
            if(gFramebuffers[i]) vkDestroyFramebuffer(vkDevice,gFramebuffers[i],nullptr);
        }
        free(gFramebuffers); gFramebuffers=nullptr;
    }
    if(vkCommandPool){
        vkDestroyCommandPool(vkDevice,vkCommandPool,nullptr);
        vkCommandPool=VK_NULL_HANDLE;
    }
    if(swapChainImageView_array){
        for(uint32_t i=0;i<swapchainImageCount;i++){
            if(swapChainImageView_array[i]) vkDestroyImageView(vkDevice,swapChainImageView_array[i],nullptr);
        }
        free(swapChainImageView_array); swapChainImageView_array=nullptr;
    }
    if(swapChainImage_array){
        free(swapChainImage_array); swapChainImage_array=nullptr;
    }
    if(vkSwapchainKHR){ vkDestroySwapchainKHR(vkDevice,vkSwapchainKHR,nullptr); vkSwapchainKHR=VK_NULL_HANDLE; }
    if(gRenderPass){ vkDestroyRenderPass(vkDevice,gRenderPass,nullptr); gRenderPass=VK_NULL_HANDLE; }
    cleanupDepthResources();
}
static uint32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    for(uint32_t i=0;i<vkPhysicalDeviceMemoryProperties.memoryTypeCount;i++){
        if((typeFilter&(1<<i)) && (vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & props)==props)
            return i;
    }
    return UINT32_MAX;
}
VkResult CreateCubeVertexBuffer() {
    VkDeviceSize sz=sizeof(Vertex)*gCubeVertices.size();
    VkBufferCreateInfo bi{};
    bi.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size=sz; bi.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(vkDevice,&bi,nullptr,&gVertexBufferCube)!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(vkDevice,gVertexBufferCube,&mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize=mr.size;
    ai.memoryTypeIndex=FindMemoryTypeIndex(mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(ai.memoryTypeIndex==UINT32_MAX) return VK_ERROR_INITIALIZATION_FAILED;
    if(vkAllocateMemory(vkDevice,&ai,nullptr,&gVertexBufferMemoryCube)!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    vkBindBufferMemory(vkDevice,gVertexBufferCube,gVertexBufferMemoryCube,0);
    void* data;
    vkMapMemory(vkDevice,gVertexBufferMemoryCube,0,sz,0,&data);
    memcpy(data,gCubeVertices.data(),(size_t)sz);
    vkUnmapMemory(vkDevice,gVertexBufferMemoryCube);
    return VK_SUCCESS;
}
VkResult CreatePyramidVertexBuffer() {
    VkDeviceSize sz=sizeof(Vertex)*gPyramidVertices.size();
    VkBufferCreateInfo bi{};
    bi.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size=sz; bi.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(vkDevice,&bi,nullptr,&gVertexBufferPyramid)!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(vkDevice,gVertexBufferPyramid,&mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize=mr.size;
    ai.memoryTypeIndex=FindMemoryTypeIndex(mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(ai.memoryTypeIndex==UINT32_MAX) return VK_ERROR_INITIALIZATION_FAILED;
    if(vkAllocateMemory(vkDevice,&ai,nullptr,&gVertexBufferMemoryPyramid)!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    vkBindBufferMemory(vkDevice,gVertexBufferPyramid,gVertexBufferMemoryPyramid,0);
    void* data;
    vkMapMemory(vkDevice,gVertexBufferMemoryPyramid,0,sz,0,&data);
    memcpy(data,gPyramidVertices.data(),(size_t)sz);
    vkUnmapMemory(vkDevice,gVertexBufferMemoryPyramid);
    return VK_SUCCESS;
}
VkResult CreateUniformBufferCube() {
    VkDeviceSize sz=sizeof(UniformBufferObject);
    VkBufferCreateInfo bi{};
    bi.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size=sz; bi.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(vkDevice,&bi,nullptr,&gUniformBufferCube)!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(vkDevice,gUniformBufferCube,&mr);
    VkMemoryAllocateInfo ai{};
    ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; ai.allocationSize=mr.size;
    ai.memoryTypeIndex=FindMemoryTypeIndex(mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(ai.memoryTypeIndex==UINT32_MAX) return VK_ERROR_INITIALIZATION_FAILED;
    if(vkAllocateMemory(vkDevice,&ai,nullptr,&gUniformBufferMemoryCube)!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    vkBindBufferMemory(vkDevice,gUniformBufferCube,gUniformBufferMemoryCube,0);
    return VK_SUCCESS;
}
VkResult CreateUniformBufferPyramid() {
    VkDeviceSize sz=sizeof(UniformBufferObject);
    VkBufferCreateInfo bi{};
    bi.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size=sz; bi.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(vkDevice,&bi,nullptr,&gUniformBufferPyramid)!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(vkDevice,gUniformBufferPyramid,&mr);
    VkMemoryAllocateInfo ai{};
    ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; ai.allocationSize=mr.size;
    ai.memoryTypeIndex=FindMemoryTypeIndex(mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(ai.memoryTypeIndex==UINT32_MAX) return VK_ERROR_INITIALIZATION_FAILED;
    if(vkAllocateMemory(vkDevice,&ai,nullptr,&gUniformBufferMemoryPyramid)!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    vkBindBufferMemory(vkDevice,gUniformBufferPyramid,gUniformBufferMemoryPyramid,0);
    return VK_SUCCESS;
}
VkResult CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding lb{};
    lb.binding=0; lb.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lb.descriptorCount=1; lb.stageFlags=VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount=1; ci.pBindings=&lb;
    return vkCreateDescriptorSetLayout(vkDevice,&ci,nullptr,&gDescriptorSetLayout);
}
VkResult CreateDescriptorPool() {
    VkDescriptorPoolSize ps{};
    ps.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ps.descriptorCount=2;
    VkDescriptorPoolCreateInfo ci{};
    ci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount=1; ci.pPoolSizes=&ps; ci.maxSets=2;
    return vkCreateDescriptorPool(vkDevice,&ci,nullptr,&gDescriptorPool);
}
VkResult CreateDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(2,gDescriptorSetLayout);
    VkDescriptorSetAllocateInfo ai{};
    ai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool=gDescriptorPool; ai.descriptorSetCount=2;
    ai.pSetLayouts=layouts.data();
    std::vector<VkDescriptorSet> sets(2);
    if(vkAllocateDescriptorSets(vkDevice,&ai,sets.data())!=VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    gDescriptorSetCube=sets[0]; gDescriptorSetPyramid=sets[1];
    {
        VkDescriptorBufferInfo bi{};
        bi.buffer=gUniformBufferCube; bi.offset=0; bi.range=sizeof(UniformBufferObject);
        VkWriteDescriptorSet wd{};
        wd.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wd.dstSet=gDescriptorSetCube; wd.dstBinding=0;
        wd.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wd.descriptorCount=1; wd.pBufferInfo=&bi;
        vkUpdateDescriptorSets(vkDevice,1,&wd,0,nullptr);
    }
    {
        VkDescriptorBufferInfo bi{};
        bi.buffer=gUniformBufferPyramid; bi.offset=0; bi.range=sizeof(UniformBufferObject);
        VkWriteDescriptorSet wd{};
        wd.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wd.dstSet=gDescriptorSetPyramid; wd.dstBinding=0;
        wd.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wd.descriptorCount=1; wd.pBufferInfo=&bi;
        vkUpdateDescriptorSets(vkDevice,1,&wd,0,nullptr);
    }
    return VK_SUCCESS;
}
static VkShaderModule CreateShaderModule(const std::vector<char> &code){
    if(code.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo ci{};
    ci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize=code.size();
    ci.pCode=reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule sm;
    if(vkCreateShaderModule(vkDevice,&ci,nullptr,&sm)!=VK_SUCCESS) return VK_NULL_HANDLE;
    return sm;
}
static VkVertexInputBindingDescription GetVertexBindingDescription(){
    VkVertexInputBindingDescription bd{};
    bd.binding=0; bd.stride=sizeof(Vertex); bd.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;
    return bd;
}
static std::array<VkVertexInputAttributeDescription,2> GetVertexAttributeDescriptions(){
    std::array<VkVertexInputAttributeDescription,2> a{};
    a[0].binding=0; a[0].location=0; a[0].format=VK_FORMAT_R32G32B32_SFLOAT; a[0].offset=offsetof(Vertex,pos);
    a[1].binding=0; a[1].location=1; a[1].format=VK_FORMAT_R32G32B32_SFLOAT; a[1].offset=offsetof(Vertex,color);
    return a;
}
VkResult CreateGraphicsPipeline(){
    auto vertCode=ReadFile("vert_shader.spv");
    auto fragCode=ReadFile("frag_shader.spv");
    VkShaderModule vertModule=CreateShaderModule(vertCode);
    VkShaderModule fragModule=CreateShaderModule(fragCode);
    if(!vertModule||!fragModule) return VK_ERROR_INITIALIZATION_FAILED;
    VkPipelineShaderStageCreateInfo vsci{};
    vsci.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vsci.stage=VK_SHADER_STAGE_VERTEX_BIT; vsci.module=vertModule; vsci.pName="main";
    VkPipelineShaderStageCreateInfo fsci{};
    fsci.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fsci.stage=VK_SHADER_STAGE_FRAGMENT_BIT; fsci.module=fragModule; fsci.pName="main";
    VkPipelineShaderStageCreateInfo stages[2]={ vsci, fsci };
    auto bd=GetVertexBindingDescription();
    auto ads=GetVertexAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vii{};
    vii.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vii.vertexBindingDescriptionCount=1; vii.pVertexBindingDescriptions=&bd;
    vii.vertexAttributeDescriptionCount=(uint32_t)ads.size(); vii.pVertexAttributeDescriptions=ads.data();
    VkPipelineInputAssemblyStateCreateInfo iasci{};
    iasci.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iasci.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport vp{};
    vp.x=0; vp.y=0; vp.width=(float)vkExtent2D_SwapChain.width; vp.height=(float)vkExtent2D_SwapChain.height;
    vp.minDepth=0; vp.maxDepth=1.0f;
    VkRect2D scissor{};
    scissor.offset={0,0}; scissor.extent=vkExtent2D_SwapChain;
    VkPipelineViewportStateCreateInfo vsci2{};
    vsci2.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vsci2.viewportCount=1; vsci2.pViewports=&vp;
    vsci2.scissorCount=1; vsci2.pScissors=&scissor;
    VkPipelineRasterizationStateCreateInfo rsci{};
    rsci.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsci.polygonMode=VK_POLYGON_MODE_LINE; rsci.cullMode=VK_CULL_MODE_NONE;
    rsci.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE; rsci.lineWidth=1.0f;
    VkPipelineMultisampleStateCreateInfo msci{};
    msci.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msci.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo dssci{};
    dssci.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dssci.depthTestEnable=VK_TRUE; dssci.depthWriteEnable=VK_TRUE; dssci.depthCompareOp=VK_COMPARE_OP_LESS;
    VkPipelineColorBlendAttachmentState cbatt{};
    cbatt.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|
                         VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cbci{};
    cbci.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbci.attachmentCount=1; cbci.pAttachments=&cbatt;
    VkPipelineLayoutCreateInfo plci{};
    plci.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount=1; plci.pSetLayouts=&gDescriptorSetLayout;
    if(vkCreatePipelineLayout(vkDevice,&plci,nullptr,&gPipelineLayout)!=VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;
    VkGraphicsPipelineCreateInfo gpci{};
    gpci.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount=2; gpci.pStages=stages;
    gpci.pVertexInputState=&vii; gpci.pInputAssemblyState=&iasci;
    gpci.pViewportState=&vsci2; gpci.pRasterizationState=&rsci;
    gpci.pMultisampleState=&msci; gpci.pDepthStencilState=&dssci;
    gpci.pColorBlendState=&cbci; gpci.layout=gPipelineLayout;
    gpci.renderPass=gRenderPass; gpci.subpass=0;
    VkResult ret=vkCreateGraphicsPipelines(vkDevice,VK_NULL_HANDLE,1,&gpci,nullptr,&gGraphicsPipeline);
    vkDestroyShaderModule(vkDevice,fragModule,nullptr);
    vkDestroyShaderModule(vkDevice,vertModule,nullptr);
    return ret;
}
VkResult buildCommandBuffers(){
    for(uint32_t i=0;i<swapchainImageCount;i++){
        vkResetCommandBuffer(vkCommandBuffer_array[i],0);
        VkCommandBufferBeginInfo bi{};
        bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(vkCommandBuffer_array[i],&bi);
        {
            VkImageMemoryBarrier b{};
            b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.image=swapChainImage_array[i];
            b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=1;
            b.srcAccessMask=0; b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(vkCommandBuffer_array[i],
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,0,nullptr,0,nullptr,1,&b);
        }
        {
            VkImageMemoryBarrier b{};
            b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.image=gDepthImages[i];
            b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
            b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=1;
            b.srcAccessMask=0; b.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(vkCommandBuffer_array[i],
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0,0,nullptr,0,nullptr,1,&b);
        }
        VkClearValue cv[2]; cv[0].color=vkClearColorValue;
        cv[1].depthStencil.depth=1.0f; cv[1].depthStencil.stencil=0;
        VkRenderPassBeginInfo rpbi{};
        rpbi.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass=gRenderPass; rpbi.framebuffer=gFramebuffers[i];
        rpbi.renderArea.offset={0,0}; rpbi.renderArea.extent=vkExtent2D_SwapChain;
        rpbi.clearValueCount=2; rpbi.pClearValues=cv;
        vkCmdBeginRenderPass(vkCommandBuffer_array[i],&rpbi,VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(vkCommandBuffer_array[i],VK_PIPELINE_BIND_POINT_GRAPHICS,gGraphicsPipeline);
        {
            VkBuffer vb[]={gVertexBufferCube};
            VkDeviceSize off[]={0};
            vkCmdBindVertexBuffers(vkCommandBuffer_array[i],0,1,vb,off);
            vkCmdBindDescriptorSets(vkCommandBuffer_array[i],
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                gPipelineLayout,0,1,&gDescriptorSetCube,0,nullptr);
            vkCmdDraw(vkCommandBuffer_array[i], (uint32_t)gCubeVertices.size(),1,0,0);
        }
        {
            VkBuffer vb[]={gVertexBufferPyramid};
            VkDeviceSize off[]={0};
            vkCmdBindVertexBuffers(vkCommandBuffer_array[i],0,1,vb,off);
            vkCmdBindDescriptorSets(vkCommandBuffer_array[i],
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                gPipelineLayout,0,1,&gDescriptorSetPyramid,0,nullptr);
            vkCmdDraw(vkCommandBuffer_array[i], (uint32_t)gPyramidVertices.size(),1,0,0);
        }
        vkCmdEndRenderPass(vkCommandBuffer_array[i]);
        {
            VkImageMemoryBarrier b{};
            b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            b.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.image=swapChainImage_array[i];
            b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.levelCount=1; b.subresourceRange.layerCount=1;
            b.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            b.dstAccessMask=0;
            vkCmdPipelineBarrier(vkCommandBuffer_array[i],
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,0,nullptr,0,nullptr,1,&b);
        }
        vkEndCommandBuffer(vkCommandBuffer_array[i]);
    }
    return VK_SUCCESS;
}
