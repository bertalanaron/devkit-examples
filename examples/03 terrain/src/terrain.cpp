#include "terrain.h"

#include <devkit/algo/flood_fill.h>
#include <devkit/algo/marching_squares.h>
#include <devkit/algo/draw.h>

bool Terrain::chunkUpdated(const glm::ivec2& chunkCoords) const
{
	bool shouldUpdate = false;
	if (m_changedChunks.contains(chunkCoords))
		return true;
	for (int x = chunkCoords.x - 1; x <= chunkCoords.x + 1; ++x)
		for (int y = chunkCoords.y - 1; y <= chunkCoords.y + 1; ++y)
		{
			if (m_changedChunks.contains(glm::ivec2(x, y)))
				return true;
		}
	return false;
}

void Terrain::generateChunk(dk::algo::NavmeshChunk& chunk, const glm::ivec2& chunkCoord)
{
	chunk.clearPolygons();
	m_views.at(chunkCoord).mesh().vertices.modify().clear();
	m_views.at(chunkCoord).mesh().indices.clear();
	const bool allowDiags = true;

	// Getter and setter used by flud fill
	const auto fludfillGetter = [this](int x, int y, int fromX, int fromY/*, std::optional<glm::ivec2> from*/) 
	{ 
		if (!m_accessor.isInbounds({ x, y }))
			return false;
		//const auto& cell = m_cells[m_accessor.indexOf({ x, y })];
		//return m_cells[m_accessor.indexOf({ x, y })].pathable;
		return m_cells[m_accessor.indexOf({ x, y })].height == m_cells[m_accessor.indexOf({ fromX, fromY })].height;
		//if (!from.has_value())
		//	return cell.pathableOverride.value_or(true);
	};
	std::vector<int> islandHeights{ 0 };
	const auto fludfillSetter = [this, &islandHeights](int x, int y, int island) 
	{ 
		if (!m_accessor.isInbounds({ x, y }))
			return;
		m_cells[m_accessor.indexOf({ x, y })].floodFillIsland = island; 
		if (islandHeights.size() <= island)
		{
			islandHeights.resize(island + 1);
			islandHeights[island] = m_cells[m_accessor.indexOf({ x, y })].height;
		}
	};

	// Flud fill to separate islands
	const auto floodFillOffset = chunkCoord * m_chunkSize - glm::ivec2(chunkCoord.x == 0, chunkCoord.y == 0);
	const auto floodFillSize   = m_chunkSize + glm::ivec2(1, 1) + glm::ivec2(chunkCoord.x == 0, chunkCoord.y == 0);
	int regionCount = dk::algo::floodFillGrid(fludfillGetter, fludfillSetter, floodFillSize, floodFillOffset, allowDiags);

	// Handle each island separately
	for (int i = 0; i < regionCount; ++i) {
		const auto height = islandHeights[i + 1];
		//const auto height = 1;
		const auto marchingsquaresComp = [this, i, height](int x, int y) -> bool 
		{
			if (!m_accessor.isInbounds({ x, y }))
				return false;
			return m_cells[m_accessor.indexOf({ x, y })].floodFillIsland == i + 1;
		};
		double minOffset = .001;
		double offsetToHigher = minOffset;
		double maxOffset = .5;
		const auto marchingsquaresEdgeOffset = [this, i, height, &minOffset, &maxOffset, &offsetToHigher](int x, int y, int fromX, int fromY) -> double 
		{
			if (!m_accessor.isInbounds({ x, y }))
				return minOffset;
			if (!m_accessor.isInbounds({ fromX, fromY }))
				return maxOffset;
			if (m_cells[m_accessor.indexOf({ x, y })].height > m_cells[m_accessor.indexOf({ fromX, fromY })].height)
				return maxOffset;
			if (m_cells[m_accessor.indexOf({ x, y })].height < m_cells[m_accessor.indexOf({ fromX, fromY })].height)
				return offsetToHigher;
			return 1;
		};

		// Construct polygon using marching squares
		const auto marchingSquaresOffset = chunkCoord * m_chunkSize - glm::ivec2(chunkCoord.x == 0, chunkCoord.y == 0);
		const auto marchingSquaresSize   = m_chunkSize + glm::ivec2(chunkCoord.x == 0, chunkCoord.y == 0);
		const auto navmeshPolygon 
			= dk::algo::marchingSquaresConstructPolygon(marchingsquaresComp, marchingsquaresEdgeOffset, marchingSquaresSize, marchingSquaresOffset, allowDiags, true/*, 0.5*/);
		minOffset = .25;
		maxOffset = .75;
		offsetToHigher = .75;
		auto terrainPolygon 
			= dk::algo::marchingSquaresConstructPolygon(marchingsquaresComp, marchingsquaresEdgeOffset, marchingSquaresSize, marchingSquaresOffset, allowDiags, true/*, dk::dbg::store_or<float, "offset_from_vertex">(0.75)*/);

		// Draw polygons
		auto out = dk::dbg::store_or<dk::gfx::VertexSink*, "navmesh_poly_out">(nullptr);
		//*out << dk::gfx::draw(navmeshPolygon, DK_COLOR(0xaaaaaaff), DK_COLOR(0xaaaaaaff), dk::geom::plane::Y() + .001 + height, -dk::geom::axis::X);
		*out << dk::gfx::draw(terrainPolygon, dk::colors::white, dk::colors::white, dk::geom::plane::Y() + .001 + height, -dk::geom::axis::X);

		dk::dbg::store_threadlocal<int, "current_height">() = height;

		chunk.pushPolygon(navmeshPolygon, dk::algo::NavmeshDecomposition::ConvexDecomp);
		m_views.at(chunkCoord).pushPolygon(std::move(terrainPolygon), height);
	}
}

