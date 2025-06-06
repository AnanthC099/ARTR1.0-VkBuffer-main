#include <stdio.h>		
#include <stdlib.h>	
#include <windows.h>	
#include <math.h>	

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

//Vulkan Presentation Surface
/*
Declare a global variable to hold presentation surface object
*/
VkSurfaceKHR vkSurfaceKHR = VK_NULL_HANDLE;

/*
Vulkan Physical device related global variables
*/
VkPhysicalDevice vkPhysicalDevice_selected = VK_NULL_HANDLE;//https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDevice.html
uint32_t graphicsQuequeFamilyIndex_selected = UINT32_MAX; //ata max aahe mag apan proper count deu
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDeviceMemoryProperties.html (Itha nahi lagnaar, staging ani non staging buffers la lagel)

/*
PrintVulkanInfo() changes
1. Remove local declaration of physicalDeviceCount and physicalDeviceArray from GetPhysicalDevice() and do it globally.
*/
uint32_t physicalDeviceCount = 0;
VkPhysicalDevice *vkPhysicalDevice_array = NULL; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDevice.html

//Device extension related variables {In MAC , we need to add portability etensions, so there will be 2 extensions. Similarly for ray tracing there will be atleast 8 extensions.}
uint32_t enabledDeviceExtensionsCount = 0;
/*
VK_KHR_SWAPCHAIN_EXTENSION_NAME
*/
const char* enabledDeviceExtensionNames_array[1];

/*
Vulkan Device
*/
VkDevice vkDevice = VK_NULL_HANDLE; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkDevice.html

/*
Device Queque
*/
VkQueue vkQueue =  VK_NULL_HANDLE; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkQueue.html

/*
Color Format and Color Space
*/
VkFormat vkFormat_color = VK_FORMAT_UNDEFINED; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkFormat.html {Will be also needed for depth later}
VkColorSpaceKHR vkColorSpaceKHR = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkColorSpaceKHR.html

/*
Presentation Mode
https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceSurfacePresentModesKHR.html
https://registry.khronos.org/vulkan/specs/latest/man/html/VkPresentModeKHR.html
*/
VkPresentModeKHR vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkPresentModeKHR.html

/*
SwapChain Related Global variables
*/
int winWidth = WIN_WIDTH;
int winHeight = WIN_HEIGHT;

//https://registry.khronos.org/vulkan/specs/latest/man/html/VkSwapchainKHR.html
VkSwapchainKHR vkSwapchainKHR =  VK_NULL_HANDLE;

//https://registry.khronos.org/vulkan/specs/latest/man/html/VkExtent2D.html
VkExtent2D vkExtent2D_SwapChain;

/*
Swapchain images and Swapchain image views related variables
*/
uint32_t swapchainImageCount = UINT32_MAX;

//https://registry.khronos.org/vulkan/specs/latest/man/html/VkImage.html
VkImage *swapChainImage_array = NULL;

//https://registry.khronos.org/vulkan/specs/latest/man/html/VkImageView.html
VkImageView *swapChainImageView_array = NULL;

/*
Command Pool
*/

//https://registry.khronos.org/vulkan/specs/latest/man/html/VkCommandPool.html
VkCommandPool vkCommandPool = VK_NULL_HANDLE; 

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
						TEXT("05_PhysicalDevice"),
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
	VkResult GetSupportedSurface(void);
	VkResult GetPhysicalDevice(void);
	VkResult PrintVulkanInfo(void);
	VkResult CreateVulKanDevice(void);
	void GetDeviceQueque(void);
	VkResult CreateSwapChain(VkBool32);
	VkResult CreateImagesAndImageViews(void);
	VkResult CreateCommandPool(void);
	
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;
	
	// Code
	vkResult = CreateVulkanInstance();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "initialize(): CreateVulkanInstance() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "initialize(): CreateVulkanInstance() succedded\n");
	}
	
	//Create Vulkan Presentation Surface
	vkResult = GetSupportedSurface();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "initialize(): GetSupportedSurface() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "initialize(): GetSupportedSurface() succedded\n");
	}
	
	//Enumerate and select physical device and its queque family index
	vkResult = GetPhysicalDevice();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "initialize(): GetPhysicalDevice() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "initialize(): GetPhysicalDevice() succedded\n");
	}
	
	//Print Vulkan Info ;
	vkResult = PrintVulkanInfo();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "initialize(): PrintVulkanInfo() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "initialize(): PrintVulkanInfo() succedded\n");
	}
	
	//Create Vulkan Device (Logical Device)
	vkResult = CreateVulKanDevice(); 
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "initialize(): CreateVulKanDevice() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "initialize(): CreateVulKanDevice() succedded\n");
	}
	
	//get Device Queque
	GetDeviceQueque();
	
	vkResult = CreateSwapChain(VK_FALSE); //https://registry.khronos.org/vulkan/specs/latest/man/html/VK_FALSE.html
	if (vkResult != VK_SUCCESS)
	{
		/*
		Why are we giving hardcoded error when returbn value is vkResult?
		Answer sir will give in swapchain
		*/
		vkResult = VK_ERROR_INITIALIZATION_FAILED; //return hardcoded failure
		fprintf(gFILE, "initialize(): CreateSwapChain() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "initialize(): CreateSwapChain() succedded\n");
	}
	
	//1. Get Swapchain image count in a global variable using vkGetSwapchainImagesKHR() API (https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetSwapchainImagesKHR.html).
	//Create Vulkan images and image views
	vkResult =  CreateImagesAndImageViews();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "initialize(): CreateImagesAndImageViews() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "initialize(): CreateImagesAndImageViews() succedded with SwapChain Image count as %d\n", swapchainImageCount);
	}
	
	vkResult = CreateCommandPool();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "initialize(): CreateCommandPool() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "initialize(): CreateCommandPool() succedded with SwapChain Image count as %d\n", swapchainImageCount);
	}
	
	fprintf(gFILE, "************************* End of initialize ******************************\n");
	
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

