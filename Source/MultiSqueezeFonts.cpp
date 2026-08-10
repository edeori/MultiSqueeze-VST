#include "MultiSqueezeFonts.h"
#include "BinaryData.h"

namespace MultiSqueezeFonts
{
    namespace
    {
        juce::Font make (const void* data, int size, float height)
        {
            auto face = juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
            return juce::Font (juce::FontOptions (face)).withHeight (height);
        }
    }

    juce::Font title (float size) { return make (BinaryData::OxaniumBold_ttf, BinaryData::OxaniumBold_ttfSize, size); }
    juce::Font uiLight (float size) { return make (BinaryData::BarlowCondensedLight_ttf, BinaryData::BarlowCondensedLight_ttfSize, size); }
    juce::Font uiMedium (float size) { return make (BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize, size); }
    juce::Font numeric (float size) { return make (BinaryData::RajdhaniMedium_ttf, BinaryData::RajdhaniMedium_ttfSize, size); }
}
