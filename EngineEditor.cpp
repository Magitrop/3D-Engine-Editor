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

#include <nfd.h>
#include <stdio.h>
#include <stdlib.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

EngineEditor::Project::ProjectState EngineEditor::Project::projectState;
std::string EngineEditor::visualStudioVersion = "Visual Studio 17 2022";
EngineEditor::Project* EngineEditor::currentProject = nullptr;
char* EngineEditor::currentProjectPath = (char*)"";

GameObject* EngineEditor::selectedGameObject;
std::vector<std::tuple<std::string, Json::value_t, EngineEditor::FieldData>> EngineEditor::fields;

bool EngineEditor::uploadResource;
char* EngineEditor::vertexShaderPath = (char*)"";
char* EngineEditor::fragmentShaderPath = (char*)"";
char EngineEditor::shaderName[50]{};
char* EngineEditor::modelPath = (char*)"";
char EngineEditor::modelName[50]{};

bool EngineEditor::changeObjectName;
char EngineEditor::newObjectName[50]{};

bool EngineEditor::createComponent;
char EngineEditor::newComponentName[100]{};
std::map<std::string, const void*> EngineEditor::loadedCustomComponents;

int EngineEditor::fieldIndex = -1;
int EngineEditor::buttonIndex = 0;
Json EngineEditor::serializedObject;

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
				OpenProject();
				//Project::projectState = Project::ProjectState::Open;
			}
			if (currentProject && ImGui::MenuItem("Save"))
			{
				SaveProject();
			}
			ImGui::EndMenu();
		}
		if (currentProject != nullptr)
		{
			if (ImGui::BeginMenu("Resources"))
			{
				if (ImGui::MenuItem("Upload New Resource"))
				{
					uploadResource = true;

					for (int i = 0; i < 50; i++)
						shaderName[i] = modelName[i] = 0;
					vertexShaderPath = (char*)"";
					fragmentShaderPath = (char*)"";
				}
				if (ImGui::MenuItem("Create Component"))
				{
					createComponent = true;

					for (int i = 0; i < 50; i++)
						newComponentName[i] = 0;
				}
				if (ImGui::MenuItem("Update Components"))
				{
					UpdateComponents();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Build"))
			{
				if (ImGui::MenuItem("Build"))
				{
					BuildProject();
				}
				ImGui::EndMenu();
			}
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
	ImGui::SetNextWindowSize(ImVec2(300, 0));
	if (ImGui::Begin("Create new project", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
	{
		static char projectName[100]{};
		ImGui::Text("Project name"); ImGui::SameLine();
		ImGui::InputText("##label", projectName, IM_ARRAYSIZE(projectName));
		if (ImGui::Button("Create##shader", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)))
			CreateProject(std::string(projectName));
		ImGui::SameLine();
		if (ImGui::Button("Cancel##shader", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)))
			Project::projectState = currentProject ? Project::ProjectState::Opened : Project::ProjectState::None;
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
//		- 150	// loaded assets menu
		- 1		// loaded assets menu gap
	));
	if (ImGui::Begin("Objects", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse))
	{
		ImGui::PushItemWidth(ImGui::GetWindowSize().x);
		int objIndex = 0;
		for (auto& obj : ObjectsManager->instantiatedObjects)
		{
			if (obj->shouldBeSerialized && ImGui::Selectable((obj->name + "##" + std::to_string(objIndex++)).c_str(), false))
			{
				selectedGameObject = obj;
				fields.clear();
				serializedObject = Json();
				for (auto& comp : selectedGameObject->components)
					comp.second->Serialize(serializedObject);
				if (serializedObject.is_object())
					GetComponentFields(serializedObject);
			}
		}
		if (ImGui::Button("+"))
		{
			ObjectsManager->Instantiate();
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
//			- 150	// loaded assets menu
			- 1		// loaded assets menu gap
		));
		if (ImGui::Begin("Object Properties", nullptr, 
			ImGuiWindowFlags_AlwaysAutoResize | 
			ImGuiWindowFlags_NoResize | 
			ImGuiWindowFlags_NoMove | 
			ImGuiWindowFlags_NoCollapse | 
			ImGuiWindowFlags_HorizontalScrollbar))
		{
			ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 20);
			if (ImGui::Selectable(selectedGameObject->name.c_str(), false, 0, 
				ImVec2(ImGui::GetWindowWidth() - ImGui::CalcTextSize("Destroy").x - 30, 0)))
			{
				changeObjectName = true;
				for (int i = 0; i < 50; i++)
					if (i < selectedGameObject->name.size())
						newObjectName[i] = selectedGameObject->name[i];
					else newObjectName[i] = 0;
			}
			ImGui::SameLine();
			if (ImGui::Button("Destroy"))
			{
				ObjectsManager->DestroyObject(selectedGameObject);
				selectedGameObject = nullptr;
			}
			else
			{
				fields.clear();
				for (auto& comp : selectedGameObject->components)
					comp.second->Serialize(serializedObject);
				if (serializedObject.is_object())
					GetComponentFields(serializedObject);

				bool addedNew = false;
				fieldIndex = -1;
				buttonIndex = 0;
				ImGui::Unindent(16.f);
				if (serializedObject.is_object())
					for (auto& field : serializedObject.get<Json::object_t>())
						if (DrawComponentField(field, serializedObject, true))
						{
							addedNew = true;
							break;
						}
				ImGui::Indent(16.f);

				if (!addedNew)
				{
					fieldIndex = -2;
					SetComponentFields(serializedObject);
					for (auto& comp : selectedGameObject->components)
						comp.second->Deserialize(serializedObject);
				}

				ImGui::NewLine();
				ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 20);
				if (ImGui::BeginCombo("##labelAdd", "Add Component"))
				{
					if (ImGui::Selectable("Line Renderer"))
					{
						selectedGameObject->AddComponent<LineRendererComponent>();
						fields.clear();
						serializedObject = Json();
						for (auto& comp : selectedGameObject->components)
							comp.second->Serialize(serializedObject);
						if (serializedObject.is_object())
							GetComponentFields(serializedObject);
					}
					if (ImGui::Selectable("Model Renderer"))
					{
						selectedGameObject->AddComponent<ModelRendererComponent>();
						fields.clear();
						serializedObject = Json();
						for (auto& comp : selectedGameObject->components)
							comp.second->Serialize(serializedObject);
						if (serializedObject.is_object())
							GetComponentFields(serializedObject);
					}
					for (auto& comp : currentProject->customComponents)
					{
						if (ImGui::Selectable(comp.first.c_str()))
						{
							if (loadedCustomComponents[comp.first])
							{
								reinterpret_cast<void(*)(GameObject*, const Json&)>
									(GetProcAddress((HMODULE)loadedCustomComponents[comp.first], "_add_comp"))
									(selectedGameObject, Json());

								fields.clear();
								serializedObject = Json();
								for (auto& comp : selectedGameObject->components)
									comp.second->Serialize(serializedObject);
								if (serializedObject.is_object())
									GetComponentFields(serializedObject);
							}
							else
							{
								std::cout << "Unable to load component " << comp.first << std::endl;
							}
						}
					}
					ImGui::EndCombo();
				}
				ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 20);
				if (selectedGameObject->components.size() > 1 &&
					ImGui::BeginCombo("##labelRemove", "Remove Component"))
				{
					for (auto& comp : selectedGameObject->components)
					{
						if (comp.second->GetComponentName() != "Transform" &&
							ImGui::Selectable(comp.second->GetComponentName().c_str()))
						{
							selectedGameObject->RemoveComponent(comp.second->GetComponentName());
							fields.clear();
							serializedObject = Json();
							for (auto& comp : selectedGameObject->components)
								comp.second->Serialize(serializedObject);
							if (serializedObject.is_object())
								GetComponentFields(serializedObject);
							break;
						}
					}
					ImGui::EndCombo();
				}
			}
		}
		ImGui::End();
	}
