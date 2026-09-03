#include <Crown.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

// Asteroids, written entirely against Crown's client API. The engine knows
// nothing about ships, bullets or scores.
class Sandbox : public Crown::Application
{
	// Everything the game needs to know about an entity, stored on the entity
	// itself through Entity::UserData. This used to be four hash maps keyed by
	// id, which meant four lookups per entity per frame and a cleanup step that
	// had to remember all four.
	enum class Kind { Ship, Bullet, Asteroid };

	struct Mover
	{
		Kind      What = Kind::Asteroid;
		glm::vec2 Velocity{ 0.0f, 0.0f };
		float     Spin = 0.0f;          // degrees per second
		float     Life = -1.0f;         // seconds remaining, negative for forever
		int       Tier = 0;             // asteroid size, 3 down to 1
	};

	static Mover* MoverOf(Crown::Entity& e)
	{
		return std::any_cast<Mover>(&e.UserData);
	}

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

		if (CountAsteroids() == 0)
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

	int CountAsteroids()
	{
		int count = 0;
		for (Crown::Entity& e : GetEntities())
			if (Mover* m = MoverOf(e); m && m->What == Kind::Asteroid)
				count++;
		return count;
	}

	// --- lifetime ----------------------------------------------------------

	void StartGame()
	{
		// Collect first: destroying shifts the vector being iterated.
		std::vector<uint32_t> everything;
		for (const Crown::Entity& e : GetEntities())
			everything.push_back(e.ID);
		for (uint32_t id : everything)
			DestroyEntity(id);

		m_Score = 0;
		m_Lives = 3;
		m_Wave = 0;
		m_GameOver = false;
		m_RespawnIn = 0.0f;
		m_FireCooldown = 0.0f;
		m_Invulnerable = 0.0f;

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
		ship.UserData = Mover{ Kind::Ship };

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

		float speed = RandomFloat(0.18f, 0.32f) * (1.0f + 0.35f * tier);
		float heading = RandomFloat(0.0f, 6.2831853f);

		Crown::Entity& rock = CreateEntity("Asteroid");
		rock.Position = { at.x, at.y, 0.0f };
		rock.Scale = { scaleFor[tier], scaleFor[tier] };
		rock.AtlasCell = (tier == 3) ? 1 : (tier == 2 ? 5 : 9);
		rock.Tint = { 0.75f, 0.78f, 0.85f, 1.0f };
		rock.Rotation = RandomFloat(0.0f, 360.0f);
		rock.UserData = Mover{
			Kind::Asteroid,
			{ std::cos(heading) * speed, std::sin(heading) * speed },
			RandomFloat(-70.0f, 70.0f),
			-1.0f,
			tier
		};
	}

	void FireBullet(glm::vec2 at, glm::vec2 direction)
	{
		Crown::Entity& bullet = CreateEntity("Bullet");
		bullet.Position = { at.x, at.y, 0.0f };
		bullet.Scale = { 0.035f, 0.035f };
		bullet.AtlasCell = 0;                     // a circle in the atlas
		bullet.Tint = { 1.0f, 0.85f, 0.35f, 1.0f };
		bullet.UserData = Mover{ Kind::Bullet, direction * 2.2f + m_ShipVelocity, 0.0f, 1.1f, 0 };
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

	// Moves everything that has a Mover, and retires bullets whose time is up.
	void Integrate(float dt)
	{
		m_Expired.clear();

		for (Crown::Entity& e : GetEntities())
		{
			Mover* m = MoverOf(e);
			if (!m || m->What == Kind::Ship)      // the ship is driven by input
				continue;

			e.Position += glm::vec3(m->Velocity * dt, 0.0f);
			e.Rotation += m->Spin * dt;
			Wrap(e);

			if (m->Life > 0.0f)
			{
				m->Life -= dt;
				if (m->Life <= 0.0f)
					m_Expired.push_back(e.ID);
			}
		}

		for (uint32_t id : m_Expired)
			DestroyEntity(id);
	}

	void ResolveCollisions()
	{
		// Gather, then act. Destroying or spawning mid-scan would invalidate
		// the entity references the scan is holding.
		m_Hits.clear();
		bool shipHit = false;

		for (Crown::Entity& e : GetEntities())
		{
			Mover* m = MoverOf(e);
			if (!m)
				continue;

			bool isBullet = m->What == Kind::Bullet;
			bool isShip   = m->What == Kind::Ship && m_Invulnerable <= 0.0f && m_RespawnIn <= 0.0f;
			if (!isBullet && !isShip)
				continue;

			FindOverlapping(e, m_Overlaps);
			for (uint32_t otherId : m_Overlaps)
			{
				Crown::Entity* other = FindEntity(otherId);
				Mover* om = other ? MoverOf(*other) : nullptr;
				if (!om || om->What != Kind::Asteroid)
					continue;                     // bullets ignore each other and the ship

				if (isBullet)
					m_Hits.push_back({ e.ID, otherId });
				else
					shipHit = true;
				break;                            // one asteroid per bullet per frame
			}
		}

		for (const Hit& hit : m_Hits)
			SplitAsteroid(hit.Asteroid, hit.Bullet);

		if (shipHit)
			LoseLife();
	}

	void SplitAsteroid(uint32_t rockId, uint32_t bulletId)
	{
		Crown::Entity* rock = FindEntity(rockId);
		Mover* m = rock ? MoverOf(*rock) : nullptr;
		if (!m)
			return;                               // already destroyed this frame

		int tier = m->Tier;
		glm::vec2 at{ rock->Position.x, rock->Position.y };

		static const int scoreFor[4] = { 0, 100, 50, 20 };
		m_Score += scoreFor[tier];

		DestroyEntity(bulletId);
		DestroyEntity(rockId);

		if (tier > 1)
		{
			for (int i = 0; i < 2; i++)
			{
				glm::vec2 offset{ RandomFloat(-0.04f, 0.04f), RandomFloat(-0.04f, 0.04f) };
				SpawnAsteroid(at + offset, tier - 1);
			}
		}
	}

	void LoseLife()
	{
		DestroyEntity(m_ShipID);
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

	struct Hit { uint32_t Bullet, Asteroid; };

	// Kept as members purely so the per-frame scans do not reallocate.
	std::vector<uint32_t> m_Overlaps;
	std::vector<uint32_t> m_Expired;
	std::vector<Hit>      m_Hits;

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
