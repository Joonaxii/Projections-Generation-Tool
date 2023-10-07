#include <ProjectionGen.h>
#include <algorithm>
#include <J-Core/IO/FileStream.h>
#include <J-Core/Log.h>
#include <J-Core/IO/Image.h>
#include <J-Core/Util/StringHelpers.h>
#include <J-Core/ThreadManager.h>

using namespace JCore;

namespace Projections {

    bool processDir(Projection& painting, const fs::path& root) {
        if (!IO::exists(root)) { return false; }

        std::vector<IO::FilePath> images{};
        auto& framesPath = painting.framesPaths;
        framesPath.clear();
        framesPath.resize(8);

        size_t maxLayers = 0;
        if (IO::getAll(root, IO::F_TYPE_FILE, images, true)) {
            static const char* exts[]{
                ".png",
                ".bmp",
                ".dds",
                ".jtex"
            };

            for (auto& path : images) {
                auto name = path.path.string();
                if (Helpers::endsWith(name.c_str(), "Icon.png", false)) { continue; }

                Span<char> spn(&name[0], name.length());
                size_t end = Helpers::lastIndexOf(spn.get(), exts, false);

                if (end == Span<char>::npos) { continue; }
                spn = spn.slice(0, end);
                PFrameIndex index(spn.get(), spn.length());

                maxLayers = std::max<size_t>(index.layer + 1, maxLayers);

                if (framesPath.size() < maxLayers) {
                    framesPath.resize(maxLayers + 1);
                }
                framesPath[index.layer].addPath(name, index);
            }
        }

        framesPath.resize(maxLayers);
        for (auto& path : framesPath) {
            path.apply();
        }
        return true;
    }

    void calculateSizes(Projection& projection) {
        ImageData imgDat{};
        projection.maxTiles = 0;

        bool sizeCalcualted = false;
        for (auto& path : projection.framesPaths) {
            size_t i = 0;
            for (auto& pth : path.paths) {
                if (!sizeCalcualted && pth.isValid() && pth.getInfo(imgDat)) {
                    int32_t w = imgDat.width >> 4;
                    int32_t h = imgDat.height >> 4;

                    auto& layer = projection.layers[i++];

                    layer.width = std::max<uint16_t>(uint16_t(w), layer.width);
                    layer.height = std::max<uint16_t>(uint16_t(h), layer.height);

                    projection.width = std::max<uint16_t>(uint16_t(w), projection.width);
                    projection.height = std::max<uint16_t>(uint16_t(h), projection.height);

                    if (projection.hasSameResolution) {
                        while (i < projection.layers.size()) {
                            auto& lr = projection.layers[i++];
                            lr.width = std::max<uint16_t>(uint16_t(w), lr.width);
                            lr.height = std::max<uint16_t>(uint16_t(h), lr.height);
                        }
                        sizeCalcualted = true;
                        break;
                    }
                }
            }

            size_t frames = 0;
            for (auto& var : path.variations) {
                frames += var.frames * (var.hasEmission ? 2 : 1);
            }

            size_t reso = size_t(projection.width) * projection.height;
            reso *= frames * path.variationCount;
            projection.maxTiles += reso;
        }
        projection.tiles.reserve(projection.maxTiles);
    }

    void Projection::load(json& cfg, const fs::path& root, const std::string& rootName) {
        using namespace JCore;
        reset();
        this->rootName = rootName;
        this->rootPath = root;

        bool isValid = processDir(*this, root);
        read(cfg, isValid);

        if (isValid) {
            calculateSizes(*this);
        }
    }

