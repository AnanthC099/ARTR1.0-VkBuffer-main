#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <fstream>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

#define WIN_WIDTH  800
#define WIN_HEIGHT 600

static const char* gAppName = "Vulkan_Ortho_Square";
HWND   ghwnd = NULL;
BOOL   gbActive = FALSE;
BOOL   gbFullscreen = FALSE;
DWORD  dwStyle = 0;
WINDOWPLACEMENT wpPrev{ sizeof(WINDOWPLACEMENT) };

VkInstance             gInstance = VK_NULL_HANDLE;
VkDebugUtilsMessengerEXT gDebugMessenger = VK_NULL_HANDLE;
VkPhysicalDevice       gPhysicalDevice = VK_NULL_HANDLE;
VkDevice               gDevice = VK_NULL_HANDLE;
VkQueue                gQueue = VK_NULL_HANDLE;
VkSurfaceKHR           gSurface = VK_NULL_HANDLE;
VkSwapchainKHR         gSwapchain = VK_NULL_HANDLE;
VkRenderPass           gRenderPass = VK_NULL_HANDLE;
VkCommandPool          gCommandPool = VK_NULL_HANDLE;
VkExtent2D             gSwapExtent{ WIN_WIDTH, WIN_HEIGHT };
VkFormat               gColorFormat = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR        gColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR       gPresentMode = VK_PRESENT_MODE_FIFO_KHR;

uint32_t gGraphicsQueueIndex = UINT32_MAX;
uint32_t gSwapImageCount = 0;
VkImage* gSwapImages = nullptr;
VkImageView* gSwapImageViews = nullptr;
VkFramebuffer* gFramebuffers = nullptr;
VkCommandBuffer* gCommandBuffers = nullptr;

#define MAX_FRAMES_IN_FLIGHT 2
VkSemaphore gImageAvailableSem[MAX_FRAMES_IN_FLIGHT];
VkSemaphore gRenderFinishedSem[MAX_FRAMES_IN_FLIGHT];
VkFence     gInFlightFences[MAX_FRAMES_IN_FLIGHT];
uint32_t    gCurrentFrame = 0;
uint32_t    gCurrentImageIndex = 0;

bool gEnableValidation = false;
VkPhysicalDeviceMemoryProperties gMemProps{};

int  gWidth  = WIN_WIDTH;
int  gHeight = WIN_HEIGHT;
bool gInitialized = false;

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

// Two triangles forming a square:
static std::vector<Vertex> gSquareVertices = {
    // First triangle
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{-0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},

    // Second triangle
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{ 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}}
};

struct UBO {
    glm::mat4 mvp;
};

VkBuffer       gVertexBuffer = VK_NULL_HANDLE;
VkDeviceMemory gVertexBufferMemory = VK_NULL_HANDLE;
VkBuffer       gUniformBuffer = VK_NULL_HANDLE;
VkDeviceMemory gUniformBufferMemory = VK_NULL_HANDLE;

VkDescriptorSetLayout gDescSetLayout = VK_NULL_HANDLE;
VkDescriptorPool      gDescPool     = VK_NULL_HANDLE;
VkDescriptorSet       gDescSet      = VK_NULL_HANDLE;
VkPipelineLayout      gPipelineLayout = VK_NULL_HANDLE;
VkPipeline            gGraphicsPipeline = VK_NULL_HANDLE;

VkClearColorValue gClearColor{ {0.5f, 0.5f, 0.5f, 1.0f} };

FILE* gLog = nullptr;  // Log file pointer

// -------------------------------------------------------------------------------------------
// Debug Callback
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData)
{
    // Print to stderr
    fprintf(stderr, "VALIDATION: %s\n", pCallbackData->pMessage);

    // Print to file log
    if (gLog)
    {
        fprintf(gLog, "VALIDATION: %s\n", pCallbackData->pMessage);
        fflush(gLog);
    }
    return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func)
        return func(instance, pCreateInfo, pAllocator, pMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func)
        func(instance, messenger, pAllocator);
}

