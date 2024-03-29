/*------------------------------------------------------------------
	$File: $
	$Date: $
	$Revision: $
	$Creator: James Martinez
	$Notice: This is ambitious
------------------------------------------------------------------*/

/*
	This is not a final platform layer
	
	- Saved game locations
	- Getting a handle to our executable
	- Asset loading path
	- threading (launch a thread)
	- raw input (Support for multiple keyboards)
	- Sleep/timeBeginPeriod
	- ClipCursor() (for multimonitor support)
	- Fullscreen support
	- WM_SETCURSOR (control cursor visibility)
	- QueryCancelAutoplay
	- WM_ACTIVATEAPP (for when we are not the active app)
	- Blit speed improvements (BitBlt)
	- Hardware acceleration (OpenGL or Direct3D or BOTH)
	- GetKeyboardLayout (for french keyboards, international WASD support)
	
	Just a partial list of shit we need to do
*/

#include "Handmade.h"

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "my_win32_handmade.h"

global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

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

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
	if(Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
	debug_read_file_result Result = {};
	
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	
	if(FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		
		if(GetFileSizeEx(FileHandle, &FileSize))
		{
			//TODO: Defines for maximum values UInt32Max
			uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			
			if(Result.Contents)
			{
				DWORD BytesRead;
				
				if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
				{
					//NOTE: File read successfully
					Result.ContentsSize = FileSize32;
				}
				else
				{
					//TODO: Logging
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}
			else
			{
				//TODO: Logging
			}
		}
		
		CloseHandle(FileHandle);
		
	}
	
	return(Result);
	
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
	bool32 Result = false;
	
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	
	if(FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;

		if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			//NOTE: File read successfully
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			//TODO: Logging
		}
	
		CloseHandle(FileHandle);
	}	
	else
	{
		//TODO: Logging
	}
	
	return(Result);
	
}

inline FILETIME
Win32GetLastWriteTime(char *Filename)
{
	FILETIME LastWriteTime = {};
	
	WIN32_FIND_DATA FindData;
	HANDLE FindHandle = FindFirstFileA(Filename, &FindData);
	
	if(FindHandle != INVALID_HANDLE_VALUE)
	{
		LastWriteTime = FindData.ftLastWriteTime;
		FindClose(FindHandle);
	}
	
	return(LastWriteTime);
}

internal win32_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
	win32_game_code Result = {};
	
	//TODO: We need the proper path here
	//TODO: Automatic determination of when updates are necessary
	
	Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
	
	CopyFile(SourceDLLName, TempDLLName, FALSE);
	
	Result.GameCodeDLL = LoadLibraryA(TempDLLName);
	if(Result.GameCodeDLL)
	{
		Result.UpdateAndRender = 
		(
			(game_update_and_render *)GetProcAddress
			(
				Result.GameCodeDLL, "GameUpdateAndRender"
			)
		);
		
		Result.GetSoundSamples = 
		(
			(game_get_sound_samples *)GetProcAddress
			(
				Result.GameCodeDLL, "GameGetSoundSamples"
			)
		);
		
		Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
	}

	if(!Result.IsValid)
	{
		Result.UpdateAndRender = GameUpdateAndRenderStub;
		Result.GetSoundSamples = GameGetSoundSamplesStub;
	}
	
	return(Result);
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode)
{
	if(GameCode->GameCodeDLL)
	{
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}
	
	GameCode->IsValid = false;
	GameCode->UpdateAndRender = GameUpdateAndRenderStub;
	GameCode->GetSoundSamples = GameGetSoundSamplesStub;
}

internal void
Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	
	if(!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}
	
	if(!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}
	
	if(XInputLibrary)
	{
		//we have to cast the GetProcAddress return value to the type we want
		XInputGetState = (X_Input_Get_State *)GetProcAddress(XInputLibrary, "XInputGetState");
		if(!XInputGetState){XInputGetState = XInputGetStateStub;}
		
		XInputSetState = (X_Input_Set_State *)GetProcAddress(XInputLibrary, "XInputSetState");
		if(!XInputSetState){XInputSetState = XInputSetStateStub;}
		
		//diagnostic
	
	}
	else
	{
		
	}
}

