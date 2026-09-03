#pragma once

#include "Texture2D.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Crown {

	// Loads each texture once and hands out references to it. Entities name a
	// texture by path, and many entities usually name the same one, so without
	// this the renderer would reload the same file every frame.
	class TextureLibrary
	{
	public:
		// Nothing is created here. TextureLibrary is a member of Application, so
		// it is constructed before Application's body has made a window, and
		// there is no GL context yet. Everything is created on first use.
		TextureLibrary() = default;

		// Never fails: an empty path gives the 1x1 white texture, and a path
		// that will not load gives it too, after Texture2D has logged why. A
		// missing sprite therefore renders as flat tint rather than vanishing
		// or taking the frame down.
		const Texture2D& Get(const std::string& path);

		// Drops every cached texture except white, so edited art is picked up
		// without restarting.
		void Reload();

		size_t LoadedCount() const { return m_Textures.size(); }

	private:
		std::unique_ptr<Texture2D> m_White;
		std::unordered_map<std::string, std::unique_ptr<Texture2D>> m_Textures;
	};

}
