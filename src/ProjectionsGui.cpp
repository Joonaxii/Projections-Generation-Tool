#include <ProjectionsGui.h>
#include <J-Core/Log.h>
#include <J-Core/ThreadManager.h>
using namespace JCore;

namespace Projections {
    bool drawLayer(ProjectionLayer& layer, PFrameLayer& layerPath) {
        bool changed = false;
        if (ImGui::CollapsingHeaderNoId(layer.name.c_str(), "Layer")) {
            ImGui::Indent();
            changed |= ImGui::InputText("Name##Layer", &layer.name);
            ImGui::PushID(layer.name.c_str());
            ImGui::Text("Variations: %zi", layerPath.variationCount);
            if (ImGui::CollapsingHeader("Variations##Layer")) {
                char temp[64]{ 0 };
                for (int32_t i = 0; i < int32_t(layerPath.variationCount); i++) {
                    ImGui::PushID(i);

                    auto& var = layer.variations[i];
                    sprintf_s(temp, "Variation %i", i);
                    if (ImGui::RadioButton("", i == layer.defaultVariation)) {
                        if (i != layer.defaultVariation) {
                            layer.defaultVariation = uint8_t(i);
                        }
                        else
                        {
                            layer.defaultVariation = 0xFF;
                        }
                        changed |= true;
                    }
                    ImGui::SameLine();

                    if (ImGui::CollapsingHeaderNoId(var.name.c_str(), temp)) {
                        ImGui::Indent(35);
                        ImGui::Text("Frame Count: %zi", layerPath.variations[i].frames);
                        ImGui::Text("Draw Mode: %s", EnumNames<TileDrawMode>::getEnumName(var.drawMode));
                        changed |= ImGui::InputText("Name##Variation", &var.name);
                        changed |= Gui::drawEnumList("Animation Mode##Layer", var.animMode);
                        if (var.animMode == AnimationMode::ANIM_Loop) {
                            changed |= Gui::drawGui("Animation Speed##Layer", var.animSpeed);
                        }

                        if (ImGui::Button("Apply To Others")) {
                            for (int32_t j = 0; j < int32_t(layerPath.variationCount); j++) {
                                if (j != i) {
                                    layer.variations[j].copyFrom(var, VariationApplyMode(VariationApplyMode::VAR_APPLY_AnimMode | VariationApplyMode::VAR_APPLY_AnimSpeed));
                                }
                            }
                            if (layerPath.variationCount > 1) {
                                changed |= true;
                            }
                        }
                        ImGui::Unindent(35);
                    }
                    ImGui::PopID();
                }
            }
            ImGui::PopID();
            ImGui::Unindent();
        }
        return changed;
    }

    bool drawMaterial(const char* label, ProjectionMaterial& material) {
        bool changed = false;
        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            changed |= ImGui::InputText("Name##Material", &material.name);
            changed |= ImGui::InputText("Description##Material", &material.description);
            changed |= Gui::drawEnumList("Rarity##Material", material.rarity);
            changed |= Gui::drawGui("Trader Info##Material", material.traderInfo);

            ImGui::BeginDisabled(material.drops >= ProjectionMaterial::MAX_DROPS);
            if (ImGui::Button("Add")) {
                changed = true;
                material.drops++;
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            bool collapsing = ImGui::CollapsingHeader("Drops##Material");
            if (collapsing) {
                ImGui::Indent();
                char buffer[65]{ 0 };
                for (int32_t i = 0; i < material.drops; i++) {
                    ImGui::PushID(i);

                    if (ImGui::Button("Remove")) {
                        material.removeDrop(i);
                        changed = true;
                        ImGui::PopID();
                        break;
                    }
                    sprintf_s(buffer, "Drop #%i", i);

                    ImGui::SameLine();
                    changed |= Gui::drawGui(buffer, material.dropInfo[i]);
                    ImGui::PopID();
                }
                ImGui::Unindent();
            }
            ImGui::Unindent();
        }
        return changed;
    }

    bool drawProjection(Projection& projection, bool& changedPath) {
        bool changed = false;
        changedPath = false;
        if (ImGui::CollapsingHeaderNoId(projection.name.c_str(), "Projection")) {
            ImGui::Indent();
            if (ImGui::InputText("Folder##Projection", &projection.rootName)) {
                changed |= true;
                changedPath = true;
            }
            changed |= ImGui::InputText("Name##Projection", &projection.name);
            changed |= ImGui::InputText("Description##Projection", &projection.description);

            changed |= ImGui::InputInt2("Look Offset##Projection", projection.lookOffset);
            changed |= ImGui::InputFloat("Look Distance##Projection", &projection.maxLookDistance);
            projection.maxLookDistance = std::max(projection.maxLookDistance, 16.0f);

            changed |= drawMaterial("Material##Projection", projection.material);

            if (ImGui::CollapsingHeader("Layers##Projection")) {
                ImGui::Indent();
                for (int32_t i = 0; i < projection.getLayerCount(); i++) {
                    ImGui::PushID(i);
                    changed |= drawLayer(projection.layers[i], projection.framesPaths[i]);
                    ImGui::PopID();
                }
                ImGui::Unindent();
            }
            ImGui::Unindent();
        }
        return changed;
    }