void Terrain::buildDone()
{
	m_changedChunks.clear();
}

void Terrain::ChunkView::pushPolygon(dk::geom::polygon2&& polygon, int height)
{
	const auto triangulated = polygon.triangulate();
	for (const auto& triangle : triangulated) 
	{
		constexpr static auto toVec3 = [](const glm::dvec2& v, double hight)
		{
			return glm::vec3(v.x, hight, v.y);
		};

		const auto indexBegin = m_mesh.vertices.size();
		auto& vertices = m_mesh.vertices.modify();
		vertices.push_back(Vertex(toVec3(triangle.vertices.at(0), height), dk::geom::axis::Y));
		vertices.push_back(Vertex(toVec3(triangle.vertices.at(1), height), dk::geom::axis::Y));
		vertices.push_back(Vertex(toVec3(triangle.vertices.at(2), height), dk::geom::axis::Y));
		auto& indices = m_mesh.indices;
		indices.push(indexBegin + 0);
		indices.push(indexBegin + 1);
		indices.push(indexBegin + 2);
	}

	const auto drawEdge = [&](const dk::geom::edge3& edge)
	{
		const auto direction = edge[1] - edge[0];
		const auto normal = glm::cross(direction, dk::geom::axis::Y);
		const auto indexBegin = m_mesh.vertices.size();
		auto& vertices = m_mesh.vertices.modify();
		vertices.push_back(Vertex((glm::vec3)edge[0], (glm::vec3)normal));
		vertices.push_back(Vertex((glm::vec3)edge[1], (glm::vec3)normal));
		vertices.push_back(Vertex((glm::vec3)edge[1] + glm::vec3(0, -height, 0), (glm::vec3)normal));
		vertices.push_back(Vertex((glm::vec3)edge[0] + glm::vec3(0, -height, 0), (glm::vec3)normal));
		auto& indices = m_mesh.indices;
		indices.push(indexBegin + 0);
		indices.push(indexBegin + 1);
		indices.push(indexBegin + 2);
		indices.push(indexBegin + 2);
		indices.push(indexBegin + 3);
		indices.push(indexBegin + 0);
	};

	for (int i = 0; i < polygon.vertices.size(); ++i)
	{
		const dk::geom::edge3 edge = [&]{ 
			const auto& v1 = polygon.vertices[i];
			const auto& v2 = polygon.vertices[(i + 1) % polygon.vertices.size()];
			return dk::geom::edge3{ glm::dvec3(v1.x, height, v1.y), glm::dvec3(v2.x, height, v2.y) };
		}();
		drawEdge(edge);
	}
	
	for (const auto& hole : polygon.holes)
		for (int i = 0; i < hole.size(); ++i)
		{
			const dk::geom::edge3 edge = [&]{ 
				const auto& v1 = hole[i];
				const auto& v2 = hole[(i + 1) % hole.size()];
				return dk::geom::edge3{ glm::dvec3(v1.x, height, v1.y), glm::dvec3(v2.x, height, v2.y) };
				}();
			drawEdge(edge);
		}
}
