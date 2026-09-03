#include <Crown.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <vector>

// Asteroids, written entirely against Crown's client API. The engine knows
// nothing about ships, bullets or scores.
class Sandbox : public Crown::Application
{
public:
	Sandbox()
		: m_Random(std::random_device{}())
	{
		StartGame();
		CROWN_INFO("Left/Right turn, Up thrusts, Space fires, R restarts.");
	}

	void OnUpdate(float dt) override
	{
		if (dt > 0.1f)          // a debugger pause should not teleport everything
			dt = 0.1f;

		if (m_GameOver)
		{
			if (Crown::Input::IsKeyPressed(Crown::Key::R))
				StartGame();
			return;
		}

		UpdateShip(dt);
		Integrate(dt);
		ResolveCollisions();
		ExpireBullets(dt);

		if (m_Asteroids.empty())
			SpawnWave(++m_Wave);
	}

	void OnImGuiRender() override
	{
		ImGui::SetNextWindowPos(ImVec2(1012.0f, 250.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(250.0f, 150.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Asteroids");
		ImGui::Text("Score %d", m_Score);
		ImGui::Text("Lives %d", m_Lives);
		ImGui::Text("Wave  %d", m_Wave);
		ImGui::Separator();
		if (m_GameOver)
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "GAME OVER - press R");
		else if (m_RespawnIn > 0.0f)
			ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "Respawning...");
		else if (m_Invulnerable > 0.0f)
			ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.0f, 1.0f), "Shielded %.1fs", m_Invulnerable);
		else
			ImGui::TextDisabled("Left/Right turn, Up thrust");
		ImGui::End();
	}

