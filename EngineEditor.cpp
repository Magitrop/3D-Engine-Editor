#include "EngineEditor.h"

#ifdef __gl_h_
#undef __gl_h_
#endif // __gl_h_
#include <glad/glad.h>

#include <thread>

#include <windows.h>
#include <fstream>
#include <filesystem>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
#include "EngineCamera.h"

EngineEditor::Project::ProjectState EngineEditor::Project::projectState;
std::string EngineEditor::visualStudioVersion = "Visual Studio 17 2022";
std::string EngineEditor::visualStudioCompilerPath;

EngineEditor::Project* EngineEditor::currentProject = nullptr;
char* EngineEditor::currentProjectPath = (char*)"";

GameObject* EngineEditor::selectedGameObject;
std::vector<std::tuple<std::string, Json::value_t, EngineEditor::FieldData>> EngineEditor::fields;

bool EngineEditor::uploadResource;
std::vector<std::string> EngineEditor::uploadingResources;
std::mutex EngineEditor::uploadingResourcesMutex;
std::vector<std::pair<Model*, GLFWwindow*>> EngineEditor::pendingLoadedModels;
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

Json EngineEditor::playSimulationSavedState;
bool EngineEditor::simulationIsPlaying;

ModelRendererComponent* EngineEditor::editorGridRenderer;
ModelRendererComponent* EngineEditor::editorAxesRenderer;

