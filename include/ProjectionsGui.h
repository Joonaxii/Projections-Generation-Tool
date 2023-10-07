#pragma once
#include <J-Core/Gui/IGuiExtras.h>
#include <unordered_map>
#include <ProjectionGen.h>

namespace Projections {

    struct ConfigGroup {
        bool isEnabled{ true };
        std::vector<ProjectionConfig*> configs{};
    };

    using ConfigGroupMap = std::unordered_map<std::string, ConfigGroup, std::hash<std::string>>;

    class ProjectionGenPanel : public JCore::IGuiPanel {
    public:
        ProjectionGenPanel() : JCore::IGuiPanel("Projection Generation") {}
        ~ProjectionGenPanel();

        void init() override;
        void draw() override;
    private:
        GenerationSettings _settings;

        std::vector<ProjectionConfig> _configs;
        ConfigGroupMap _groups;
        ProjectionGroup _tempGroup;
        JCore::ImageData _tmpBuf;
        JCore::ImageData _imgBuf;

        void loadSettings();
        void saveSettings();
    };
}

template<>
bool JCore::IGuiDrawable<Projections::BiomeFlags>::onGui(const char* label, Projections::BiomeFlags& bFlags, const bool doInline) {
    bool changed = false;
    if (doInline) {
        ImGui::PushID(label);
        ImGui::Text(label);

        ImGui::SameLine();
        if (Gui::drawBitMask<Projections::BiomeFlags, 0>("##Elevation", bFlags)) {
            changed = true;
        }

        ImGui::SameLine();
        if (Gui::drawBitMask<Projections::BiomeFlags, 1>("##Biome", bFlags)) {
            changed = true;
        }

        ImGui::SameLine();
        if (Gui::drawBitMask<Projections::BiomeFlags, 2>("##Effect", bFlags)) {
            changed = true;
        }

        ImGui::SameLine();
        if (Gui::drawBitMask<Projections::BiomeFlags, 3>("##Evil", bFlags)) {
            changed = true;
        }
                
        ImGui::SameLine();
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
        if (Gui::drawBitMask<Projections::WorldConditions, 0>("##WFlags", wFlags)) {
            changed = true;
        }

        ImGui::SameLine();
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
bool JCore::IGuiDrawable<Projections::TraderInfo>::onGui(const char* label, Projections::TraderInfo& tInfo, const bool doInline) {
    bool changed = false;
    ImGui::PushID(label);
    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();

        changed |= ImGui::Checkbox("Allow Trading##TraderInfo", &tInfo.canBeTraded);

        ImGui::BeginDisabled(!tInfo.canBeTraded);
        changed |= Gui::drawGui("Biome Flags##TraderInfo", tInfo.biomeFlags);
        changed |= Gui::drawGui("World Conditions##TraderInfo", tInfo.worldConditions);
        changed |= ImGui::DragFloat("Weight##TraderInfo", &tInfo.weight, 0.1f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::EndDisabled();

        ImGui::Unindent();
    }
    ImGui::PopID();
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::DropInfo>::onGui(const char* label, Projections::DropInfo& dInfo, const bool doInline) {
    using namespace Projections;
    bool changed = false;
    ImGui::PushID(label);
    char tempBuf[256]{0};
    bool isModded = dInfo.isModded();
    sprintf_s(tempBuf, "%s [%s]", label, isModded ? dInfo.modSource.c_str() : EnumNames<Projections::NPCID>::getEnumName(dInfo.sourceEntity));

    if (ImGui::CollapsingHeaderNoId(tempBuf, "##DropInfo")) {
        ImGui::Indent();
        ImGui::Indent();

        changed |= ImGui::Checkbox("Modded Entity##DropInfo", &isModded);
        dInfo.setIsModded(isModded);


        if (isModded) {
            changed |= ImGui::InputText("Source Entity##DropInfoMod", &dInfo.modSource);
        }
        else {
            bool isNetId = dInfo.isNetID();
            changed |= ImGui::Checkbox("Is NetID ##DropInfo", &isNetId);
            dInfo.setIsNetID(isNetId);

            if (isNetId) {
                changed |= Gui::drawEnumList<NPCID, 1>("Source Entity (NetID)", dInfo.sourceEntity, true, true);
            }
            else {
                changed |= Gui::drawEnumList<NPCID, 0>("Source Entity", dInfo.sourceEntity, true, true);
            }

        }

        changed |= Gui::drawGui("Biome Flags##DropInfo", dInfo.biomeFlags);
        changed |= Gui::drawGui("World Conditions##DropInfo", dInfo.worldConditions);
        float chance = dInfo.chance * 100.0f;
        changed |= ImGui::SliderFloat("Chance##TraderInfo", &chance, 0.0f, 100.0f, "%.3f%%", ImGuiSliderFlags_AlwaysClamp);
        dInfo.chance = chance * 0.01f;
        changed |= ImGui::DragFloat("Weight##TraderInfo", &dInfo.weight, 0.1f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
   
        ImGui::Unindent();
        ImGui::Unindent();
    }
    ImGui::PopID();
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::AnimationSpeed>::onGui(const char* label, Projections::AnimationSpeed& aSpeed, const bool doInline) {
    using namespace Projections;
    bool changed = false;
    ImGui::PushID(label);
    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();

        changed |= Gui::drawEnumList("Animation Speed##ASpeed", aSpeed.type);
        switch (aSpeed.type)
        {
            case ANIM_S_Ticks: {
                int32_t val = aSpeed.ticks;
                changed |= ImGui::DragInt("Ticks Per Frame##ASpeed", &val, 1, 1, 1000, "%d", ImGuiSliderFlags_AlwaysClamp);
                aSpeed.ticks = uint16_t(val);
                break;
            }
            case ANIM_S_Duration:
                changed |= ImGui::DragFloat("Frame Duration (MS)##ASpeed", &aSpeed.duration, 0.1f, 0.1f, 1000000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                break;
        }
        ImGui::Unindent();
    }
    ImGui::PopID();
    return changed;
}


template<>
bool JCore::IGuiDrawable<Projections::OutputFormat>::onGui(const char* label, Projections::OutputFormat& fmt, const bool doInline) {
    bool changed = false;
    if (doInline) {
        ImGui::PushID(label);
        ImGui::Text(label);

        ImGui::SameLine();
        if (Gui::drawBitMask("##Type", fmt.type)) {
            changed = true;
        }

        ImGui::BeginDisabled(fmt.type == Projections::A_EXP_PNG);
        ImGui::SameLine();
        if (Gui::drawBitMask("##Size", fmt.size)) {
            changed = true;
        }
        ImGui::EndDisabled();

        ImGui::PopID();
        return changed;
    }

    ImGui::PushID(label);
    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();

        if (Gui::drawEnumList("Type##OutFmt", fmt.type)) {
            changed = true;
        }

        ImGui::BeginDisabled(fmt.type == Projections::A_EXP_PNG);
        if (Gui::drawEnumList("Size##OutFmt", fmt.size)) {
            changed = true;
        }
        ImGui::EndDisabled();

        ImGui::Unindent();
    }
    ImGui::PopID();
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::GenerationSettings>::onGui(const char* label, Projections::GenerationSettings& settings, const bool doInline) {
    using namespace JCore;
    bool changed = false;
    if (doInline) {
        ImGui::PushID(label);
        ImGui::Text(label);
        ImGui::SameLine();

        if (ImGui::Button("Reset##Settings")) {
            settings.reset();
            changed |= true;
        }
        ImGui::SameLine();
        changed |= Gui::drawBitMask("##FlagsSettings", settings.flags, true, false);
        ImGui::SameLine();
        changed |= ImGui::SliderInt("##AClipSettings", &settings.alphaClip, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        changed |= Gui::drawGuiInline("##AtlFmt", settings.atlasFormat);

        ImGui::PopID();
        return changed;
    }

    if (ImGui::Button("Reset##Settings")) {
        settings.reset();
        changed |= true;
    }
    ImGui::SameLine();
    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();
        changed |= Gui::drawBitMask("Flags##Settings", settings.flags, true, false);
        changed |= ImGui::SliderInt("Alpha Clip##Settings", &settings.alphaClip, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
        changed |= Gui::drawGui("Atlas Format##Settings", settings.atlasFormat);
        ImGui::Unindent();
    }
    return changed;
}