    void Projection::load(ProjectionGroup& group, bool premultiply, uint8_t alphaClip, ImageData& imgDat) {
        size_t startC = group.rawTiles.size();

        fs::path iconPath(rootPath);
        iconPath += "/Icon.png";
        material.icon = IO::exists(iconPath) ? group.addIcon(iconPath) : -1;

        TaskProgress prog{};
        prog.flags = TaskProgress::HAS_SUB_TASK | TaskProgress::HAS_SUB_RELATIVE;
        prog.setSubMessage("'%s'", "");
        prog.subProgress.setProgress(0U, 0);
        prog.subProgress.setStep(0U, 0);
        prog.subProgress.setType(PROG_RangeInt, 0);
        prog.subProgress.setType(PROG_RangeInt, 1);

        prog.subProgress.setFlags(0, PROG_IsInclusive | PROG_ShowRange, false);
        prog.subProgress.setFlags(1, PROG_IsInclusive | PROG_ShowRange, false);
        prog.subProgress.setFlags(0, PROG_IsStepped, true);

        TaskManager::reportProgress(prog, TASK_COPY_SUBMESSAGE | TASK_COPY_FLAGS | TASK_COPY_SUBPROGRESS);

        uint32_t layerC = uint32_t(framesPaths.size());

        FileStream stream{};
        PixelTile temp[4]{};
        PixelBlockCH tBlocks{};

        temp[0].setRotation(0);
        temp[1].setRotation(1);
        temp[2].setRotation(2);
        temp[3].setRotation(3);

        tiles.clear();
        int32_t reso = width * height;
        tiles.reserve(reso * layerC);
        prog.subProgress.setProgress(0, layerC);
        size_t start = group.rawPIXTiles.size();
        for (uint32_t i = 0; i < layerC; i++) {
            auto& layer = layers[i];
            auto& layerPath = framesPaths[i];
            //TODO: Possibly figure out bounds etc, might not be needed

            int32_t total = 0;

            prog.setSubMessage("Calculating Buffers '%s'", layer.name.c_str());

            prog.subProgress.setProgress(i + 1);
            prog.subProgress.setStep(0U);
            TaskManager::reportProgress(prog, TASK_COPY_SUBMESSAGE | TASK_COPY_SUBPROGRESS);

            size_t emis = 0;

            PixelTile tempTile{};
            CRCBlock crcBuf{};
            size_t pI = 0;

            prog.subProgress.setStep(0U, uint32_t(layerPath.paths.size() * reso));
            prog.setSubMessage("Building Tiles '%s' [%zi, %08X]", layer.name.c_str(), 0, group.rawTiles.size());
            TaskManager::reportProgress(prog, TASK_COPY_SUBMESSAGE | TASK_COPY_SUBPROGRESS);

            uint32_t stepP = 0;
            for (auto& img : layerPath.paths) {
                bool isEmission = img.index.variation.getFlag();
                if (img.isValid() && img.decodeImage(imgDat)) {
                    auto& vari = layer.variations[img.index.variation.getValue()];
                    prog.setSubMessage("Building Tiles '%s' [%zi, %08X]", layer.name.c_str(), pI++, group.rawTiles.size());
                    prog.subProgress.setStep(stepP);
                    TaskManager::reportProgress(prog, TASK_COPY_SUBPROGRESS | TASK_COPY_SUBMESSAGE);

                    uint32_t current = 0;
                    int32_t w = imgDat.width >> 4;
                    int32_t h = imgDat.height >> 4;

                    if (width != w || height != h) {
                        JCORE_WARN("[Projections] Warning: Resolutions don't match! ({0}x{1} =/= {2}x{3})", width, height, w, h);
                        break;
                    }

                    if (isEmission) {
                        for (int32_t y = 0, yP = 0; y < h; y++, yP += (imgDat.width << 4)) {
                            for (int32_t x = 0, xP = yP; x < w; x++, xP += 16) {
                                auto& tileID = tiles[emis++];
                                if (tileID == 0) {
                                    tiles.push_back(PixelTileIndex());

                                    stepP++;
                                    prog.subProgress.setStep(stepP);
                                    TaskManager::reportProgress(prog, TASK_COPY_SUBPROGRESS);
                                    continue;
                                }

                                temp[0].readFrom(tBlocks, imgDat, xP, alphaClip, crcBuf);
                                if (!vari.directEmission) {
                                    temp[0].maskWith(tBlocks[0], group.rawPIXTiles[tileID.getIndex()][tileID.getRotation()], alphaClip, crcBuf);
                                }
                                bool isEmpty = temp[0].isEmpty();

                                if (!isEmpty) {
                                    if (premultiply) {
                                        temp[0].doPremultiply(tBlocks, crcBuf);
                                    }
                                    temp[1].copyFlipped(tBlocks, tBlocks, temp[0], 1, crcBuf);
                                    temp[2].copyFlipped(tBlocks, tBlocks, temp[0], 2, crcBuf);
                                    temp[3].copyFlipped(tBlocks, tBlocks, temp[0], 3, crcBuf);
                                }

                                tiles.push_back(group.addTile(crcBuf, temp, tBlocks.blocks, start));
                                stepP++;
                                prog.subProgress.setStep(stepP);
                            }
                        }
                    }
                    else {
                        for (int32_t y = 0, yP = 0; y < h; y++, yP += (imgDat.width << 4)) {
                            for (int32_t x = 0, xP = yP; x < w; x++, xP += 16) {
                                temp[0].readFrom(tBlocks, imgDat, xP, alphaClip, crcBuf);
                                bool isEmpty = temp[0].isEmpty();

                                if (!isEmpty) {
                                    if (premultiply) {
                                        temp[0].doPremultiply(tBlocks, crcBuf);
                                    }
                                    temp[1].copyFlipped(tBlocks, tBlocks, temp[0], 1, crcBuf);
                                    temp[2].copyFlipped(tBlocks, tBlocks, temp[0], 2, crcBuf);
                                    temp[3].copyFlipped(tBlocks, tBlocks, temp[0], 3, crcBuf);
                                }
                                tiles.push_back(group.addTile(crcBuf, temp, tBlocks.blocks, start));

                                stepP++;
                                prog.subProgress.setStep(stepP);
                                TaskManager::reportProgress(prog, TASK_COPY_SUBPROGRESS);
                            }
                        }
                    }
                }
                else {
                    if (isEmission) { emis += reso; }
                    tiles.insert(tiles.end(), reso, PixelTileIndex());

                    stepP += reso;
                    prog.subProgress.setStep(stepP);
                    prog.setSubMessage("Building Tiles'%s' [%zi]", layer.name.c_str(), pI++);
                    TaskManager::reportProgress(prog, TASK_COPY_SUBPROGRESS | TASK_COPY_SUBMESSAGE);
                }
            }
        }
        prog.subProgress.setStep(prog.subProgress.step[1].uintV);
        TaskManager::reportProgress(prog, TASK_COPY_SUBPROGRESS);

        JCORE_INFO("Built projection '{0}' ({1}% saved)", name.c_str(), (1.0f - (float((group.rawTiles.size() - startC)) / std::max<float>(float(tiles.size()), 1.0f))) * 100.0f);
    }

