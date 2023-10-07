#include <ImageUtils/ImageUtilsTypes.h>
#include <J-Core/IO/ImageUtils.h>
#include <algorithm>
#include <J-Core/Log.h>
using namespace JCore;

namespace Projections {
    struct Gray {
        uint8_t value{};
        uint32_t instances{};
    };

    bool doColorToAlpha(ImageData& input, ColorToAlphaSettings& settings) {
        if (!input.data || !settings.output) {
            JCORE_ERROR("[ImageUtils] Error: Input data was null! ({0} - {1})", input.data ? "OK" : "NULL", settings.output ? "OK" : "NULL");
            return false;
        }
        auto& outImg = *settings.output;

        outImg.format = TextureFormat::RGBA32;

        outImg.width = input.width;
        outImg.height = input.height;
        if (!outImg.doAllocate()) { return false; }

        Color32* output = reinterpret_cast<Color32*>(outImg.data);
        size_t reso = size_t(input.width) * input.height;
        size_t bpp = getBitsPerPixel(input.format) >> 3;

        static constexpr float BYTE_TO_FLOAT = 1.0f / 255.0f;

        float mA = settings.diffFloor * BYTE_TO_FLOAT;
        float mX = settings.diffCeil * BYTE_TO_FLOAT;

        float temp[4]{0};
        float refR = float(settings.refColor.r * BYTE_TO_FLOAT), refG = float(settings.refColor.g * BYTE_TO_FLOAT), refB = float(settings.refColor.b * BYTE_TO_FLOAT);
        for (size_t i = 0, j = 0; i < reso; i++, j += bpp) {
            auto& cPix = output[i];
            convertPixel(input.format, input.data, input.data + j, cPix);

            colorToAlpha(cPix, refR, refG, refB, mA, mX);
            if (settings.premultiply) {
                premultiplyC32(cPix);
            }
        }
        return true;
    }

    bool sortGrayValues(const Gray& lhs, const Gray& rhs) {
        return rhs.instances < lhs.instances;
    }

    void updateDitherPixel(Color32* pixels, int32_t x, int32_t y, int32_t width, int32_t height, const int32_t* error, bool applyRgb, bool applyAlpha, float errorBias) {
        if (x < 0 || x >= width || y < 0 || y >= height) { return; }
        Color32& pixel = pixels[y * width + x];
        int32_t k[4]{ pixel.r,pixel.g,pixel.b,pixel.a };

        if (applyRgb) {
            k[0] += int32_t(error[0] * errorBias);
            k[1] += int32_t(error[1] * errorBias);
            k[2] += int32_t(error[2] * errorBias);

            pixel.r = uint8_t(std::clamp(k[0], 0, 255));
            pixel.g = uint8_t(std::clamp(k[1], 0, 255));
            pixel.b = uint8_t(std::clamp(k[2], 0, 255));
        }

        if (applyAlpha) {
            k[3] += int32_t(error[3] * errorBias);
            pixel.a = uint8_t(std::clamp(k[3], 0, 255));
        }
    }
    void updateDitherPixel(Color4444* pixels, int32_t x, int32_t y, int32_t width, int32_t height, const int32_t* error, bool applyRgb, bool applyAlpha, float errorBias) {
        if (x < 0 || x >= width || y < 0 || y >= height) { return; }
        Color4444& pixel = pixels[y * width + x];
        int32_t k[4]{ remapUI4ToUI8(pixel.getR()),remapUI4ToUI8(pixel.getG()),remapUI4ToUI8(pixel.getB()),remapUI4ToUI8(pixel.getA()) };

        if (applyRgb) {
            k[0] += int32_t(error[0] * errorBias);
            k[1] += int32_t(error[1] * errorBias);
            k[2] += int32_t(error[2] * errorBias);

            pixel.setR(uint16_t(std::clamp(k[0], 0, 0xFF) >> 4));
            pixel.setG(uint16_t(std::clamp(k[1], 0, 0xFF) >> 4));
            pixel.setB(uint16_t(std::clamp(k[2], 0, 0xFF) >> 4));
        }

        if (applyAlpha) {
            k[3] += int32_t(error[3] * errorBias);
            pixel.setA(uint16_t(uint8_t(std::clamp(k[3], 0, 0xFF) >> 4)));
        }
    }

