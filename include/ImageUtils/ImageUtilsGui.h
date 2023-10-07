#pragma once
#include <J-Core/Gui/IGuiExtras.h>
#include <ImageUtils/ImageUtilsTypes.h>

namespace Projections {

    class ImageUtilPanel : public JCore::IGuiPanel {
    public:
        ImageUtilPanel() : JCore::IGuiPanel("Image Processing"), _settings{}, _inImg{}, _outImg{}, _color{}, _alpha{}{}
        ~ImageUtilPanel();

        void init() override;
        void draw() override;

        ColorReductionSettings& getColorSettings() { return _color; }
        AlphaReductionSettings& getAlphaSettings() { return _alpha; }
        CrossFadeSettings& getCrossFadeSettings() { return _crossFade; }
        FadeSettings& getFadeSettings() { return _fade; }
        AlphaEdgeFadeSettings& getEdgeFadeSettings() { return _edgeFade; }
        TemporalLowpassSettings& getTemporalSettings() { return _temporal; }
        MorphologicalSettings& getMorphSettings() { return _morph; }
        ImageUtilSettings& getSettings() { return _settings; }
        ChromaKeySettings& getChromaKeySettings() { return _chroma; }

        JCore::ImageData& getInput() { return _inImg; }
        JCore::ImageData& getOutput() { return _outImg; }

    private:
        ImageUtilSettings _settings;
        JCore::ImageData _inImg;
        JCore::ImageData _outImg;

        ColorReductionSettings _color;
        AlphaReductionSettings _alpha;
        CrossFadeSettings _crossFade;
        FadeSettings _fade;
        AlphaEdgeFadeSettings _edgeFade;
        TemporalLowpassSettings _temporal;
        MorphologicalSettings _morph;
        ChromaKeySettings _chroma;

        void loadSettings();
        void saveSettings();
    };
}


