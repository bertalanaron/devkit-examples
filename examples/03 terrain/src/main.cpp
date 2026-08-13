#include "editor_client.h"

#include <devkit/gfx/texture.h>
#include <devkit/gfx/scene.h>

//namespace dk
//{
//
//namespace details
//{
//inline std::unordered_map<std::string, std::shared_ptr<spdlog::async_logger>> loggers{};
//}
//
//auto& logger(const std::string& ns)
//{
//	auto it = details::loggers.find(ns);
//	if (it == details::loggers.end())
//		it = details::loggers.emplace(spdlog::async_factory::create<spdlog::sinks::wincolor_stdout_sink_mt>(ns)).first;
//	return *it->second;
//}
//
//void log(const std::string& ns, spdlog::level::level_enum level, const std::string& msg, const spdlog::source_loc& loc = {})
//{
//	logger(ns).log(loc, level, msg);
//}
//
//#ifdef _DEBUG
//void log_trace(const std::string& ns, const std::string& msg, const spdlog::source_loc& loc = {})
//{ log(ns, spdlog::level::trace, msg, loc); }
//#else
//void log_trace(auto&&...) { }
//#endif
//
//void log_debug(const std::string& ns, const std::string& msg, const spdlog::source_loc& loc = {})
//{ log(ns, spdlog::level::debug, msg, loc); }
//
//void log_info(const std::string& ns, const std::string& msg, const spdlog::source_loc& loc = {})
//{ log(ns, spdlog::level::info, msg, loc); }
//
//void log_warn(const std::string& ns, const std::string& msg, const spdlog::source_loc& loc = {})
//{ log(ns, spdlog::level::warn, msg, loc); }
//
//void log_err(const std::string& ns, const std::string& msg, const spdlog::source_loc& loc = {})
//{ log(ns, spdlog::level::err, msg, loc); }
//
//void log_critical(const std::string& ns, const std::string& msg, const spdlog::source_loc& loc = {})
//{ log(ns, spdlog::level::critical, msg, loc); }
//
//}

void setupAssetManager(dk::io::AssetManager& assets, mINI::INIStructure& ini)
{
	// Define root folder
	assets.root(ini["data"]["path"]);

	// Register types
	//assets.type<dk::gfx::Scene>("fbx", dk::gfx::Scene::loadFromFile);
	assets.type<dk::gfx::Texture2D>({ "png", "jpg" },
		dk::gfx::Texture2D::load, std::nullopt, std::nullopt, dk::io::AssetManager::Async);
	assets.type<dk::gfx::ShaderSource>("glsl", dk::gfx::ShaderSource::load, &dk::gfx::ShaderSource::update);

	// Setup directories to watch
	assets.watch("/textures", true);
	//assets.watch("/textures/terrain", false);
	//assets.watch("/textures/ui"     , true);
	assets.watch("/shaders" , true);
}

int main()
{
	// Set loglevel
	spdlog::set_level(spdlog::level::trace);

	// Initialize and run editor client
	EditorClient editor;
	editor.run();

	return 0;
}
