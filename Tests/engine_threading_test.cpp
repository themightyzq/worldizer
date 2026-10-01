/**
    EngineThreadingTest — the IR hand-off between loading threads and the audio
    thread. Regression test for the pluginval abort
    "terminating due to uncaught exception of type std::bad_function_call".

    Each juce::dsp::Convolution owns a single-producer command queue. When two
    threads push to it at once (a loader in loadImpulseResponse, the host thread
    in reset() from releaseResources, a second loader) the FIFO commits a slot
    nobody wrote, and JUCE's loader thread calls an empty FixedSizeFunction,
    which throws and terminates the process. So a failure here is usually not a
    FAIL line but an abort: CTest reports it as a failed test either way.

      1. Room engine storm: an "audio" thread processing, two loaders using the
         check-then-load pattern (render thread + a second loading thread), and a
         "host" thread that parks the audio thread and calls reset() / prepare()
         the way releaseResources / prepareToPlay do. Afterwards the engine must
         still load and play a known IR.
      2. Character stage storm: SourceCharacter::setIR from two threads (the
         message-thread timer and a host thread in prepareToPlay) plus host
         prepare / reset, with audio running.
      3. Host stops processing: with an unconsumed swap queued, a scene render
         must still finish and clear the "rendering..." indicator, and the latest
         requested IR must be the one that plays once audio resumes.

    The storms are probabilistic (see the report numbers in the run log); the
    fixed engine must survive the whole run. The test uses only the API the
    pre-fix engine also had, so it can be built against the old sources to show
    the failure. Returns 0 on pass / nonzero on fail (CTest).
*/
#include <JuceHeader.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include "../Source/DSP/ConvolutionEngine.h"
#include "../Source/DSP/SourceCharacter.h"
#include "../Source/Plugin/RenderThread.h"
#include "../Source/Model/TestScenes.h"

using namespace Worldizer;
using Clock = std::chrono::steady_clock;

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "  PASS: " : "  FAIL: ") << what << std::endl;
    if (! condition)
        ++failures;
}

double secondsSince (Clock::time_point t)
{
    return std::chrono::duration<double> (Clock::now() - t).count();
}

// Unit tap at 0 plus a 0.5 tap at `tap`: distinguishable after Trim/Normalise.
juce::AudioBuffer<float> twoTapIR (int tap)
{
    juce::AudioBuffer<float> ir (1, tap + 1);
    ir.clear();
    ir.setSample (0, 0, 1.0f);
    ir.setSample (0, tap, 0.5f);
    return ir;
}

/** Lets the audio side be paused by a "host" thread, so reset()/prepare() run
    only while process() is not running (the real host contract). */
struct AudioGate
{
    std::atomic<bool> pauseRequested { false };
    std::atomic<bool> parked { false };
    std::atomic<bool> stop { false };

    // Audio thread: returns false when the loop should exit.
    bool waitIfPaused()
    {
        if (! pauseRequested.load())
            return ! stop.load();
        parked.store (true);
        while (pauseRequested.load() && ! stop.load())
            std::this_thread::yield();
        parked.store (false);
        return ! stop.load();
    }

    // Host thread.
    template <typename Fn>
    void whileParked (Fn&& fn)
    {
        pauseRequested.store (true);
        while (! parked.load() && ! stop.load())
            std::this_thread::yield();
        fn();
        pauseRequested.store (false);
        while (parked.load() && ! stop.load())
            std::this_thread::yield();
    }
};

/** Runs a storm for `seconds`, extended (up to 4x) until the host thread has
    completed at least `minCycles` park/reset/prepare cycles, so a heavily
    loaded machine still exercises the host side. */
void runStorm (double seconds, const std::atomic<int>& cycles, int minCycles)
{
    const auto start = Clock::now();
    while (secondsSince (start) < seconds
           || (cycles.load() < minCycles && secondsSince (start) < 4.0 * seconds))
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
}

void fillNoise (juce::AudioBuffer<float>& b, juce::Random& rng)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (ch, i, 0.1f * (rng.nextFloat() * 2.0f - 1.0f));
}

/** Pumps silence until the engine has settled on its latest IR (no swap or
    crossfade pending, and JUCE's own loader + 50 ms internal crossover done),
    then returns the engine's response to a unit impulse on channel 0. */
