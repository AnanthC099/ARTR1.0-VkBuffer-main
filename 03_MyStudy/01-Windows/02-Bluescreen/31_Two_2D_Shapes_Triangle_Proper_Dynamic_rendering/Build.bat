cls

del Vk.exe Log.txt vert_shader.spv frag_shader.spv

glslc -fshader-stage=vert vert_shader.glsl -o vert_shader.spv

glslc -fshader-stage=frag frag_shader.glsl -o frag_shader.spv

cl /EHsc /I"C:\VulkanSDK\Anjaneya\Include" /c Vk.cpp /Fo"Vk.obj"

rc.exe Vk.rc

link Vk.obj Vk.res /LIBPATH:"C:\VulkanSDK\Anjaneya\Lib" vulkan-1.lib user32.lib gdi32.lib kernel32.lib /OUT:Vk.exe 

del Vk.obj Vk.pdb Vk.res

Vk.exe

