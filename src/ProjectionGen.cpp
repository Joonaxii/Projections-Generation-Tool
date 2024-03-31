#include <ProjectionGen.h>
#include <algorithm>
#include <J-Core/IO/FileStream.h>
#include <J-Core/Log.h>
#include <J-Core/IO/Image.h>
#include <J-Core/Util/StringUtils.h>
#include <J-Core/TaskManager.h>
#include <J-Core/Math/Color24.h>
#include <J-Core/Math/Color32.h>

using namespace JCore;

namespace Projections {
    enum UI8Mode {
        UI8_Red = 0x01,
        UI8_Green = 0x02,
        UI8_Blue = 0x04,
        UI8_Alpha = 0x08,
    };

    template<typename T, uint32_t MAX_RUN>
    static uint32_t getRunLength_Long(size_t pos, const T* data, size_t length) {
        uint32_t runLen = 1;
        const T& datPtr = data[pos++];
        while (pos < length && runLen < MAX_RUN) {
            if (data[pos] != datPtr) {
                break;
            }
            runLen++;
            pos++;
        }
        return runLen;
    }

    template<typename T>
    static int32_t applyRLE_Normal(const Stream& stream, int32_t resolution, const T* pixels) {
        int32_t bytesWritten = 0;

        int32_t pos = 0;
        size_t bToWrite = 0;
        uint32_t hdr = 0;
        while (pos < resolution) {
            uint32_t runLen = getRunLength_Long<T, 1073741824U>(pos, pixels, resolution);
            uint32_t rLen = runLen - 1;
            hdr = rLen << 2;

            if (runLen > 4194304) {
                hdr |= 0x3;
            }
            else if (runLen > 16384) {
                hdr |= 0x2;
            }
            else if (runLen > 64) {
                hdr |= 0x1;
            }
            bytesWritten += (size_t(hdr & 0x3) + 1 + sizeof(T));
            pos += runLen;

            stream.write(&hdr, size_t(hdr & 0x3) + 1, false);
            stream.writeValue(pixels[pos], 1, false);
        }
        return bytesWritten;
    }

    static void readAsColor32(const ImageData& src, Color32* pixels, uint8_t alphaClip) {
        int32_t pixC = src.width * src.height;
        if (src.format == TextureFormat::RGBA32) {
            memcpy(pixels, src.data, pixC * sizeof(Color32));
            if (alphaClip > 0) {
                for (int32_t i = 0; i < pixC; i++) {
                    if (pixels[i].a < alphaClip) {
                        reinterpret_cast<uint32_t&>(pixels[i]) = 0;
                    }
                }
            }
            return;
        }

        const uint8_t* dataPos = src.getData();
        size_t bpp = getBitsPerPixel(src.format) >> 3;
        for (int32_t i = 0; i < pixC; i++) {
            convertPixel(src.format, src.data, dataPos, pixels[i]);
            if (pixels[i].a < alphaClip) {
                reinterpret_cast<uint32_t&>(pixels[i]) = 0;
            }
            dataPos += bpp;
        }
    }

    static void readAsUI8(const ImageData& src, uint8_t* pixels, uint8_t colorMask) {
        int32_t pixC = src.width * src.height;
        const uint8_t* dataPos = src.getData();
        Color32 temp{};
        size_t bpp = getBitsPerPixel(src.format) >> 3;
        for (int32_t i = 0; i < pixC; i++) {
            convertPixel(src.format, src.data, dataPos, temp);

            switch (colorMask & (UI8_Red | UI8_Green | UI8_Blue))
            {
                case UI8_Red | UI8_Green | UI8_Blue:
                    pixels[i] = uint8_t(Math::min(0.299f * temp.r + 0.587f * temp.g + 0.114f * temp.b, 255.0f));
                    break;
                case UI8_Red:
                    pixels[i] = temp.r;
                    break;
                case UI8_Green:
                    pixels[i] = temp.g;
                    break;
                case UI8_Blue:
                    pixels[i] = temp.b;
                    break;

                case UI8_Red | UI8_Green:
                    pixels[i] = uint8_t(uint32_t(temp.r) + (temp.g >> 1));
                    break;
                case UI8_Green | UI8_Blue:
                    pixels[i] = uint8_t(uint32_t(temp.g) + (temp.b >> 1));
                    break;
                case UI8_Red | UI8_Blue:
                    pixels[i] = uint8_t(uint32_t(temp.r) + (temp.b >> 1));
                    break;
            }

            if (colorMask == UI8_Alpha) {
                pixels[i] = temp.a;
            }
            else if ((colorMask & UI8_Alpha) != 0) {
                pixels[i] = multUI8(pixels[i], temp.a);
            }

            dataPos += bpp;
        }
    }

