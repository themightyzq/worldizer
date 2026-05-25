#pragma once

#include <JuceHeader.h>

namespace Worldizer
{
    // Product identity — locked in
    constexpr auto kProductName        = "Worldizer";
    constexpr auto kCompanyName        = "ZQSFX";
    constexpr auto kCompanyWebsite     = "https://github.com/zqsfx/worldizer";
    constexpr auto kBundleId           = "com.zqsfx.worldizer";
    constexpr auto kPluginManufCode    = "ZQSF";
    constexpr auto kPluginCode         = "Wrld";

    // Version
    constexpr int  kVersionMajor       = 0;
    constexpr int  kVersionMinor       = 0;
    constexpr int  kVersionPatch       = 1;
    constexpr auto kVersionString      = "0.0.1";

    // Window
    constexpr int  kDefaultWindowWidth  = 900;
    constexpr int  kDefaultWindowHeight = 650;
    constexpr int  kMinWindowWidth      = 700;
    constexpr int  kMinWindowHeight     = 550;
    constexpr int  kMaxWindowWidth      = 1400;
    constexpr int  kMaxWindowHeight     = 1000;

    // DSP
    constexpr int  kMaxIRLengthSeconds  = 6;
    constexpr int  kIRBands             = 8;  // octave bands: 31.25, 62.5, 125, 250, 500, 1k, 2k, 4k, 8k Hz

    // Ray tracer quality
    constexpr int  kRaysPreview         = 1000;
    constexpr int  kRaysFull            = 20000;
    constexpr int  kMaxBouncesPreview   = 8;
    constexpr int  kMaxBouncesFull      = 32;
}
