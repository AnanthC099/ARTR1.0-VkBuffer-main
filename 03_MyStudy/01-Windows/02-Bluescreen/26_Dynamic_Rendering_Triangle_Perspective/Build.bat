cls

del Vk.exe Log.txt triangle.vert.spirv triangle.frag.spirv

glslc -fshader-stage=vert triangle.vert -o triangle.vert.spirv

glslc -fshader-stage=frag triangle.frag -o triangle.frag.spirv

cl /I"C:\VulkanSDK\Vulkan\Include" /c Vk.cpp /Fo"Vk.obj"

rc.exe Vk.rc

link Vk.obj Vk.res /LIBPATH:"C:\VulkanSDK\Vulkan\Lib" vulkan-1.lib user32.lib gdi32.lib kernel32.lib /OUT:Vk.exe 

del Vk.obj Vk.pdb Vk.res

Vk.exe


