#pragma once
#include <J-Core/IO/ImageUtils.h>
#include <J-Core/IO/FileStream.h>
#include <nlohmann/json.hpp>
#include <J-Core/Util/EnumUtils.h>
#include <J-Core/IO/IOUtils.h>
#include <J-Core/Util/Bitset.h>

namespace Projections {

    enum ImageUtilFlags : uint8_t {
        IMG_UTIL_FLG_APPLY_RGB = 0x1,
        IMG_UTIL_FLG_APPLY_ALPHA = 0x2,
        IMG_UTIL_FLG_APPLY_OVERWRITE = 0x4,
        IMG_UTIL_FLG_TO_RGB4444 = 0x8,
    };

    struct ImageUtilSettings {
        ImageUtilFlags flags{ ImageUtilFlags(IMG_UTIL_FLG_APPLY_RGB | IMG_UTIL_FLG_APPLY_ALPHA | IMG_UTIL_FLG_APPLY_OVERWRITE) };

        void reset() {
            flags = ImageUtilFlags(IMG_UTIL_FLG_APPLY_RGB | IMG_UTIL_FLG_APPLY_ALPHA | IMG_UTIL_FLG_APPLY_OVERWRITE);
        }
        constexpr bool applyRGB() const { return bool(flags & IMG_UTIL_FLG_APPLY_RGB); }
        constexpr bool applyAlpha() const { return bool(flags & IMG_UTIL_FLG_APPLY_ALPHA); }
        constexpr bool doOverwrite() const { return bool(flags & IMG_UTIL_FLG_APPLY_OVERWRITE); }
        constexpr bool convertTo4Bit() const { return bool(flags & IMG_UTIL_FLG_TO_RGB4444); }

        void write(json& jsonF) const {
            jsonF["flags"] = int32_t(flags);
        }

        void read(json& jsonF) {
            flags = ImageUtilFlags(jsonF.value("flags", int32_t(IMG_UTIL_FLG_APPLY_RGB | IMG_UTIL_FLG_APPLY_ALPHA | IMG_UTIL_FLG_APPLY_OVERWRITE)));
        }
    };

    struct ColorToAlphaSettings {
        bool premultiply{};
        int32_t diffFloor = 0x00;
        int32_t diffCeil = 0xFF;
        JCore::Color32 refColor{};
        JCore::ImageData* output{ nullptr };
    };

    struct AlphaEdgeFadeSettings {
        int32_t minDist = 0x00;
        int32_t maxDist = 0xFF;
        JCore::Color32 refColor{ JCore::Color32::White };

        void reset() {
            minDist = 0x00;
            maxDist = 0xFF;
            refColor = JCore::Color32::White;
        }
    };

    struct ChromaKeySettings {
        bool premultiply{};
        JCore::Color32 refColor{};
        float minDifference{ 0 };
        float maxDifference{ 0.25f };

        void reset() {
            refColor = JCore::Color32::Green;
            minDifference = 0.0f;
            maxDifference = 0.25f;
            premultiply = false;
        }
    };

    struct ColorReductionSettings {
        bool applyDithering{true};
        uint8_t maxDepth{8};
        JCore::ImageData* output{ nullptr };
    };

    struct AlphaReductionSettings {
        bool applyDithering{true};
        uint8_t maxDepth{8};
    };
       
    struct AlphaMaskSettings {
        bool applyDithering{true};
        uint8_t maxDepth{8};
    };

    struct TemporalLowpassSettings {
        float coefficient{0.5f};
    };

    enum CrossFadeType : uint8_t {
        CROSS_Linear,
        CROSS_Quadratic,
        CROSS_Cubic,
    };

    struct CrossFadeSettings {
        CrossFadeType type{};
        int32_t startFrame {0};
        float frameRate {24};
        float crossfadeDuration {0.5f};

        void reset() {
            type = CROSS_Linear;
            startFrame = 0;
            frameRate = 24;
            crossfadeDuration = 0.5f;
        }
    };

