#pragma once
#include "EngineEditor.h"
#include <GLFW\glfw3.h>
#include <Vectors.h>

class CameraComponent;
class TransformComponent;
struct EditorCamera final
{
	static CameraComponent* camera;
	static TransformComponent* cameraTransform;

	static Vector2 movementDirection;
	static Vector3 sightDirection;
	static float rotationSpeed;
	static float movementSpeed;
	static float zoomIntensity;

	static float yaw, pitch;

	static void OnCreate();
	static void OnUpdate();
	static void OnMouseMove(double x, double y);
	static void OnMouseWheel(double xoffset, double yoffset);
};
