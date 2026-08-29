#pragma once

namespace Crown {

	// Renders into an off-screen colour texture instead of the window, so the
	// scene can be shown inside an editor panel.
	class Framebuffer
	{
	public:
		Framebuffer(unsigned int width, unsigned int height);
		~Framebuffer();

		Framebuffer(const Framebuffer&) = delete;              // owns GL names
		Framebuffer& operator=(const Framebuffer&) = delete;

		void Bind() const;
		void Unbind() const;

		// No-op when the size is unchanged, so this is safe to call every frame.
		void Resize(unsigned int width, unsigned int height);

		unsigned int GetColorAttachment() const { return m_ColorAttachment; }
		unsigned int GetWidth() const { return m_Width; }
		unsigned int GetHeight() const { return m_Height; }

	private:
		void Invalidate();
		void Destroy();

		unsigned int m_RendererID = 0;
		unsigned int m_ColorAttachment = 0;
		unsigned int m_Width = 0, m_Height = 0;
	};

}
