#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
using Json = nlohmann::json;

class GameObject;
class Component;
class EngineEditor
{
	struct Project final
	{
		enum class ProjectState
		{
			None,
			Create,
			Open,
			Opened
		};

		static ProjectState projectState;
		std::string projectName;
		std::string projectPath;
		std::vector<std::string> customComponents;
	};
	static std::string visualStudioVersion;
	static Project* currentProject;
	static GameObject* selectedGameObject;

	static void DrawMenuBar();
	static void DrawCreateProjectMenu();
	static void DrawOpenProjectMenu();
	static void DrawOpenedProjectMenu();
	static void DrawLoadedAssetsMenu();

	static void DrawComponentField(std::pair<const std::string, Json> js);
public:
	static void DrawEngineMenu();

	static void OpenProject(std::string projectName);
	static void CreateProject(std::string projectName);
	static void CreateComponent(std::string componentName);
	static void BuildProject(bool buildAndOpen);
};