    void doFloydDitheringGray(Color32* pixels, int32_t width, int32_t height, uint8_t colorBits, uint8_t alphaBits) {
        int32_t reso = width * height;

        bool applyRGB = colorBits < 8;
        bool appyAlpha = alphaBits < 8;
        int32_t error[4]{ 0 };
        Color32 quant{};
        for (int32_t y = 0, yP = 0; y < height; y++, yP += width) {
            for (int32_t x = 0; x < width; x++) {
                auto& pix = pixels[yP + x];
                quant = pix;
                if (applyRGB) {
                    quant.r = quantizeUI8(pix.r, colorBits);
                    quant.g = quantizeUI8(pix.g, colorBits);
                    quant.b = quantizeUI8(pix.b, colorBits);

                    error[0] = pix.r - quant.r;
                    error[1] = pix.g - quant.g;
                    error[2] = pix.b - quant.b;
                }

                if (appyAlpha) {
                    quant.a = quantizeUI8(pix.a, alphaBits);
                    error[3] = pix.a - quant.a;
                }
                pix = quant;

                updateDitherPixel(pixels, x + 1, y, width, height, error, applyRGB, appyAlpha, 7.0f / 16.0f);
                updateDitherPixel(pixels, x - 1, y + 1, width, height, error, applyRGB, appyAlpha, 3.0f / 16.0f);
                updateDitherPixel(pixels, x, y + 1, width, height, error, applyRGB, appyAlpha, 5.0f / 16.0f);
                updateDitherPixel(pixels, x + 1, y + 1, width, height, error, applyRGB, appyAlpha, 1.0f / 16.0f);
            }
        }
    }
    void doFloydDitheringGray(const ImageData& img, Color4444* pixels, int32_t width, int32_t height, bool applyRGB, bool appyAlpha) {
        int32_t reso = width * height;

        int32_t error[4]{ 0 };
        Color32 px{};
        Color32 quant{};
        for (int32_t y = 0, yP = 0; y < height; y++, yP += width) {
            for (int32_t x = 0; x < width; x++) {
                auto& pix = pixels[yP + x];
                convertPixel(img.format, img.data, img.data + img.paletteSize * sizeof(Color32), quant);
                if (applyRGB) {
                    error[0] = quant.r - remapUI4ToUI8(pix.getR());
                    error[1] = quant.g - remapUI4ToUI8(pix.getG());
                    error[2] = quant.b - remapUI4ToUI8(pix.getB());
                }

                if (appyAlpha) {
                    error[3] = quant.a - remapUI4ToUI8(pix.getA());
                }

                updateDitherPixel(pixels, x + 1, y, width, height, error, applyRGB, appyAlpha, 7.0f / 16.0f);
                updateDitherPixel(pixels, x - 1, y + 1, width, height, error, applyRGB, appyAlpha, 3.0f / 16.0f);
                updateDitherPixel(pixels, x, y + 1, width, height, error, applyRGB, appyAlpha, 5.0f / 16.0f);
                updateDitherPixel(pixels, x + 1, y + 1, width, height, error, applyRGB, appyAlpha, 1.0f / 16.0f);
            }
        }
    }

