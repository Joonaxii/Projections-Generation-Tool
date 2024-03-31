#include <ProjectionsGui.h>
#include <J-Core/Log.h>
#include <J-Core/TaskManager.h>
using namespace JCore;

namespace Projections {

    bool doNameIDGui(const char* label, std::string& id) {
        bool changed = ImGui::InputText(label, &id);
        Projections::ProjectionIndex idx(id);
        ImGui::TextDisabled("Hash: [0x%08X - 0x%08X]", idx.lo, idx.hi);
        return changed;
    }

    static bool doNameGui(const char* label, std::string& name, float width, bool doInline) {
        float avail = (width - (doInline ? 10 : 0));

        ImGui::SetNextItemWidth(avail * (doInline ? 0.75f : 1.0f));
        bool changed = ImGui::InputText(label, &name);
        uint32_t hash = calculateNameHash(name);
        if (doInline) {
            ImGui::SameLine();
        }
        ImGui::SetNextItemWidth(avail * (doInline ? 0.25f : 1.0f));
        ImGui::TextDisabled(doInline ? "[0x%08X]" : "Hash: [0x%08X]", hash);
        return changed;
    }

    static bool drawProjection(const char* label, Projection& proj) {
        bool changed = false;
        ImGui::PushID(label);
        if (ImGui::CollapsingHeaderNoId(proj.material.nameID.length() < 1 ? "<No ID>" : proj.material.nameID.c_str(), label)) {
            ImGui::Indent();

            if (ImGui::CollapsingHeader("Info")) {
                ImGui::Indent();
                ImGui::Text("[Frame Info]");
                ImGui::TextDisabled(" - Num. of Frames: %zi", proj.frames.size() / Math::max<size_t>(proj.layers.size(), 1));
                ImGui::TextDisabled(" - Num. of Layers: %zi", Math::max<size_t>(proj.layers.size(), 1));

                ImGui::Text("[Audio Info]");
                if (proj.audioInfo.hasValidAudio()) {
                    auto& audio = proj.audioInfo.variants[0].audio;
                    ImGui::TextDisabled(" - Num. of Audio Variants: %zi", proj.audioInfo.variants.size());
                    ImGui::TextDisabled(" - Sample Rate: %zi", size_t(audio.sampleRate));
                    ImGui::TextDisabled(" - Sample Count: %zi", size_t(audio.sampleCount));
                    ImGui::TextDisabled(" - Num. of Channels: %zi", size_t(audio.channels));
                }
                else {
                    ImGui::TextDisabled(" - No Audio or Audio Invalid!");
                }

                ImGui::Unindent();
            }

            changed |= Gui::drawGuiInline("Material##Projection", proj.material);
            changed |= ImGui::InputText("Frame Path##Projection", &proj.framePath);
            changed |= Gui::drawEnumList("Animation Mode##Projection", proj.animMode);

            if (ImGui::CollapsingHeader("Tags")) {
                bool tagsChanged = false;
                ImGui::Indent();
                for (size_t i = 0; i < proj.tags.size(); i++) {
                    ImGui::PushID(int32_t(i));
                    if (ImGui::Button("+")) {
                        proj.duplicateTagAt(i);
                        tagsChanged |= true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::SameLine();

                    if (ImGui::Button("-")) {
                        proj.removeTagAt(i);
                        tagsChanged |= true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::SameLine();

                    tagsChanged |= ImGui::InputText("##Tag", &proj.tags[i].value);
                    ImGui::PopID();
                }

                if (proj.tags.size() < 1 && ImGui::Button("Add New Tag")) {
                    proj.tags.emplace_back().reset();
                    tagsChanged = true;
                }

                ImGui::Unindent();
                changed |= tagsChanged;

                if (tagsChanged) {
                    proj.refreshTags();
                }
            }

            if (ImGui::CollapsingHeader("Raw Tags")) {
                ImGui::Indent();
                for (auto& tag : proj.rawTags) {
                    ImGui::TextDisabled("%s", tag.c_str());
                }
                ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader("Masks")) {
                ImGui::Indent();
                for (size_t i = 0; i < proj.masks.size(); i++) {
                    ImGui::PushID(int32_t(i));
                    if (ImGui::Button("+")) {
                        proj.duplicateMaskAt(i);
                        changed |= true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::SameLine();

                    if (ImGui::Button("-")) {
                        proj.removeMaskAt(i);
                        changed |= true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::SameLine();

                    changed |= ImGui::Checkbox("Compressed", &proj.masks[i].compress);
                    ImGui::SameLine();

                    changed |= doNameGui("##Name", proj.masks[i].name, 240.0f, true);
                    ImGui::SameLine();
                    if (Gui::searchDialogLeft("Mask Path", 0x1, proj.masks[i].path, "PNG File (*.png)\0*.png\0\0")) {
                        Gui::clearGuiInput("##Name");
                        IO::eraseRoot(proj.masks[i].path, proj.material.root);
                        proj.masks[i].checkName();
                        changed = true;
                    }


                    ImGui::PopID();
                }

                if (proj.masks.size() < 1 && ImGui::Button("Add New Mask")) {
                    proj.masks.emplace_back().reset();
                    changed = true;
                }

                ImGui::Unindent();
            }
            ImGui::Unindent();
        }
        ImGui::PopID();
        return changed;
    }

    ProjectionGenPanel::~ProjectionGenPanel() {
        _buffers.clear();
    }

    void ProjectionGenPanel::init() {
        IGuiPanel::init();
        loadSettings();

        static constexpr size_t MAX_SIZE = 1024;
        static constexpr size_t MAX_SAMPLES = 480000 * 16;
        _buffers.init(MAX_SIZE, MAX_SAMPLES);
    }

    static void sanitizeFileName(std::string& str) {
        for (size_t i = 0; i < str.size(); i++) {
            char& c = str[i];
            switch (c)
            {
            case '/':
            case '\\':
            case '?':
            case ';':
            case ':':
            case '\"':
            case '\'':
            case '!':
            case ',':
                c = '-';
                break;
            }
        }
    }

    static constexpr char HEADER[] = "PDAT";

    static void exportData(uint8_t flags, ProjectionGenPanel* panel) {
        if (flags == 0 || !panel) { return; }

        std::string outPath = IO::openFolder("");
        if (IO::exists(outPath)) {
            TaskManager::beginTask([outPath, flags, panel]()
                {
                    const ImageData* preview = &panel->getBuffers().frameBuffer;
                    auto& sources = panel->getSources();

                    std::vector<Projection*> projections{};
                    std::vector<PMaterial*> materials{};
                    std::vector<PBundle*> bundles{};

                    size_t tP = 0;
                    size_t tM = 0;
                    size_t tB = 0;
                    for (auto& src : sources) {
                        uint8_t toExport = flags & src.isValid;
                        tP += src.totalProjections;
                        projections.reserve(tP);

                        tM += src.totalMaterials;
                        materials.reserve(tM);

                        tB += src.totalBundles;
                        bundles.reserve(tB);

                        if (src.isValidDir() && toExport != 0) {
                            if (toExport & ProjectionSource::VALID_PROJECTIONS) {
                                for (auto& gr : src.projections) {
                                    if (gr.noExport) { continue; }
                                    for (auto& itm : gr.projections) {
                                        if (itm.material.noExport) { continue; }
                                        projections.push_back(&itm);
                                    }
                                }
                            }

                            if (toExport & ProjectionSource::VALID_MATERIALS) {
                                for (auto& gr : src.materials) {
                                    if (gr.noExport) { continue; }
                                    for (auto& itm : gr.materials) {
                                        if (itm.noExport) { continue; }
                                        materials.push_back(&itm);
                                    }
                                }
                            }

                            if (toExport & ProjectionSource::VALID_MATERIALS) {
                                for (auto& gr : src.bundles) {
                                    if (gr.noExport) { continue; }
                                    for (auto& itm : gr.bundles) {
                                        if (itm.material.noExport) { continue; }
                                        bundles.push_back(&itm);
                                    }
                                }
                            }
                        }
                    }

                    REPORT_PROGRESS(
                        TaskManager::setTaskTitle("Exporting...");
                        TaskManager::reportPreview(nullptr);

                        TaskManager::regLevel(0);
                        TaskManager::regLevel(1);
                        TaskManager::regLevel(2);

                        TaskManager::report(0, "Exporting Projections...");
                        TaskManager::report(1, "Writing...");
                        TaskManager::reportProgress(0, 0.0, -1, (bundles.size() > 0 ? 1 : 0) + (materials.size() > 0 ? 1 : 0) + (projections.size() > 0 ? 1 : 0));
                        TaskManager::reportProgress(1, 0.0, -2, projections.size());
                        TaskManager::reportProgress(2, 0.0, 0.0, 0.0);
                    );

                    FileStream fs{};
                    std::string outFile{};
                    std::string outFileTmp{};
                    std::string outName{};
                    char tmpSize[128]{};
                    char tmpInfo[64]{};

                    std::chrono::steady_clock::time_point time{};
                    for (auto& proj : projections) {
                        REPORT_PROGRESS(
                            TaskManager::report(1, "Writing... %s", proj->material.nameID.c_str());
                        );

                        outName = proj->material.nameID;
                        sanitizeFileName(outName);
                        outName.append(".pdat");

                        outFile = IO::combine(outPath, outName);
                        outFileTmp = outFile;
                        outFileTmp.append(".tmp");

                        if (fs.open(outFileTmp, "wb")) {
                            fs.write(HEADER, 1, 4, false);
                            fs.writeValue(Projections::PROJ_GEN_VERSION);
                            time = std::chrono::high_resolution_clock::now();
                            if (proj->write(fs, panel->getBuffers())) {
                                Utils::formatDataSize(tmpSize, fs.size());
                                fs.close();

                                IO::moveFile(outFileTmp, outFile, true);
                                int32_t pCount = panel->getBuffers().palette.count;
                                if (pCount > 256) {
                                    sprintf_s(tmpInfo, "16-Bit Indexed [%d]", pCount);
                                }
                                else if (pCount > 1) {
                                    sprintf_s(tmpInfo, "8-Bit Indexed [%d]", pCount);
                                }
                                else {
                                    sprintf_s(tmpInfo, "RGBA32");
                                }

                                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - time).count();
                                JCORE_INFO("Exported Projection '{}'! ({} - {} | Elapsed: {} sec, {} ms)", proj->material.nameID, tmpInfo, tmpSize, (elapsed / 1000.0), elapsed);
                            }
                            else {
                                fs.close();
                                fs::remove(outFileTmp);
                                if (TaskManager::performSkip()) {
                                    JCORE_WARN("Skipped Exporting '{}'!", proj->material.nameID);
                                }
                                else if (TaskManager::isCanceling()) {
                                    JCORE_WARN("Cancelled Exporting!");
                                    TaskManager::unregLevel(2);
                                    goto endTask;
                                }
                                else {
                                    JCORE_ERROR("Failed to export Projection '{}'", proj->material.nameID);
                                }
                            }
                        }
                        else {
                            JCORE_ERROR("Failed to export Projection '{}', could not open file '{}' for writing!", proj->material.nameID, outFileTmp);
                        }

                        REPORT_PROGRESS(
                            TaskManager::reportProgress(2, 0);
                            TaskManager::reportIncrement(1);
                        );
                    }
                    JCORE_TRACE("Exported {} Projections...", projections.size());
                    REPORT_PROGRESS(
                        TaskManager::reportProgress(1, 0.0, 0, materials.size());
                        TaskManager::unregLevel(2);
                        TaskManager::reportIncrement(0);
                        TaskManager::report(0, "Exporting P-Materials...");
                    );
                    if (TaskManager::isCanceling()) {
                        JCORE_WARN("Cancelled Exporting!");
                        goto endTask;
                    }

                    for (auto& mat : materials) {
                        REPORT_PROGRESS(
                            TaskManager::report(1, "Writing... %s", mat->nameID.c_str());
                        );
                        outName = mat->nameID;
                        sanitizeFileName(outName);
                        outName.append(".pmat");

                        outFile = IO::combine(outPath, outName);
                        outFileTmp = outFile;
                        outFileTmp.append(".tmp");

                        if (fs.open(outFileTmp, "wb")) {
                            fs.write(HEADER, 1, 4, false);
                            fs.writeValue(Projections::PROJ_GEN_VERSION);
                            mat->write(fs);

                            Utils::formatDataSize(tmpSize, fs.size());
                            fs.close();
                            IO::moveFile(outFileTmp, outFile, true);
                            JCORE_INFO("Exported P-Material '{}'! ({})", mat->nameID, tmpSize);
                        }
                        else {
                            JCORE_ERROR("Failed to export P-Material '{}', could not open file {} for writing!", mat->nameID, outFileTmp);
                        }

                        REPORT_PROGRESS(
                            TaskManager::reportProgress(2, 0);
                            TaskManager::reportIncrement(1);
                        );
                    }
                    JCORE_TRACE("Exported {} P-Materials...", materials.size());
                    REPORT_PROGRESS(
                        TaskManager::reportProgress(1, 0.0, 0, bundles.size());
                        TaskManager::reportIncrement(0);
                        TaskManager::report(0, "Exporting P-Bundles...");
                    );

                    if (TaskManager::isCanceling()) {
                        JCORE_WARN("Cancelled Exporting!");
                        goto endTask;
                    }

                    for (auto& bun : bundles) {
                        REPORT_PROGRESS(
                            TaskManager::report(1, "Writing... %s", bun->material.nameID.c_str());
                        );

                        outName = bun->material.nameID;
                        sanitizeFileName(outName);
                        outName.append(".pbun");

                        outFile = IO::combine(outPath, outName);
                        outFileTmp = outFile;
                        outFileTmp.append(".tmp");

                        if (fs.open(outFileTmp, "wb")) {
                            fs.write(HEADER, 1, 4, false);
                            fs.writeValue(Projections::PROJ_GEN_VERSION);
                            bun->write(fs);
                            Utils::formatDataSize(tmpSize, fs.size());
                            fs.close();
                            IO::moveFile(outFileTmp, outFile, true);
                            JCORE_INFO("Exported P-Bundle {}! ({})", bun->material.nameID, tmpSize);
                        }
                        else {
                            JCORE_ERROR("Failed to export P-Bundle {}, could not open file {} for writing!", bun->material.nameID, outFileTmp);
                        }

                        REPORT_PROGRESS(
                            TaskManager::reportProgress(2, 0);
                            TaskManager::reportIncrement(1);
                        );
                    }
                    JCORE_TRACE("Exported {} P-Bundles...", bundles.size());
                    REPORT_PROGRESS(
                        TaskManager::reportProgress(1, double(bundles.size()));
                        TaskManager::reportIncrement(0);
                    );

                endTask:
                    REPORT_PROGRESS(
                        TaskManager::unregLevel(0);
                        TaskManager::unregLevel(1);
                    );
                }, NO_TASK, NO_TASK, Task::F_HAS_CANCEL | Task::F_HAS_SKIP | Task::F_HAS_PREVIEW);
        }
    }

    void ProjectionGenPanel::draw() {
        IGuiPanel::draw();

        char temp[64]{};
        bool shouldLoad = false;

        float fullH = ImGui::GetContentRegionAvail().y - 5;
        float fullW = ImGui::GetContentRegionAvail().x;

        static float halfW = 0.5f;
        static float editW = 0.5f;

        static float halfH = 0.333f;
        static float editH = 0.666f;

        float hW = halfW * fullW;
        float eW = editW * fullW;

        float hH = halfH * fullH;
        float eH = editH * fullH;

        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.5f);
        ImGui::SetNextWindowSize({ hW - 5, hH - 5 }, ImGuiCond_Always);
        if (ImGui::BeginChild("##Sources", { hW - 5, hH - 5 }, true)) {
            {
                if (ImGui::Button("Add Source (Folder)")) {
                    std::string path = IO::openFolder("");
                    shouldLoad |= addSource(path) != nullptr;
                }

                ImGui::SameLine();
                if (ImGui::Button("Add Source (Text File)")) {
                    std::string path = IO::openFile("Text File (*.txt)\0*.txt\0\0");
                    shouldLoad |= addSource(path) != nullptr;
                }
            }

            ImGui::TextDisabled("Sources: [%zi]", _sources.size());
            ImGui::Indent();
            size_t toRemove = SIZE_MAX;
            for (size_t i = 0; i < _sources.size(); i++) {
                auto& src = _sources[i];
                ImGui::PushID(int32_t(i));
                if (ImGui::Button("Reload")) {
                    shouldLoad |= true;
                    src.shouldLoad = true;
                }
                ImGui::SameLine();

                if (ImGui::Button("Remove") && toRemove == SIZE_MAX) {
                    toRemove = i;
                }
                ImGui::SameLine();

                std::string_view pName = IO::getName(src.path);

                ImGui::TextDisabled("%.*s [%zi projections, %zi materials, %zi bundles]", int32_t(pName.length()), pName.data(), src.projections.size(), src.materials.size(), src.bundles.size());
                ImGui::PopID();
            }

            if (toRemove != SIZE_MAX) {
                eraseSource(toRemove);
            }
        }
        ImGui::EndChild();

        ImRect rectMid(0.0f, hH - 1.5f, hW, hH + 1.5f);
        ImGui::SplitterBehavior(rectMid, ImGui::GetID("##SplitterMid"), ImGuiAxis_Y, &hH, &eH, 25.0f, 25.0f);

        ImGui::SetNextWindowSize({ hW - 5,  (fullH - (hH + 5)) }, ImGuiCond_Always);
        ImGui::SetNextWindowPos({ 0 , hH + 5 }, ImGuiCond_Always);
        if (ImGui::BeginChild("ProjectionSimulator##Sim", { hW - 5, (fullH - (hH + 5)) }, true)) {

        }
        ImGui::EndChild();

        ImRect rect(hW - 1.5f, 0.0f, hW + 1.5f, fullH);
        ImGui::SplitterBehavior(rect, ImGui::GetID("##Splitter"), ImGuiAxis_X, &hW, &eW, 125.0f, 125.0f);

        ImGui::SetNextWindowPos({ hW + 5 , 0 }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ (fullW - (hW + 5)) , fullH }, ImGuiCond_Always);
        if (ImGui::BeginChild("P-Items##SRC", { (fullW - (hW + 5)) , fullH }, true)) {
            ImGui::TextDisabled("P-Items: [%zi projections, %zi materials, %zi bundles]", _loadedProjections, _loadedMaterials, _loadedBundles);

            static bool exportB[3]{
                true, true, true
            };

            bool isValid = _isLoaded && ((_loadedBundles > 0 && exportB[2]) || (_loadedMaterials > 0 && exportB[1]) || (_loadedProjections > 0 && exportB[0]));

            ImGui::BeginDisabled(!isValid);
            if (ImGui::Button("Export")) {
                uint8_t src = 0;
                if (exportB[0]) {
                    src |= ProjectionSource::VALID_PROJECTIONS;
                }

                if (exportB[1]) {
                    src |= ProjectionSource::VALID_MATERIALS;
                }

                if (exportB[2]) {
                    src |= ProjectionSource::VALID_BUNDLES;
                }

                exportData(src, this);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();

            ImGui::Checkbox("Projections##EXPORT", exportB);
            ImGui::SameLine();
            ImGui::Checkbox("P-Materials##EXPORT", exportB + 1);
            ImGui::SameLine();
            ImGui::Checkbox("P-Bundles##EXPORT", exportB + 2);

            {
                if (_loadedProjections > 0) {
                    sprintf_s(temp, "Projections [%zi]", _loadedProjections);
                    if (ImGui::CollapsingHeaderNoId(temp, "Projections")) {
                        ImGui::Indent();
                        for (size_t i = 0, k = 0; i < _sources.size(); i++)
                        {
                            auto& src = _sources[i];
                            if (src.isValid & ProjectionSource::VALID_PROJECTIONS) {
                                for (size_t j = 0; j < src.projections.size(); j++, k++) {
                                    bool changed = false;
                                    ImGui::PushID(int32_t(k));
                                    auto& proj = src.projections[j];
                                    std::string_view name = IO::getName(proj.path);

                                    ImGui::BeginDisabled(!proj.isValid);
                                    sprintf_s(temp, "%.*s [%zi]", int32_t(name.length()), name.data(), proj.projections.size());

                                    bool exp = !proj.noExport;
                                    if (ImGui::Checkbox("##No-Export", &exp)) {
                                        proj.noExport = !exp;
                                        changed |= true;
                                    }
                                    ImGui::SameLine();
                                    if (ImGui::CollapsingHeaderNoId(temp, "P-Group") && proj.isValid) {
                                        ImGui::Indent();
                                        ImGui::TextDisabled("Path: %s", proj.path.c_str());

                                        for (size_t j = 0; j < proj.projections.size(); j++) {
                                            ImGui::PushID(int32_t(j));
                                            auto& pRef = proj.projections[j];

                                            exp = !pRef.material.noExport;
                                            if (ImGui::Checkbox("##No-Export", &exp)) {
                                                pRef.material.noExport = !exp;
                                                changed = true;
                                            }

                                            ImGui::SameLine();
                                            changed |= drawProjection("Projection", pRef);
                                            ImGui::PopID();
                                        }

                                        ImGui::Unindent();
                                    }
                                    ImGui::EndDisabled();
                                    if (changed) {
                                        proj.write();
                                    }
                                    ImGui::PopID();
                                }
                            }
                        }
                        ImGui::Unindent();
                    }
                }
                else {
                    ImGui::TextDisabled("No Projections...");
                }

                if (_loadedMaterials > 0) {
                    sprintf_s(temp, "P-Materials [%zi]", _loadedMaterials);
                    if (_loadedMaterials > 0 && ImGui::CollapsingHeaderNoId(temp, "P-Materials")) {
                        ImGui::Indent();
                        for (size_t i = 0, k = 0; i < _sources.size(); i++)
                        {
                            auto& src = _sources[i];
                            if (src.isValid & ProjectionSource::VALID_MATERIALS) {
                                for (size_t j = 0; j < src.materials.size(); j++, k++) {
                                    bool changed = false;
                                    ImGui::PushID(int32_t(k));
                                    auto& mat = src.materials[j];
                                    std::string_view name = IO::getName(mat.path);

                                    ImGui::BeginDisabled(!mat.isValid);
                                    sprintf_s(temp, "%.*s [%zi]", int32_t(name.length()), name.data(), mat.materials.size());

                                    bool exp = !mat.noExport;
                                    if (ImGui::Checkbox("##No-Export", &exp)) {
                                        mat.noExport = !exp;
                                        changed |= true;
                                    }
                                    ImGui::SameLine();
                                    if (ImGui::CollapsingHeaderNoId(temp, "P-Group") && mat.isValid) {
                                        ImGui::Indent();
                                        ImGui::TextDisabled("Path: %s", mat.path.c_str());

                                        for (size_t j = 0; j < mat.materials.size(); j++) {
                                            ImGui::PushID(int32_t(j));
                                            auto& mRef = mat.materials[j];

                                            exp = !mRef.noExport;
                                            if (ImGui::Checkbox("##No-Export", &exp)) {
                                                mRef.noExport = !exp;
                                                changed = true;
                                            }

                                            ImGui::SameLine();
                                            changed |= Gui::drawGui(nullptr, mRef);
                                            ImGui::PopID();
                                        }

                                        ImGui::Unindent();
                                    }
                                    ImGui::EndDisabled();
                                    if (changed) {
                                        mat.write();
                                    }
                                    ImGui::PopID();
                                }
                            }
                        }
                        ImGui::Unindent();
                    }
                }
                else {
                    ImGui::TextDisabled("No P-Materials...");
                }

                if (_loadedBundles > 0) {

                }
                else {
                    ImGui::TextDisabled("No P-Bundles...");
                }
            }
        }
        ImGui::EndChild();

        ImGui::PopStyleVar();

        if (fullW > 0) {
            halfW = hW / fullW;
            editW = eW / fullW;
        }

        if (fullH > 0) {
            halfH = hH / fullH;
            editH = eH / fullH;
        }

        if (shouldLoad) {
            load();
        }
    }

    void ProjectionGenPanel::loadSettings() {
        //FileStream fs{};
        //if (fs.open("Projections-Settings.json", "rb")) {
        //    char* temp = reinterpret_cast<char*>(_malloca(fs.size() + 1));
        //    if (temp) {
        //        _settings.reset();
        //
        //        temp[fs.size()] = 0;
        //        fs.read(temp, fs.size(), 1);
        //
        //        json jsonF = json::parse(temp, temp + fs.size(), nullptr, false, true);
        //        _freea(temp);
        //        _settings.read(jsonF);
        //
        //        jsonF.clear();
        //    }
        //    fs.close();
        //}
    }

    void ProjectionGenPanel::saveSettings() {
        //json jsonF = json::object_t();
        //_settings.write(jsonF);
        //
        //FileStream fs{};
        //if (fs.open("Projections-Settings.json", "wb")) {
        //    auto dump = jsonF.dump(4);
        //    fs.write(dump.c_str(), dump.length(), 1, false);
        //    fs.close();
        //}
    }

    void ProjectionGenPanel::load() {
        _isLoaded = false;
        bool* loaded = &_isLoaded;

        _loadedProjections = 0;
        _loadedMaterials = 0;
        _loadedBundles = 0;

        size_t* loadPr = &_loadedProjections;
        size_t* loadMt = &_loadedMaterials;
        size_t* loadBu = &_loadedBundles;

        TaskManager::beginTask([this, loaded, loadPr, loadMt, loadBu]()
            {
                std::vector<fs::path> pathsProj{};
                std::vector<fs::path> pathsPMat{};
                std::vector<fs::path> pathsPBun{};

                static constexpr float PERCENT = 1.0f / 3.0f;
                REPORT_PROGRESS(
                    TaskManager::regLevel(0);
                TaskManager::regLevel(1);
                TaskManager::regLevel(2);

                TaskManager::setTaskTitle("Reading Configs...");
                TaskManager::report(0, "Loading Sources...");
                TaskManager::report(1, "Loading...");

                TaskManager::reportProgress(0, 0.0, 0.0, _sources.size());
                TaskManager::reportProgress(1, 0.0, 0.0, 3.0);
                TaskManager::reportProgress(2, 0.0, 0.0, 0.0);
                );

                if (TaskManager::isCanceling()) {
                    goto taskEnd;
                }

                char temp[513]{};
                for (size_t i = 0; i < _sources.size(); i++) {
                    auto& src = _sources[i];
                    if (src.shouldLoad) {
                        src.reset();

                        pathsProj.clear();
                        pathsPMat.clear();
                        pathsPBun.clear();

                        auto fName = IO::getName(src.path);
                        REPORT_PROGRESS(
                            TaskManager::report(1, "Loading Projections... %.*s", fName.length(), fName.data());
                        TaskManager::reportStep(0, 0.0);
                        TaskManager::reportProgress(1, 0.0, 0.0, 3.0);
                        TaskManager::reportProgress(2, 0.0, 0.0, 0.0);
                        );

                        std::string_view rootPath = src.path;
                        if (Utils::endsWith(src.path, ".txt", false)) {
                            rootPath = IO::getDirectory(src.path);
                        }
                        rootPath = Utils::trimEnd(rootPath, "/\\");

                        sprintf_s(temp, "%.*s", int32_t(rootPath.length()), rootPath.data());

                        size_t left = sizeof(temp) - rootPath.length();
                        char* namePart = temp + rootPath.length();
                        for (auto& sDir : src.dirs) {
                            if (TaskManager::isCanceling()) {
                                goto taskEnd;
                            }

                            if (sDir.length() > 1) {
                                sprintf_s(namePart, left, "%.*s", int32_t(sDir.length()), sDir.data());
                            }

                            if (!IO::getAll(temp, IO::F_TYPE_FILE,
                                [this, i, &pathsProj, &pathsPMat, &pathsPBun](const fs::path& path)
                                {
                                    std::wstring_view pView = path.c_str();
                                    if (Utils::endsWith(pView, L"P-Data.json") && !this->isAlreadyLoaded(pView, i, ProjectionGenPanel::MODE_PROJECTION)) {
                                        pathsProj.emplace_back(path);
                                    }
                                    else if (Utils::endsWith(pView, L"P-Material.json") && !this->isAlreadyLoaded(pView, i, ProjectionGenPanel::MODE_PMATERIAL)) {
                                        pathsPMat.emplace_back(path);
                                    }
                                    else if (Utils::endsWith(pView, L"P-Bundle.json") && !this->isAlreadyLoaded(pView, i, ProjectionGenPanel::MODE_PBUNDLE)) {
                                        pathsPBun.emplace_back(path);
                                    }
                                }, true)) {
                                JCORE_WARN("Couldn't find any data json files in '{}'", temp);
                                continue;
                            }
                            if (sDir.length() < 2) { break; }
                        }

                        // Projections
                        if (pathsProj.size() > 0) {
                            auto& proj = src.projections;
                            REPORT_PROGRESS(
                                TaskManager::reportStep(1, 0.0);
                                TaskManager::reportTarget(2, 0.0, pathsProj.size());
                            );

                            proj.reserve(pathsProj.size());
                            std::string str{};
                            for (auto& pth : pathsProj) {
                                if (TaskManager::isCanceling()) {
                                    goto taskEnd;
                                }

                                str = pth.string();
                                if (!proj.emplace_back(str).read()) {
                                    JCORE_WARN("Failed to load '{}'", str);
                                }
                                else { 
                                    (*loadPr)++; 
                                }
                                src.totalProjections++;
                                REPORT_PROGRESS(
                                    TaskManager::reportIncrement(2);
                                    TaskManager::reportStepFrom(1, 2);
                                );
                            }
                            src.isValid |= ProjectionSource::VALID_PROJECTIONS;
                        }

                        // P-Materials
                        REPORT_PROGRESS(
                            TaskManager::report(1, "Loading P-Materials... %.*s", fName.length(), fName.data());
                        );

                        if (pathsPMat.size() > 0) {
                            auto& mats = src.materials;
                            REPORT_PROGRESS(
                                TaskManager::reportStep(1, 0.0);
                            TaskManager::reportTarget(2, 0.0, pathsPMat.size());
                            );

                            mats.reserve(pathsPMat.size());
                            std::string str{};
                            for (auto& pth : pathsPMat) {

                                if (TaskManager::isCanceling()) {
                                    goto taskEnd;
                                }

                                str = pth.string();
                                if (!mats.emplace_back(str).read()) {
                                    JCORE_WARN("Failed to load '{}'", str);
                                }
                                else { (*loadMt)++; }
                                src.totalMaterials++;
                                REPORT_PROGRESS(
                                    TaskManager::reportIncrement(2);
                                TaskManager::reportStepFrom(1, 2);
                                );
                            }
                            src.isValid |= ProjectionSource::VALID_MATERIALS;
                        }

                        // P-Bundles
                        REPORT_PROGRESS(
                            TaskManager::report(1, "Loading P-Bundles... %.*s", fName.length(), fName.data());
                        );

                        if (pathsPBun.size() > 0) {
                            auto& buns = src.bundles;
                            REPORT_PROGRESS(
                                TaskManager::reportStep(1, 0.0);
                            TaskManager::reportTarget(2, 0.0, pathsPBun.size());
                            );

                            buns.reserve(pathsPBun.size());
                            std::string str{};
                            for (auto& pth : pathsPBun) {

                                if (TaskManager::isCanceling()) {
                                    goto taskEnd;
                                }

                                str = pth.string();
                                if (!buns.emplace_back(str).read()) {
                                    JCORE_WARN("Failed to load '{}'", str);
                                }
                                else { (*loadBu)++; }
                                src.totalBundles++;
                                REPORT_PROGRESS(
                                    TaskManager::reportIncrement(2);
                                TaskManager::reportStepFrom(1, 2);
                                );
                            }
                            src.isValid |= ProjectionSource::VALID_BUNDLES;
                        }
                        src.shouldLoad = false;
                    }
                    else {
                        for (auto& item : src.projections) {
                            if (item.isValid) {
                                (*loadPr)++;
                            }
                        }

                        for (auto& item : src.materials) {
                            if (item.isValid) {
                                (*loadMt)++;
                            }
                        }

                        for (auto& item : src.bundles) {
                            if (item.isValid) {
                                (*loadBu)++;
                            }
                        }
                    }

                    REPORT_PROGRESS(
                        TaskManager::reportIncrement(0);
                    TaskManager::reportProgress(1, 0.0, 0.0, 3.0);
                    TaskManager::reportProgress(2, 0.0, 0.0, 0.0);
                    );
                }
                *loaded = true;

            taskEnd:
                REPORT_PROGRESS(
                    TaskManager::unregLevel(0);
                    TaskManager::unregLevel(1);
                    TaskManager::unregLevel(2);
                );

                if (!*loaded) {
                    JCORE_WARN("Cancelled source loading!");
                }
            }, NO_TASK, NO_TASK, Task::F_HAS_CANCEL);
    }

    bool ProjectionGroup::read() {
        isValid = false;
        FileStream fs{};
        if (fs.open(path, "rb")) {
            char* temp = reinterpret_cast<char*>(_malloca(fs.size() + 1));
            if (temp) {
                reset();

                temp[fs.size()] = 0;
                fs.read(temp, fs.size(), 1);

                json jsonF = json::parse(temp, temp + fs.size(), nullptr, false, true);
                _freea(temp);

                if (jsonF.is_object()) {
                    noExport = jsonF.value("noExport", false);
                    json& pArr = jsonF["projections"];
                    if (pArr.is_array()) {
                        projections.reserve(pArr.size());
                        for (size_t i = 0; i < pArr.size(); i++) {
                            if (pArr[i].is_object()) {
                                auto& proj = projections.emplace_back();
                                if (proj.read(pArr[i], path)) {
                                    if (!proj.prepare()) {
                                        JCORE_WARN("Failed to prepare Projection {}", proj.material.nameID);
                                    }
                                }
                                else {
                                    JCORE_WARN("Failed to read Projection #{} in {}", i, path);
                                }
                            }
                        }
                    }
                    isValid = true;
                }
                jsonF.clear();
            }
            fs.close();
        }
        return isValid;
    }

    void ProjectionGroup::write() {
        if (!isValid) { return; }
        json jsonF = json::object_t();
        jsonF["noExport"] = noExport;

        json::array_t arr{};
        for (size_t i = 0; i < projections.size(); i++) {
            json& val = arr.emplace_back(json{});
            val["noExport"] = projections[i].material.noExport;
            projections[i].write(val);
        }
        jsonF["projections"] = arr;

        FileStream fs{};
        if (fs.open(path, "wb")) {
            auto dump = jsonF.dump(4);
            fs.write(dump.c_str(), dump.length(), 1, false);
            fs.close();
        }
    }

    void ProjectionSource::refreshDirs() {
        dirs.clear();
        if (Utils::endsWith(path, ".txt", false)) {
            FileStream fs(path, "rb");
            if (fs.isOpen()) {
                std::string line{};
                std::string_view rootP = IO::getDirectory(path);
                while (fs.readLine(line)) {
                    std::string_view view = Utils::trim(line);
                    if (Utils::isWhiteSpace(view) || Utils::startsWith(view, "#")) { continue; }

                    view = Utils::trimStart(view, "/\\");;
                    if (fs::path(view).is_absolute()) {
                        view = IO::eraseRoot(view, rootP);
                        if (Utils::isWhiteSpace(view)) { continue; }
                    }

                    auto& str = dirs.emplace_back(view);
                    if (!Utils::startsWith(str, "\\/")) {
                        str.insert(0, 1, '/');
                    }
                    JCORE_TRACE("Added relative dir: '{}'", view);
                }
            }
            fs.close();
        }
        else {
            dirs.push_back("");
        }
    }
}