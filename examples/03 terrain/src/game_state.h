#pragma once
#include "terrain.h"

#include <mini/ini.h>
#include <devkit/io/asset_manager.h>

void setupAssetManager(dk::io::AssetManager& assets, mINI::INIStructure& ini);

class GameState {
public:
	std::unique_ptr<Terrain> terrain;

	GameState()
	{
		terrain = std::make_unique<Terrain>();
	}

	std::unique_ptr<GameState> clone()
	{
		return std::unique_ptr<GameState>(new GameState(*this));
	}

	static GameState load(const std::filesystem::path& path);

	void save(const std::filesystem::path& path);

private:
	GameState(const GameState& other)
	{
		// Clone terrain
		if (other.terrain.get())
			terrain = std::make_unique<Terrain>(*other.terrain);
	}
};
