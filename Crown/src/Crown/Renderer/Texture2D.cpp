#include "crpch.h"
#include "Texture2D.h"

#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Crown {

	Texture2D::Texture2D(const std::string& path)
	{
		// stb reads top-down; OpenGL's first texel row is the bottom of the
		// image. Without this flip every texture renders upside down.
		stbi_set_flip_vertically_on_load(1);

		int w = 0, h = 0, channels = 0;
		stbi_uc* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
		if (!data)
		{
			CROWN_CORE_ERROR("Texture2D: could not load '{0}' ({1})", path, stbi_failure_reason());
			CreateWhite();
			return;
		}

		unsigned int internalFormat = 0, dataFormat = 0;
		if (channels == 4)      { internalFormat = GL_RGBA8; dataFormat = GL_RGBA; }
		else if (channels == 3) { internalFormat = GL_RGB8;  dataFormat = GL_RGB;  }
		else
		{
			CROWN_CORE_ERROR("Texture2D: '{0}' has {1} channels, only 3 and 4 are supported", path, channels);
			stbi_image_free(data);
			CreateWhite();
			return;
		}

		m_Width = w; m_Height = h;

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, internalFormat, m_Width, m_Height);

		// Nearest keeps the checkerboard crisp; swap to LINEAR for photographic art.
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

		// Rows are tightly packed; the default 4-byte alignment corrupts RGB
		// images whose width is not a multiple of 4.
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, dataFormat, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
		m_Loaded = true;
		CROWN_CORE_INFO("Texture2D: loaded '{0}' ({1}x{2}, {3} channels)", path, m_Width, m_Height, channels);
	}

	Texture2D::Texture2D()
	{
		CreateWhite();
	}

	void Texture2D::CreateWhite()
	{
		m_Width = m_Height = 1;
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, GL_RGBA8, 1, 1);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		unsigned int white = 0xffffffff;
		glTextureSubImage2D(m_RendererID, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &white);
	}

	Texture2D::~Texture2D()
	{
		glDeleteTextures(1, &m_RendererID);
	}

	void Texture2D::Bind(unsigned int slot) const
	{
		glBindTextureUnit(slot, m_RendererID);
	}

}
