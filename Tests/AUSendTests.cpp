// Exercise the built component through the same AUv2 API used by a DAW.
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>

static void require (bool ok, const char* message)
{
    if (! ok) { std::fprintf (stderr, "%s\n", message); std::exit (1); }
}

static UInt32 inputs (AudioUnit unit)
{
    UInt32 count = 0, size = sizeof (count);
    require (AudioUnitGetProperty (unit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input,
                                  0, &count, &size) == noErr, "query inputs");
    return count;
}

static bool named (AudioUnit unit, UInt32 bus, CFStringRef expected)
{
    CFStringRef name = nullptr;
    UInt32 size = sizeof (name);
    require (AudioUnitGetProperty (unit, kAudioUnitProperty_ElementName, kAudioUnitScope_Input,
                                  bus, &name, &size) == noErr && name, "query input name");
    const bool matches = CFEqual (name, expected);
    CFRelease (name);
    return matches;
}

static void set (AudioUnit unit, const char* name, float value)
{
    UInt32 size = 0;
    require (AudioUnitGetPropertyInfo (unit, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global,
                                       0, &size, nullptr) == noErr, "query parameters");
    std::vector<AudioUnitParameterID> ids (size / sizeof (AudioUnitParameterID));
    require (AudioUnitGetProperty (unit, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global,
                                  0, ids.data(), &size) == noErr, "list parameters");
    for (auto id : ids)
    {
        AudioUnitParameterInfo info {};
        size = sizeof (info);
        require (AudioUnitGetProperty (unit, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global,
                                      id, &info, &size) == noErr, "parameter info");
        if ((info.flags & kAudioUnitParameterFlag_CFNameRelease) && info.cfNameString)
            CFRelease (info.cfNameString);
        if (std::strcmp (info.name, name) == 0)
        {
            // The AU reports the signed [-20, 20] routing parameter on its native scale.
            if (std::strncmp (name, "DAW send slot ", 14) == 0)
                value = info.defaultValue + value * info.maxValue / 40.0f;
            require (AudioUnitSetParameter (unit, id, kAudioUnitScope_Global, 0, value, 0) == noErr, name);
            return;
        }
    }
    require (false, name);
}

static OSStatus supply (void* context, AudioUnitRenderActionFlags* flags, const AudioTimeStamp*,
                        UInt32, UInt32 frames, AudioBufferList* buffers)
{
    *flags &= ~kAudioUnitRenderAction_OutputIsSilence;
    for (UInt32 ch = 0; ch < buffers->mNumberBuffers; ++ch)
        std::fill_n (static_cast<float*> (buffers->mBuffers[ch].mData), frames,
                     *static_cast<float*> (context));
    return noErr;
}

int main (int argc, char** argv)
{
    require (argc == 2 || argc == 3, "expected AU binary path");
    const bool studioOne = argc == 3 && std::strcmp (argv[2], "--studio-one") == 0;
    auto* binary = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    require (binary != nullptr, "load built AU");
    auto factory = reinterpret_cast<AudioComponentFactoryFunction> (dlsym (binary, "MultiSqueezeAUFactory"));
    require (factory != nullptr, "find AU factory");
    AudioComponentDescription desc { kAudioUnitType_Effect, 'MSts', 'MthP', 0, 0 };
    auto component = AudioComponentRegister (&desc, CFSTR ("MultiSqueeze send test"), 1, factory);
    AudioUnit unit = nullptr;
    require (component && AudioComponentInstanceNew (component, &unit) == noErr, "instantiate AU");
    if (studioOne)
        require (inputs (unit) == 2 && named (unit, 1, CFSTR ("Sidechain 1")), "single host send");
    else
        require (inputs (unit) == 5 && named (unit, 4, CFSTR ("Sidechain 4")), "legacy inputs");
    require (AudioUnitInitialize (unit) == noErr, "initialize before changing routes");

    int changes = 0;
    auto changed = [] (void* context, AudioUnit, AudioUnitPropertyID, AudioUnitScope, AudioUnitElement)
    { ++*static_cast<int*> (context); };
    AudioUnitAddPropertyListener (unit, kAudioUnitProperty_ElementCount, changed, &changes);
    set (unit, "DAW send slot 3", 1);
    if (! studioOne)
    {
        require (inputs (unit) == 2 && named (unit, 1, CFSTR ("Band 3")), "Band 3 as first send");
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.25, false);
        require (changes > 0, "host receives live input change");
    }

    set (unit, "DAW send slot 1", 2);
    if (studioOne)
        require (inputs (unit) == 2, "both bands share the Studio One send");
    else
        require (inputs (unit) == 3 && named (unit, 2, CFSTR ("Band 1")), "independent second send");
    float mainLevel = 0.02f, keyLevel = 1.0f;
    AURenderCallbackStruct mainInput { supply, &mainLevel }, key { supply, &keyLevel };
    require (AudioUnitSetProperty (unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                                  0, &mainInput, sizeof (mainInput)) == noErr, "connect main input");
    require (AudioUnitSetProperty (unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                                  studioOne ? 1 : 2, &key, sizeof (key)) == noErr, "connect send to Band 1");
    set (unit, "Auto gain 1", 0);

    struct Stereo { UInt32 count; AudioBuffer channels[2]; };
    std::array<float, 256> left {}, right {};
    Stereo stereo { 2, {{ 1, 1024, left.data() }, { 1, 1024, right.data() }} };
    AudioTimeStamp time {};
    time.mFlags = kAudioTimeStampSampleTimeValid;
    auto render = [&]
    {
        for (int block = 0; block < 256; ++block)
        {
            AudioUnitRenderActionFlags flags = 0;
            require (AudioUnitRender (unit, &flags, &time, 0, 256,
                        reinterpret_cast<AudioBufferList*> (&stereo)) == noErr, "render routed AU");
            time.mSampleTime += 256;
        }
        return std::abs (left.back());
    };
    require (render() < 0.008f, "named send compresses the low band");
    set (unit, "DAW send slot 1", -2);
    require (inputs (unit) == 2 && render() > 0.018f, "disabling restores detector and removes target");
    set (unit, "Split 2", 0);
    require (named (unit, 1, CFSTR ("Sidechain 1")), "inactive band disappears");
    AudioUnitRemovePropertyListenerWithUserData (unit, kAudioUnitProperty_ElementCount, changed, &changes);
    AudioUnitUninitialize (unit);
    AudioComponentInstanceDispose (unit);
}