    bool drawConfig(ProjectionConfig& config, bool& reorder, bool& reload) {
        bool changed = false;
        reload = false;
        changed |= ImGui::InputText("Group##ProjectionCfg", &config.group);
        reorder |= changed;

        char hdrName[96]{ 0 };
        sprintf_s(hdrName, "Projections [%zi]", config.projections.size());

        bool enabled = !config.ignore;
        if (ImGui::Checkbox("##ProjectionCfg", &enabled)) {
            config.ignore = !enabled;
            changed |= true;
        }
        ImGui::SameLine();
        bool changedPath = false;

        if (ImGui::CollapsingHeaderNoId(hdrName, "DrawConfig")) {
            ImGui::Indent();
            ImGui::Indent();
            for (size_t i = 0; i < config.projections.size(); i++) {
                ImGui::PushID(int32_t(i));
                changedPath = false;
                changed |= drawProjection(config.projections[i], changedPath);
                ImGui::PopID();

                if (changedPath) {
                    reload = true;
                    break;
                }
            }

            ImGui::Separator();
            static char nameBuffer[260]{ 0 };
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("##NameAdd", nameBuffer, 260);
            ImGui::BeginDisabled(strlen(nameBuffer) == 0);
            ImGui::SameLine();
            if (ImGui::Button("Add Projection##NewProjection")) {
                config.addProjection(nameBuffer);
                reload = true;
                changed = true;
            }
            ImGui::EndDisabled();

            ImGui::Unindent();
            ImGui::Unindent();
        }
        return changed;
    }

    void processConfigGroups(ConfigGroupMap& groups, std::vector<ProjectionConfig>& configs) {
        std::unordered_map<std::string, bool, std::hash<std::string>> setupLut{};
        for (auto& grp : groups) {
            setupLut.insert(std::make_pair(grp.first, grp.second.isEnabled));
        }
        groups.clear();
        for (auto& cfg : configs) {
            auto find = groups.find(cfg.group);
            if (find == groups.end()) {
                auto fn = setupLut.find(cfg.group);
                bool state = fn != setupLut.end() ? fn->second : true;
                groups[cfg.group] = { state, std::vector<ProjectionConfig*>{ &cfg } };
                continue;
            }
            find->second.configs.push_back(&cfg);
        }
    }

    static bool sortByProjection(const Projection* a, const Projection* b) {
        return b->maxTiles > a->maxTiles;
    }

    ProjectionGenPanel::~ProjectionGenPanel() {
        _tmpBuf.clear(true);
        _imgBuf.clear(true);
    }

    void ProjectionGenPanel::init() {
        IGuiPanel::init();
        loadSettings();

        static constexpr size_t MAX_ATLAS_SIZE = 8192;
        _imgBuf.doAllocate((MAX_ATLAS_SIZE * MAX_ATLAS_SIZE * sizeof(Color32)));
    }

