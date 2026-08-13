#pragma once
#include <devkit/common/utils.h>
#include <mini/ini.h>

class ExampleApplicationBase {
public:
	ExampleApplicationBase()
	{
		// Parse ini file
		m_ini = [] {
			std::string path = dk::common::executable_path().string();
			if (path.contains(".exe"))
				path.replace(path.find(".exe"), 4, ".ini");
			else
				path += ".ini";
			mINI::INIFile file(path);
			mINI::INIStructure ini;
			file.read(ini);
			return ini;
		}();
	}

private:
	mINI::INIStructure m_ini;

protected:
	auto& ini() { return m_ini; }
	const auto& ini() const { return m_ini; }

	std::optional<std::string> ini(const std::string& label, const std::string& value)
	{
		if (!m_ini.has(label) || !m_ini[label].has(value))
			return std::nullopt;
		return m_ini[label][value];
	}

	std::string ini_or(
		const std::string& label, 
		const std::string& value, 
		const std::string& fallback)
	{
		const auto result = ini(label, value);
		if (result.has_value())
			return result.value();
		return fallback;
	}
};