    bool doColorReduction(const ImageData& input, bool toRGB444, const ColorReductionSettings& settings, bool applyRGB, const AlphaReductionSettings* alphaSettings) {
        if (input.format != TextureFormat::RGBA32 || settings.output == nullptr) { return false; }
        auto& outImg = *settings.output;

        outImg.format = TextureFormat::RGBA32;
        outImg.width = input.width;
        outImg.height = input.height;
        if (!outImg.doAllocate()) { return false; }
        int32_t reso = input.width * input.height;
        const Color32* src = reinterpret_cast<const Color32*>(input.data);
        Color32* dst32 = reinterpret_cast<Color32*>(settings.output->data);
        Color4444* dst16 = reinterpret_cast<Color4444*>(settings.output->data);

        if (input.format == TextureFormat::RGBA32 && !toRGB444) {
            memcpy(dst32, src, outImg.getSize());
        }
        else {
            size_t bpp = getBitsPerPixel(input.format) >> 3;

            for (size_t i = 0, j = 0; i < reso; i++, j += bpp) {
                convertPixel(input.format, input.data, input.data + j, dst32[i]);
            }
        }

        bool colorDither = applyRGB && settings.applyDithering;
        bool alphaDither = (alphaSettings && alphaSettings->applyDithering);

        if (!colorDither && (settings.maxDepth < 8 || toRGB444) && applyRGB) {
            for (size_t i = 0; i < reso; i++) {
                auto& pix = dst32[i];
                pix.r = quantizeUI8(pix.r, toRGB444 ? 4 : settings.maxDepth);
                pix.g = quantizeUI8(pix.g, toRGB444 ? 4 : settings.maxDepth);
                pix.b = quantizeUI8(pix.b, toRGB444 ? 4 : settings.maxDepth);
            }
        }

        if (!alphaDither && (alphaSettings && (alphaSettings->maxDepth < 8 || toRGB444))) {
            for (size_t i = 0; i < reso; i++) {
                auto& pix = dst32[i];
                pix.a = quantizeUI8(pix.a, toRGB444 ? 4 : alphaSettings->maxDepth);
            }
        }

        if (colorDither || alphaDither) {
            doFloydDitheringGray(dst32, outImg.width, outImg.height, colorDither ? toRGB444 ? 4 : settings.maxDepth : 8, alphaDither ? toRGB444 ? 4 : alphaSettings->maxDepth : 8);
        }

        if (toRGB444) {
            outImg.format = TextureFormat::RGBA4444;
            for (size_t i = 0; i < reso; i++) {
                auto& c32 = dst32[i];
                dst16[i] = Color4444(c32.r >> 4, c32.g >> 4, c32.b >> 4, c32.a >> 4);
            }
        }

        return true;
    }

    bool doTemporalLowpass(JCore::ImageData& prev, JCore::ImageData& current, const TemporalLowpassSettings& settings) {

        int32_t reso = prev.width * prev.height;
        if (reso != current.width * current.height) {
            JCORE_WARN("[ImageUtils] Warning: Temporal Lowpass frame resolutions don't match!");
            return false;
        }

        switch (prev.format)
        {
            default:
                JCORE_WARN("[ImageUtils] Warning: Temporal Lowpass unsupported format '{0}'!", getTextureFormatName(prev.format));
                return false;
            case TextureFormat::RGB24:
            case TextureFormat::RGBA32:
                break;
        }

        size_t bpp = getBitsPerPixel(prev.format) >> 3;
        float dR = 0;
        float dG = 0;
        float dB = 0;
        float dA = 0;
        bool hasAlpha = prev.format == TextureFormat::RGBA32;
        if (prev.format == current.format) {
            for (size_t i = 0, p = 0; i < reso; i++, p += bpp) {
                dR = (float(current.data[p]) - prev.data[p]) * settings.coefficient;
                dG = (float(current.data[p + 1]) - prev.data[p + 1]) * settings.coefficient;
                dB = (float(current.data[p + 2]) - prev.data[p + 2]) * settings.coefficient;

                prev.data[p] = uint8_t(std::clamp<float>(prev.data[p] + dR, 0.0f, 255.0f));
                prev.data[p + 1] = uint8_t(std::clamp<float>(prev.data[p + 1] + dG, 0.0f, 255.0f));
                prev.data[p + 2] = uint8_t(std::clamp<float>(prev.data[p + 2] + dB, 0.0f, 255.0f));

                if (hasAlpha) {
                    dA = (float(current.data[p + 3]) - prev.data[p + 3]) * settings.coefficient;
                    prev.data[p + 3] = uint8_t(std::clamp<float>(prev.data[p + 3] + dA, 0.0f, 255.0f));
                }
            }
            return true;
        }

        Color32 temp{};
        temp.a = 0xFF;
        const uint8_t* dataCur = current.data + (current.isIndexed() ? current.paletteSize * 4 : 0);
        size_t bppC = getBitsPerPixel(current.format) >> 3;
        hasAlpha &= current.hasAlpha();
        for (size_t i = 0, p = 0, pC = 0; i < reso; i++, p += bpp, pC += bppC) {

            convertPixel(current.format, current.data, dataCur + bppC, temp);

            dR = (float(temp.r) - prev.data[p]) * settings.coefficient;
            dG = (float(temp.g) - prev.data[p + 1]) * settings.coefficient;
            dB = (float(temp.b) - prev.data[p + 2]) * settings.coefficient;

            prev.data[p] = uint8_t(std::clamp<float>(prev.data[p] + dR, 0.0f, 255.0f));
            prev.data[p + 1] = uint8_t(std::clamp<float>(prev.data[p + 1] + dG, 0.0f, 255.0f));
            prev.data[p + 2] = uint8_t(std::clamp<float>(prev.data[p + 2] + dB, 0.0f, 255.0f));

            if (hasAlpha) {
                dA = (float(temp.a) - prev.data[p + 3]) * settings.coefficient;
                prev.data[p + 3] = uint8_t(std::clamp<float>(prev.data[p + 3] + dA, 0.0f, 255.0f));
            }
        }
        return true;
    }