std::vector<float> settledImpulseResponse (ConvolutionEngine& engine, int blockSize, int length)
{
    juce::AudioBuffer<float> buf (2, blockSize);
    const auto start = Clock::now();
    auto idleSince = Clock::now();
    bool wasIdle = false;
    while (secondsSince (start) < 5.0)
    {
        buf.clear();
        juce::dsp::AudioBlock<float> ab (buf);
        engine.process (ab);
        const bool idle = ! engine.isIRPending();
        if (idle && ! wasIdle) idleSince = Clock::now();
        wasIdle = idle;
        if (idle && secondsSince (idleSince) > 0.4)
            break;
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    std::vector<float> out;
    for (int block = 0; (int) out.size() < length; ++block)
    {
        buf.clear();
        if (block == 0)
        {
            buf.setSample (0, 0, 1.0f);
            buf.setSample (1, 0, 1.0f);
        }
        juce::dsp::AudioBlock<float> ab (buf);
        engine.process (ab);
        for (int i = 0; i < blockSize && (int) out.size() < length; ++i)
            out.push_back (buf.getSample (0, i));
    }
    return out;
}

bool hasTapAt (const std::vector<float>& r, int tap)
{
    const float ref = std::abs (r[0]);
    return ref > 1.0e-3f && std::abs (r[(size_t) tap] / ref - 0.5f) < 0.02f;
}

//==============================================================================
void roomEngineStorm (double seconds, int numLoaders)
{
    std::cout << "\n[1] Room engine: " << numLoaders << " loader(s) + host reset/prepare + audio, "
              << seconds << " s" << std::endl;

    ConvolutionEngine engine;
    engine.prepare (44100.0, 512, 2);

    AudioGate gate;
    std::atomic<int> blocks { 0 }, loads { 0 }, cycles { 0 };
    std::atomic<int> blockSize { 512 }; // host changes it only while audio is parked

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buf (2, 1024);
        juce::Random rng (1);
        while (gate.waitIfPaused())
        {
            fillNoise (buf, rng);
            auto ab = juce::dsp::AudioBlock<float> (buf).getSubBlock (0, (size_t) blockSize.load());
            engine.process (ab);
            ++blocks;
        }
    });

    auto loader = [&] (int seed)
    {
        juce::Random rng (seed);
        while (! gate.stop.load())
        {
            // The pre-fix callers' pattern: wait for the idle convolver, then load.
            while (engine.isIRPending() && ! gate.stop.load())
                std::this_thread::yield();
            auto ir = twoTapIR (8 + rng.nextInt (120));
            engine.loadIR (ir, 44100.0, 1.0f);
            ++loads;
        }
    };
    std::vector<std::thread> loaders;
    for (int i = 0; i < numLoaders; ++i)
        loaders.emplace_back (loader, 2 + i);

    std::thread host ([&]
    {
        const double rates[] = { 44100.0, 48000.0, 96000.0 };
        const int    sizes[] = { 64, 128, 256, 512, 1024 };
        int i = 0;
        while (! gate.stop.load())
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
            gate.whileParked ([&]
            {
                engine.reset();                                    // releaseResources
                engine.prepare (rates[i % 3], sizes[i % 5], 2);   // prepareToPlay
                blockSize.store (sizes[i % 5]);
            });
            ++i;
            ++cycles;
        }
    });

    runStorm (seconds, cycles, 30);
    gate.stop.store (true);
    audio.join(); host.join();
    for (auto& t : loaders) t.join();

    std::cout << "  blocks " << blocks.load() << ", loads " << loads.load()
              << ", host cycles " << cycles.load() << std::endl;
    check (true, "no std::bad_function_call / abort under concurrent loaders, reset and prepare");
    check (loads.load() > 100 && cycles.load() >= 10, "the storm actually exercised loads and host cycles");

    // The engine must still work: load a known IR and hear exactly it.
    engine.prepare (48000.0, 256, 2);
    engine.loadIR (twoTapIR (57), 48000.0, 5.0f, /*normalise*/ false);
    const auto r = settledImpulseResponse (engine, 256, 128);
    check (std::abs (r[0] - 1.0f) < 0.02f && hasTapAt (r, 57),
           "after the storm the engine plays the loaded IR (r[0]=" + juce::String (r[0], 3)
           + ", r[57]=" + juce::String (r[57], 3) + ")");
}

