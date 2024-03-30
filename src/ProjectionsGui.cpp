#include <ProjectionsGui.h>
#include <J-Core/Log.h>
#include <J-Core/ThreadManager.h>
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
                    TaskProgress progress{};
                    progress.setTitle("Exporting...");
                    progress.progress.clear();
                    progress.subProgress.clear();
                    progress.flags |= TaskProgress::HAS_SUB_TASK;
                    progress.setMessage("Exporting Projections...");
                    progress.setSubMessage("Writing...");
                    progress.preview = nullptr;
                    TaskManager::reportProgress(progress);
                    progress.preview = &panel->getBuffers().frameBuffer;

                    auto& sources = panel->getSources();
                    float lenSR = Math::max<float>(float(sources.size()), 1.0f);

                    static constexpr float PERCENT = 1.0f / 3.0f;
                    float startP = 0;
                    size_t ind = 0;

                    FileStream fs{};
                    std::string outFile{};
                    std::string outFileTmp{};
                    std::string outName{};
                    char tmpSize[128]{};
                    char tmpInfo[64]{};

                    std::chrono::steady_clock::time_point time{};
                    for (size_t i = 0; i < sources.size(); i++)
                    {
                        auto& source = sources[i];
   
                        uint8_t toExport = flags & source.isValid;
                        if (source.isValidDir() && toExport != 0) {

                            startP = 0;
                            progress.subProgress.setProgress(startP);

                            TaskManager::clearPreview();
                            if (TaskManager::isCancelling()) {
                                JCORE_WARN("Cancelled Exporting!");
                                return;
                            }
                            if (flags & ProjectionSource::VALID_PROJECTIONS) {
                                auto& projGs = source.projections;
                                for (size_t j = 0; j < projGs.size(); j++) {
                                    auto& projG = projGs[j];
                                    auto sv = IO::getName(projG.path);
                                    if (!projG.noExport) {
                                        float fLen = Math::max<float>(float(projG.projections.size()), 1.0f);
                                        progress.setMessage("Exporting Projections... %.*s", sv.length(), sv.data());
                                        progress.setSubMessage("Writing...");
                                        progress.subProgress.setProgress(startP);
                                        TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS | TASK_COPY_MESSAGE | TASK_COPY_SUBMESSAGE);

                                        for (size_t k = 0; k < projG.projections.size(); k++) {
                                            auto& proj = projG.projections[k];
                                            if (!proj.material.noExport) {
                                                progress.setSubMessage("Writing... %s", proj.material.nameID.c_str());
                                                TaskManager::reportProgress(progress, TASK_COPY_SUBMESSAGE);
                                                outName = proj.material.nameID;
                                                sanitizeFileName(outName);
                                                outName.append(".pdat");

                                                outFile = IO::combine(outPath, outName);
                                                outFileTmp = outFile;
                                                outFileTmp.append(".tmp");

                                                if (fs.open(outFileTmp, "wb")) {
                                                    fs.write(HEADER, 1, 4, false);
                                                    fs.writeValue(Projections::PROJ_GEN_VERSION);
                                                    time = std::chrono::high_resolution_clock::now();
                                                    if (proj.write(fs, panel->getBuffers())) {
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
                                                        JCORE_INFO("Exported Projection '{}'! ({} - {} | Elapsed: {} sec, {} ms)", proj.material.nameID, tmpInfo, tmpSize, (elapsed / 1000.0), elapsed);
                                                    }
                                                    else {
                                                        fs.close();
                                                        fs::remove(outFileTmp);
                                                        if (TaskManager::performSkip()) {
                                                            JCORE_WARN("Skipped Exporting '{}'!", proj.material.nameID);
                                                        }
                                                        else if (TaskManager::isCancelling()) {
                                                            JCORE_WARN("Cancelled Exporting!");
                                                            return;
                                                        }
                                                        else {
                                                            JCORE_ERROR("Failed to export Projection '{}'", proj.material.nameID);
                                                        }
                                                    }
                                                }
                                                else {
                                                    JCORE_ERROR("Failed to export Projection '{}', could not open file '{}' for writing!", proj.material.nameID, outFileTmp);
                                                }
                                            }
                                            progress.subProgress.setProgress(startP + (k + 1) / fLen);
                                            TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
                                        }
                                    }
                                }
                            }
                            else {
                                TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
                            }
                            startP += PERCENT;

                            TaskManager::clearPreview();
                            progress.subProgress.setProgress(startP);

                            if (TaskManager::isCancelling()) {
                                JCORE_WARN("Cancelled Exporting!");
                                return;
                            }
                            if (flags & ProjectionSource::VALID_MATERIALS) {
                                auto& matGs = source.materials;
                                for (size_t j = 0; j < matGs.size(); j++) {
                                    auto& matG = matGs[j];
                                    auto sv = IO::getName(matG.path);
                                    if (!matG.noExport) {
                                        float fLen = Math::max<float>(float(matG.materials.size()), 1.0f);
                                        progress.setMessage("Exporting P-Materials... %.*s", sv.length(), sv.data());
                                        progress.setSubMessage("Writing...");
                                        progress.subProgress.setProgress(startP);
                                        TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS | TASK_COPY_MESSAGE | TASK_COPY_SUBMESSAGE);

                                        for (size_t k = 0; k < matG.materials.size(); k++) {
                                            auto& mat = matG.materials[k];
                                            if (!mat.noExport) {
                                                progress.setSubMessage("Writing... %s", mat.nameID.c_str());
                                                TaskManager::reportProgress(progress, TASK_COPY_SUBMESSAGE);
                                                outName = mat.nameID;
                                                sanitizeFileName(outName);
                                                outName.append(".pmat");

                                                outFile = IO::combine(outPath, outName);
                                                outFileTmp = outFile;
                                                outFileTmp.append(".tmp");

                                                if (fs.open(outFileTmp, "wb")) {
                                                    fs.write(HEADER, 1, 4, false);
                                                    fs.writeValue(Projections::PROJ_GEN_VERSION);
                                                    mat.write(fs);

                                                    Utils::formatDataSize(tmpSize, fs.size());
                                                    fs.close();
                                                    IO::moveFile(outFileTmp, outFile, true);
                                                    JCORE_INFO("Exported P-Material '{}'! ({})", mat.nameID, tmpSize);
                                                }
                                                else {
                                                    JCORE_ERROR("Failed to export P-Material '{}', could not open file {} for writing!", mat.nameID, outFileTmp);
                                                }
                                            }
                                            progress.subProgress.setProgress(startP + (k + 1) / fLen);
                                            TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
                                        }
                                    }
                                }
                            }
                            else {
                                TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
                            }
                            startP += PERCENT;

                            TaskManager::clearPreview();
                            progress.subProgress.setProgress(startP);

                            if (TaskManager::isCancelling()) {
                                JCORE_WARN("Cancelled Exporting!");
                                return;
                            }
                            if (flags & ProjectionSource::VALID_BUNDLES) {
                                auto& bunGs = source.bundles;
                                for (size_t j = 0; j < bunGs.size(); j++) {
                                    auto& bunG = bunGs[j];
                                    auto sv = IO::getName(bunG.path);
                                    if (!bunG.noExport) {
                                        float fLen = Math::max<float>(float(bunG.bundles.size()), 1.0f);
                                        progress.setMessage("Exporting P-Bundles... %.*s", sv.length(), sv.data());
                                        progress.setSubMessage("Writing...");
                                        progress.subProgress.setProgress(startP);
                                        TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS | TASK_COPY_MESSAGE | TASK_COPY_SUBMESSAGE);

                                        for (size_t k = 0; k < bunG.bundles.size(); k++) {
                                            auto& bund = bunG.bundles[k];
                                            if (!bund.material.noExport) {
                                                progress.setSubMessage("Writing... %s", bund.material.nameID.c_str());
                                                TaskManager::reportProgress(progress, TASK_COPY_SUBMESSAGE);
                                                outName = bund.material.nameID;
                                                sanitizeFileName(outName);
                                                outName.append(".pbun");

                                                outFile = IO::combine(outPath, outName);
                                                outFileTmp = outFile;
                                                outFileTmp.append(".tmp");

                                                if (fs.open(outFileTmp, "wb")) {
                                                    fs.write(HEADER, 1, 4, false);
                                                    fs.writeValue(Projections::PROJ_GEN_VERSION);
                                                    bund.write(fs);
                                                    Utils::formatDataSize(tmpSize, fs.size());
                                                    fs.close();
                                                    IO::moveFile(outFileTmp, outFile, true);
                                                    JCORE_INFO("Exported P-Bundle {}! ({})", bund.material.nameID, tmpSize);
                                                }
                                                else {
                                                    JCORE_ERROR("Failed to export P-Bundle {}, could not open file {} for writing!", bund.material.nameID, outFileTmp);
                                                }
                                            }
                                            progress.subProgress.setProgress(startP + (k + 1) / fLen);
                                            TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
                                        }
                                    }
                                }
                            }
                            else {
                                TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
                            }
                            startP += PERCENT;

                            progress.subProgress.setProgress(startP);
                            TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
                        }

                        progress.progress.setProgress((i + 1) / lenSR);
                        TaskManager::reportProgress(progress, TASK_COPY_PROGRESS);
                    }


                }, []() {});
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
                TaskProgress progress{};
                progress.setTitle("Reading Configs...");
                progress.progress.clear();
                progress.subProgress.clear();
                progress.flags |= TaskProgress::HAS_SUB_TASK;
                progress.setMessage("Loading Sources...");
                progress.setSubMessage("Loading...");
                TaskManager::reportProgress(progress);
                std::vector<fs::path> pathsProj{};
                std::vector<fs::path> pathsPMat{};
                std::vector<fs::path> pathsPBun{};

                static constexpr float PERCENT = 1.0f / 3.0f;

                float  startP = 0;
                size_t ind = 0;

                float lenFS = Math::max<float>(float(_sources.size()), 1.0f);
                char temp[513]{};
                for (size_t i = 0; i < _sources.size(); i++) {
                    auto& src = _sources[i];
                    startP = 0;

                    if (src.shouldLoad) {
                        src.reset();

                        pathsProj.clear();
                        pathsPMat.clear();
                        pathsPBun.clear();

                        auto fName = IO::getName(src.path);
                        progress.setSubMessage("Loading Projections... %.*s", fName.length(), fName.data());

                        progress.subProgress.setProgress(0.0f);
                        TaskManager::reportProgress(progress, TASK_COPY_SUBMESSAGE | TASK_COPY_SUBPROGRESS);

                        auto& proj = src.projections;
                        // Projections
                   
                        std::string_view rootPath = src.path;
                        if (Utils::endsWith(src.path, ".txt", false)) {
                            rootPath = IO::getDirectory(src.path);
                        }
                        rootPath = Utils::trimEnd(rootPath, "/\\");

                        sprintf_s(temp, "%.*s", int32_t(rootPath.length()), rootPath.data());

                        size_t left = sizeof(temp) - rootPath.length();
                        char* namePart = temp + rootPath.length();
                        for (auto& sDir : src.dirs) {
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

                        if (pathsProj.size() > 0) {
                            float fLen = Math::max(pathsProj.size() - 1.0f, 1.0f);
                            ind = 0;
                            proj.reserve(pathsProj.size());
                            std::string str{};
                            for (auto& pth : pathsProj) {
                                str = pth.string();
                                if (!proj.emplace_back(str).read()) {
                                    JCORE_WARN("Failed to load '{}'", str);
                                }
                                else { (*loadPr)++; }
                                progress.progress.setProgress(startP + (ind / fLen) * PERCENT);
                                TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
                            }
                            src.isValid |= ProjectionSource::VALID_PROJECTIONS;
                        }

                        // P-Materials
                        startP += PERCENT;
                        progress.subProgress.setProgress(startP);
                        progress.setSubMessage("Loading P-Materials... %.*s", fName.length(), fName.data());
                        TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS | TASK_COPY_SUBMESSAGE);

                        auto& mats = src.materials;
                        if (pathsPMat.size() > 0) {
                            float fLen = Math::max(pathsPMat.size() - 1.0f, 1.0f);
                            ind = 0;
                            mats.reserve(pathsPMat.size());
                            std::string str{};
                            for (auto& pth : pathsPMat) {
                                str = pth.string();
                                if (!mats.emplace_back(str).read()) {
                                    JCORE_WARN("Failed to load '{}'", str);
                                }
                                else { (*loadMt)++; }
                                progress.progress.setProgress(startP + (ind / fLen) * PERCENT);
                                TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
                            }
                            src.isValid |= ProjectionSource::VALID_MATERIALS;
                        }

                        // P-Bundles
                        startP += PERCENT;
                        progress.subProgress.setProgress(startP);
                        progress.setSubMessage("Loading P-Bundles... %.*s", fName.length(), fName.data());
                        TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS | TASK_COPY_SUBMESSAGE);

                        auto& buns = src.bundles;
                        if (pathsPBun.size() > 0) {
                            float fLen = Math::max(pathsPBun.size() - 1.0f, 1.0f);
                            ind = 0;
                            buns.reserve(pathsPBun.size());
                            std::string str{};
                            for (auto& pth : pathsPBun) {
                                str = pth.string();
                                if (!buns.emplace_back(str).read()) {
                                    JCORE_WARN("Failed to load '{}'", str);
                                }
                                else { (*loadBu)++; }
                                progress.progress.setProgress(startP + (ind / fLen) * PERCENT);
                                TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
                            }
                            src.isValid |= ProjectionSource::VALID_BUNDLES;
                        }

                        progress.subProgress.setProgress(1.0f);
                        TaskManager::reportProgress(progress, TASK_COPY_SUBPROGRESS);
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
                    progress.progress.setProgress((i + 1.0f) / lenFS);
                    TaskManager::reportProgress(progress, TASK_COPY_PROGRESS);
                }

                progress.progress.setProgress(1.0f);
                progress.subProgress.setProgress(1.0f);
                TaskManager::reportProgress(progress);
                *loaded = true;
            }, []() {});
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