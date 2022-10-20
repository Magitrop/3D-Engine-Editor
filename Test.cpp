#ifdef __gl_h_
#undef __gl_h_
#endif // __gl_h_
#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <nfd.h>
#include <stdio.h>
#include <stdlib.h>

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
	//ResourceManager->UploadShader(Shader("source\\TextVertexShader.vertexshader", "source\\TextFragmentShader.fragmentshader"), "Text");
	ResourceManager->UploadShader(Shader("source\\simple-vert.glsl", "source\\simple-frag.glsl"), "Simple");
	ResourceManager->UploadShader(Shader("source\\depth-vert.glsl", "source\\depth-frag.glsl"), "Depth");
	ResourceManager->UploadShader(Shader("source\\shadows-vert.glsl", "source\\shadows-frag.glsl"), "Shadows");
	ResourceManager->UploadShader(Shader("source\\grid-vert.glsl", "source\\grid-frag.glsl"), "Grid");
	ResourceManager->UploadShader(Shader("source\\axis-vert.glsl", "source\\axis-frag.glsl"), "Axis");
}
void LoadModels()
{
	ResourceManager->UploadModel(Model("source\\castle.fbx"), "Castle");
}

#include <windows.h>

int main()
{
	EngineEditor::SetCompilerPath();
	InitializationHandler->Init({ 1600, 800 });

	GLFWwindow* window = InitializationHandler->GetWindow();
	glfwSetWindowTitle(window, "Current project: none");

	LoadShaders();
	//LoadModels();

	Lightings->lightPosition = Vector3(-1, 0, 0);

	EngineEditor::SetEditorCallbacks();
	while (!glfwWindowShouldClose(window))
	{
		LightingController::PrepareDepthMap();

		glViewport(0, 0, Screen->GetWindowResolution().x, Screen->GetWindowResolution().y);
		glClear(GL_COLOR_BUFFER_BIT);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, LightingController::GetDepthMapID());
		ResourceManager->GetShader("Shadows").SetInt("shadowMap", 0);

		RenderingController::Render(true);
		glClear(GL_DEPTH_BUFFER_BIT);
		RenderingController::Render(false);

		EngineEditor::HandleEditorShortcuts();
		EngineEditor::CheckTextInputActive();

		EngineEditor::DrawEngineMenu();
		EventsController->Update();

		glfwPollEvents();
		glfwSwapBuffers(window);
	}

	return 0;
}