internal void
Win32InitializeDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	//load the library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	
	if(DSoundLibrary)
	{
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)
			GetProcAddress(DSoundLibrary, "DirectSoundCreate");	
		
		LPDIRECTSOUND DirectSound;
		
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			//we're creating memory and writing int16 bit samples into it
			//LEFT RIGHT LEFT RIGHT LEFT RIGHT 
			//how big is [LEFT RIGHT] in bytes? 16+16/8 = 4
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample)/8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;
			
			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				//do we need DSBCAPS_GLOBALFOCUS?
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				
				//create primary buffer
				
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				
				if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						//we've set the format of the primary of buffer. if everything went right..
						OutputDebugStringA("Primary buffer set\n");
					}
					else
					{
						//MORE DIAGNOSTICS
					}
				}
				else
				{
					//MORE DIAGNOSTICS
				}
			}
			else
			{
				//MORE DIAGNOSTICS
			}
			
			// DSCBCAPS_GETCURRENTPOSITION2
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			
			if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
			{
				OutputDebugStringA("Secondary buffer created successfully\n");
			}
		
			//start playing
		}
		else
		{
			//diagnostic
		}
	}
	else
	{
		//diagnostic
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
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if(Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}
	
	Buffer->Width = Width;
	Buffer->Height = Height;
	
	int BytesPerPixel = 4;
	Buffer->BytesPerPixel = BytesPerPixel;
	
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Width*BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext, win32_offscreen_buffer *Buffer,
	int WindowWidth, int WindowHeight)
{
	//Correct aspect ratio
	StretchDIBits
	(
		DeviceContext, 0, 0, WindowWidth, WindowHeight, 
		0, 0, Buffer->Width, Buffer->Height, Buffer->Memory,
		&Buffer->Info, DIB_RGB_COLORS, SRCCOPY
	);
}

internal void
Win32ClearBuffer(win32_sound_output *SoundOutput)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
	&Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{
		int8 *DestSample = (int8 *)Region1;
		for(DWORD SampleIndex = 0; SampleIndex < Region1Size; ++SampleIndex)
		{
			*DestSample++ = 0;
		}
		DestSample = (int8 *)Region2;
		for(DWORD SampleIndex = 0; SampleIndex < Region2Size; ++SampleIndex)
		{
			*DestSample++ = 0;
		}
	}
	
	GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
	game_sound_output_buffer *SourceBuffer)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
	&Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{	

		DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		
		int16 *SourceSample = SourceBuffer->Samples;
		
		for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
		
		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		DestSample = (int16 *)Region2;
		
		for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
	
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	
	}
}

internal void 
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
	Assert(NewState->EndedDown != IsDown);
	
	NewState->EndedDown = IsDown;
	++NewState->HalfTransitionCount;
}

internal void 
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState,
	game_button_state *NewState, DWORD ButtonBit)
{
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
	real32 Result = 0;
	
	if(Value < -DeadZoneThreshold)
	{
		Result = (real32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
	}
	else if(Value > DeadZoneThreshold)
	{
		Result = (real32)((Value - DeadZoneThreshold) / (32767.0f + DeadZoneThreshold));
	}
	
	return(Result);
}

internal void
Win32BeginRecordingInput(win32_state *Win32State, int InputRecordingIndex)
{
	
	Win32State->InputRecordingIndex = InputRecordingIndex;
	
	char *FileName = "foo.hmi";
	
	Win32State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE, 
								0, 0, CREATE_ALWAYS, 0, 0);
	
	DWORD BytesToWrite = (DWORD)Win32State->TotalSize;
	Assert(Win32State->TotalSize == BytesToWrite);
	DWORD BytesWritten;
	WriteFile(Win32State->RecordingHandle, Win32State->GameMemoryBlock,
				BytesToWrite, &BytesWritten, 0);
}

