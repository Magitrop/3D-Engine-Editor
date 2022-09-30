#include "EngineEditor.h"

#ifdef __gl_h_
#undef __gl_h_
#endif // __gl_h_
#include <glad/glad.h>

#include <windows.h>
#include <fstream>
#include <filesystem>
#include <sstream>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "EngineEditor.h"
#include <Engine.h>
#include <Initializer.h>
#include <EventsController.h>
#include <LightingController.h>
#include <RenderingController.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

EngineEditor::Project::ProjectState EngineEditor::Project::projectState;
std::string EngineEditor::visualStudioVersion = "Visual Studio 17 2022";
EngineEditor::Project* EngineEditor::currentProject = nullptr;
GameObject* EngineEditor::selectedGameObject;

void EngineEditor::DrawMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Projects"))
		{
			if (ImGui::MenuItem("New project"))
			{
				Project::projectState = Project::ProjectState::Create;
			}
			if (ImGui::MenuItem("Open project"))
			{
				Project::projectState = Project::ProjectState::Open;
			}
			ImGui::EndMenu();
		}
		if (currentProject != nullptr)
			if (ImGui::BeginMenu("Build"))
			{
				if (ImGui::MenuItem("Build"))
				{
					BuildProject(false);
				}
				if (ImGui::MenuItem("Build and run"))
				{
					BuildProject(true);
				}
				ImGui::EndMenu();
			}
		if (ImGui::BeginMenu("Environment"))
		{
			if (ImGui::BeginMenu("Set environment"))
			{
				if (ImGui::MenuItem("Visual Studio 17 2022"))
				{
					visualStudioVersion = "Visual Studio 17 2022";
				}
				if (ImGui::MenuItem("Visual Studio 16 2019"))
				{
					visualStudioVersion = "Visual Studio 16 2019";
				}
				if (ImGui::MenuItem("Visual Studio 15 2017"))
				{
					visualStudioVersion = "Visual Studio 15 2017";
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
	}
	ImGui::EndMainMenuBar();
}

void EngineEditor::DrawCreateProjectMenu()
{
	ImGui::SetNextWindowSize(ImVec2(500, 75));
	if (ImGui::Begin("Create new project", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
	{
		static char projectName[100]{};

		ImGuiStyle& style = ImGui::GetStyle();
		float size = ImGui::CalcTextSize("Enter project name:").x + style.FramePadding.x * 2.0f;
		float avail = ImGui::GetContentRegionAvail().x;
		float off = (avail - size) * 0.5f;
		if (off > 0.0f)
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
		ImGui::Text("Enter project name:");

		ImGui::PushItemWidth(375);
		ImGui::InputText("##label", projectName, IM_ARRAYSIZE(projectName)); ImGui::SameLine();
		if (ImGui::Button("Create", ImVec2(100, 20)))
			CreateProject(std::string(projectName));
	}
	ImGui::End();
}

void EngineEditor::DrawOpenProjectMenu()
{
	ImGui::SetNextWindowSize(ImVec2(500, 75));
	if (ImGui::Begin("Open project", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
	{
		static char projectName[100]{};

		ImGuiStyle& style = ImGui::GetStyle();
		float size = ImGui::CalcTextSize("Enter project name:").x + style.FramePadding.x * 2.0f;
		float avail = ImGui::GetContentRegionAvail().x;
		float off = (avail - size) * 0.5f;
		if (off > 0.0f)
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
		ImGui::Text("Enter project name:");

		ImGui::PushItemWidth(375);
		ImGui::InputText("##label", projectName, IM_ARRAYSIZE(projectName)); ImGui::SameLine();
		if (ImGui::Button("Open", ImVec2(100, 20)))
			OpenProject(std::string(projectName));
	}
	ImGui::End();
}

void EngineEditor::DrawOpenedProjectMenu()
{
#pragma region ObjectsHierarchy
	ImGui::SetNextWindowPos(ImVec2(0, 20));
	ImGui::SetNextWindowSize(ImVec2(200,
		ImGui::GetIO().DisplaySize.y
		- 20		// main menu bar
		- 150	// loaded assets menu
		- 1		// loaded assets menu gap
	));
	if (ImGui::Begin("Objects", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse))
	{
		ImGui::PushItemWidth(ImGui::GetWindowSize().x);
		for (auto& o : ObjectsManager::instantiatedObjects)
		{
			if (ImGui::Selectable(o->name.c_str(), false))
			{
				selectedGameObject = o;
			}
		}
	}
	ImGui::End();
#pragma endregion

#pragma region ObjectProperties
	if (selectedGameObject != nullptr)
	{
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 400, 20));
		ImGui::SetNextWindowSizeConstraints(ImVec2(400, 0), ImVec2(400, 
			ImGui::GetIO().DisplaySize.y
			- 20	// main menu bar
			- 150	// loaded assets menu
			- 1		// loaded assets menu gap
		));
		if (ImGui::Begin(selectedGameObject->name.c_str(), nullptr, 
			ImGuiWindowFlags_AlwaysAutoResize | 
			ImGuiWindowFlags_NoResize | 
			ImGuiWindowFlags_NoMove | 
			ImGuiWindowFlags_NoCollapse | 
			ImGuiWindowFlags_HorizontalScrollbar))
		{
			for (auto& c : selectedGameObject->components)
			{
				if (ImGui::CollapsingHeader(c.second->GetComponentName().c_str()))
				{
					Json j;
					c.second->Serialize(j);
					if (j.is_object())
					{
						for (auto& kvp : j.get<Json::object_t>())
						{
							DrawComponentField(kvp);
						}
					}
				}
			}
		}
		ImGui::End();
	}
#pragma endregion

#pragma region CurrentProjectMenu
	ImGui::SetNextWindowSize(ImVec2(500, 75));
	if (ImGui::Begin(("Current project: " + currentProject->projectName).c_str(), nullptr, ImGuiWindowFlags_NoResize))
	{
		static char componentName[100]{};

		ImGuiStyle& style = ImGui::GetStyle();
		float size = ImGui::CalcTextSize("Create new component").x + style.FramePadding.x * 2.0f;
		float avail = ImGui::GetContentRegionAvail().x;
		float off = (avail - size) * 0.5f;
		if (off > 0.0f)
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
		ImGui::Text("Create new component");

		ImGui::PushItemWidth(375);
		ImGui::InputText("##label", componentName, sizeof(componentName)); ImGui::SameLine();
		if (ImGui::Button("Create", ImVec2(100, 20)))
			CreateComponent(componentName);
	}
	ImGui::End();
#pragma endregion
}

void EngineEditor::DrawLoadedAssetsMenu()
{
	ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 150));
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 150));
	if (ImGui::Begin("##label", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
	{
		if (ImGui::ImageButton(NULL, ImVec2(100, 100)))
		{

		}
	}
	ImGui::End();
}

void EngineEditor::DrawEngineMenu()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	DrawMenuBar();
	DrawLoadedAssetsMenu();
	switch (Project::projectState)
	{
	case Project::ProjectState::Create:
		DrawCreateProjectMenu();
		break;
	case Project::ProjectState::Open:
		DrawOpenProjectMenu();
		break;
	case Project::ProjectState::Opened:
		DrawOpenedProjectMenu();
		break;
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void EngineEditor::DrawComponentField(std::pair<const std::string, Json> js)
{
	int a;
	float b;
	bool c;
	char s[50];

	std::ostringstream ss;
	ss << std::left << std::setfill(' ') << std::setw(20) << js.first;

	ImGui::Indent(16.0f);
	switch (js.second.type())
	{
	case Json::value_t::number_integer:
		ImGui::Text(ss.str().substr(0, 20).c_str()); ImGui::SameLine(); ImGui::InputInt("##label", &a);
		break;
	case Json::value_t::number_float:
		ImGui::Text(ss.str().substr(0, 20).c_str()); ImGui::SameLine(); ImGui::InputFloat("##label", &b);
		break;
	case Json::value_t::boolean:
		ImGui::Text(ss.str().substr(0, 20).c_str()); ImGui::SameLine(); ImGui::Checkbox("##label", &c);
		break;
	case Json::value_t::string:
		ImGui::Text(ss.str().substr(0, 20).c_str()); ImGui::SameLine(); ImGui::InputText("##label", s, 50);
		break;
	case Json::value_t::object:
		if (ImGui::CollapsingHeader(js.first.c_str()))
			for (auto& kvp : js.second.get<Json::object_t>())
			{
				DrawComponentField(kvp);
			}
		break;
	}
	ImGui::Unindent(16.0f);
}

void EngineEditor::OpenProject(std::string projectName)
{
	currentProject = new Project();

	currentProject->projectName = projectName;
	currentProject->projectPath = std::filesystem::current_path().string() + "/" + projectName;

	Project::projectState = Project::ProjectState::Opened;
}
void EngineEditor::CreateProject(std::string projectName)
{
	currentProject = new Project();

	currentProject->projectName = projectName;
	currentProject->projectPath = std::filesystem::current_path().string() + "/" + projectName;
	std::filesystem::create_directory(currentProject->projectPath);
	std::filesystem::create_directory(currentProject->projectPath + "/build");

	std::ofstream cmakeFile(currentProject->projectPath + "/CMakeLists.txt");
	cmakeFile << R"(
cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
get_filename_component(PARENT_DIR ../ ABSOLUTE)	

set(PROJECT_NAME newproj)

project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Bullet REQUIRED PATHS "${PARENT_DIR}/libraries/bullet3")
find_package(Imgui REQUIRED PATHS "${PARENT_DIR}/libraries/imgui")

include_directories(${PARENT_DIR}/libraries/engine/debug/include)
link_directories(${PARENT_DIR}/libraries/engine/debug/lib)
find_library(EngineLib engine "${PARENT_DIR}/libraries/engine/debug/lib")

include_directories(${PARENT_DIR}/libraries/controllers/debug/include)
link_directories(${PARENT_DIR}/libraries/controllers/debug/lib)
find_library(ControllersLib controllers "${PARENT_DIR}/libraries/controllers/debug/lib")

include_directories(${PARENT_DIR}/libraries/freetype/include)
find_library(FreetypeLib freetype "${PARENT_DIR}/libraries/freetype/release dll/win64")

include_directories(${PARENT_DIR}/libraries/assimp/include)
find_library(AssimpLib assimp "${PARENT_DIR}/libraries/assimp/lib/Debug")

file(WRITE null.cpp "")
add_executable(
	${PROJECT_NAME}
		null.cpp
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(${PARENT_DIR}/libraries/glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw)
target_link_libraries(${PROJECT_NAME} glfw)

add_subdirectory(${PARENT_DIR}/libraries/glad ${CMAKE_CURRENT_BINARY_DIR}/glad)
target_link_libraries(${PROJECT_NAME} glad)

add_subdirectory(${PARENT_DIR}/libraries/glm ${CMAKE_CURRENT_BINARY_DIR}/glm)
target_link_libraries(${PROJECT_NAME} glm)

target_link_libraries(${PROJECT_NAME} ${FreetypeLib})
target_include_directories(${PROJECT_NAME} PRIVATE "libraries/freetype/include")

target_link_libraries(${PROJECT_NAME} ${AssimpLib})
target_include_directories(${PROJECT_NAME} PRIVATE "libraries/assimp/include")

target_link_libraries(${PROJECT_NAME} ${EngineLib})
target_include_directories(${PROJECT_NAME} PRIVATE "libraries/engine/debug/include")
target_link_libraries(${PROJECT_NAME} ${ControllersLib})
target_include_directories(${PROJECT_NAME} PRIVATE "libraries/controllers/debug/include")

target_compile_definitions(${PROJECT_NAME} PRIVATE ${BULLET_DEFINITIONS})
target_include_directories(${PROJECT_NAME} PRIVATE ${BULLET_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${BULLET_LIBRARIES})

target_compile_definitions(${PROJECT_NAME} PRIVATE ${IMGUI_DEFINITIONS})
target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE "${PARENT_DIR}/libraries/imgui/include/loaders")
target_link_libraries(${PROJECT_NAME} ${IMGUI_LIBRARIES})
set(FILES_TO_ADD
	"${PARENT_DIR}/libraries/imgui/include/loaders/imgui_impl_glfw.h"
	"${PARENT_DIR}/libraries/imgui/include/loaders/imgui_impl_glfw.cpp"
	"${PARENT_DIR}/libraries/imgui/include/loaders/imgui_impl_opengl3.cpp"
	"${PARENT_DIR}/libraries/imgui/include/loaders/imgui_impl_opengl3.h"
	"${PARENT_DIR}/libraries/imgui/include/loaders/imgui_impl_opengl3_loader.h")
target_sources(${PROJECT_NAME} PRIVATE ${FILES_TO_ADD})
)";
	cmakeFile.close();

	system(("cmake -G \"" +
		visualStudioVersion + "\" \"" +
		currentProject->projectPath + "\" -B \"" +
		currentProject->projectPath + "/build\"").c_str());
	Project::projectState = Project::ProjectState::Opened;
}
void EngineEditor::CreateComponent(std::string componentName)
{
	currentProject->customComponents.push_back(componentName);

	std::ofstream componentFile(currentProject->projectPath + "/" + componentName + ".cpp");
	componentFile << R"(
#include <Engine.h>

class )" + componentName + R"( : public Component
{
	COMPONENT()" + componentName + R"(, Component)
public:
	virtual void OnUpdate() override
	{
		
	}
};
)";
	componentFile.close();

	std::string allComponents;
	for (auto it = currentProject->customComponents.begin(); it != currentProject->customComponents.end(); it++)
		allComponents += *it + ".cpp\n";
	std::ofstream cmakeFile(currentProject->projectPath + "/CMakeLists.txt");
	cmakeFile << R"(
cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
get_filename_component(PARENT_DIR ../ ABSOLUTE)	

set(PROJECT_NAME newproj)

project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Bullet REQUIRED PATHS "../libraries/bullet3")
find_package(Imgui REQUIRED PATHS "${PARENT_DIR}/libraries/imgui")

