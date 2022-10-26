#include "EngineCamera.h"

#include <EventSystem.h>
#include <GameObject.h>
#include <Screen.h>

#include <glm/gtx/projection.hpp>

CameraComponent* EditorCamera::camera;
TransformComponent* EditorCamera::cameraTransform;

Vector3 EditorCamera::movementDirection;
Vector3 EditorCamera::sightDirection = Vector3(0, 0, 1);

float EditorCamera::rotationSpeed = 0.03f;
float EditorCamera::movementSpeed = 0.05f;
float EditorCamera::zoomIntensity = 0.2f;

float EditorCamera::yaw = 45, EditorCamera::pitch = -20;

void EditorCamera::OnMouseMove(double x, double y)
{
	if (EventSystem->GetMouseButton(GLFW_MOUSE_BUTTON_RIGHT))
	{
		yaw -= EventSystem->GetMouseMotion().x * rotationSpeed;
		pitch += EventSystem->GetMouseMotion().y * rotationSpeed;

		if (pitch > 89.0f)
			pitch = 89.0f;
		if (pitch < -89.0f)
			pitch = -89.0f;

		sightDirection.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		sightDirection.y = sin(glm::radians(pitch));
		sightDirection.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		cameraTransform->SetForward(glm::normalize(sightDirection));

		camera->RecalculateViewMatrix();
		Screen->SetCursorVisibility(false);
	}
	else
		Screen->SetCursorVisibility(true);
}
void EditorCamera::OnMouseWheel(double xoffset, double yoffset)
{
	cameraTransform->Translate(cameraTransform->GetForward() * static_cast<float>(yoffset) * zoomIntensity);
	camera->RecalculateViewMatrix();
}
void EditorCamera::OnUpdate()
{
	if (EventSystem->GetKey(GLFW_KEY_LEFT_CONTROL))
		return;

	if (EventSystem->GetKey(GLFW_KEY_W))
		movementDirection.z = 1;
	else if (EventSystem->GetKey(GLFW_KEY_S))
		movementDirection.z = -1;
	else
		movementDirection.z = 0;

	if (EventSystem->GetKey(GLFW_KEY_Q))
		movementDirection.y = 1;
	else if (EventSystem->GetKey(GLFW_KEY_E))
		movementDirection.y = -1;
	else
		movementDirection.y = 0;

	if (EventSystem->GetKey(GLFW_KEY_D))
		movementDirection.x = -1;
	else if (EventSystem->GetKey(GLFW_KEY_A))
		movementDirection.x = 1;
	else
		movementDirection.x = 0;

	if (movementDirection.x || 
		movementDirection.y || 
		movementDirection.z)
	{
		movementDirection = glm::normalize(movementDirection);
		Vector3 movement =
			(movementDirection.x * cameraTransform->GetRight() +
				movementDirection.y * cameraTransform->GetUp() +
				movementDirection.z * cameraTransform->GetForward()) * movementSpeed * (EventSystem->GetKey(GLFW_KEY_LEFT_SHIFT) ? 3.f : 1.f);
		cameraTransform->Translate(movement);
		camera->RecalculateViewMatrix();
	}
}

void EditorCamera::OnCreate()
{
	sightDirection.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	sightDirection.y = sin(glm::radians(pitch));
	sightDirection.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraTransform->SetForward(glm::normalize(sightDirection));
	camera->RecalculateViewMatrix();
}