//==============================================================================
void characterStorm (double seconds)
{
    std::cout << "\n[2] SourceCharacter: setIR from two threads + host prepare/reset + audio, "
              << seconds << " s" << std::endl;

    SourceCharacter character;
    const juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    character.prepare (spec);

    AudioGate gate;
    std::atomic<int> sets { 0 }, cycles { 0 };

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buf (2, 512);
        juce::Random rng (4);
        while (gate.waitIfPaused())
        {
            fillNoise (buf, rng);
            juce::dsp::AudioBlock<float> ab (buf);
            character.process (ab);
        }
    });

    // Stand-ins for the message-thread character reload (timer) and the host
    // thread's loadCharactersFromParams() in prepareToPlay.
    auto setter = [&] (int seed)
    {
        juce::Random rng (seed);
        while (! gate.stop.load())
        {
            if (rng.nextInt (4) == 0)
                character.setNone();
            else
                character.setIR (twoTapIR (8 + rng.nextInt (120)), 48000.0);
            ++sets;
            // Brief gap so the host thread can win pendingLock (the lock is not
            // fair); the message-thread timer it stands in for runs every 15 ms.
            std::this_thread::sleep_for (std::chrono::microseconds (50));
        }
    };
    std::thread setterA (setter, 5), setterB (setter, 6);

    std::thread host ([&]
    {
        while (! gate.stop.load())
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (3));
            gate.whileParked ([&]
            {
                character.reset();
                character.prepare (spec);
            });
            ++cycles;
        }
    });

    runStorm (seconds, cycles, 30);
    gate.stop.store (true);
    audio.join(); setterA.join(); setterB.join(); host.join();

    std::cout << "  setIR/setNone calls " << sets.load() << ", host cycles " << cycles.load() << std::endl;
    check (true, "no abort with character loads racing host prepare/reset");
    check (sets.load() > 100 && cycles.load() >= 10, "the storm actually exercised loads and host cycles");
}

//==============================================================================
void hostStopsProcessing()
{
    std::cout << "\n[3] Host not calling processBlock: render completes, latest IR wins" << std::endl;

    ConvolutionEngine engine;
    engine.prepare (48000.0, 512, 2);
    RenderThread renderThread (engine);

    // A baked IR is handed to the engine, but no audio runs to consume the swap.
    renderThread.requestIRLoad (twoTapIR (40), 48000.0, 5.0f);
    auto t0 = Clock::now();
    while (! engine.isIRPending() && secondsSince (t0) < 2.0)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    check (engine.isIRPending(), "baked IR queued on the engine (swap waiting for audio)");

    // A scene edit triggers a full render while the host is still stopped.
    RenderThread::Job job;
    job.scene = TestScenes::smallConcreteRoom();
    job.quality = RenderThread::Job::Quality::Full;
    renderThread.requestRender (job);

    t0 = Clock::now();
    while (renderThread.getLastFullRender().ir.getNumSamples() == 0 && secondsSince (t0) < 30.0)
        std::this_thread::sleep_for (std::chrono::milliseconds (5));
    check (renderThread.getLastFullRender().ir.getNumSamples() > 0, "the scene trace finished");

    t0 = Clock::now();
    while (renderThread.isRendering() && secondsSince (t0) < 2.0)
        std::this_thread::sleep_for (std::chrono::milliseconds (5));
    check (! renderThread.isRendering(),
           "\"rendering...\" indicator clears with no audio running (was stuck forever)");

    // The newest request must be what plays when audio resumes.
    renderThread.requestIRLoad (twoTapIR (90), 48000.0, 5.0f);
    std::this_thread::sleep_for (std::chrono::milliseconds (300));

    const auto r = settledImpulseResponse (engine, 512, 128);
    check (hasTapAt (r, 90) && std::abs (r[40] / juce::jmax (1.0e-6f, std::abs (r[0]))) < 0.02f,
           "latest requested IR plays once audio resumes (r[40]/r[0]="
           + juce::String (r[40] / juce::jmax (1.0e-6f, std::abs (r[0])), 3) + ", r[90]/r[0]="
           + juce::String (r[90] / juce::jmax (1.0e-6f, std::abs (r[0])), 3) + ")");
}
} // namespace

int main (int argc, char* argv[])
{
    // SourceCharacter is a juce::Timer; a MessageManager must exist (its
    // callbacks are never dispatched here, which is fine: the test drives the
    // pending-slot flush through setIR itself).
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Optional: [seconds] [room loaders] [parts, e.g. "13"]. One room loader +
    // the host thread is the plugin's real render-thread vs
    // releaseResources/prepareToPlay case.
    const double seconds = argc > 1 ? juce::String (argv[1]).getDoubleValue() : 3.0;
    const int roomLoaders = argc > 2 ? juce::jlimit (1, 4, juce::String (argv[2]).getIntValue()) : 2;
    const juce::String parts = argc > 3 ? juce::String (argv[3]) : juce::String ("123");

    if (parts.containsChar ('1')) roomEngineStorm (seconds, roomLoaders);
    if (parts.containsChar ('2')) characterStorm (seconds);
    if (parts.containsChar ('3')) hostStopsProcessing();

    std::cout << (failures == 0 ? "\nALL PASS\n" : "\n" + juce::String (failures) + " FAILURES\n");
    return failures == 0 ? 0 : 1;
}
