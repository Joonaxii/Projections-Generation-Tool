#pragma once
#include <J-Core/Gui/IGuiExtras.h>
#include <ImageUtils/ImageUtilsGui.h>
#include <J-Core/IO/FileStream.h>
#include <J-Core/IO/Image.h>
#include <J-Core/IO/IOUtils.h>
#include <J-Core/Log.h>
#include <J-Core/Util/StringHelpers.h>
#include <J-Core/ThreadManager.h>

using namespace JCore;

namespace Projections {

    bool drawColorToAlphaSettings(const char* label, ColorToAlphaSettings& settings) {
        bool changed = false;
        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();

            changed |= ImGui::Checkbox("Premultiply##ClrAlph", &settings.premultiply);

            if (ImGui::SliderInt("Difference Floor##ClrAlph", &settings.diffFloor, 0, 0xFF, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                settings.diffCeil = std::max(settings.diffFloor, settings.diffCeil);
                changed = true;
            }
            if (ImGui::SliderInt("Difference Ceiling##ClrAlph", &settings.diffCeil, 0, 0xFF, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                settings.diffFloor = std::min(settings.diffFloor, settings.diffCeil);
                changed = true;
            }

            auto& refColor = settings.refColor;
            ImColor clr(refColor.r, refColor.g, refColor.b);
            if (ImGui::ColorEdit3("Reference Color##ClrAlph", &clr.Value.x)) {
                settings.refColor = Color32(
                    uint8_t(Math::clamp(clr.Value.x, 0.0f, 1.0f) * 255.0f),
                    uint8_t(Math::clamp(clr.Value.y, 0.0f, 1.0f) * 255.0f),
                    uint8_t(Math::clamp(clr.Value.z, 0.0f, 1.0f) * 255.0f));
                changed = true;
            }
            ImGui::Unindent();
        }
        return changed;
    }

    void singleImageClrToAlpha(ColorToAlphaSettings& clrToAlp) {
        if (ImGui::CollapsingHeader("Color to Alpha Settings##ClrToAlphaSingle")) {
            ImGui::Indent();
            static char tempPath[261]{ 0 };
            static bool isValid{ false };
            static bool isDirty{ false };

            static Color32 bgColor(128, 32, 32, 255);

            static ImageData mainImg{};
            static std::shared_ptr<Texture> texMain = std::make_shared<Texture>();
            static std::shared_ptr<Texture> texOutput = std::make_shared<Texture>();

            if (Gui::searchDialogLeft("PNG/BMP Image", 0x1, tempPath, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);

                bool isBmp = Helpers::endsWith(tempPath, ".bmp", false);
                isDirty = false;
                if (isValid) {

                    if (isBmp) { isDirty = Bmp::decode(tempPath, mainImg); }
                    else { isDirty = Png::decode(tempPath, mainImg); }
                    if (isDirty) {
                        texMain->create(mainImg.data, mainImg.format, mainImg.paletteSize, mainImg.width, mainImg.height, mainImg.flags);
                    }
                }
            }

            if (drawColorToAlphaSettings("Settings##Img", clrToAlp) && isValid) {
                isDirty |= true;
            }

            if (isDirty && mainImg.data) {
                if (doColorToAlpha(mainImg, clrToAlp)) {
                    texOutput->create(clrToAlp.output->data, clrToAlp.output->format, clrToAlp.output->paletteSize, clrToAlp.output->width, clrToAlp.output->height, clrToAlp.output->flags);
                }
                isDirty = false;
            }

            Gui::drawTexture(texMain, GUI_TEX_BLEND_INDEXED | GUI_TEX_INFO_BELOW | GUI_TEX_SPLIT_INDEXED, 360, 360, true, 0.1f, nullptr, nullptr, nullptr, &bgColor);
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            Gui::drawTexture(texOutput, GUI_TEX_INFO_BELOW, 360, 360, true, 0.1f, nullptr, nullptr, nullptr, &bgColor);
            ImGui::Spacing();

            ImGui::Unindent();
        }
    }

    void batchConvertClrToAlpha(bool changed, ImageUtilPanel& panel) {
        if (ImGui::CollapsingHeader("Color to Alpha##ClrToAlphaBatch")) {
            static char tempPath[261]{ 0 };
            static char outPath[261]{ 0 };
            static bool isValid{ false };
            static bool isValidOut{ false };

            static ColorToAlphaSettings clrToAlp;

            static const char* allowedFiles[]{
                ".png",
                ".bmp",
            };
  
            if (clrToAlp.output == nullptr) {
                clrToAlp.output = &panel.getOutput();
            }

            ImGui::PushID("ClrToAlpha");
            if (Gui::searchDialogLeft("Image Root", 0x1, tempPath)) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);
            }
            if (Gui::searchDialogLeft("Image Out", 0x1, outPath)) {
                JCORE_INFO("Selected Output Path'{0}'", outPath);
                isValidOut = IO::exists(outPath);
            }
      
            singleImageClrToAlpha(clrToAlp);

            ImGui::BeginDisabled(!isValid || !isValidOut);
            if (ImGui::Button("Apply Color to Alpha")) {

                TaskManager::beginTask(
                    [&panel]() {
                        TaskProgress prog{};
                        prog.setTitle("Applying color to alpha...");
                        prog.setMessage(" [0/0]");
                        prog.progress.clear();
                        prog.progress.setType(PROG_RangeFloat, 0);
                        prog.progress.setFlags(0, PROG_IsStepped, true);
                        TaskManager::reportProgress(prog);

                        FileStream stream{};
                        ImageData buffer{};
                        std::vector<fs::path> files{};

                        if (IO::getAllFilesByExt(tempPath, files, allowedFiles, 2)) {
                            char tempBuf[261]{ 0 };
                            uint32_t i = 0;
                            prog.progress.setProgress(0U, uint32_t(files.size()));
                            for (auto& path : files) {
                                std::string name = path.filename().string();
                                std::string fPath = path.string();
                                prog.setMessage("%s [%i/%zi]", name.c_str(), ++i, files.size());
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS | TASK_COPY_MESSAGE);

                                bool valid = false;
                                bool isBmp = false;
                                if (stream.open(fPath.c_str(), "rb")) {
                                    if (Helpers::endsWith(name.c_str(), ".bmp", false)) {
                                        valid = Bmp::decode(stream, buffer);
                                        isBmp = true;
                                    }
                                    else {
                                        valid = Png::decode(stream, buffer);
                                    }
                                    stream.close();
                                }

                                if (valid) {
                                    if (doColorToAlpha(buffer, clrToAlp)) {
                                        sprintf_s(tempBuf, "%s/%s%s", outPath, path.stem().string().c_str(), isBmp ? ".bmp" : ".png");
                                        stream.open(tempBuf, "wb");
                     
                                        if (isBmp) { Bmp::encode(stream, panel.getOutput()); }
                                        else { Png::encode(stream, panel.getOutput()); }
                                        stream.close();
                                    }
                                }
                                prog.progress.setProgress(i);
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            }

                            buffer.clear(true);
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                        }
                    },
                    []() {});

            }
            ImGui::EndDisabled();

