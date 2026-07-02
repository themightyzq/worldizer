#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

namespace Worldizer
{
/**
    Looped playback of a room-tone bed, mixed under the worldized signal.

    The shipped beds are baked seam-blended (see Tools/bake_assets), so runtime
    looping is a plain position wrap — no per-loop crossfade cost. Bed CHANGES
    (preset switch) crossfade old->new over ~250 ms on the audio thread.

    Lock-free bed swap: a fixed pool of 4 slots. The message thread claims a Free
    slot (CAS), fills it (allocation happens there), and publishes it via an
    atomic one-deep pending index — rapid preset switches converge to the latest.
    The audio thread picks the pending slot up when it is not already fading,
    crossfades, and releases the outgoing slot back to Free. The audio thread
    never allocates, frees, locks, or waits; the message thread never touches a
    slot the audio thread is reading.

    Beds are resampled to the host rate at load time (message thread) so playback
    is pitch-true at any sample rate.
*/
class AmbientBed
{
public:
    AmbientBed() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Sets the room-tone bed (message thread). Mono beds feed all channels.
        The buffer is copied/resampled into a pool slot. */
    void setSample (const juce::AudioBuffer<float>& sample, double sourceSampleRate);

    /** Removes the bed (crossfades to silence). Message thread. */
    void clearSample();

    /** Linear mix gain, 0..1 (0 = off, bit-exact skip once faded). Any thread. */
    void setLevel (float gain);

    /** Adds the bed into the buffer in place. Audio thread; real-time safe. */
    void addToBuffer (juce::AudioBuffer<float>& buffer);

private:
    enum class SlotState : int { Free = 0, Loading, Pending, Active, Fading };

    struct Slot
    {
        juce::AudioBuffer<float> buffer;   // empty => silence (the "no bed" slot)
        std::atomic<SlotState> state { SlotState::Free };
    };

    int  claimFreeSlot();                  // message thread; -1 if none free
    void publishSlot (int slotIndex);      // message thread

    static constexpr int kNumSlots = 4;
    std::array<Slot, kNumSlots> slots;
    std::atomic<int> pendingSlot { -1 };

    // Audio-thread-only playback state.
    int activeSlot   = -1;  // -1 => silence
    int fadingSlot   = -1;  // outgoing during a crossfade
    int activePos    = 0;
    int fadingPos    = 0;
    int fadeRemaining = 0;
    int fadeTotal     = 0;

    std::atomic<float> levelTarget { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> levelSmoothed;

    double sampleRate = 48000.0;
    std::atomic<bool> prepared { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmbientBed)
};
} // namespace Worldizer
