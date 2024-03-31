#pragma once
#include <J-Core/Gui/IGuiExtras.h>
#include <ProjectionGen.h>
#include <J-Core/Util/StringUtils.h>

namespace Projections {

    struct ProjectionGroup {
        std::string path{};
        std::vector<Projection> projections{};
        bool isValid{};
        bool noExport{};

        ProjectionGroup(std::string_view path) : path(path) {}

        void reset() {
            isValid = false;
            noExport = false;
            for (auto& item : projections) {
                item.reset();
            }
            projections.clear();
        }

        bool read();
        void write();
    };

    struct PMaterialGroup {
        std::string path{};
        std::vector<PMaterial> materials{};
        bool isValid{};
        bool noExport{};

        PMaterialGroup(std::string_view path) : path(path) {}

        void reset() {
            isValid = false;
            noExport = false;
            for (auto& item : materials) {
                item.reset();
            }
            materials.clear();
        }

        bool read(){return false;}
        void write(){}
    };

    struct PBundleGroup {
        std::string path{};
        std::vector<PBundle> bundles{};
        bool isValid{};
        bool noExport{};

        PBundleGroup(std::string_view path) : path(path) {}

        void reset() {
            isValid = false;
            noExport = false;
            for (auto& item : bundles) {
                item.reset();
            }
            bundles.clear();
        }

        bool read() { return false; }
        void write(){}
    };

    struct ProjectionSource {
        enum : uint8_t {
            VALID_PROJECTIONS = 0x1,
            VALID_MATERIALS = 0x2,
            VALID_BUNDLES = 0x4,
        };

        std::string path{};
        std::vector<ProjectionGroup> projections{};
        std::vector<PMaterialGroup> materials{};
        std::vector<PBundleGroup> bundles{};
        std::vector<std::string> dirs{};

        size_t totalProjections{ 0 };
        size_t totalMaterials{ 0 };
        size_t totalBundles{ 0 };

        bool shouldLoad{};
        uint8_t isValid{};

        ProjectionSource(std::string_view path) : path(path), dirs{} {
            refreshDirs();
        }

        bool isValidDir() const {
            return dirs.size() > 0;
        }

        void reset() {
            totalProjections = 0;
            totalMaterials = 0;
            totalBundles = 0;
            for (auto& item : projections) {
                item.reset();
            }
            projections.clear();

            for (auto& item : materials) {
                item.reset();
            }
            materials.clear();

            for (auto& item : bundles) {
                item.reset();
            }
            bundles.clear();

            isValid = 0x00;
            refreshDirs();
        }

        void refreshDirs();
    };

    class ProjectionGenPanel : public JCore::IGuiPanel {
    public:
        enum {
            MODE_PROJECTION,
            MODE_PMATERIAL,
            MODE_PBUNDLE,
        };


        ProjectionGenPanel() : JCore::IGuiPanel("Projection Generation"), 
            _isLoaded{}, 
            _loadedProjections{0},
            _loadedMaterials{0},
            _loadedBundles{0}
        {}
        ~ProjectionGenPanel();

        void init() override;
        void draw() override;

        PBuffers& getBuffers() { return _buffers; }
        std::vector<ProjectionSource>& getSources() { return _sources; }

        bool isAlreadyLoaded(std::wstring_view view, size_t curI, int32_t mode)  const {
            using namespace JCore;
            for (size_t i = 0; i < curI; i++) {
                auto& source = _sources[i];
                switch (mode) {
                    default:
                        for (auto& p : source.projections) {
                            if (IO::pathsMatch(p.path, view, false)) {
                                return true;
                            }
                        }
                        break;
                case MODE_PMATERIAL:
                    for (auto& p : source.materials) {
                        if (IO::pathsMatch(p.path, view, false)) {
                            return true;
                        }
                    }
                    break;
                case MODE_PBUNDLE:
                    for (auto& p : source.bundles) {
                        if (IO::pathsMatch(p.path, view, false)) {
                            return true;
                        }
                    }
                    break;
                }
            }
            return false;
        }

    private:
        bool _isLoaded;
        std::vector<ProjectionSource> _sources{};

        size_t _loadedProjections;
        size_t _loadedMaterials;
        size_t _loadedBundles;
        PBuffers _buffers;

        void loadSettings();
        void saveSettings();

        void load();

