#pragma once

#include <glm/glm.hpp>

namespace Crown {

	class OrthographicCamera
	{
	public:
		OrthographicCamera(float left, float right, float bottom, float top);

		void SetProjection(float left, float right, float bottom, float top);

		const glm::vec3& GetPosition() const { return m_Position; }
		void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateView(); }

		float GetRotation() const { return m_Rotation; }
		void SetRotation(float degrees) { m_Rotation = degrees; RecalculateView(); }

		const glm::mat4& GetViewProjection() const { return m_ViewProjection; }

	private:
		void RecalculateView();

		glm::mat4 m_Projection;
		glm::mat4 m_View;
		glm::mat4 m_ViewProjection;

		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
		float m_Rotation = 0.0f;
	};

}
