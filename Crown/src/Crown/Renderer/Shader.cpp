#include "crpch.h"
#include "Shader.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

namespace Crown {

	unsigned int Shader::Compile(unsigned int type, const std::string& src)
	{
		unsigned int id = glCreateShader(type);
		const char* raw = src.c_str();
		glShaderSource(id, 1, &raw, nullptr);
		glCompileShader(id);

		int ok = 0;
		glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
		if (!ok)
		{
			int len = 0;
			glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
			std::vector<char> log(len);
			glGetShaderInfoLog(id, len, &len, log.data());
			CROWN_CORE_ERROR("Shader compile failed: {0}", log.data());
			glDeleteShader(id);
			return 0;
		}
		return id;
	}

	Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc)
	{
		unsigned int vs = Compile(GL_VERTEX_SHADER, vertexSrc);
		unsigned int fs = Compile(GL_FRAGMENT_SHADER, fragmentSrc);

		if (vs && fs)
		{
			m_RendererID = glCreateProgram();
			glAttachShader(m_RendererID, vs);
			glAttachShader(m_RendererID, fs);
			glLinkProgram(m_RendererID);

			int ok = 0;
			glGetProgramiv(m_RendererID, GL_LINK_STATUS, &ok);
			if (!ok)
			{
				int len = 0;
				glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &len);
				std::vector<char> log(len);
				glGetProgramInfoLog(m_RendererID, len, &len, log.data());
				CROWN_CORE_ERROR("Shader link failed: {0}", log.data());
				glDeleteProgram(m_RendererID);
				m_RendererID = 0;
			}
		}

		glDeleteShader(vs);
		glDeleteShader(fs);
	}

	Shader::~Shader()
	{
		glDeleteProgram(m_RendererID);
	}

	void Shader::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void Shader::Unbind() const
	{
		glUseProgram(0);
	}

	void Shader::SetInt(const std::string& name, int value) const
	{
		glUniform1i(glGetUniformLocation(m_RendererID, name.c_str()), value);
	}

	void Shader::SetMat4(const std::string& name, const glm::mat4& value) const
	{
		// ponytail: uniform locations are looked up per call. Cache them in a map
		// if a profiler ever says this matters.
		int location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
	}

}
