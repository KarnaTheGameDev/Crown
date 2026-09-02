#include <Crown.h>

class Sandbox : public Crown::Application
{
public:
	Sandbox()
	{
		// Scene content lives here, not in the engine. A handful of sprites so
		// there is something to click, drag and save.
		for (int i = 0; i < 8; i++)
		{
			Crown::Entity& block = CreateEntity("Block " + std::to_string(i));
			float angle = i * 6.2831853f / 8.0f;
			block.Position = { std::cos(angle) * 0.9f, std::sin(angle) * 0.55f, 0.0f };
			block.Scale = { 0.16f, 0.16f };
			block.AtlasCell = i;
		}

		Crown::Entity& player = CreateEntity("Player");
		player.Position = { 0.0f, 0.0f, 0.0f };
		player.Scale = { 0.22f, 0.22f };
		player.AtlasCell = 3;
		player.Tint = { 1.0f, 0.85f, 0.35f, 1.0f };

		// Keep the id, not the index. Deleting anything above the player would
		// shift every index below it.
		m_PlayerID = player.ID;

		CROWN_INFO("Arrow keys move the player.");
	}

	void OnUpdate(float deltaTime) override
	{
		// Looked up each frame: the player can be deleted from the Hierarchy,
		// and the vector reallocates whenever an entity is added.
		Crown::Entity* player = FindEntity(m_PlayerID);
		if (!player)
			return;

		glm::vec2 direction{ 0.0f, 0.0f };
		if (Crown::Input::IsKeyPressed(Crown::Key::Left))  direction.x -= 1.0f;
		if (Crown::Input::IsKeyPressed(Crown::Key::Right)) direction.x += 1.0f;
		if (Crown::Input::IsKeyPressed(Crown::Key::Down))  direction.y -= 1.0f;
		if (Crown::Input::IsKeyPressed(Crown::Key::Up))    direction.y += 1.0f;

		if (direction != glm::vec2(0.0f))
		{
			// Normalise, or moving diagonally is faster than moving straight.
			player->Position += glm::vec3(glm::normalize(direction) * m_Speed * deltaTime, 0.0f);
			player->Rotation += 90.0f * deltaTime;
		}
	}

private:
	uint32_t m_PlayerID = 0;
	float m_Speed = 1.2f;      // world units per second
};

Crown::Application* Crown::CreateApplication() {
	return new Sandbox();
}