void EngineEditor::DrawMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Projects"))
		{
			if (ImGui::MenuItem("New project"))
			{
				createComponent = changeObjectName = uploadResource = false;
				Project::projectState = Project::ProjectState::Create;
			}
			if (ImGui::MenuItem("Open project"))
			{
				OpenProject();
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
					changeObjectName = false;
					createComponent = false;

					for (int i = 0; i < 50; i++)
						shaderName[i] = modelName[i] = 0;
					vertexShaderPath = (char*)"";
					fragmentShaderPath = (char*)"";
				}
				if (ImGui::MenuItem("Create Component"))
				{
					createComponent = true;
					changeObjectName = false;
					uploadResource = false;

					for (int i = 0; i < 50; i++)
						newComponentName[i] = 0;
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
	if (!currentProject)
		return;

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
			// change object's name color if it is inactive
			ImGui::PushStyleColor(ImGuiCol_Text, obj->isActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(150, 150, 150, 255));
			if (obj->shouldBeSerialized && ImGui::Selectable((obj->name + "##" + std::to_string(objIndex++)).c_str(), false))
			{
				changeObjectName = false;
				selectedGameObject = obj;
				fields.clear();
				serializedObject = Json();
				for (auto& comp : selectedGameObject->components)
					comp.second->Serialize(serializedObject);
				if (serializedObject.is_object())
					GetComponentFields(serializedObject);
			}
			ImGui::PopStyleColor();
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
		if (!simulationIsPlaying)
		{
			editorAxesRenderer->gameObject->isActive = true;
			editorAxesRenderer->gameObject->transform->SetPosition(selectedGameObject->transform->GetPosition());
			editorAxesRenderer->gameObject->transform->SetRotation(selectedGameObject->transform->GetRotation());
		}

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
			ImGui::Checkbox("##labelActive", &selectedGameObject->isActive); ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 20);
			if (ImGui::Selectable(selectedGameObject->name.c_str(), false, 0, 
				ImVec2(ImGui::GetWindowWidth() - ImGui::CalcTextSize("Destroy").x - 60, 0)))
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
				static bool addedNew = false;
				if (!addedNew)
				{
					fields.clear();
					for (auto& comp : selectedGameObject->components)
						comp.second->Serialize(serializedObject);
					if (serializedObject.is_object())
						GetComponentFields(serializedObject);
				}

				addedNew = false;
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
				else
				{
					fields.clear();
					if (serializedObject.is_object())
						GetComponentFields(serializedObject);
				}

				ImGui::NewLine();
				ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 20);
				if (ImGui::BeginCombo("##labelAdd", "Add Component"))
				{
					if (ImGui::Selectable("Camera"))
					{
						selectedGameObject->AddComponent<CameraComponent>();
					}
					if (ImGui::Selectable("Line Renderer"))
					{
						selectedGameObject->AddComponent<LineRendererComponent>();
					}
					if (ImGui::Selectable("Model Renderer"))
					{
						selectedGameObject->AddComponent<ModelRendererComponent>();
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
	else
	{
		editorAxesRenderer->gameObject->isActive = false;
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
	if (!currentProject)
		return false;

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
					std::thread uploadShaderThread(
						[&]()
						{
							// locking queue in order to add new resource name
							uploadingResourcesMutex.lock();
							auto it = uploadingResources.insert(uploadingResources.begin(), shaderName);
							uploadingResourcesMutex.unlock();

							ResourceManager->UploadShader(new Shader(vertexShaderPath, fragmentShaderPath), shaderName);

							// locking queue in order to delete resource name
							uploadingResourcesMutex.lock();
							uploadingResources.erase(it);
							uploadingResourcesMutex.unlock();
						});
					uploadShaderThread.detach();
					uploaded = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel##shader", ImVec2(ImGui::GetWindowWidth() / 2 - 10, 20)))
					uploadResource = false;
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Model"))
			{
				static std::string fileName = "none";
				if (ImGui::CollapsingHeader("Model"))
				{
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
					glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
					auto window = glfwCreateWindow(1, 1, "Loading", nullptr, InitializationHandler->GetWindow());
					glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
					std::thread uploadModelThread(
						[&, window]() 
						{
							std::string visibleName = std::string(modelName) + " (" + fileName + ")";

							// locking queue in order to add new resource name
							uploadingResourcesMutex.lock();
							uploadingResources.push_back(visibleName);
							uploadingResourcesMutex.unlock();

							glfwMakeContextCurrent(window);
							pendingLoadedModels.push_back(std::make_pair(new Model(modelPath), window));

							// locking queue in order to delete resource name
							uploadingResourcesMutex.lock();
							uploadingResources.erase(std::find(uploadingResources.begin(), uploadingResources.end(), visibleName));
							uploadingResourcesMutex.unlock();
						});
					uploadModelThread.detach();
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
void EngineEditor::DrawLoadingResourcesQueue()
{
	int index = 1;
	for (auto& r : uploadingResources)
	{
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 250, ImGui::GetIO().DisplaySize.y - 30 * index));
		ImGui::SetNextWindowSize(ImVec2(250, 30));
		std::string windowName = "##labelUploading";
		windowName += index;
		if (ImGui::Begin(windowName.c_str(), nullptr,
			ImGuiWindowFlags_NoResize | 
			ImGuiWindowFlags_AlwaysAutoResize | 
			ImGuiWindowFlags_NoCollapse | 
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoTitleBar))
		{
			std::string label = "Loading " + r + "...";
			ImGui::Text(label.c_str());
		}
		ImGui::End();
		index++;
	}
}
void EngineEditor::IteratePendingLoadedModels()
{
	auto it = pendingLoadedModels.begin();
	while (it != pendingLoadedModels.end())
	{
		for (auto& mesh : it->first->meshes)
			mesh->SetupMesh();
		ResourceManager->UploadModel(it->first, modelName);
		glfwDestroyWindow(it->second);
		it = pendingLoadedModels.erase(it);
	}
}

bool EngineEditor::DrawChangeObjectNameMenu()
{
	if (!currentProject || !selectedGameObject)
		return false;

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
	if (!currentProject)
		return false;

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
	DrawLoadingResourcesQueue();
	IteratePendingLoadedModels();

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
	std::ifstream configFile(currentProject->projectPath + "/" + currentProject->projectName + ".config");
	std::stringstream saveBuffer, configBuffer;
	saveBuffer << projectFile.rdbuf();
	configBuffer << configFile.rdbuf();

	try
	{
		Json saveFile = Json::parse(saveBuffer.str());
		Json configFile = Json::parse(configBuffer.str());

		Json components = configFile["Components"];
		if (components.is_array())
			for (auto& o : components.get<Json::array_t>())
			{
				std::string componentName = o["name"];
				std::string lastWriteTime = o["date"];
				if (std::to_string(std::filesystem::last_write_time(currentProject->projectName + R"(\)" + componentName + ".cpp").time_since_epoch().count())
					!= lastWriteTime)
				{
					std::stringstream cmd;
					//C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat
					cmd << R"(call ")" << visualStudioCompilerPath << R"(" x64 )"
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
					if (!system(cmd.str().c_str()))
						lastWriteTime = std::to_string(std::filesystem::last_write_time(currentProject->projectName + R"(\)" + componentName + ".cpp").time_since_epoch().count());
				}
				currentProject->customComponents.push_back(std::make_pair(componentName, lastWriteTime));
				std::string componentPath = currentProject->projectName + R"(\)";
				componentPath += componentName;
				componentPath += R"(.dll)";
				HINSTANCE hDll = LoadLibraryA(componentPath.c_str());
				if (hDll)
				{
					loadedCustomComponents[componentName] = hDll;
					reinterpret_cast<void(*)(SingletonManager*)>(GetProcAddress(hDll, "_set_singleton_manager"))(SingletonManager::Instance());
				}
				else
				{
					std::cout << "Unable to load component " << componentName << std::endl;
				}
			}

		Json scene = saveFile["Scene"];
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
						if (loadedCustomComponents.find(componentName) != loadedCustomComponents.end())
						{
							HINSTANCE hDll = (HINSTANCE)loadedCustomComponents[componentName];
							reinterpret_cast<void(*)(SingletonManager*)>(GetProcAddress(hDll, "_set_singleton_manager"))(SingletonManager::Instance());
							reinterpret_cast<void(*)(GameObject*, const Json&)>(GetProcAddress(hDll, "_add_comp"))(obj, comp);
						}
						else
						{
							std::cout << "Unable to load component " << componentName << "." << std::endl;
						}
					}
				}
			}
	}
	catch (Json::parse_error)
	{
		std::cout << "Error with .project or .config file, the project cannot be loaded." << std::endl;
		delete currentProject;
		currentProject = nullptr;

		glfwSetWindowTitle(InitializationHandler->GetWindow(), "Current project: none");
		Project::projectState = Project::ProjectState::None;
		return;
	}

	Json config;
	for (auto& it : currentProject->customComponents)
	{
		Json comp;
		comp["name"] = it.first;
		comp["date"] = std::to_string(std::filesystem::last_write_time(currentProject->projectName + R"(\)" + it.first + ".cpp").time_since_epoch().count());
		config["Components"].push_back(comp);
	}
	std::ofstream(currentProject->projectPath + "/" + currentProject->projectName + ".config") << config;
	projectFile.close();

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

#ifdef __gl_h_
#undef __gl_h_
#endif // __gl_h_
#include <glad/glad.h>

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
	SaveProject();
	Project::projectState = Project::ProjectState::Opened;
}
void EngineEditor::CreateComponent(std::string componentName)
{
	if (!currentProject)
		return;

	std::ofstream componentFile(currentProject->projectPath + "/" + componentName + ".cpp");
	componentFile << R"(
#include <Engine.h>

#ifdef __gl_h_
#undef __gl_h_
#endif // __gl_h_
#include <glad/glad.h>

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

	UpdateComponents();
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
			//C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat
			cmd << R"(call ")" << visualStudioCompilerPath << R"(" x64 )"
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
			if (!system(cmd.str().c_str()))
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

	std::ofstream projectConfigFile(currentProject->projectPath + "/" + currentProject->projectName + ".config");
	Json configFile;
	for (auto it = currentProject->customComponents.begin(); it != currentProject->customComponents.end(); it++)
	{
		Json comp;
		comp["name"] = it->first;
		comp["date"] = std::to_string(std::filesystem::last_write_time(currentProject->projectName + R"(\)" + it->first + ".cpp").time_since_epoch().count());
		configFile["Components"].push_back(comp);
	}
	projectConfigFile << configFile;
}
void EngineEditor::BuildProject()
{
	if (!currentProject)
		return;

	SaveProject();
	system(("CMake\\bin\\cmake --build \"" + currentProject->projectPath + "/build\"").c_str());

	std::filesystem::path assimpDll = "assimp-vc143-mtd.dll";
	std::filesystem::path freetypeDll = "freetype.dll";

	std::filesystem::path projectFile = currentProject->projectName + "/" + currentProject->projectName + ".project";
	std::filesystem::path projectConfigFile = currentProject->projectName + "/" + currentProject->projectName + ".config";

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
		std::filesystem::copy_file(projectFile, targetParent / sourceDirectory / "MainScene.scene", std::filesystem::copy_options::overwrite_existing);
		std::filesystem::copy_file(projectConfigFile, targetParent / sourceDirectory / "components.config", std::filesystem::copy_options::overwrite_existing);
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
	Json saveFile;
	for (auto it = ObjectsManager->instantiatedObjects.begin(); it != ObjectsManager->instantiatedObjects.end(); it++)
		if ((*it)->shouldBeSerialized)
			saveFile["Scene"].push_back((*it)->Serialize());
	projectFile << saveFile;

	std::ofstream projectConfigFile(currentProject->projectPath + "/" + currentProject->projectName + ".config");
	Json configFile;
	for (auto& it : currentProject->customComponents)
	{
		Json comp;
		comp["name"] = it.first;
		comp["date"] = std::to_string(std::filesystem::last_write_time(currentProject->projectName + R"(\)" + it.first + ".cpp").time_since_epoch().count());
		configFile["Components"].push_back(comp);
	}
	// save shaders and models with paths
	/*for (auto& shader : ResourceManager->loadedShaders)
	{

	}*/
	projectConfigFile << configFile;
}