        int32_t alreadyHasSource(std::string_view path, size_t ignore = SIZE_MAX) const {
            for (size_t i = 0; i < _sources.size(); i++) {
                if (ignore == i) { continue; }
                auto& src = _sources[i];
                auto rootA = JCore::IO::eraseRoot(path, src.path, true);

                if (rootA.length() < path.length()) {
                    return -2;
                }

                rootA = JCore::IO::eraseRoot(src.path, path, true);
                if (rootA.length() < src.path.length()) {
                    return int32_t(i);
                }
            }
            return -1;
        }

        ProjectionSource* addSource(std::string_view path) {
            if (JCore::IO::exists(path)) {
                if (fs::is_directory(path) || JCore::Utils::endsWith(path, ".txt")) {
                    size_t ind = _sources.size();
                    int32_t ret = alreadyHasSource(path);

                    if (ret <= -2) {
                        JCORE_WARN("Could not add source '{}' because it already exists in sources!", path);
                        return nullptr;
                    }
                    else if (ret >= 0) {
                        ind = size_t(ret);
                        JCORE_WARN("Replacing '{}' because newly added path '{}' is on a higher level!", _sources[ind].path, path);
                        _sources[ind].path = std::string{ path };
                        _sources[ind].shouldLoad = true;
                    }
                    else {
                        _sources.emplace_back(path).shouldLoad = true;
                    }
                    return &_sources[ind];
                }
            }
            return nullptr;
        }

        void eraseSource(size_t index) {
            if (index >= _sources.size()) {
                return;
            }

            if (_isLoaded) {
                _loadedProjections -= _sources[index].projections.size();
                _loadedMaterials -= _sources[index].materials.size();
                _loadedBundles -= _sources[index].bundles.size();
            }

            _sources[index].reset();
            _sources.erase(_sources.begin() + index);
        }

        void reset() {
            for (auto& src : _sources) {
                src.reset();
            }
            _sources.clear();
        }
    };

    bool doNameIDGui(const char* label, std::string& id);
}

