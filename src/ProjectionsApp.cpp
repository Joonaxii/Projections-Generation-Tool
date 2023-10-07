#include <ProjectionsApp.h>

#include <J-Core/ThreadManager.h>
#include <J-Core/Math/Math.h>
#include <J-Core/Util/StringHelpers.h>

#include <GLFW/glfw3.h>
#include <ImageUtils/ImageUtilsGui.h>
#include <ProjectionsGui.h>

using namespace JCore;
namespace Projections {
    ProjectionsApp::ProjectionsApp(const AppSpecs& specs) : Application(specs) {}
    ProjectionsApp::~ProjectionsApp() {
        for (size_t i = 0; i < _panels.size(); i++) {
            IGuiPanel* panel = _panels[i];
            if (panel) { delete panel; }
        }
        _panels.clear();
    }

    void ProjectionsApp::start() {
        _panels.clear();

        _panels.emplace_back(new ProjectionGenPanel())->init();
        _panels.emplace_back(new ImageUtilPanel())->init();
    }

    void ProjectionsApp::doGui() {
        bool shouldBeDisabled = JCore::TaskManager::getCurrentTask().isRunning();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
        float headerY = ImGui::GetFrameHeight() * 2.2f;

        ImVec2 pos;
        ImVec2 size;

        ImGui::BeginDisabled(shouldBeDisabled);
        //Header
        {
            pos = viewport->Pos;

            size = viewport->Size;
            size.x += 5;
            size.y = headerY;

            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(size);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGui::Begin("##Header", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

            ImGui::SetCursorPos({ 5, ImGui::GetFrameHeight() });
            if (ImGui::BeginChild("##MainView", { 0, ImGui::GetFrameHeight() })) {
                if (ImGui::BeginTabBar("##Main-Tab")) {
                    ImGui::PushID("Main-Tab");
                    for (size_t i = 0; i < _panels.size(); i++) {
                        ImGui::PushID(int32_t(i));
                        if (ImGui::BeginTabItem(_panels[i]->getTitleC_Str())) {
                            _currentPanel = i;
                            ImGui::EndTabItem();
                        }
                        ImGui::PopID();
                    }
                    ImGui::PopID();
                    ImGui::EndTabBar();
                }
            }
            ImGui::EndChild();
            ImGui::End();
        }

        pos = viewport->Pos;
        pos.y += headerY;

        size = viewport->Size;
        size.y -= headerY;

        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##Main", nullptr, window_flags & ~ImGuiWindowFlags_MenuBar);
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(2);

        if (_panels.size()) {
            auto panel = _panels[_currentPanel];
            ImGui::BeginChild(panel->getTitleC_Str());
            panel->draw();
            ImGui::EndChild();
        }
        ImGui::End();
        ImGui::EndDisabled();
        JCore::Application::doGui();
    }
}