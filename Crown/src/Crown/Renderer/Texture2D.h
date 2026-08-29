#pragma once

#include <string>

namespace Crown {

	class Texture2D
	{
	public:
		// Loads an image from disk. On failure the texture falls back to solid
		// white, so a missing file tints nothing rather than rendering black.
		explicit Texture2D(const std::string& path);

		// 1x1 solid white. Multiplying by this leaves vertex colour untouched,
		// which lets untextured geometry share the textured shader.
		Texture2D();

		~Texture2D();

		Texture2D(const Texture2D&) = delete;              // owns a GL name
		Texture2D& operator=(const Texture2D&) = delete;

		void Bind(unsigned int slot = 0) const;

		unsigned int GetWidth() const { return m_Width; }
		unsigned int GetHeight() const { return m_Height; }
		bool IsLoaded() const { return m_Loaded; }

	private:
		void CreateWhite();

		unsigned int m_RendererID = 0;
		unsigned int m_Width = 0, m_Height = 0;
		bool m_Loaded = false;
	};

}