/*
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
		
		
		//10. When done destroy it uninitilialize() by using vkDestroySwapchainKHR() (https://registry.khronos.org/vulkan/specs/latest/man/html/vkDestroySwapchainKHR.html) Vulkan API.
		//Destroy swapchain
		vkDestroySwapchainKHR(vkDevice, vkSwapchainKHR, NULL);
		vkSwapchainKHR = VK_NULL_HANDLE;
		fprintf(gFILE, "uninitialize(): vkDestroySwapchainKHR() is done\n");
		
		//Destroy Vulkan device
		
		//No need to destroy/uninitialize device queque
		
		//No need to destroy selected physical device
		if(vkDevice)
		{
			vkDeviceWaitIdle(vkDevice); //First synchronization function
			fprintf(gFILE, "uninitialize(): vkDeviceWaitIdle() is done\n");
			vkDestroyDevice(vkDevice, NULL); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkDestroyDevice.html
			vkDevice = VK_NULL_HANDLE;
			fprintf(gFILE, "uninitialize(): vkDestroyDevice() is done\n");
		}
		
		if(vkSurfaceKHR)
		{
			// The destroy() of vkDestroySurfaceKHR() generic not platform specific
			vkDestroySurfaceKHR(vkInstance, vkSurfaceKHR, NULL); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkDestroySurfaceKHR.html
			vkSurfaceKHR = VK_NULL_HANDLE;
			fprintf(gFILE, "uninitialize(): vkDestroySurfaceKHR() sucedded\n");
		}

		// Destroy VkInstance in uninitialize()
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
*/

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
		
		//Destroy Vulkan device
		if(vkDevice)
		{
			vkDeviceWaitIdle(vkDevice); //First synchronization function
			fprintf(gFILE, "uninitialize(): vkDeviceWaitIdle() is done\n");
			
			/*
			Step_14_3 In uninitialize(), destroy commandpool using VkDestroyCommandPool.
			// https://registry.khronos.org/vulkan/specs/latest/man/html/vkDestroyCommandPool.html
			*/
			if(vkCommandPool)
			{
				vkDestroyCommandPool(vkDevice, vkCommandPool, NULL);
				vkCommandPool = VK_NULL_HANDLE;
				fprintf(gFILE, "uninitialize(): vkDestroyCommandPool() is done\n");
			}
			
			/*
			9. In unitialize(), keeping the "destructor logic aside" for a while , first destroy image views from imagesViews array in a loop using vkDestroyImageViews() api.
			(https://registry.khronos.org/vulkan/specs/latest/man/html/vkDestroyImageView.html)
			*/
			for(uint32_t i =0; i < swapchainImageCount; i++)
			{
				vkDestroyImageView(vkDevice, swapChainImageView_array[i], NULL);
				fprintf(gFILE, "uninitialize(): vkDestroyImageView() is done\n");
			}
			
			/*
			10. In uninitialize() , now actually free imageView array using free().
			free imageView array
			*/
			if(swapChainImageView_array)
			{
				free(swapChainImageView_array);
				swapChainImageView_array = NULL;
				fprintf(gFILE, "uninitialize():swapChainImageView_array is freed\n");
			}
			
			/*
			7. In unitialize(), keeping the "destructor logic aside" for a while , first destroy swapchain images from swap chain images array in a loop using vkDestroyImage() api. 
			(https://registry.khronos.org/vulkan/specs/latest/man/html/vkDestroyImage.html)
			//Free swap chain images
			*/
			/*
			for(uint32_t i = 0; i < swapchainImageCount; i++)
			{
				vkDestroyImage(vkDevice, swapChainImage_array[i], NULL);
				fprintf(gFILE, "uninitialize(): vkDestroyImage() is done\n");
			}
			*/
			
			/*
			8. In uninitialize() , now actually free swapchain image array using free().
			*/
			if(swapChainImage_array)
			{
				free(swapChainImage_array);
				swapChainImage_array = NULL;
				fprintf(gFILE, "uninitialize():swapChainImage_array is freed\n");
			}
			
			/*
			10. When done destroy it uninitilialize() by using vkDestroySwapchainKHR() (https://registry.khronos.org/vulkan/specs/latest/man/html/vkDestroySwapchainKHR.html) Vulkan API.
			Destroy swapchain
			*/
			vkDestroySwapchainKHR(vkDevice, vkSwapchainKHR, NULL);
			vkSwapchainKHR = VK_NULL_HANDLE;
			fprintf(gFILE, "uninitialize(): vkDestroySwapchainKHR() is done\n");
			
			
			vkDestroyDevice(vkDevice, NULL); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkDestroyDevice.html
			vkDevice = VK_NULL_HANDLE;
			fprintf(gFILE, "uninitialize(): vkDestroyDevice() is done\n");
		}
		
		//No need to destroy/uninitialize device queque
		
		//No need to destroy selected physical device
		
		if(vkSurfaceKHR)
		{
			/*
			The destroy() of vkDestroySurfaceKHR() generic not platform specific
			*/
			vkDestroySurfaceKHR(vkInstance, vkSurfaceKHR, NULL); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkDestroySurfaceKHR.html
			vkSurfaceKHR = VK_NULL_HANDLE;
			fprintf(gFILE, "uninitialize(): vkDestroySurfaceKHR() sucedded\n");
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
		fprintf(gFILE, "FillInstanceExtensionNames(): Vulkan Instance Extension Name = %s\n", instanceExtensionNames_array[i]);
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

VkResult GetSupportedSurface(void)
{
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;
	
	/*
	Declare and memset a platform(Windows, Linux , Android etc) specific SurfaceInfoCreate structure
	*/
	VkWin32SurfaceCreateInfoKHR vkWin32SurfaceCreateInfoKHR;
	memset((void*)&vkWin32SurfaceCreateInfoKHR, 0 , sizeof(struct VkWin32SurfaceCreateInfoKHR));
	
	/*
	Initialize it , particularly its HINSTANCE and HWND members
	*/
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkWin32SurfaceCreateInfoKHR.html
	vkWin32SurfaceCreateInfoKHR.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	vkWin32SurfaceCreateInfoKHR.pNext = NULL;
	vkWin32SurfaceCreateInfoKHR.flags = 0;
	vkWin32SurfaceCreateInfoKHR.hinstance = (HINSTANCE)GetWindowLongPtr(ghwnd, GWLP_HINSTANCE); //This member can also be initialized by using (HINSTANCE)GetModuleHandle(NULL); {typecasted HINSTANCE}
	vkWin32SurfaceCreateInfoKHR.hwnd = ghwnd;
	
	/*
	Now call VkCreateWin32SurfaceKHR() to create the presentation surface object
	*/
	//https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateWin32SurfaceKHR.html
	vkResult = vkCreateWin32SurfaceKHR(vkInstance, &vkWin32SurfaceCreateInfoKHR, NULL, &vkSurfaceKHR);
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "GetSupportedSurface(): vkCreateWin32SurfaceKHR()  function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "GetSupportedSurface(): vkCreateWin32SurfaceKHR() succedded\n");
	}
	
	return vkResult;
}