    void FrameMask::write(const Stream& stream, std::string_view root, int32_t width, int32_t height) const {
        static ImageData buffer{};
        size_t pos = stream.tell();

        std::string mPath = IO::combine(root, path);
        if (Utils::isWhiteSpace(name)) {
            JCORE_WARN("Given name for '{}' is whitespace!", IO::getName(path));
            goto noData;
        }
        
        if (Png::decode(mPath, buffer)) {
            int32_t reso = buffer.width * buffer.height;

            if (reso == 0 || width != buffer.width || height != buffer.height) {
                JCORE_WARN("Mask '{}' resolution is invalid or doesn't match projection resolution! {}x{} =/= {}x{}", IO::getName(path), buffer.width, buffer.height, width, height);
                goto noData;
            }

            uint8_t* byteBuf = reinterpret_cast<uint8_t*>(_malloca(reso));
        
            if (!byteBuf) {
                JCORE_WARN("Failed to allocate frame mask for '{}'!", IO::getName(path));
                goto noData;
            }
            readAsUI8(buffer, byteBuf, UI8_Red | UI8_Alpha);

            if (compress) {
                stream.writeValue<int32_t>(0);
                writeShortString(name, stream);

                size_t posA = stream.tell();
                applyRLE_Normal(stream, reso, byteBuf);
                size_t posB = stream.tell();
                stream.seek(pos, SEEK_SET);
                stream.writeValue(int32_t(posB - posA));
                stream.seek(posB, SEEK_SET);
            }
            else {
                stream.writeValue<int32_t>(~reso);
                writeShortString(name, stream);
                stream.write(byteBuf, reso);
            }
            _freea(byteBuf);
            return;
        }
        JCORE_WARN("Failed to decode mask file! '{}'", mPath);
    noData:
        stream.writeValue<int32_t>(0);
    }

    void PMaterial::write(const Stream& stream) const {
        static ImageData readBuffer{};
        static ImageData iconBuffer{};

        writeShortString(nameID, stream);
        writeShortString(name, stream);
        writeShortString(description, stream);

        stream.writeValue(rarity);
        stream.writeValue(priority);
        stream.writeValue(flags);
        coinValue.write(stream);

        uint16_t srcCount = uint16_t(Math::min<size_t>(sources.size(), UINT16_MAX));
        stream.writeValue<uint16_t>(srcCount);
        for (size_t i = 0; i < srcCount; i++) {
            sources[i].write(stream);
        }

        srcCount = uint16_t(Math::min<size_t>(recipes.size(), UINT16_MAX));
        stream.writeValue<uint16_t>(srcCount);
        for (size_t i = 0; i < srcCount; i++) {
            recipes[i].write(stream);
        }

        std::string iconP = IO::combine(root, icon);
        if (iconMode > TexMode::TEX_None && iconMode < __TEX_COUNT) {

            DataFormat format{DataFormat::FMT_UNKNOWN};
            if (Image::tryDecode(iconP, readBuffer, format) && 
                iconBuffer.doAllocate(readBuffer.width, readBuffer.height, TextureFormat::RGBA32)) {
                Color32* pixels = reinterpret_cast<Color32*>(iconBuffer.data);
                readAsColor32(readBuffer, pixels, 8);

                stream.writeValue(iconMode);
                size_t pos = stream.tell();
                stream.writeValue<uint32_t>(0);

                switch (iconMode) {
                case Projections::TEX_PNG:
                    Png::encode(stream, iconBuffer, 6);
                    break;
                case Projections::TEX_DDS:
                    DDS::encode(stream, iconBuffer);
                    break;
                case Projections::TEX_JTEX:
                    JTEX::encode(stream, iconBuffer);
                    break;
                case Projections::TEX_RLE:
                    applyRLE_Normal(stream, iconBuffer.width*iconBuffer.height, reinterpret_cast<const Color32*>(iconBuffer.data));
                    break;
                }

                size_t endPos = stream.tell();
                stream.seek(pos, SEEK_SET);
                stream.writeValue<uint32_t>(uint32_t(endPos - (pos + 4)));
                stream.seek(endPos, SEEK_SET);
                return;
            }
        }
        stream.writeValue(TEX_None);
        stream.writeValue<uint32_t>(0);
    }