#pragma endregion
}
void EngineEditor::DrawLoadedAssetsMenu()
{
	/*ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 150));
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 150));
	if (ImGui::Begin("##label", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
	{
		
	}
	ImGui::End();*/
}

bool EngineEditor::DrawUploadResourceMenu()
{
	bool uploaded = false;
	ImGui::SetNextWindowSize(ImVec2(300, 0));
	if (ImGui::Begin("Upload Resource", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::BeginTabBar("Upload Resource", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_NoCloseWithMiddleMouseButton))
		{
			if (ImGui::BeginTabItem("Shader"))
			{
				if (ImGui::CollapsingHeader("Vertex Shader"))
				{
					static std::string fileName = "none";
					ImGui::Indent(16.f);
					if (ImGui::Button("Select##vertex"))
					{
						NFD_OpenDialog("glsl", NULL, &vertexShaderPath);
						fileName = vertexShaderPath;
						fileName = fileName.substr(fileName.find_last_of("/\\") + 1);
					}
					ImGui::SameLine(); ImGui::Text(fileName.c_str());
					ImGui::Unindent(16.f);
				}
				if (ImGui::CollapsingHeader("Fragment Shader"))
				{
					static std::string fileName = "none";
					ImGui::Indent(16.f);
					if (ImGui::Button("Select##fragment"))
					{
						NFD_OpenDialog("glsl", NULL, &fragmentShaderPath);
						fileName = fragmentShaderPath;
						fileName = fileName.substr(fileName.find_last_of("/\\") + 1);
					}
					ImGui::SameLine();ImGui::Text(fileName.c_str());
					ImGui::Unindent(16.f);
				}
				ImGui::Text("Shader Name"); ImGui::SameLine();
				ImGui::InputText("##label", shaderName, 50);
				if (ImGui::Button("Upload##shader", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)) && 
					std::string(vertexShaderPath).size() > 0 &&
					std::string(fragmentShaderPath).size() > 0 &&
					std::string(shaderName).size() > 0)
				{
					ResourceManager->UploadShader(Shader(vertexShaderPath, fragmentShaderPath), shaderName);
					uploaded = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel##shader", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)))
					uploadResource = false;
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Model"))
			{
				if (ImGui::CollapsingHeader("Model"))
				{
					static std::string fileName = "none";
					ImGui::Indent(16.f);
					if (ImGui::Button("Select##model"))
					{
						NFD_OpenDialog("fbx", NULL, &modelPath);
						fileName = modelPath;
						fileName = fileName.substr(fileName.find_last_of("/\\") + 1);
					}
					ImGui::SameLine(); ImGui::Text(fileName.c_str());
					ImGui::Unindent(16.f);
				}
				ImGui::Text("Model Name"); ImGui::SameLine();
				ImGui::InputText("##label", modelName, 50);
				if (ImGui::Button("Upload##model", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)) &&
					std::string(modelPath).size() > 0 &&
					std::string(modelName).size() > 0)
				{
					ResourceManager->UploadModel(Model(modelPath), modelName);
					uploaded = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel##shader", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)))
					uploadResource = false;
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
	return uploaded;
}
bool EngineEditor::DrawChangeObjectNameMenu()
{
	bool uploaded = false;
	ImGui::SetNextWindowSize(ImVec2(300, 0));
	if (ImGui::Begin("Rename Object", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("##label", newObjectName, 50);
		if (ImGui::Button("Apply", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)))
		{
			selectedGameObject->name = newObjectName;
			uploaded = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)))
		{
			changeObjectName = false;
		}
	}
	ImGui::End();
	return uploaded;
}
bool EngineEditor::DrawCreateComponentMenu()
{
	bool created = false;
	ImGui::SetNextWindowSize(ImVec2(300, 0));
	if (ImGui::Begin("Create Component", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Component Name"); ImGui::SameLine();
		ImGui::InputText("##label", newComponentName, 50);
		if (ImGui::Button("Create", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)))
		{
			CreateComponent(newComponentName);
			created = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)))
		{
			createComponent = false;
		}
	}
	ImGui::End();
	return created;
}

