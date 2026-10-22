#pragma once
#include <JuceHeader.h>

// Converts musical note divisions to delay time in milliseconds, given a BPM.
// Single source of truth for the synced-division list (used by the APVTS
// choice parameter AND the time calculation, so they can never drift apart).
class TempoSync
{
public:
    // Ordered short -> long. Index 9 ("1/4") is the sensible default.
    static juce::StringArray getDivisionNames();
    static int getDefaultDivisionIndex() { return 9; } // "1/4"

    // Length of one division as a fraction of a whole note (e.g. 1/4 -> 0.25,
    // dotted = x1.5, triplet = x2/3).
    static double getWholeNoteFraction (int divisionIndex);

    // delay time in ms for a division at a given tempo.
    static double divisionToMs (int divisionIndex, double bpm);

    static constexpr double kFallbackBpm = 120.0;
};