include_directories(${PARENT_DIR}/libraries/engine/debug/include)
link_directories(${PARENT_DIR}/libraries/engine/debug/lib)
find_library(EngineLib engine "${PARENT_DIR}/libraries/engine/debug/lib")

include_directories(${PARENT_DIR}/libraries/controllers/debug/include)
link_directories(${PARENT_DIR}/libraries/controllers/debug/lib)
find_library(ControllersLib controllers "${PARENT_DIR}/libraries/controllers/debug/lib")

include_directories(../libraries/freetype/include)
find_library(FreetypeLib freetype "../libraries/freetype/release dll/win64")

include_directories(../libraries/assimp/include)
find_library(AssimpLib assimp "../libraries/assimp/lib/Debug")

add_executable(
	${PROJECT_NAME}
	)" << allComponents << R"(
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(../libraries/glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw)
target_link_libraries(${PROJECT_NAME} glfw)

add_subdirectory(../libraries/glad ${CMAKE_CURRENT_BINARY_DIR}/glad)
target_link_libraries(${PROJECT_NAME} glad)

add_subdirectory(../libraries/glm ${CMAKE_CURRENT_BINARY_DIR}/glm)
target_link_libraries(${PROJECT_NAME} glm)

target_link_libraries(${PROJECT_NAME} ${FreetypeLib})
target_include_directories(${PROJECT_NAME} PRIVATE "libraries/freetype/include")

target_link_libraries(${PROJECT_NAME} ${AssimpLib})
target_include_directories(${PROJECT_NAME} PRIVATE "libraries/assimp/include")

target_link_libraries(${PROJECT_NAME} ${EngineLib})
target_include_directories(${PROJECT_NAME} PRIVATE "libraries/engine/debug/include")
target_link_libraries(${PROJECT_NAME} ${ControllersLib})
target_include_directories(${PROJECT_NAME} PRIVATE "libraries/controllers/debug/include")

target_compile_definitions(${PROJECT_NAME} PRIVATE ${BULLET_DEFINITIONS})
target_include_directories(${PROJECT_NAME} PRIVATE ${BULLET_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${BULLET_LIBRARIES})

target_compile_definitions(${PROJECT_NAME} PRIVATE ${IMGUI_DEFINITIONS})
target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE "${PARENT_DIR}/libraries/imgui/include/loaders")
target_link_libraries(${PROJECT_NAME} ${IMGUI_LIBRARIES})
set(FILES_TO_ADD
	"${PARENT_DIR}/libraries/imgui/include/loaders/imgui_impl_glfw.h"
	"${PARENT_DIR}/libraries/imgui/include/loaders/imgui_impl_glfw.cpp"
	"${PARENT_DIR}/libraries/imgui/include/loaders/imgui_impl_opengl3.cpp"
	"${PARENT_DIR}/libraries/imgui/include/loaders/imgui_impl_opengl3.h"
	"${PARENT_DIR}/libraries/imgui/include/loaders/imgui_impl_opengl3_loader.h")
target_sources(${PROJECT_NAME} PRIVATE ${FILES_TO_ADD})
)";
	cmakeFile.close();
}
void EngineEditor::BuildProject(bool buildAndOpen)
{
	system(("cmake --build \"" + currentProject->projectPath + "/build\"").c_str());
	if (buildAndOpen)
		ShellExecuteA(NULL, "open", (currentProject->projectPath + "\\build\\Debug\\newproj.exe").c_str(), NULL, NULL, SW_SHOW);
}