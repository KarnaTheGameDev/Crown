#include "crpch.h"
#include "TextureLibrary.h"

namespace Crown {

	const Texture2D& TextureLibrary::Get(const std::string& path)
	{
		// Created on first request rather than in the constructor: by the time
		// anything asks for a texture there is a GL context, which is not true
		// when this object is constructed.
		if (!m_White)
			m_White = std::make_unique<Texture2D>();

		if (path.empty())
			return *m_White;

		auto found = m_Textures.find(path);
		if (found != m_Textures.end())
			return *found->second;

		// Cached even when the load failed. Texture2D has already logged the
		// reason and fallen back to white, and retrying every frame would
		// reprint that message a hundred times a second.
		auto inserted = m_Textures.emplace(path, std::make_unique<Texture2D>(path));
		return *inserted.first->second;
	}

	void TextureLibrary::Reload()
	{
		m_Textures.clear();
		CROWN_CORE_INFO("TextureLibrary: cleared, textures will reload on next use");
	}

}