    bool Projection::prepare() {
        std::string path = IO::combine(material.root, framePath);
        if (!IO::exists(path)) {
            JCORE_WARN("Failed to prepare Projection '{0}': Frame path '{1}' doesn't exist!", material.nameID, path);
            return false;
        }

        static constexpr std::string_view ALLOWED_FILES[]{
            "png",
            "dds",
            "bmp",
            "jtex",
        };

        static std::vector<fs::path> paths{};
        static std::vector<PFramePath> tempPaths{};
        paths.clear();
        tempPaths.clear();

        float frameDuration = 1.0f / Math::max(frameRate, 0.001f);
        if (IO::getAll(path, IO::F_TYPE_FILE, paths, true,
            [](const fs::path& path) {
                auto ext = IO::getExtension(path);
                if (ext.length() < 1) { return false; }
                for (size_t i = 0; i < sizeof(ALLOWED_FILES) / sizeof(std::string_view); i++) {
                    if (Utils::strIEquals(ext, ALLOWED_FILES[i])) {
                        return true;
                    }
                }
                return false;
            })) {
            int32_t lowest = INT32_MAX;
            int32_t highest = 0;

            std::string tempStr{};
            std::string_view temp{};
            for (size_t i = 0; i < paths.size(); i++) {
                tempStr = paths[i].string();
                temp = IO::getName(tempStr);

                if (!Utils::startsWith(temp, "Frame", false)) {
                    continue;
                }

                PFrameIndex idx(temp);
                tempPaths.emplace_back(temp, idx);

                int32_t frameIdx = int32_t(idx.getIndex());
                lowest = Math::min<int32_t>(frameIdx, lowest);
                highest = Math::max<int32_t>(frameIdx, highest);
            }

            if (highest < lowest) {

                JCORE_WARN("Failed to prepare frames for '{0}'!", material.nameID);
                return false;
            }

            highest -= lowest;

            int32_t fCount = highest + 1;
            int32_t total = fCount * int32_t(layers.size());

            frames.resize(total);
            for (size_t i = 0; i < frames.size(); i++) {
                frames[i].reset();
                frames[i].frameDuration = frameDuration;
            }

            width = 0;
            height = 0;

            ImageData tempData{};
            for (auto& tmp : tempPaths) {
                auto& idx = tmp.index;
                if (idx.getLayer() >= layers.size()) {
                    continue;
                }
                int32_t index = idx.getIndex() - lowest;
                if (index >= fCount) { continue; }

                size_t tgt = index * layers.size() + idx.getLayer();
                (idx.isEmissive() ? frames[tgt].pathE : frames[tgt].path) = tmp;

                if ((width < 1 || height < 1) && tmp.getInfo(tempData, path)) {
                    width = tempData.width;
                    height = tempData.height;

                    if (width > PROJ_MAX_RESOLUTION || height >= PROJ_MAX_RESOLUTION) {
                        JCORE_ERROR("Failed to prepare '{}'! (Frame resolution {}x{} is larger than max of {})", material.nameID, width, height, PROJ_MAX_RESOLUTION);
                        return false;
                    }
                }
            }

            for (auto& inf : frameInfo) {
                //if (inf.sourceStart.isValid(fCount, layers.size(), true)) {
                //    bool isRange = inf.sourceEnd.isValid(fCount, layers.size(), true) && inf.sourceEnd.frame;
                //
                //    auto& frame = frames[inf.source.layer + inf.source.frame * layers.size()];
                //
                //    if (inf.frameDuration > 0) {
                //        frame.frameDuration = inf.frameDuration;
                //    }
                //    if (inf.target.isValid(fCount, layers.size())) {
                //        auto& frameTgt = frames[inf.source.layer + inf.source.frame * layers.size()];
                //        (inf.source.emission ? frame.pathE : frame.path) = (inf.target.emission ? frameTgt.pathE : frameTgt.path);
                //    }
                //}
            }

            if (!audioInfo.prepare(material.root)) {
                JCORE_WARN("Failed to prepare audio for '{}'! (One or more audio variants were invalid, no audio will be exported!)", material.nameID);
            }

            if (width > 0 && height > 0) {
                prepared = true;
                return true;
            }
        }

        JCORE_ERROR("Failed to prepare '{}'!", material.nameID);
        return false;
    }

