#include <stdio.h>		
#include <stdlib.h>	
#include <windows.h>		

#include "VK.h"			
#define LOG_FILE (char*)"Log.txt" 

//Vulkan related header files
#define VK_USE_PLATFORM_WIN32_KHR // XLIB_KHR, MACOS_KHR & MOLTEN something
#include <vulkan/vulkan.h> //(Only those members are enabled connected with above macro {conditional compilation using #ifdef internally})

#define WIN_WIDTH 800
#define WIN_HEIGHT 600

//Vulkan related libraries
#pragma comment(lib, "vulkan-1.lib")

// Global Function Declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

const char* gpszAppName = "ARTR";

HWND ghwnd = NULL;
BOOL gbActive = FALSE;
DWORD dwStyle = 0;
//WINDOWPLACEMENT wpPrev = { sizeof(WINDOWPLACEMENT) }; //dont do this as cpp style
WINDOWPLACEMENT wpPrev;
BOOL gbFullscreen = FALSE;

// Global Variable Declarations
FILE* gFILE = NULL;

//Vulkan related global variables

//Instance extension related variables
uint32_t enabledInstanceExtensionsCount = 0;
/*
VK_KHR_SURFACE_EXTENSION_NAME
VK_KHR_WIN32_SURFACE_EXTENSION_NAME
*/
const char* enabledInstanceExtensionNames_array[2];

//Vulkan Instance
VkInstance vkInstance = VK_NULL_HANDLE;

// Entry-Point Function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
	// Function Declarations
	VkResult initialize(void);
	void uninitialize(void);
	void display(void);
	void update(void);

	// Local Variable Declarations
	WNDCLASSEX wndclass;
	HWND hwnd;
	MSG msg;
	TCHAR szAppName[256];
	int iResult = 0;

	int SW = GetSystemMetrics(SM_CXSCREEN);
	int SH = GetSystemMetrics(SM_CYSCREEN);
	int xCoordinate = ((SW / 2) - (WIN_WIDTH / 2));
	int yCoordinate = ((SH / 2) - (WIN_HEIGHT / 2));

	BOOL bDone = FALSE;
	VkResult vkResult = VK_SUCCESS;

	// Code

	// Log File
	gFILE = fopen(LOG_FILE, "w");
	if (!gFILE)
	{
		MessageBox(NULL, TEXT("Program cannot open log file!"), TEXT("Error"), MB_OK | MB_ICONERROR);
		exit(0);
	}
	else
	{
		fprintf(gFILE, "WinMain()-> Program started successfully\n");
	}
	
	wsprintf(szAppName, TEXT("%s"), gpszAppName);

	// WNDCLASSEX Initilization 
	wndclass.cbSize = sizeof(WNDCLASSEX);
	wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.lpfnWndProc = WndProc;
	wndclass.hInstance = hInstance;
	wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.lpszClassName = szAppName;
	wndclass.lpszMenuName = NULL;
	wndclass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));

	// Register WNDCLASSEX
	RegisterClassEx(&wndclass);


	// Create Window								// glutCreateWindow
	hwnd = CreateWindowEx(WS_EX_APPWINDOW,			// to above of taskbar for fullscreen
						szAppName,
						TEXT("03_Instance"),
						WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
						xCoordinate,				// glutWindowPosition 1st Parameter
						yCoordinate,				// glutWindowPosition 2nd Parameter
						WIN_WIDTH,					// glutWindowSize 1st Parameter
						WIN_HEIGHT,					// glutWindowSize 2nd Parameter
						NULL,
						NULL,
						hInstance,
						NULL);

	ghwnd = hwnd;

	// Initialization
	vkResult = initialize();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "WinMain(): initialize()  function failed\n");
		DestroyWindow(hwnd);
		hwnd = NULL;
	}
	else
	{
		fprintf(gFILE, "WinMain(): initialize() succedded\n");
	}

	// Show The Window
	ShowWindow(hwnd, iCmdShow);
	UpdateWindow(hwnd);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);

	// Game Loop
	while (bDone == FALSE)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				bDone = TRUE;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			if (gbActive == TRUE)
			{
				display();
				update();
			}
		}
	}

	// Uninitialization
	uninitialize();	

	return((int)msg.wParam);
}