    void Projection::write(const ProjectionGroup& group, const Stream& stream, const std::vector<PixelTileCH>& rawTiles, const PixelBlockVector& rawBlocks) const {
        writeShortString(name, stream);
        writeShortString(description, stream);

        stream.writeValue(uint8_t(width - 1));
        stream.writeValue(uint8_t(height - 1));

        stream.writeValue(maxLookDistance);
        stream.write(lookOffset, sizeof(lookOffset));

        stream.writeValue<int32_t>(int32_t(tiles.size()));
        TileData tempTile{};

        uint32_t rotations[4]{ 0 };

        for (auto& tData : tiles) {
            uint8_t rot = tData.getRotation();
            const auto& tile = rawTiles[tData.getIndex()][0];

            if (tile.isEmpty()) {
                rotations[0]++;
                stream.writeValue(NullTile);
                continue;
            }
            rotations[rot]++;
            tempTile.setX(tile.x);
            tempTile.setY(tile.y);
            tempTile.setAtlas(tile.data);
            tempTile.setRotation(rot);
            tempTile.setColor(tile.getColor(rawBlocks[tData.getIndex()][rot]));
            stream.writeValue(tempTile);
        }

        int32_t layerC = int32_t(framesPaths.size());
        stream.writeValue<int32_t>(layerC);
        for (size_t i = 0; i < layerC; i++) {
            layers[i].write(stream, framesPaths[i]);
        }
        material.write(stream);
    }

    void ProjectionGroup::write(const std::string& name, const OutputFormat& atlasFormat, const Stream& stream, std::vector<Projection*>& projections) const {
        uint32_t flagsAndVersion = (PROJ_GEN_VERSION & 0x00FFFFFFU) | (atlasFormat.type << 24U);

        stream.writeValue(flagsAndVersion);
        writeShortString(name, stream);
        stream.writeValue<uint8_t>(uint8_t(atlases.size() & 0x7F));
        stream.writeValue<int16_t>(int16_t(icons.size()));
        stream.writeValue<int16_t>(int16_t(projections.size()));

        size_t tiles = 0;
        for (auto& proj : projections) {
            if (proj->isValid) {
                proj->write(*this, stream, rawTiles, rawPIXTiles);
                tiles += proj->tiles.size();
            }
        }

        JCORE_INFO("Built group '{0}' ({1}% total saved)", name.c_str(), (1.0f - (float(rawTiles.size()) / std::max<float>(float(tiles), 1.0f))) * 100.0f);
    }

    void ProjectionGroup::doSave(const std::string& name, const OutputFormat& atlasFormat, bool premultiplyIcons, bool isDX9, char* path, char* dirName, JCore::ImageData img, std::vector<Projection*>& projections) {
        size_t len = strlen(path);

        sprintf_s(dirName, 512 - len, "/%s", name.c_str());
        IO::createDirectory(path);
        size_t lenName = strlen(dirName);
        size_t tLen = 512 - (len + lenName);
        char* fileName = dirName + lenName;

        buildAtlases(isDX9);
        saveAtlases(img, atlasFormat, path);

        int32_t num = 0;
        for (auto& icon : icons) {
            sprintf_s(fileName, tLen, "/Icon_%i.png", num++);
            if (premultiplyIcons) {
                if (Png::decode(icon.string().c_str(), img)) {
                    switch (img.format) {
                        default:
                            IO::copyTo(icon, path);
                            continue;

                        case TextureFormat::RGBA32:
                            premultiplyC32(reinterpret_cast<Color32*>(img.data), size_t(img.width) * img.height);
                            break;

                        case TextureFormat::Indexed8:
                        case TextureFormat::Indexed16:
                            premultiplyC32(reinterpret_cast<Color32*>(img.data), img.paletteSize);
                            break;
                    }
                    if (Png::encode(path, img, 6)) { continue; }
                }
            }
            IO::copyTo(icon, path);
        }
        sprintf_s(fileName, tLen, "/Config.pdata");

        FileStream fs(path, "wb");
        if (fs.isOpen()) {
            write(name, atlasFormat, fs, projections);
        }
        fs.close();
    }

