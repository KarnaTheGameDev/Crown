#include <Crown.h>

class Sandbox : public Crown::Application
{
public:
	Sandbox()
	{
		// Application's constructor has already seeded the background grid, so
		// the player goes on the end and is drawn last, on top of it.
		Crown::Entity player;
		player.Name = "Player";
		player.Position = { 0.0f, 0.0f, 0.0f };
		player.Scale = { 0.22f, 0.22f };
		player.AtlasCell = 3;
		player.Tint = { 1.0f, 0.85f, 0.35f, 1.0f };

		GetEntities().push_back(player);
		m_Player = (int)GetEntities().size() - 1;

		CROWN_INFO("Arrow keys move the player.");
	}

	void OnUpdate(float deltaTime) override
	{
		auto& entities = GetEntities();
		// The player can be deleted from the Hierarchy panel like anything else.
		if (m_Player < 0 || m_Player >= (int)entities.size())
			return;

		Crown::Entity& player = entities[m_Player];

		glm::vec2 direction{ 0.0f, 0.0f };
		if (Crown::Input::IsKeyPressed(Crown::Key::Left))  direction.x -= 1.0f;
		if (Crown::Input::IsKeyPressed(Crown::Key::Right)) direction.x += 1.0f;
		if (Crown::Input::IsKeyPressed(Crown::Key::Down))  direction.y -= 1.0f;
		if (Crown::Input::IsKeyPressed(Crown::Key::Up))    direction.y += 1.0f;

		if (direction != glm::vec2(0.0f))
		{
			// Normalise, or moving diagonally is faster than moving straight.
			player.Position += glm::vec3(glm::normalize(direction) * m_Speed * deltaTime, 0.0f);
			player.Rotation += 90.0f * deltaTime;
		}
	}

private:
	int m_Player = -1;
	float m_Speed = 1.2f;      // world units per second
};

Crown::Application* Crown::CreateApplication() {
	return new Sandbox();
}