VkResult GetPhysicalDevice(void)
{
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;
	
	/*
	2. Call vkEnumeratePhysicalDevices() to get Physical device count
	*/
	vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, NULL); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkEnumeratePhysicalDevices.html (first call)
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() first call failed with error code %d\n", vkResult);
		return vkResult;
	}
	else if (physicalDeviceCount == 0)
	{
		fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() first call resulted in 0 physical devices\n");
		vkResult = VK_ERROR_INITIALIZATION_FAILED;
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() first call succedded\n");
	}
	
	/*
	3. Allocate VkPhysicalDeviceArray object according to above count
	*/
	vkPhysicalDevice_array = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
	//for sake of brevity no error checking
	
	/*
	4. Call vkEnumeratePhysicalDevices() again to fill above array
	*/
	vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, vkPhysicalDevice_array); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkEnumeratePhysicalDevices.html (seocnd call)
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() second call failed with error code %d\n", vkResult);
		vkResult = VK_ERROR_INITIALIZATION_FAILED; //return hardcoded failure
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "GetPhysicalDevice(): vkEnumeratePhysicalDevices() second call succedded\n");
	}
	
	/*
	5. Start a loop using physical device count and physical device, array obtained above (Note: declare a boolean bFound variable before this loop which will decide whether we found desired physical device or not)
	Inside this loop, 
	a. Declare a local variable to hold queque count
	b. Call vkGetPhysicalDeviceQuequeFamilyProperties() to initialize above queque count variable
	c. Allocate VkQuequeFamilyProperties array according to above count
	d. Call vkGetPhysicalDeviceQuequeFamilyProperties() again to fill above array
	e. Declare VkBool32 type array and allocate it using the same above queque count
	f. Start a nested loop and fill above VkBool32 type array by calling vkGetPhysicalDeviceSurfaceSupportKHR()
	g. Start another nested loop(not inside above loop , but nested in main loop) and check whether physical device
	   in its array with its queque family "has"(Sir told to underline) graphics bit or not. 
	   If yes then this is a selected physical device and assign it to global variable. 
	   Similarly this index is the selected queque family index and assign it to global variable too and set bFound to true
	   and break out from second nested loop
	h. Now we are back in main loop, so free queque family array and VkBool32 type array
	i. Still being in main loop, acording to bFound variable break out from main loop
	j. free physical device array 
	*/
	VkBool32 bFound = VK_FALSE; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkBool32.html
	for(uint32_t i = 0; i < physicalDeviceCount; i++)
	{
		/*
		a. Declare a local variable to hold queque count
		*/
		uint32_t quequeCount = UINT32_MAX;
		
		
		/*
		b. Call vkGetPhysicalDeviceQuequeFamilyProperties() to initialize above queque count variable
		*/
		//Strange call returns void
		//Error checking not done above as yacha VkResult nahi aahe
		//Kiti physical devices denar , jevde array madhe aahet tevda -> Second parameter
		//If physical device is present , then it must separate atleast one qurque family
		vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &quequeCount, NULL);//https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceQueueFamilyProperties.html
		
		/*
		c. Allocate VkQuequeFamilyProperties array according to above count
		*/
		struct VkQueueFamilyProperties *vkQueueFamilyProperties_array = NULL;//https://registry.khronos.org/vulkan/specs/latest/man/html/VkQueueFamilyProperties.html
		vkQueueFamilyProperties_array = (struct VkQueueFamilyProperties*) malloc(sizeof(struct VkQueueFamilyProperties) * quequeCount);
		//for sake of brevity no error checking
		
		/*
		d. Call vkGetPhysicalDeviceQuequeFamilyProperties() again to fill above array
		*/
		vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &quequeCount, vkQueueFamilyProperties_array);//https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceQueueFamilyProperties.html
		
		/*
		e. Declare VkBool32 type array and allocate it using the same above queque count
		*/
		VkBool32 *isQuequeSurfaceSupported_array = NULL;
		isQuequeSurfaceSupported_array = (VkBool32*) malloc(sizeof(VkBool32) * quequeCount);
		//for sake of brevity no error checking
		
		/*
		f. Start a nested loop and fill above VkBool32 type array by calling vkGetPhysicalDeviceSurfaceSupportKHR()
		*/
		for(uint32_t j =0; j < quequeCount ; j++)
		{
			//vkGetPhysicalDeviceSurfaceSupportKHR ->Supported surface la tumhi dilela surface support karto ka?
			//vkPhysicalDevice_array[i] -> ya device cha
			//j -> ha index
			//vkSurfaceKHR -> ha surface
			//isQuequeSurfaceSupported_array-> support karto ki nahi bhar
			vkResult = vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice_array[i], j, vkSurfaceKHR, &isQuequeSurfaceSupported_array[j]); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceSurfaceSupportKHR.html
		}
		
		/*
		g. Start another nested loop(not inside above loop , but nested in main loop) and check whether physical device
		   in its array with its queque family "has"(Sir told to underline) graphics bit or not. 
		   If yes then this is a selected physical device and assign it to global variable. 
		   Similarly this index is the selected queque family index and assign it to global variable too and set bFound to true
		   and break out from second nested loop
		*/
		for(uint32_t j =0; j < quequeCount ; j++)
		{
			//https://registry.khronos.org/vulkan/specs/latest/man/html/VkQueueFamilyProperties.html
			//https://registry.khronos.org/vulkan/specs/latest/man/html/VkQueueFlagBits.html
			if(vkQueueFamilyProperties_array[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				//select ith graphic card, queque familt at j, bFound la TRUE karun break vha
				if(isQuequeSurfaceSupported_array[j] == VK_TRUE)
				{
					vkPhysicalDevice_selected = vkPhysicalDevice_array[i];
					graphicsQuequeFamilyIndex_selected = j;
					bFound = VK_TRUE;
					break;
				}
			}
		}
		
		/*
		h. Now we are back in main loop, so free queque family array and VkBool32 type array
		*/
		if(isQuequeSurfaceSupported_array)
		{
			free(isQuequeSurfaceSupported_array);
			isQuequeSurfaceSupported_array = NULL;
			fprintf(gFILE, "GetPhysicalDevice(): succedded to free isQuequeSurfaceSupported_array\n");
		}
		
		
		if(vkQueueFamilyProperties_array)
		{
			free(vkQueueFamilyProperties_array);
			vkQueueFamilyProperties_array = NULL;
			fprintf(gFILE, "GetPhysicalDevice(): succedded to free vkQueueFamilyProperties_array\n");
		}
		
		/*
		i. Still being in main loop, acording to bFound variable break out from main loop
		*/
		if(bFound == VK_TRUE)
		{
			break;
		}
	}
	
	/*
	6. Do error checking according to value of bFound
	*/
	if(bFound == VK_TRUE)
	{
		fprintf(gFILE, "GetPhysicalDevice(): GetPhysicalDevice() suceeded to select required physical device with graphics enabled\n");
		
		/*
		PrintVulkanInfo() changes
		2. Accordingly remove physicaldevicearray freeing block from if(bFound == VK_TRUE) block and we will later write this freeing block in printVkInfo().
		*/
		
		/*
		//j. free physical device array 
		if(vkPhysicalDevice_array)
		{
			free(vkPhysicalDevice_array);
			vkPhysicalDevice_array = NULL;
			fprintf(gFILE, "GetPhysicalDevice(): succedded to free vkPhysicalDevice_array\n");
		}
		*/
	}
	else
	{
		fprintf(gFILE, "GetPhysicalDevice(): GetPhysicalDevice() failed to obtain graphics supported physical device\n");
		
		/*
		j. free physical device array 
		*/
		if(vkPhysicalDevice_array)
		{
			free(vkPhysicalDevice_array);
			vkPhysicalDevice_array = NULL;
			fprintf(gFILE, "GetPhysicalDevice(): succedded to free vkPhysicalDevice_array\n");
		}
		
		vkResult = VK_ERROR_INITIALIZATION_FAILED;
		return vkResult;
	}
	
	/*
	7. memset the global physical device memory property structure
	*/
	memset((void*)&vkPhysicalDeviceMemoryProperties, 0, sizeof(struct VkPhysicalDeviceMemoryProperties)); //https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDeviceMemoryProperties.html
	
	/*
	8. initialize above structure by using vkGetPhysicalDeviceMemoryProperties() //https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceMemoryProperties.html
	No need of error checking as we already have physical device
	*/
	vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_selected, &vkPhysicalDeviceMemoryProperties);
	
	/*
	9. Declare a local structure variable VkPhysicalDeviceFeatures, memset it  and initialize it by calling vkGetPhysicalDeviceFeatures() 
	// https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDeviceFeatures.html
	// //https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceFeatures.html
	*/
	VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures;
	memset((void*)&vkPhysicalDeviceFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
	vkGetPhysicalDeviceFeatures(vkPhysicalDevice_selected, &vkPhysicalDeviceFeatures);
	
	/*
	10. By using "tescellation shader" member of above structure check selected device's tescellation shader support
	11. By using "geometry shader" member of above structure check selected device's geometry shader support
	*/
	if(vkPhysicalDeviceFeatures.tessellationShader)
	{
		fprintf(gFILE, "GetPhysicalDevice(): Supported physical device supports tessellation shader\n");
	}
	else
	{
		fprintf(gFILE, "GetPhysicalDevice(): Supported physical device does not support tessellation shader\n");
	}
	
	if(vkPhysicalDeviceFeatures.geometryShader)
	{
		fprintf(gFILE, "GetPhysicalDevice(): Supported physical device supports geometry shader\n");
	}
	else
	{
		fprintf(gFILE, "GetPhysicalDevice(): Supported physical device does not support geometry shader\n");
	}
	
	/*
	12. There is no need to free/uninitialize/destroy selected physical device?
	Bcoz later we will create Vulkan logical device which need to be destroyed and its destruction will automatically destroy selected physical device.
	*/
	
	return vkResult;
}

