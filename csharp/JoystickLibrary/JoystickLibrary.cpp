/*
* Copyright (c) 2016, Wisconsin Robotics
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
* * Redistributions of source code must retain the above copyright
*   notice, this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
*   notice, this list of conditions and the following disclaimer in the
*   documentation and/or other materials provided with the distribution.
* * Neither the name of Wisconsin Robotics nor the
*   names of its contributors may be used to endorse or promote products
*   derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL WISCONSIN ROBOTICS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "JoystickLibrary.h"

#include <map>
#include <array>

using namespace JoystickLibrary;

const std::array<POV, 8> povList = {
    POV::POV_NORTH,
    POV::POV_NORTHEAST,
    POV::POV_EAST,
    POV::POV_SOUTHEAST,
    POV::POV_SOUTH,
    POV::POV_SOUTHWEST,
    POV::POV_WEST,
    POV::POV_NORTHWEST,
};

struct EnumerateContext
{
    int *nextJoystickID;
    int *requestedJoysticks;
    int *connectedJoysticks;
    int *vendor_id;
    int *product_id;
    std::map<int, JoystickData> *jsMap;
    LPDIRECTINPUT8 di;
};


/**
 Callback to configure the axis values on the joystick.
 @param instance A pointer to the device instance
 @param context A pointer to the joystick instance
 @return DIENUM_CONTINUE on success, DIENUM_STOP otherwise
 */
static BOOL CALLBACK JoystickConfigCallback(const DIDEVICEOBJECTINSTANCE *instance, void *context)
{
    DIPROPRANGE propRange;
    LPDIRECTINPUTDEVICE8 joystick;

    if (!context)
        return DIENUM_STOP;

    memset(&propRange, 0, sizeof(DIPROPRANGE));
    propRange.diph.dwSize = sizeof(DIPROPRANGE);
    propRange.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    propRange.diph.dwHow = DIPH_BYID;
    propRange.diph.dwObj = instance->dwType;
    propRange.lMin = -100;
    propRange.lMax = +100;

    // Set the range for the axis
    joystick = (LPDIRECTINPUTDEVICE8)context;
    if (FAILED(joystick->SetProperty(DIPROP_RANGE, &propRange.diph)))
        return DIENUM_STOP;

    return DIENUM_CONTINUE;
}

/**
  A callback function called for each found joystick.
  @param instance A pointer to the device instance.
  @param context (unused)
  @return DIENUM_CONTINUE on success, DIENUM_STOP otherwise
 */
static BOOL CALLBACK EnumerateJoysticks(const DIDEVICEINSTANCE *instance, void *context)
{
    EnumerateContext *info = (EnumerateContext *) context;
    LPDIRECTINPUTDEVICE8 joystick;
    DIPROPDWORD dipdw;

    if (info->connectedJoysticks >= info->requestedJoysticks)
        return DIENUM_STOP;

    DIPROPGUIDANDPATH jsGuidPath;
    jsGuidPath.diph.dwSize = sizeof(DIPROPGUIDANDPATH);
    jsGuidPath.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    jsGuidPath.diph.dwHow = DIPH_DEVICE;
    jsGuidPath.diph.dwObj = 0;

    if (FAILED(info->di->CreateDevice(instance->guidInstance, &joystick, nullptr)))
        return DIENUM_CONTINUE;

    if (FAILED(joystick->GetProperty(DIPROP_GUIDANDPATH, &jsGuidPath.diph)))
    {
        joystick->Release();
        return DIENUM_CONTINUE;
    }

    // check if joystick was a formerly removed one
    for (auto& pair : *(info->jsMap))
    {
        DIPROPGUIDANDPATH hw_info;
        LPDIRECTINPUTDEVICE8 inactiveJoystick;

        inactiveJoystick = (LPDIRECTINPUTDEVICE8)pair.second.os_obj;
        hw_info.diph.dwSize = sizeof(DIPROPGUIDANDPATH);
        hw_info.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        hw_info.diph.dwHow = DIPH_DEVICE;
        hw_info.diph.dwObj = 0;

        if (FAILED(inactiveJoystick->GetProperty(DIPROP_GUIDANDPATH, &hw_info.diph)))
            continue;

        // path match
        if (hw_info.wszPath && jsGuidPath.wszPath && lstrcmp(hw_info.wszPath, jsGuidPath.wszPath) == 0)
        {
            // if this path is already active, don't enumerate
            if (pair.second.alive)
            {
                joystick->Release();
                return DIENUM_CONTINUE;
            }

            pair.second.alive = true;
            inactiveJoystick->Acquire();

            *(info->connectedJoysticks)++;
            joystick->Release();
            return DIENUM_CONTINUE;
        }
    }

    // check vendor and product ID
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;

    if (FAILED(joystick->GetProperty(DIPROP_VIDPID, &dipdw.diph)))
    {
        joystick->Release();
        return DIENUM_CONTINUE;
    }

    if (LOWORD(dipdw.dwData) != *(info->vendor_id) || HIWORD(dipdw.dwData) != *(info->product_id))
    {
        joystick->Release();
        return DIENUM_CONTINUE;
    }

    // create and start tracking joystick
    // use DIJOYSTATE struct for data acquisition
    if (FAILED(joystick->SetDataFormat(&c_dfDIJoystick)))
        return DIENUM_CONTINUE;

    // axis configuration to -100 -> 100
    if (FAILED(joystick->EnumObjects(JoystickConfigCallback, joystick, DIDFT_AXIS)))
        return DIENUM_CONTINUE;

    // new joystick - add to map & acquire
    joystick->Acquire();
    (*(info->jsMap))[*(info->nextJoystickID)] = { };
    (*(info->jsMap))[*(info->nextJoystickID)].alive = true;
    (*(info->jsMap))[*(info->nextJoystickID)].os_obj = joystick;
    *(info->nextJoystickID)++;
    *(info->connectedJoysticks)++;

    return DIENUM_CONTINUE;
}

