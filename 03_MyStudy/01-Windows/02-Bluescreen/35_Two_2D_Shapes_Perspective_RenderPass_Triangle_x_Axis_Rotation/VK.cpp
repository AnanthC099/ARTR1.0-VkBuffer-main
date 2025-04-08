/*
    Vulkan_RenderPass_WithLayoutTransitions_RotateX.cpp

    A self-contained example that uses a standard Vulkan render pass +
    framebuffers, with explicit layout transitions for swapchain images.

    The LEFT TRIANGLE now rotates around the X-axis in 3D space, 
    while the RIGHT SQUARE remains stationary.

    KEY CHANGES for X-axis rotation:
      - Vertex has glm::vec3 for pos (instead of glm::vec2).
      - The pipeline uses VK_FORMAT_R32G32B32_SFLOAT for position.
      - We have a new function RotateTriangleCPU_XAxis() that:
          * Finds the centroid in 3D (cx, cy, cz).
          * Performs a rotation around the X-axis:
              y' = dy cos(θ) - dz sin(θ)
              z' = dy sin(θ) + dz cos(θ)
            where (dx,dy,dz) = originalPos - centroid.
            (x' remains dx for rotation around X)
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <fstream>
#include <vector>   // still used for enumerating layers, devices, etc.
#include <array>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#pragma comment(lib, "vulkan-1.lib")

// ---------------------------------------------------------------------------
//  Win32 Declarations
// ---------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

#define WIN_WIDTH  800
#define WIN_HEIGHT 600

// Globals for Win32
const char* gpszAppName = "Vulkan_RenderPass_LayoutTransitions_RotateX";

HWND  ghwnd          = NULL;
BOOL  gbActive       = FALSE;
DWORD dwStyle        = 0;
WINDOWPLACEMENT wpPrev{ sizeof(WINDOWPLACEMENT) };
BOOL  gbFullscreen   = FALSE;

FILE* gFILE          = nullptr;
bool  gEnableValidation = true;  // set true to enable validation layers

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

// Vulkan Forward Declarations
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

// Clear color
VkClearColorValue vkClearColorValue{};

// Window dimension
int  winWidth  = WIN_WIDTH;
int  winHeight = WIN_HEIGHT;
BOOL bInitialized = FALSE;

// Render pass + framebuffers
VkRenderPass   gRenderPass = VK_NULL_HANDLE;
VkFramebuffer* gFramebuffers = nullptr;

// ---------------------------------------------------------------------------
// Vertex & Uniform Data
// ---------------------------------------------------------------------------
struct Vertex {
    glm::vec3 pos;   // 3D position
    glm::vec3 color;
};

// We have 9 vertices total: first 3 for the left triangle, next 6 for the right square
static Vertex gVertices[9] =
{
    // === Left Triangle (indices 0..2) ===
    {{-0.75f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // Red
    {{-0.25f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},  // Green
    {{-0.50f,  0.50f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // Blue

    // === Right Square made of 2 triangles (indices 3..8) ===
    {{ 0.25f, -0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}},  // Yellow
    {{ 0.75f, -0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}},  // Yellow
    {{ 0.25f,  0.50f, 0.0f}, {1.0f, 1.0f, 0.0f}}, // Yellow

    {{ 0.75f, -0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}},  // ...
    {{ 0.75f,  0.50f, 0.0f}, {1.0f, 1.0f, 0.0f}},
    {{ 0.25f,  0.50f, 0.0f}, {1.0f, 1.0f, 0.0f}},
};

// Store the original triangle positions (x, y, z=0) for the left triangle
static glm::vec3 gTrianglePosOriginal[3] = {
    glm::vec3(-0.75f, -0.5f, 0.0f),
    glm::vec3(-0.25f, -0.5f, 0.0f),
    glm::vec3(-0.50f,  0.5f,  0.0f),
};

// We'll rotate around the X-axis, so let's keep a global angle
static float gTriangleAngleDegrees = 0.0f;

// Uniform data
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

// ---------------------------------------------------------------------------
// Forward for new rotating function
// ---------------------------------------------------------------------------
static void RotateTriangleCPU_XAxis(float angleDegrees);

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------
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
        TEXT("Vulkan X-Axis Rotation on Left Triangle"),
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
        fprintf(gFILE, "[ERROR] Cannot create window.\n");
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

// ---------------------------------------------------------------------------
// WndProc
// ---------------------------------------------------------------------------
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
        if (bInitialized == TRUE) {
            int width  = LOWORD(lParam);
            int height = HIWORD(lParam);

            winWidth  = width;
            winHeight = height;

            if (width > 0 && height > 0) {
                fprintf(gFILE, "[LOG] WM_SIZE -> RecreateSwapChain (%d x %d)\n", width, height);
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
    static MONITORINFO mi = { sizeof(MONITORINFO) };
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
// Vulkan Setup
// ============================================================================
static uint32_t enabledInstanceLayerCount = 0;
static const char* enabledInstanceLayerNames_array[1];

static VkResult FillInstanceLayerNames();
static VkResult FillInstanceExtensionNames();
static VkResult CreateVulkanInstance();
static VkResult CreateSurfaceWin32();
static VkResult SelectPhysicalDevice();
static VkResult FillDeviceExtensionNames();
static VkResult CreateLogicalDeviceAndQueue();
static VkResult getSurfaceFormatAndColorSpace();
static VkResult getPresentMode();
static VkResult CreateSwapChain(VkBool32 vsync);
static VkResult CreateImagesAndImageViews();
static VkResult CreateCommandPool();
static VkResult CreateCommandBuffers();
static VkResult CreateSemaphores();
static VkResult CreateFences();
static VkResult CreateRenderPass();
static VkResult CreateFramebuffers();
static VkResult CreateVertexBuffer();
static VkResult CreateUniformBuffer();
static VkResult CreateDescriptorSetLayout();
static VkResult CreateDescriptorPool();
static VkResult CreateDescriptorSet();
static VkResult CreateGraphicsPipeline();
static VkResult buildCommandBuffers();
static void     cleanupSwapChain();

// Recreate
VkResult recreateSwapChain()
{
    fprintf(gFILE, "[LOG] recreateSwapChain()...\n");
    if (winWidth == 0 || winHeight == 0) {
        fprintf(gFILE, "[LOG] Window minimized, skipping.\n");
        return VK_SUCCESS;
    }
    cleanupSwapChain();

    VkResult res;
    res = CreateSwapChain(VK_FALSE);       if (res) return res;
    res = CreateImagesAndImageViews();     if (res) return res;

    res = CreateRenderPass();              if (res) return res;
    res = CreateFramebuffers();            if (res) return res;

    res = CreateCommandPool();             if (res) return res;
    res = CreateCommandBuffers();          if (res) return res;

    // Recreate pipeline
    if (gGraphicsPipeline) {
        vkDestroyPipeline(vkDevice, gGraphicsPipeline, nullptr);
        gGraphicsPipeline = VK_NULL_HANDLE;
    }
    if (gPipelineLayout) {
        vkDestroyPipelineLayout(vkDevice, gPipelineLayout, nullptr);
        gPipelineLayout = VK_NULL_HANDLE;
    }
    res = CreateGraphicsPipeline();        if (res) return res;

    res = buildCommandBuffers();           if (res) return res;

    return VK_SUCCESS;
}

// initialize()
VkResult initialize(void)
{
    fprintf(gFILE, "[LOG] === initialize() ===\n");
    VkResult vkRes;
    vkRes = CreateVulkanInstance();          if (vkRes) return vkRes;
    vkRes = CreateSurfaceWin32();            if (vkRes) return vkRes;
    vkRes = SelectPhysicalDevice();          if (vkRes) return vkRes;
    vkRes = CreateLogicalDeviceAndQueue();   if (vkRes) return vkRes;

    vkRes = CreateSwapChain(VK_FALSE);       if (vkRes) return vkRes;
    vkRes = CreateImagesAndImageViews();     if (vkRes) return vkRes;

    vkRes = CreateRenderPass();              if (vkRes) return vkRes;
    vkRes = CreateFramebuffers();            if (vkRes) return vkRes;

    vkRes = CreateCommandPool();             if (vkRes) return vkRes;
    vkRes = CreateCommandBuffers();          if (vkRes) return vkRes;

    vkRes = CreateDescriptorSetLayout();     if (vkRes) return vkRes;
    vkRes = CreateUniformBuffer();           if (vkRes) return vkRes;
    vkRes = CreateDescriptorPool();          if (vkRes) return vkRes;
    vkRes = CreateDescriptorSet();           if (vkRes) return vkRes;

    vkRes = CreateVertexBuffer();            if (vkRes) return vkRes;
    vkRes = CreateGraphicsPipeline();        if (vkRes) return vkRes;

    vkRes = CreateSemaphores();              if (vkRes) return vkRes;
    vkRes = CreateFences();                  if (vkRes) return vkRes;

    // Clear color
    memset(&vkClearColorValue, 0, sizeof(vkClearColorValue));
    vkClearColorValue.float32[0] = 0.3f;
    vkClearColorValue.float32[1] = 0.3f;
    vkClearColorValue.float32[2] = 0.3f;
    vkClearColorValue.float32[3] = 1.0f;

    // Build initial command buffers
    vkRes = buildCommandBuffers();           if (vkRes) return vkRes;

    bInitialized = TRUE;
    fprintf(gFILE, "[LOG] === initialize() done ===\n");
    return VK_SUCCESS;
}

// display()
VkResult display(void)
{
    if (!bInitialized) return VK_SUCCESS;

    vkWaitForFences(vkDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(vkDevice, 1, &inFlightFences[currentFrame]);

    VkResult res = vkAcquireNextImageKHR(
        vkDevice,
        vkSwapchainKHR,
        UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE,
        &currentImageIndex
    );

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        fprintf(gFILE, "[LOG] acquire -> VK_ERROR_OUT_OF_DATE_KHR => recreate\n");
        recreateSwapChain();
        return VK_SUCCESS;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        fprintf(gFILE, "[ERROR] acquireNextImage failed: %d\n", res);
        return res;
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &imageAvailableSemaphores[currentFrame];
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &vkCommandBuffer_array[currentImageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &renderFinishedSemaphores[currentFrame];

    vkQueueSubmit(vkQueue, 1, &submitInfo, inFlightFences[currentFrame]);

    VkPresentInfoKHR presentInfo;
    memset(&presentInfo, 0, sizeof(presentInfo));
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinishedSemaphores[currentFrame];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &vkSwapchainKHR;
    presentInfo.pImageIndices      = &currentImageIndex;

    res = vkQueuePresentKHR(vkQueue, &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        fprintf(gFILE, "[LOG] present -> outOfDate/Suboptimal => recreate\n");
        recreateSwapChain();
    } else if (res != VK_SUCCESS) {
        fprintf(gFILE, "[ERROR] vkQueuePresentKHR failed: %d\n", res);
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

// update()
void update(void)
{
    // 1. Increase angle for continuous spin
    gTriangleAngleDegrees += 1.0f; // or any speed you like
    if (gTriangleAngleDegrees >= 360.0f) {
        gTriangleAngleDegrees -= 360.0f;
    }

    // 2. Rotate only the left triangle around the X-axis
    RotateTriangleCPU_XAxis(gTriangleAngleDegrees);

    // 3. Update uniform buffer (same MVP for entire scene)
    //    This does not affect the triangle's local spin, but handles the camera/projection.
    struct UniformBufferObject ubo;
    memset(&ubo, 0, sizeof(ubo));

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view  = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));

    float aspect = (float)winWidth / (float)winHeight;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // Y-flip for Vulkan
    proj[1][1] *= -1.0f;

    ubo.mvp = proj * view * model;

    // Map uniform buffer
    void* data = nullptr;
    vkMapMemory(vkDevice, gUniformBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(vkDevice, gUniformBufferMemory);
}

// uninitialize()
void uninitialize(void)
{
    fprintf(gFILE, "[LOG] uninitialize()...\n");
    if (gbFullscreen) {
        ToggleFullscreen();
        gbFullscreen = FALSE;
    }
    if (vkDevice) {
        vkDeviceWaitIdle(vkDevice);

        // Destroy sync
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

        // Cleanup swapchain
        cleanupSwapChain();

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
        fprintf(gFILE, "uninitialize() completed.\n");
        fclose(gFILE);
        gFILE = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Rotate the left triangle (indices [0..2]) around the X-axis by angleDegrees
// ---------------------------------------------------------------------------
static void RotateTriangleCPU_XAxis(float angleDegrees)
{
    float angleRad = glm::radians(angleDegrees);

    // 1. Compute centroid (cx, cy, cz)
    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    for(int i = 0; i < 3; i++) {
        cx += gTrianglePosOriginal[i].x;
        cy += gTrianglePosOriginal[i].y;
        cz += gTrianglePosOriginal[i].z;
    }
    cx /= 3.0f;
    cy /= 3.0f;
    cz /= 3.0f;

    // 2. For each of the 3 triangle vertices, compute the new (x,y,z)
    for(int i = 0; i < 3; i++) {
        float x0 = gTrianglePosOriginal[i].x;
        float y0 = gTrianglePosOriginal[i].y;
        float z0 = gTrianglePosOriginal[i].z;

        // Translate so that centroid is at origin
        float dx = x0 - cx;
        float dy = y0 - cy;
        float dz = z0 - cz;

        // Rotation around X-axis:
        //   x' = dx (unchanged)
        //   y' = dy cos(angle) - dz sin(angle)
        //   z' = dy sin(angle) + dz cos(angle)
        float rx = dx; // no change in X for rotation about X-axis
        float ry = (dy * cosf(angleRad)) - (dz * sinf(angleRad));
        float rz = (dy * sinf(angleRad)) + (dz * cosf(angleRad));

        // Translate back
        float x_new = cx + rx;
        float y_new = cy + ry;
        float z_new = cz + rz;

        // Overwrite the actual vertex in gVertices
        gVertices[i].pos.x = x_new;
        gVertices[i].pos.y = y_new;
        gVertices[i].pos.z = z_new;
    }

    // 3. Update the GPU vertex buffer with these new positions
    VkDeviceSize bufferSize = sizeof(gVertices);
    void* mappedPtr = nullptr;
    vkMapMemory(vkDevice, gVertexBufferMemory, 0, bufferSize, 0, &mappedPtr);
    memcpy(mappedPtr, gVertices, (size_t)bufferSize);
    vkUnmapMemory(vkDevice, gVertexBufferMemory);
}

// ---------------------------------------------------------------------------
// The rest are the Vulkan-setup functions (same as standard swapchain sample)
// ---------------------------------------------------------------------------
static VkResult FillInstanceLayerNames()
{
    if (!gEnableValidation)
        return VK_SUCCESS;

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if (layerCount == 0)
        return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    bool foundValidation = false;
    for (auto &lp : layers) {
        if (strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            foundValidation = true;
            break;
        }
    }

    if (foundValidation) {
        enabledInstanceLayerCount = 1;
        enabledInstanceLayerNames_array[0] = "VK_LAYER_KHRONOS_validation";
    } else {
        enabledInstanceLayerCount = 0;
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

    for (auto &ep : props) {
        if (strcmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
            enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            return;
        }
    }
}

static VkResult FillInstanceExtensionNames()
{
    // We need KHR surface + KHR win32
    enabledInstanceExtensionsCount = 0;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
    enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

    // Optionally debug
    AddDebugUtilsExtensionIfPresent();
    return VK_SUCCESS;
}

static VkResult CreateVulkanInstance()
{
    FillInstanceExtensionNames();
    FillInstanceLayerNames();

    VkApplicationInfo appInfo;
    memset(&appInfo, 0, sizeof(appInfo));
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = gpszAppName;
    appInfo.applicationVersion = 1;
    appInfo.pEngineName        = gpszAppName;
    appInfo.engineVersion      = 1;
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = enabledInstanceExtensionsCount;
    createInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;
    createInfo.enabledLayerCount       = enabledInstanceLayerCount;
    createInfo.ppEnabledLayerNames     = enabledInstanceLayerNames_array;

    VkResult res = vkCreateInstance(&createInfo, nullptr, &vkInstance);
    if (res != VK_SUCCESS) return res;

    // Debug messenger
    if (gEnableValidation && enabledInstanceLayerCount > 0) {
        VkDebugUtilsMessengerCreateInfoEXT dbgCreate;
        memset(&dbgCreate, 0, sizeof(dbgCreate));
        dbgCreate.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgCreate.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgCreate.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgCreate.pfnUserCallback = DebugCallback;

        CreateDebugUtilsMessengerEXT(vkInstance, &dbgCreate, nullptr, &gDebugUtilsMessenger);
    }
    return VK_SUCCESS;
}

static VkResult CreateSurfaceWin32()
{
    VkWin32SurfaceCreateInfoKHR createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    createInfo.hwnd      = ghwnd;

    return vkCreateWin32SurfaceKHR(vkInstance, &createInfo, nullptr, &vkSurfaceKHR);
}

static VkResult SelectPhysicalDevice()
{
    vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr);
    if (physicalDeviceCount == 0)
        return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkPhysicalDevice> pds(physicalDeviceCount);
    vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, pds.data());

    bool found = false;
    for (auto pd : pds) {
        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfp(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueCount, qfp.data());

        std::vector<VkBool32> canPresent(queueCount);
        for(uint32_t i=0; i<queueCount; i++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, vkSurfaceKHR, &canPresent[i]);
        }
        for(uint32_t i=0; i<queueCount; i++) {
            if((qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent[i]) {
                vkPhysicalDevice_sel = pd;
                graphicsQueueIndex   = i;
                found = true;
                break;
            }
        }
        if (found) break;
    }
    if (!found) return VK_ERROR_INITIALIZATION_FAILED;

    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_sel, &vkPhysicalDeviceMemoryProperties);
    return VK_SUCCESS;
}

static uint32_t enabledDeviceExtensionsCount = 0;
static const char* enabledDeviceExtensionNames_array[1];

static VkResult FillDeviceExtensionNames()
{
    // We only need VK_KHR_swapchain
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel, nullptr, &extCount, nullptr);
    if (extCount == 0)
        return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_sel, nullptr, &extCount, exts.data());

    bool foundSwapchain = false;
    for(auto &e : exts) {
        if(strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)==0)
            foundSwapchain = true;
    }
    if (!foundSwapchain) return VK_ERROR_INITIALIZATION_FAILED;

    enabledDeviceExtensionsCount = 1;
    enabledDeviceExtensionNames_array[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    return VK_SUCCESS;
}

static VkResult CreateLogicalDeviceAndQueue()
{
    FillDeviceExtensionNames();

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo qinfo;
    memset(&qinfo, 0, sizeof(qinfo));
    qinfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qinfo.queueFamilyIndex = graphicsQueueIndex;
    qinfo.queueCount       = 1;
    qinfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo dci;
    memset(&dci, 0, sizeof(dci));
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qinfo;
    dci.enabledExtensionCount   = enabledDeviceExtensionsCount;
    dci.ppEnabledExtensionNames = enabledDeviceExtensionNames_array;

    VkResult res = vkCreateDevice(vkPhysicalDevice_sel, &dci, nullptr, &vkDevice);
    if(res) return res;

    vkGetDeviceQueue(vkDevice, graphicsQueueIndex, 0, &vkQueue);
    return VK_SUCCESS;
}

static VkResult getSurfaceFormatAndColorSpace()
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &count, nullptr);
    if(count == 0) return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &count, formats.data());

    if (count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        vkFormat_color = VK_FORMAT_B8G8R8A8_UNORM;
        vkColorSpaceKHR= formats[0].colorSpace;
    } else {
        vkFormat_color = formats[0].format;
        vkColorSpaceKHR= formats[0].colorSpace;
    }
    return VK_SUCCESS;
}

static VkResult getPresentMode()
{
    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &pmCount, nullptr);
    if(!pmCount) return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkPresentModeKHR> pms(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &pmCount, pms.data());

    vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR; // fallback
    for(auto pm : pms) {
        if(pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    return VK_SUCCESS;
}

static VkResult CreateSwapChain(VkBool32 vsync)
{
    getSurfaceFormatAndColorSpace();

    VkSurfaceCapabilitiesKHR caps;
    memset(&caps, 0, sizeof(caps));
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_sel, vkSurfaceKHR, &caps);

    uint32_t imgCount = caps.minImageCount + 1;
    if ((caps.maxImageCount>0) && (imgCount>caps.maxImageCount))
        imgCount = caps.maxImageCount;

    if (caps.currentExtent.width != UINT32_MAX) {
        vkExtent2D_SwapChain = caps.currentExtent;
    } else {
        vkExtent2D_SwapChain.width  = (uint32_t)winWidth;
        vkExtent2D_SwapChain.height = (uint32_t)winHeight;
        if(vkExtent2D_SwapChain.width < caps.minImageExtent.width)
            vkExtent2D_SwapChain.width = caps.minImageExtent.width;
        else if(vkExtent2D_SwapChain.width > caps.maxImageExtent.width)
            vkExtent2D_SwapChain.width = caps.maxImageExtent.width;
        if(vkExtent2D_SwapChain.height < caps.minImageExtent.height)
            vkExtent2D_SwapChain.height = caps.minImageExtent.height;
        else if(vkExtent2D_SwapChain.height> caps.maxImageExtent.height)
            vkExtent2D_SwapChain.height = caps.maxImageExtent.height;
    }

    getPresentMode();

    VkSwapchainCreateInfoKHR sci;
    memset(&sci, 0, sizeof(sci));
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = vkSurfaceKHR;
    sci.minImageCount    = imgCount;
    sci.imageFormat      = vkFormat_color;
    sci.imageColorSpace  = vkColorSpaceKHR;
    sci.imageExtent      = vkExtent2D_SwapChain;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform     = caps.currentTransform;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = vkPresentModeKHR;
    sci.clipped          = VK_TRUE;
    sci.oldSwapchain     = VK_NULL_HANDLE;

    return vkCreateSwapchainKHR(vkDevice, &sci, nullptr, &vkSwapchainKHR);
}

static VkResult CreateImagesAndImageViews()
{
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, nullptr);
    if(!swapchainImageCount) return VK_ERROR_INITIALIZATION_FAILED;

    swapChainImage_array = (VkImage*)malloc(sizeof(VkImage)*swapchainImageCount);
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapChainImage_array);

    swapChainImageView_array = (VkImageView*)malloc(sizeof(VkImageView)*swapchainImageCount);

    for(uint32_t i=0; i<swapchainImageCount; i++){
        VkImageViewCreateInfo ivci;
        memset(&ivci, 0, sizeof(ivci));
        ivci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image                           = swapChainImage_array[i];
        ivci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format                          = vkFormat_color;
        ivci.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel   = 0;
        ivci.subresourceRange.levelCount     = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(vkDevice, &ivci, nullptr, &swapChainImageView_array[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

static VkResult CreateCommandPool()
{
    VkCommandPoolCreateInfo cpci;
    memset(&cpci, 0, sizeof(cpci));
    cpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = graphicsQueueIndex;
    cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    return vkCreateCommandPool(vkDevice, &cpci, nullptr, &vkCommandPool);
}

static VkResult CreateCommandBuffers()
{
    vkCommandBuffer_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*swapchainImageCount);

    VkCommandBufferAllocateInfo cbai;
    memset(&cbai, 0, sizeof(cbai));
    cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool        = vkCommandPool;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = swapchainImageCount;

    return vkAllocateCommandBuffers(vkDevice, &cbai, vkCommandBuffer_array);
}

static VkResult CreateSemaphores()
{
    VkSemaphoreCreateInfo sci;
    memset(&sci, 0, sizeof(sci));
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        if (vkCreateSemaphore(vkDevice, &sci, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vkDevice, &sci, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}

static VkResult CreateFences()
{
    VkFenceCreateInfo fci;
    memset(&fci, 0, sizeof(fci));
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        if(vkCreateFence(vkDevice, &fci, nullptr, &inFlightFences[i])!=VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

static VkResult CreateRenderPass()
{
    VkAttachmentDescription colorAtt;
    memset(&colorAtt, 0, sizeof(colorAtt));
    colorAtt.format         = vkFormat_color;
    colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef;
    memset(&colorRef, 0, sizeof(colorRef));
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass;
    memset(&subpass, 0, sizeof(subpass));
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dependency;
    memset(&dependency, 0, sizeof(dependency));
    dependency.srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass      = 0;
    dependency.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci;
    memset(&rpci, 0, sizeof(rpci));
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &colorAtt;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dependency;

    return vkCreateRenderPass(vkDevice, &rpci, nullptr, &gRenderPass);
}

static VkResult CreateFramebuffers()
{
    gFramebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*swapchainImageCount);
    for(uint32_t i=0; i<swapchainImageCount; i++){
        VkImageView iv[1] = { swapChainImageView_array[i] };

        VkFramebufferCreateInfo fbci;
        memset(&fbci, 0, sizeof(fbci));
        fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass      = gRenderPass;
        fbci.attachmentCount = 1;
        fbci.pAttachments    = iv;
        fbci.width           = vkExtent2D_SwapChain.width;
        fbci.height          = vkExtent2D_SwapChain.height;
        fbci.layers          = 1;

        if (vkCreateFramebuffer(vkDevice, &fbci, nullptr, &gFramebuffers[i]) != VK_SUCCESS)
            return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

static void cleanupSwapChain()
{
    vkDeviceWaitIdle(vkDevice);

    if (vkCommandBuffer_array) {
        vkFreeCommandBuffers(vkDevice, vkCommandPool, swapchainImageCount, vkCommandBuffer_array);
        free(vkCommandBuffer_array);
        vkCommandBuffer_array = nullptr;
    }
    if (gFramebuffers) {
        for(uint32_t i=0; i<swapchainImageCount; i++){
            if(gFramebuffers[i]) vkDestroyFramebuffer(vkDevice, gFramebuffers[i], nullptr);
        }
        free(gFramebuffers);
        gFramebuffers=nullptr;
    }
    if (vkCommandPool) {
        vkDestroyCommandPool(vkDevice, vkCommandPool, nullptr);
        vkCommandPool = VK_NULL_HANDLE;
    }
    if (swapChainImageView_array) {
        for(uint32_t i=0; i<swapchainImageCount; i++){
            if(swapChainImageView_array[i]) vkDestroyImageView(vkDevice, swapChainImageView_array[i], nullptr);
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
}

static uint32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags props)
{
    for(uint32_t i=0; i<vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++){
        if((typeFilter & (1<<i)) && ((vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & props)==props)){
            return i;
        }
    }
    return UINT32_MAX;
}

static VkResult CreateVertexBuffer()
{
    VkDeviceSize bufSize = sizeof(gVertices);

    VkBufferCreateInfo bci;
    memset(&bci, 0, sizeof(bci));
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = bufSize;
    bci.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateBuffer(vkDevice, &bci, nullptr, &gVertexBuffer)!=VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gVertexBuffer, &memReq);

    VkMemoryAllocateInfo mai;
    memset(&mai, 0, sizeof(mai));
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReq.size;
    mai.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(mai.memoryTypeIndex==UINT32_MAX)
        return VK_ERROR_INITIALIZATION_FAILED;

    if(vkAllocateMemory(vkDevice, &mai, nullptr, &gVertexBufferMemory)!=VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    vkBindBufferMemory(vkDevice, gVertexBuffer, gVertexBufferMemory, 0);

    // Initial copy
    void* data=nullptr;
    vkMapMemory(vkDevice, gVertexBufferMemory, 0, bufSize, 0, &data);
    memcpy(data, gVertices, (size_t)bufSize);
    vkUnmapMemory(vkDevice, gVertexBufferMemory);

    return VK_SUCCESS;
}

static VkResult CreateUniformBuffer()
{
    VkDeviceSize bsize = sizeof(UniformBufferObject);

    VkBufferCreateInfo bci;
    memset(&bci, 0, sizeof(bci));
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = bsize;
    bci.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateBuffer(vkDevice, &bci, nullptr, &gUniformBuffer)!=VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vkDevice, gUniformBuffer, &memReq);

    VkMemoryAllocateInfo mai;
    memset(&mai, 0, sizeof(mai));
    mai.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex= FindMemoryTypeIndex(memReq.memoryTypeBits,
       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(mai.memoryTypeIndex==UINT32_MAX)
        return VK_ERROR_INITIALIZATION_FAILED;

    if(vkAllocateMemory(vkDevice, &mai, nullptr, &gUniformBufferMemory)!=VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    vkBindBufferMemory(vkDevice, gUniformBuffer, gUniformBufferMemory, 0);
    return VK_SUCCESS;
}

static VkResult CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboLayout;
    memset(&uboLayout, 0, sizeof(uboLayout));
    uboLayout.binding            = 0;
    uboLayout.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayout.descriptorCount    = 1;
    uboLayout.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo dslci;
    memset(&dslci, 0, sizeof(dslci));
    dslci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 1;
    dslci.pBindings    = &uboLayout;

    return vkCreateDescriptorSetLayout(vkDevice, &dslci, nullptr, &gDescriptorSetLayout);
}

static VkResult CreateDescriptorPool()
{
    VkDescriptorPoolSize poolSize;
    memset(&poolSize, 0, sizeof(poolSize));
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo dpci;
    memset(&dpci, 0, sizeof(dpci));
    dpci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes    = &poolSize;
    dpci.maxSets       = 1;

    return vkCreateDescriptorPool(vkDevice, &dpci, nullptr, &gDescriptorPool);
}

static VkResult CreateDescriptorSet()
{
    VkDescriptorSetAllocateInfo dsai;
    memset(&dsai, 0, sizeof(dsai));
    dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool     = gDescriptorPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts        = &gDescriptorSetLayout;

    if(vkAllocateDescriptorSets(vkDevice, &dsai, &gDescriptorSet)!=VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkDescriptorBufferInfo dbi;
    memset(&dbi, 0, sizeof(dbi));
    dbi.buffer = gUniformBuffer;
    dbi.offset = 0;
    dbi.range  = sizeof(UniformBufferObject);

    VkWriteDescriptorSet wds;
    memset(&wds, 0, sizeof(wds));
    wds.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wds.dstSet          = gDescriptorSet;
    wds.dstBinding      = 0;
    wds.dstArrayElement = 0;
    wds.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    wds.descriptorCount = 1;
    wds.pBufferInfo     = &dbi;

    vkUpdateDescriptorSets(vkDevice, 1, &wds, 0, nullptr);
    return VK_SUCCESS;
}

// SPIR-V loader
static std::vector<char> ReadFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if(!file.is_open()) return {};

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

static VkShaderModule CreateShaderModule(const std::vector<char> &code)
{
    if(code.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo smci;
    memset(&smci, 0, sizeof(smci));
    smci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = code.size();
    smci.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule sm = VK_NULL_HANDLE;
    vkCreateShaderModule(vkDevice, &smci, nullptr, &sm);
    return sm;
}

// Pipeline
static VkVertexInputBindingDescription GetVertexBindingDesc()
{
    VkVertexInputBindingDescription bd;
    memset(&bd, 0, sizeof(bd));
    bd.binding   = 0;
    bd.stride    = sizeof(Vertex);
    bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bd;
}
static void GetVertexAttrDescs(VkVertexInputAttributeDescription outAttrs[2])
{
    // Position => location 0 => R32G32B32 SFLOAT
    outAttrs[0].binding  = 0;
    outAttrs[0].location = 0;
    outAttrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    outAttrs[0].offset   = offsetof(Vertex, pos);

    // Color => location 1 => R32G32B32 SFLOAT
    outAttrs[1].binding  = 0;
    outAttrs[1].location = 1;
    outAttrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    outAttrs[1].offset   = offsetof(Vertex, color);
}

static VkResult CreateGraphicsPipeline()
{
    // Load SPIR-V
    auto vertSPV = ReadFile("vert_shader.spv");
    auto fragSPV = ReadFile("frag_shader.spv");
    if(vertSPV.empty() || fragSPV.empty()) return VK_ERROR_INITIALIZATION_FAILED;

    VkShaderModule vertMod = CreateShaderModule(vertSPV);
    VkShaderModule fragMod = CreateShaderModule(fragSPV);
    if(vertMod==VK_NULL_HANDLE || fragMod==VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkPipelineShaderStageCreateInfo vinfo;
    memset(&vinfo, 0, sizeof(vinfo));
    vinfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vinfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vinfo.module = vertMod;
    vinfo.pName  = "main";

    VkPipelineShaderStageCreateInfo finfo;
    memset(&finfo, 0, sizeof(finfo));
    finfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    finfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    finfo.module = fragMod;
    finfo.pName  = "main";

    VkPipelineShaderStageCreateInfo stages[2] = { vinfo, finfo };

    VkVertexInputBindingDescription bindDesc = GetVertexBindingDesc();
    VkVertexInputAttributeDescription attrDesc[2];
    GetVertexAttrDescs(attrDesc);

    VkPipelineVertexInputStateCreateInfo vis;
    memset(&vis, 0, sizeof(vis));
    vis.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vis.vertexBindingDescriptionCount   = 1;
    vis.pVertexBindingDescriptions      = &bindDesc;
    vis.vertexAttributeDescriptionCount = 2;
    vis.pVertexAttributeDescriptions    = attrDesc;

    VkPipelineInputAssemblyStateCreateInfo ias;
    memset(&ias, 0, sizeof(ias));
    ias.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp;
    memset(&vp, 0, sizeof(vp));
    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)vkExtent2D_SwapChain.width;
    vp.height   = (float)vkExtent2D_SwapChain.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    VkRect2D scissor;
    memset(&scissor, 0, sizeof(scissor));
    scissor.offset = {0,0};
    scissor.extent = vkExtent2D_SwapChain;

    VkPipelineViewportStateCreateInfo vs;
    memset(&vs, 0, sizeof(vs));
    vs.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1;
    vs.pViewports    = &vp;
    vs.scissorCount  = 1;
    vs.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rs;
    memset(&rs, 0, sizeof(rs));
    rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode             = VK_POLYGON_MODE_FILL;
    rs.cullMode                = VK_CULL_MODE_NONE;
    rs.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth               = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms;
    memset(&ms, 0, sizeof(ms));
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cbas;
    memset(&cbas, 0, sizeof(cbas));
    cbas.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb;
    memset(&cb, 0, sizeof(cb));
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbas;

    VkPipelineLayoutCreateInfo plci;
    memset(&plci, 0, sizeof(plci));
    plci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts    = &gDescriptorSetLayout;

    if(vkCreatePipelineLayout(vkDevice, &plci, nullptr, &gPipelineLayout)!=VK_SUCCESS)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkGraphicsPipelineCreateInfo gpci;
    memset(&gpci, 0, sizeof(gpci));
    gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount          = 2;
    gpci.pStages             = stages;
    gpci.pVertexInputState   = &vis;
    gpci.pInputAssemblyState = &ias;
    gpci.pViewportState      = &vs;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState   = &ms;
    gpci.pColorBlendState    = &cb;
    gpci.layout              = gPipelineLayout;
    gpci.renderPass          = gRenderPass;
    gpci.subpass             = 0;

    VkResult res = vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &gpci, nullptr, &gGraphicsPipeline);

    vkDestroyShaderModule(vkDevice, fragMod, nullptr);
    vkDestroyShaderModule(vkDevice, vertMod, nullptr);
    return res;
}

static VkResult buildCommandBuffers()
{
    for(uint32_t i=0; i<swapchainImageCount; i++){
        vkResetCommandBuffer(vkCommandBuffer_array[i], 0);

        VkCommandBufferBeginInfo bi;
        memset(&bi, 0, sizeof(bi));
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(vkCommandBuffer_array[i], &bi);

        // Transition -> COLOR_ATTACHMENT_OPTIMAL
        {
            VkImageMemoryBarrier imb;
            memset(&imb, 0, sizeof(imb));
            imb.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imb.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
            imb.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imb.image = swapChainImage_array[i];
            imb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imb.subresourceRange.baseMipLevel   = 0;
            imb.subresourceRange.levelCount     = 1;
            imb.subresourceRange.baseArrayLayer = 0;
            imb.subresourceRange.layerCount     = 1;
            imb.srcAccessMask = 0;
            imb.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(
                vkCommandBuffer_array[i],
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0, 0, nullptr, 0, nullptr, 1, &imb
            );
        }

        // Begin RenderPass
        VkClearValue clearVal;
        memset(&clearVal, 0, sizeof(clearVal));
        clearVal.color = vkClearColorValue;

        VkRenderPassBeginInfo rpbi;
        memset(&rpbi, 0, sizeof(rpbi));
        rpbi.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass      = gRenderPass;
        rpbi.framebuffer     = gFramebuffers[i];
        rpbi.renderArea.offset={0,0};
        rpbi.renderArea.extent= vkExtent2D_SwapChain;
        rpbi.clearValueCount = 1;
        rpbi.pClearValues    = &clearVal;

        vkCmdBeginRenderPass(vkCommandBuffer_array[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(vkCommandBuffer_array[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gGraphicsPipeline);

        // Bind descriptor
        vkCmdBindDescriptorSets(
            vkCommandBuffer_array[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            gPipelineLayout,
            0,1,
            &gDescriptorSet,
            0, nullptr
        );

        // Bind vertex buffer
        VkBuffer vb[1] = { gVertexBuffer };
        VkDeviceSize offs[1] = { 0 };
        vkCmdBindVertexBuffers(vkCommandBuffer_array[i], 0, 1, vb, offs);

        // Draw the left triangle (indices 0..2 => 3 vertices)
        vkCmdDraw(vkCommandBuffer_array[i], 3, 1, 0, 0);

        // Draw the right square (indices 3..8 => 6 vertices)
        vkCmdDraw(vkCommandBuffer_array[i], 6, 1, 3, 0);

        vkCmdEndRenderPass(vkCommandBuffer_array[i]);

        // Transition -> PRESENT_SRC_KHR
        {
            VkImageMemoryBarrier imb2;
            memset(&imb2, 0, sizeof(imb2));
            imb2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imb2.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imb2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imb2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imb2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imb2.image = swapChainImage_array[i];
            imb2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imb2.subresourceRange.baseMipLevel = 0;
            imb2.subresourceRange.levelCount   = 1;
            imb2.subresourceRange.baseArrayLayer=0;
            imb2.subresourceRange.layerCount   = 1;
            imb2.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            imb2.dstAccessMask = 0;

            vkCmdPipelineBarrier(
                vkCommandBuffer_array[i],
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,0,nullptr,0,nullptr,1,&imb2
            );
        }

        vkEndCommandBuffer(vkCommandBuffer_array[i]);
    }
    return VK_SUCCESS;
}
