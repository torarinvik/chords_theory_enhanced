#pragma once

#include <string>

namespace theory
{

enum class Degree
{
    I, II, III, IV, V, VI, VII
};

constexpr int kNumDegrees = 7;

// Matches chords.json's per-scale degree key, e.g. Degree::IV -> "IV" (always uppercase).
// Use formatRomanNumeral() for quality-aware functional display (ii, V, vii°, V/ii).
std::string getDegreeLabel(Degree degree);

// Inverse of getDegreeLabel(). Throws std::invalid_argument if label isn't one of "I".."VII".
// Also accepts lowercase roman forms (ii, iii, vi, vii).
Degree parseDegree(const std::string& label);

// Functional roman numeral with chord-quality case: I, ii, iii, IV, V, vi, vii° / VII.
// majorLike: uppercase triad (I, IV, V); minorLike: lowercase (ii, iii, vi); dimLike: vii°.
enum class RomanQualityHint { MajorLike, MinorLike, DimLike, Dominant };
std::string formatRomanNumeral(Degree degree, RomanQualityHint quality);

}