/*
PrintVkInfo() changes
3. Write printVkInfo() user defined function with following steps
3a. Start a loop using global physical device count and inside it declare  and memset VkPhysicalDeviceProperties struct variable (https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDeviceProperties.html).
3b. Initialize this struct variable by calling vkGetPhysicalDeviceProperties() (https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceProperties.html) vulkan api.
3c. Print Vulkan API version using apiVersion member of above struct.
	This requires 3 Vulkan macros.
3d. Print device name by using "deviceName" member of above struct.
3e. Use "deviceType" member of above struct in a switch case block and accordingly print device type.
3f. Print hexadecimal Vendor Id of device using "vendorId" member of above struct.
3g. Print hexadecimal deviceID of device using "deviceId" member of struct.
Note*: For sake of completeness, we can repeat a to h points from GetPhysicalDevice() {05-GetPhysicalDevice notes},
but now instead of assigning selected queque and selected device, print whether this device supports graphic bit, compute bit, transfer bit using if else if else if blocks
Similarly we also can repeat device features from GetPhysicalDevice() and can print all around 50 plus device features including support to tescellation shader and geometry shader.
3h. Free physicaldevice array here which we removed from if(bFound == VK_TRUE) block of GetPhysicalDevice().
*/
VkResult PrintVulkanInfo(void)
{
	VkResult vkResult = VK_SUCCESS;
	
	//Code
	fprintf(gFILE, "************************* Shree Ganesha******************************\n");
	
	/*
	PrintVkInfo() changes
	3a. Start a loop using global physical device count and inside it declare  and memset VkPhysicalDeviceProperties struct variable
	*/
	for(uint32_t i = 0; i < physicalDeviceCount; i++)
	{
		/*
		PrintVkInfo() changes
		3b. Initialize this struct variable by calling vkGetPhysicalDeviceProperties()
		*/
		VkPhysicalDeviceProperties vkPhysicalDeviceProperties; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDeviceProperties.html
		memset((void*)&vkPhysicalDeviceProperties, 0, sizeof(VkPhysicalDeviceProperties));
		vkGetPhysicalDeviceProperties(vkPhysicalDevice_array[i], &vkPhysicalDeviceProperties ); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceProperties.html
		
		/*
		PrintVkInfo() changes
		3c. Print Vulkan API version using apiVersion member of above struct.
		This requires 3 Vulkan macros.
		*/
		//uint32_t majorVersion,minorVersion,patchVersion;
		//https://registry.khronos.org/vulkan/specs/latest/man/html/VK_VERSION_MAJOR.html -> api deprecation for which we changed to VK_API_VERSION_XXXXX
		uint32_t majorVersion = VK_API_VERSION_MAJOR(vkPhysicalDeviceProperties.apiVersion); //https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDeviceProperties.html
		uint32_t minorVersion = VK_API_VERSION_MINOR(vkPhysicalDeviceProperties.apiVersion);
		uint32_t patchVersion = VK_API_VERSION_PATCH(vkPhysicalDeviceProperties.apiVersion);
		
		//API Version
		fprintf(gFILE,"apiVersion = %d.%d.%d\n", majorVersion, minorVersion, patchVersion);
		
		/*
		PrintVkInfo() changes
		3d. Print device name by using "deviceName" member of above struct.
		*/
		fprintf(gFILE,"deviceName = %s\n", vkPhysicalDeviceProperties.deviceName);
		
		/*
		PrintVkInfo() changes
		3e. Use "deviceType" member of above struct in a switch case block and accordingly print device type.
		*/
		switch(vkPhysicalDeviceProperties.deviceType)
		{
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
				fprintf(gFILE,"deviceType = Integrated GPU (iGPU)\n");
			break;
			
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
				fprintf(gFILE,"deviceType = Discrete GPU (dGPU)\n");
			break;
			
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				fprintf(gFILE,"deviceType = Virtual GPU (vGPU)\n");
			break;
			
			case VK_PHYSICAL_DEVICE_TYPE_CPU:
				fprintf(gFILE,"deviceType = CPU\n");
			break;
			
			case VK_PHYSICAL_DEVICE_TYPE_OTHER:
				fprintf(gFILE,"deviceType = Other\n");
			break;
			
			default:
				fprintf(gFILE, "deviceType = UNKNOWN\n");
			break;
		}
		
		/*
		PrintVkInfo() changes
		3f. Print hexadecimal Vendor Id of device using "vendorId" member of above struct.
		*/
		fprintf(gFILE,"vendorID = 0x%04x\n", vkPhysicalDeviceProperties.vendorID);
		
		/*
		PrintVkInfo() changes
		3g. Print hexadecimal deviceID of device using "deviceId" member of struct.
		*/
		fprintf(gFILE,"deviceID = 0x%04x\n", vkPhysicalDeviceProperties.deviceID);
	}
	
	/*
	PrintVkInfo() changes
	3h. Free physicaldevice array here which we removed from if(bFound == VK_TRUE) block of GetPhysicalDevice().
	*/
	if(vkPhysicalDevice_array)
	{
		free(vkPhysicalDevice_array);
		vkPhysicalDevice_array = NULL;
		fprintf(gFILE, "PrintVkInfo(): succedded to free vkPhysicalDevice_array\n");
	}
	
	return vkResult;
}

