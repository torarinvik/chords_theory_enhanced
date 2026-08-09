#include "Theory/Degree.h"

#include <cctype>
#include <stdexcept>

namespace theory
{

std::string getDegreeLabel(Degree degree)
{
    switch (degree)
    {
        case Degree::I:   return "I";
        case Degree::II:  return "II";
        case Degree::III: return "III";
        case Degree::IV:  return "IV";
        case Degree::V:   return "V";
        case Degree::VI:  return "VI";
        case Degree::VII: return "VII";
    }

    return "I";
}

std::string formatRomanNumeral(Degree degree, RomanQualityHint quality)
{
    const char* majorForms[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    const char* minorForms[] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    const int idx = static_cast<int>(degree);
    if (idx < 0 || idx > 6)
        return "I";

    switch (quality)
    {
        case RomanQualityHint::MajorLike:
        case RomanQualityHint::Dominant:
            return majorForms[idx];
        case RomanQualityHint::MinorLike:
            return minorForms[idx];
        case RomanQualityHint::DimLike:
            return std::string(minorForms[idx]) + "o"; // ascii dim mark (portable)
    }
    return majorForms[idx];
}

Degree parseDegree(const std::string& label)
{
    // Strip dim suffix and compare case-insensitively for roman bodies.
    std::string body;
    body.reserve(label.size());
    for (char c : label)
    {
        if (c == 'o' || c == 'O' || static_cast<unsigned char>(c) > 127)
            break; // stop at ° (utf-8) or ascii 'o' dim mark
        if (std::isalpha(static_cast<unsigned char>(c)))
            body.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    if (body == "I")   return Degree::I;
    if (body == "II")  return Degree::II;
    if (body == "III") return Degree::III;
    if (body == "IV")  return Degree::IV;
    if (body == "V")   return Degree::V;
    if (body == "VI")  return Degree::VI;
    if (body == "VII") return Degree::VII;

    throw std::invalid_argument("Unknown degree label: " + label);
}

}