    static void setBitIfCan(Bitset& bits, int32_t x, int32_t y, int32_t width, int32_t height, bool value) {
        if (x < 0 || y < 0 || x >= width || y >= height) { return; }
        bits.set(size_t(y) * width + x, value);
    }

    static void setAll(Bitset& bits, MorphType type, int32_t yP, int32_t yPP, int32_t yNP, int32_t edgeX, int32_t edgeY, bool value) {
        bits.set(size_t(yP), true);
        switch (type)
        {
            default:
                if ((edgeX & 0x1)) {
                    bits.set(size_t(yP - 1), value);
                }
                if ((edgeX & 0x2)) {
                    bits.set(size_t(yP + 1), value);
                }

                if ((edgeY & 0x1)) {
                    bits.set(size_t(yPP), value);
                    if ((edgeX & 0x1)) {
                        bits.set(size_t(yPP - 1), value);
                    }
                    if ((edgeX & 0x2)) {
                        bits.set(size_t(yPP + 1), value);
                    }
                }

                if ((edgeY & 0x2)) {
                    bits.set(size_t(yNP), value);
                    if ((edgeX & 0x1)) {
                        bits.set(size_t(yNP - 1), value);
                    }
                    if ((edgeX & 0x2)) {
                        bits.set(size_t(yNP + 1), value);
                    }
                }
                break;
            case MORPH_4_Cardinal:
                if ((edgeX & 0x1)) {
                    bits.set(size_t(yP - 1), value);
                }
                if ((edgeX & 0x2)) {
                    bits.set(size_t(yP + 1), value);
                }

                if ((edgeY & 0x1)) {
                    bits.set(size_t(yPP), value);
                }

                if ((edgeY & 0x2)) {
                    bits.set(size_t(yNP), value);
                }
                break;
            case MORPH_4_Diagonal:
                if ((edgeY & 0x1)) {
                    if ((edgeX & 0x1)) {
                        bits.set(size_t(yPP - 1), value);
                    }
                    if ((edgeX & 0x2)) {
                        bits.set(size_t(yPP + 1), value);
                    }
                }

                if ((edgeY & 0x2)) {
                    if ((edgeX & 0x1)) {
                        bits.set(size_t(yNP - 1), value);
                    }
                    if ((edgeX & 0x2)) {
                        bits.set(size_t(yNP + 1), value);
                    }
                }
                break;
        }
    }

    static bool shouldErode(Bitset& bits, MorphType type, int32_t yP, int32_t yPP, int32_t yNP, int32_t edgeX, int32_t edgeY) {
        size_t count = 0;
        switch (type)
        {
            case Projections::MORPH_4_Diagonal:
                if (edgeY & 0x1) {
                    if ((edgeX & 0x1) && bits[size_t(yPP - 1)]) { count++; }
                    if ((edgeX & 0x2) && bits[size_t(yPP + 1)]) { count++; }
                }

                if (edgeY & 0x2) {
                    if ((edgeX & 0x1) && bits[size_t(yNP - 1)]) { count++; }
                    if ((edgeX & 0x2) && bits[size_t(yNP + 1)]) { count++; }
                }
                return count < 4;
            case Projections::MORPH_4_Cardinal:
                if ((edgeX & 0x1) && bits[size_t(yP - 1)]) {
                    count++;
                }
                if ((edgeX & 0x2) && bits[size_t(yP + 1)]) {
                    count++;
                }

                if ((edgeY & 0x1)) {
                    if (bits[size_t(yPP)]) { count++; }
                }

                if ((edgeY & 0x2)) {
                    if (bits[size_t(yNP)]) { count++; }
                }
                return count < 4;
            default:
                if ((edgeX & 0x1) && bits[size_t(yP - 1)]) {
                    count++;
                }
                if ((edgeX & 0x2) && bits[size_t(yP + 1)]) {
                    count++;
                }

                if ((edgeY & 0x1)) {
                    if (bits[size_t(yPP)]) { count++; }

                    if ((edgeX & 0x1) && bits[size_t(yPP - 1)]) { count++; }
                    if ((edgeX & 0x2) && bits[size_t(yPP + 1)]) { count++; }
                }

                if ((edgeY & 0x2)) {
                    if (bits[size_t(yNP)]) { count++; }

                    if ((edgeX & 0x1) && bits[size_t(yNP - 1)]) { count++; }
                    if ((edgeX & 0x2) && bits[size_t(yNP + 1)]) { count++; }
                }
                return count < 8;
        }
    }