VkResult FillDeviceExtensionNames(void)
{
	// Code
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;

	/*
	1. Find how many device extensions are supported by Vulkan driver of/for this version and keept the count in a local variable.
	*/
	uint32_t deviceExtensionCount = 0;

	//https://registry.khronos.org/vulkan/specs/latest/man/html/vkEnumerateDeviceExtensionProperties.html
	vkResult = vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected, NULL, &deviceExtensionCount, NULL );
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "FillDeviceExtensionNames(): First call to vkEnumerateDeviceExtensionProperties()  function failed\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "FillDeviceExtensionNames(): First call to vkEnumerateDeviceExtensionProperties() succedded and returned %u count\n", deviceExtensionCount);
	}

	/*
	 Allocate and fill struct VkExtensionProperties 
	 (https://registry.khronos.org/vulkan/specs/latest/man/html/VkExtensionProperties.html) structure array, 
	 corresponding to above count
	*/
	VkExtensionProperties* vkExtensionProperties_array = NULL;
	vkExtensionProperties_array = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * deviceExtensionCount);
	if (vkExtensionProperties_array != NULL)
	{
		//Add log here later for failure
		//exit(-1);
	}
	else
	{
		//Add log here later for success
	}

	vkResult = vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected, NULL, &deviceExtensionCount, vkExtensionProperties_array);
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "FillDeviceExtensionNames(): Second call to vkEnumerateDeviceExtensionProperties()  function failed\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "FillDeviceExtensionNames(): Second call to vkEnumerateDeviceExtensionProperties() succedded\n");
	}

	/*
	Fill and display a local string array of extension names obtained from VkExtensionProperties structure array
	*/
	char** deviceExtensionNames_array = NULL;
	deviceExtensionNames_array = (char**)malloc(sizeof(char*) * deviceExtensionCount);
	if (deviceExtensionNames_array != NULL)
	{
		//Add log here later for failure
		//exit(-1);
	}
	else
	{
		//Add log here later for success
	}

	for (uint32_t i =0; i < deviceExtensionCount; i++)
	{
		//https://registry.khronos.org/vulkan/specs/latest/man/html/VkExtensionProperties.html
		deviceExtensionNames_array[i] = (char*)malloc( sizeof(char) * (strlen(vkExtensionProperties_array[i].extensionName) + 1));
		memcpy(deviceExtensionNames_array[i], vkExtensionProperties_array[i].extensionName, (strlen(vkExtensionProperties_array[i].extensionName) + 1));
		fprintf(gFILE, "FillDeviceExtensionNames(): Vulkan Device Extension Name = %s\n", deviceExtensionNames_array[i]);
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
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
	Accordingly set two global variables, "required extension count" and "required extension names array"
	*/
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkBool32.html -> Vulkan cha bool
	VkBool32 vulkanSwapchainExtensionFound = VK_FALSE;
	for (uint32_t i = 0; i < deviceExtensionCount; i++)
	{
		if (strcmp(deviceExtensionNames_array[i], VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
		{
			vulkanSwapchainExtensionFound = VK_TRUE;
			enabledDeviceExtensionNames_array[enabledDeviceExtensionsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		}
	}

	/*
	As not needed hence forth , free local string array
	*/
	for (uint32_t i =0 ; i < deviceExtensionCount; i++)
	{
		free(deviceExtensionNames_array[i]);
	}
	free(deviceExtensionNames_array);

	/*
	Print whether our required device extension names or not (He log madhe yenar. Jithe print asel sarv log madhe yenar)
	*/
	if (vulkanSwapchainExtensionFound == VK_FALSE)
	{
		//Type mismatch in return VkResult and VKBool32, so return hardcoded failure
		vkResult = VK_ERROR_INITIALIZATION_FAILED; //return hardcoded failure
		fprintf(gFILE, "FillDeviceExtensionNames(): VK_KHR_SWAPCHAIN_EXTENSION_NAME not found\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "FillDeviceExtensionNames(): VK_KHR_SWAPCHAIN_EXTENSION_NAME is found\n");
	}

	/*
	Print only enabled device extension names
	*/
	for (uint32_t i = 0; i < enabledDeviceExtensionsCount; i++)
	{
		fprintf(gFILE, "FillDeviceExtensionNames(): Enabled Vulkan Device Extension Name = %s\n", enabledDeviceExtensionNames_array[i]);
	}

	return vkResult;
}

VkResult CreateVulKanDevice(void)
{
	//function declaration
	VkResult FillDeviceExtensionNames(void);
	
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;
	
	/*
	fill device extensions
	2. Call previously created FillDeviceExtensionNames() in it.
	*/
	vkResult = FillDeviceExtensionNames();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateVulKanDevice(): FillDeviceExtensionNames()  function failed\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateVulKanDevice(): FillDeviceExtensionNames() succedded\n");
	}
	
	/*
	Newly added code
	*/
	//float queuePriorities[1]  = {1.0};
	float queuePriorities[1];
	queuePriorities[0] = 1.0f;
	VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkDeviceQueueCreateInfo.html
	memset(&vkDeviceQueueCreateInfo, 0, sizeof(VkDeviceQueueCreateInfo));
	
	vkDeviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	vkDeviceQueueCreateInfo.pNext = NULL;
	vkDeviceQueueCreateInfo.flags = 0;
	vkDeviceQueueCreateInfo.queueFamilyIndex = graphicsQuequeFamilyIndex_selected;
	vkDeviceQueueCreateInfo.queueCount = 1;
	vkDeviceQueueCreateInfo.pQueuePriorities = queuePriorities;
	
	/*
	3. Declare and initialize VkDeviceCreateInfo structure (https://registry.khronos.org/vulkan/specs/latest/man/html/VkDeviceCreateInfo.html).
	*/
	VkDeviceCreateInfo vkDeviceCreateInfo;
	memset(&vkDeviceCreateInfo, 0, sizeof(VkDeviceCreateInfo));
	
	/*
	4. Use previously obtained device extension count and device extension array to initialize this structure.
	*/
	vkDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	vkDeviceCreateInfo.pNext = NULL;
	vkDeviceCreateInfo.flags = 0;
	vkDeviceCreateInfo.enabledExtensionCount = enabledDeviceExtensionsCount;
	vkDeviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensionNames_array;
	vkDeviceCreateInfo.enabledLayerCount = 0;
	vkDeviceCreateInfo.ppEnabledLayerNames = NULL;
	vkDeviceCreateInfo.pEnabledFeatures = NULL;
	vkDeviceCreateInfo.queueCreateInfoCount = 1;
	vkDeviceCreateInfo.pQueueCreateInfos = &vkDeviceQueueCreateInfo;
	
	/*
	5. Now call vkCreateDevice to create actual Vulkan device and do error checking.
	*/
	vkResult = vkCreateDevice(vkPhysicalDevice_selected, &vkDeviceCreateInfo, NULL, &vkDevice); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateDevice.html
	if(vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateVulKanDevice(): vkCreateDevice()  function failed\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateVulKanDevice(): vkCreateDevice() succedded\n");
	}
	
	return vkResult;
}

void GetDeviceQueque(void)
{
	//Code
	vkGetDeviceQueue(vkDevice, graphicsQuequeFamilyIndex_selected, 0, &vkQueue); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetDeviceQueue.html
	if(vkQueue == VK_NULL_HANDLE)
	{
		fprintf(gFILE, "GetDeviceQueque(): vkGetDeviceQueue() returned NULL for vkQueue\n");
		return;
	}
	else
	{
		fprintf(gFILE, "GetDeviceQueque(): vkGetDeviceQueue() succedded\n");
	}
}

VkResult getPhysicalDeviceSurfaceFormatAndColorSpace(void)
{
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;
	
	//Code
	//Get count of supported surface color formats
	uint32_t FormatCount = 0;
	
	//https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceSurfaceFormatsKHR.html
	vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_selected, vkSurfaceKHR, &FormatCount, NULL);
	if(vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): First call to vkGetPhysicalDeviceSurfaceFormatsKHR() failed\n");
		return vkResult;
	}
	else if(FormatCount == 0)
	{
		vkResult = VK_ERROR_INITIALIZATION_FAILED; //return hardcoded failure
		fprintf(gFILE, "vkGetPhysicalDeviceSurfaceFormatsKHR():: First call to vkGetPhysicalDeviceSurfaceFormatsKHR() returned FormatCount as 0\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): First call to vkGetPhysicalDeviceSurfaceFormatsKHR() succedded\n");
	}
	
	//Declare and allocate VkSurfaceKHR array
	VkSurfaceFormatKHR *vkSurfaceFormatKHR_array = (VkSurfaceFormatKHR*)malloc(FormatCount * sizeof(VkSurfaceFormatKHR)); //https://registry.khronos.org/vulkan/specs/latest/man/html/VkSurfaceFormatKHR.html
	//For sake of brevity  no error checking
	
	//Filling the array
	vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_selected, vkSurfaceKHR, &FormatCount, vkSurfaceFormatKHR_array); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceSurfaceFormatsKHR.html
	if(vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): Second call to vkGetPhysicalDeviceSurfaceFormatsKHR()  function failed\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace():  Second call to vkGetPhysicalDeviceSurfaceFormatsKHR() succedded\n");
	}
	
	//According to contents of array , we have to decide surface format and color space
	//Decide surface format first
	if( (1 == FormatCount) && (vkSurfaceFormatKHR_array[0].format == VK_FORMAT_UNDEFINED) )
	{
		vkFormat_color = VK_FORMAT_B8G8R8A8_UNORM; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkFormat.html
	}
	else 
	{
		vkFormat_color = vkSurfaceFormatKHR_array[0].format; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkFormat.html
	}
	
	//Decide color space second
	vkColorSpaceKHR = vkSurfaceFormatKHR_array[0].colorSpace;
	
	//free the array
	if(vkSurfaceFormatKHR_array)
	{
		fprintf(gFILE, "getPhysicalDeviceSurfaceFormatAndColorSpace(): vkSurfaceFormatKHR_array is freed\n");
		free(vkSurfaceFormatKHR_array);
		vkSurfaceFormatKHR_array = NULL;
	}
	
	return vkResult;
}

