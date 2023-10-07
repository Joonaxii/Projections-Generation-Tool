#include <J-Core/Application.h>
#include <vector>
#include <imgui.h>
#include <imgui_internal.h>
#include <J-Core/Gui/IGuiDrawable.h>

namespace Projections {
	class ProjectionsApp : public JCore::Application {
	public:
		ProjectionsApp(const JCore::AppSpecs& specs);
		~ProjectionsApp();

	protected:
		void start() override;
		void doGui() override;

	private:
		ImGuiID _dockspaceID;
		size_t _currentPanel;
		std::vector<JCore::IGuiPanel*> _panels{};
	};
}