    struct FadeSettings {
        CrossFadeType type{};
        int32_t startFrame{ 0 };
        int32_t fadeFrames{ 8 };

        void reset() {
            type = CROSS_Linear;
            startFrame = 0;
            fadeFrames = 8;
        }
    };

    enum MorphType : uint8_t {
        MORPH_4_Diagonal,
        MORPH_4_Cardinal,
        MORPH_8_Direction,
    };

    struct MorphPass {
        bool dilate{ false };
        MorphType type{ MORPH_8_Direction };
        int32_t numOfTimes{ 0 };
        bool invert{ false };

        void reset() {
            dilate = false;
            type = MORPH_8_Direction;
            numOfTimes = 0;
            invert = false;
        }
    };

    struct MorphologicalSettings {
        static constexpr size_t MAX_PASSES = 12;
        int32_t minR{0};
        int32_t maxR{ 0x100 };

        int32_t minG{ 0 };
        int32_t maxG{ 0x100 };

        int32_t minB{ 0 };
        int32_t maxB{ 0x100 };

        MorphPass passes[MAX_PASSES]{};

        void reset() {
            minR = 0x00;
            maxR = 0x100;
            
            minG = 0x00;
            maxG = 0x100;
            
            minB = 0x00;
            maxB = 0x100;
            for (size_t i = 0; i < MAX_PASSES; i++)  {
                passes[i].reset();
            }
        }
    };

    bool doColorToAlpha(JCore::ImageData& input, ColorToAlphaSettings& settings);
    bool doColorReduction(const JCore::ImageData& input, bool toRGB444, const ColorReductionSettings& settings, bool applyRGB, const AlphaReductionSettings* alphaSettings = nullptr);

    bool doTemporalLowpass(JCore::ImageData& prev, JCore::ImageData& current, const TemporalLowpassSettings& settings);
    bool doMorphPasses(JCore::Bitset& bits, JCore::ImageData& frame, MorphologicalSettings& settings);
    bool doCrossFade(const JCore::ImageData& lhs, const JCore::ImageData& rhs, float time, JCore::ImageData& output, CrossFadeSettings& settings);
    bool doFade(const JCore::ImageData& lhs, float time, JCore::ImageData& output, FadeSettings& settings);
    bool doEdgeFade(const JCore::ImageData& lhs, JCore::ImageData& output, AlphaEdgeFadeSettings& settings);

    bool doChromaKey(const JCore::ImageData& image, JCore::ImageData& output, const ChromaKeySettings& chroma, const JCore::ImageData* andMask = nullptr, const JCore::ImageData* orMask = nullptr);

    bool doGrayToAlpha(JCore::ImageData& output);
    bool doAlphaMask(const JCore::ImageData& input, const JCore::ImageData& mask, JCore::ImageData& output);
    bool doImageMask(int32_t minDist, int32_t maxDist, const JCore::ImageData& input, const JCore::ImageData& mask, JCore::ImageData& output);
}

namespace JCore {
    template<>
    inline constexpr int32_t EnumNames<Projections::ImageUtilFlags>::Count{ 4 };

    template<>
    inline const char** EnumNames<Projections::ImageUtilFlags>::getEnumNames() {
        static const char* names[EnumNames<Projections::ImageUtilFlags>::Count] =
        {
            "Apply RGB",
            "Apply Alpha",
            "Overwrite",
            "Convert To RGBA4444",
        };
        return names;
    }


    template<>
    inline constexpr int32_t EnumNames<Projections::MorphType>::Count{ 3 };

    template<>
    inline const char** EnumNames<Projections::MorphType>::getEnumNames() {
        static const char* names[EnumNames<Projections::MorphType>::Count] =
        {
            "Diagonal (4)",
            "Cardinal (4)",
            "All Directions (8)",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::CrossFadeType>::Count{ 3 };

    template<>
    inline const char** EnumNames<Projections::CrossFadeType>::getEnumNames() {
        static const char* names[EnumNames<Projections::CrossFadeType>::Count] =
        {
            "Linear",
            "Quadratic",
            "Cubic",
        };
        return names;
    }
}