VkResult getPhysicalDevicePresentMode(void)
{
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;
	
	//Code
	//mailbox bhetel aata , fifo milel android la kadachit
	uint32_t presentModeCount = 0;
	
	//https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceSurfacePresentModesKHR.html
	vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_selected, vkSurfaceKHR, &presentModeCount, NULL);
	if(vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "getPhysicalDevicePresentMode(): First call to vkGetPhysicalDeviceSurfaceFormatsKHR() failed\n");
		return vkResult;
	}
	else if(presentModeCount == 0)
	{
		vkResult = VK_ERROR_INITIALIZATION_FAILED; //return hardcoded failure
		fprintf(gFILE, "getPhysicalDevicePresentMode():: First call to vkGetPhysicalDeviceSurfaceFormatsKHR() returned presentModeCount as 0\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "getPhysicalDevicePresentMode(): First call to vkGetPhysicalDeviceSurfaceFormatsKHR() succedded\n");
	}
	
	//Declare and allocate VkPresentModeKHR array
	VkPresentModeKHR  *vkPresentModeKHR_array = (VkPresentModeKHR*)malloc(presentModeCount * sizeof(VkPresentModeKHR)); //https://registry.khronos.org/vulkan/specs/latest/man/html/VkPresentModeKHR.html
	//For sake of brevity  no error checking
	
	//Filling the array
	vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_selected, vkSurfaceKHR, &presentModeCount, vkPresentModeKHR_array); //https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceSurfaceFormatsKHR.html
	if(vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "getPhysicalDevicePresentMode(): Second call to vkGetPhysicalDeviceSurfacePresentModesKHR()  function failed\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "getPhysicalDevicePresentMode():  Second call to vkGetPhysicalDeviceSurfacePresentModesKHR() succedded\n");
	}
	
	//According to contents of array , we have to decide presentation mode
	for(uint32_t i=0; i < presentModeCount; i++)
	{
		if(vkPresentModeKHR_array[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkPresentModeKHR.html
			break;
		}
	}
	
	if(vkPresentModeKHR != VK_PRESENT_MODE_MAILBOX_KHR)
	{
		vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkPresentModeKHR.html
	}
	
	fprintf(gFILE, "getPhysicalDevicePresentMode(): vkPresentModeKHR is %d\n", vkPresentModeKHR);
	
	//free the array
	if(vkPresentModeKHR_array)
	{
		fprintf(gFILE, "getPhysicalDevicePresentMode(): vkPresentModeKHR_array is freed\n");
		free(vkPresentModeKHR_array);
		vkPresentModeKHR_array = NULL;
	}
	
	return vkResult;
}

