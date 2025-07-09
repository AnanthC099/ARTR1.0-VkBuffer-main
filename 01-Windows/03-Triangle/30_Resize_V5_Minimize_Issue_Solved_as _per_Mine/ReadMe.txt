destroy
Framebuffer
CommandBuffer
Pipeline
PipelineLayout
RenderPass
Swapchain Images and Views
Swapchain

Recreate -> Size dependant resources (In initialize we do both size dependant and size independent resources. In case of size dependant resources indicatyed below, there is interdepenendency also.)
Swapchain
Swapchain Images and Views
RenderPass

PipelineLayout
Pipeline

Framebuffers

CommandBuffers (SwapchainImageCount lagto)
buildCommandBuffer

Prototype sequence as above in resize()