internal void
Win32EndRecordingInput(win32_state *Win32State)
{
	CloseHandle(Win32State->RecordingHandle);
	Win32State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayback(win32_state *Win32State, int InputPlayingIndex)
{
	
	Win32State->InputPlayingIndex = InputPlayingIndex;
	
	char *FileName = "foo.hmi";
	
	Win32State->PlaybackHandle = CreateFileA(FileName, GENERIC_READ, 
		FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	
	DWORD BytesToRead = (DWORD)Win32State->TotalSize;
	Assert(Win32State->TotalSize == BytesToRead);
	DWORD BytesRead;
	ReadFile(Win32State->PlaybackHandle, Win32State->GameMemoryBlock, BytesToRead, &BytesRead, 0);
}

internal void
Win32EndInputPlayback(win32_state *Win32State)
{
	CloseHandle(Win32State->PlaybackHandle);
	Win32State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state *Win32State, game_input *NewInput)
{
	DWORD BytesWritten;
	WriteFile(Win32State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void
Win32PlaybackInput(win32_state *Win32State, game_input *NewInput)
{
	DWORD BytesRead = 0;
	
	if(ReadFile(Win32State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
	{
		if(BytesRead == 0)
		{
			//NOTE: End of stream, repeat loop
			int PlayingIndex = Win32State->InputPlayingIndex;
			Win32EndInputPlayback(Win32State);
			Win32BeginInputPlayback(Win32State, PlayingIndex);
		}
	}
}

internal void
Win32ProcessPendingMessages(win32_state *Win32State, game_controller_input *KeyboardController)
{
	MSG Message;
	
	while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch(Message.message)
		{
			
			case WM_QUIT:
			{
				GlobalRunning = false;
			} break;
			
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				uint32 VKCode = (uint32)Message.wParam;
				//Either we get 1 << 30 or 0, we want 1 if the former and 0 if the latter
				bool32 WasDown = ((Message.lParam & (1 << 30)) != 0); 
				bool32 IsDown = 	((Message.lParam & (1 << 31)) == 0);
				
				if(WasDown != IsDown)
				{
					
					if(VKCode == 'W')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
					}
					else if(VKCode == 'A')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
					}
					else if(VKCode == 'S')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
					}
					else if(VKCode == 'D')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
					}
					else if(VKCode == 'Q')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
					}
					else if(VKCode == 'E')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
					}
					else if(VKCode == VK_UP)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
					}
					else if(VKCode == VK_DOWN)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
					}
					else if(VKCode == VK_LEFT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
					}
					else if(VKCode == VK_RIGHT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
					}
					else if(VKCode == VK_ESCAPE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
					}
					else if(VKCode == VK_SPACE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
					}
#if HANDMADE_INTERNAL
					else if(VKCode == 'P')
					{
						if(IsDown)
						{
							GlobalPause = !GlobalPause;
						}
					}
					else if(VKCode == 'K')
					{
						if(IsDown)
						{
							//Implement recording button separate from loop button
						}
					}
					else if(VKCode == 'L')
					{
						if(IsDown)
						{
							if(Win32State->InputRecordingIndex == 0)
							{
								Win32BeginRecordingInput(Win32State, 1);
							}
							else
							{
								Win32EndRecordingInput(Win32State);
								Win32BeginInputPlayback(Win32State, 1);
							}
						}							
					}
#endif
				}
		
				bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
		
				if((VKCode == VK_F4) && AltKeyWasDown)
				{
					GlobalRunning = false;
				}
			} break;
			
			default:
			{
				TranslateMessage(&Message);
				DispatchMessageA(&Message);
			} break;	
		}
	}
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
		case WM_CLOSE:
		{
			// NOTE: Handle this with a message to user
			OutputDebugStringA("WM_CLOSE\n");
			GlobalRunning = false;
		} break;
		
		case WM_ACTIVATEAPP:
		{
			if(wParam == TRUE)
			{
				SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
			}
			else
			{
				SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
			}
		} break;

		case WM_DESTROY:
		{
			// NOTE: Handle this as an error - recreate window
			OutputDebugStringA("WM_DESTROY\n");
			GlobalRunning = false;
		} break;
		
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Assert(!"Keyboard input came in through a non-dispatch message!");
							
		} break;
		
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

inline LARGE_INTEGER
Win32GetClockValue(void)
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return(Result);
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	real32 Result = ((real32)(End.QuadPart - Start.QuadPart) / 
						(real32)GlobalPerfCountFrequency);
								
	return(Result);
}

