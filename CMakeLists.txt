cmake_minimum_required (VERSION 2.4...3.8)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project ("Projections-GUI")
add_subdirectory("ext/J-Core")
set(PROJECTIONS_SOURCES  )

IF (WIN32)
	add_compile_definitions(WIN32_LEAN_AND_MEAN)
	add_compile_definitions(NOMINMAX)
ENDIF(WIN32)

set(PROJ_SRC
	"include/ProjectionsApp.h"
	"src/ProjectionsApp.cpp"
	
	"include/ProjectionsGui.h"
	"src/ProjectionsGui.cpp"
	
	"include/ProjectionGen.h"
	"src/ProjectionGen.cpp"
	
	"src/main.cpp"
)
source_group("Projections" FILES ${PROJ_SRC})
list(APPEND PROJECTIONS_SOURCES ${PROJ_SRC})	

set(PROJ_SRC
	"include/ImageUtils/ImageUtilsTypes.h"
	"src/ImageUtils/ImageUtilsTypes.cpp"
	
	"include/ImageUtils/ImageUtilsGui.h"
	"src/ImageUtils/ImageUtilsGui.cpp"
)
source_group("Projections/Image Utils" FILES ${PROJ_SRC})
list(APPEND PROJECTIONS_SOURCES ${PROJ_SRC})	

add_executable(Projections-GUI ${PROJECTIONS_SOURCES})
target_link_libraries(Projections-GUI J-Core nlohmann_json)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Projections-GUI)

include_directories("include")
include_directories("ext/J-Core/ext/zlib/")
include_directories("ext/J-Core/ext/imgui")
include_directories("ext/J-Core/ext/json/include")
include_directories("ext/J-Core/ext/glm/glm")
include_directories("ext/J-Core/ext/spdlog/include")
include_directories("ext/J-Core/ext/include")
include_directories("ext/J-Core/include")