    struct FramePointer {
        uint32_t index{};
        constexpr FramePointer(uint32_t value) : index(value) {}
        constexpr FramePointer(uint32_t frame, uint32_t layer, bool isEmission) : 
            index((layer & 0x7FFFFFU) | ((layer & 0x7FU) << 23) | (isEmission ? 0x40000000U : 0x00) | 0x80000000U)
        {}

        constexpr FramePointer(uint32_t size, bool isCompressed) :
            index((size & 0x7FFFFFU) | (isCompressed ? 0x40000000U : 0x00))
        {}

        constexpr bool operator==(const FramePointer& other) const {
            return index == other.index;
        }
        
        constexpr bool operator!=(const FramePointer& other) const {
            return index != other.index;
        }
    };

    static constexpr FramePointer EmptyFrame = FramePointer(0x00);

    static void parseFrameIndex(
        uint32_t current, uint32_t layerC, 
        uint32_t& index, uint32_t& layer) {
        index = current / layerC;
        layer = current % layerC;
    }

    static FramePointer indexOfCrcBlock(const CRCBlocks& block, const PrFrame* frames, uint32_t current, uint32_t layerC, bool isEmission) {
        uint32_t fr{0}, lr{ 0 };
        for (uint32_t i = 0; i < current; i++) {
            if (frames[i].block[0] == block) {
                parseFrameIndex(i, layerC, fr, lr);
                return FramePointer(fr, lr, false);
            }

            if (frames[i].block[0] == block) {
                parseFrameIndex(i, layerC, fr, lr);
                return FramePointer(fr, lr, true);
            }
        }
        if (isEmission && frames[current].block[0] == frames[current].block[1]) {
            parseFrameIndex(current, layerC, fr, lr);
            return FramePointer(fr, lr, true);
        }
        return EmptyFrame;
    }

