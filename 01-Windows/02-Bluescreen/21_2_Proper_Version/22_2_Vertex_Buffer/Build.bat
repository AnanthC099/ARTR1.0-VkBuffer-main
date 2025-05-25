cls

del Vk.exe Log.txt

cl /I"C:\VulkanSDK\1.4.309.0\Include" /c Vk.c /Fo"Vk.obj"

rc.exe Vk.rc

link Vk.obj Vk.res /LIBPATH:"C:\VulkanSDK\1.4.309.0\Lib" vulkan-1.lib user32.lib gdi32.lib kernel32.lib /OUT:Vk.exe 

del Vk.obj Vk.pdb Vk.res

Vk.exe