template<>
bool JCore::IGuiDrawable<Projections::Tag>::onGui(const char* label, Projections::Tag& tag, const bool doInline) {
    bool changed = false;
    static std::string temp{};
    ImGui::PushID(label);
    {
        changed |= ImGui::InputText("Tag##Name", &tag.value);
    }
    ImGui::PopID();
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::CoinData>::onGui(const char* label, Projections::CoinData& cData, const bool doInline) {
    bool changed = false;

    int32_t copper{ 0 }, silver{ 0 }, gold{ 0 }, platinum{0};
    cData.extract(copper, silver, gold, platinum);

    ImGui::PushID(label);
    if (doInline) {
        ImGui::Text(label);
        ImGui::SameLine();

        auto avail = (ImGui::GetContentRegionAvail().x - 25) * 0.25f;
        ImGui::SetNextItemWidth(avail);     
        changed |= ImGui::SliderInt("##Copper", &copper, 0, 99, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(avail);
        changed |= ImGui::SliderInt("##Silver", &silver, 0, 99, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(avail);
        changed |= ImGui::SliderInt("##Gold", &gold, 0, 99, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(avail);
        changed |= ImGui::SliderInt("##Platinum", &platinum, 0, 999, "%d", ImGuiSliderFlags_AlwaysClamp);
    }
    else {
        if (ImGui::CollapsingHeader(label)) {
            ImGui::Indent();
            changed |= ImGui::SliderInt("Copper##Coins", &copper, 0, 99, "%d", ImGuiSliderFlags_AlwaysClamp);
            changed |= ImGui::SliderInt("Silver##Coins", &silver, 0, 99, "%d", ImGuiSliderFlags_AlwaysClamp);
            changed |= ImGui::SliderInt("Gold##Coins", &gold, 0, 99, "%d", ImGuiSliderFlags_AlwaysClamp);
            changed |= ImGui::SliderInt("Platinum##Coins", &platinum, 0, 999, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Unindent();
        }
    }

    ImGui::PopID();
    if (changed) {
        cData.merge(copper, silver, gold, platinum);
    }
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::BiomeFlags>::onGui(const char* label, Projections::BiomeFlags& bFlags, const bool doInline) {
    bool changed = false;
    if (doInline) {
        ImGui::PushID(label);
        ImGui::Text(label);
        ImGui::SameLine();

        float avail = (ImGui::GetContentRegionAvail().x - 50) * 0.2f;
        ImGui::SetNextItemWidth(avail);
        if (Gui::drawBitMask<Projections::BiomeFlags, 0>("##Elevation", bFlags)) {
            changed = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(avail);
        if (Gui::drawBitMask<Projections::BiomeFlags, 1>("##Biome", bFlags)) {
            changed = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(avail);
        if (Gui::drawBitMask<Projections::BiomeFlags, 2>("##Effect", bFlags)) {
            changed = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(avail);
        if (Gui::drawBitMask<Projections::BiomeFlags, 3>("##Evil", bFlags)) {
            changed = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(avail);
        if (Gui::drawBitMask<Projections::BiomeFlags, 4>("##Require", bFlags)) {
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    ImGui::PushID(label);
    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();

        if (Gui::drawBitMask<Projections::BiomeFlags, 0>("Elevation##BiomeFlags", bFlags)) {
            changed = true;
        }

        if (Gui::drawBitMask<Projections::BiomeFlags, 1>("Biome##BiomeFlags", bFlags)) {
            changed = true;
        }

        if (Gui::drawBitMask<Projections::BiomeFlags, 2>("Effect##BiomeFlags", bFlags)) {
            changed = true;
        }
        
        if (Gui::drawBitMask<Projections::BiomeFlags, 3>("Purity/Evil##BiomeFlags", bFlags)) {
            changed = true;
        }

        if (Gui::drawBitMask<Projections::BiomeFlags, 4>("Require Mask##BiomeFlags", bFlags)) {
            changed = true;
        }

        ImGui::Unindent();
    }
    ImGui::PopID();
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::WorldConditions>::onGui(const char* label, Projections::WorldConditions& wFlags, const bool doInline) {
    bool changed = false;
    if (doInline) {
        ImGui::PushID(label);
        ImGui::Text(label);
        ImGui::SameLine();

        float avail = (ImGui::GetContentRegionAvail().x - 50) * 0.5f;

        ImGui::SetNextItemWidth(avail);
        if (Gui::drawBitMask<Projections::WorldConditions, 0>("##WFlags", wFlags)) {
            changed = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(avail);
        if (Gui::drawBitMask<Projections::WorldConditions, 1>("##Bosses", wFlags)) {
            changed = true;
        }

        //ImGui::SameLine();
        //if (Gui::drawBitMask<Projections::WorldConditions, 2>("##Effect", wFlags)) {
        //    changed = true;
        //}
        //
        //ImGui::SameLine();
        //if (Gui::drawBitMask<Projections::WorldConditions, 3>("##Evil", wFlags)) {
        //    changed = true;
        //}
        //        
        //ImGui::SameLine();
        //if (Gui::drawBitMask<Projections::WorldConditions, 4>("##Require", wFlags)) {
        //    changed = true;
        //}

        ImGui::PopID();
        return changed;
    }

    ImGui::PushID(label);
    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();

        if (Gui::drawBitMask<Projections::WorldConditions, 0>("Flags##WorldConditions", wFlags)) {
            changed = true;
        }

        if (Gui::drawBitMask<Projections::WorldConditions, 1>("Bosses##WorldConditions", wFlags)) {
            changed = true;
        }

        //if (Gui::drawBitMask<Projections::WorldConditions, 2>("Effect##WorldConditions", wFlags)) {
        //    changed = true;
        //}
        //
        //if (Gui::drawBitMask<Projections::WorldConditions, 3>("Purity/Evil##WorldConditions", wFlags)) {
        //    changed = true;
        //}
        //
        //if (Gui::drawBitMask<Projections::WorldConditions, 4>("Require Mask##WorldConditions", wFlags)) {
        //    changed = true;
        //}

        ImGui::Unindent();
    }
    ImGui::PopID();
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::DropSource>::onGui(const char* label, Projections::DropSource& dInfo, const bool doInline) {
    using namespace Projections;
    bool changed = false;
    ImGui::PushID(label);
    char tempBuf[256]{0};
    bool isModded = dInfo.isModded();

    switch (dInfo.pool)
    {
        default:
            sprintf_s(tempBuf, "%s [No Pool]", label);
            break;
            case PoolType::Pool_NPC: {
                NPCID idd = NPCID(dInfo.id + NPCID::NPC_OFFSET);
                std::string_view nameS = isModded ? dInfo.strId : Enum::nameOf<NPCID, 0>(idd);
                sprintf_s(tempBuf, "%s [Entity: %.*s]", label, int32_t(nameS.length()), nameS.data());
                break;
            }
        case PoolType::Pool_FishingQuest:
            sprintf_s(tempBuf, "%s [Fishing Quest Reward]", label);
            break;
        case PoolType::Pool_Treasure:
            sprintf_s(tempBuf, "%s [Treasure Bag, %d]", label, dInfo.id);
            break;
        case PoolType::Pool_Trader:
            sprintf_s(tempBuf, "%s [Trader Pool]", label);
            break;
    }

    bool expanded = ImGui::CollapsingHeaderNoId(tempBuf, "##DropInfo");
    if (expanded) {
        ImGui::Indent();
        changed |= Gui::drawEnumList("##PoolType", dInfo.pool);

        ImGui::BeginDisabled(dInfo.pool == PoolType::Pool_None);
        switch (dInfo.pool)
        {
            case PoolType::Pool_NPC:
                changed |= ImGui::Checkbox("Modded Entity##DropInfo", &isModded);
                dInfo.setIsModded(isModded);
                if (isModded) {
                    changed |= ImGui::InputText("Source Entity##DropInfoMod", &dInfo.strId);
                }
                else {
                    bool isNetId = dInfo.isNetID();
                    changed |= ImGui::Checkbox("Is NetID ##DropInfo", &isNetId);
                    dInfo.setIsNetID(isNetId);

                    NPCID tempID = NPCID(dInfo.id + NPCID::NPC_OFFSET);
                    if (isNetId) {
                        if (Gui::drawEnumList<NPCID, 0, 0>("Source Entity (NetID)", tempID, true, true)) {
                            dInfo.id = int32_t(tempID) - NPCID::NPC_OFFSET;
                            changed = true;
                        }
                    }
                    else {
                        if (Gui::drawEnumList<NPCID, 1, 0>("Source Entity", tempID, true, true)) {
                            dInfo.id = int32_t(tempID) - NPCID::NPC_OFFSET;
                            changed = true;
                        }
                    }
                }
                break;
            case PoolType::Pool_Treasure:
                changed |= ImGui::InputInt("Treasure Bag ID##DropInfo", &dInfo.id);
                break;
        }

        auto& condition = dInfo.conditions.conditions;

        changed |= ImGui::SliderInt("Stack##DropInfo", &dInfo.stack, 1, 9999);

        changed |= Gui::drawGuiInline("Biome Flags", condition.biomeConditions);
        changed |= Gui::drawGuiInline("World Flags", condition.worldConditions);
        float chance = dInfo.conditions.chance * 100.0f;
        changed |= ImGui::SliderFloat("Chance##TraderInfo", &chance, 0.0f, 100.0f, "%.3f%%", ImGuiSliderFlags_AlwaysClamp);
        dInfo.conditions.chance = chance * 0.01f;
        changed |= ImGui::DragFloat("Weight##TraderInfo", &dInfo.conditions.weight, 0.1f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
   
        ImGui::Unindent();
        ImGui::EndDisabled();
    }
    ImGui::PopID();
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::RecipeIngredient>::onGui(const char* label, Projections::RecipeIngredient& rItem, const bool doInline) {
    using namespace JCore;
    using namespace Projections;
    bool changed = false;
    ImGui::PushID(label);

    float avail = ImGui::GetContentRegionAvail().x;
    {
       {
            switch (rItem.type)
            {
            default:
                ImGui::Text("%s [No Item]", label);
                break;
            case RecipeType::RECIPE_VANILLA:
                ImGui::Text("%s [Vanilla Item: %d]", label, rItem.itemID);
                break;
            case RecipeType::RECIPE_MODDED:
                ImGui::Text("%s [Modded Item: %s]", label, rItem.itemName.c_str());
                break;
            case RecipeType::RECIPE_PROJECTION:
            case RecipeType::RECIPE_PROJECTION_MAT:
            case RecipeType::RECIPE_PROJECTION_BUN:
                ImGui::Text("%s [P-Item: %s]", label, rItem.itemName.c_str());
                break;
            }
        }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(Math::max(ImGui::GetContentRegionAvail().x * 0.25f, 100.0f));
        changed |= Gui::drawEnumList("##RecipeType", rItem.type);
        ImGui::BeginDisabled(rItem.type == RecipeType::RECIPE_NONE);

        switch (rItem.type)
        {
        case RecipeType::RECIPE_VANILLA:
            ImGui::SameLine();
            ImGui::SetNextItemWidth(Math::max(ImGui::GetContentRegionAvail().x * 0.333f, 100.0f));
            changed |= ImGui::InputInt("##Ingredient", &rItem.itemID);
            break;
        case RecipeType::RECIPE_MODDED:
            ImGui::SameLine();
            ImGui::SetNextItemWidth(Math::max(ImGui::GetContentRegionAvail().x * 0.333f, 100.0f));
            changed |= ImGui::InputText("##Ingredient", &rItem.itemName);
            break;
        case RecipeType::RECIPE_PROJECTION:
        case RecipeType::RECIPE_PROJECTION_MAT:
        case RecipeType::RECIPE_PROJECTION_BUN:
            ImGui::SameLine();
            ImGui::SetNextItemWidth(Math::max(ImGui::GetContentRegionAvail().x * 0.333f, 100.0f));
            changed |= ImGui::InputText("##Ingredient", &rItem.itemName);
            break;
        }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(Math::max(ImGui::GetContentRegionAvail().x * 0.333f, 100.0f));
        changed |= ImGui::SliderInt("Stack##Ingredient", &rItem.count, 1, 9999);
        ImGui::EndDisabled();
    }
    ImGui::PopID();
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::Recipe>::onGui(const char* label, Projections::Recipe& recipe, const bool doInline) {
    using namespace Projections;
    bool changed = false;
    ImGui::PushID(label);
    char tempBufA[64]{ 0 };
    char tempBufB[256]{ 0 };

    float indent = ImGui::GetCurrentContext()->Style.IndentSpacing;

    if (recipe.uiFlags & UIFlags::UI_IsElement) {
        ImGui::SetNextItemWidth(30);
        ImGui::BeginDisabled((recipe.uiFlags & UIFlags::UI_DisableDuplicate) != 0);
        if (ImGui::Button("+")) {
            recipe.uiFlags |= UIFlags::UI_Duplicate;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        ImGui::SetNextItemWidth(30);
        ImGui::BeginDisabled((recipe.uiFlags & UIFlags::UI_DisableRemove) != 0);
        if (ImGui::Button("-")) {
            recipe.uiFlags |= UIFlags::UI_Remove;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        indent += 30 + (ImGui::GetCurrentContext()->Style.ItemSpacing.x * 2);
    }

    if (ImGui::CollapsingHeaderNoId(label, "##Recipe")) {
        ImGui::Indent(indent);
        for (int32_t i = 0; i < int32_t(recipe.items.size()); i++) {
            ImGui::PushID(i);
            sprintf_s(tempBufA, "Ingredient #%d", i);
            auto& itm = recipe.items[i];

            float indentB = ImGui::GetCurrentContext()->Style.IndentSpacing;
            {
                ImGui::SetNextItemWidth(30);
                if (ImGui::Button("+")) {
                    recipe.duplicate(i);
                    ImGui::PopID();
                    changed = true;
                    break;
                }
                ImGui::SameLine();

                ImGui::SetNextItemWidth(30);
                ImGui::BeginDisabled(recipe.items.size() < 2);
                if (ImGui::Button("-")) {
                    recipe.removeAt(i);
                    ImGui::EndDisabled();
                    ImGui::PopID();
                    changed = true;
                    break;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                indentB += 30 + (ImGui::GetCurrentContext()->Style.ItemSpacing.x * 2);
            }

            if (ImGui::CollapsingHeader(tempBufA)) {       
                ImGui::Indent(indentB);
                for (int32_t j = 0; j < int32_t(itm.ingredients.size()); j++)
                {
                    ImGui::PushID(j);

                    if (ImGui::Button("+##Ing")) {
                        itm.duplicate(j);
                        changed = true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::SameLine();

                    ImGui::BeginDisabled(itm.ingredients.size() < 2);
                    if (ImGui::Button("-##Ing")) {
                        itm.removeAt(j);
                        changed = true;
                        ImGui::EndDisabled();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();

                    changed |= Gui::drawGui(j == 0 ? "Main" : "Alt.", itm.ingredients[j]);
                    ImGui::PopID();
                }
                ImGui::Unindent(indentB);
            }
            ImGui::PopID();
        }
        ImGui::Unindent(indent);
    }
    ImGui::PopID();
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::PMaterial>::onGui(const char* label, Projections::PMaterial& mat, const bool doInline) {
    using namespace Projections;
    bool changed = false;

    static std::string temp{};
    ImGui::PushID(label);
    if(ImGui::CollapsingHeaderNoId(doInline ? "Material" : mat.nameID.length() < 1 ? "<No ID>" : mat.nameID.c_str(), label)) {
        ImGui::Indent();
        changed |= doNameIDGui("Name ID##PMaterial", mat.nameID);

        changed |= ImGui::InputText("Display Name##PMaterial", &mat.name);
        changed |= ImGui::InputTextMultiline("Description##PMaterial", &mat.description);

        changed |= Gui::drawEnumList("Rarity##PMaterial", mat.rarity);
        changed |= ImGui::DragInt("Priority##PMaterial", &mat.priority);

        bool allowShimmer = (mat.flags & PMaterialFlags::PMat_AllowShimmer) != 0;
        changed |= ImGui::Checkbox("Allow Shimmer##PMaterial", &allowShimmer);
        mat.flags = PMaterialFlags(allowShimmer ? mat.flags | PMaterialFlags::PMat_AllowShimmer : mat.flags & ~PMaterialFlags::PMat_AllowShimmer);

        changed |= Gui::drawGuiInline("Coin Value", mat.coinValue);

        if (ImGui::CollapsingHeader("Pools##PMaterial")) {
            ImGui::Indent();
            char temp[32]{0};
            for (int32_t i = 0; i < int32_t(mat.sources.size()); i++) {
                ImGui::PushID(i);
                sprintf_s(temp, "Pool #%d", i);

                ImGui::SetNextItemWidth(25);
                if (ImGui::Button("+")) {
                    mat.duplicateSource(i);
                    ImGui::PopID();
                    changed = true;
                    break;
                }
                ImGui::SameLine();

                ImGui::SetNextItemWidth(25);
                if (ImGui::Button("-")) {
                    mat.removeSourceAt(i);
                    ImGui::PopID();
                    changed = true;
                    break;
                }
                ImGui::SameLine();

                changed |= Gui::drawGui(temp, mat.sources[i]);
                ImGui::PopID();
            }

            if (mat.sources.size() < 1) {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 15);
                if (ImGui::Button("Add New Pool##PMaterial-Source")) {
                    if (mat.sources.size() < 1) {
                        mat.sources.emplace_back().reset();
                    }
                    else {
                        mat.duplicateSource(mat.sources.size() - 1);
                    }
                    changed = true;
                }
            }
            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Recipes##PMaterial")) {
            ImGui::Indent();
            char temp[32]{ 0 };
            for (int32_t i = 0; i < int32_t(mat.recipes.size()); i++) {
                ImGui::PushID(i);
                sprintf_s(temp, "Recipe #%d", i);

                auto& rec = mat.recipes[i];
                rec.uiFlags = UI_IsElement;
                
                changed |= Gui::drawGui(temp, rec);
                ImGui::PopID();

                if (rec.uiFlags & UI_Duplicate) {
                    mat.duplicateRecipe(i);
                    break;
                }
                else if(rec.uiFlags & UI_Remove){
                    mat.removeRecipeAt(i);
                    break;
                }
            }

            if (mat.recipes.size() < 1) {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 15);
                if (ImGui::Button("Add New Recipe##PMaterial-Recipe")) {
                    if (mat.recipes.size() < 1) {
                        mat.recipes.emplace_back().reset();
                    }
                    else {
                        mat.duplicateRecipe(mat.recipes.size() - 1);
                    }
                    changed = true;
                }
            }
            ImGui::Unindent();
        }

        ImGui::SetNextItemWidth(100);
        changed |= Gui::drawEnumList("Icon Format##PMaterial", mat.iconMode);
        ImGui::BeginDisabled(mat.iconMode == TexMode::TEX_None);
        ImGui::SameLine();
        if (Gui::searchDialogLeft("Icon Path (PNG)", 0x1, mat.icon, "PNG File (*.png)\0*.png\0\0")) {
            IO::eraseRoot(mat.icon, mat.root);
            changed = true;
        }
        ImGui::EndDisabled();

        ImGui::Unindent();
    }
    ImGui::PopID();
    return changed;
}
