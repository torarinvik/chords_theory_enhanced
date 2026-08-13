#include "Theory/ChordDisplay.h"

#include "Theory/ChordDatabase.h"
#include "Theory/NextChordScorer.h"

namespace theory
{

std::string romanForChordInKeyScale(const Chord& chord, Key key, Scale scale)
{
    if (chord.notes.empty())
        return {};

    const auto& keyScale = ChordDatabase::getInstance().get(key, scale);
    return NextChordScorer::romanForChord(chord, keyScale);
}

std::string formatAbsoluteWithRoman(const Chord& chord, Key key, Scale scale,
                                    const std::string& absoluteNameFallback)
{
    const std::string absolute = !absoluteNameFallback.empty()
        ? absoluteNameFallback
        : (!chord.readableName.empty() ? chord.readableName : chord.symbol);

    if (absolute.empty())
        return romanForChordInKeyScale(chord, key, scale);

    const auto roman = romanForChordInKeyScale(chord, key, scale);
    if (roman.empty())
        return absolute;

    return absolute + " · " + roman;
}

}