internal void
Win32DebugDrawVerticalHash
(
	win32_offscreen_buffer *Backbuffer, 
	int X, int Top, int Bottom, uint32 Color
)
{
	if(Top <= 0)
	{
		Top = 0;
	}

	if(Bottom > Backbuffer->Height)
	{
		Bottom = Backbuffer->Height;
	}

	if((X >= 0) && (X < Backbuffer->Width))
	{
		uint8 *Pixel = ((uint8 *)Backbuffer->Memory + 
						X*Backbuffer->BytesPerPixel + 
						Top*Backbuffer->Pitch);
		
		for(int Y = Top; Y < Bottom; ++Y)
		{
			*(uint32 *)Pixel = Color;
			Pixel += Backbuffer->Pitch;
		}
	}
}

inline void
Win32DrawSoundBufferMarker
(
	win32_offscreen_buffer *Backbuffer, 
	win32_sound_output *SoundOutput,
	DWORD Value, real32 Coefficient, 
	int PadX, int Top, int Bottom, uint32 Color
)
{
	real32 XReal32 = (Coefficient * (real32)Value);
	int X = PadX + (int)XReal32;
	Win32DebugDrawVerticalHash(Backbuffer, X, Top, Bottom, Color);
}

#if 1
internal void
Win32DebugSyncDisplay
(
	win32_offscreen_buffer *Backbuffer, int MarkerCount,
	win32_debug_time_marker *Markers, win32_sound_output *SoundOutput,
	real32 TargetSecondsPerFrame, int CurrentMarkerIndex
)
{
	//TODO: Draw where we're writing our sound
	
	int PadX = 16;
	int PadY = 16;
	
	int LineHeight = 64;
	
	real32 Coefficient = (real32)(Backbuffer->Width - 2*PadX) / 
		(real32)SoundOutput->SecondaryBufferSize;
	
	for
	(
		int MarkerIndex = 0;
		MarkerIndex < MarkerCount;
		++MarkerIndex
	)
	{
		win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
		Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);
		
		DWORD PlayColor = 0xFFFFFFFF;
		DWORD WriteColor = 0xFFFF0000;
		DWORD ExpectedFlipColor = 0xFFFFFF00;
		DWORD PlayWindowColor = 0xFFFF00FF;
		
		int Top = PadY;
		int Bottom = PadY + LineHeight;
		if(MarkerIndex == CurrentMarkerIndex)
		{
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;
			
			int FirstTop = Top;
			
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, ThisMarker->OutputPlayCursor, Coefficient, PadX, Top, Bottom, PlayColor);
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, ThisMarker->OutputWriteCursor, Coefficient, PadX, Top, Bottom, WriteColor);

			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;
			
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, ThisMarker->OutputLocation, Coefficient, PadX, Top, Bottom, PlayColor);
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, Coefficient, PadX, Top, Bottom, WriteColor);
			
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;
			
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, ThisMarker->ExpectedFlipPlayCursor, Coefficient, PadX, FirstTop, Bottom, ExpectedFlipColor);
		}
		
		Win32DrawSoundBufferMarker
		(
			Backbuffer, SoundOutput, ThisMarker->FlipPlayCursor, 
			Coefficient, PadX, Top, Bottom, PlayColor
		);
		
		Win32DrawSoundBufferMarker
		(
			Backbuffer, SoundOutput, ThisMarker->FlipPlayCursor + 480*SoundOutput->BytesPerSample, 
			Coefficient, PadX, Top, Bottom, PlayWindowColor
		);
		
		Win32DrawSoundBufferMarker
		(
			Backbuffer, SoundOutput, ThisMarker->FlipWriteCursor,
			Coefficient, PadX, Top, Bottom, WriteColor
		);
	}	
}
#endif

internal void 
ConcatenateStrings
(
	size_t SourceACount, char *SourceA,
	size_t SourceBCount, char *SourceB,
	size_t DestinationCount, char *Destination
)
{
	//TODO: Destination bounds checking
	
	for(int Index = 0; Index < SourceACount; ++Index)
	{
		*Destination++ = *SourceA++; 
	}
	
	for(int Index = 0; Index < SourceBCount; ++Index)
	{
		*Destination++ = *SourceB++;
	}
	
	*Destination++ = 0;
}

