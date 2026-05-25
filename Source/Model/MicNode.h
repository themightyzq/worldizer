#pragma once

#include <JuceHeader.h>

namespace wz
{
/**
    A microphone within a Scene. Carries position, orientation, polar pattern,
    and the selected mic character (an id into the mic IR library). Multiple
    MicNodes form a mic array (multi-mic is post-MVP; MVP renders stereo).
*/
struct MicNode
{
    juce::Vector3D<float> position {}; // metres
    float yaw   = 0.0f;                // degrees
    float pitch = 0.0f;                // degrees

    juce::String polarPattern { "omni" };
    juce::String micCharacter;         // id into Resources/Mics
};
} // namespace wz
