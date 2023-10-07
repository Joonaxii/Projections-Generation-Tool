#include <J-Core/EntryPoint.h>
#include <ProjectionsApp.h>

using namespace JCore;

Application* JCore::createApplication(AppArgs args) {
	AppSpecs specs;
	specs.name = "Projection Generator";
	specs.args = args;
	return new Projections::ProjectionsApp(specs);
}
