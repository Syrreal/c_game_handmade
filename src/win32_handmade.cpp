#include <windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>

#define internal static 
#define local_persist static 
#define global_variable static

typedef int32_t bool32;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel = 4;
};


struct win32_window_dimensions
{
    int Width;
    int Height;
};

internal win32_window_dimensions Win32GetWindowDimensions(HWND hWnd)
{
    win32_window_dimensions Result;

    RECT ClientRect;
    GetClientRect(hWnd, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}

// TODO: This is global _for now_
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
    if(!XInputLibrary)
    {
        // TODO: Diagnostic
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }

    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");

        // TODO: Diagnostic
    }
    else
    {
        // TODO: Diagnostic
    }
}

internal void Win32InitDSound(HWND hWnd, int32_t SamplesPerSecond ,int32_t BufferSize)
{
    // Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    
    if(DSoundLibrary)
    {
        // Get a DirectSound object
        direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        
        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if(SUCCEEDED(DirectSound->SetCooperativeLevel(hWnd, DSSCL_PRIORITY)))
            {
                // "Create" a primary buffer
                // NOTE: THIS IS NOT A SOUND BUFFER!!!
                // The primary buffer is ONLY used as a "handle" to the sound card to set the WaveFormat
                // ONLY WRITE INTO THE SECONDARY BUFFER, the secondary buffer is the actual sound buffer
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                LPDIRECTSOUNDBUFFER PrimaryBuffer;

                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    // Required by windows to set the format of the primary buffer with SetFormat
                    // and NOT with the lpwfxFormat member
                    HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
                    if(SUCCEEDED(Error))
                    {
                        OutputDebugStringA("Primary buffer format was set. \n");
                    }
                    else
                    {
                        // TODO: Diagnostic
                    }

                }
                else
                {
                    // TODO: Diagnostic
                }

                // "Create" a secondary buffer that you can actually write to
                BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_CTRLVOLUME;
                BufferDescription.dwBufferBytes = BufferSize;
                BufferDescription.lpwfxFormat = &WaveFormat;
                LPDIRECTSOUNDBUFFER SecondaryBuffer;

                HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
                if(SUCCEEDED(Error))
                {
                    OutputDebugStringA("Secondary Buffer created successfully. \n");
                }
                else
                {
                    // TODO: Diagnostic
                }
                

            }
            else
            {
                // TODO: Diagnostic
            }
        }
        else
        {
            // TODO: Diagnostic
        }
    }
    else
    {
        // TODO: Diagnostic
    }
}

internal void RenderWeirdGradient(win32_offscreen_buffer* Buffer, int XOffset, int YOffset)
{
    int Width = Buffer->Width;
    int Height = Buffer->Height;
    uint8_t* Row = (uint8_t *)Buffer->Memory;
    for(int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32_t* Pixel = (uint32_t*)Row;
        for(int X = 0; X < Buffer->Width; ++X)
        {
            uint8_t B = uint8_t(X + XOffset);
            uint8_t G = uint8_t(Y + YOffset);
            // uint8_t R = (Width+Height)*0.5*0.5;
            // Pixel = 0xXXRRGGBB
            *Pixel++ = B | (G << 8);// | (R << 16);
        }
        Row += Buffer->Pitch;
    }
}

internal void ResizeDIBSection(win32_offscreen_buffer* Buffer, int Width, int Height)
{
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    
    Buffer->Pitch = Buffer->Width*Buffer->BytesPerPixel;
    int BitmapMemorySize = Width*Height*(Buffer->BytesPerPixel);
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    //TODO: Probably clear this to black

    
}

