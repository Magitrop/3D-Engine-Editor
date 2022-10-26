#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <GLFW/glfw3.h>

#include <nlohmann/json.hpp>
using Json = nlohmann::json;

class GameObject;
class Component;
class CameraComponent;
class ModelRendererComponent;
class Model;
class EngineEditor final
{
	EngineEditor() = delete;

	struct Project final
	{
		enum class ProjectState
		{
			None,
			Create,
			//Open,
			Opened
		} static projectState;

		std::string projectName;
		std::string projectPath;
		std::vector<std::pair<std::string, std::string>> customComponents; // name, last write time
	};
	struct FieldData final
	{
		int i = 0;
		float f = 0;
		bool b = false;
		char* s = new char[50];
	};
	static std::string visualStudioVersion;
	static std::string visualStudioCompilerPath;
	static Project* currentProject;
	static char* currentProjectPath;

	static GameObject* selectedGameObject;
	static std::vector<std::tuple<std::string, Json::value_t, FieldData>> fields;

	static bool uploadResource;
	static std::vector<std::string> uploadingResources;
	static std::mutex uploadingResourcesMutex;
	static std::vector<std::pair<Model*, GLFWwindow*>> pendingLoadedModels; // models whose meshes should be rebuilt
	static char* vertexShaderPath;
	static char* fragmentShaderPath;
	static char shaderName[50];
	static char* modelPath;
	static char modelName[50];

	static bool changeObjectName;
	static char newObjectName[50];

	static bool createComponent;
	static char newComponentName[100];
	static std::map<std::string, const void*> loadedCustomComponents;

	static int fieldIndex;
	static int buttonIndex;
	static Json serializedObject;

	static Json playSimulationSavedState;
	static bool simulationIsPlaying;

	static ModelRendererComponent* editorGridRenderer;
	static ModelRendererComponent* editorAxesRenderer;

	static void DrawMenuBar();
	static void DrawCreateProjectMenu();
	static void DrawOpenedProjectMenu();
	static void DrawLoadedAssetsMenu();

	static bool DrawUploadResourceMenu();
	static void DrawLoadingResourcesQueue();
	static void IteratePendingLoadedModels(); // check if there are pending models which meshes should be rebuilt

	static bool DrawChangeObjectNameMenu();
	static bool DrawCreateComponentMenu();

	static void GetComponentFields(Json& j);
	static void SetComponentFields(Json& parent);
	static bool DrawComponentField(std::pair<const std::string, Json>& j, Json& parent, bool shouldBeDrawn);

	static void ClearProject();
	static void ResetProject();
	static void CreateMainObjects();

	static void BeginPlaySimulation();
	static void EndPlaySimulation();
public:
	static void DrawEngineMenu();

	static void OpenProject();
	static void CreateProject(std::string projectName);
	static void CreateComponent(std::string componentName);
	static void UpdateComponents();
	static void SaveProject();
	static void BuildProject();

	static void SetEditorCallbacks();
	static void SetCompilerPath();
	static void EnableObjectSerialization(GameObject* obj, bool enable);

	static void PlaySimulation(bool play);
	static bool SimulationIsPlaying();

	static void HandleEditorShortcuts();
	static void CheckTextInputActive();
	static void DrawEditorOverlay();
};