    static void writeFrameData(std::string_view framePath, int32_t index, PrFrame* frames, int32_t layerC, const Stream& stream,
        PBuffers& buffers, bool altTex, float minCompression = 0.25f, uint8_t alphaClip = 8) {

        auto& frame = frames[index];
        PFramePath* path = (altTex ? &frame.pathE : &frame.path);
        CRCBlocks* crcBlock = (altTex ? &frame.block[1] : &frame.block[0]);

        TaskManager::waitForBuffer();
        if (path->isValid() && 
            path->decodeImage(buffers.readBuffer, framePath) &&
            buffers.frameBuffer.doAllocate(buffers.readBuffer.width, buffers.readBuffer.height, TextureFormat::RGBA32)) {
            Color32* pixels = reinterpret_cast<Color32*>(buffers.frameBuffer.data);
            readAsColor32(buffers.readBuffer, pixels, alphaClip);

            REPORT_PROGRESS(
                TaskManager::reportPreview(&buffers.frameBuffer);
            );

            bool isEmpty = !crcBlock->from(buffers.frameBuffer.data, buffers.frameBuffer.getSize());
            if (isEmpty) {
                goto noData;
            }

            FramePointer ptr = indexOfCrcBlock(*crcBlock, frames, index, layerC, altTex);
            if (ptr != EmptyFrame) {
                stream.writeValue(EmptyFrame);
                stream.writeZero(3);
                return;
            }

            static constexpr size_t DATA_OFFSET = sizeof(FramePointer) + sizeof(uint16_t) + sizeof(uint8_t);
            int32_t reso = buffers.frameBuffer.width * buffers.frameBuffer.height;
            int32_t ogSize = reso * sizeof(Color32);
            
            uint16_t pOffset = 0;
            uint8_t imageMode = buffers.palette.count > 256 ? 0x2 : 0x1;
            int32_t lowest = INT_MAX;
            int32_t highest = 0;
            for (int32_t i = 0; i < reso && imageMode > 0; i++) {
                int32_t ind = buffers.palette.add(pixels[i]);
                if (ind < 0) {
                    memcpy(&buffers.palette, &buffers.palettePrev, sizeof(buffers.palettePrev));
                    imageMode = 0;
                    break;
                }
                else {
                    if (buffers.palette.count > 256 && imageMode == 1) {
                        for (size_t j = 0; j < i; j++) {
                            buffers.idxUI16[j] = buffers.idxUI8[j];
                        }
                        imageMode = 2;
                    }

                    if (imageMode == 1) {
                        buffers.idxUI8[i] = uint8_t(ind);
                    }
                    else {
                        buffers.idxUI16[i] = uint16_t(ind);
                    }
                    lowest = Math::min(ind, lowest);
                    highest = Math::max(ind, highest);
                }
            }

            if (imageMode == 2) {
                int32_t diff = highest - lowest;
                if (diff <= 256) {
                    pOffset = uint16_t(lowest);
                    for (size_t i = 0; i < reso; i++) {
                        buffers.idxUI8[i] = uint8_t(buffers.idxUI16[i] - pOffset);
                    }
                    imageMode = 1;
                }
            }

            if (imageMode != 0) {
                memcpy(&buffers.palettePrev, &buffers.palette, sizeof(buffers.palette));
            }

            size_t pos = stream.tell();
            stream.writeValue(ptr);
            stream.writeValue(imageMode);
            stream.writeValue(pOffset);
            int32_t bWrite = 0;
            void* bufferToWrite = 0;

            switch (imageMode)
            {
            default:
                bWrite = applyRLE_Normal(stream, reso, pixels);
                bufferToWrite = pixels;
                break;
            case 1:
                ogSize = reso;
                bWrite = applyRLE_Normal(stream, reso, buffers.idxUI8);
                bufferToWrite = buffers.idxUI8;
                break;
            case 2:
                ogSize = reso * 2;
                bWrite = applyRLE_Normal(stream, reso, buffers.idxUI16);
                bufferToWrite = buffers.idxUI16;
                break;
            }
            float pr = 1.0f - ((float(bWrite) / ogSize));
            if (imageMode == 0) {
                for (int32_t i = 0; i < reso; i++) {
                    premultiplyC32(pixels[i]);
                }
            }

            if (pr < minCompression) {
                stream.seek(pos, SEEK_SET);
                ptr = FramePointer(uint32_t(ogSize), false);
                stream.writeValue(ptr);
                stream.writeValue(imageMode);
                stream.writeValue(pOffset);
                stream.write(bufferToWrite, ogSize, false);
            }
            else {
    
                size_t endPos = stream.tell();
                stream.seek(pos, SEEK_SET);
                ptr = FramePointer(uint32_t(endPos - (pos + DATA_OFFSET)), true);
                stream.writeValue(ptr);
                stream.writeValue(imageMode);
                stream.writeValue(pOffset);
                stream.seek(endPos, SEEK_SET);
            }
            return;
        }
    noData:
        stream.writeValue(EmptyFrame);
        stream.writeZero(3);
    }