// CALLBACK Function
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	// Function Declarations
	void ToggleFullscreen( void );
	void resize(int, int);

	// Code
	switch (iMsg)
	{
		case WM_CREATE:
			memset((void*)&wpPrev, 0 , sizeof(WINDOWPLACEMENT));
			wpPrev.length = sizeof(WINDOWPLACEMENT);
		break;
		
		case WM_SETFOCUS:
			gbActive = TRUE;
			break;

		case WM_KILLFOCUS:
			gbActive = FALSE;
			break;

		case WM_SIZE:
			resize(LOWORD(lParam), HIWORD(lParam));
			break;

		/*
		case WM_ERASEBKGND:
			return(0);
		*/

		case WM_KEYDOWN:
			switch (LOWORD(wParam))
			{
			case VK_ESCAPE:
				fprintf(gFILE, "WndProc() VK_ESCAPE-> Program ended successfully.\n");
				fclose(gFILE);
				gFILE = NULL;
				DestroyWindow(hwnd);
				break;
			}
			break;

		case WM_CHAR:
			switch (LOWORD(wParam))
			{
			case 'F':
			case 'f':
				if (gbFullscreen == FALSE)
				{
					ToggleFullscreen();
					gbFullscreen = TRUE;
					fprintf(gFILE, "WndProc() WM_CHAR(F key)-> Program entered Fullscreen.\n");
				}
				else
				{
					ToggleFullscreen();
					gbFullscreen = FALSE;
					fprintf(gFILE, "WndProc() WM_CHAR(F key)-> Program ended Fullscreen.\n");
				}
				break;
			}
			break;

		case WM_RBUTTONDOWN:								
			DestroyWindow(hwnd);
			break;

		case WM_CLOSE:
			uninitialize();
			DestroyWindow(hwnd);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		
		default:
			break;
	}

	return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}


void ToggleFullscreen(void)
{
	// Local Variable Declarations
	MONITORINFO mi = { sizeof(MONITORINFO) };

	// Code
	if (gbFullscreen == FALSE)
	{
		dwStyle = GetWindowLong(ghwnd, GWL_STYLE);

		if (dwStyle & WS_OVERLAPPEDWINDOW)
		{
			if (GetWindowPlacement(ghwnd, &wpPrev) && GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi))
			{
				SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);

				SetWindowPos(ghwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOZORDER | SWP_FRAMECHANGED);
				// HWND_TOP ~ WS_OVERLAPPED, rc ~ RECT, SWP_FRAMECHANGED ~ WM_NCCALCSIZE msg
			}
		}

		ShowCursor(FALSE);
	}
	else {
		SetWindowPlacement(ghwnd, &wpPrev);
		SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
		// SetWindowPos has greater priority than SetWindowPlacement and SetWindowStyle for Z-Order
		ShowCursor(TRUE);
	}
}

VkResult initialize(void)
{
	//Function declaration
	VkResult CreateVulkanInstance(void);
	
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;
	
	// Code
	vkResult = CreateVulkanInstance();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "initialize(): CreateVulkanInstance()  function failed\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "initialize(): CreateVulkanInstance() succedded\n");
	}
	

	return vkResult;
}


void resize(int width, int height)
{
	// Code
}


void display(void)
{
	// Code
}


void update(void)
{
	// Code
}

void uninitialize(void)
{
		// Function Declarations
		void ToggleFullScreen(void);


		if (gbFullscreen == TRUE)
		{
			ToggleFullscreen();
			gbFullscreen = FALSE;
		}

		// Destroy Window
		if (ghwnd)
		{
			DestroyWindow(ghwnd);
			ghwnd = NULL;
		}

		/*
		Destroy VkInstance in uninitialize()
		*/
		if(vkInstance)
		{
			vkDestroyInstance(vkInstance, NULL); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkDestroyInstance.html
			vkInstance = VK_NULL_HANDLE;
			fprintf(gFILE, "uninitialize(): vkDestroyInstance() sucedded\n");
		}

		// Close the log file
		if (gFILE)
		{
			fprintf(gFILE, "uninitialize()-> Program ended successfully.\n");
			fclose(gFILE);
			gFILE = NULL;
		}

}

//Definition of Vulkan related functions