    bool doMorphPasses(JCore::Bitset& bits, JCore::ImageData& frame, MorphologicalSettings& settings) {
        bits.setAll(false);

        Bitset tempSet(bits.size());
        Bitset alphaSet(bits.size());
        int32_t reso = frame.width * frame.height;
        switch (frame.format)
        {
            default: {
                Color32 temp{};
                uint8_t* dataStart = frame.getData();
                size_t bpp = getBitsPerPixel(frame.format) >> 3;
                for (size_t i = 0, bP = 0; i < reso; i++, bP += bpp) {
                    convertPixel(frame.format, frame.data, dataStart + bP, temp);
                    alphaSet.set(i, temp.a > 0);
                    tempSet.set(i, temp.a > 0 && getThresholdRGB(temp.r, temp.g, temp.b, settings.minR, settings.maxR, settings.minG, settings.maxG, settings.minB, settings.maxB));
                }
                break;
            }

            case TextureFormat::RGB24: {
                alphaSet.setAll(true);
                for (size_t i = 0, bP = 0; i < reso; i++, bP += 3) {
                    tempSet.set(i, getThresholdRGB(frame.data[bP + 0], frame.data[bP + 1], frame.data[bP + 2], settings.minR, settings.maxR, settings.minG, settings.maxG, settings.minB, settings.maxB));
                }
                break;
            }
            case TextureFormat::RGBA32: {
                for (size_t i = 0, bP = 0; i < reso; i++, bP += 4) {
                    alphaSet.set(i, frame.data[bP + 3] > 0);
                    tempSet.set(i, frame.data[bP + 3] >  0 && getThresholdRGB(frame.data[bP + 0], frame.data[bP + 1], frame.data[bP + 2], settings.minR, settings.maxR, settings.minG, settings.maxG, settings.minB, settings.maxB));
                }
                break;
            }
        }

        bits.copyFrom(tempSet);
        int32_t edgeX = 0;
        int32_t edgeY = 0;
        for (size_t i = 0; i < MorphologicalSettings::MAX_PASSES; i++) {
            auto& pass = settings.passes[i];
            for (size_t k = 0; k < pass.numOfTimes; k++) {
                if (pass.dilate) {
                    for (int32_t y = 0, yPP = -frame.width, yNP = frame.width, yP = 0; y < frame.height; y++, yP += frame.width, yPP += frame.width, yNP += frame.width) {
                        edgeY = 0x0;
                        if (y > 0) {
                            edgeY |= 0x1;
                        }
                        if (y < frame.height - 1) {
                            edgeY |= 0x2;
                        }
                        for (int32_t x = 0; x < frame.width; x++) {
                            edgeX = 0x0;
                            if (x > 0) {
                                edgeX |= 0x1;
                            }
                            if (x < frame.width - 1) {
                                edgeX |= 0x2;
                            }
                            if (tempSet[size_t(yP) + x]) {
                                setAll(bits, pass.type, yP + x, yPP + x, yNP + x, edgeX, edgeY, true);
                            }
                        }
                    }
                }
                else {
                    for (int32_t y = 0, yPP = -frame.width, yNP = frame.width, yP = 0; y < frame.height; y++, yP += frame.width, yPP += frame.width, yNP += frame.width) {
                        edgeY = 0x0;
                        if (y > 0) {
                            edgeY |= 0x1;
                        }
                        if (y < frame.height - 1) {
                            edgeY |= 0x2;
                        }
                        for (int32_t x = 0; x < frame.width; x++) {
                            edgeX = 0x0;
                            if (x > 0) {
                                edgeX |= 0x1;
                            }
                            if (x < frame.width - 1) {
                                edgeX |= 0x2;
                            }
                            if (tempSet[size_t(yP) + x] && shouldErode(tempSet, pass.type, yP + x, yPP + x, yNP + x, edgeX, edgeY)) {
                                bits.set(size_t(yP) + x, false);
                            }
                        }
                    }
                }
                bits.andWith(alphaSet);
                tempSet.copyFrom(bits);
            }
            if (pass.invert) {
                bits.flip();
                bits.andWith(alphaSet);
            }
        }
        return true;
    }

