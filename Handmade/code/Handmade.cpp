/*------------------------------------------------------------------
	$File: $
	$Date: $
	$Revision: $
	$Creator: James Martinez
	$Notice: This is ambitious
------------------------------------------------------------------*/

#include "Handmade.h"

internal void
GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz, game_state *GameState)
{

	int16 ToneVolume = 3000;
	//int ToneHz = 256;
	int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;
	
	int16 *SampleOut = SoundBuffer->Samples;
	
	for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
	{
		real32 SineValue = sinf(GameState->tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
		
		GameState->tSine += 2.0f*Pi*1.0f/(real32)WavePeriod;
		if(GameState->tSine > 2.0f*Pi)
		{
			GameState->tSine -= 2.0f*Pi;
		}
	}
}

internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	
	for(int Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		
		for(int X = 0; X < Buffer->Width; ++X)
		{
			uint8 Blue = (uint8)(X + BlueOffset);
			uint8 Green = (uint8)(Y + GreenOffset);
		
			*Pixel++ = ((Green << 8) | Blue);
		}
		
		Row += Buffer->Pitch;
	}
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == 
		(ArrayCount(Input->Controllers[0].Buttons)));
		
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	
	if(!Memory->IsInitialized)
	{
		char *Filename = __FILE__;
		
		debug_read_file_result File = Memory->DEBUGPlatformReadEntireFile(Filename);
		
		if(File.Contents)
		{
			Memory->DEBUGPlatformWriteEntireFile("test.out", File.ContentsSize, File.Contents);
			Memory->DEBUGPlatformFreeFileMemory(File.Contents);
		}
		
		GameState->ToneHz = 512;
		GameState->tSine = 0.0f;
		
		//we may do this in the platform layer instead
		Memory->IsInitialized = true;
	}
	
	for
	(	
		int ControllerIndex = 0; 
		ControllerIndex < ArrayCount(Input->Controllers);
		++ControllerIndex
	)
	{
		game_controller_input *Controller = GetController(Input, ControllerIndex);
		
		if(Controller->IsAnalog)
		{
			//NOTE: use analog movement tuning
			GameState->BlueOffset += (int)(4.0f*Controller->StickAverageX);
			GameState->ToneHz = 256 + (int)(128.0f*Controller->StickAverageY);
		}
		else
		{
			//NOTE: use digital movement tuning
			if(Controller->MoveLeft.EndedDown)
			{
				GameState->BlueOffset -= 1;
			}				
			
			if(Controller->MoveRight.EndedDown)
			{
				GameState->BlueOffset += 1;
			}
		}
		
		//Input.AButtonHalfTransitionCount;
		
		if(Controller->ActionDown.EndedDown)
		{
			GameState->GreenOffset += 1;
		}
	}	
	
	RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	GameOutputSound(SoundBuffer, GameState->ToneHz, GameState);
}

#if HANDMADE_WIN32

#include "windows.h"
BOOL WINAPI DllMain
(
	_In_ HINSTANCE hinstDLL,
	_In_ DWORD     fdwReason,
	_In_ LPVOID    lpvReserved
)
{
	return(TRUE);
}

#endif