JoystickService::JoystickService(int number_joysticks)
{
    requestedJoysticks = number_joysticks;
    connectedJoysticks = 0;
    nextJoystickID = 1;
    this->jsPollerStop = false;
    this->initialized = false;
    this->jsMap = new std::map<int, JoystickData>();
    this->m_lock = gcnew Object();
}

JoystickService::~JoystickService()
{
    this->jsPollerStop = true;

    if (this->jsPoller->IsAlive)
        this->jsPoller->Join();

    if (jsMap)
    {
        for (auto& pair : *jsMap)
        {
            LPDIRECTINPUTDEVICE8 js = (LPDIRECTINPUTDEVICE8)pair.second.os_obj;

            if (js)
            {
                js->Unacquire();
                js->Release();
            }
        }

        delete jsMap;
    }

    if (di)
        di->Release();
}

bool JoystickService::Initialize()
{
    HRESULT hr;

    if (this->initialized)
        return false;

    // initialize directinput
    hr = DirectInput8Create(
        GetModuleHandle(nullptr),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        (LPVOID *) di,
        nullptr
    );

    if (FAILED(hr))
        return false;

    this->initialized = true;
    return true;
}

bool JoystickService::Start(void)
{
    if (!this->initialized)
        return false;

    jsPollerStop = false;
    jsPoller = gcnew Thread(gcnew ThreadStart(this, &JoystickService::PollJoysticks));
    jsPoller->Start();
    return true;
}

bool JoystickService::IsValidJoystickID(int joystickID)
{
    return this->initialized && 
        (jsMap->find(joystickID) != jsMap->end()) &&
        jsMap->at(joystickID).alive;
}

bool JoystickService::RemoveJoystick(int joystickID)
{
    if (!this->initialized || !this->IsValidJoystickID(joystickID))
        return false;

    msclr::lock l(m_lock);

    this->jsMap->at(joystickID).os_obj->Unacquire();
    this->jsMap->at(joystickID).alive = false;
    connectedJoysticks--;

    return true;
}

void JoystickService::PollJoysticks()
{
    if (!this->initialized)
        return;

    while (!this->jsPollerStop)
    {
        HRESULT hr;
        DIJOYSTATE js;
        LPDIRECTINPUTDEVICE8 joystick;

        msclr::lock l(m_lock);
        this->LocateJoysticks();

        for (auto& pair : *jsMap)
        {
            if (!pair.second.alive)
                continue;

            JoystickData& jsData = pair.second;
            joystick = (LPDIRECTINPUTDEVICE8)jsData.os_obj;

            hr = joystick->Poll();
            if (FAILED(hr))
            {
                do
                {
                    hr = joystick->Acquire();
                } while (hr == DIERR_INPUTLOST);

                // joystick fatal error
                if (hr == DIERR_INVALIDPARAM ||
                    hr == DIERR_NOTINITIALIZED ||
                    hr == DIERR_OTHERAPPHASPRIO)
                {
                    // release and invalidate joystick - not found!
                    jsData.os_obj->Unacquire();
                    jsData.alive = false;
                    connectedJoysticks--;
                    continue;
                }
            }

            hr = joystick->GetDeviceState(sizeof(DIJOYSTATE), &js);
            if (FAILED(hr))
            {
                // release and invalidate joystick - not found!
                jsData.os_obj->Unacquire();
                jsData.alive = false;
                connectedJoysticks--;
                continue;
            }

            jsData.state = js;
        }

        Thread::Sleep(100);
    }
}

void JoystickService::LocateJoysticks()
{
    EnumerateContext context;

    pin_ptr<int> p_connectedJoysticks = &this->connectedJoysticks;
    pin_ptr<int> p_nextJoystickID = &this->nextJoystickID;
    pin_ptr<int> p_product_id = &this->product_id;
    pin_ptr<int> p_requestedJoysticks = &this->requestedJoysticks;
    pin_ptr<int> p_vendor_id = &this->vendor_id;
    
    context.connectedJoysticks = p_connectedJoysticks;
    context.jsMap = this->jsMap;
    context.nextJoystickID = p_nextJoystickID;
    context.product_id = p_product_id;
    context.requestedJoysticks = p_requestedJoysticks;
    context.vendor_id = p_vendor_id;
    context.di = this->di;

    di->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumerateJoysticks, &context, DIEDFL_ATTACHEDONLY);
}

List<int>^ JoystickService::GetJoystickIDs()
{
    List<int>^ ids = gcnew List<int>();

    for (auto& pair : *jsMap)
        if (pair.second.alive)
            ids->Add(pair.first);

    return ids;
}

int JoystickService::GetConnectedJoysticksCount()
{
    return connectedJoysticks;
}

DIJOYSTATE JoystickService::GetState(int joystickID)
{
    return (*(this->jsMap))[joystickID].state;
}