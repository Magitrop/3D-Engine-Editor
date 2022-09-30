#ifdef __gl_h_
#undef __gl_h_
#endif // __gl_h_
#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "EngineEditor.h"
#include <Engine.h>
#include <Initializer.h>
#include <EventsController.h>
#include <LightingController.h>
#include <RenderingController.h>

#include <nlohmann/json.hpp>
using Json = nlohmann::json;

void LoadShaders()
{
	ResourceManager::UploadShader(Shader("source\\TextVertexShader.vertexshader", "source\\TextFragmentShader.fragmentshader"), "Text");
	ResourceManager::UploadShader(Shader("source\\SimpleVertexShader.vertexshader", "source\\SimpleFragmentShader.fragmentshader"), "Simple");
	//Shader::UploadShader(Shader("..\\source\\TextureVertShader.vertexshader", "..\\source\\TextureFragmentShader.fragmentshader"), "Map");
	ResourceManager::UploadShader(Shader("source\\DepthShader.vertexshader", "source\\DepthFragShader.fragmentshader"), "Depth");
	ResourceManager::UploadShader(Shader("source\\ShadowsVert.vertexshader", "source\\ShadowsFrag.fragmentshader"), "Shadows");
}
void LoadModels()
{
	ResourceManager::UploadModel(Model("source\\castle.fbx"), "Castle");
}

int main()
{
	InitializationHandler::Init({ 1600, 800 });
	auto camera = ObjectsManager::Instantiate<CameraComponent>(Vector3(0, 0, -3));
	camera->gameObject->name = "Main Camera";
	EventsController::SetAsMainCamera(camera);
	LoadShaders();

	GLFWwindow* window = const_cast<GLFWwindow*>(InitializationHandler::GetWindow());
	LineRendererComponent* l = ObjectsManager::Instantiate<LineRendererComponent>();

	while (!glfwWindowShouldClose(window))
	{
		LightingController::PrepareDepthMap();

		glViewport(0, 0, Screen::GetWindowResolution().x, Screen::GetWindowResolution().y);
		glClear(GL_COLOR_BUFFER_BIT);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, LightingController::GetDepthMapID());
		ResourceManager::GetShader("Shadows").SetInt("shadowMap", 0);

		RenderingController::Render(true);
		glClear(GL_DEPTH_BUFFER_BIT);
		RenderingController::Render(false);

		EngineEditor::DrawEngineMenu();

		EventsController::Update();
		glfwPollEvents();
		glfwSwapBuffers(window);
	}

	return 0;
}