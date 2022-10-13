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
	//ResourceManager::UploadShader(Shader("source\\TextVertexShader.vertexshader", "source\\TextFragmentShader.fragmentshader"), "Text");
	ResourceManager::UploadShader(Shader("source\\simple-vert.glsl", "source\\simple-frag.glsl"), "Simple");
	//Shader::UploadShader(Shader("..\\source\\TextureVertShader.vertexshader", "..\\source\\TextureFragmentShader.fragmentshader"), "Map");
	ResourceManager::UploadShader(Shader("source\\depth-vert.glsl", "source\\depth-frag.glsl"), "Depth");
	ResourceManager::UploadShader(Shader("source\\shadows-vert.glsl", "source\\shadows-frag.glsl"), "Shadows");
	ResourceManager::UploadShader(Shader("source\\grid-vert.glsl", "source\\grid-frag.glsl"), "Grid");
}
void LoadModels()
{
	ResourceManager::UploadModel(Model("source\\castle.fbx"), "Castle");
}

#include <windows.h>

int main()
{
	/*system(
		R"(call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 )"
		R"(&& cl /std:c++17 /EHsc /MDd "libraries\glad\src\glad.c" "A\ComponentsEntry.cpp" "A\MyComponent.cpp" /Fo".\A\\")"
		R"( /I"..\WindowLib")"
		R"( /I"libraries\glad\include")"
		R"( /I"libraries\glfw\include")"
		R"( /I"libraries\glm")"
		R"( /I"libraries\json\include")"
		R"( /I"libraries\assimp\include")"
		R"( /link opengl32.lib user32.lib gdi32.lib shell32.lib "..\WindowLib\x64\Debug\WindowLib.lib" "libraries\glad\Debug\glad.lib" "libraries\glfw\build\src\Debug\glfw3.lib" "libraries\assimp\lib\Debug\assimp.lib")"
		R"(&& link /DLL /out:"A\MyComponent.dll" "A\MyComponent.obj" "A\ComponentsEntry.obj")"
	);*/

	//system(R"(MinGW\bin\g++ -std=c++17 -g -fPIC ..\WindowLib\*.cpp -I"libraries\glfw\include" -L".\libraries\glfw\src\Debug\glfw3.lib" -I"libraries\glm" -I"libraries\json\include" -I"libraries\assimp\include" -L"libraries\assimp\lib\Debug" -lassimp -I"libraries\glad\include" -L"libraries\glad\Debug" -lglad -o A\window.o)");
	//system(R"(MinGW\bin\ar rcs A\libwindow.a A\window.o)");

	//system(R"(MinGW\bin\g++ -c A\null.cpp -I"..\WindowLib" -L"..\WindowLib\x64\Debug" -lwindow -I"..\ControllersLib" -L"..\ControllersLib\x64\Debug" -lcontrollers -I"libraries\glfw\include" -I"libraries\glm" -I"libraries\json\include" -I"libraries\assimp\include" -L"libraries\assimp\lib\Debug" -o A\test1.o)");
	//system(R"(MinGW\bin\g++ -shared A\test1.o -L"A" -ltesting -I"A" -o A\test.dll)");

	//system(R"(MinGW\bin\g++ -c A\null.cpp -I"..\WindowLib" -L"..\WindowLib\x64\Debug" -I"..\ControllersLib" -L"..\ControllersLib\x64\Debug" -I"libraries\glfw\include" -I"libraries\glm" -I"libraries\json\include" -I"libraries\assimp\include" -L"libraries\assimp\lib\Debug" -o A\test1.o)");
	//system(R"(MinGW\bin\g++ -shared -o A\test.dll A\test1.o A\test2.o)");
	//system(R"(MinGW\bin\g++ -c A\MyComponent.cpp -I"..\WindowLib" -L"..\WindowLib\x64\Debug\WindowLib.lib" -I"..\ControllersLib" -L"..\ControllersLib\x64\Debug\ControllersLib.lib"  -I"libraries\glfw\include" -I"libraries\glm" -I"libraries\json\include" -I"libraries\assimp\include" -L"libraries\assimp\lib\Debug" -o A\test2.o)");
	//system(R"(MinGW\bin\g++ -shared -o A\test.dll A\test2.o -I"..\WindowLib" -L"..\WindowLib\x64\Debug" -lWindowLib -I"..\ControllersLib" -L"..\ControllersLib\x64\Debug\ControllersLib.lib" -I"libraries\glfw\include" -I"libraries\glm" -I"libraries\json\include" -I"libraries\assimp\include" -L"libraries\assimp\lib\Debug")");

	/*GameObject* go = ObjectsManager::Instantiate();
	HINSTANCE hDll = LoadLibraryA(R"(A\test.dll)");
	if (hDll)
	{
		std::cout << reinterpret_cast<Component*(*)()>(GetProcAddress(hDll, "AddToObject"))()->GetComponentName() << std::endl;
	}
	else
	{
		std::cout << "LoadLibrary returned 0" << std::endl;
	}*/

	InitializationHandler::Init({ 1600, 800 });
	GLFWwindow* window = InitializationHandler::GetWindow();
	glfwSetWindowTitle(window, "Current project: none");
	LoadShaders();
	//LoadModels();

	Lightings::lightPosition = Vector3(-1, 0, 0);

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