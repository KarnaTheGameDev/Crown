#pragma once

#include <string>
#include <glm/glm.hpp>

namespace Crown {

	// Compiles and owns one GL program. Extracted from Application because the
	// compile/link/error-report path is ~45 lines that no caller should repeat.
	class Shader
	{
	public:
		Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
		~Shader();

		void Bind() const;
		void Unbind() const;

		void SetMat4(const std::string& name, const glm::mat4& value) const;

	private:
		// Returns 0 and logs the driver's message if the stage does not compile.
		static unsigned int Compile(unsigned int type, const std::string& src);

		unsigned int m_RendererID = 0;
	};

}