    void ProjectionGenPanel::draw() {
        IGuiPanel::draw();
        if (Gui::drawGui("Settings##Gen", _settings)) {
            saveSettings();
        }

        using namespace JCore;
        static bool validPath{};
        static bool doLoad{};
        static char rootPath[513]{};
        static char outPath[513]{};

        if (Gui::searchDialogLeft("Path to projections", 0x1, rootPath)) {
            JCORE_INFO("Selected Input Path'{0}'", rootPath);
            validPath = IO::exists(rootPath);
        }

        if (Gui::searchDialogLeft("Output path for projections", 0x1, outPath)) {
            JCORE_INFO("Selected Output Path'{0}'", outPath);
        }

        ImGui::BeginDisabled(!validPath);
        if (ImGui::Button("Load Projections##Terraria") || (doLoad && validPath)) {
            _configs.clear();
            std::vector<fs::path> paths{};
            doLoad = false;
            if (IO::getAllFilesByExt(rootPath, paths, "Config.json", true)) {
                _configs.resize(paths.size());
                size_t i = 0;
                for (auto& path : paths) {
                    auto& cfg = _configs[i++];
                    cfg.load(path);
                }

            }
            processConfigGroups(_groups, _configs);
        }
        ImGui::EndDisabled();

        int32_t ind = 0;

        char buff[128]{ 0 };
        sprintf_s(buff, "Groups [%zi]", _groups.size());
        if (ImGui::CollapsingHeaderNoId(buff, "Group")) {

            char temp[96]{ 0 };
            char nameBuf[96]{ 0 };
            ImGui::Indent();
            for (auto& group : _groups) {
                ImGui::PushID(ind++);

                sprintf_s(nameBuf, "%s", group.first.c_str());
                ImGui::Checkbox("", &group.second.isEnabled);
                ImGui::SameLine();
                if (ImGui::CollapsingHeaderNoId(nameBuf, "Configs")) {
                    int32_t indD = 0;
                    ImGui::Indent();
                    ImGui::Indent();
                    bool reOrder = false;
                    bool reload = false;
                    bool changed = false;
                    for (auto& cfg : group.second.configs) {
                        sprintf_s(temp, "Config #%i", indD);
                        ImGui::PushID(indD);
                        reload = false;
                        if (ImGui::CollapsingHeaderNoId(temp, "Config")) {
                            ImGui::Indent();
                            changed |= drawConfig(*cfg, reOrder, reload);
                            ImGui::Unindent();
                        }
                        ImGui::PopID();
                        indD++;

                        if (changed) {
                            cfg->save();
                        }

                        if (reOrder) {
                            processConfigGroups(_groups, _configs);
                            ImGui::Unindent();
                            ImGui::PopID();
                            goto end;
                        }

                        if (reload) {
                            doLoad = true;
                            ImGui::Unindent();
                            ImGui::PopID();
                            goto end;
                        }
                    }
                    ImGui::Unindent();
                    ImGui::Unindent();
                }
                ImGui::PopID();
            }
        end:
            ImGui::Unindent();
        }

        ImGui::BeginDisabled(!validPath || _groups.size() < 1);
        if (ImGui::Button("Generate Projections")) {
            TaskManager::beginTask([this]()
                {
                    using std::chrono::high_resolution_clock;
                    using std::chrono::duration_cast;
                    using std::chrono::duration;
                    using std::chrono::milliseconds;

                    TaskProgress progress{};
                    progress.setTitle("Generating Projections...");
                    progress.progress.clear();
                    progress.subProgress.clear();
                    progress.setMessage(" ");
                    TaskManager::reportProgress(progress);

                    if (size_t pLen = strlen(outPath)) {
                        IO::createDirectory(outPath);
                        char tempPath[512]{};
                        memcpy(tempPath, outPath, pLen);
                        tempPath[pLen] = 0;
                        char* namePtr = tempPath + pLen;
                        if (_imgBuf.data) {
                            uint32_t ii = 0;
                            progress.progress.setProgress(ii, uint32_t(_groups.size()));
                            std::vector<Projection*> projections{};
                            for (auto& group : _groups) {
                                if (group.second.isEnabled) {
                                    progress.setMessage("Loading Configs: '%s'", group.first.c_str());
                                    if (group.second.configs.size() < 1)
                                    {
                                        progress.progress.setProgress(++ii);
                                        TaskManager::reportProgress(progress, TASK_COPY_PROGRESS | TASK_COPY_MESSAGE);
                                        continue;
                                    }
                                    projections.clear();
                                    progress.progress.setProgress(++ii);
                                    TaskManager::reportProgress(progress, TASK_COPY_MESSAGE | TASK_COPY_PROGRESS);

                                    _tempGroup.clear();
                                    _tempGroup.useSlow = !_settings.useSIMD();
                                    size_t reserve = 1;
                                    for (auto& cfg : group.second.configs) {
                                        if (cfg->ignore) {
                                            continue;
                                        }
                                        for (auto& proj : cfg->projections) {
                                            if (!proj.isValid) { continue; }
                                            projections.push_back(&proj);
                                            reserve += proj.maxTiles;
                                        }
                                    }

                                    reserve >>= 2;
                                    reserve = reserve < 8 ? 8 : reserve;

                                    std::sort(projections.begin(), projections.end(), sortByProjection);

                                    _tempGroup.rawTiles.reserve(reserve);
                                    _tempGroup.rawPIXTiles.reserve(reserve);

                                    _tempGroup.pushNullTile();
                                    bool anyValid = projections.size() > 0;

                                    auto t1 = high_resolution_clock::now();
                                    for (auto& proj : projections) {
                                        proj->load(_tempGroup, _settings.doPremultTiles(), uint8_t(_settings.alphaClip), _tmpBuf);
                                    }
                                    auto t2 = high_resolution_clock::now();

                                    auto ms_int = duration_cast<std::chrono::milliseconds>(t2 - t1);
                                    auto sec_int = duration_cast<std::chrono::seconds>(t2 - t1);

                                    JCORE_TRACE("Elapsed Time [{0}, '{1}']: {2} ms | {3} sec", _tempGroup.useSlow ? "MEMCMP" : "SIMD", group.first.c_str(), ms_int.count(), sec_int.count());

                                    progress.setMessage("Saving Group: '%s'", group.first.c_str());
                                    TaskManager::reportProgress(progress, TASK_COPY_PROGRESS | TASK_COPY_MESSAGE | TASK_COPY_FLAGS);

                                    if (anyValid)
                                    {
                                        _tempGroup.doSave(group.first, _settings.atlasFormat, _settings.doPremultIcons(), _settings.isDX9(), tempPath, namePtr, _imgBuf, projections);
                                    }
                                }
                            }
                        }
                    }
                }, []() {});

        }
        ImGui::EndDisabled();
    }

    void ProjectionGenPanel::loadSettings() {
        FileStream fs{};
        if (fs.open("Projections-Settings.json", "rb")) {
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

    void ProjectionGenPanel::saveSettings() {
        json jsonF = json::object_t();
        _settings.write(jsonF);

        FileStream fs{};
        if (fs.open("Projections-Settings.json", "wb")) {
            auto dump = jsonF.dump(4);
            fs.write(dump.c_str(), dump.length(), 1, false);
            fs.close();
        }
    }
}