#include "crpch.h"
#include "Framebuffer.h"

#include <glad/gl.h>

namespace Crown {

	// A zero-sized attachment is invalid GL, and a docked panel can briefly
	// report zero while it is being dragged.
	static constexpr unsigned int s_MinSize = 1;
	static constexpr unsigned int s_MaxSize = 8192;

	Framebuffer::Framebuffer(unsigned int width, unsigned int height)
		: m_Width(std::clamp(width, s_MinSize, s_MaxSize)),
		  m_Height(std::clamp(height, s_MinSize, s_MaxSize))
	{
		Invalidate();
	}

	Framebuffer::~Framebuffer()
	{
		Destroy();
	}

	void Framebuffer::Destroy()
	{
		glDeleteFramebuffers(1, &m_RendererID);
		glDeleteTextures(1, &m_ColorAttachment);
		m_RendererID = m_ColorAttachment = 0;
	}

	void Framebuffer::Invalidate()
	{
		if (m_RendererID)
			Destroy();

		glCreateFramebuffers(1, &m_RendererID);

		glCreateTextures(GL_TEXTURE_2D, 1, &m_ColorAttachment);
		glTextureStorage2D(m_ColorAttachment, 1, GL_RGBA8, m_Width, m_Height);
		glTextureParameteri(m_ColorAttachment, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_ColorAttachment, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// Clamp, or linear filtering samples the opposite edge at the panel border.
		glTextureParameteri(m_ColorAttachment, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_ColorAttachment, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glNamedFramebufferTexture(m_RendererID, GL_COLOR_ATTACHMENT0, m_ColorAttachment, 0);

		GLenum status = glCheckNamedFramebufferStatus(m_RendererID, GL_FRAMEBUFFER);
		CROWN_CORE_ASSERT(status == GL_FRAMEBUFFER_COMPLETE, "Framebuffer incomplete!");
	}

	void Framebuffer::Bind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		glViewport(0, 0, m_Width, m_Height);
	}

	void Framebuffer::Unbind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Framebuffer::Resize(unsigned int width, unsigned int height)
	{
		width  = std::clamp(width,  s_MinSize, s_MaxSize);
		height = std::clamp(height, s_MinSize, s_MaxSize);
		if (width == m_Width && height == m_Height)
			return;                                    // reallocating every frame would thrash

		m_Width = width;
		m_Height = height;
		Invalidate();
	}

}