VkResult CreateVulkanInstance(void)
{
	/*
		As explained before fill and initialize required extension names and count in 2 respective global variables (Lasst 8 steps mhanje instance cha first step)
	*/
	//Function declarations
	VkResult FillInstanceExtensionNames(void);
	
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;

	// Code
	vkResult = FillInstanceExtensionNames();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateVulkanInstance(): FillInstanceExtensionNames()  function failed\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateVulkanInstance(): FillInstanceExtensionNames() succedded\n");
	}
	
	/*
	Initialize struct VkApplicationInfo (Somewhat limbu timbu)
	*/
	struct VkApplicationInfo vkApplicationInfo;
	memset((void*)&vkApplicationInfo, 0, sizeof(struct VkApplicationInfo)); //Dont use ZeroMemory to keep parity across all OS
	
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkApplicationInfo.html/
	vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; //First member of all Vulkan structure, for genericness and typesafety
	vkApplicationInfo.pNext = NULL;
	vkApplicationInfo.pApplicationName = gpszAppName; //any string will suffice
	vkApplicationInfo.applicationVersion = 1; //any number will suffice
	vkApplicationInfo.pEngineName = gpszAppName; //any string will suffice
	vkApplicationInfo.engineVersion = 1; //any number will suffice
	/*
	Mahatavacha aahe, 
	on fly risk aahe Sir used VK_API_VERSION_1_3 as installed 1.3.296 version
	Those using 1.4.304 must use VK_API_VERSION_1_4
	*/
	vkApplicationInfo.apiVersion = VK_API_VERSION_1_4; 
	
	/*
	Initialize struct VkInstanceCreateInfo by using information from Step1 and Step2 (Important)
	*/
	struct VkInstanceCreateInfo vkInstanceCreateInfo;
	memset((void*)&vkInstanceCreateInfo, 0, sizeof(struct VkInstanceCreateInfo));
	
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkInstanceCreateInfo.html
	vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vkInstanceCreateInfo.pNext = NULL;
	vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;
	//folowing 2 members important
	vkInstanceCreateInfo.enabledExtensionCount = enabledInstanceExtensionsCount;
	vkInstanceCreateInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;
	
	/*
	Call vkCreateInstance() to get VkInstance in a global variable and do error checking
	*/
	//https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateInstance.html
	//2nd parameters is NULL as saying tuza memory allocator vapar , mazyakade custom memory allocator nahi
	vkResult = vkCreateInstance(&vkInstanceCreateInfo, NULL, &vkInstance);
	if (vkResult == VK_ERROR_INCOMPATIBLE_DRIVER)
	{
		fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() function failed due to incompatible driver with error code %d\n", vkResult);
		return vkResult;
	}
	else if (vkResult == VK_ERROR_EXTENSION_NOT_PRESENT)
	{
		fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() function failed due to required extension not present with error code %d\n", vkResult);
		return vkResult;
	}
	else if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() function failed due to unknown reason with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateVulkanInstance(): vkCreateInstance() succedded\n");
	}
	
	return vkResult;
}