void EngineEditor::SetEditorCallbacks()
{
	GLFWwindow* window = InitializationHandler->GetWindow();
	glfwSetWindowFocusCallback(window,
		[](GLFWwindow* window, int focused)
		{
			if (focused && !simulationIsPlaying)
				UpdateComponents();
		});
	EventSystem->onUpdateEditor.push_back(
		[]()
		{
			if (EditorCamera::camera && !simulationIsPlaying)
				EditorCamera::OnUpdate();
		});
	EventSystem->onMouseMoveEventEditor.push_back(
		[](GLFWwindow* window, double x, double y, Vector2 motion)
		{
			if (EditorCamera::camera && !simulationIsPlaying)
				EditorCamera::OnMouseMove(x, y);
		});
	EventSystem->onMouseWheelEventEditor.push_back(
		[](GLFWwindow* window, double xoffset, double yoffset)
		{
			if (EditorCamera::camera && !simulationIsPlaying)
				EditorCamera::OnMouseWheel(xoffset, yoffset);
		});
}
void EngineEditor::SetCompilerPath()
{
	// open or create configuration file
	std::fstream configurationFile;
	configurationFile.open(std::filesystem::current_path().string() + "\\engine.config", std::fstream::in | std::fstream::out);
	if (!configurationFile)
		configurationFile.open(std::filesystem::current_path().string() + "\\engine.config", std::fstream::in | std::fstream::out | std::fstream::trunc);

	std::stringstream buffer;
	buffer << configurationFile.rdbuf();

	Json config;
	try
	{
		config = Json::parse(buffer.str());
		if (config.contains("compiler"))
			visualStudioCompilerPath = config["compiler"];
		if (config.contains("version"))
			visualStudioVersion = config["version"];
	}
	catch (Json::parse_error)
	{
		char* compilerPath;
		if (NFD_OpenDialog("bat", "C:\\Program Files", &compilerPath) == nfdresult_t::NFD_OKAY)
		{
			config["compiler"] = visualStudioCompilerPath = compilerPath;
			configurationFile << config;
		}
		else
		{
			visualStudioCompilerPath = std::string();
			std::cout << "Visual Studio compiler is not set." << std::endl;
		}
		visualStudioVersion = "Visual Studio 17 2022";
	}
	configurationFile.close();
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

		size_t attributeStart, attributeEnd;
		if ((attributeStart = ss.str().find_first_of('[')) != std::string::npos &&
			(attributeEnd	= ss.str().find_first_of(']')) != std::string::npos)
		{
			std::string attribute = ss.str().substr(attributeStart + 1, attributeEnd - attributeStart - 1);
			if (attribute != "hide")
			{
				ImGui::Text(ss.str().substr(attributeEnd + 1, 20).c_str()); ImGui::SameLine();
				ImGui::InputInt(label.c_str(), &std::get<2>(fields[fieldIndex]).i);
			}
		}
		else
		{
			ImGui::Text(ss.str().substr(0, 20).c_str()); ImGui::SameLine();
			ImGui::InputInt(label.c_str(), &std::get<2>(fields[fieldIndex]).i);
		}
		break;
	}
	case Json::value_t::number_float:
	{
		if (!shouldBeDrawn) break;

		size_t attributeStart, attributeEnd;
		if ((attributeStart = ss.str().find_first_of('[')) != std::string::npos &&
			(attributeEnd	= ss.str().find_first_of(']')) != std::string::npos)
		{
			std::string attribute = ss.str().substr(attributeStart + 1, attributeEnd - attributeStart - 1);
			if (attribute != "hide")
			{
				ImGui::Text(ss.str().substr(attributeEnd + 1, 20).c_str()); ImGui::SameLine();
				ImGui::InputFloat(label.c_str(), &std::get<2>(fields[fieldIndex]).f, 0.f, 0.f, "%.8f");
			}
		}
		else
		{
			ImGui::Text(ss.str().substr(0, 20).c_str()); ImGui::SameLine();
			ImGui::InputFloat(label.c_str(), &std::get<2>(fields[fieldIndex]).f, 0.f, 0.f, "%.8f");
		}
		break;
	}
	case Json::value_t::boolean:
	{
		if (!shouldBeDrawn) break;

		size_t attributeStart, attributeEnd;
		if ((attributeStart = ss.str().find_first_of('[')) != std::string::npos &&
			(attributeEnd	= ss.str().find_first_of(']')) != std::string::npos)
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
			std::string attribute = ss.str().substr(attributeStart + 1, attributeEnd - attributeStart - 1);
			if (attribute == "shader")
			{
				ImGui::Text(ss.str().substr(attributeEnd + 1, 20).c_str()); ImGui::SameLine();
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
				ImGui::Text(ss.str().substr(attributeEnd + 1, 20).c_str()); ImGui::SameLine();
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
			else if (attribute != "hide")
			{
				ImGui::Text(ss.str().substr(attributeEnd + 1, 20).c_str()); ImGui::SameLine();
				ImGui::InputText(label.c_str(), std::get<2>(fields[fieldIndex]).s, 50);
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

		size_t attributeStart, attributeEnd;
		std::string attribute;
		if ((attributeStart = ss.str().find_first_of('[')) != std::string::npos &&
			(attributeEnd = ss.str().find_first_of(']')) != std::string::npos)
			attribute = ss.str().substr(attributeStart + 1, attributeEnd - attributeStart - 1);
		if (shouldBeDrawn && attribute != "hide" && ImGui::CollapsingHeader(header.c_str()))
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

		size_t attributeStart, attributeEnd;
		std::string attribute;
		if ((attributeStart = ss.str().find_first_of('[')) != std::string::npos &&
			(attributeEnd = ss.str().find_first_of(']')) != std::string::npos)
			attribute = ss.str().substr(attributeStart + 1, attributeEnd - attributeStart - 1);
		if (shouldBeDrawn && attribute != "hide" && ImGui::CollapsingHeader(header.c_str()))
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
	if (!currentProject)
		return;

	// creating camera
	EditorCamera::camera = ObjectsManager->Instantiate<CameraComponent>(Vector3(-2, 1, -2));
	EditorCamera::cameraTransform = EditorCamera::camera->gameObject->transform;
	EditorCamera::camera->gameObject->name = "Editor Camera";
	EditorCamera::OnCreate();
	EventsController->SetAsMainCamera(EditorCamera::camera);
	EngineEditor::EnableObjectSerialization(EditorCamera::camera->gameObject, false);

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
	editorGridRenderer = ObjectsManager->Instantiate<ModelRendererComponent>();
	editorGridRenderer->SetModel(new Model({ new Mesh(vertices, triangles, textures) }));
	editorGridRenderer->SetShader(ResourceManager->GetShader("Grid"));
	editorGridRenderer->gameObject->name = "Grid Renderer";
	editorGridRenderer->renderQueueIndex = -1;
	EngineEditor::EnableObjectSerialization(editorGridRenderer->gameObject, false);

	// creating axes
	vertices =
	{
		Vector3(0), Vector3(0),
		Vector3(0), Vector3(0),
		Vector3(0), Vector3(0),
	};
	triangles =
	{
		0, 1, 
		2, 3, 
		4, 5,
	};
	textures = {};
	editorAxesRenderer = ObjectsManager->Instantiate<ModelRendererComponent>();
	Mesh* axesMesh = new Mesh(vertices, triangles, textures);
	axesMesh->wireframeWidth = 3;
	axesMesh->SetDrawMode(DrawMode::Wireframe);
	editorAxesRenderer->SetModel(new Model({ axesMesh }));
	editorAxesRenderer->SetShader(ResourceManager->GetShader("Axis"));
	editorAxesRenderer->gameObject->name = "Axis";
	editorAxesRenderer->renderQueueIndex = 1;
	EngineEditor::EnableObjectSerialization(editorAxesRenderer->gameObject, false);
	editorAxesRenderer->gameObject->isActive = false;
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

void EngineEditor::BeginPlaySimulation()
{
	if (!currentProject || simulationIsPlaying)
		return;

	playSimulationSavedState.clear();
	for (auto it = ObjectsManager->instantiatedObjects.begin(); it != ObjectsManager->instantiatedObjects.end(); it++)
		if ((*it)->shouldBeSerialized)
			playSimulationSavedState["Scene"].push_back((*it)->Serialize());

	EventsController->SetPlayMode(simulationIsPlaying = true);

	editorGridRenderer->gameObject->isActive = false;
	editorAxesRenderer->gameObject->isActive = false;
	EditorCamera::camera->gameObject->isActive = false;

	Screen->SetCursorVisibility(true);
	EventsController->SetAsMainCamera(ObjectsManager->FindObjectOfType<CameraComponent>());
	for (auto it = ObjectsManager->instantiatedObjects.begin(); it != ObjectsManager->instantiatedObjects.end(); it++)
		if ((*it)->isActive)
			for (auto& comp : (*it)->components)
				comp.second->OnCreate();
}
void EngineEditor::EndPlaySimulation()
{
	if (!currentProject || !simulationIsPlaying)
		return;

	EventsController->SetPlayMode(simulationIsPlaying = false);

	{
		auto it = ObjectsManager->instantiatedObjects.begin();
		while (it != ObjectsManager->instantiatedObjects.end())
		{
			GameObject* go = *it;
			// do not destroy editor camera and grid renderer etc
			if ((*it)->shouldBeSerialized)
			{
				// copy-paste of ObjectsManager::DestroyObject function
				delete* it;
				it = ObjectsManager->instantiatedObjects.erase(it);
			}
			else ++it;
		}
	}
	/*while (!ObjectsManager->instantiatedObjects.empty())
		ObjectsManager->DestroyObject(ObjectsManager->instantiatedObjects.front());*/

	for (auto& comp : loadedCustomComponents)
		FreeLibrary((HMODULE)comp.second);
	loadedCustomComponents.clear();

	{
		auto it = ObjectsManager->renderQueue.begin();
		while (it != ObjectsManager->renderQueue.end())
			if ((*it)->gameObject->shouldBeSerialized)
				it = ObjectsManager->renderQueue.erase(it);
			else ++it;
	}
	ObjectsManager->nextObjectID = 0;
	ObjectsManager->nextComponentID = 0;
	selectedGameObject = nullptr;

	Json scene = playSimulationSavedState["Scene"];
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

	editorGridRenderer->gameObject->isActive = true;
	editorAxesRenderer->gameObject->isActive = true;
	EditorCamera::camera->gameObject->isActive = true;
	EventsController->SetAsMainCamera(EditorCamera::camera);
}
void EngineEditor::PlaySimulation(bool play)
{
	play ? BeginPlaySimulation() : EndPlaySimulation();
}
bool EngineEditor::SimulationIsPlaying()
{
	return simulationIsPlaying;
}

void EngineEditor::HandleEditorShortcuts()
{
	if (EventSystem->GetKey(GLFW_KEY_LEFT_CONTROL))
	{
		if (EventSystem->GetKeyClick(GLFW_KEY_S))
			EngineEditor::SaveProject();
	}

	if (EventSystem->GetKeyClick(GLFW_KEY_ENTER))
	{
		EngineEditor::PlaySimulation(!EngineEditor::SimulationIsPlaying());
	}

	if (EventSystem->GetKeyClick(GLFW_KEY_ESCAPE))
	{
		// close all modal windows
		if (createComponent || changeObjectName || uploadResource)
			createComponent = changeObjectName = uploadResource = false;
		// if all windows were already closed then deselect current game object
		else
			selectedGameObject = nullptr;
	}
}
void EngineEditor::CheckTextInputActive()
{
	EventsController->enableMouseWheelEvent = 
		EventsController->enableKeyboardEvent = 
		!ImGui::GetIO().WantCaptureKeyboard;
}
void EngineEditor::DrawEditorOverlay()
{
	if (!currentProject || simulationIsPlaying)
		return;
	editorAxesRenderer->Render();
}