void EngineEditor::DrawEngineMenu()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	DrawMenuBar();
	DrawLoadedAssetsMenu();
	if (uploadResource)
		if (DrawUploadResourceMenu())
			uploadResource = false;
	if (changeObjectName)
		if (DrawChangeObjectNameMenu())
			changeObjectName = false;
	if (createComponent)
		if (DrawCreateComponentMenu())
			createComponent = false;
	switch (Project::projectState)
	{
	case Project::ProjectState::Create:
		DrawCreateProjectMenu();
		break;
	/*case Project::ProjectState::Open:
		DrawOpenProjectMenu();
		break;*/
	case Project::ProjectState::Opened:
		DrawOpenedProjectMenu();
		break;
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void EngineEditor::OpenProject()
{
	if (NFD_OpenDialog("project", NULL, &currentProjectPath) != nfdresult_t::NFD_OKAY)
		return;
	ClearProject();

	currentProject = new Project();
	std::string projectFilePath = currentProjectPath;
	std::string projectName = projectFilePath;
	projectName = projectName.substr(projectName.find_last_of("/\\") + 1);
	currentProject->projectPath = projectFilePath.substr(0, projectFilePath.find_last_not_of(projectName));
	currentProject->projectName = projectName.substr(0, projectName.find_last_of('.'));

	std::ifstream projectFile(currentProject->projectPath + "/" + currentProject->projectName + ".project");
	std::stringstream buffer;
	buffer << projectFile.rdbuf();

	try
	{
		Json projectFile = Json::parse(buffer.str());

		Json components = projectFile["Components"];
		if (components.is_array())
			for (auto& o : components.get<Json::array_t>())
			{
				std::string componentName = o["name"];
				std::string lastWriteTime = o["date"];
				currentProject->customComponents.push_back(std::make_pair(componentName, lastWriteTime));
				if (std::to_string(std::filesystem::last_write_time(currentProject->projectName + R"(\)" + componentName + ".cpp").time_since_epoch().count())
					!= lastWriteTime)
				{
					std::stringstream cmd;
					cmd << R"(call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 )"
						R"(&& cl /std:c++17 /EHsc /MDd "libraries\glad\src\glad.c" ")"
						<< currentProject->projectName << R"(\ComponentsEntry.cpp" ")"
						<< currentProject->projectName << R"(\)" << componentName << R"(.cpp" /Fo".\)"
						<< currentProject->projectName << R"(\\")"
						R"( /I"..\EngineLib")"
						R"( /I"libraries\glad\include")"
						R"( /I"libraries\glfw\include")"
						R"( /I"libraries\glm")"
						R"( /I"libraries\json\include")"
						R"( /I"libraries\assimp\include")"
						R"( /I"libraries\engine\debug\include")"
						R"( /I"libraries\controllers\debug\include")"
						R"( /link opengl32.lib user32.lib gdi32.lib shell32.lib "libraries\glad\Debug\glad.lib" "libraries\glfw\build\src\Debug\glfw3.lib" "libraries\assimp\lib\Debug\assimp.lib")"
						R"(&& link /DLL /out:")"
						<< currentProject->projectName << R"(\)" << componentName << R"(.dll" ")"
						<< currentProject->projectName << R"(\)" << componentName << R"(.obj" ")"
						<< currentProject->projectName << R"(\ComponentsEntry.obj")";
					system(cmd.str().c_str());
				}
			}

		Json scene = projectFile["Scene"];
		if (scene.is_array())
			for (auto& o : scene.get<Json::array_t>())
			{
				GameObject* obj = ObjectsManager->Instantiate();
				obj->name = o.begin().key();
				for (auto& comp : o.begin().value().get<Json::array_t>())
				{
					std::string componentName = comp.begin().key();
					if (componentName == "Transform")
					{
						obj->AddComponent<TransformComponent>()->Deserialize(comp);
					}
					else if (componentName == "Camera")
					{
						obj->AddComponent<CameraComponent>()->Deserialize(comp);
					}
					else if (componentName == "Model Renderer")
					{
						obj->AddComponent<ModelRendererComponent>()->Deserialize(comp);
					}
					else if (componentName == "Line Renderer")
					{
						obj->AddComponent<LineRendererComponent>()->Deserialize(comp);
					}
					else
					{
						if (loadedCustomComponents.find(componentName) == loadedCustomComponents.end())
						{
							std::string componentPath = currentProject->projectName + R"(\)";
							componentPath += componentName;
							componentPath += R"(.dll)";
							HINSTANCE hDll = LoadLibraryA(componentPath.c_str());
							if (hDll)
							{
								loadedCustomComponents[componentName] = hDll;
								reinterpret_cast<void(*)(SingletonManager*)>(GetProcAddress(hDll, "_set_singleton_manager"))(SingletonManager::Instance());
								reinterpret_cast<void(*)(GameObject*, const Json&)>(GetProcAddress(hDll, "_add_comp"))(obj, comp);
							}
							else
							{
								std::cout << "Unable to load component " << componentName << std::endl;
							}
						}
						else
						{
							reinterpret_cast<void(*)(GameObject*, const Json&)>
								(GetProcAddress((HMODULE)loadedCustomComponents[componentName], "_add_comp"))
								(obj, comp);
						}
					}
				}
			}
		//UpdateComponents();
	}
	catch (Json::parse_error)
	{
		std::cout << "Error with .project file, the project may be loaded incorrectly." << std::endl;
		delete currentProject;
		currentProject = nullptr;

		glfwSetWindowTitle(InitializationHandler->GetWindow(), "Current project: none");
		Project::projectState = Project::ProjectState::None;
		return;
	}

	std::string windowTitle = "Current project: " + currentProject->projectName;
	glfwSetWindowTitle(InitializationHandler->GetWindow(), windowTitle.c_str());

	ResetProject();
	CreateMainObjects();
	Project::projectState = Project::ProjectState::Opened;
}
void EngineEditor::CreateProject(std::string projectName)
{
	ClearProject();

	currentProject = new Project();
	currentProject->projectName = projectName;
	currentProject->projectPath = std::filesystem::current_path().string() + "/" + projectName;
	std::filesystem::create_directory(currentProject->projectPath);
	std::filesystem::create_directory(currentProject->projectPath + "/build");

	std::ofstream componentsEntryFile(currentProject->projectPath + "/ComponentsEntry.cpp");
	componentsEntryFile << R"(
#include <windows.h>
#include <iostream>
#include <Singleton.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "glad.lib")
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "EngineLib.lib")

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, INT nCmdShow)
{
    return 0;
}