    bool doCrossFade(const JCore::ImageData& lhs, const JCore::ImageData& rhs, float time, JCore::ImageData& output, CrossFadeSettings& settings) {
        output.width = lhs.width;
        output.height = lhs.height;
        output.format = TextureFormat::RGBA32;

        if (!output.doAllocate()) { return false; }

        switch (settings.type)
        {
            case CROSS_Quadratic:
                time *= time;
                break;
            case CROSS_Cubic:
                time *= time * time;
                break;
        }

        int32_t reso = lhs.width * lhs.height;
        int32_t bppL = getBitsPerPixel(lhs.format) >> 3;
        int32_t bppR = getBitsPerPixel(rhs.format) >> 3;

        Color32 cLhs{ 0xFF,0xFF,0XFF,0XFF };
        Color32 cRhs{ 0xFF,0xFF,0xFF,0xFF };
        Color32* pix = reinterpret_cast<Color32*>(output.data);
        const uint8_t* clrL = lhs.getData();
        const uint8_t* clrR = rhs.getData();

        for (int32_t i = 0; i < reso; i++) {
            convertPixel(lhs.format, lhs.data, clrL, cLhs);
            convertPixel(rhs.format, rhs.data, clrR, cRhs);
            pix[i] = lerp(cLhs, cRhs, time);
            clrL += bppL;
            clrR += bppR;
        }
        return true;
    }

    bool doFade(const JCore::ImageData& lhs, float time, JCore::ImageData& output, FadeSettings& settings) {
        output.width = lhs.width;
        output.height = lhs.height;
        output.format = TextureFormat::RGBA32;

        if (!output.doAllocate()) { return false; }
        bool isOut = time < 0.5f;

        time = isOut ? (1.0f - (time * 2.0f)) : (time - 0.5f) * 2.0f;
        switch (settings.type)
        {
            case CROSS_Quadratic:
                time *= time;
                break;
            case CROSS_Cubic:
                time *= time * time;
                break;
        }
        uint8_t alpha = uint8_t(time * 255.0f);

        int32_t reso = lhs.width * lhs.height;
        int32_t bppL = getBitsPerPixel(lhs.format) >> 3;

        Color32 cLhs{ 0xFF,0xFF,0XFF, 0X00 };
        Color32* pix = reinterpret_cast<Color32*>(output.data);
        const uint8_t* clrL = lhs.getData();

        for (int32_t i = 0; i < reso; i++) {
            convertPixel(lhs.format, lhs.data, clrL, pix[i]);
            pix[i].a = multUI8(pix[i].a, alpha);
            clrL += bppL;
        }

        return true;
    }

    bool doEdgeFade(const JCore::ImageData& lhs, JCore::ImageData& output, AlphaEdgeFadeSettings& settings) {
        output.width = lhs.width;
        output.height = lhs.height;
        output.format = TextureFormat::RGBA32;

        if (!output.doAllocate()) {
            return false;
        }

        Color32* outPix = reinterpret_cast<Color32*>(output.data);
        const uint8_t* data = lhs.getData();
        int32_t bpp = getBitsPerPixel(lhs.format) >> 3;
        int32_t reso = lhs.width * lhs.height;

        if (lhs.format == TextureFormat::RGBA32) {
            memcpy(output.data, lhs.data, size_t(reso) * bpp);
        }
        else {
            for (int32_t i = 0; i < reso; i++) {
                convertPixel(lhs.format, lhs.data, data, outPix[i]);
                data += bpp;
            }
        } 
        return false;
    }

