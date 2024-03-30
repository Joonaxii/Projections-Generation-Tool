#pragma once
#include <J-Core/Gui/IGuiExtras.h>
#include <ProjectionGen.h>

namespace Projections {
    class ProjectionSimPanel : public JCore::IGuiPanel {
    public:
        ProjectionSimPanel() : JCore::IGuiPanel("Projection Simulator") {}
        ~ProjectionSimPanel();

        void init() override;
        void draw() override;
    };
}