VkResult FillInstanceExtensionNames(void)
{
	// Code
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;

	/*
	1. Find how many instance extensions are supported by Vulkan driver of/for this version and keept the count in a local variable.
	1.3.296 madhe ek instance navta , je aata add zala aahe 1.4.304 madhe , VK_NV_DISPLAY_STEREO
	*/
	uint32_t instanceExtensionCount = 0;

	//https://registry.khronos.org/vulkan/specs/latest/man/html/vkEnumerateInstanceExtensionProperties.html
	vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, NULL);
	/* like in OpenCL
	1st - which layer extension required, as want all so NULL (akha driver supported kelleli extensions)
	2nd - count de mala
	3rd - Extension cha property cha array, NULL aahe karan count nahi ajun aplyakade
	*/
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "FillInstanceExtensionNames(): First call to vkEnumerateInstanceExtensionProperties()  function failed\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "FillInstanceExtensionNames(): First call to vkEnumerateInstanceExtensionProperties() succedded\n");
	}

	/*
	 Allocate and fill struct VkExtensionProperties 
	 (https://registry.khronos.org/vulkan/specs/latest/man/html/VkExtensionProperties.html) structure array, 
	 corresponding to above count
	*/
	VkExtensionProperties* vkExtensionProperties_array = NULL;
	vkExtensionProperties_array = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * instanceExtensionCount);
	if (vkExtensionProperties_array != NULL)
	{
		//Add log here later for failure
		//exit(-1);
	}
	else
	{
		//Add log here later for success
	}

	vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, vkExtensionProperties_array);
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "FillInstanceExtensionNames(): Second call to vkEnumerateInstanceExtensionProperties()  function failed\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "FillInstanceExtensionNames(): Second call to vkEnumerateInstanceExtensionProperties() succedded\n");
	}

	/*
	Fill and display a local string array of extension names obtained from VkExtensionProperties structure array
	*/
	char** instanceExtensionNames_array = NULL;
	instanceExtensionNames_array = (char**)malloc(sizeof(char*) * instanceExtensionCount);
	if (instanceExtensionNames_array != NULL)
	{
		//Add log here later for failure
		//exit(-1);
	}
	else
	{
		//Add log here later for success
	}

	for (uint32_t i =0; i < instanceExtensionCount; i++)
	{
		//https://registry.khronos.org/vulkan/specs/latest/man/html/VkExtensionProperties.html
		instanceExtensionNames_array[i] = (char*)malloc( sizeof(char) * (strlen(vkExtensionProperties_array[i].extensionName) + 1));
		memcpy(instanceExtensionNames_array[i], vkExtensionProperties_array[i].extensionName, (strlen(vkExtensionProperties_array[i].extensionName) + 1));
		fprintf(gFILE, "FillInstanceExtensionNames(): Vulkan Extension Name = %s\n", instanceExtensionNames_array[i]);
	}

	/*
	As not required here onwards, free VkExtensionProperties array
	*/
	if (vkExtensionProperties_array)
	{
		free(vkExtensionProperties_array);
		vkExtensionProperties_array = NULL;
	}

	/*
	Find whether above extension names contain our required two extensions
	VK_KHR_SURFACE_EXTENSION_NAME
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME
	Accordingly set two global variables, "required extension count" and "required extension names array"
	*/
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkBool32.html -> Vulkan cha bool
	VkBool32 vulkanSurfaceExtensionFound = VK_FALSE;
	VkBool32 vulkanWin32SurfaceExtensionFound = VK_FALSE;
	for (uint32_t i = 0; i < instanceExtensionCount; i++)
	{
		if (strcmp(instanceExtensionNames_array[i], VK_KHR_SURFACE_EXTENSION_NAME) == 0)
		{
			vulkanSurfaceExtensionFound = VK_TRUE;
			enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
		}

		if (strcmp(instanceExtensionNames_array[i], VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0)
		{
			vulkanWin32SurfaceExtensionFound = VK_TRUE;
			enabledInstanceExtensionNames_array[enabledInstanceExtensionsCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
		}
	}

	/*
	As not needed hence forth , free local string array
	*/
	for (uint32_t i =0 ; i < instanceExtensionCount; i++)
	{
		free(instanceExtensionNames_array[i]);
	}
	free(instanceExtensionNames_array);

	/*
	Print whether our required instance extension names or not (He log madhe yenar. Jithe print asel sarv log madhe yenar)
	*/
	if (vulkanSurfaceExtensionFound == VK_FALSE)
	{
		//Type mismatch in return VkResult and VKBool32, so return hardcoded failure
		vkResult = VK_ERROR_INITIALIZATION_FAILED; //return hardcoded failure
		fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_SURFACE_EXTENSION_NAME not found\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_SURFACE_EXTENSION_NAME is found\n");
	}

	if (vulkanWin32SurfaceExtensionFound == VK_FALSE)
	{
		//Type mismatch in return VkResult and VKBool32, so return hardcoded failure
		vkResult = VK_ERROR_INITIALIZATION_FAILED; //return hardcoded failure
		fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_WIN32_SURFACE_EXTENSION_NAME not found\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "FillInstanceExtensionNames(): VK_KHR_WIN32_SURFACE_EXTENSION_NAME is found\n");
	}

	/*
	Print only enabled extension names
	*/
	for (uint32_t i = 0; i < enabledInstanceExtensionsCount; i++)
	{
		fprintf(gFILE, "FillInstanceExtensionNames(): Enabled Vulkan Instance Extension Name = %s\n", enabledInstanceExtensionNames_array[i]);
	}

	return vkResult;
}