    bool doChromaKey(const JCore::ImageData& image, JCore::ImageData& output, const ChromaKeySettings& chroma, const JCore::ImageData* andMask, const JCore::ImageData* orMask) {
        output.width = image.width;
        output.height = image.height;
        output.format = TextureFormat::RGBA32;

        if (!output.doAllocate()) {
            return false;
        }

        float crK, cbK, yK;
        float crP, cbP, yP;
        getCbCrY(chroma.refColor, crK, cbK, yK);

        Color32 temp{};
        const uint8_t* data = image.getData();
        int32_t bpp = getBitsPerPixel(image.format) >> 3;
        int32_t reso = image.width * image.height;

        float min = chroma.minDifference * 255.0f;
        float max = chroma.maxDifference * 255.0f;

        float r, g, b;
        float rK(float(chroma.refColor.r)), gK(float(chroma.refColor.g)), bK(float(chroma.refColor.b));

        Color32* pixels = reinterpret_cast<Color32*>(output.data);
        Color32* maskOR = reinterpret_cast<Color32*>(orMask ? orMask->data : nullptr);
        Color32* maskAND = reinterpret_cast<Color32*>(andMask ? andMask->data : nullptr);
        for (size_t i = 0; i < reso; i++) {
            convertPixel(image.format, image.data, data, temp);
            Color32& cPix = pixels[i];
            getCbCrY(temp, crP, cbP, yP);

            r = float(temp.r);
            g = float(temp.g);
            b = float(temp.b);

            float mask = getCbCrYDist(crP, cbP, crK, cbK, min, max);

            if (maskOR) {
                mask = std::min(mask + maskOR[i].a * (1.0f / 255.0f), 1.0f);
            }

            if (maskAND) {
                mask *= maskAND[i].a * (1.0f / 255.0f);
            }

            float iMask = 1.0f - mask;

            cPix.r = uint8_t(std::max<float>(r - iMask * rK, 0.0f));
            cPix.g = uint8_t(std::max<float>(g - iMask * gK, 0.0f));
            cPix.b = uint8_t(std::max<float>(b - iMask * bK, 0.0f));
            cPix.a = uint8_t(float(temp.a) * mask);

            if (chroma.premultiply) {
                premultiplyC32(cPix);
            }
            data += bpp;
        }

        return true;
    }

    bool doGrayToAlpha(JCore::ImageData& input) {
        if (input.format != TextureFormat::RGBA32) { return false; }
        Color32* pixels = reinterpret_cast<Color32*>(input.data);
        int32_t reso = input.width * input.height;
        for (int32_t i = 0; i < reso; i++) {
            auto& pix = pixels[i];
            pix.a = multUI8(pix.a, (uint32_t(pix.r) + pix.g + pix.b) / 3);
        }
        return true;
    }

    bool doAlphaMask(const JCore::ImageData& input, const JCore::ImageData& mask, JCore::ImageData& output) {
        output.width = input.width;
        output.height = input.height;
        output.format = TextureFormat::RGBA32;

        if (mask.format != TextureFormat::RGBA32 || !output.doAllocate()) { return false; }

        Color32* outPix = reinterpret_cast<Color32*>(output.data);
        const Color32* maskPix = reinterpret_cast<const Color32*>(mask.data);
        int32_t reso = input.width * input.height;

        const uint8_t* iData = input.getData();
        int32_t bpp = getBitsPerPixel(input.format) >> 3;
        for (int32_t i = 0; i < reso; i++) {
            auto& pix = outPix[i];
            convertPixel(input.format, input.data, iData, pix);
            pix.a = multUI8(pix.a, maskPix[i].a);
            iData += bpp;
        }
        return true;
    }

    bool doImageMask(int32_t minDist, int32_t maxDist, const JCore::ImageData& input, const JCore::ImageData& mask, JCore::ImageData& output) {
        output.width = input.width;
        output.height = input.height;
        output.format = TextureFormat::RGBA32;

        if (mask.format != TextureFormat::RGBA32 || !output.doAllocate()) { return false; }

        Color32* outPix = reinterpret_cast<Color32*>(output.data);
        const Color32* maskPix = reinterpret_cast<const Color32*>(mask.data);
        int32_t reso = input.width * input.height;

        int32_t tgt = maxDist - minDist;
        const uint8_t* iData = input.getData();
        int32_t bpp = getBitsPerPixel(input.format) >> 3;
        for (int32_t i = 0; i < reso; i++) {
            auto& pix = outPix[i];
            convertPixel(input.format, input.data, iData, pix);
            if (maskPix[i].a < 0x80) { iData += bpp; continue; }

            int32_t dist = channelDistance(pix, maskPix[i]);

            if (dist <= minDist) {
                pix.a = 0;
            }
            else if (dist < maxDist) {
                pix.a = uint8_t(pix.a * ((dist - minDist) / float(tgt)));
            }
            iData += bpp;
        }
        return true;
    }
}