extern "C" __declspec(dllexport) inline void _set_singleton_manager(SingletonManager* instance)
{
    SingletonManager::Instance() = instance;
}
)";

	std::ofstream cmakeFile(currentProject->projectPath + "/CMakeLists.txt");
	cmakeFile << R"(
cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
get_filename_component(PARENT_DIR ../ ABSOLUTE)	

set(PROJECT_NAME )" << currentProject->projectName << R"()

project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Bullet REQUIRED PATHS "${PARENT_DIR}/libraries/bullet3")
find_package(Imgui REQUIRED PATHS "${PARENT_DIR}/libraries/imgui")

include_directories(${PARENT_DIR}/libraries/engine/debug/include)
link_directories(${PARENT_DIR}/libraries/engine/debug/lib)
find_library(EngineLib EngineLib "${PARENT_DIR}/libraries/engine/debug/lib")

include_directories(${PARENT_DIR}/libraries/controllers/debug/include)
link_directories(${PARENT_DIR}/libraries/controllers/debug/lib)
find_library(ControllersLib ControllersLib "${PARENT_DIR}/libraries/controllers/debug/lib")

include_directories(${PARENT_DIR}/libraries/freetype/include)
find_library(FreetypeLib freetype "${PARENT_DIR}/libraries/freetype/release dll/win64")

include_directories(${PARENT_DIR}/libraries/assimp/include)
find_library(AssimpLib assimp "${PARENT_DIR}/libraries/assimp/lib/Debug")

