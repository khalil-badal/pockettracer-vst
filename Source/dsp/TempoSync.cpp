#include "TempoSync.h"

namespace
{
    // Base note as a fraction of a whole note, plus modifier.
    struct Division { const char* name; double base; double modifier; };

    // Ordered short -> long.  base = straight value; modifier: 1.0 straight,
    // 1.5 dotted (D), 2/3 triplet (T).
    const Division kDivisions[] =
    {
        { "1/32",  1.0 / 32.0, 1.0      },
        { "1/32D", 1.0 / 32.0, 1.5      },
        { "1/32T", 1.0 / 32.0, 2.0/3.0  },
        { "1/16",  1.0 / 16.0, 1.0      },
        { "1/16D", 1.0 / 16.0, 1.5      },
        { "1/16T", 1.0 / 16.0, 2.0/3.0  },
        { "1/8",   1.0 / 8.0,  1.0      },
        { "1/8D",  1.0 / 8.0,  1.5      },
        { "1/8T",  1.0 / 8.0,  2.0/3.0  },
        { "1/4",   1.0 / 4.0,  1.0      },   // index 9 = default
        { "1/4D",  1.0 / 4.0,  1.5      },
        { "1/4T",  1.0 / 4.0,  2.0/3.0  },
        { "1/2",   1.0 / 2.0,  1.0      },
        { "1/2D",  1.0 / 2.0,  1.5      },
        { "1/2T",  1.0 / 2.0,  2.0/3.0  },
        { "1/1",   1.0,        1.0      },   // one bar (4/4)
    };

    constexpr int kNumDivisions = (int) (sizeof (kDivisions) / sizeof (kDivisions[0]));
}

juce::StringArray TempoSync::getDivisionNames()
{
    juce::StringArray names;
    for (int i = 0; i < kNumDivisions; ++i)
        names.add (kDivisions[i].name);
    return names;
}

double TempoSync::getWholeNoteFraction (int divisionIndex)
{
    divisionIndex = juce::jlimit (0, kNumDivisions - 1, divisionIndex);
    const auto& d = kDivisions[divisionIndex];
    return d.base * d.modifier;
}

double TempoSync::divisionToMs (int divisionIndex, double bpm)
{
    if (bpm <= 0.0)
        bpm = kFallbackBpm;

    const double quarterNoteMs = 60000.0 / bpm;     // ms per quarter note
    const double wholeNotes    = getWholeNoteFraction (divisionIndex);
    return quarterNoteMs * 4.0 * wholeNotes;        // 4 quarters per whole note
}