            ImGui::PopID();
        }
    }

    bool drawReductionSettings(const char* label, ColorReductionSettings& settings) {
        bool changed = false;
        if (ImGui::CollapsingHeader(label)) {
            changed |= ImGui::Checkbox("Apply Dithering##Color", &settings.applyDithering);

            int32_t val = settings.maxDepth;
            changed |= ImGui::SliderInt("Bit Depth##Color", &val, 1, 8, "%d", ImGuiSliderFlags_AlwaysClamp);
            settings.maxDepth = uint8_t(val);
        }
        return changed;
    }

    bool drawReductionSettings(const char* label, AlphaReductionSettings& settings) {
        bool changed = false;
        if (ImGui::CollapsingHeader(label)) {
            changed |= ImGui::Checkbox("Apply Dithering##Alpha", &settings.applyDithering);
            int32_t val = settings.maxDepth;
            changed |= ImGui::SliderInt("Bit Depth##Alpha", &val, 1, 8, "%d", ImGuiSliderFlags_AlwaysClamp);
            settings.maxDepth = uint8_t(val);
        }
        return changed;
    }

    void drawColorReductionMenu(const char* label, bool changed, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static bool isValid{ false };

        static std::shared_ptr<Texture> texIn = std::make_shared<Texture>();
        static std::shared_ptr<Texture> texOut = std::make_shared<Texture>();

        static bool isDirty = false;

        auto& imgIn = panel.getInput();
        auto& imgOut = panel.getOutput();
        auto& settings = panel.getSettings();
        if (ImGui::CollapsingHeader(label)) {
            isDirty |= changed;
            ImGui::Indent();
            if (Gui::searchDialogLeft("PNG/BMP Image", 0x1, tempPath, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);

                bool isBmp = Helpers::endsWith(tempPath, ".bmp", false);
                isDirty = false;
                if (isValid) {

                    if (isBmp) { isDirty = Bmp::decode(tempPath, imgIn); }
                    else { isDirty = Png::decode(tempPath, imgIn); }
                    if (isDirty) {
                        texIn->create(imgIn.data, imgIn.format, imgIn.paletteSize, imgIn.width, imgIn.height, imgIn.flags);
                    }
                }
            }

            isDirty |= drawReductionSettings("Color Settings", panel.getColorSettings());
            isDirty |= drawReductionSettings("Alpha Settings", panel.getAlphaSettings());

            if (isDirty && imgIn.data) {
                doColorReduction(imgIn, settings.convertTo4Bit(), panel.getColorSettings(), settings.applyRGB(), settings.applyAlpha() ? &panel.getAlphaSettings() : nullptr);
                texOut->create(imgOut.data, imgOut.format, imgOut.paletteSize, imgOut.width, imgOut.height, imgOut.flags);
            }

            Gui::drawTexture(texIn, GUI_TEX_BLEND_INDEXED | GUI_TEX_INFO_BELOW | GUI_TEX_SPLIT_INDEXED, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            Gui::drawTexture(texOut, GUI_TEX_INFO_BELOW, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);
            ImGui::Spacing();

            ImGui::Unindent();
        }
    }

    void drawBatchReduction(const char* label, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static bool isValid{ false };       
        
        static char outPath[261]{ 0 };
        static bool isValidOut{ false };
        static const char* allowedFiles[]{
            ".png",
            ".bmp",
        };
 
        static bool isDirty{ true };
        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            ImGui::PushID(label);
            if (Gui::searchDialogLeft("Image Root", 0x1, tempPath)) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);
            }
                       
            
            if (Gui::searchDialogLeft("Image Out", 0x1, outPath)) {
                JCORE_INFO("Selected Output Path'{0}'", outPath);
                isValidOut = IO::exists(outPath);
            }

            drawReductionSettings("Color Settings", panel.getColorSettings());
            drawReductionSettings("Alpha Settings", panel.getAlphaSettings());

            ImGui::BeginDisabled(!isValid || !isValidOut);
            if (ImGui::Button("Apply Reduction")) {

                TaskManager::beginTask(
                    [&panel]() {
                        TaskProgress prog{};
                        prog.setTitle("Applying color reduction...");
                        prog.setMessage(" [0/0]");
                        prog.progress.clear();
                        prog.progress.setType(PROG_RangeFloat, 0);
                        prog.progress.setFlags(0, PROG_IsStepped, true);
                        TaskManager::reportProgress(prog);

                        FileStream stream{};
                        ImageData buffer{};
                        std::vector<fs::path> files{};

                        if (IO::getAllFilesByExt(tempPath, files, allowedFiles, 2)) {
  
                            char tempBuf[261]{ 0 };
                            uint32_t i = 0;
                            prog.progress.setProgress(0U, uint32_t(files.size()));
                            for (auto& path : files) {
                                std::string name = path.filename().string();
                                std::string fPath = path.string();
                                prog.setMessage("%s [%i/%zi]", name.c_str(), ++i, files.size());
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS | TASK_COPY_MESSAGE);

                                bool valid = false;
                                bool isBmp = false;
                                if (stream.open(fPath.c_str(), "rb")) {
                                    if (Helpers::endsWith(name.c_str(), ".bmp", false)) {
                                        valid = Bmp::decode(stream, buffer);
                                        isBmp = true;
                                    }
                                    else {
                                        valid = Png::decode(stream, buffer);
                                    }
                                    stream.close();
                                }

                                if (valid) {
                                    auto& settings = panel.getSettings();
                                    bool convertToRGBA4 = settings.convertTo4Bit();
                                    if (doColorReduction(buffer, convertToRGBA4, panel.getColorSettings(), settings.applyRGB(), settings.applyAlpha() ? &panel.getAlphaSettings() : nullptr)) {
                                        sprintf_s(tempBuf, "%s/%s%s", outPath, path.stem().string().c_str(), convertToRGBA4 ? ".dds" : isBmp ? ".bmp" : ".png");
                                        stream.open(tempBuf, "wb");

                                        if (convertToRGBA4) {
                                            DDS::encode(stream, panel.getOutput());
                                        }
                                        else {
                                            if (isBmp) { Bmp::encode(stream, panel.getOutput()); }
                                            else { Png::encode(stream, panel.getOutput()); }
                                        }

                                        stream.close();
                                    }
                                }
                                prog.progress.setProgress(i, uint32_t(files.size()));
                            }

                            buffer.clear(true);
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                        }
                    },
                    []() {});

            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::Unindent();
        }
    }

    bool drawTemporalSettings(const char* label, TemporalLowpassSettings& settings) {
        bool changed = false;
        if (ImGui::CollapsingHeader(label)) {
            ImGui::PushID(label);
            ImGui::Indent();
            changed |= ImGui::SliderFloat("Coefficient##Temporal", &settings.coefficient, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Unindent();
            ImGui::PopID();
        }
        return changed;
    }
    void drawTemporalLowpass(const char* label, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static bool isValid{ false };       
        static char outPath[261]{ 0 };
        static bool isValidOut{ false };
        static const char* allowedFiles[]{
            ".png",
            ".bmp",
        };

        static bool isDirty{ true };
        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            ImGui::PushID(label);
            if (Gui::searchDialogLeft("Image Root", 0x1, tempPath)) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);
            }           
            
            if (Gui::searchDialogLeft("Image Out", 0x1, outPath)) {
                JCORE_INFO("Selected Output Path'{0}'", outPath);
                isValidOut = IO::exists(outPath);
            }

            drawTemporalSettings("Settings##Temporal", panel.getTemporalSettings());

            ImGui::BeginDisabled(!isValid || !isValidOut);
            if (ImGui::Button("Apply Temporal Lowpass")) {

                TaskManager::beginTask(
                    [&panel]() {
                        TaskProgress prog{};
                        prog.setTitle("Applying temporal lowpass...");
                        prog.setMessage(" [0/0]");
                        prog.progress.clear();
                        prog.progress.setType(PROG_RangeFloat, 0);
                        prog.progress.setFlags(0, PROG_IsStepped, false);
                        TaskManager::reportProgress(prog);

                        FileStream stream{};
                        ImageData prev{};
                        ImageData buffer{};
                        std::vector<fs::path> files{};

                        bool first = true;
                        if (IO::getAllFilesByExt(tempPath, files, allowedFiles, 2)) {
                            IO::sortByName(files);
                            char tempBuf[261]{ 0 };
                            uint32_t i = 0;
                            prog.progress.setProgress(0U, uint32_t(files.size()));
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            for (auto& path : files) {
                                std::string name = path.filename().string();
                                std::string fPath = path.string();
                                prog.setMessage("%s [%i/%zi]", name.c_str(), ++i, files.size());
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS | TASK_COPY_MESSAGE);

                                bool valid = false;
                                bool isBmp = false;
                                if (stream.open(fPath.c_str(), "rb")) {
                                    if (Helpers::endsWith(name.c_str(), ".bmp", false)) {
                                        valid = Bmp::decode(stream, buffer);
                                        isBmp = true;
                                    }
                                    else {
                                        valid = Png::decode(stream, buffer);
                                    }
                                    stream.close();
                                }

                                if (first) {
                                    prev.width = buffer.width;
                                    prev.height = buffer.height;
                                    prev.format = buffer.hasAlpha() ? TextureFormat::RGBA32 : TextureFormat::RGB24;
                                    if (prev.doAllocate()) {
                                        if (prev.format == buffer.format) {
                                            memcpy(prev.data, buffer.data, buffer.getSize());
                                        }
                                        else {
                                            Color32 temp{};
                                            temp.a = 0xFF;
                                            int32_t bpp = getBitsPerPixel(prev.format) >> 3;
                                            int32_t bppC = getBitsPerPixel(buffer.format) >> 3;
                                            int32_t reso = prev.width * prev.height;

                                            uint8_t* dataC = buffer.getData();
                                            for (size_t i = 0, p = 0, pB = 0; i < reso; i++, p += bpp, pB += bppC) {
                                                convertPixel(buffer.format, buffer.data, dataC + pB, temp);
                                                memcpy(prev.data + p, &temp, bpp);
                                            }
                                        }
                                      
                                    }
                                    first = false;
                                }

                                if (valid) {   
                                    auto& settings = panel.getSettings();
                                    if (doTemporalLowpass(prev, buffer, panel.getTemporalSettings())) {
                                        sprintf_s(tempBuf, "%s/%s%s", outPath, path.stem().string().c_str(), isBmp ? ".bmp" : ".png");
                                        stream.open(tempBuf, "wb");

                                        if (isBmp) { Bmp::encode(stream, prev); }
                                        else { Png::encode(stream, prev); }

                                        stream.close();
                                    }
                                }
                                prog.progress.setProgress(i, uint32_t(files.size()));
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            }

                            prev.clear(true);
                            buffer.clear(true);
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                        }
                    },
                    []() {});

            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::Unindent();
        }
    }

    void drawMorphologicalSingle(const char* label, bool changed, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static char tempPathAND[261]{ 0 };
        static char tempPathOR[261]{ 0 };
        static bool isValid{ false };
        static bool isDirty{ false };

        static Bitset bitSet{};
        static Bitset andMask{};
        static Bitset orMask{};
        static ImageData tempBuff{};
        static std::shared_ptr<Texture> texIn = std::make_shared<Texture>();
        static std::shared_ptr<Texture> texOut = std::make_shared<Texture>();
        static std::shared_ptr<Texture> texOutColor = std::make_shared<Texture>();

        if (ImGui::CollapsingHeader(label)) {
            auto& imgIn = panel.getInput();
            auto& imgOut = panel.getOutput();

            ImGui::PushID(label);
            isDirty |= changed;
            ImGui::Indent();
            if (Gui::searchDialogLeft("PNG/BMP Image", 0x1, tempPath, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);

                bool isBmp = Helpers::endsWith(tempPath, ".bmp", false);
                isDirty = false;
                if (isValid) {

                    if (isBmp) { isDirty = Bmp::decode(tempPath, imgIn); }
                    else { isDirty = Png::decode(tempPath, imgIn); }
                    if (isDirty) {
                        texIn->create(imgIn.data, imgIn.format, imgIn.paletteSize, imgIn.width, imgIn.height, imgIn.flags);
                    }
                }
            }

            if (Gui::searchDialogLeft("PNG/BMP Image (OR Mask)", 0x1, tempPathOR, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPathOR);

                bool isBmp = Helpers::endsWith(tempPathOR, ".bmp", false);
                if (IO::exists(tempPathOR)) {

                    bool can = false;
                    if (isBmp) { can |= Bmp::decode(tempPathOR, tempBuff); }
                    else { can |= Png::decode(tempPathOR, tempBuff); }
                    if (can) {
                        orMask.resize(tempBuff.width * tempBuff.height);

                        Color32 tmpC{};
                        uint8_t* dataSt = tempBuff.getData();
                        size_t bpp = getBitsPerPixel(tempBuff.format) >> 3;
                        for (size_t i = 0, yP = 0; i < orMask.size(); i++, yP += bpp) {
                            convertPixel(tempBuff.format, tempBuff.data, dataSt + yP, tmpC);
                            orMask.set(i, tmpC.r > 127);
                        }
                        isDirty |= true;
                    }
                    else {
                        orMask.setAll(false);
                    }
                }
                else {
                    orMask.setAll(false);
                }
            }

            if (Gui::searchDialogLeft("PNG/BMP Image (AND Mask)", 0x1, tempPathAND, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPathAND);

                bool isBmp = Helpers::endsWith(tempPathAND, ".bmp", false);
                if (IO::exists(tempPathAND)) {

                    bool can = false;
                    if (isBmp) { can |= Bmp::decode(tempPathAND, tempBuff); }
                    else { can |= Png::decode(tempPathAND, tempBuff); }
                    if (can) {
                        andMask.resize(tempBuff.width * tempBuff.height);

                        Color32 tmpC{};
                        uint8_t* dataSt = tempBuff.getData();
                        size_t bpp = getBitsPerPixel(tempBuff.format) >> 3;
                        for (size_t i = 0, yP = 0; i < andMask.size(); i++, yP += bpp) {
                            convertPixel(tempBuff.format, tempBuff.data, dataSt+ yP, tmpC);
                            andMask.set(i, tmpC.r > 127);
                        }
                        isDirty |= true;
                    }
                    else {
                        andMask.setAll(true);
                    }
                }
                else {
                    andMask.setAll(true);
                }
            }

            if (isDirty && imgIn.data) {
                bitSet.resize(size_t(imgIn.width) * imgIn.height);
                if (doMorphPasses(bitSet, imgIn, panel.getMorphSettings())) {

                    if (orMask.size() == bitSet.size()) {
                        bitSet.orWith(orMask);
                    }

                    if (andMask.size() == bitSet.size()) {
                        bitSet.andWith(andMask);
                    }
          
                    imgOut.format = TextureFormat::RGBA32;
                    imgOut.width = imgIn.width;
                    imgOut.height = imgIn.height;
                    if (imgOut.doAllocate()) {
                        int32_t reso = imgOut.width * imgOut.height;
                        Color32* colors = reinterpret_cast<Color32*>(imgOut.data);
                        for (int32_t i = 0; i < reso; i++) {
                            colors[i] = bitSet[i] ? Color32::White : Color32::Clear;
                        }
                        texOut->create(imgOut.data, imgOut.format, imgOut.paletteSize, imgOut.width, imgOut.height, imgOut.flags);

                        Color32 tmpC{};
                        uint8_t* dataSt = imgIn.getData();
                        size_t bppI = getBitsPerPixel(imgIn.format) >> 3;
                        for (size_t i = 0, yPP = 0; i < reso; i++, yPP += bppI) {
                            if (colors[i].r > 0) {
                                convertPixel(imgIn.format, imgIn.data, dataSt + yPP, tmpC);
                                memcpy(&colors[i], &tmpC, 4);
                            }
                        }
                        texOutColor->create(imgOut.data, imgOut.format, imgOut.paletteSize, imgOut.width, imgOut.height, imgOut.flags);
                    }
                }
                isDirty = false;
            }

            Gui::drawTexture(texIn, GUI_TEX_BLEND_INDEXED | GUI_TEX_INFO_BELOW | GUI_TEX_SPLIT_INDEXED, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            Gui::drawTexture(texOut, GUI_TEX_INFO_BELOW, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);            ImGui::SameLine();
            ImGui::SameLine();
            Gui::drawTexture(texOutColor, GUI_TEX_INFO_BELOW, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);
            ImGui::Spacing();

            ImGui::Unindent();
            ImGui::PopID();
        }
    }
    void drawMorphological(const char* label, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static bool isValid{ false };      
        static char outPath[261]{ 0 };
        static bool isValidOut{ false };
        static const char* allowedFiles[]{
            ".png",
            ".bmp",
        };

        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            ImGui::PushID(label);

            bool changed = false;
            changed |= Gui::drawGui("Settings##Morph", panel.getMorphSettings());

            drawMorphologicalSingle("Preview##Morph", changed, panel);

            if (Gui::searchDialogLeft("Image Root", 0x1, tempPath)) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);
            }
            
            if (Gui::searchDialogLeft("Image Out", 0x1, outPath)) {
                JCORE_INFO("Selected Output Path'{0}'", outPath);
                isValidOut = IO::exists(outPath);
            }

            static Bitset andMask{};
            static Bitset orMask{};
            static ImageData tempBuff{};
            static char tempPathAND[261]{ 0 };
            static char tempPathOR[261]{ 0 };

            if (Gui::searchDialogLeft("PNG/BMP Image (OR Mask)", 0x1, tempPathOR, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPathOR);

                bool isBmp = Helpers::endsWith(tempPathOR, ".bmp", false);
                if (IO::exists(tempPathOR)) {

                    bool can = false;
                    if (isBmp) { can |= Bmp::decode(tempPathOR, tempBuff); }
                    else { can |= Png::decode(tempPathOR, tempBuff); }
                    if (can) {
                        orMask.resize(tempBuff.width * tempBuff.height);

                        Color32 tmpC{};
                        uint8_t* dataSt = tempBuff.getData();
                        size_t bpp = getBitsPerPixel(tempBuff.format) >> 3;
                        for (size_t i = 0, yP = 0; i < orMask.size(); i++, yP += bpp) {
                            convertPixel(tempBuff.format, tempBuff.data, dataSt + yP, tmpC);
                            orMask.set(i, tmpC.r > 127);
                        }
                    }
                    else {
                        orMask.setAll(false);
                    }
                }
                else {
                    orMask.setAll(false);
                }
            }

            if (Gui::searchDialogLeft("PNG/BMP Image (AND Mask)", 0x1, tempPathAND, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPathAND);

                bool isBmp = Helpers::endsWith(tempPathAND, ".bmp", false);
                if (IO::exists(tempPathAND)) {

                    bool can = false;
                    if (isBmp) { can |= Bmp::decode(tempPathAND, tempBuff); }
                    else { can |= Png::decode(tempPathAND, tempBuff); }
                    if (can) {
                        andMask.resize(tempBuff.width * tempBuff.height);

                        Color32 tmpC{};
                        uint8_t* dataSt = tempBuff.getData();
                        size_t bpp = getBitsPerPixel(tempBuff.format) >> 3;
                        for (size_t i = 0, yP = 0; i < andMask.size(); i++, yP += bpp) {
                            convertPixel(tempBuff.format, tempBuff.data, dataSt + yP, tmpC);
                            andMask.set(i, tmpC.r > 127);
                        }
                    }
                    else {
                        andMask.setAll(true);
                    }
                }
                else {
                    andMask.setAll(true);
                }
            }

            ImGui::BeginDisabled(!isValid || !isValidOut);
            if (ImGui::Button("Apply Moprhological Isolation")) {

                TaskManager::beginTask(
                    [&panel]() {
                        TaskProgress prog{};
                        prog.setTitle("Applying moprhological isolation...");
                        prog.setMessage(" [0/0]");
                        prog.progress.clear();
                        prog.progress.setType(PROG_RangeFloat, 0);
                        prog.progress.setFlags(0, PROG_IsStepped, true);
                        TaskManager::reportProgress(prog);

                        FileStream stream{};
                        ImageData buffer{};
                        ImageData bufferOut{};
                        Bitset bitset;
                        std::vector<fs::path> files{};

                        bool first = true;
                        if (IO::getAllFilesByExt(tempPath, files, allowedFiles, 2)) {

                            char tempBuf[261]{ 0 };
                            uint32_t i = 0;
                            prog.progress.setProgress(0U, uint32_t(files.size()));
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            for (auto& path : files) {
                                std::string name = path.filename().string();
                                std::string fPath = path.string();
                                prog.setMessage("%s [%i/%zi]", name.c_str(), ++i, files.size());
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS | TASK_COPY_MESSAGE);

                                bool valid = false;
                                bool isBmp = false;
                                if (stream.open(fPath.c_str(), "rb")) {
                                    if (Helpers::endsWith(name.c_str(), ".bmp", false)) {
                                        valid = Bmp::decode(stream, buffer);
                                        isBmp = true;
                                    }
                                    else {
                                        valid = Png::decode(stream, buffer);
                                    }
                                    stream.close();
                                }

                                if (valid) {
                                    auto& settings = panel.getSettings();
                                    int32_t reso = buffer.width * buffer.height;
                                    bitset.resize(reso);
                                    if (doMorphPasses(bitset, buffer, panel.getMorphSettings())) {
                                    
                                        if (bitset.size() == orMask.size()) {
                                            bitset.orWith(orMask);
                                        }

                                        if (bitset.size() == andMask.size()) {
                                            bitset.andWith(andMask);
                                        }

                                        bufferOut.width = buffer.width;
                                        bufferOut.height = buffer.height;
                                        bufferOut.format = TextureFormat::RGBA32;

                                        bufferOut.doAllocate();

                                        size_t bpp = getBitsPerPixel(buffer.format) >> 3;
                                        uint8_t* dataSt = buffer.getData();
                                        Color32* clr32 = reinterpret_cast<Color32*>(bufferOut.data);
                                        for (size_t i = 0, yP = 0; i < reso; i++, yP += bpp)  {
                                            if (bitset[i]) {
                                                convertPixel(buffer.format, buffer.data, dataSt + yP, clr32[i]);
                                                continue;
                                            }
                                            clr32[i] = Color32::Clear;
                                        }

                                        sprintf_s(tempBuf, "%s/%s%s", outPath, path.stem().string().c_str(),  isBmp ? ".bmp" : ".png");
                                        stream.open(tempBuf, "wb");

                                        Png::encode(stream, bufferOut);
                                        stream.close();
                                    }
                                }
                                prog.progress.setProgress(i, uint32_t(files.size()));
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            }

                            buffer.clear(true);
                            bufferOut.clear(true);
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                        }
                    },
                    []() {});

            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::Unindent();
        }
    }

    void drawCrossFade(const char* label, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static bool isValid{ false };
        static char outPath[261]{ 0 };
        static bool isValidOut{ false };
        static const char* allowedFiles[]{
            ".png",
            ".bmp",
        };

        static bool isDirty{ true };
        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            ImGui::PushID(label);
            if (Gui::searchDialogLeft("Image Root", 0x1, tempPath)) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);
            }

            if (Gui::searchDialogLeft("Image Out", 0x1, outPath)) {
                JCORE_INFO("Selected Output Path'{0}'", outPath);
                isValidOut = IO::exists(outPath);
            }

            Gui::drawGui("Settings##CFade", panel.getCrossFadeSettings());

            ImGui::BeginDisabled(!isValid || !isValidOut);
            if (ImGui::Button("Apply Cross Fade")) {

                TaskManager::beginTask(
                    [&panel]() {
                        TaskProgress prog{};
                        prog.setTitle("Applying cross fade...");
                        prog.setMessage(" [0/0]");
                        prog.progress.clear();
                        prog.progress.setType(PROG_RangeInt, 0);
                        prog.progress.setFlags(0, PROG_IsStepped, false);
                        TaskManager::reportProgress(prog);

                        ImageData prev{};
                        ImageData current{};
                        ImageData buffer{};
                        std::vector<fs::path> files{};

                        bool first = true;
                        if (IO::getAllFilesByExt(tempPath, files, allowedFiles, 2)) {
                            IO::sortByName(files);
                            char tempBuf[261]{ 0 };
                            uint32_t i = 0;
     
                            auto& cSettings = panel.getCrossFadeSettings();

                            size_t filesLeft = files.size() - cSettings.startFrame;
                            size_t frameC = size_t(cSettings.frameRate * cSettings.crossfadeDuration);
                            if (frameC < 1) { return; }
                            if (frameC >= filesLeft) { return; }

                            size_t frameMin = frameC + cSettings.startFrame;
                            size_t frameMax = (files.size() - frameC);

                            DataFormat fmtIn{DataFormat::FMT_UNKNOWN};
                            DataFormat fmtOut{DataFormat::FMT_UNKNOWN};

                            char buff[261]{ 0 };
                            size_t tFrames = 0;
                            for (size_t i = 0; i < cSettings.startFrame; i++)
                            {
                                std::string fPath = files[i].string();
                                sprintf_s(buff, "%s/Frame_%zi%s", outPath, tFrames++, Format::getExtension(fPath.c_str()));
                                IO::copyTo(files[i], buff);
                            }

                            for (size_t i = frameMin; i < frameMax; i++)
                            {
                                std::string fPath = files[i].string();
                                sprintf_s(buff, "%s/Frame_%zi%s", outPath, tFrames++, Format::getExtension(fPath.c_str()));
                                IO::copyTo(files[i], buff);
                            }
                            size_t last = frameMax - (frameMin + frameC) + cSettings.startFrame;

                            prog.progress.setProgress(0U, uint32_t(frameC));
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);

                            for (size_t i = 0, j = files.size() - frameC - 1, k = cSettings.startFrame; i < frameC; i++, k++, j++) {
                                float nT = float(i) / (float(frameC) - 1.0f);

                                bool validLhs = false;
                                bool validRhs = false;
                                std::string fPath = files[j].string();
                                validLhs = Image::tryDecode(fPath.c_str(), prev, fmtIn);
                          
                                if (validLhs) {
                                    fPath = files[i + cSettings.startFrame].string();
                                    validRhs = Image::tryDecode(fPath.c_str(), current, fmtOut);
                                }

                                if (validLhs && validRhs) {
                                    if (doCrossFade(prev, current, nT, buffer, cSettings)) {   
                                        sprintf_s(buff, "%s/Frame_%zi%s", outPath, tFrames++, Format::getExtension(fmtOut));
                                        Image::tryEncode(buff, buffer, fmtOut);
                                    }
                                }

                                prog.progress.setProgress(uint32_t(i));
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            }
                            prev.clear(true);
                            current.clear(true);
                            buffer.clear(true);
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                        }
                    },
                    []() {});

            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::Unindent();
        }
    }

    void drawFade(const char* label, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static bool isValid{ false };
        static char outPath[261]{ 0 };
        static bool isValidOut{ false };
        static const char* allowedFiles[]{
            ".png",
            ".bmp",
        };

        static bool isDirty{ true };
        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            ImGui::PushID(label);
            if (Gui::searchDialogLeft("Image Root", 0x1, tempPath)) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);
            }

            if (Gui::searchDialogLeft("Image Out", 0x1, outPath)) {
                JCORE_INFO("Selected Output Path'{0}'", outPath);
                isValidOut = IO::exists(outPath);
            }

            Gui::drawGui("Settings##Fade", panel.getFadeSettings());

            ImGui::BeginDisabled(!isValid || !isValidOut);
            if (ImGui::Button("Apply Fade")) {

                TaskManager::beginTask(
                    [&panel]() {
                        TaskProgress prog{};
                        prog.setTitle("Applying fade...");
                        prog.setMessage(" [0/0]");
                        prog.progress.clear();
                        prog.progress.setType(PROG_RangeFloat, 0);
                        prog.progress.setFlags(0, PROG_IsStepped, false);
                        TaskManager::reportProgress(prog);

                        FileStream stream{};
                        ImageData prev{};
                        ImageData buffer{};
                        std::vector<fs::path> files{};

                        bool first = true;
                        if (IO::getAllFilesByExt(tempPath, files, allowedFiles, 2)) {
                            IO::sortByName(files);
                            char tempBuf[261]{ 0 };
                            uint32_t i = 0;

                            auto& cSettings = panel.getFadeSettings();

                            size_t filesLeft = files.size();
                            size_t frameC = size_t(cSettings.fadeFrames);
                            if (frameC < 1) { return; }
                            if (frameC >= filesLeft) { return; }

                            size_t frameMin = cSettings.fadeFrames;
                            size_t frameMax = (files.size() - frameC);

                            char buff[261]{ 0 };
                            for (size_t i = frameMin, j = 0; i < frameMax; i++, j++) {
                                std::string fPath = files[i].string();
                                sprintf_s(buff, "%s/Frame_%zi%s", outPath, j, Helpers::endsWith(fPath.c_str(), ".bmp", false) ? ".bmp" : ".png");
                                IO::copyTo(files[i], buff);
                            }

                            size_t end = frameMax - frameMin;
                            for (size_t i = 0; i < frameC; i++) {
                                float nT = (float(i) / std::max(float(frameC) - 1.0f, 1.0f)) * 0.5f;

                                bool validLhs = false;
                                bool isBmp = false;
                                std::string fPath = files[frameMax + i].string();
                                if (stream.open(fPath.c_str(), "rb")) {
                                    if (Helpers::endsWith(fPath.c_str(), ".bmp", false)) {
                                        validLhs = Bmp::decode(stream, prev);
                                        isBmp = true;
                                    }
                                    else {
                                        validLhs = Png::decode(stream, prev);
                                    }
                                    stream.close();
                                }

                                if (validLhs) {
                                    if (doFade(prev, nT, buffer, cSettings)) {
                                        sprintf_s(buff, "%s/Frame_%zi%s", outPath, end, isBmp ? ".bmp" : ".png");
                                        if (stream.open(buff, "wb")) {
                                            if (isBmp) {
                                                Bmp::encode(stream, buffer);
                                            }
                                            else {
                                                Png::encode(stream, buffer);
                                            }
                                            stream.close();
                                        }
                                    }
                                }

                                prog.progress.setProgress(uint32_t(end - frameMax));
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                                end++;
                            }

                            for (size_t i = 0; i < frameC; i++) {
                                float nT = (float(i) / std::max(float(frameC) - 1.0f, 1.0f)) * 0.5f;

                                bool validLhs = false;
                                bool isBmp = false;
                                std::string fPath = files[i].string();
                                if (stream.open(fPath.c_str(), "rb")) {
                                    if (Helpers::endsWith(fPath.c_str(), ".bmp", false)) {
                                        validLhs = Bmp::decode(stream, prev);
                                        isBmp = true;
                                    }
                                    else {
                                        validLhs = Png::decode(stream, prev);
                                    }
                                    stream.close();
                                }

                                if (validLhs) {
                                    if (doFade(prev, nT + 0.5f, buffer, cSettings)) {
                                        sprintf_s(buff, "%s/Frame_%zi%s", outPath, end, isBmp ? ".bmp" : ".png");
                                        if (stream.open(buff, "wb")) {
                                            if (isBmp) {
                                                Bmp::encode(stream, buffer);
                                            }
                                            else {
                                                Png::encode(stream, buffer);
                                            }
                                            stream.close();
                                        }
                                    }
                                }

                                prog.progress.setProgress(uint32_t(end - frameMax));
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                                end++;
                            }

                            prev.clear(true);
                            buffer.clear(true);
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                        }
                    },
                    []() {});

            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::Unindent();
        }
    }


    void drawChromaKeySingle(const char* label, bool changed, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static char tempPathAND[261]{ 0 };
        static char tempPathOR[261]{ 0 };
        static bool isValid{ false };
        static bool isDirty{ false };

        static ImageData tempBuff{};
        static std::shared_ptr<Texture> texIn = std::make_shared<Texture>();
        static std::shared_ptr<Texture> texOut = std::make_shared<Texture>();

        if (ImGui::CollapsingHeader(label)) {
            auto& imgIn = panel.getInput();
            auto& imgOut = panel.getOutput();

            ImGui::PushID(label);
            isDirty |= changed;
            ImGui::Indent();
            if (Gui::searchDialogLeft("PNG/BMP Image", 0x1, tempPath, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);

                bool isBmp = Helpers::endsWith(tempPath, ".bmp", false);
                isDirty = false;
                if (isValid) {

                    if (isBmp) { isDirty = Bmp::decode(tempPath, imgIn); }
                    else { isDirty = Png::decode(tempPath, imgIn); }
                    if (isDirty) {
                        texIn->create(imgIn.data, imgIn.format, imgIn.paletteSize, imgIn.width, imgIn.height, imgIn.flags);
                    }
                }
            }

            if (isDirty && imgIn.data) {
                if (doChromaKey(imgIn, tempBuff, panel.getChromaKeySettings())) {
                    texOut->create(tempBuff.data, tempBuff.format, tempBuff.paletteSize, tempBuff.width, tempBuff.height, tempBuff.flags);
                }
                isDirty = false;
            }

            Gui::drawTexture(texIn, GUI_TEX_BLEND_INDEXED | GUI_TEX_INFO_SIDE | GUI_TEX_SPLIT_INDEXED, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);
            ImGui::Separator();
            Gui::drawTexture(texOut, GUI_TEX_INFO_SIDE, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);            

            ImGui::Unindent();
            ImGui::PopID();
        }
    }

    void drawChromaKey(const char* label, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static bool isValid{ false };
        static char outPath[261]{ 0 };
        static char tempPathAND[261]{ 0 };
        static char tempPathOR[261]{ 0 };
        static bool useMasks{ false };
        static bool isValidOut{ false };
        static const char* allowedFiles[]{
            ".png",
            ".bmp",
        };

        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            ImGui::PushID(label);

            bool changed = false;
            changed |= Gui::drawGui("Settings##Chroma", panel.getChromaKeySettings());

            drawChromaKeySingle("Preview##Chroma", changed, panel);

            if (Gui::searchDialogLeft("Image Root", 0x1, tempPath)) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);
            }

            if (Gui::searchDialogLeft("Image Out", 0x1, outPath)) {
                JCORE_INFO("Selected Output Path'{0}'", outPath);
                isValidOut = IO::exists(outPath);
            }

            ImGui::Checkbox("Use Masks##Chroma", &useMasks);
            if (useMasks) {
                if (Gui::searchDialogLeft("OR Mask", 0x1, tempPathOR, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                    JCORE_INFO("Selected OR Mask'{0}'", tempPathOR);
                }
                             
                if (Gui::searchDialogLeft("AND Mask", 0x1, tempPathAND, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                    JCORE_INFO("Selected AND Mask'{0}'", tempPathAND);
                }
            }

            ImGui::BeginDisabled(!isValid || !isValidOut);
            if (ImGui::Button("Apply Chroma Key")) {

                TaskManager::beginTask(
                    [&panel]() {
                        TaskProgress prog{};
                        prog.setTitle("Applying chroma key...");
                        prog.setMessage(" [0/0]");
                        prog.progress.clear();
                        prog.progress.setType(PROG_RangeFloat, 0);
                        prog.progress.setFlags(0, PROG_IsStepped, true);
                        TaskManager::reportProgress(prog);

                        FileStream stream{};
                        ImageData buffer{};
                        ImageData bufferOut{};

                        ImageData bufferAND{};
                        ImageData bufferOR{};
                        int32_t masks = 0;
                        std::vector<fs::path> files{};

                        if (useMasks) {
                            bool endsBMP = false;
                            endsBMP = Helpers::endsWith(tempPathAND, ".bmp", false);
                            if (IO::exists(tempPathOR) && (endsBMP ? Bmp::decode(tempPathOR, bufferOR) : Png::decode(tempPathOR, bufferOR))) {
                                masks |= 0x2;
                                doGrayToAlpha(bufferOR);
                            }

                            endsBMP = Helpers::endsWith(tempPathAND, ".bmp", false);
                            if (IO::exists(tempPathAND) && (endsBMP ? Bmp::decode(tempPathAND, bufferAND) : Png::decode(tempPathAND, bufferAND))) {
                                masks |= 0x1;
                                doGrayToAlpha(bufferAND);
                            }

                            JCORE_TRACE("Using Masks: {0:b}", masks);
                        }
                        bool first = true;
                        if (IO::getAllFilesByExt(tempPath, files, allowedFiles, 2)) {

                            char tempBuf[261]{ 0 };
                            uint32_t i = 0;
                            prog.progress.setProgress(0U, uint32_t(files.size()));
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            for (auto& path : files) {
                                std::string name = path.filename().string();
                                std::string fPath = path.string();
                                prog.setMessage("%s [%i/%zi]", name.c_str(), ++i, files.size());
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS | TASK_COPY_MESSAGE);

                                bool valid = false;
                                bool isBmp = false;
                                if (stream.open(fPath.c_str(), "rb")) {
                                    if (Helpers::endsWith(name.c_str(), ".bmp", false)) {
                                        valid = Bmp::decode(stream, buffer);
                                        isBmp = true;
                                    }
                                    else {
                                        valid = Png::decode(stream, buffer);
                                    }
                                    stream.close();
                                }

                                if (valid) {
                                    auto& settings = panel.getSettings();
                                    int32_t reso = buffer.width * buffer.height;
                                    if (doChromaKey(buffer, bufferOut, panel.getChromaKeySettings(), ((masks & 0x1) ? &bufferAND : nullptr), ((masks & 0x2) ? &bufferOR : nullptr))) {
                                        sprintf_s(tempBuf, "%s/%s%s", outPath, path.stem().string().c_str(), isBmp ? ".bmp" : ".png");
                                        stream.open(tempBuf, "wb");

                                        if (isBmp) {
                                            Bmp::encode(stream, bufferOut);
                                        }
                                        else {
                                            Png::encode(stream, bufferOut);
                                        }
                                        stream.close();
                                    }
                                }
                                prog.progress.setProgress(i, uint32_t(files.size()));
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            }

                            buffer.clear(true);
                            bufferAND.clear(true);
                            bufferOR.clear(true);
                            bufferOut.clear(true);
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                        }
                    },
                    []() {});

            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::Unindent();
        }
    }


    void drawEdgeAlphaSingle(const char* label, bool changed, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static char tempPathAND[261]{ 0 };
        static char tempPathOR[261]{ 0 };
        static bool isValid{ false };
        static bool isDirty{ false };

        static ImageData tempBuff{};
        static std::shared_ptr<Texture> texIn = std::make_shared<Texture>();
        static std::shared_ptr<Texture> texOut = std::make_shared<Texture>();

        if (ImGui::CollapsingHeader(label)) {
            auto& imgIn = panel.getInput();
            auto& imgOut = panel.getOutput();

            ImGui::PushID(label);
            isDirty |= changed;
            ImGui::Indent();
            if (Gui::searchDialogLeft("PNG/BMP Image", 0x1, tempPath, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);

                bool isBmp = Helpers::endsWith(tempPath, ".bmp", false);
                isDirty = false;
                if (isValid) {

                    if (isBmp) { isDirty = Bmp::decode(tempPath, imgIn); }
                    else { isDirty = Png::decode(tempPath, imgIn); }
                    if (isDirty) {
                        texIn->create(imgIn.data, imgIn.format, imgIn.paletteSize, imgIn.width, imgIn.height, imgIn.flags);
                    }
                }
            }

            //if (isDirty && imgIn.data) {
            //    if (doChromaKey(imgIn, tempBuff, panel.getEdgeFadeSettings())) {
            //        texOut->create(tempBuff.data, tempBuff.format, tempBuff.paletteSize, tempBuff.width, tempBuff.height, tempBuff.flags);
            //    }
            //    isDirty = false;
            //}

            Gui::drawTexture(texIn, GUI_TEX_BLEND_INDEXED | GUI_TEX_INFO_SIDE | GUI_TEX_SPLIT_INDEXED, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);
            ImGui::Separator();
            Gui::drawTexture(texOut, GUI_TEX_INFO_SIDE, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);

            ImGui::Unindent();
            ImGui::PopID();
        }
    }
    void drawEdgeAlpha(const char* label, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static bool isValid{ false };
        static char outPath[261]{ 0 };
        static bool isValidOut{ false };
        static const char* allowedFiles[]{
            ".png",
            ".bmp",
        };

        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            ImGui::PushID(label);

            bool changed = false;
            changed |= Gui::drawGui("Settings##Chroma", panel.getChromaKeySettings());

            drawChromaKeySingle("Preview##Chroma", changed, panel);

            if (Gui::searchDialogLeft("Image Root", 0x1, tempPath)) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);
            }

            if (Gui::searchDialogLeft("Image Out", 0x1, outPath)) {
                JCORE_INFO("Selected Output Path'{0}'", outPath);
                isValidOut = IO::exists(outPath);
            }

            static ImageData tempBuff{};
            static char tempPathAND[261]{ 0 };
            static char tempPathOR[261]{ 0 };

            ImGui::BeginDisabled(!isValid || !isValidOut);
            if (ImGui::Button("Apply Chroma Key")) {

                TaskManager::beginTask(
                    [&panel]() {
                        TaskProgress prog{};
                        prog.setTitle("Applying chroma key...");
                        prog.setMessage(" [0/0]");
                        prog.progress.clear();
                        prog.progress.setType(PROG_RangeFloat, 0);
                        prog.progress.setFlags(0, PROG_IsStepped, true);
                        TaskManager::reportProgress(prog);

                        FileStream stream{};
                        ImageData buffer{};
                        ImageData bufferOut{};
                        std::vector<fs::path> files{};

                        bool first = true;
                        if (IO::getAllFilesByExt(tempPath, files, allowedFiles, 2)) {

                            char tempBuf[261]{ 0 };
                            uint32_t i = 0;
                            prog.progress.setProgress(0U, uint32_t(files.size()));
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            for (auto& path : files) {
                                std::string name = path.filename().string();
                                std::string fPath = path.string();
                                prog.setMessage("%s [%i/%zi]", name.c_str(), ++i, files.size());
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS | TASK_COPY_MESSAGE);

                                bool valid = false;
                                bool isBmp = false;
                                if (stream.open(fPath.c_str(), "rb")) {
                                    if (Helpers::endsWith(name.c_str(), ".bmp", false)) {
                                        valid = Bmp::decode(stream, buffer);
                                        isBmp = true;
                                    }
                                    else {
                                        valid = Png::decode(stream, buffer);
                                    }
                                    stream.close();
                                }

                                if (valid) {
                                    auto& settings = panel.getSettings();
                                    int32_t reso = buffer.width * buffer.height;
                                    if (doChromaKey(buffer, bufferOut, panel.getChromaKeySettings())) {
                                        sprintf_s(tempBuf, "%s/%s%s", outPath, path.stem().string().c_str(), isBmp ? ".bmp" : ".png");
                                        stream.open(tempBuf, "wb");

                                        if (isBmp) {
                                            Bmp::encode(stream, bufferOut);
                                        }
                                        else {
                                            Png::encode(stream, bufferOut);
                                        }
                                        stream.close();
                                    }
                                }
                                prog.progress.setProgress(i, uint32_t(files.size()));
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            }

                            buffer.clear(true);
                            bufferOut.clear(true);
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                        }
                    },
                    []() {});

            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::Unindent();
        }
    }

    void drawAlphaMaskSingle(const char* label, char* maskPath, ImageData& mask, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static bool isValid{ false };
        static bool isDirty{ false };

        static ImageData tempBuff{};
        static std::shared_ptr<Texture> texIn = std::make_shared<Texture>();
        static std::shared_ptr<Texture> texOut = std::make_shared<Texture>();
        static std::shared_ptr<Texture> maskTex = std::make_shared<Texture>();

        if (ImGui::CollapsingHeader(label)) {
            auto& imgIn = panel.getInput();
            auto& imgOut = panel.getOutput();

            ImGui::PushID(label);
            ImGui::Indent();

            if (Gui::searchDialogLeft("PNG/BMP Image (Mask)", 0x1, maskPath, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", maskPath);
                isValid = IO::exists(maskPath);

                bool isBmp = Helpers::endsWith(maskPath, ".bmp", false);
                isDirty = false;
                if (isValid) {
                    if (isBmp) { isDirty |= Bmp::decode(maskPath, mask); }
                    else { isDirty |= Png::decode(maskPath, mask); }
                    if (isDirty) {
                        doGrayToAlpha(mask);
                        maskTex->create(mask.data, mask.format, mask.paletteSize, mask.width, mask.height, mask.flags);
                    }
                }
            }

            if (Gui::searchDialogLeft("PNG/BMP Image (Preview)", 0x1, tempPath, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);

                bool isBmp = Helpers::endsWith(tempPath, ".bmp", false);
                isDirty = false;
                if (isValid) {
                    if (isBmp) { isDirty |= Bmp::decode(tempPath, imgIn); }
                    else { isDirty |= Png::decode(tempPath, imgIn); }
                    if (isDirty) {
                        texIn->create(imgIn.data, imgIn.format, imgIn.paletteSize, imgIn.width, imgIn.height, imgIn.flags);
                    }
                }
            }

            if (isDirty && imgIn.data && mask.data) {
                if (doAlphaMask(imgIn, mask, imgOut)) {
                    texOut->create(imgOut.data, imgOut.format, imgOut.paletteSize, imgOut.width, imgOut.height, imgOut.flags);
                }
                isDirty = false;
            }

            Gui::drawTexture(texIn, GUI_TEX_BLEND_INDEXED | GUI_TEX_INFO_SIDE | GUI_TEX_SPLIT_INDEXED, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);
            ImGui::Separator();
            Gui::drawTexture(maskTex, GUI_TEX_INFO_SIDE, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);
            ImGui::Separator();
            Gui::drawTexture(texOut, GUI_TEX_INFO_SIDE, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);

            ImGui::Unindent();
            ImGui::PopID();
        }
    }
    void drawAlphaMask(const char* label, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static char maskPath[261]{ 0 };
        static char maskRefsPath[261]{ 0 };
        static bool isValid{ false };
        static bool isValidRef{ false };
        static char outPath[261]{ 0 };
        static bool useBatch{ false };
        static bool isValidOut{ false };
        static ImageData mask{};

        static const char* allowedFiles[]{
            ".png",
            ".bmp",
        };

        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            ImGui::PushID(label);

            ImGui::Checkbox("Use Batch##Masking", &useBatch);
            if (useBatch) {
                if (Gui::searchDialogLeft("Mask Ref Root", 0x1, maskRefsPath)) {
                    JCORE_INFO("Selected Path'{0}'", maskRefsPath);
                    isValidRef = IO::exists(maskRefsPath);
                }
            }
            else {
                drawAlphaMaskSingle("Preview##Chroma", maskPath, mask, panel);
            }


            if (Gui::searchDialogLeft("Image Root", 0x1, tempPath)) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);
            }

            if (Gui::searchDialogLeft("Image Out", 0x1, outPath)) {
                JCORE_INFO("Selected Output Path'{0}'", outPath);
                isValidOut = IO::exists(outPath);
            }

            ImGui::BeginDisabled(!isValid || (useBatch ? !isValidRef : !isValidOut && mask.data));
            if (ImGui::Button("Apply Alpha Mask")) {

                TaskManager::beginTask(
                    [&panel]() {
                        TaskProgress prog{};
                        prog.setTitle("Applying alpha mask...");
                        prog.setMessage(" [0/0]");
                        prog.progress.clear();
                        prog.progress.setType(PROG_RangeFloat, 0);
                        prog.progress.setFlags(0, PROG_IsStepped, true);
                        TaskManager::reportProgress(prog);

                        FileStream stream{};
                        ImageData buffer{};
                        ImageData bufferOut{};
                        ImageData bufferRef{};
                        std::vector<fs::path> files{};
                        std::vector<fs::path> filesRef{};

                        bool first = true;
                        if (IO::getAllFilesByExt(tempPath, files, allowedFiles, 2)) {
                            if (useBatch && isValidRef && (!IO::getAllFilesByExt(maskRefsPath, filesRef, allowedFiles, 2) || filesRef.size() < 1)) {
                                return;
                            }

                            char tempBuf[261]{ 0 };
                            uint32_t i = 0;
                            prog.progress.setProgress(0U, uint32_t(files.size()));
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            int ii = 0;
                            for (auto& path : files) {
                                std::string name = path.filename().string();
                                std::string fPath = path.string();
                                prog.setMessage("%s [%i/%zi]", name.c_str(), ++i, files.size());
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS | TASK_COPY_MESSAGE);

                                bool valid = false;
                                bool isBmp = false;
                                if (stream.open(fPath.c_str(), "rb")) {
                                    if (Helpers::endsWith(name.c_str(), ".bmp", false)) {
                                        valid = Bmp::decode(stream, buffer);
                                        isBmp = true;
                                    }
                                    else {
                                        valid = Png::decode(stream, buffer);
                                    }
                                    stream.close();
                                }

                                if (valid) {

                                    if (useBatch) {
                                        auto& fRef = filesRef[ii % filesRef.size()];
                                        name = fRef.filename().string();
                                        fPath = fRef.string();

                                        if (stream.open(fPath.c_str(), "rb")) {
                                            bool isV = false;
                                            if (Helpers::endsWith(name.c_str(), ".bmp", false)) {
                                                isV = Bmp::decode(stream, bufferRef);
                                            }
                                            else {
                                                isV = Png::decode(stream, bufferRef);
                                            }
                                            stream.close();
                                            if (!isV) { goto end; }
                                        }
                                        else { goto end; }
                                    }

                                    auto& settings = panel.getSettings();
                                    int32_t reso = buffer.width * buffer.height;
                                    if (doAlphaMask(buffer, useBatch ? bufferRef :  mask, bufferOut)) {
                                        sprintf_s(tempBuf, "%s/%s%s", outPath, path.stem().string().c_str(), isBmp ? ".bmp" : ".png");
                                        stream.open(tempBuf, "wb");

                                        if (isBmp) {
                                            Bmp::encode(stream, bufferOut);
                                        }
                                        else {
                                            Png::encode(stream, bufferOut);
                                        }
                                        stream.close();
                                    }
                                }

                                end:
                                ii++;
                                prog.progress.setProgress(i, uint32_t(files.size()));
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            }

                            buffer.clear(true);
                            bufferOut.clear(true);
                            bufferRef.clear(true);
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                        }
                    },
                    []() {});

            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::Unindent();
        }
    }


    void drawImageMaskSingle(const char* label, char* maskPath, int32_t& minDist, int32_t& maxDist, ImageData& mask, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static bool isValid{ false };
        static bool isDirty{ false };

        static ImageData tempBuff{};
        static std::shared_ptr<Texture> texIn = std::make_shared<Texture>();
        static std::shared_ptr<Texture> texOut = std::make_shared<Texture>();
        static std::shared_ptr<Texture> maskTex = std::make_shared<Texture>();

        if (ImGui::CollapsingHeader(label)) {
            auto& imgIn = panel.getInput();
            auto& imgOut = panel.getOutput();

            ImGui::PushID(label);
            ImGui::Indent();

            if (Gui::searchDialogLeft("PNG/BMP Image (Mask)", 0x1, maskPath, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", maskPath);
                isValid = IO::exists(maskPath);

                bool isBmp = Helpers::endsWith(maskPath, ".bmp", false);
                isDirty = false;
                if (isValid) {
                    if (isBmp) { isDirty |= Bmp::decode(maskPath, mask); }
                    else { isDirty |= Png::decode(maskPath, mask); }
                    if (isDirty) {
                        maskTex->create(mask.data, mask.format, mask.paletteSize, mask.width, mask.height, mask.flags);
                    }
                }
            }

            if (Gui::searchDialogLeft("PNG/BMP Image (Preview)", 0x1, tempPath, "PNG File (*.png)\0*.png\0BMP File (*.bmp)\0*.bmp\0")) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);

                bool isBmp = Helpers::endsWith(tempPath, ".bmp", false);
                isDirty = false;
                if (isValid) {
                    if (isBmp) { isDirty |= Bmp::decode(tempPath, imgIn); }
                    else { isDirty |= Png::decode(tempPath, imgIn); }
                    if (isDirty) {
                        texIn->create(imgIn.data, imgIn.format, imgIn.paletteSize, imgIn.width, imgIn.height, imgIn.flags);
                    }
                }
            }

            isDirty |= ImGui::SliderInt("Min Distance##ImageMask", &minDist, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
            isDirty |= ImGui::SliderInt("Max Distance##ImageMask", &maxDist, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);

            if (isDirty && imgIn.data && mask.data) {
                if (doImageMask(minDist, maxDist, imgIn, mask, imgOut)) {
                    texOut->create(imgOut.data, imgOut.format, imgOut.paletteSize, imgOut.width, imgOut.height, imgOut.flags);
                }
                isDirty = false;
            }

            Gui::drawTexture(texIn, GUI_TEX_BLEND_INDEXED | GUI_TEX_INFO_SIDE | GUI_TEX_SPLIT_INDEXED, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);
            ImGui::Separator();
            Gui::drawTexture(maskTex, GUI_TEX_INFO_SIDE, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);
            ImGui::Separator();
            Gui::drawTexture(texOut, GUI_TEX_INFO_SIDE, 360, 360, true, 0.1f, nullptr, nullptr, nullptr);

            ImGui::Unindent();
            ImGui::PopID();
        }
    }
    void drawImageMask(const char* label, ImageUtilPanel& panel) {
        static char tempPath[261]{ 0 };
        static char maskPath[261]{ 0 };
        static bool isValid{ false };
        static char outPath[261]{ 0 };
        static bool isValidOut{ false };
        static int32_t minDist = 0x00;
        static int32_t maxDist = 0x08;
        static ImageData mask{};

        static const char* allowedFiles[]{
            ".png",
            ".bmp",
        };

        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            ImGui::PushID(label);

            drawImageMaskSingle("Preview##Mask", maskPath, minDist, maxDist, mask, panel);

            if (Gui::searchDialogLeft("Image Root", 0x1, tempPath)) {
                JCORE_INFO("Selected Path'{0}'", tempPath);
                isValid = IO::exists(tempPath);
            }

            if (Gui::searchDialogLeft("Image Out", 0x1, outPath)) {
                JCORE_INFO("Selected Output Path'{0}'", outPath);
                isValidOut = IO::exists(outPath);
            }

            ImGui::BeginDisabled(!isValid || !isValidOut && mask.data);
            if (ImGui::Button("Apply Image Mask")) {

                TaskManager::beginTask(
                    [&panel]() {
                        TaskProgress prog{};
                        prog.setTitle("Applying image mask...");
                        prog.setMessage(" [0/0]");
                        prog.progress.clear();
                        prog.progress.setType(PROG_RangeFloat, 0);
                        prog.progress.setFlags(0, PROG_IsStepped, true);
                        TaskManager::reportProgress(prog);

                        FileStream stream{};
                        ImageData buffer{};
                        ImageData bufferOut{};
                        std::vector<fs::path> files{};

                        bool first = true;
                        if (IO::getAllFilesByExt(tempPath, files, allowedFiles, 2)) {

                            char tempBuf[261]{ 0 };
                            uint32_t i = 0;
                            prog.progress.setProgress(0U, uint32_t(files.size()));
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            for (auto& path : files) {
                                std::string name = path.filename().string();
                                std::string fPath = path.string();
                                prog.setMessage("%s [%i/%zi]", name.c_str(), ++i, files.size());
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS | TASK_COPY_MESSAGE);

                                bool valid = false;
                                bool isBmp = false;
                                if (stream.open(fPath.c_str(), "rb")) {
                                    if (Helpers::endsWith(name.c_str(), ".bmp", false)) {
                                        valid = Bmp::decode(stream, buffer);
                                        isBmp = true;
                                    }
                                    else {
                                        valid = Png::decode(stream, buffer);
                                    }
                                    stream.close();
                                }

                                if (valid) {
                                    auto& settings = panel.getSettings();
                                    int32_t reso = buffer.width * buffer.height;
                                    if (doImageMask(minDist, maxDist, buffer, mask, bufferOut)) {
                                        sprintf_s(tempBuf, "%s/%s%s", outPath, path.stem().string().c_str(), isBmp ? ".bmp" : ".png");
                                        stream.open(tempBuf, "wb");

                                        if (isBmp) {
                                            Bmp::encode(stream, bufferOut);
                                        }
                                        else {
                                            Png::encode(stream, bufferOut);
                                        }
                                        stream.close();
                                    }
                                }
                                prog.progress.setProgress(i, uint32_t(files.size()));
                                TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                            }

                            buffer.clear(true);
                            bufferOut.clear(true);
                            TaskManager::reportProgress(prog, TASK_COPY_PROGRESS);
                        }
                    },
                    []() {});

            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::Unindent();
        }
    }


    ImageUtilPanel::~ImageUtilPanel() {
        _inImg.clear(true);
        _outImg.clear(true);
    }

    void ImageUtilPanel::init() {
        IGuiPanel::init();
        _color.output = &_outImg;
        loadSettings();
    }

    void ImageUtilPanel::draw() {
        IGuiPanel::draw();
        bool changed = false;
        if (changed = Gui::drawGui("Settings##ImageUtils", _settings)) {
            saveSettings();
        }

        drawBatchReduction("Bit-Depth Reduction", *this);
        batchConvertClrToAlpha(changed, *this);
        drawTemporalLowpass("Temporal Lowpass##ImageUtils", *this);
        drawMorphological("Morphological Isolation##ImageUtils", *this);
        drawCrossFade("Cross Fade##ImageUtils", *this);
        drawFade("Fade##ImageUtils", *this);
        drawChromaKey("Chroma Key##ImageUtils", *this);
        drawAlphaMask("Alpha Mask##ImageUtils", *this);
        drawImageMask("Image Mask##ImageUtils", *this);
    }

    void ImageUtilPanel::loadSettings() {
        FileStream fs{};
        if (fs.open("ImageUtil-Settings.json", "rb")) {
            char* temp = reinterpret_cast<char*>(_malloca(fs.size() + 1));
            if (temp) {
                _settings.reset();

                temp[fs.size()] = 0;
                fs.read(temp, fs.size(), 1);

                json jsonF = json::parse(temp, temp + fs.size(), nullptr, false, true);
                _freea(temp);
                _settings.read(jsonF);
                jsonF.clear();
            }
            fs.close();
        }
    }

    void ImageUtilPanel::saveSettings() {
        json jsonF = json::object_t();
        _settings.write(jsonF);

        FileStream fs{};
        if (fs.open("ImageUtil-Settings.json", "wb")) {
            auto dump = jsonF.dump(4);
            fs.write(dump.c_str(), dump.length(), 1, false);
            fs.close();
        }
    }
}