    void ProjectionGroup::buildAtlases(bool isDX9) {
        atlases.clear();

        JCORE_TRACE("[Projections] Number of tiles {0}", rawTiles.size());

        std::vector<AtlasDefiniton> defs{};
        if (packSpritesFixed<16, 16, 1>(isDX9 ? 4096 : 8192, rawTiles.size(), defs)) {
            uint8_t atlas = 0;
            for (auto& def : defs) {
                atlases.emplace_back().setup(atlas++, def, rawTiles);
                if (atlas > 0x1F) { break; }
            }
        }
    }

    void ProjectionGroup::saveAtlases(JCore::ImageData img, const OutputFormat& atlasFormat, const char* root) const {
        char buffer[512]{ 0 };
        size_t len = strlen(root);

        memcpy(buffer, root, len);
        buffer[len] = 0;

        char* name = buffer + len;

        IO::fixPath(buffer);

        FileStream stream = FileStream();
        if (img.data) {
            uint8_t num = 0;
            size_t left = 512 - len;
            for (auto& atlas : atlases) {
                sprintf_s(name, left, "/Atlas_%i%s", num, atlasFormat.getExtension());

                stream.close();
                if (stream.open(buffer, "wb")) {
                    atlas.saveToPNG(1, num, atlasFormat, img, stream, rawTiles, rawPIXTiles);
                }

                num++;
                stream.close();
                if (num > 0x1F) { break; }
            }
        }
    }

    int16_t ProjectionGroup::addIcon(const fs::path& path) {
        int16_t iconId = int16_t(icons.size());
        icons.emplace_back(path);
        return iconId;
    }

    PixelTileIndex ProjectionGroup::addTile(CRCBlock& crc, PixelTile tiles[4], PixelBlock blocks[4], size_t start) {
        if (tiles[0].isEmpty()) { return PixelTileIndex(); }

        PixelTileIndex ind = useSlow ? indexOfTileMemcmp(crc[0], blocks[0]) : indexOfTileSIMD(crc[0], blocks[0], start);
        if (ind.getData() != UINT32_MAX) {
            return ind;
        }
        uint32_t idx = uint32_t(rawTiles.size());
        ind.setRotation(0);
        ind.setIndex(idx);

        rawPIXTiles.emplace_back(blocks);
        rawTiles.emplace_back(tiles);
        crcTiles.emplace_back(crc);
        return ind;
    }

    void ProjectionConfig::save() const {
        json jsonF = json::object_t();
        jsonF["group"] = group;
        jsonF["ignore"] = ignore;

        json arr = json::object_t();
        for (auto& paint : projections) {
            json obj = json::object_t();
            paint.write(obj);
            arr[paint.rootName] = obj;
        }
        jsonF["projections"] = arr;

        FileStream fs{};
        if (fs.open(root.string(), "wb")) {
            auto dump = jsonF.dump(4);
            fs.write(dump.c_str(), dump.length(), 1, false);
            fs.close();
        }
    }

    void ProjectionConfig::load(const fs::path& root) {
        clear();
        this->root = root;

        FileStream fs{};
        if (fs.open(root.string(), "rb")) {
            char* temp = reinterpret_cast<char*>(_malloca(fs.size() + 1));
            if (temp) {
                temp[fs.size()] = 0;
                fs.read(temp, fs.size(), 1);

                json jsonF = json::parse(temp, temp + fs.size(), nullptr, false, true);
                _freea(temp);

                group = IO::readString(jsonF["group"], "");
                ignore = jsonF.value("ignore", false);

                char buffer[512]{ 0 };
                std::string str = root.parent_path().string();
                size_t namePos = str.length();
                memcpy(buffer, str.c_str(), str.length());
                buffer[str.length()] = 0;

                char* nameStr = buffer + namePos;

                auto& paints = jsonF["projections"].items();
                for (auto& kvp : paints) {
                    const std::string& str = kvp.key();
                    sprintf_s(nameStr, 260, "/%s", str.c_str());
                    auto pathOut = fs::path(buffer);
                    projections.emplace_back().load(kvp.value(), pathOut, str);
                }
                jsonF.clear();
            }
            fs.close();
        }
    }
}