add_executable(
	${PROJECT_NAME}
		ComponentsEntry.cpp
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

	system(("CMake\\bin\\cmake -G \"" +
		visualStudioVersion + "\" \"" +
		currentProject->projectPath + "\" -B \"" +
		currentProject->projectPath + "/build\"").c_str());

	ResetProject();
	CreateMainObjects();
	Project::projectState = Project::ProjectState::Opened;
}
void EngineEditor::CreateComponent(std::string componentName)
{
	if (!currentProject)
		return;

	std::ofstream componentFile(currentProject->projectPath + "/" + componentName + ".cpp");
	componentFile << R"(
#include <Engine.h>

extern "C"
{
	namespace )" + componentName + R"(
	{
		class )" + componentName + R"( : public Component
		{
			COMPONENT()" + componentName + R"(, Component)
		public:
			int sample;

			void Serialize(Json& writeTo) override
			{
				writeTo[GetComponentName()]["sample"] = sample;
			}

			void Deserialize(const Json& readFrom) override
			{
				if (readFrom[GetComponentName()].contains("sample")) sample = readFrom[GetComponentName()]["sample"];
			}

			virtual std::string GetComponentName() override
			{
				return ")" + componentName + R"(";
			}
		};

		// do not modify!
		__declspec(dllexport) inline void _add_comp(GameObject* go, const Json& readFrom)
		{
			gladLoadGL();
			if (readFrom.is_null())
				go->AddComponent<)" + componentName + R"(>();
			else
				go->AddComponent<)" + componentName + R"(>()->Deserialize(readFrom);
		}

		// do not modify!
		__declspec(dllexport) inline void _remove_comp(GameObject* go)
		{
			go->RemoveComponent<)" + componentName + R"(>();
		}

		// do not modify!
		__declspec(dllexport) inline bool _has_comp(GameObject* go)
		{
			return go->HasComponent<)" + componentName + R"(>();
		}

		// do not modify!
		__declspec(dllexport) inline Json _serialize_comp(GameObject* go)
		{
			Json j;
			go->GetComponent<)" + componentName + R"(>()->Serialize(j);
			return j;
		}
	}
}
)";
	componentFile.close();
	currentProject->customComponents.push_back(
		std::make_pair(
			componentName, 
			"0"));

	std::string allComponents;
	for (auto it = currentProject->customComponents.begin(); it != currentProject->customComponents.end(); it++)
		allComponents += it->first + ".cpp\n";
	std::ofstream cmakeFile(currentProject->projectPath + "/CMakeLists.txt");
	cmakeFile << R"(
cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
get_filename_component(PARENT_DIR ../ ABSOLUTE)	

set(PROJECT_NAME )" << currentProject->projectName << R"()

project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Bullet REQUIRED PATHS "../libraries/bullet3")
find_package(Imgui REQUIRED PATHS "${PARENT_DIR}/libraries/imgui")

include_directories(${PARENT_DIR}/libraries/engine/debug/include)
link_directories(${PARENT_DIR}/libraries/engine/debug/lib)
find_library(EngineLib EngineLib "${PARENT_DIR}/libraries/engine/debug/lib")

include_directories(${PARENT_DIR}/libraries/controllers/debug/include)
link_directories(${PARENT_DIR}/libraries/controllers/debug/lib)
find_library(ControllersLib ControllersLib "${PARENT_DIR}/libraries/controllers/debug/lib")

include_directories(../libraries/freetype/include)
find_library(FreetypeLib freetype "../libraries/freetype/release dll/win64")

include_directories(../libraries/assimp/include)
find_library(AssimpLib assimp "../libraries/assimp/lib/Debug")

