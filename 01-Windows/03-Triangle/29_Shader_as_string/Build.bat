cls

del Vk.exe Log.txt

cl /MD /std:c++17 /I"C:\VulkanSDK\Vulkan\Include" /I"C:\VulkanSDK\Vulkan\Include\glslang" /c Vk.cpp /Fo"Vk.obj"

rc.exe Vk.rc

link Vk.obj Vk.res /LIBPATH:"C:\VulkanSDK\Vulkan\Lib" vulkan-1.lib user32.lib gdi32.lib kernel32.lib  /OUT:Vk.exe 

del Vk.obj Vk.pdb Vk.res

Vk.exe

