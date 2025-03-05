cls

del Vk.exe Log.txt

cl.exe /c /EHsc Vk.c

rc.exe Vk.rc

link.exe Vk.obj Vk.res Kernel32.lib User32.lib GDI32.lib /SUBSYSTEM:WINDOWS /MACHINE:x64

del Vk.res Vk.obj

Vk.exe