VkResult CreateSwapChain(VkBool32 vsync)
{
	/*
	Function Declaration
	*/
	VkResult getPhysicalDeviceSurfaceFormatAndColorSpace(void);
	VkResult getPhysicalDevicePresentMode(void);
	
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;
	
	/*
	Code
	*/
	
	/*
	Surface Format and Color Space
	1. Get Physical Device Surface supported color format and physical device surface supported color space , using Step 10.
	*/
	vkResult = getPhysicalDeviceSurfaceFormatAndColorSpace();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateSwapChain(): getPhysicalDeviceSurfaceFormatAndColorSpace() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateSwapChain(): getPhysicalDeviceSurfaceFormatAndColorSpace() succedded\n");
	}
	
	/*
	2. Get Physical Device Surface capabilities by using Vulkan API vkGetPhysicalDeviceSurfaceCapabilitiesKHR (https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceSurfaceCapabilitiesKHR.html)
    and accordingly initialize VkSurfaceCapabilitiesKHR structure (https://registry.khronos.org/vulkan/specs/latest/man/html/VkSurfaceCapabilitiesKHR.html).
	*/
	VkSurfaceCapabilitiesKHR vkSurfaceCapabilitiesKHR;
	memset((void*)&vkSurfaceCapabilitiesKHR, 0, sizeof(VkSurfaceCapabilitiesKHR));
	vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_selected, vkSurfaceKHR, &vkSurfaceCapabilitiesKHR);
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateSwapChain(): vkGetPhysicalDeviceSurfaceCapabilitiesKHR() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateSwapChain(): vkGetPhysicalDeviceSurfaceCapabilitiesKHR() succedded\n");
	}
	
	/*
	3. By using minImageCount and maxImageCount members of above structure , decide desired ImageCount for swapchain.
	*/
	uint32_t testingNumerOfSwapChainImages = vkSurfaceCapabilitiesKHR.minImageCount + 1;
	uint32_t desiredNumerOfSwapChainImages = 0; //To find this
	if( (vkSurfaceCapabilitiesKHR.maxImageCount > 0) && (vkSurfaceCapabilitiesKHR.maxImageCount < testingNumerOfSwapChainImages) )
	{
		desiredNumerOfSwapChainImages = vkSurfaceCapabilitiesKHR.maxImageCount;
	}
	else
	{
		desiredNumerOfSwapChainImages = vkSurfaceCapabilitiesKHR.minImageCount;
	}
		
	/*
	4. By using currentExtent.width and currentExtent.height members of above structure and comparing them with current width and height of window, decide image width and image height of swapchain.
	Choose size of swapchain image
	*/
	memset((void*)&vkExtent2D_SwapChain, 0 , sizeof(VkExtent2D));
	if(vkSurfaceCapabilitiesKHR.currentExtent.width != UINT32_MAX)
	{
		vkExtent2D_SwapChain.width = vkSurfaceCapabilitiesKHR.currentExtent.width;
		vkExtent2D_SwapChain.height = vkSurfaceCapabilitiesKHR.currentExtent.height;
		fprintf(gFILE, "CreateSwapChain(): Swapchain Image Width x SwapChain  Image Height = %d X %d\n", vkExtent2D_SwapChain.width, vkExtent2D_SwapChain.height);
	}
	else
	{
		vkExtent2D_SwapChain.width = vkSurfaceCapabilitiesKHR.currentExtent.width;
		vkExtent2D_SwapChain.height = vkSurfaceCapabilitiesKHR.currentExtent.height;
		fprintf(gFILE, "CreateSwapChain(): Swapchain Image Width x SwapChain  Image Height = %d X %d\n", vkExtent2D_SwapChain.width, vkExtent2D_SwapChain.height);
	
		/*
		If surface size is already defined, then swapchain image size must match with it.
		*/
		VkExtent2D vkExtent2D;
		memset((void*)&vkExtent2D, 0, sizeof(VkExtent2D));
		vkExtent2D.width = (uint32_t)winWidth;
		vkExtent2D.height = (uint32_t)winHeight;
		
		vkExtent2D_SwapChain.width = max(vkSurfaceCapabilitiesKHR.minImageExtent.width, min(vkSurfaceCapabilitiesKHR.maxImageExtent.width, vkExtent2D.width));
		vkExtent2D_SwapChain.height = max(vkSurfaceCapabilitiesKHR.minImageExtent.height, min(vkSurfaceCapabilitiesKHR.maxImageExtent.height, vkExtent2D.height));
		fprintf(gFILE, "CreateSwapChain(): Swapchain Image Width x SwapChain  Image Height = %d X %d\n", vkExtent2D_SwapChain.width, vkExtent2D_SwapChain.height);
	}
	
	/*
	5. Decide how we are going to use swapchain images, means whether we we are going to store image data and use it later (Deferred Rendering) or we are going to use it immediatly as color attachment.
	Set Swapchain image usage flag
	Image usage flag hi concept aahe
	*/
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkImageUsageFlagBits.html
	VkImageUsageFlagBits vkImageUsageFlagBits = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT -> Imp, VK_IMAGE_USAGE_TRANSFER_SRC_BIT->Optional
	/*
	Although VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not usefule here for triangle application.
	It is useful for texture, fbo, compute shader
	*/
	
	
	/*
	6. Swapchain  is capable of storing transformed image before presentation, which is called as PreTransform. 
    While creating swapchain , we can decide whether to pretransform or not the swapchain images. (Pre transform also includes flipping of image)
   
    Whether to consider pretransform/flipping or not?
	*/
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkSurfaceTransformFlagBitsKHR.html
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkSurfaceCapabilitiesKHR.html
	VkSurfaceTransformFlagBitsKHR vkSurfaceTransformFlagBitsKHR;
	if(vkSurfaceCapabilitiesKHR.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		vkSurfaceTransformFlagBitsKHR = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		vkSurfaceTransformFlagBitsKHR = vkSurfaceCapabilitiesKHR.currentTransform;
	}
	
	/*
	Presentation Mode
	7. Get Presentation mode for swapchain images using Step 11.
	*/
	vkResult = getPhysicalDevicePresentMode();
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateSwapChain(): getPhysicalDevicePresentMode() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateSwapChain(): getPhysicalDevicePresentMode() succedded\n");
	}
	
	/*
	8. According to above data, declare ,memset and initialize VkSwapchainCreateInfoKHR  structure (https://registry.khronos.org/vulkan/specs/latest/man/html/VkSwapchainCreateInfoKHR.html)
	bas aata structure bharaycha aahe
	*/
	struct VkSwapchainCreateInfoKHR vkSwapchainCreateInfoKHR;
	memset((void*)&vkSwapchainCreateInfoKHR, 0, sizeof(struct VkSwapchainCreateInfoKHR));
	vkSwapchainCreateInfoKHR.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	vkSwapchainCreateInfoKHR.pNext = NULL;
	vkSwapchainCreateInfoKHR.flags = 0;
	vkSwapchainCreateInfoKHR.surface = vkSurfaceKHR;
	vkSwapchainCreateInfoKHR.minImageCount = desiredNumerOfSwapChainImages;
	vkSwapchainCreateInfoKHR.imageFormat = vkFormat_color;
	vkSwapchainCreateInfoKHR.imageColorSpace = vkColorSpaceKHR;
	vkSwapchainCreateInfoKHR.imageExtent.width = vkExtent2D_SwapChain.width;
	vkSwapchainCreateInfoKHR.imageExtent.height = vkExtent2D_SwapChain.height;
	vkSwapchainCreateInfoKHR.imageUsage = vkImageUsageFlagBits;
	vkSwapchainCreateInfoKHR.preTransform = vkSurfaceTransformFlagBitsKHR;
	vkSwapchainCreateInfoKHR.imageArrayLayers = 1; //concept
	vkSwapchainCreateInfoKHR.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkSharingMode.html
	vkSwapchainCreateInfoKHR.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkCompositeAlphaFlagBitsKHR.html
	vkSwapchainCreateInfoKHR.presentMode = vkPresentModeKHR;
	vkSwapchainCreateInfoKHR.clipped = VK_TRUE;
	//vkSwapchainCreateInfoKHR.oldSwapchain is of no use in this application. Will be used in resize.
	
	/*
	9. At the end , call vkCreateSwapchainKHR() (https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateSwapchainKHR.html) Vulkan API to create the swapchain
	*/
	vkResult = vkCreateSwapchainKHR(vkDevice, &vkSwapchainCreateInfoKHR, NULL, &vkSwapchainKHR);
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateSwapChain(): vkCreateSwapchainKHR() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateSwapChain(): vkCreateSwapchainKHR() succedded\n");
	}
	
	return vkResult;
}