    static void writeFrame(std::string_view framePath, PBuffers& buffers, int32_t index, PrFrame* frames, int32_t layerC, const Stream& stream, float minCompression = 0.25f, uint8_t alphaClip = 8) {
        auto& frame = frames[index];

        stream.writeValue(frame.flags);
        stream.writeValue(frame.frameDuration);

        for (int32_t i = 0, j = index; i < layerC; i++, j++) {
            writeFrameData(framePath, j, frames, layerC, stream, buffers, false, minCompression, alphaClip);
            writeFrameData(framePath, j, frames, layerC, stream, buffers, true, minCompression, alphaClip);
        }
    }

    bool Projection::write(const Stream& stream, PBuffers& buffers, float minCompression) {
        if (width < 1 || width > 1024 || height < 1 || height > 1024) {
            JCORE_ERROR("Failed to write '{}'! (Invalid resolution! {}x{})", material.nameID, width, height);
            return false;
        }

        if (!buffers.frameBuffer.doAllocate(width, height, TextureFormat::RGBA32)) {
            JCORE_ERROR("Failed to write '{}'! (Couldn't allocate image buffer!)", material.nameID);
            return false;
        }
        buffers.palettePrev.clear();
        buffers.palette.clear();
        buffers.noPalette = false;

        int32_t lrC = Math::max<int32_t>(int32_t(layers.size()), 1);
        int32_t frameCount = int32_t(frames.size() / lrC);

        REPORT_PROGRESS(
            TaskManager::regLevel(2);
            TaskManager::reportProgress(2, 0.0, 0.0, frameCount);
        );
        std::string framePath = IO::combine(material.root, this->framePath);

        material.write(stream);
        stream.writeValue(loopStart);
        stream.writeValue(width);
        stream.writeValue(height);
        stream.writeValue(animMode);

        stream.writeValue(int32_t(stackThresholds.size()));
        for (size_t i = 0; i < stackThresholds.size(); i++) {
            stackThresholds[i].write(stream);
        }

        uint16_t tagC = uint16_t(Math::min<size_t>(rawTags.size(), UINT16_MAX));
        stream.writeValue(tagC);
        for (size_t i = 0; i < tagC; i++) {
            writeShortString(rawTags[i], stream);
        }

        stream.writeValue(int32_t(layers.size()));
        for (auto& lr : layers) {
            lr.write(stream);
        }
        stream.writeValue(frameCount);

        for (int32_t i = 0, j = 0; i < frameCount; i++, j += lrC) {
            if (TaskManager::isSkipping()) {
                REPORT_PROGRESS(
                    TaskManager::reportProgress(2, frameCount);
                    TaskManager::unregLevel(2);
                );
                return false;
            }

            if (TaskManager::isCanceling()) {
                REPORT_PROGRESS(
                    TaskManager::reportProgress(2, frameCount);
                    TaskManager::unregLevel(2);
                );
                return false;
            }

            writeFrame(framePath, buffers, j, frames.data(), lrC, stream,  minCompression, 8);
            REPORT_PROGRESS(
                TaskManager::reportIncrement(2);
            );
        }
        stream.writeValue(buffers.palette.count);
        for (size_t i = 0; i < buffers.palette.count; i++) {
            premultiplyC32(buffers.palette.colors[i]);
        }
        stream.write(buffers.palette.colors, sizeof(Color32) * buffers.palette.count, false);

        REPORT_PROGRESS(
            TaskManager::reportProgress(2, frameCount);
            TaskManager::unregLevel(2);
        );
        stream.writeValue<int32_t>(int32_t(masks.size()));
        for (size_t i = 0; i < masks.size(); i++) {
            masks[i].write(stream, material.root, width, height);
        }

        audioInfo.write(stream, material.root, buffers.audioBuffer);
        return true;
    }
}