template<>
bool JCore::IGuiDrawable<Projections::ChromaKeySettings>::onGui(const char* label, Projections::ChromaKeySettings& settings, const bool doInline) {
    using namespace JCore;
    bool changed = false;

    ImGui::PushID(label);
    if (ImGui::Button("Reset##Settings")) {
        settings.reset();
        changed |= true;
    }
    ImGui::SameLine();
    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();
        changed |= ImGui::Checkbox("Premultiply##Settings", &settings.premultiply);

        auto& refColor = settings.refColor;
        ImColor clr(refColor.r, refColor.g, refColor.b);
        if (ImGui::ColorEdit3("Key Color##Settings", &clr.Value.x)) {
            settings.refColor = Color32(
                uint8_t(Math::clamp(clr.Value.x, 0.0f, 1.0f) * 255.0f),
                uint8_t(Math::clamp(clr.Value.y, 0.0f, 1.0f) * 255.0f),
                uint8_t(Math::clamp(clr.Value.z, 0.0f, 1.0f) * 255.0f));
            changed = true;
        }

        changed |= ImGui::SliderFloat("Min Difference##Settings", &settings.minDifference, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::SliderFloat("Max Difference##Settings", &settings.maxDifference, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Unindent();
    }
    ImGui::PopID();
    return changed;
}


template<>
bool JCore::IGuiDrawable<Projections::ImageUtilSettings>::onGui(const char* label, Projections::ImageUtilSettings& settings, const bool doInline) {
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
        ImGui::Unindent();
    }
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::MorphPass>::onGui(const char* label, Projections::MorphPass& pass, const bool doInline) {
    using namespace JCore;
    bool changed = false;
    if (doInline) {
        ImGui::PushID(label);
        ImGui::Text(label);

        ImGui::SameLine();
        changed |= ImGui::Checkbox("##PassDilate", &pass.dilate);
        ImGui::SameLine();
        changed |= Gui::drawEnumList("##PassType", pass.type);
        ImGui::SameLine();
        changed |= ImGui::SliderInt("##PassNumTimes", &pass.numOfTimes, 0, 32, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        changed |= ImGui::Checkbox("##Invert", &pass.invert);

        ImGui::PopID();
        return changed;
    }

    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();
        changed |= ImGui::Checkbox("Dilate##Pass", &pass.dilate);
        changed |= Gui::drawEnumList("Type##Pass", pass.type);
        changed |= ImGui::SliderInt("Number of Times##Pass", &pass.numOfTimes, 0, 32, "%d", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::Checkbox("Invert##Pass", &pass.invert);
        ImGui::Unindent();
    }
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::MorphologicalSettings>::onGui(const char* label, Projections::MorphologicalSettings& settings, const bool doInline) {
    using namespace JCore;
    bool changed = false;
    //if (doInline) {
    //    ImGui::PushID(label);
    //    ImGui::Text(label);
    //    ImGui::SameLine();
    //
    //    if (ImGui::Button("Reset##Settings")) {
    //        settings.reset();
    //        changed |= true;
    //    }
    //    ImGui::SameLine();
    //    changed |= Gui::drawEnumList("##FlagsSettings", settings.flags, true, false);
    //
    //    ImGui::PopID();
    //    return changed;
    //}

    if (ImGui::Button("Reset##Settings")) {
        settings.reset();
        changed |= true;
    }
    ImGui::SameLine();
    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();
        changed |= ImGui::SliderInt("Min Threshold (R)##MorphSettings", &settings.minR, 0, 0x100, "%d", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::SliderInt("Max Threshold (R)##MorphSettings", &settings.maxR, 0, 0x100, "%d", ImGuiSliderFlags_AlwaysClamp);
             
        changed |= ImGui::SliderInt("Min Threshold (G)##MorphSettings", &settings.minG, 0, 0x100, "%d", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::SliderInt("Max Threshold (G)##MorphSettings", &settings.maxG, 0, 0x100, "%d", ImGuiSliderFlags_AlwaysClamp);
               
        changed |= ImGui::SliderInt("Min Threshold (B)##MorphSettings", &settings.minB, 0, 0x100, "%d", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::SliderInt("Max Threshold (B)##MorphSettings", &settings.maxB, 0, 0x100, "%d", ImGuiSliderFlags_AlwaysClamp);

        if (ImGui::CollapsingHeader("Passes##MorphSettings")) {
            ImGui::Indent();
            char buf[32]{ 0 };
            for (size_t i = 0; i < Projections::MorphologicalSettings::MAX_PASSES; i++) {
                ImGui::PushID(int32_t(i));
                auto& pass = settings.passes[i];
                sprintf_s(buf, "Pass #%zi", i);
                changed |= Gui::drawGui(buf, pass);
                ImGui::PopID();
            }
            ImGui::Unindent();
        }
        ImGui::Unindent();
    }
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::CrossFadeSettings>::onGui(const char* label, Projections::CrossFadeSettings& settings, const bool doInline) {
    using namespace JCore;
    bool changed = false;
    if (ImGui::Button("Reset##Settings")) {
        settings.reset();
        changed |= true;
    }
    ImGui::SameLine();
    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();
        changed |= Gui::drawEnumList("Fade Type##Settings", settings.type);
        changed |= ImGui::DragInt("Start Frame##Settings", &settings.startFrame, 1, 0, UINT16_MAX, "%d", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::DragFloat("Frame Rate##Settings", &settings.frameRate, 0.1f, 0.1f, 120, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::DragFloat("Duration (Seconds)##Settings", &settings.crossfadeDuration, 0.05f, 0.1f, 1000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Unindent();
    }
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::FadeSettings>::onGui(const char* label, Projections::FadeSettings& settings, const bool doInline) {
    using namespace JCore;
    bool changed = false;
    if (ImGui::Button("Reset##Settings")) {
        settings.reset();
        changed |= true;
    }
    ImGui::SameLine();
    if (ImGui::CollapsingHeader(label)) {
        ImGui::Indent();
        changed |= Gui::drawEnumList("Fade Type##Settings", settings.type);
        changed |= ImGui::DragInt("Start Frame##Settings", &settings.startFrame, 1, 0, UINT16_MAX, "%d", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::DragInt("Fade Length (Frames)##Settings", &settings.fadeFrames, 1.0f, 1, 128, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Unindent();
    }
    return changed;
}

template<>
bool JCore::IGuiDrawable<Projections::AlphaEdgeFadeSettings>::onGui(const char* label, Projections::AlphaEdgeFadeSettings& settings, const bool doInline) {
    using namespace JCore;
    bool changed = false;
    if (ImGui::Button("Reset##Settings")) {
        settings.reset();
        changed |= true;
    }
    ImGui::SameLine();
    if (ImGui::CollapsingHeader(label)) {
        ImGui::PushID(label);
        ImGui::Indent();
        auto& refColor = settings.refColor;
        ImColor clr(refColor.r, refColor.g, refColor.b);
        if (ImGui::ColorEdit3("Reference Color##Settings", &clr.Value.x)) {
            settings.refColor = Color32(
                uint8_t(Math::clamp(clr.Value.x, 0.0f, 1.0f) * 255.0f),
                uint8_t(Math::clamp(clr.Value.y, 0.0f, 1.0f) * 255.0f),
                uint8_t(Math::clamp(clr.Value.z, 0.0f, 1.0f) * 255.0f));
            changed = true;
        }
        changed |= ImGui::SliderInt("Min Difference##Settings", &settings.minDist, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::SliderInt("Max Difference##Settings", &settings.maxDist, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Unindent();
        ImGui::PopID();
    }
    return changed;
}