VkResult CreateImagesAndImageViews(void)
{
	//Variable declarations
	VkResult vkResult = VK_SUCCESS;
	
	//Code
	
	//1. Get Swapchain image count in a global variable using vkGetSwapchainImagesKHR() API (https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetSwapchainImagesKHR.html).
	vkResult = vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, NULL);
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateImagesAndImageViews(): first call to vkGetSwapchainImagesKHR() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else if(swapchainImageCount == 0)
	{
		vkResult = vkResult = VK_ERROR_INITIALIZATION_FAILED; //return hardcoded failure
		fprintf(gFILE, "CreateImagesAndImageViews(): first call to vkGetSwapchainImagesKHR() function returned swapchain Image Count as 0\n");
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateImagesAndImageViews(): first call to vkGetSwapchainImagesKHR() succedded with swapchainImageCount as %d\n", swapchainImageCount);
	}
	
	//2. Declare a global VkImage type array and allocate it to swapchain image count using malloc. (https://registry.khronos.org/vulkan/specs/latest/man/html/VkImage.html)
	// Allocate swapchain image array
	swapChainImage_array = (VkImage*)malloc(sizeof(VkImage) * swapchainImageCount);
	if(swapChainImage_array == NULL)
	{
			fprintf(gFILE, "CreateImagesAndImageViews(): swapChainImage_array is NULL. malloc() failed\n");
	}
	
	//3. Now call same function again which we called in Step 1 and fill this array.
	//Fill this array by swapchain images
	vkResult = vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapChainImage_array);
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateImagesAndImageViews(): second call to vkGetSwapchainImagesKHR() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateImagesAndImageViews(): second call to vkGetSwapchainImagesKHR() succedded with swapchainImageCount as %d\n", swapchainImageCount);
	}
	
	//4. Declare another global array of type VkImageView(https://registry.khronos.org/vulkan/specs/latest/man/html/VkImageView.html) and allocate it to sizeof Swapchain image count.
	// Allocate array of swapchain image view
	swapChainImageView_array = (VkImageView*)malloc(sizeof(VkImageView) * swapchainImageCount);
	if(swapChainImageView_array == NULL)
	{
			fprintf(gFILE, "CreateImagesAndImageViews(): swapChainImageView_array is NULL. malloc() failed\n");
	}
	
	//5. Declare  and initialize VkImageViewCreateInfo struct (https://registry.khronos.org/vulkan/specs/latest/man/html/VkImageViewCreateInfo.html) except its ".image" member.
	//Initialize VkImageViewCreateInfo struct
	VkImageViewCreateInfo vkImageViewCreateInfo;
	memset((void*)&vkImageViewCreateInfo, 0, sizeof(VkImageViewCreateInfo));
	
	/*
	typedef struct VkImageViewCreateInfo {
    VkStructureType            sType;
    const void*                pNext;
    VkImageViewCreateFlags     flags;
    VkImage                    image;
    VkImageViewType            viewType;
    VkFormat                   format;
    VkComponentMapping         components;
    VkImageSubresourceRange    subresourceRange;
	} VkImageViewCreateInfo;
	*/
	
	vkImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vkImageViewCreateInfo.pNext = NULL;
	vkImageViewCreateInfo.flags = 0;
	
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkFormat.html
	vkImageViewCreateInfo.format = vkFormat_color;
	
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkComponentMapping.html
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkComponentSwizzle.html
	vkImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	vkImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	vkImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	vkImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkImageSubresourceRange.html
	/*
	typedef struct VkImageSubresourceRange {
    VkImageAspectFlags    aspectMask;
    uint32_t              baseMipLevel;
    uint32_t              levelCount;
    uint32_t              baseArrayLayer;
    uint32_t              layerCount;
	} VkImageSubresourceRange;
	*/
	
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkImageAspectFlags.html
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkImageAspectFlagBits.html
	vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	vkImageViewCreateInfo.subresourceRange.levelCount = 0;
	vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	vkImageViewCreateInfo.subresourceRange.layerCount = 1;
	
	//https://registry.khronos.org/vulkan/specs/latest/man/html/VkImageViewType.html
	vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	
	
	//6. Now start a loop for swapchain image count and inside this loop, initialize above ".image" member to swapchain image array index we obtained above and then call vkCreateImage() to fill  above ImageView array.
	//Fill image view array using above struct
	for(uint32_t i = 0; i < swapchainImageCount; i++)
	{
		vkImageViewCreateInfo.image = swapChainImage_array[i];
		
		//https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateImageView.html
		vkResult = vkCreateImageView(vkDevice, &vkImageViewCreateInfo, NULL, &swapChainImageView_array[i]);
		if (vkResult != VK_SUCCESS)
		{
			fprintf(gFILE, "CreateImagesAndImageViews(): vkCreateImageView() function failed with error code %d at iteration %d\n", vkResult, i);
			return vkResult;
		}
		else
		{
			fprintf(gFILE, "CreateImagesAndImageViews(): vkCreateImageView() succedded for iteration %d\n", i);
		}
	}
	
	return vkResult;
}

VkResult CreateCommandPool()
{
	VkResult vkResult = VK_SUCCESS;
	
	/*
	1. Declare and initialize VkCreateCommandPoolCreateInfo structure.
	https://registry.khronos.org/vulkan/specs/latest/man/html/VkCommandPoolCreateInfo.html
	
	typedef struct VkCommandPoolCreateInfo {
    VkStructureType             sType;
    const void*                 pNext;
    VkCommandPoolCreateFlags    flags;
    uint32_t                    queueFamilyIndex;
	} VkCommandPoolCreateInfo;
	
	*/
	VkCommandPoolCreateInfo vkCommandPoolCreateInfo;
	memset((void*)&vkCommandPoolCreateInfo, 0, sizeof(VkCommandPoolCreateInfo));
	
	vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	vkCommandPoolCreateInfo.pNext = NULL;
	/*
	This flag states that Vulkan should create such command pools which will contain such command buffers capable of reset and restart.
	These command buffers are long lived.
	Other transient one{transfer one} is short lived.
	*/
	vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; //https://registry.khronos.org/vulkan/specs/latest/man/html/VkCommandPoolCreateFlagBits.html
	vkCommandPoolCreateInfo.queueFamilyIndex = graphicsQuequeFamilyIndex_selected;
	
	/*
	2. Call VkCreateCommandPool to create command pool.
	https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/vkCreateCommandPool.html
	*/
	vkResult = vkCreateCommandPool(vkDevice, &vkCommandPoolCreateInfo, NULL, &vkCommandPool);
	if (vkResult != VK_SUCCESS)
	{
		fprintf(gFILE, "CreateCommandPool(): vkCreateCommandPool() function failed with error code %d\n", vkResult);
		return vkResult;
	}
	else
	{
		fprintf(gFILE, "CreateCommandPool(): vkCreateCommandPool() succedded\n");
	}
	
	return vkResult;
}