int CALLBACK
WinMain
(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR     CommandLine,
	int       ShowCode
)
{
	//NOTE: Don't use MAX_PATH in code that's user-facing, it can be dangerous
	//and lead to bad results
	char EXEFilename[MAX_PATH];
	DWORD SizeOfFileName = GetModuleFileNameA(0, EXEFilename, sizeof(EXEFilename));
	char *OnePastLastSlash = EXEFilename;
	for(char *Scan = EXEFilename; *Scan; ++Scan)
	{
		if(*Scan == '\\')
		{
			OnePastLastSlash = Scan + 1;
		}
	}

	char SourceGameCodeDLLFilename[] = "handmade.dll";
	char SourceGameCodeDLLFullPath[MAX_PATH];
	
	ConcatenateStrings
	(
		OnePastLastSlash - EXEFilename, EXEFilename,
		sizeof(SourceGameCodeDLLFilename) - 1, SourceGameCodeDLLFilename,
		sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath
	);

	char TempGameCodeDLLFilename[] = "handmade_temp.dll";
	char TempGameCodeDLLFullPath[MAX_PATH];
	
	ConcatenateStrings
	(
		OnePastLastSlash - EXEFilename, EXEFilename,
		sizeof(TempGameCodeDLLFilename) - 1, TempGameCodeDLLFilename,
		sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath
	);
	
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	//NOTE: Set the scheduler granularity to 1ms for more granular Sleep()
	UINT DesiredSchedulerMS = 1;
	bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
	
	Win32LoadXInput();

	WNDCLASSA WindowClassObject = {};

	Win32ResizeDIBSection(&GlobalBackbuffer, 1200, 720);
	
	WindowClassObject.style = CS_HREDRAW|CS_VREDRAW;
	WindowClassObject.lpfnWndProc = Win32MainWindowCallback;
	WindowClassObject.hInstance = Instance;
	//WindowClass.hIcon;
	WindowClassObject.lpszClassName = "HandmadeHeroWindowClass";
	
	//TODO: How do we reliably query this on Windows?
#define MonitorRefreshHz 120
#define GameUpdateHz (MonitorRefreshHz / 2)

	real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

	if(RegisterClassA(&WindowClassObject))
	{
		HWND Window = 
			CreateWindowExA(
				WS_EX_TOPMOST|WS_EX_LAYERED,
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
			win32_sound_output SoundOutput = {};

			//make this so long that there's no way the game could ever pause for this long so we can see where the play cursor is?
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * 
				SoundOutput.BytesPerSample;
				
			//TODO: Get rid of LatencySampleCount
			SoundOutput.LatencySampleCount = 3*(SoundOutput.SamplesPerSecond / GameUpdateHz);
			//TODO: Compute the variance and see what the lowest reasonable value is
			SoundOutput.SafetyBytes = (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample / GameUpdateHz / 3);
			Win32InitializeDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			win32_state Win32State = {};
			GlobalRunning = true;

#if 0
			//NOTE: This tests the PlayCursor/WriteCursor update frequency
			//On Handmade Hero machine it was 480 samples
			while(GlobalRunning)
			{
				DWORD PlayCursor;
				DWORD WriteCursor;
				
				GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
				
				char TextBuffer[256];
						
				_snprintf_s
				(
					TextBuffer, sizeof(TextBuffer),
					"PC: %u WC:%u\n", PlayCursor, WriteCursor
				);
				
				OutputDebugStringA(TextBuffer);
				
			}
#endif		
			int16 *Samples = (int16 *)VirtualAlloc
			(	
				0,
				SoundOutput.SecondaryBufferSize,
				MEM_RESERVE|MEM_COMMIT, 
				PAGE_READWRITE
			);
			
#if HANDMADE_INTERNAL
		LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
		LPVOID BaseAddress = 0;
#endif	
			
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes(1);
			GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
			GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
			GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

			//Handle various memory footprints
			
			Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			
			Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize,
									MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			
			GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
			
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + 
										GameMemory.PermanentStorageSize);
			
			if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
		
				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];

				LARGE_INTEGER LastCounter = Win32GetClockValue();
				LARGE_INTEGER FlipWallClock = Win32GetClockValue();
						
				int DebugTimeMarkerIndex = 0;
				win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {0};
				
				DWORD AudioLatencyBytes = 0;
				real32 AudioLatencySeconds = 0;
				bool32 SoundIsValid = false;
	
				win32_game_code Game = Win32LoadGameCode
				(
					SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath
				);
				
				uint32 LoadCounter = 0;
									
				uint64 LastCycleCount = __rdtsc();
				while(GlobalRunning)
				{

					FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
					
					if(CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
					{
						Win32UnloadGameCode(&Game);
						Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
						LoadCounter = 0;
					}
					
					//TODO: Zeroing macro
					//TODO: We can't zero everything because the up/down state will be wrong
					game_controller_input *OldKeyboardController = GetController(OldInput, 0);
					game_controller_input *NewKeyboardController = GetController(NewInput, 0);
					*NewKeyboardController = {};
					NewKeyboardController->IsConnected = true;
					
					for(int ButtonIndex = 0;
						ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
						++ButtonIndex)
					{
						NewKeyboardController->Buttons[ButtonIndex].EndedDown = 
							OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}
					
					Win32ProcessPendingMessages(&Win32State, NewKeyboardController);
					
					if(!GlobalPause)
					{
						DWORD MaxControllerCount = XUSER_MAX_COUNT;
						
						if(MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
						{
							MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
						}
						
						//should we poll this more frequently
						for(DWORD ControllerIndex = 0;	
							ControllerIndex < MaxControllerCount; 
							++ControllerIndex)
						{
							DWORD OurControllerIndex = ControllerIndex + 1;
							
							game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
							game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

							XINPUT_STATE ControllerState;
							//when we call XInputGetState, if no controller is plugged in for an index, XINPUT will stall
							//for several millis. THIS IS REALLY BAD, WE ONLY WANT TO PULL THE CONNECTED CONTROLLERS
							if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
							{
								NewController->IsConnected = true;
								
								//TODO: see if controllerstate.dwpacketnumber increments too rapidly
								XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
								
								//NOTE: This is a square deadzone, check XInput to verify
								//that the deadzone is "round" and show how to do round
								//deadzone processing
								
								NewController->StickAverageX = Win32ProcessXInputStickValue
								(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								
								NewController->StickAverageY = Win32ProcessXInputStickValue
								(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								
								if((NewController->StickAverageX != 0.0f) || 
									(NewController->StickAverageY != 0.0f))
								{
									NewController->IsAnalog = true;
								}
								
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
								{
									NewController->StickAverageY = 1.0f;
									NewController->IsAnalog = false;
								}
								
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
								{
									NewController->StickAverageY = -1.0f;
									NewController->IsAnalog = false;
								}
								
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
								{
									NewController->StickAverageX = -1.0f;
									NewController->IsAnalog = false;
								}
								
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
								{
									NewController->StickAverageX = 1.0f;
									NewController->IsAnalog = false;
								}
															
								real32 Threshold = 0.5f;
								Win32ProcessXInputDigitalButton
								(
									(NewController->StickAverageX < -Threshold) ? 1 : 0, 
									&OldController->MoveLeft,	&NewController->MoveLeft, 1
								);							
								Win32ProcessXInputDigitalButton
								(
									(NewController->StickAverageX > Threshold) ? 1 : 0, 
									&OldController->MoveRight, &NewController->MoveRight, 1
								);
								Win32ProcessXInputDigitalButton
								(
									(NewController->StickAverageY > Threshold) ? 1 : 0, 
									&OldController->MoveUp, &NewController->MoveUp, 1
								);							
								Win32ProcessXInputDigitalButton
								(
									(NewController->StickAverageY < -Threshold) ? 1 : 0, 
									&OldController->MoveDown, &NewController->MoveDown, 1
								);							
								
								Win32ProcessXInputDigitalButton
								(
									Pad->wButtons, &OldController->ActionDown,
									&NewController->ActionDown, XINPUT_GAMEPAD_A
								);
								Win32ProcessXInputDigitalButton
								(
									Pad->wButtons, &OldController->ActionLeft,
									&NewController->ActionLeft, XINPUT_GAMEPAD_B
								);
								Win32ProcessXInputDigitalButton
								(
									Pad->wButtons, &OldController->ActionRight,
									&NewController->ActionRight, XINPUT_GAMEPAD_X
								);
								Win32ProcessXInputDigitalButton
								(
									Pad->wButtons, &OldController->ActionUp,
									&NewController->ActionUp, XINPUT_GAMEPAD_Y
								);
								
								Win32ProcessXInputDigitalButton
								(
									Pad->wButtons, &OldController->LeftShoulder,
									&NewController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER
								);
								Win32ProcessXInputDigitalButton
								(
									Pad->wButtons, &OldController->RightShoulder,
									&NewController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER
								);
								
								Win32ProcessXInputDigitalButton
								(
									Pad->wButtons, &OldController->Start,
									&NewController->Start, XINPUT_GAMEPAD_START
								);
								Win32ProcessXInputDigitalButton
								(
									Pad->wButtons, &OldController->Back,
									&NewController->Back, XINPUT_GAMEPAD_BACK
								);
								
							}
							else
							{
								//controller is not available
								NewController->IsConnected = false;
							}
						}

						game_offscreen_buffer Buffer = {};
						Buffer.Memory = GlobalBackbuffer.Memory;
						Buffer.Pitch = GlobalBackbuffer.Pitch;
						Buffer.Height = GlobalBackbuffer.Height;
						Buffer.Width = GlobalBackbuffer.Width;
						Buffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;
						
						if(Win32State.InputRecordingIndex)
						{
							Win32RecordInput(&Win32State, NewInput);
						}
						
						if(Win32State.InputPlayingIndex)
						{
							Win32PlaybackInput(&Win32State, NewInput);
						}
						
						Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);
						
						LARGE_INTEGER AudioWallClock = Win32GetClockValue();
						real32 FromBeginToAudioSeconds = 
							1000.0f*Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);
						
						DWORD PlayCursor;
						DWORD WriteCursor;
						if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
						{
							
							/* NOTE: 
							Here is how sound output computation works.
							
							We define a safety value that is the number of samples we think our game 
							update loop may vary by (let's say up to 2ms).
							
							When we wake up to write audio, we'll look and see what the play
							cursor position is and forecast ahead where we think the play cursor
							will be on the next frame boundary.
							
							We then look to see if the write cursor is before that by our safety value. 
							If it is, the target fill position is that frame boundary plus one frame. 
							This gives us perfect audio sync in the case of a card with low enough latency.
							
							If the write cursor is after the safety margin, we assume that we 
							can never sync the audio perfectly, so we'll write one frame's worth of 
							audio plus the safety margin's worth of guard samples.
							*/
							
							if(!SoundIsValid)
							{
								SoundOutput.RunningSampleIndex = WriteCursor / 
									SoundOutput.BytesPerSample;
								SoundIsValid = true;
							}
							
							DWORD ByteToLock =
							(
								(SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) %
								SoundOutput.SecondaryBufferSize
							);
							
							DWORD ExpectedSoundBytesPerFrame = 
							(	
								(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / 
								GameUpdateHz
							);
							
							real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
							
							DWORD ExpectedBytesUntilFlip = 
							(
								(DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * 
								(real32)ExpectedSoundBytesPerFrame)
							);
							
							DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;
							
							DWORD SafeWriteCursor = WriteCursor;
							
							if(SafeWriteCursor < PlayCursor)
							{
								SafeWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							
							Assert(SafeWriteCursor >= PlayCursor);
		
							SafeWriteCursor += SoundOutput.SafetyBytes;
							
							bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);
							
							DWORD TargetCursor = 0;
							if(AudioCardIsLowLatency)
							{
								TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
							}
							else
							{
								TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + 
												SoundOutput.SafetyBytes);
							}
							
							TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);
							
							DWORD BytesToWrite = 0;
							if(ByteToLock > TargetCursor)
							{
								BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
								BytesToWrite += TargetCursor;
							}
							else
							{
								BytesToWrite = TargetCursor - ByteToLock;
							}
							
							game_sound_output_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
							SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
							SoundBuffer.Samples = Samples;
							Game.GetSoundSamples(&GameMemory, &SoundBuffer);
								
#if HANDMADE_INTERNAL
							win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
							Marker->OutputPlayCursor = PlayCursor;
							Marker->OutputWriteCursor = WriteCursor;
							Marker->OutputLocation = ByteToLock;
							Marker->OutputByteCount = BytesToWrite;
							Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
							
							DWORD UnwrappedWriteCursor = WriteCursor;
							if(UnwrappedWriteCursor < PlayCursor)
							{
								UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
							AudioLatencySeconds = 
							(
								((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) /
								(real32)SoundOutput.SamplesPerSecond
							);
							
							char TextBuffer[256];
							_snprintf_s
							(
								TextBuffer, sizeof(TextBuffer),
								"BTL:%u TC:%u BTW:%u - PC:%u WC: %u DELTA: %u (%fs)\n",
								ByteToLock, TargetCursor, BytesToWrite, PlayCursor, 
								WriteCursor, AudioLatencyBytes, AudioLatencySeconds
							);
							OutputDebugStringA(TextBuffer);
#endif					
							Win32FillSoundBuffer
							(
								&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer
							);
						}
						else
						{
							SoundIsValid = false;
						}
															
						LARGE_INTEGER WorkCounter = Win32GetClockValue();
						real32 WorkSecondsElapsed = Win32GetSecondsElapsed
						(
							LastCounter, WorkCounter
						);

						//TODO: Test and debug
						real32 SecondsElapsedForFrame = WorkSecondsElapsed;
						if(SecondsElapsedForFrame < TargetSecondsPerFrame)
						{
							if(SleepIsGranular)
							{
								DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - 
									SecondsElapsedForFrame));
								
								if(SleepMS > 0)
								{
									Sleep(SleepMS);
								}
							
							}
							
							real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed
							(
								LastCounter, Win32GetClockValue()
							);
							
							if(TestSecondsElapsedForFrame < TargetSecondsPerFrame)
							{
								//TODO: LOG MISSED SLEEP
							}

							while(SecondsElapsedForFrame < TargetSecondsPerFrame)
							{
								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, 
									Win32GetClockValue());
							}
						}
						else
						{
							//TODO: Missed frame rate!
							//TODO: Logging
						}
						
						LARGE_INTEGER EndCounter = Win32GetClockValue();
						real32 MSPerFrame = 1000.0f*Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;
						
						win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if 1
						//NOTE: Current is wrong on on zero'th index
						Win32DebugSyncDisplay
						(
							&GlobalBackbuffer, ArrayCount(DebugTimeMarkers), 
							DebugTimeMarkers, &SoundOutput, TargetSecondsPerFrame,
							DebugTimeMarkerIndex - 1
						);
#endif		
						HDC DeviceContext = GetDC(Window);
						
						Win32DisplayBufferInWindow(DeviceContext, &GlobalBackbuffer,
												Dimension.Width, Dimension.Height);
												
						ReleaseDC(Window, DeviceContext);
						
						FlipWallClock = Win32GetClockValue();
						
#if HANDMADE_INTERNAL
						//NOTE: This is debug code
						{
							DWORD PlayCursor;
							DWORD WriteCursor;
							if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
							{
								Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
								win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
								Marker->FlipPlayCursor = PlayCursor;
								Marker->FlipWriteCursor = WriteCursor;
							}
						}
#endif
						game_input *Temp = NewInput;
						NewInput = OldInput;
						OldInput = Temp;
						//should these be cleared?
						
						uint64 EndCycleCount = __rdtsc();
						uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
						LastCycleCount = EndCycleCount;
						
						real64 FPS = 0.0f;//(real64)GlobalPerfCountFrequency/(real64)CounterElapsed;
						real64 MCPF = ((real64)CyclesElapsed / 1000000.0f);

						char FPSBuffer[256];
						_snprintf_s
						(
							FPSBuffer, sizeof(FPSBuffer),
							"%.02fms/f, / %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF
						);
						OutputDebugStringA(FPSBuffer);
					
#if HANDMADE_INTERNAL
						++DebugTimeMarkerIndex;
						if(DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers))
						{
							DebugTimeMarkerIndex = 0;							
						}
#endif	
					}
				}
			}
			else
			{
				
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
	return 0;
}