add_executable(
	${PROJECT_NAME}
	ComponentsEntry.cpp
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
void EngineEditor::UpdateComponents()
{
	if (!currentProject)
		return;

	std::map<std::string, std::vector<std::pair<GameObject*, Json>>> objectsWithCustomComponents;
	for (auto& obj : ObjectsManager->instantiatedObjects)
		for (auto& comp : loadedCustomComponents)
			if (comp.second != nullptr && 
				reinterpret_cast<bool(*)(GameObject*)>(GetProcAddress((HMODULE)comp.second, "_has_comp"))(obj))
			{
				Json serializedComp = reinterpret_cast<Json(*)(GameObject*)>(GetProcAddress((HMODULE)comp.second, "_serialize_comp"))(obj);
				reinterpret_cast<void(*)(GameObject*)>(GetProcAddress((HMODULE)comp.second, "_remove_comp"))(obj);
				objectsWithCustomComponents[comp.first].push_back(std::make_pair(obj, serializedComp));
			}

	for (auto& comp : loadedCustomComponents)
		FreeLibrary((HMODULE)comp.second);
	loadedCustomComponents.clear();

	for (auto& comp : currentProject->customComponents)
	{
		if (std::to_string(std::filesystem::last_write_time(currentProject->projectName + R"(\)" + comp.first + ".cpp").time_since_epoch().count())
			!= comp.second)
		{
			std::stringstream cmd;
			cmd << R"(call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 )"
				R"(&& cl /std:c++17 /EHsc /MDd "libraries\glad\src\glad.c" ")"
				<< currentProject->projectName << R"(\ComponentsEntry.cpp" ")"
				<< currentProject->projectName << R"(\)" << comp.first << R"(.cpp" /Fo".\)"
				<< currentProject->projectName << R"(\\")"
				R"( /I"..\EngineLib")"
				R"( /I"libraries\glad\include")"
				R"( /I"libraries\glfw\include")"
				R"( /I"libraries\glm")"
				R"( /I"libraries\json\include")"
				R"( /I"libraries\assimp\include")"
				R"( /I"libraries\engine\debug\include")"
				R"( /I"libraries\controllers\debug\include")"
				R"( /link opengl32.lib user32.lib gdi32.lib shell32.lib "libraries\glad\Debug\glad.lib" "libraries\glfw\build\src\Debug\glfw3.lib" "libraries\assimp\lib\Debug\assimp.lib")"
				R"(&& link /DLL /out:")"
				<< currentProject->projectName << R"(\)" << comp.first << R"(.dll" ")"
				<< currentProject->projectName << R"(\)" << comp.first << R"(.obj" ")"
				<< currentProject->projectName << R"(\ComponentsEntry.obj")";
			system(cmd.str().c_str());
			comp.second = std::to_string(std::filesystem::last_write_time(currentProject->projectName + R"(\)" + comp.first + ".cpp").time_since_epoch().count());
		}
		std::string componentPath = currentProject->projectName + R"(\)";
		componentPath += comp.first;
		componentPath += R"(.dll)";
		HINSTANCE hDll = LoadLibraryA(componentPath.c_str());
		if (hDll)
		{
			loadedCustomComponents[comp.first] = hDll;
		}
		else
		{
			std::cout << "Unable to load component " << comp.first << std::endl;
		}
	}

	for (auto& comp : loadedCustomComponents)
	{
		if (objectsWithCustomComponents.find(comp.first) != objectsWithCustomComponents.end())
		{
			reinterpret_cast<void(*)(SingletonManager*)>(GetProcAddress((HMODULE)comp.second, "_set_singleton_manager"))(SingletonManager::Instance());
			for (auto& obj : objectsWithCustomComponents[comp.first])
				reinterpret_cast<void(*)(GameObject*, const Json&)>(GetProcAddress((HMODULE)comp.second, "_add_comp"))(obj.first, obj.second);
		}
	}
}
void EngineEditor::BuildProject()
{
	if (!currentProject)
		return;

	SaveProject();
	system(("CMake\\bin\\cmake --build \"" + currentProject->projectPath + "/build\"").c_str());
	std::cout << "next" << std::endl;

	std::filesystem::path assimpDll = "assimp-vc143-mtd.dll";
	std::filesystem::path freetypeDll = "freetype.dll";
	std::filesystem::path projectFile = currentProject->projectName + "/" + currentProject->projectName + ".project";
	std::filesystem::path sceneFile = "MainScene.scene";
	std::filesystem::path targetParent = (currentProject->projectPath + "/build/Debug").c_str();
	std::filesystem::path sourceDirectory = "source";
	try
	{
		std::filesystem::create_directories(targetParent);
		std::filesystem::copy(sourceDirectory, targetParent / sourceDirectory, std::filesystem::copy_options::overwrite_existing);
		std::filesystem::copy_file(assimpDll, targetParent / assimpDll.filename(), std::filesystem::copy_options::overwrite_existing);
		std::filesystem::copy_file(freetypeDll, targetParent / freetypeDll.filename(), std::filesystem::copy_options::overwrite_existing);
		for (auto& customComponent : currentProject->customComponents)
		{
			std::filesystem::path compName = customComponent.first + ".dll";
			std::filesystem::path compPath = currentProject->projectName + "/" + customComponent.first + ".dll";
			std::filesystem::copy_file(compPath, targetParent / compName.filename(), std::filesystem::copy_options::overwrite_existing);
		}
		std::filesystem::copy_file(projectFile, targetParent / sourceDirectory / sceneFile.filename(), std::filesystem::copy_options::overwrite_existing);
	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}

	system(("start \"\" \"" + currentProject->projectPath + "/build/Debug\"").c_str());
	//if (buildAndOpen)
	//	ShellExecuteA(NULL, "open", (currentProject->projectPath + "\\build\\Debug\\" + currentProject->projectName + ".exe").c_str(), NULL, NULL, SW_SHOW);
}

void EngineEditor::SaveProject()
{
	if (!currentProject)
		return;

	std::ofstream projectFile(currentProject->projectPath + "/" + currentProject->projectName + ".project");

	//for (auto it = currentProject->customComponents.begin(); it != currentProject->customComponents.end(); it++)
	//	projectFile << "";

	Json scene;
	for (auto it = ObjectsManager->instantiatedObjects.begin(); it != ObjectsManager->instantiatedObjects.end(); it++)
		if ((*it)->shouldBeSerialized)
			scene["Scene"].push_back((*it)->Serialize());
	for (auto it = currentProject->customComponents.begin(); it != currentProject->customComponents.end(); it++)
	{
		Json comp;
		comp["name"] = it->first;
		comp["date"] = std::to_string(std::filesystem::last_write_time(currentProject->projectName + R"(\)" + it->first + ".cpp").time_since_epoch().count());
		scene["Components"].push_back(comp);
	}
	projectFile << scene;
}

void EngineEditor::EnableObjectSerialization(GameObject* obj, bool enable)
{
	obj->shouldBeSerialized = enable;
}

