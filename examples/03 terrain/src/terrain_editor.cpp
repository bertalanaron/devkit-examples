//#include "terrain_editor.h"
//
//#include <devkit/io/file_dialog.h>
//
//#define TERRAIN_HEIGHTMAP_VERSION 1
//
//void writeBin(std::ofstream& ofs, auto& value)
//{
//	ofs.write(reinterpret_cast<const char*>(&value), sizeof(std::decay_t<decltype(value)>));
//}
//
//void readBin(std::ifstream& ifs, auto& value)
//{
//	ifs.read(reinterpret_cast<char*>(&value), sizeof(std::decay_t<decltype(value)>));
//}
//
//bool TerrainEditor::saveHeightMap(Terrain& terrain)
//{
//	const auto filters = dk::io::FileDialog::makeFilters(dk::io::FileDialog::filter("heightmap", "bin"));
//	const auto optSavePath = dk::io::FileDialog::saveFile(dk::common::executable_path().string(), filters);
//	if (!optSavePath)
//	{
//		spdlog::info("Canceled terrain heightmap save");
//		return false;
//	}
//
//	std::ofstream ofs(optSavePath.value(), std::ios::binary);
//	if (!ofs.is_open())
//	{
//		spdlog::warn("Couldn't save terrain heightmap into {}", optSavePath.value().string());
//		return false;
//	}
//
//	int version = TERRAIN_HEIGHTMAP_VERSION;
//	writeBin(ofs, version);
//	writeBin(ofs, terrain.m_gridSize.x);
//	writeBin(ofs, terrain.m_gridSize.y);
//	writeBin(ofs, terrain.m_chunkSize.x);
//	writeBin(ofs, terrain.m_chunkSize.y);
//
//	for (int x = 0; x < terrain.m_gridSize.x; ++x)
//		for (int y = 0; y < terrain.m_gridSize.x; ++y)
//			writeBin(ofs, terrain.m_cells[terrain.m_accessor.indexOf({ x, y })].height);
//
//	ofs.close();
//	return true;
//}
//
//void TerrainEditor::loadHeightMap(Terrain& terrain)
//{
//	const auto filters = dk::io::FileDialog::makeFilters(dk::io::FileDialog::filter("heightmap", "bin"));
//	const auto optLoadPath = dk::io::FileDialog::openFile(dk::common::executable_path().string(), filters);
//	if (!optLoadPath)
//	{
//		spdlog::info("Canceled terrain heightmap load");
//		return;
//	}
//
//	std::ifstream ifs(optLoadPath.value(), std::ios::binary);
//	if (!ifs.is_open())
//	{
//		spdlog::warn("Couldn't load terrain heightmap from {}", optLoadPath.value().string());
//		return;
//	}
//
//	int heightmapVersion;
//	glm::ivec2 gridSize;
//	glm::ivec2 chunkSize;
//
//	readBin(ifs, heightmapVersion);
//	if (heightmapVersion != TERRAIN_HEIGHTMAP_VERSION)
//	{
//		spdlog::error("Terrain heightmap version mismatch");
//		std::terminate();
//	}
//
//	readBin(ifs, gridSize.x);
//	readBin(ifs, gridSize.y);
//	readBin(ifs, chunkSize.x);
//	readBin(ifs, chunkSize.y);
//
//	for (int x = 0; x < terrain.m_gridSize.x; ++x)
//		for (int y = 0; y < terrain.m_gridSize.x; ++y)
//		{
//			int height;
//			readBin(ifs, height);
//			terrain.m_cells[terrain.m_accessor.indexOf({ x, y })].height = height;
//		}
//	regenerateNavmesh(terrain);
//}
