/*------------------------------------------------------------------
	$File: $
	$Date: $
	$Revision: $
	$Creator: James Martinez
	$Notice: This is ambitious
------------------------------------------------------------------*/

#include <windows.h>
#include <stdint.h>
#include <xinput.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;

struct win32_window_dimension
{
	int Width;
	int Height;
};


#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(X_Input_Get_State);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable X_Input_Get_State *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(X_Input_Set_State);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}

global_variable X_Input_Set_State *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void
Win32_Load_X_Input(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if(!XInputLibrary)
	{
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}
	
	if(XInputLibrary)
	{
		//we have to cast the GetProcAddress return value to the type we want
		XInputGetState = (X_Input_Get_State *)GetProcAddress(XInputLibrary, "XInputGetState");
		if(!XInputGetState){XInputGetState = XInputGetStateStub;}
		
		XInputSetState = (X_Input_Set_State *)GetProcAddress(XInputLibrary, "XInputSetState");
		if(!XInputSetState){XInputSetState = XInputSetStateStub;}
	}
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

internal void
RenderWeirdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	
	for(int Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		
		for(int X = 0; X < Buffer->Width; ++X)
		{
			
			uint8 Blue = (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);
		
			*Pixel++ = ((Green << 8) | Blue);
			
		}
		
		Row += Buffer->Pitch;

	}
	
}

internal void 
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if(Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}
	
	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;
	
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*Buffer->BytesPerPixel;
	
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	
	Buffer->Pitch = Width*Buffer->BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext, win32_offscreen_buffer *Buffer,
						int WindowWidth, int WindowHeight)
{
	//Correct aspect ratio
	StretchDIBits(DeviceContext,
				  0, 0, WindowWidth, WindowHeight, //Window size changes,
				  0, 0, Buffer->Width, Buffer->Height, //but the back buffer size does not 
				  Buffer->Memory,
				  &Buffer->Info,
	              DIB_RGB_COLORS, SRCCOPY);
				  
}

LRESULT CALLBACK 
Win32MainWindowCallback(HWND   Window,
						UINT   Message,
						WPARAM wParam,
						LPARAM lParam)

{
	LRESULT Result = 0;

	switch(Message)
	{
		case WM_DESTROY:
		{
			// NOTE: Handle this as an error - recreate window
			OutputDebugStringA("WM_DESTROY\n");
			
		} break;
		
		case WM_CLOSE:
		{
			// NOTE: Handle this with a message to user
			OutputDebugStringA("WM_CLOSE\n");
			GlobalRunning = false;
			
		} break;
		
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
			
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 VKCode = wParam;
			//Either we get 1 << 30 or 0, we want 1 if the former and 0 if the latter
			bool WasDown = ((lParam & (1 << 30)) != 0); 
			bool IsDown = 	((lParam & (1 << 31)) == 0);
			
			if(WasDown != IsDown)
			{
				
				if(VKCode == 'W')
				{
					
				}
				else if(VKCode == 'A')
				{
					
				}
				else if(VKCode == 'S')
				{
					
				}
				else if(VKCode == 'D')
				{
					
				}
				else if(VKCode == 'Q')
				{
					
				}
				else if(VKCode == 'E')
				{
					
				}
				else if(VKCode == VK_UP)
				{
					
				}
				else if(VKCode == VK_DOWN)
				{
					
				}
				else if(VKCode == VK_LEFT)
				{
					
				}
				else if(VKCode == VK_RIGHT)
				{
					
				}
				else if(VKCode == VK_ESCAPE)
				{
					OutputDebugStringA("ESCAPE: ");
					if(IsDown)
					{
						OutputDebugStringA("IsDown ");
					}
					if(WasDown)
					{
						OutputDebugStringA("WasDown");
					}OutputDebugStringA("\n");
				}
				else if(VKCode == VK_SPACE)
				{
					
				}
			}
	
			bool32 AltKeyWasDown = (lParam & (1 << 29));
	
			if((VKCode == VK_F4) && AltKeyWasDown)
			{
				GlobalRunning = false;
			}
			
		}break;
		
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			
			Win32DisplayBufferInWindow(DeviceContext, &GlobalBackbuffer,
									Dimension.Height, Dimension.Width);
									
			EndPaint(Window, &Paint);
			
		} break;
		
		default:
		{
			//OutputDebugStringA("default\n");
			
			Result = DefWindowProcA(Window, Message, wParam, lParam); 
		
		} break;
	}
	return(Result);
}

int CALLBACK 
WinMain(
  HINSTANCE Instance,
  HINSTANCE PrevInstance,
  LPSTR     CommandLine,
  int       ShowCode)
   
{
	Win32_Load_X_Input();
	
	WNDCLASS WindowClassObject = {};
	
	Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

	WindowClassObject.style = CS_HREDRAW|CS_VREDRAW; //repaint the whole window, not just the new section
	
	//POINTERS DO NOT UNIQUELY IDENTIY MEMORY, ONLY MEMORY IN ONE PROCESS
	WindowClassObject.lpfnWndProc = Win32MainWindowCallback;
	
	WindowClassObject.hInstance = Instance;
	//WindowClass.hIcon;
	
	WindowClassObject.lpszClassName = "HandmadeHeroWindowClass";
	
	if(RegisterClassA(&WindowClassObject))
	{
		//Pass window class so windows knows that our window is going to call Win32MainWindowCallback
		//every time it gets an event
		HWND Window = 
			CreateWindowExA(
				0,
				WindowClassObject.lpszClassName,
				"Handmade Hero",
				WS_OVERLAPPEDWINDOW|WS_VISIBLE,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				0,
				0,
				Instance,
				0);
		
		if(Window)
		{
			GlobalRunning = true;
			
			int XOffset = 0; int YOffset = 0;
			
			HDC DeviceContext = GetDC(Window);
			
			while(GlobalRunning)
			{
		
				MSG Message;
				
				while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
				{
					if(Message.message == WM_QUIT)
					{
						GlobalRunning = false;
					}
					
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}
				
				//should we poll this more frequently
				for(DWORD ControllerIndex = 0;	ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
				{
					XINPUT_STATE ControllerState;

					//when we call XInputGetState, if no controller is plugged in for an index, XINPUT will stall
					//for several millis. THIS IS REALLY BAD, WE ONLY WANT TO PULL THE CONNECTED CONTROLLERS
					if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						//controller is plugged in
						//see if controllerstate.dwpacketnumber increments too rapidly
						XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
						
						bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);         
						bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
						bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
						bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
					
						int16 StickX = Pad->sThumbLX;
						int16 StickY = Pad->sThumbLY;
						
						XOffset += StickX >> 12;
						YOffset += StickY >> 12;
						
					}
					else
					{
						//controller is not available
					}
				}
				
				RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);
				
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				
				Win32DisplayBufferInWindow(DeviceContext, &GlobalBackbuffer,
									Dimension.Width, Dimension.Height);
		
			}
		}
		else
		{
			//logging
		}
	}
	else
	{
		//logging
	}
	return(0);
}