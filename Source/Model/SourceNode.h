#pragma once

#include <JuceHeader.h>

namespace wz
{
/**
    A sound source within a Scene — the reproducer (speaker) the input audio is
    "played through". Carries position, orientation, and the selected speaker
    character (an id into the speaker IR library).
*/
struct SourceNode
{
    juce::Vector3D<float> position {}; // metres
    float yaw   = 0.0f;                // degrees
    float pitch = 0.0f;                // degrees

    juce::String speakerCharacter;     // id into Resources/Speakers
};
} // namespace wz
