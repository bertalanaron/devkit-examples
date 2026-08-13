#pragma once
#include <devkit/algo/geometry.h>
#include <devkit/algo/chunk_accessor.h>
#include <devkit/algo/navmesh.h>

class TerrainEditor;

class Terrain 
	: public dk::algo::NavmeshGenerator 
{
private:
	using Accessor = dk::algo::GridChunkAccessor<dk::algo::GridChunkLayout::Grouped>;

	class ChunkView {
	private:
		using Vertex = dk::gfx::Vertex<glm::vec3, glm::vec3>;
	public:
		ChunkView()
			: m_mesh(dk::common::id<Vertex>)
			, m_cliffModelTransforms(dk::common::id<dk::gfx::Vertex<glm::mat4>>)
		{ }

		auto& mesh()
		{ return m_mesh; }

		void pushPolygon(dk::geom::polygon2&& polygon, int height);

	private:
		dk::gfx::Mesh         m_mesh;
		dk::gfx::VertexBuffer m_cliffModelTransforms;
	};

public:
	Terrain(const Terrain&) = default;
	Terrain()
		: NavmeshGenerator(std::thread::hardware_concurrency())
		, m_gridSize(glm::ivec2(64, 64))
		, m_chunkSize(glm::ivec2(16, 16))
		, m_accessor(&m_chunkSize, &m_gridSize)
		, m_cells(m_gridSize.x * m_gridSize.y)
		, m_navmesh(m_accessor.sizeInChunks(), m_chunkSize)
	{ 
		// Create chunk views
		for (int x = 0; x < m_accessor.sizeInChunks().x; ++x)
			for (int y = 0; y < m_accessor.sizeInChunks().y; ++y)
				m_views.emplace(glm::ivec2(x, y), ChunkView());

		// Mark chunks as changed
		for (int x = 0; x < m_accessor.sizeInChunks().x; ++x)
			for (int y = 0; y < m_accessor.sizeInChunks().y; ++y)
				m_changedChunks.insert(glm::ivec2(x, y));
	}

	struct Cell {
		int                 height           = 0;
		std::optional<bool> pathableOverride = std::nullopt;

		bool                pathable         = true;
		mutable int         floodFillIsland  = -1;
	};

	void setHeight(const glm::ivec2& coords, int height)
	{
		if (!m_accessor.isInbounds(coords))
			return;
		auto& cell = m_cells[m_accessor.indexOf(coords)];
		cell.height = height;
		bool pathable = true;
		for (int x = coords.x - 1; x <= coords.x + 1; ++x)
			for (int y = coords.y - 1; y <= coords.y + 1; ++y)
			{
				if (!m_accessor.isInbounds({ x, y }))
					continue;
				auto& other = m_cells[m_accessor.indexOf({ x, y })];
				if (other.height > cell.height)
					pathable = false;
				else if (other.height < cell.height)
					other.pathable = other.pathableOverride.value_or(false);
			}
		cell.pathable = cell.pathableOverride.value_or(pathable);
		m_changedChunks.insert(m_accessor.chunkCoordsOf(coords));
	}

	void overridePathable(const glm::ivec2& coords, std::optional<bool> isPathable)
	{
		if (!m_accessor.isInbounds(coords))
			return;
		auto& cell            = m_cells[m_accessor.indexOf(coords)];
		cell.pathableOverride = isPathable;
		if (isPathable.has_value()) {
			cell.pathable         = isPathable.value();
		}
		else {
			for (int x = coords.x - 1; x <= coords.x + 1; ++x)
				for (int y = coords.y - 1; y <= coords.y + 1; ++y)
				{
					if (!m_accessor.isInbounds({ x, y }))
						continue;
					auto& other = m_cells[m_accessor.indexOf({ x, y })];
					if (other.height > cell.height)
						cell.pathable = cell.pathableOverride.value_or(false);
				}
		}
		m_changedChunks.insert(m_accessor.chunkCoordsOf(coords));
	}

	void regenerate()
	{
		for (int x = 0; x < m_accessor.sizeInChunks().x; ++x)
			for (int y = 0; y < m_accessor.sizeInChunks().y; ++y)
				m_changedChunks.insert(glm::ivec2(x, y));
	}

	void render(dk::gfx::FrameBuffer& frameBuffer, dk::gfx::Shader& shader)
	{
		for (auto& [coords, view] : m_views) {
			shader.layout(view.mesh());
			frameBuffer.render(shader, view.mesh().indices, dk::gfx::Primitive::Triangles);
		}
	}

	dk::algo::Navmesh& navmesh()
	{ return m_navmesh; }

	const auto& accessor() const
	{ return m_accessor; }

	auto& operator[](const glm::ivec2& coords)
	{ return m_cells[m_accessor.indexOf(coords)]; }

private:
	glm::ivec2        m_gridSize;
	glm::ivec2        m_chunkSize;
	Accessor          m_accessor;
	std::vector<Cell> m_cells;

	dk::algo::Navmesh m_navmesh;

	std::unordered_map<glm::ivec2, ChunkView> m_views;

	std::unordered_set<glm::ivec2> m_changedChunks;


private:
	bool chunkUpdated(const glm::ivec2& chunkCoord) const override;

	void generateChunk(dk::algo::NavmeshChunk& chunk, const glm::ivec2& chunkCoord) override;

	void buildDone() override;

	friend class dk::algo::NavmeshGenerator;
	friend class TerrainEditor;
};