private:
	// --- world -------------------------------------------------------------

	static constexpr float s_HalfWidth  = 1.5f;
	static constexpr float s_HalfHeight = 0.85f;

	// Radius used for gameplay collision. Circles rather than boxes: every
	// asteroid spins, and a circle is rotation-invariant where a box is not.
	static float RadiusOf(const Crown::Entity& e)
	{
		return 0.75f * std::max(e.Scale.x, e.Scale.y);
	}

	void Wrap(Crown::Entity& e) const
	{
		if (e.Position.x >  s_HalfWidth)  e.Position.x = -s_HalfWidth;
		if (e.Position.x < -s_HalfWidth)  e.Position.x =  s_HalfWidth;
		if (e.Position.y >  s_HalfHeight) e.Position.y = -s_HalfHeight;
		if (e.Position.y < -s_HalfHeight) e.Position.y =  s_HalfHeight;
	}

	float RandomFloat(float lo, float hi)
	{
		return std::uniform_real_distribution<float>(lo, hi)(m_Random);
	}

	// --- lifetime ----------------------------------------------------------

	// Destroy an entity and drop every piece of state keyed to it, so the maps
	// cannot outlive the scene.
	void Kill(uint32_t id)
	{
		DestroyEntity(id);
		m_Velocity.erase(id);
		m_Spin.erase(id);
		m_BulletLife.erase(id);
		m_Tier.erase(id);
		m_Asteroids.erase(std::remove(m_Asteroids.begin(), m_Asteroids.end(), id), m_Asteroids.end());
		m_Bullets.erase(std::remove(m_Bullets.begin(), m_Bullets.end(), id), m_Bullets.end());
	}

	void StartGame()
	{
		for (uint32_t id : std::vector<uint32_t>(m_Asteroids)) Kill(id);
		for (uint32_t id : std::vector<uint32_t>(m_Bullets))   Kill(id);
		if (m_ShipID)
			Kill(m_ShipID);

		m_Score = 0;
		m_Lives = 3;
		m_Wave = 0;
		m_GameOver = false;
		m_RespawnIn = 0.0f;
		m_FireCooldown = 0.0f;

		SpawnShip();
		SpawnWave(++m_Wave);
	}

	void SpawnShip()
	{
		Crown::Entity& ship = CreateEntity("Ship");
		ship.Position = { 0.0f, 0.0f, 0.0f };
		ship.Scale = { 0.11f, 0.11f };
		ship.AtlasCell = 2;                       // a triangle in the atlas
		ship.Tint = { 0.85f, 0.95f, 1.0f, 1.0f };
		m_ShipID = ship.ID;
		m_ShipVelocity = { 0.0f, 0.0f };
		// Without this you can respawn straight into the asteroid that just
		// killed you, and lose every life in about a second.
		m_Invulnerable = 2.0f;
	}

	void SpawnWave(int wave)
	{
		int count = 3 + wave;
		for (int i = 0; i < count; i++)
		{
			// Around the edge, so nothing lands on top of the ship at spawn.
			float angle = RandomFloat(0.0f, 6.2831853f);
			glm::vec2 at{ std::cos(angle) * s_HalfWidth, std::sin(angle) * s_HalfHeight };
			SpawnAsteroid(at, 3);
		}
		CROWN_INFO("Wave {0}: {1} asteroids", wave, count);
	}

	void SpawnAsteroid(glm::vec2 at, int tier)
	{
		static const float scaleFor[4] = { 0.0f, 0.085f, 0.13f, 0.20f };

		Crown::Entity& rock = CreateEntity("Asteroid");
		rock.Position = { at.x, at.y, 0.0f };
		rock.Scale = { scaleFor[tier], scaleFor[tier] };
		rock.AtlasCell = (tier == 3) ? 1 : (tier == 2 ? 5 : 9);
		rock.Tint = { 0.75f, 0.78f, 0.85f, 1.0f };
		rock.Rotation = RandomFloat(0.0f, 360.0f);

		uint32_t id = rock.ID;                    // rock dangles after the next create
		float speed = RandomFloat(0.18f, 0.32f) * (1.0f + 0.35f * tier);
		float heading = RandomFloat(0.0f, 6.2831853f);
		m_Velocity[id] = { std::cos(heading) * speed, std::sin(heading) * speed };
		m_Spin[id] = RandomFloat(-70.0f, 70.0f);
		m_Tier[id] = tier;
		m_Asteroids.push_back(id);
	}

	// --- per frame ---------------------------------------------------------

	void UpdateShip(float dt)
	{
		m_FireCooldown -= dt;
		m_Invulnerable = std::max(0.0f, m_Invulnerable - dt);

		if (m_RespawnIn > 0.0f)
		{
			m_RespawnIn -= dt;
			if (m_RespawnIn <= 0.0f)
				SpawnShip();
			return;
		}

		Crown::Entity* ship = FindEntity(m_ShipID);
		if (!ship)
			return;

		if (Crown::Input::IsKeyPressed(Crown::Key::Left))  ship->Rotation += 200.0f * dt;
		if (Crown::Input::IsKeyPressed(Crown::Key::Right)) ship->Rotation -= 200.0f * dt;

		// The atlas triangle points up, so facing is rotation + 90 degrees.
		float facing = glm::radians(ship->Rotation + 90.0f);
		glm::vec2 forward{ std::cos(facing), std::sin(facing) };

		if (Crown::Input::IsKeyPressed(Crown::Key::Up))
			m_ShipVelocity += forward * 1.8f * dt;

		float blink = (m_Invulnerable > 0.0f && std::fmod(m_Invulnerable, 0.24f) < 0.12f) ? 0.35f : 1.0f;
		ship->Tint = { 0.85f, 0.95f, 1.0f, blink };

		m_ShipVelocity *= (1.0f - 0.5f * dt);         // drag, so it does not run away
		ship->Position += glm::vec3(m_ShipVelocity * dt, 0.0f);
		Wrap(*ship);

		bool firing = Crown::Input::IsKeyPressed(Crown::Key::Space);
		glm::vec2 muzzle{ ship->Position.x + forward.x * 0.09f,
		                  ship->Position.y + forward.y * 0.09f };

		// Everything the ship pointer was needed for is done: firing creates an
		// entity, which reallocates the vector and invalidates it.
		if (firing && m_FireCooldown <= 0.0f)
		{
			FireBullet(muzzle, forward);
			m_FireCooldown = 0.22f;
		}
	}

	void FireBullet(glm::vec2 at, glm::vec2 direction)
	{
		Crown::Entity& bullet = CreateEntity("Bullet");
		bullet.Position = { at.x, at.y, 0.0f };
		bullet.Scale = { 0.035f, 0.035f };
		bullet.AtlasCell = 0;                     // a circle in the atlas
		bullet.Tint = { 1.0f, 0.85f, 0.35f, 1.0f };

		uint32_t id = bullet.ID;
		m_Velocity[id] = direction * 2.2f + m_ShipVelocity;
		m_BulletLife[id] = 1.1f;
		m_Bullets.push_back(id);
	}

	void Integrate(float dt)
	{
		// Iterating ids rather than entities: nothing here creates or destroys,
		// but keeping the habit means adding a spawn later cannot corrupt this.
		for (const auto& entry : m_Velocity)
		{
			Crown::Entity* e = FindEntity(entry.first);
			if (!e)
				continue;

			e->Position += glm::vec3(entry.second * dt, 0.0f);

			auto spin = m_Spin.find(entry.first);
			if (spin != m_Spin.end())
				e->Rotation += spin->second * dt;

			Wrap(*e);
		}
	}

	void ExpireBullets(float dt)
	{
		std::vector<uint32_t> dead;
		for (auto& entry : m_BulletLife)
		{
			entry.second -= dt;
			if (entry.second <= 0.0f)
				dead.push_back(entry.first);
		}
		for (uint32_t id : dead)
			Kill(id);
	}

	void ResolveCollisions()
	{
		// Collect first, act second. Killing or spawning inside the scan would
		// invalidate the entity pointers the scan is reading.
		struct Hit { uint32_t Bullet, Asteroid; };
		std::vector<Hit> hits;
		bool shipHit = false;

		Crown::Entity* ship = (m_RespawnIn > 0.0f || m_Invulnerable > 0.0f)
			? nullptr : FindEntity(m_ShipID);

		for (uint32_t rockId : m_Asteroids)
		{
			Crown::Entity* rock = FindEntity(rockId);
			if (!rock)
				continue;

			for (uint32_t bulletId : m_Bullets)
			{
				Crown::Entity* bullet = FindEntity(bulletId);
				if (bullet && Touching(*rock, *bullet))
				{
					hits.push_back({ bulletId, rockId });
					break;                        // one bullet per rock per frame
				}
			}

			if (ship && !shipHit && Touching(*rock, *ship))
				shipHit = true;
		}

		for (const Hit& hit : hits)
			SplitAsteroid(hit.Asteroid, hit.Bullet);

		if (shipHit)
			LoseLife();
	}

	static bool Touching(const Crown::Entity& a, const Crown::Entity& b)
	{
		glm::vec2 delta{ a.Position.x - b.Position.x, a.Position.y - b.Position.y };
		float reach = RadiusOf(a) + RadiusOf(b);
		return glm::dot(delta, delta) <= reach * reach;
	}

	void SplitAsteroid(uint32_t rockId, uint32_t bulletId)
	{
		auto tier = m_Tier.find(rockId);
		if (tier == m_Tier.end())
			return;                               // already destroyed this frame

		int currentTier = tier->second;
		Crown::Entity* rock = FindEntity(rockId);
		glm::vec2 at = rock ? glm::vec2(rock->Position.x, rock->Position.y) : glm::vec2(0.0f);

		static const int scoreFor[4] = { 0, 100, 50, 20 };
		m_Score += scoreFor[currentTier];

		Kill(bulletId);
		Kill(rockId);

		if (currentTier > 1)
		{
			for (int i = 0; i < 2; i++)
			{
				glm::vec2 offset{ RandomFloat(-0.04f, 0.04f), RandomFloat(-0.04f, 0.04f) };
				SpawnAsteroid(at + offset, currentTier - 1);
			}
		}
	}

	void LoseLife()
	{
		Kill(m_ShipID);
		m_ShipID = 0;
		m_Lives--;

		if (m_Lives <= 0)
		{
			m_GameOver = true;
			CROWN_INFO("Game over. Score {0}, reached wave {1}.", m_Score, m_Wave);
			return;
		}

		m_RespawnIn = 1.5f;
		CROWN_INFO("Hit. {0} lives left.", m_Lives);
	}

	// --- state -------------------------------------------------------------

	// Entity has no room for gameplay data, so it lives here keyed by id.
	std::unordered_map<uint32_t, glm::vec2> m_Velocity;
	std::unordered_map<uint32_t, float>     m_Spin;
	std::unordered_map<uint32_t, float>     m_BulletLife;
	std::unordered_map<uint32_t, int>       m_Tier;

	std::vector<uint32_t> m_Asteroids;
	std::vector<uint32_t> m_Bullets;

	uint32_t  m_ShipID = 0;
	glm::vec2 m_ShipVelocity{ 0.0f, 0.0f };

	int   m_Score = 0;
	int   m_Lives = 3;
	int   m_Wave = 0;
	bool  m_GameOver = false;
	float m_RespawnIn = 0.0f;
	float m_FireCooldown = 0.0f;
	float m_Invulnerable = 0.0f;

	std::mt19937 m_Random;
};

Crown::Application* Crown::CreateApplication() {
	return new Sandbox();
}