void EngineEditor::GetComponentFields(Json& j)
{
	for (auto& field : j.get<Json::object_t>())
	{
		switch (field.second.type())
		{
		case Json::value_t::number_integer:
		{
			int i = j[field.first];
			FieldData data = { };
			data.i = i;
			fields.push_back(std::make_tuple(field.first, Json::value_t::number_integer, data));
			break;
		}
		case Json::value_t::number_float:
		{
			float f = j[field.first];
			FieldData data = { };
			data.f = f;
			fields.push_back(std::make_tuple(field.first, Json::value_t::number_float, data));
			break;
		}
		case Json::value_t::boolean:
		{
			bool b = j[field.first];
			FieldData data = { };
			data.b = b;
			fields.push_back(std::make_tuple(field.first, Json::value_t::boolean, data));
			break;
		}
		case Json::value_t::string:
		{
			std::string s = field.second;
			s = s.substr(0, 50);
			FieldData data = { };
			for (int i = 0; i < 50; i++)
				if (i < s.size())
					data.s[i] = s[i];
				else data.s[i] = 0;
			fields.push_back(std::make_tuple(field.first, Json::value_t::string, data));
			break;
		}
		case Json::value_t::array:
		{
			FieldData data = { };
			int index = 0;
			fields.push_back(std::make_tuple(field.first, Json::value_t::array, data));
			for (auto& f : field.second)
			{
				fields.push_back(std::make_tuple(std::to_string(index++), Json::value_t::object, data));
				GetComponentFields(f);
			}
			break;
		}
		case Json::value_t::object:
		{
			FieldData data = { };
			fields.push_back(std::make_tuple(field.first, Json::value_t::object, data));
			GetComponentFields(field.second);
			break;
		}
		}
	}
}
void EngineEditor::SetComponentFields(Json& parent)
{
	fieldIndex++;
	Json temp = parent;
	switch (temp.type())
	{
	case Json::value_t::number_integer:
	{
		temp = std::get<2>(fields[fieldIndex]).i;
		break;
	}
	case Json::value_t::number_float:
	{
		temp = std::get<2>(fields[fieldIndex]).f;
		break;
	}
	case Json::value_t::boolean:
	{
		temp = std::get<2>(fields[fieldIndex]).b;
		break;
	}
	case Json::value_t::string:
	{
		temp = std::get<2>(fields[fieldIndex]).s;
		break;
	}
	case Json::value_t::array:
	{
		for (auto& f : temp)
			SetComponentFields(f);
		break;
	}
	case Json::value_t::object:
	{
		for (auto& field : temp.get<Json::object_t>())
		{
			Json tempObj = temp[field.first];
			SetComponentFields(tempObj);
			temp[field.first] = tempObj;
		}
		break;
	}
	}
	parent = temp;
}
bool EngineEditor::DrawComponentField(std::pair<const std::string, Json>& j, Json& parent, bool shouldBeDrawn)
{
	fieldIndex++;
	std::string label = "##label";
	label += fieldIndex;

	std::ostringstream ss;
	ss << std::left << std::setfill(' ') << std::setw(100) << j.first;

	ImGui::Indent(16.0f);
	switch (std::get<1>(fields[fieldIndex]))
	{
	case Json::value_t::number_integer:
	{
		if (!shouldBeDrawn) break;
		ImGui::Text(ss.str().substr(0, 20).c_str()); ImGui::SameLine();
		ImGui::InputInt(label.c_str(), &std::get<2>(fields[fieldIndex]).i);
		break;
	}
	case Json::value_t::number_float:
	{
		if (!shouldBeDrawn) break;
		ImGui::Text(ss.str().substr(0, 20).c_str()); ImGui::SameLine();
		ImGui::InputFloat(label.c_str(), &std::get<2>(fields[fieldIndex]).f, 0.f, 0.f, "%.8f");
		break;
	}
	case Json::value_t::boolean:
	{
		if (!shouldBeDrawn) break;

		size_t attributeStart, attributeEnd;
		if ((attributeStart = ss.str().find_first_of('[')) != std::string::npos &&
			(attributeEnd = ss.str().find_first_of(']')) != std::string::npos)
		{
			std::string attribute = ss.str().substr(attributeStart + 1, attributeEnd - attributeStart - 1);
			if (attribute != "hide")
			{
				ImGui::Text(ss.str().substr(attributeEnd + 1, 20).c_str()); ImGui::SameLine();
				ImGui::Checkbox(label.c_str(), &std::get<2>(fields[fieldIndex]).b);
			}
		}
		else
		{
			ImGui::Text(ss.str().substr(0, 20).c_str()); ImGui::SameLine();
			ImGui::Checkbox(label.c_str(), &std::get<2>(fields[fieldIndex]).b);
		}
		break;
	}
	case Json::value_t::string:
	{
		if (!shouldBeDrawn) break;

		size_t attributeStart, attributeEnd;
		if ((attributeStart = ss.str().find_first_of('[')) != std::string::npos &&
			(attributeEnd	= ss.str().find_first_of(']')) != std::string::npos)
		{
			ImGui::Text(ss.str().substr(attributeEnd + 1, 20).c_str());
			std::string attribute = ss.str().substr(attributeStart + 1, attributeEnd - attributeStart - 1);
			if (attribute == "shader")
			{
				ImGui::SameLine();
				if (ImGui::BeginCombo(label.c_str(), std::get<2>(fields[fieldIndex]).s))
				{
					for (auto& s : ResourceManager->loadedShaders)
					{
						if (ImGui::Selectable(s.first.c_str()))
						{
							std::get<2>(fields[fieldIndex]).s = const_cast<char*>(s.first.c_str());
						}
					}
					ImGui::EndCombo();
				}
			}
			else if (attribute == "model")
			{
				ImGui::SameLine();
				if (ImGui::BeginCombo(label.c_str(), std::get<2>(fields[fieldIndex]).s))
				{
					for (auto& s : ResourceManager->loadedModels)
					{
						if (ImGui::Selectable(s.first.c_str()))
						{
							std::get<2>(fields[fieldIndex]).s = const_cast<char*>(s.first.c_str());
						}
					}
					ImGui::EndCombo();
				}
			}
		}
		else
		{
			ImGui::Text(ss.str().substr(0, 20).c_str()); ImGui::SameLine();
			ImGui::InputText(label.c_str(), std::get<2>(fields[fieldIndex]).s, 50);
		}
		break;
	}
	case Json::value_t::array:
	{
		int index = 0;
		std::string header = j.first;
		header += "###";
		header += std::to_string(fieldIndex);
		if (shouldBeDrawn && ImGui::CollapsingHeader(header.c_str()))
		{
			for (auto& field : j.second.get<Json::array_t>())
			{
				std::string buttonHeader = "x##" + std::to_string(buttonIndex++);
				ImGui::Unindent(3.0f);
				if (j.second.get<Json::array_t>().size() > 1)
				{
					if (ImGui::Button(buttonHeader.c_str()))
					{
						j.second.erase(index);
						parent[j.first] = j.second;
						return true;
					}
					ImGui::SameLine();
				}
				ImGui::Indent(3.0f);

				std::pair<const std::string, Json> p = std::make_pair(std::to_string(index++), field);
				ImGui::Indent(3.0f);
				if (DrawComponentField(p, j.second, true))
				{
					if (parent.is_object())
						parent[j.first] = j.second;
					else
						parent[std::stoi(j.first)] = j.second;
					return true;
				}
				ImGui::Unindent(3.0f);
			}
			std::string buttonHeader = "+##" + std::to_string(buttonIndex++);
			ImGui::Indent(16.0f);
			if (ImGui::Button(buttonHeader.c_str()))
			{
				j.second.push_back(*(j.second.get<Json::array_t>().end() - 1));
				parent[j.first] = j.second;
				return true;
			}
			/*if (j.second.get<Json::array_t>().size() > 1)
			{
				buttonHeader = "-##" + std::to_string(buttonIndex++);
				ImGui::SameLine();
				if (ImGui::Button(buttonHeader.c_str()))
				{
					j.second.erase(j.second.get<Json::array_t>().size() - 1);
					parent[j.first] = j.second;
					return true;
				}
			}*/
			ImGui::Unindent(16.0f);
		}
		else
			for (auto& field : j.second.get<Json::array_t>())
			{
				std::pair<const std::string, Json> p = std::make_pair(std::to_string(index++), field);
				if (DrawComponentField(p, j.second, false))
				{
					if (parent.is_object())
						parent[j.first] = j.second;
					else
						parent[std::stoi(j.first)] = j.second;
					return true;
				}
			}
		break;
	}
	case Json::value_t::object:
	{
		std::string header = j.first;
		header += "##";
		header += std::to_string(fieldIndex);
		if (shouldBeDrawn && ImGui::CollapsingHeader(header.c_str()))
		{
			for (auto& field : j.second.get<Json::object_t>())
				if (DrawComponentField(field, j.second, true))
				{
					if (parent.is_object())
						parent[j.first] = j.second;
					else
						parent[std::stoi(j.first)] = j.second;
					return true;
				}
		}
		else
			for (auto& field : j.second.get<Json::object_t>())
				if (DrawComponentField(field, j.second, false))
				{
					if (parent.is_object())
						parent[j.first] = j.second;
					else
						parent[std::stoi(j.first)] = j.second;
					return true;
				}
		break;
	}
	}
	ImGui::Unindent(16.0f);
	return false;
}