// -------------------------------------------------------------------------------------------
// Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        wpPrev.length = sizeof(WINDOWPLACEMENT);
        break;
    case WM_SETFOCUS:
        gbActive = TRUE;
        break;
    case WM_KILLFOCUS:
        gbActive = FALSE;
        break;
    case WM_SIZE:
        if (gInitialized) {
            gWidth = LOWORD(lParam);
            gHeight = HIWORD(lParam);
            if (gWidth > 0 && gHeight > 0) {
                vkDeviceWaitIdle(gDevice);
                extern VkResult recreateSwapChain();

                // [LOG]
                fprintf(gLog, "[WM_SIZE] Recreating swap chain with new size: %d x %d\n", gWidth, gHeight);
                fflush(gLog);

                recreateSwapChain();
            }
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        break;
    case WM_CHAR:
        if (wParam == 'f' || wParam == 'F') {
            MONITORINFO mi{ sizeof(MONITORINFO) };
            if (!gbFullscreen) {
                dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
                if (dwStyle & WS_OVERLAPPEDWINDOW) {
                    if (GetWindowPlacement(ghwnd, &wpPrev) &&
                        GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi)) {
                        SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
                        SetWindowPos(
                            ghwnd, HWND_TOP,
                            mi.rcMonitor.left,
                            mi.rcMonitor.top,
                            mi.rcMonitor.right - mi.rcMonitor.left,
                            mi.rcMonitor.bottom - mi.rcMonitor.top,
                            SWP_NOZORDER | SWP_FRAMECHANGED);
                        ShowCursor(FALSE);
                        gbFullscreen = TRUE;

                        // [LOG]
                        fprintf(gLog, "[WM_CHAR] Entered fullscreen mode.\n");
                        fflush(gLog);
                    }
                }
            } else {
                SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
                SetWindowPlacement(ghwnd, &wpPrev);
                SetWindowPos(
                    ghwnd, HWND_TOP, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                ShowCursor(TRUE);
                gbFullscreen = FALSE;

                // [LOG]
                fprintf(gLog, "[WM_CHAR] Exited fullscreen mode.\n");
                fflush(gLog);
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
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// -------------------------------------------------------------------------------------------
// Helper: read SPIR-V
static std::vector<char> ReadFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        fprintf(gLog, "[ReadFile] Failed to open file: %s\n", filename.c_str());
        fflush(gLog);
        return {};
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    fprintf(gLog, "[ReadFile] Successfully read file: %s (%zu bytes)\n",
            filename.c_str(), fileSize);
    fflush(gLog);

    return buffer;
}

// Create shader module
static VkShaderModule CreateShaderModule(const std::vector<char>& code)
{
    if (code.empty()) return VK_NULL_HANDLE;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod;
    if (vkCreateShaderModule(gDevice, &createInfo, nullptr, &mod) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateShaderModule] Failed to create shader module.\n");
        fflush(gLog);
        return VK_NULL_HANDLE;
    }

    fprintf(gLog, "[CreateShaderModule] Shader module created successfully.\n");
    fflush(gLog);
    return mod;
}

// -------------------------------------------------------------------------------------------
// Memory helper
static uint32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    for (uint32_t i = 0; i < gMemProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (gMemProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

// -------------------------------------------------------------------------------------------
// Create Vertex + Uniform buffers
static VkResult CreateVertexBuffer()
{
    fprintf(gLog, "[CreateVertexBuffer] Creating vertex buffer...\n");
    fflush(gLog);

    VkDeviceSize sizeBytes = sizeof(Vertex) * gSquareVertices.size();

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = sizeBytes;
    bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    if (vkCreateBuffer(gDevice, &bufInfo, nullptr, &gVertexBuffer) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateVertexBuffer] vkCreateBuffer failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(gDevice, gVertexBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        fprintf(gLog, "[CreateVertexBuffer] Could not find suitable memory type.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (vkAllocateMemory(gDevice, &allocInfo, nullptr, &gVertexBufferMemory) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateVertexBuffer] vkAllocateMemory failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkBindBufferMemory(gDevice, gVertexBuffer, gVertexBufferMemory, 0);

    void* data;
    vkMapMemory(gDevice, gVertexBufferMemory, 0, sizeBytes, 0, &data);
    memcpy(data, gSquareVertices.data(), (size_t)sizeBytes);
    vkUnmapMemory(gDevice, gVertexBufferMemory);

    fprintf(gLog, "[CreateVertexBuffer] Vertex buffer created and data copied.\n");
    fflush(gLog);
    return VK_SUCCESS;
}

static VkResult CreateUniformBuffer()
{
    fprintf(gLog, "[CreateUniformBuffer] Creating uniform buffer...\n");
    fflush(gLog);

    VkDeviceSize sizeBytes = sizeof(UBO);

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = sizeBytes;
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (vkCreateBuffer(gDevice, &bufInfo, nullptr, &gUniformBuffer) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateUniformBuffer] vkCreateBuffer failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(gDevice, gUniformBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        fprintf(gLog, "[CreateUniformBuffer] Could not find suitable memory type.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (vkAllocateMemory(gDevice, &allocInfo, nullptr, &gUniformBufferMemory) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateUniformBuffer] vkAllocateMemory failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkBindBufferMemory(gDevice, gUniformBuffer, gUniformBufferMemory, 0);

    fprintf(gLog, "[CreateUniformBuffer] Uniform buffer created.\n");
    fflush(gLog);
    return VK_SUCCESS;
}

// -------------------------------------------------------------------------------------------
// Descriptors
static VkResult CreateDescriptorSetLayout()
{
    fprintf(gLog, "[CreateDescriptorSetLayout] Creating descriptor set layout...\n");
    fflush(gLog);

    VkDescriptorSetLayoutBinding uboLayout{};
    uboLayout.binding            = 0;
    uboLayout.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayout.descriptorCount    = 1;
    uboLayout.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboLayout;

    if (vkCreateDescriptorSetLayout(gDevice, &layoutInfo, nullptr, &gDescSetLayout) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateDescriptorSetLayout] vkCreateDescriptorSetLayout failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    fprintf(gLog, "[CreateDescriptorSetLayout] Descriptor set layout created.\n");
    fflush(gLog);
    return VK_SUCCESS;
}

static VkResult CreateDescriptorPool()
{
    fprintf(gLog, "[CreateDescriptorPool] Creating descriptor pool...\n");
    fflush(gLog);

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(gDevice, &poolInfo, nullptr, &gDescPool) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateDescriptorPool] vkCreateDescriptorPool failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    fprintf(gLog, "[CreateDescriptorPool] Descriptor pool created.\n");
    fflush(gLog);
    return VK_SUCCESS;
}

static VkResult CreateDescriptorSet()
{
    fprintf(gLog, "[CreateDescriptorSet] Allocating and updating descriptor set...\n");
    fflush(gLog);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = gDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &gDescSetLayout;

    if (vkAllocateDescriptorSets(gDevice, &allocInfo, &gDescSet) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateDescriptorSet] vkAllocateDescriptorSets failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = gUniformBuffer;
    bufInfo.offset = 0;
    bufInfo.range  = sizeof(UBO);

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = gDescSet;
    write.dstBinding      = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo     = &bufInfo;

    vkUpdateDescriptorSets(gDevice, 1, &write, 0, nullptr);

    fprintf(gLog, "[CreateDescriptorSet] Descriptor set allocated and updated.\n");
    fflush(gLog);
    return VK_SUCCESS;
}

// -------------------------------------------------------------------------------------------
// Pipeline
static VkVertexInputBindingDescription GetBindingDesc()
{
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride  = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

static std::vector<VkVertexInputAttributeDescription> GetAttrDescs()
{
    std::vector<VkVertexInputAttributeDescription> ads(2);
    ads[0].binding  = 0;
    ads[0].location = 0;
    ads[0].format   = VK_FORMAT_R32G32_SFLOAT;
    ads[0].offset   = offsetof(Vertex, pos);

    ads[1].binding  = 0;
    ads[1].location = 1;
    ads[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    ads[1].offset   = offsetof(Vertex, color);
    return ads;
}

static VkResult CreateGraphicsPipeline()
{
    fprintf(gLog, "[CreateGraphicsPipeline] Creating graphics pipeline...\n");
    fflush(gLog);

    auto vertCode = ReadFile("vert_shader.spv");
    auto fragCode = ReadFile("frag_shader.spv");
    VkShaderModule vertModule = CreateShaderModule(vertCode);
    VkShaderModule fragModule = CreateShaderModule(fragCode);
    if (!vertModule || !fragModule)
    {
        fprintf(gLog, "[CreateGraphicsPipeline] Failed to create shader modules.\n");
        fflush(gLog);
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

    VkPipelineShaderStageCreateInfo stages[] = { vs, fs };

    auto bindingDesc = GetBindingDesc();
    auto attrDescs   = GetAttrDescs();

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount   = 1;
    vi.pVertexBindingDescriptions      = &bindingDesc;
    vi.vertexAttributeDescriptionCount = (uint32_t)attrDescs.size();
    vi.pVertexAttributeDescriptions    = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.width  = (float)gSwapExtent.width;
    viewport.height = (float)gSwapExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = gSwapExtent;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.pViewports    = &viewport;
    vp.scissorCount  = 1;
    vp.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = VK_CULL_MODE_NONE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cbAttach{};
    cbAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|
                              VK_COLOR_COMPONENT_G_BIT|
                              VK_COLOR_COMPONENT_B_BIT|
                              VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cbAttach;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &gDescSetLayout;

    if (vkCreatePipelineLayout(gDevice, &layoutInfo, nullptr, &gPipelineLayout) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateGraphicsPipeline] vkCreatePipelineLayout failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkGraphicsPipelineCreateInfo pipeInfo{};
    pipeInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeInfo.stageCount          = 2;
    pipeInfo.pStages             = stages;
    pipeInfo.pVertexInputState   = &vi;
    pipeInfo.pInputAssemblyState = &ia;
    pipeInfo.pViewportState      = &vp;
    pipeInfo.pRasterizationState = &rs;
    pipeInfo.pMultisampleState   = &ms;
    pipeInfo.pColorBlendState    = &cb;
    pipeInfo.layout              = gPipelineLayout;
    pipeInfo.renderPass          = gRenderPass;
    pipeInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(gDevice, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &gGraphicsPipeline) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateGraphicsPipeline] vkCreateGraphicsPipelines failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkDestroyShaderModule(gDevice, fragModule, nullptr);
    vkDestroyShaderModule(gDevice, vertModule, nullptr);

    fprintf(gLog, "[CreateGraphicsPipeline] Graphics pipeline created successfully.\n");
    fflush(gLog);
    return VK_SUCCESS;
}

// -------------------------------------------------------------------------------------------
// Command buffers
static VkResult CreateCommandPool()
{
    fprintf(gLog, "[CreateCommandPool] Creating command pool...\n");
    fflush(gLog);

    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = gGraphicsQueueIndex;
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    return vkCreateCommandPool(gDevice, &info, nullptr, &gCommandPool);
}

static VkResult CreateCommandBuffers()
{
    fprintf(gLog, "[CreateCommandBuffers] Creating command buffers...\n");
    fflush(gLog);

    gCommandBuffers = (VkCommandBuffer*)malloc(gSwapImageCount * sizeof(VkCommandBuffer));
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool        = gCommandPool;
    alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = gSwapImageCount;
    return vkAllocateCommandBuffers(gDevice, &alloc, gCommandBuffers);
}

// -------------------------------------------------------------------------------------------
// Render pass, framebuffers
static VkResult CreateRenderPass()
{
    fprintf(gLog, "[CreateRenderPass] Creating render pass...\n");
    fflush(gLog);

    VkAttachmentDescription colorAtt{};
    colorAtt.format         = gColorFormat;
    colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &colorAtt;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;

    return vkCreateRenderPass(gDevice, &rpInfo, nullptr, &gRenderPass);
}

static VkResult CreateFramebuffers()
{
    fprintf(gLog, "[CreateFramebuffers] Creating framebuffers...\n");
    fflush(gLog);

    gFramebuffers = (VkFramebuffer*)malloc(gSwapImageCount * sizeof(VkFramebuffer));
    for (uint32_t i = 0; i < gSwapImageCount; i++) {
        VkImageView att[] = { gSwapImageViews[i] };
        VkFramebufferCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass      = gRenderPass;
        info.attachmentCount = 1;
        info.pAttachments    = att;
        info.width           = gSwapExtent.width;
        info.height          = gSwapExtent.height;
        info.layers          = 1;
        if (vkCreateFramebuffer(gDevice, &info, nullptr, &gFramebuffers[i]) != VK_SUCCESS)
        {
            fprintf(gLog, "[CreateFramebuffers] vkCreateFramebuffer failed.\n");
            fflush(gLog);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}

// -------------------------------------------------------------------------------------------
// Swapchain
static VkResult CreateSwapchain()
{
    fprintf(gLog, "[CreateSwapchain] Creating swap chain...\n");
    fflush(gLog);

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gPhysicalDevice, gSurface, &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gPhysicalDevice, gSurface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gPhysicalDevice, gSurface, &formatCount, formats.data());
    gColorFormat = (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
                 ? VK_FORMAT_B8G8R8A8_UNORM : formats[0].format;
    gColorSpace = formats[0].colorSpace;

    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gPhysicalDevice, gSurface, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> pm(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gPhysicalDevice, gSurface, &pmCount, pm.data());
    gPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : pm) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            gPresentMode = m;
            break;
        }
    }
    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width  = (uint32_t)gWidth;
        extent.height = (uint32_t)gHeight;
        if (extent.width < caps.minImageExtent.width)  extent.width = caps.minImageExtent.width;
        if (extent.height< caps.minImageExtent.height) extent.height= caps.minImageExtent.height;
        if (extent.width > caps.maxImageExtent.width)  extent.width = caps.maxImageExtent.width;
        if (extent.height> caps.maxImageExtent.height) extent.height= caps.maxImageExtent.height;
    }
    gSwapExtent = extent;

    uint32_t desiredCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && desiredCount > caps.maxImageCount)
        desiredCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = gSurface;
    ci.minImageCount    = desiredCount;
    ci.imageFormat      = gColorFormat;
    ci.imageColorSpace  = gColorSpace;
    ci.imageExtent      = gSwapExtent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = gPresentMode;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(gDevice, &ci, nullptr, &gSwapchain) != VK_SUCCESS)
    {
        fprintf(gLog, "[CreateSwapchain] vkCreateSwapchainKHR failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetSwapchainImagesKHR(gDevice, gSwapchain, &gSwapImageCount, nullptr);
    gSwapImages = (VkImage*)malloc(gSwapImageCount * sizeof(VkImage));
    vkGetSwapchainImagesKHR(gDevice, gSwapchain, &gSwapImageCount, gSwapImages);

    gSwapImageViews = (VkImageView*)malloc(gSwapImageCount * sizeof(VkImageView));
    for (uint32_t i = 0; i < gSwapImageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image    = gSwapImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format   = gColorFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(gDevice, &viewInfo, nullptr, &gSwapImageViews[i]) != VK_SUCCESS)
        {
            fprintf(gLog, "[CreateSwapchain] vkCreateImageView failed.\n");
            fflush(gLog);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    fprintf(gLog, "[CreateSwapchain] Swap chain created. Image count: %d\n", gSwapImageCount);
    fflush(gLog);
    return VK_SUCCESS;
}

// -------------------------------------------------------------------------------------------
// Cleanup and recreate
static void cleanupSwapChain()
{
    fprintf(gLog, "[cleanupSwapChain] Cleaning up swap chain...\n");
    fflush(gLog);

    if (gFramebuffers) {
        for (uint32_t i = 0; i < gSwapImageCount; i++)
            if (gFramebuffers[i]) vkDestroyFramebuffer(gDevice, gFramebuffers[i], nullptr);
        free(gFramebuffers);
        gFramebuffers = nullptr;
    }
    if (gRenderPass) {
        vkDestroyRenderPass(gDevice, gRenderPass, nullptr);
        gRenderPass = VK_NULL_HANDLE;
    }
    if (gCommandBuffers) {
        vkFreeCommandBuffers(gDevice, gCommandPool, gSwapImageCount, gCommandBuffers);
        free(gCommandBuffers);
        gCommandBuffers = nullptr;
    }
    if (gCommandPool) {
        vkDestroyCommandPool(gDevice, gCommandPool, nullptr);
        gCommandPool = VK_NULL_HANDLE;
    }
    if (gSwapImageViews) {
        for (uint32_t i = 0; i < gSwapImageCount; i++)
            if (gSwapImageViews[i]) vkDestroyImageView(gDevice, gSwapImageViews[i], nullptr);
        free(gSwapImageViews);
        gSwapImageViews = nullptr;
    }
    if (gSwapImages) {
        free(gSwapImages);
        gSwapImages = nullptr;
    }
    if (gSwapchain) {
        vkDestroySwapchainKHR(gDevice, gSwapchain, nullptr);
        gSwapchain = VK_NULL_HANDLE;
    }
    if (gGraphicsPipeline) {
        vkDestroyPipeline(gDevice, gGraphicsPipeline, nullptr);
        gGraphicsPipeline = VK_NULL_HANDLE;
    }
    if (gPipelineLayout) {
        vkDestroyPipelineLayout(gDevice, gPipelineLayout, nullptr);
        gPipelineLayout = VK_NULL_HANDLE;
    }
}

VkResult recreateSwapChain()
{
    fprintf(gLog, "[recreateSwapChain] Recreating swap chain...\n");
    fflush(gLog);

    if (gWidth == 0 || gHeight == 0) return VK_SUCCESS;
    vkDeviceWaitIdle(gDevice);
    cleanupSwapChain();

    if (CreateSwapchain()          != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateCommandPool()        != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateCommandBuffers()     != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateRenderPass()         != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateFramebuffers()       != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateGraphicsPipeline()   != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;

    // Re-record the command buffers
    for (uint32_t i = 0; i < gSwapImageCount; i++) {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(gCommandBuffers[i], &begin);

        VkClearValue cv{};
        cv.color = gClearColor;

        VkRenderPassBeginInfo rp{};
        rp.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass       = gRenderPass;
        rp.framebuffer      = gFramebuffers[i];
        rp.renderArea.extent= gSwapExtent;
        rp.clearValueCount  = 1;
        rp.pClearValues     = &cv;

        vkCmdBeginRenderPass(gCommandBuffers[i], &rp, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(gCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gGraphicsPipeline);

        VkDescriptorSet sets[] = { gDescSet };
        vkCmdBindDescriptorSets(gCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                gPipelineLayout, 0, 1, sets, 0, nullptr);

        VkBuffer vb[] = { gVertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(gCommandBuffers[i], 0, 1, vb, offsets);

        vkCmdDraw(gCommandBuffers[i], 6, 1, 0, 0);

        vkCmdEndRenderPass(gCommandBuffers[i]);
        vkEndCommandBuffer(gCommandBuffers[i]);
    }
    fprintf(gLog, "[recreateSwapChain] Swap chain recreated successfully.\n");
    fflush(gLog);
    return VK_SUCCESS;
}

// -------------------------------------------------------------------------------------------
// Update Uniform
void UpdateUniformBuffer()
{
    UBO ubo{};
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view  = glm::mat4(1.0f);
    glm::mat4 proj  = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    // Flip Y for Vulkan
    proj[1][1] *= -1.0f;
    ubo.mvp = proj * view * model;

    void* data;
    vkMapMemory(gDevice, gUniformBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(gDevice, gUniformBufferMemory);

    fprintf(gLog, "[UpdateUniformBuffer] Uniform buffer (MVP) updated.\n");
    fflush(gLog);
}

// -------------------------------------------------------------------------------------------
// Render
VkResult drawFrame()
{
    fprintf(gLog, "[drawFrame] Starting frame. CurrentFrame: %d\n", gCurrentFrame);
    fflush(gLog);

    vkWaitForFences(gDevice, 1, &gInFlightFences[gCurrentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(gDevice, 1, &gInFlightFences[gCurrentFrame]);

    VkResult res = vkAcquireNextImageKHR(
        gDevice, gSwapchain, UINT64_MAX,
        gImageAvailableSem[gCurrentFrame],
        VK_NULL_HANDLE,
        &gCurrentImageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        fprintf(gLog, "[drawFrame] vkAcquireNextImageKHR -> OUT_OF_DATE_KHR. Recreating swapchain.\n");
        fflush(gLog);
        recreateSwapChain();
        return VK_SUCCESS;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        fprintf(gLog, "[drawFrame] vkAcquireNextImageKHR -> error %d\n", res);
        fflush(gLog);
        return res;
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &gImageAvailableSem[gCurrentFrame];
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &gCommandBuffers[gCurrentImageIndex];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &gRenderFinishedSem[gCurrentFrame];

    vkQueueSubmit(gQueue, 1, &submit, gInFlightFences[gCurrentFrame]);

    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &gRenderFinishedSem[gCurrentFrame];
    present.swapchainCount     = 1;
    present.pSwapchains        = &gSwapchain;
    present.pImageIndices      = &gCurrentImageIndex;

    res = vkQueuePresentKHR(gQueue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
    {
        fprintf(gLog, "[drawFrame] vkQueuePresentKHR -> OUT_OF_DATE or SUBOPTIMAL. Recreating.\n");
        fflush(gLog);
        recreateSwapChain();
    }

    gCurrentFrame = (gCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

// -------------------------------------------------------------------------------------------
// Vulkan initialization
static VkResult InitVulkan()
{
    fprintf(gLog, "[InitVulkan] Initializing Vulkan...\n");
    fflush(gLog);

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = gAppName;
    appInfo.applicationVersion = 1;
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    const char* instExt[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    VkInstanceCreateInfo instCI{};
    instCI.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instCI.pApplicationInfo        = &appInfo;
    instCI.enabledExtensionCount   = 2;
    instCI.ppEnabledExtensionNames = instExt;

    // Create the Vulkan instance
    if (vkCreateInstance(&instCI, nullptr, &gInstance) != VK_SUCCESS)
    {
        fprintf(gLog, "[InitVulkan] vkCreateInstance failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Create Win32 surface
    VkWin32SurfaceCreateInfoKHR surfCI{};
    surfCI.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfCI.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE);
    surfCI.hwnd      = ghwnd;
    if (vkCreateWin32SurfaceKHR(gInstance, &surfCI, nullptr, &gSurface) != VK_SUCCESS)
    {
        fprintf(gLog, "[InitVulkan] vkCreateWin32SurfaceKHR failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Enumerate physical devices
    uint32_t physCount = 0;
    vkEnumeratePhysicalDevices(gInstance, &physCount, nullptr);
    std::vector<VkPhysicalDevice> pds(physCount);
    vkEnumeratePhysicalDevices(gInstance, &physCount, pds.data());

    fprintf(gLog, "[InitVulkan] Found %u physical device(s).\n", physCount); // [LOG]
    fflush(gLog);

    // [LOG] Print properties of each device
    for (uint32_t i = 0; i < physCount; ++i)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(pds[i], &props);

        fprintf(gLog,
            "  -> Device %u: Name: %s | VendorID: 0x%X | DeviceID: 0x%X | API Version: %u.%u.%u\n",
            i, props.deviceName, props.vendorID, props.deviceID,
            VK_VERSION_MAJOR(props.apiVersion),
            VK_VERSION_MINOR(props.apiVersion),
            VK_VERSION_PATCH(props.apiVersion));
    }
    fflush(gLog);

    // Select a physical device which has a queue that supports both graphics + present
    for (auto pd : pds) {
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());

        std::vector<VkBool32> canPresent(qCount);
        for (uint32_t i = 0; i < qCount; i++)
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, gSurface, &canPresent[i]);

        for (uint32_t i = 0; i < qCount; i++) {
            if ((qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent[i]) {
                gPhysicalDevice = pd;
                gGraphicsQueueIndex = i;
                break;
            }
        }
        if (gPhysicalDevice) break;
    }

    if (!gPhysicalDevice) {
        fprintf(gLog, "[InitVulkan] No suitable physical device found!\n"); // [LOG]
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // [LOG] Print the final chosen device properties
    {
        VkPhysicalDeviceProperties chosenProps;
        vkGetPhysicalDeviceProperties(gPhysicalDevice, &chosenProps);
        fprintf(gLog, "[InitVulkan] Chosen device: %s\n", chosenProps.deviceName);
        fprintf(gLog, "[InitVulkan] Queue family index: %u\n", gGraphicsQueueIndex);
        fflush(gLog);
    }

    // Get memory properties
    vkGetPhysicalDeviceMemoryProperties(gPhysicalDevice, &gMemProps);

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qCI{};
    qCI.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qCI.queueFamilyIndex = gGraphicsQueueIndex;
    qCI.queueCount       = 1;
    qCI.pQueuePriorities = &priority;

    const char* devExt[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo devCI{};
    devCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devCI.queueCreateInfoCount    = 1;
    devCI.pQueueCreateInfos       = &qCI;
    devCI.enabledExtensionCount   = 1;
    devCI.ppEnabledExtensionNames = devExt;

    if (vkCreateDevice(gPhysicalDevice, &devCI, nullptr, &gDevice) != VK_SUCCESS)
    {
        fprintf(gLog, "[InitVulkan] vkCreateDevice failed.\n");
        fflush(gLog);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetDeviceQueue(gDevice, gGraphicsQueueIndex, 0, &gQueue);

    // Now create swapchain, command pool, buffers, etc
    if (CreateSwapchain()         != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateCommandPool()       != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateCommandBuffers()    != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateRenderPass()        != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateFramebuffers()      != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;

    if (CreateDescriptorSetLayout() != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateUniformBuffer()       != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateDescriptorPool()      != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateDescriptorSet()       != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateVertexBuffer()        != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (CreateGraphicsPipeline()    != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;

    // Create semaphores + fences
    VkSemaphoreCreateInfo semCI{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo     fenceCI{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(gDevice, &semCI, nullptr, &gImageAvailableSem[i]);
        vkCreateSemaphore(gDevice, &semCI, nullptr, &gRenderFinishedSem[i]);
        vkCreateFence(gDevice, &fenceCI, nullptr, &gInFlightFences[i]);
    }

    // Record command buffers
    for (uint32_t i = 0; i < gSwapImageCount; i++) {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(gCommandBuffers[i], &begin);

        VkClearValue cv{};
        cv.color = gClearColor;

        VkRenderPassBeginInfo rp{};
        rp.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass       = gRenderPass;
        rp.framebuffer      = gFramebuffers[i];
        rp.renderArea.extent= gSwapExtent;
        rp.clearValueCount  = 1;
        rp.pClearValues     = &cv;

        vkCmdBeginRenderPass(gCommandBuffers[i], &rp, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(gCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gGraphicsPipeline);

        VkDescriptorSet sets[] = { gDescSet };
        vkCmdBindDescriptorSets(gCommandBuffers[i],
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                gPipelineLayout, 0, 1, sets, 0, nullptr);

        VkBuffer vb[] = { gVertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(gCommandBuffers[i], 0, 1, vb, offsets);

        vkCmdDraw(gCommandBuffers[i], 6, 1, 0, 0);

        vkCmdEndRenderPass(gCommandBuffers[i]);
        vkEndCommandBuffer(gCommandBuffers[i]);
    }

    gInitialized = true;
    fprintf(gLog, "[InitVulkan] Vulkan initialized successfully.\n");
    fflush(gLog);
    return VK_SUCCESS;
}

// -------------------------------------------------------------------------------------------
// Cleanup
static void Cleanup()
{
    fprintf(gLog, "[Cleanup] Cleaning up...\n");
    fflush(gLog);

    vkDeviceWaitIdle(gDevice);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (gInFlightFences[i])       vkDestroyFence(gDevice, gInFlightFences[i], nullptr);
        if (gImageAvailableSem[i])    vkDestroySemaphore(gDevice, gImageAvailableSem[i], nullptr);
        if (gRenderFinishedSem[i])    vkDestroySemaphore(gDevice, gRenderFinishedSem[i], nullptr);
    }

    cleanupSwapChain();

    if (gVertexBuffer) {
        vkDestroyBuffer(gDevice, gVertexBuffer, nullptr);
        gVertexBuffer = VK_NULL_HANDLE;
    }
    if (gVertexBufferMemory) {
        vkFreeMemory(gDevice, gVertexBufferMemory, nullptr);
        gVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (gUniformBuffer) {
        vkDestroyBuffer(gDevice, gUniformBuffer, nullptr);
        gUniformBuffer = VK_NULL_HANDLE;
    }
    if (gUniformBufferMemory) {
        vkFreeMemory(gDevice, gUniformBufferMemory, nullptr);
        gUniformBufferMemory = VK_NULL_HANDLE;
    }
    if (gDescPool) {
        vkDestroyDescriptorPool(gDevice, gDescPool, nullptr);
        gDescPool = VK_NULL_HANDLE;
    }
    if (gDescSetLayout) {
        vkDestroyDescriptorSetLayout(gDevice, gDescSetLayout, nullptr);
        gDescSetLayout = VK_NULL_HANDLE;
    }

    if (gDevice) {
        vkDestroyDevice(gDevice, nullptr);
        gDevice = VK_NULL_HANDLE;
    }
    if (gSurface) {
        vkDestroySurfaceKHR(gInstance, gSurface, nullptr);
        gSurface = VK_NULL_HANDLE;
    }
    if (gInstance) {
        vkDestroyInstance(gInstance, nullptr);
        gInstance = VK_NULL_HANDLE;
    }

    fprintf(gLog, "[Cleanup] Cleanup done.\n");
    fflush(gLog);
}

// -------------------------------------------------------------------------------------------
// WinMain
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    gLog = fopen("Log.txt", "w");
    if (!gLog) {
        MessageBox(NULL, TEXT("Could not open Log.txt for writing!"), TEXT("Error"), MB_OK);
        return 0;
    }
    fprintf(gLog, "Log file created.\n");
    fflush(gLog);

    WNDCLASSEX wc{};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = gAppName;
    RegisterClassEx(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x  = (sw - WIN_WIDTH) / 2;
    int y  = (sh - WIN_HEIGHT)/ 2;

    ghwnd = CreateWindowEx(
        WS_EX_APPWINDOW, gAppName, gAppName,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, WIN_WIDTH, WIN_HEIGHT,
        nullptr, nullptr, hInst, nullptr);

    if (!ghwnd) {
        fprintf(gLog, "[WinMain] Failed to create window.\n");
        fflush(gLog);
        fclose(gLog);
        return 0;
    }

    fprintf(gLog, "[WinMain] Window created. Initializing Vulkan...\n");
    fflush(gLog);

    if (InitVulkan() != VK_SUCCESS) {
        MessageBox(NULL, TEXT("Vulkan initialization failed."), TEXT("Error"), MB_OK);
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    ShowWindow(ghwnd, SW_SHOW);
    UpdateWindow(ghwnd);
    SetForegroundWindow(ghwnd);
    SetFocus(ghwnd);

    MSG msg{};
    while (true) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            if (gbActive) {
                UpdateUniformBuffer();
                drawFrame();
            }
        }
    }

    Cleanup();

    if (gLog) {
        fprintf(gLog, "[WinMain] Exiting application.\n");
        fclose(gLog);
        gLog = nullptr;
    }

    return (int)msg.wParam;
}