internal void Win32DisplayBuffer(HDC DeviceContext, win32_offscreen_buffer* Buffer,
                                int WindowWidth, int WindowHeight)
{
    // FIXME: Aspect ratio Correction
    // local_persist int HeightRatio;
    // local_persist int WidthRatio;
    // int Val1;
    // int Val2;
    // if(!HeightRatio && !WidthRatio)
    // {
    //     if(Buffer->Height == Buffer->Width)
    //     {
    //         HeightRatio = Buffer->Height;
    //         WidthRatio = Buffer->Width;
    //     }
    //     else if(Buffer->Height < Buffer->Width)
    //     {
    //         Val1 = Buffer->Width;
    //         Val2 = Buffer->Height;
    //     }
    //     else
    //     {
    //         Val1 = Buffer->Height;
    //         Val2 = Buffer->Width;
    //     }
    //     while(Val2)
    //     {
    //         int Val3 = Val1 - Val2;
    //         if(!Val3)
    //         {
    //             HeightRatio = Buffer->Height/Val2;
    //             WidthRatio = Buffer->Width/Val2;
    //             break;
    //         }
    //         if(Val3 < Val2)
    //         {
    //             Val1 = Val2;
    //             Val2 = Val3;
    //         }
    //         else
    //             Val1 = Val3;
    //     }
    // }
    // int TrueWidth = WindowWidth - (WindowWidth%WidthRatio);
    // int TrueHeight = WindowHeight - (WindowHeight%HeightRatio);
    // TODO: Filtering after aspect ratio correction (may be able to use stretch contexts instead)   

    StretchDIBits(DeviceContext,
                0, 0, WindowWidth, WindowHeight,
                0, 0, Buffer->Width, Buffer->Height,
                Buffer->Memory, &Buffer->Info,
                DIB_RGB_COLORS, SRCCOPY);
}

internal LRESULT CALLBACK Win32MainWindowCallback( HWND hWnd, 
                                        UINT Msg,
                                        WPARAM WParam,
                                        LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Msg)
    {
        case WM_DESTROY:
        {
            // TODO: Handle this as an error - recreate window?
            GlobalRunning = false;
            OutputDebugStringA("WM_DESTROY\n");
        } break;
        case WM_CLOSE:
        {
            // TODO: Handle this with a message to the user?
            GlobalRunning = false;
            OutputDebugStringA("WM_CLOSE\n");
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
            uint32_t VKCode = WParam;
            bool WasDown = ((LParam & (1 << 30)) != 0);
            bool IsDown = ((LParam & (1 << 30)) == 0);

            if (WasDown != IsDown)
            {
                if (VKCode == 'W')
                {
                }
                else if (VKCode == 'A')
                {
                }
                else if (VKCode == 'S')
                {
                }
                else if (VKCode == 'D')
                {
                }
                else if (VKCode == 'Q')
                {
                }
                else if (VKCode == 'E')
                {
                }
                else if (VKCode == VK_SPACE)
                {
                }
                else if (VKCode == VK_SHIFT)
                {
                }
                else if (VKCode == VK_ESCAPE)
                {
                }
                else if (VKCode == VK_LBUTTON)
                {
                }
            }
            uint32_t AltKeyBit = (1 << 29);
            bool32 AltKeyWasDown = ((LParam & AltKeyBit) != 0);
            if((VKCode == VK_F4) && AltKeyWasDown)
            {
                GlobalRunning = false;
            }
        } break;
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(hWnd, &Paint);
            win32_window_dimensions Dimensions = Win32GetWindowDimensions(hWnd);
            Win32DisplayBuffer(DeviceContext, &GlobalBackBuffer, Dimensions.Width, Dimensions.Height);
            EndPaint(hWnd, &Paint);
        } break;
        default:
        {
            // OutputDebugStringA("default\n");
            Result = DefWindowProc(hWnd, Msg, WParam, LParam);
        } break;
    }

    return Result;
}