void EngineEditor::CreateMainObjects()
{
	// creating camera
	auto camera = ObjectsManager->Instantiate<CameraComponent>(Vector3(0, 0, -3));
	camera->gameObject->name = "Main Camera";
	EventsController->SetAsMainCamera(camera);
	EngineEditor::EnableObjectSerialization(camera->gameObject, false);

	// creating grid
	std::vector<Vertex> vertices =
	{
		Vector3(1, 1, 0), Vector3(-1, -1, 0), Vector3(-1, 1, 0),
		Vector3(-1, -1, 0), Vector3(1, 1, 0), Vector3(1, -1, 0)
	};
	std::vector<unsigned int> triangles =
	{
		0, 1, 2,
		3, 4, 5
	};
	std::vector<Texture> textures = {};
	ModelRendererComponent* grid = ObjectsManager->Instantiate<ModelRendererComponent>();
	grid->SetModel(new Model({ new Mesh(vertices, triangles, textures) }));
	grid->SetShader(&ResourceManager->GetShader("Grid"));
	grid->useDepthBuffer = true;
	EngineEditor::EnableObjectSerialization(grid->gameObject, false);
}
void EngineEditor::ResetProject()
{
	selectedGameObject = nullptr;
	fields.clear();

	vertexShaderPath = (char*)"";
	fragmentShaderPath = (char*)"";
	modelPath = (char*)"";

	fieldIndex = -1;
	buttonIndex = 0;
	serializedObject = Json();
}
void EngineEditor::ClearProject()
{
	if (currentProject)
		delete currentProject;

	while (!ObjectsManager->instantiatedObjects.empty())
		ObjectsManager->DestroyObject(ObjectsManager->instantiatedObjects.front());

	for (auto& comp : loadedCustomComponents)
		FreeLibrary((HMODULE)comp.second);
	loadedCustomComponents.clear();

	ObjectsManager->renderQueue.clear();
	ObjectsManager->nextObjectID = 0;
	ObjectsManager->nextComponentID = 0;

	selectedGameObject = nullptr;
}