int CALLBACK WinMain(HINSTANCE hInstance, 
                    HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine,
                    int nCmdShow)
{
    Win32LoadXInput();

    WNDCLASSA Win32WindowClass{};
    Win32WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    Win32WindowClass.lpfnWndProc = Win32MainWindowCallback;
    Win32WindowClass.hInstance = hInstance;
    // WindowClass.hIcon;
    Win32WindowClass.lpszClassName = "FancySmanschyWindowClass";

    ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    if(RegisterClassA(&Win32WindowClass))
    {
        HWND hWnd = CreateWindowExA(
                                    0,
                                    Win32WindowClass.lpszClassName,
                                    "UranBitch",
                                    WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                    CW_USEDEFAULT,
                                    CW_USEDEFAULT,
                                    CW_USEDEFAULT,
                                    CW_USEDEFAULT,
                                    0,
                                    0,
                                    hInstance,
                                    0);
        if(hWnd)
        {
            HDC DeviceContext = GetDC(hWnd);

            // Graphics Test
            int XOffset = 0;
            int YOffset = 0;

            // Set the samples per second for DirectSound
            // then set buffersize to the SamplesPerSecond * bits per sample * the number of channels
            int32_t SamplesPerSecond = 48000;
            int BytesPerSample = sizeof(int16_t)*2;
            int32_t SecondaryBufferSize = SamplesPerSecond*BytesPerSample;
            Win32InitDSound(hWnd, SamplesPerSecond, SecondaryBufferSize);
            // Sound Test
            int ToneHz = 50;
            int ToneVolume = 500;
            uint32_t RunningSampleIndex = 0;
            int SquareWavePeriod = SamplesPerSecond/ToneHz;

            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);


            GlobalRunning = 1;
            while(GlobalRunning)
            {
                MSG Message;
                while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                        GlobalRunning = false;
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                //TODO: Should we poll this more frequently?
                for(DWORD ControllerIndex=0; ControllerIndex < XUSER_MAX_COUNT; ControllerIndex++)
                {
                    XINPUT_STATE ControllerState;
                    if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // This controller is plugged in
                        //TODO: See if ControllerState.dwPacketNumber incrememnts too rapidly
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                        bool DPadUp = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool DPadDown = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool DPadLeft = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool DPadRight = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool ShoulderLeft = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool ShoulderRight = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool ButtonA = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool ButtonB = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool ButtonX = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool ButtonY = (Pad->wButtons & XINPUT_GAMEPAD_Y);
                        bool ButtonStart = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool ButtonBack = (Pad->wButtons & XINPUT_GAMEPAD_BACK);

                        int16_t StickX = Pad->sThumbLX;
                        int16_t StickY = Pad->sThumbLY;

                    }
                    else
                    {
                        //The controller is not available
                    }
                }

                RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);

                // Direct sound output test
                DWORD PlayCursor;
                DWORD WriteCursor;
                if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                {
                    DWORD ByteToLock = RunningSampleIndex*BytesPerSample % SecondaryBufferSize;
                    DWORD BytesToWrite;
                    if(BytesToLock == PlayCursor)
                    {
                        BytesToWrite = SecondaryBufferSize
                    }
                    else if(ByteToLock > PlayCursor)
                    {
                        BytesToWrite = SecondaryBufferSize - ByteToLock;
                        BytesToWrite += PlayCursor;
                    }
                    else
                    {
                        BytesToWrite = PlayCursor - ByteToLock;
                    }

                    VOID *Region1;
                    DWORD Region1Size;
                    VOID *Region2;
                    DWORD Region2Size;


                    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                                &Region1, &Region1Size,
                                                &Region2, &Region2Size, 0)))
                    {
                        // TODO: assert that Region1Size/Region2Size is valid
                        int16_t* SampleOut = (int16_t*)Region1;
                        DWORD Region1SampleCount = Region1Size/BytesPerSample;
                        for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
                        {
                            int16_t SampleValue = ((RunningSampleIndex++ / (SquareWavePeriod/2)) % 2) ? ToneVolume : -ToneVolume;
                            *SampleOut++ = SampleValue;
                            *SampleOut++ = SampleValue;
                            ++RunningSampleIndex;
                        }

                        SampleOut = (int16_t*)Region2;
                        DWORD Region2SampleCount = Region2Size/BytesPerSample;
                        for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
                        {
                            int16_t SampleValue = ((RunningSampleIndex++ / (SquareWavePeriod/2)) % 2) ? ToneVolume : -ToneVolume;
                            *SampleOut++ = SampleValue;
                            *SampleOut++ = SampleValue;
                            ++RunningSampleIndex;
                        }
                    }
                    //TODO: assert that unlock succeeded
                    GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
                }


                win32_window_dimensions Dimensions = Win32GetWindowDimensions(hWnd);
                Win32DisplayBuffer(DeviceContext, &GlobalBackBuffer, Dimensions.Width, Dimensions.Height);
                ReleaseDC(hWnd, DeviceContext);

                YOffset += 1;
                
            }
        }
        else
        {
            // TODO: Logging
        }
    }
    else
    {
        // TODO: Logging
    }


}