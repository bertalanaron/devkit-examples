#include "examples/common/src/example_application.h"

#include <devkit/common.h>
#include <devkit/io.h>
#include <devkit/gfx.h>
#include <devkit/algo.h>
#include <devkit/io/frame.h>

#include <mini/ini.h>

#include <imgui.h>

class AlgoTester 
	: public ExampleApplicationBase
{
public:
	void setup()
	{
		// Setup asset manager directories
		m_assets.root(ini("data", "path").value());
		m_assets.watch("/shaders" , true);

		// Setup asset types
		m_assets.type<dk::gfx::ShaderSource>("glsl", dk::gfx::ShaderSource::load, &dk::gfx::ShaderSource::update);
		m_assets.type<dk::gfx::Texture2D>("png", dk::gfx::Texture2D::load);
		m_assets.typeName<dk::gfx::ShaderSource>("ShaderSource");
		m_assets.typeName<dk::gfx::Texture2D>("Texture");
		// Load assets
		m_assets.synchronize();

		// Setup window
		m_window.config(dk::io::Window::Title("Algo Tester"));
		m_window.config(dk::io::Window::Theme::Dark);
		m_window.open(4);
		dk::gfx::backBuffer().config(dk::gfx::FrameBuffer::Multisample::Enabled);
		dk::gfx::backBuffer().config(dk::gfx::FrameBuffer::DepthTest::Enabled);

		// Setup user inputs
		m_inputManager.define("shift"             , dk::io::modkey::none + dk::io::button::left);
		m_inputManager.define("toggle_fullscreen" , dk::io::key::f);
		m_inputManager.define("create_edge"       , dk::io::modkey::none + dk::io::button::right);

		// Create shaders
		m_shaders.insert("rgba", m_assets.getMultipleWeak<dk::gfx::ShaderSource>("/shaders/rgba_vs.glsl", "/shaders/rgba_fs.glsl"));

		// Setup camera
		m_camera.lookat   = glm::vec3(0,0,-.1);
		m_camera.position = glm::vec3(0,10,0);
		m_ucCamera.bind("u_camera.VP",        [&]() -> glm::mat4 { return m_camera.P() * m_camera.V(); });
		m_ucCamera.bind("u_camera.position",  [&]() -> glm::vec3 { return m_camera.position; });
		m_ucCamera.bind("u_camera.direction", [&]() -> glm::vec3 { return m_camera.lookat - m_camera.position; });
	}

	void run()
	{
		dk::algo::Funnel             funnel(glm::dvec2(0,0), dk::geom::edge2{ glm::dvec2(0,1), glm::dvec2(0,1) });
		std::vector<dk::geom::edge2> edges;
		dk::geom::edge2              currentEdge;

		while (m_window.isOpen()) {
			const auto& frame = m_window.beginFrame();
			m_assets.synchronize();
			dk::gfx::backBuffer().clear(dk::gfx::Clear::Color | dk::gfx::Clear::Depth, DK_COLOR(0x333333ff));
			moveCamera(frame);

			m_shaders["rgba"].uniforms() << m_ucCamera;
			auto drawer2d = dk::gfx::drawer2d(dk::geom::plane::Y(), -dk::geom::axis::X);
			m_dbgVertexSink << dk::gfx::draw(dk::geom::ray3({}, dk::geom::axis::X), dk::colors::red)
				            << dk::gfx::draw(dk::geom::ray3({}, dk::geom::axis::Y), dk::colors::lime)
				            << dk::gfx::draw(dk::geom::ray3({}, dk::geom::axis::Z), dk::colors::blue);

			if (m_inputManager.activated("create_edge"))
				currentEdge[0] = dk::geom::xz(dk::geom::intersection(m_camera.castRay(frame.cursorN()), dk::geom::plane::Y()));
			if (m_inputManager.active("create_edge")) {
				currentEdge[1] = dk::geom::xz(dk::geom::intersection(m_camera.castRay(frame.cursorN()), dk::geom::plane::Y()));
				m_dbgVertexSink << drawer2d(currentEdge, dk::colors::maroon);
			}
			if (m_inputManager.deactivated("create_edge")) {
				funnel.appendPortal(currentEdge);
				edges.push_back(dk::geom::edge2{currentEdge[0], currentEdge[1]});
			}
			funnel.evaluateContainedPaths();
			for (const auto& edge : edges)
				m_dbgVertexSink << drawer2d(edge, dk::colors::aqua, dk::colors::blue);
			m_dbgVertexSink << drawer2d(funnel, dk::colors::red, dk::colors::orange, dk::colors::orange, dk::colors::maroon);

			// Toggle fullscreen with the f key
			if (m_inputManager.activated("toggle_fullscreen"))
				m_window.config(dk::common::toggle(m_window.config.get<dk::io::Window::Mode>()));
			// Close window with the esc key
			if (dk::io::key::esc) m_window.close();

			m_dbgVertexSink.flush(m_shaders["rgba"], dk::gfx::backBuffer());

			m_window.endFrame();
		}
	}

	AlgoTester()
		: m_dbgVertexSink(dk::common::id<dk::gfx::RGBAVertex>)
	{ 
		dk::dbg::store<dk::gfx::VertexSink*, "funnel_dbg">() = &m_dbgVertexSink;
	}

private:
	dk::io::Window       m_window;
	dk::io::AssetManager m_assets;
	dk::io::InputManager m_inputManager;

	using shaders_t = std::unordered_map<std::string, std::unique_ptr<dk::gfx::Shader>>;

	dk::gfx::VertexSink        m_dbgVertexSink;
	dk::gfx::Camera            m_camera;
	dk::gfx::UniformCollection m_ucCamera;
	//shaders_t                  m_shaders;
	dk::gfx::ShaderCollection  m_shaders;

	void moveCamera(const dk::io::Frame& frame)
	{
		if (m_inputManager.active("shift"))
		{
			auto cursorProjection     = dk::geom::intersection(m_camera.castRay(frame.cursorN()), dk::geom::plane::Y());
			auto prevCursorProjection = dk::geom::intersection(m_camera.castRay(frame.cursorN() - frame.cursorDeltaN()), dk::geom::plane::Y());
			dk::gfx::Camera::Orbit::shift(m_camera, prevCursorProjection - cursorProjection);
		}

		// Zoom
		if (dk::io::wheel::up && !dk::io::button::middle)
			dk::gfx::Camera::Orbit::zoom(m_camera, 0.9);
		if (dk::io::wheel::down && !dk::io::button::middle)
			dk::gfx::Camera::Orbit::zoom(m_camera, 1.1);

		// Set camera aspect ratio
		m_camera.asp = dk::gfx::backBuffer().aspectRatio();
	}
};

int main(void) {
	spdlog::set_level(spdlog::level::trace);

	AlgoTester rts;
	rts.setup();
	rts.run();

	return 0;
}

//#include <queue>
//
//class Navmesh {
//public:
//	std::optional<const dk::geom::polygon2*> polygonAtPoint(const glm::dvec2& point) const;
//
//	std::optional<const dk::geom::polygon2*> polygonOfEdge(const dk::geom::edge2& edge) const;
//
//	unsigned polygonLevel(const dk::geom::polygon2* polygon) const;
//};
//
//class Route {};
//
//// Definitions:
////	- trivial path -> dst and start points are both contained in the same lvl 2 or under region
////
////  - lvl0 -> has no neighbouring nodes
////  - lvl1 -> has exactly 1 neighbouring node
////  - lvl2 -> has 2 or more neighbouring nodes
////  - lvl3 -> has 3 or more lvl2 neighbours
//
//class Context {
//public:
//	std::optional<Route> findPath(const glm::dvec2& start)
//	{
//		// Setup and optionally find path in lvl less than 2 neighbouring nodes
//		bool lvl3DstNodesDiscovered = false;
//		auto optRoute = bootstrap(lvl3DstNodesDiscovered, start);
//		if (!lvl3DstNodesDiscovered)
//			return optRoute;
//
//	}
//
//private:
//	template <typename K, typename T>
//	using map = std::unordered_map<K, T>;
//
//	template <typename T>
//	using set = std::unordered_set<T>;
//
//	using Vertex = glm::dvec2;
//	using Node   = const dk::geom::polygon2*;
//	using Edge   = dk::geom::edge2;
//	using Funnel = dk::geom::Funnel;
//
//	struct Access {
//		Edge     edge;
//		uint16_t index;
//	};
//
//	struct FrontierElemCost {
//		double fMinCost;
//		double hCost;
//	};
//
//	using FrontierElem = std::pair<Access, FrontierElemCost>;
//	using Frontier     = std::priority_queue<FrontierElem>;
//
//private:
//	// @returns The trivial route if it exists
//	std::optional<Route> bootstrap(bool& shouldContinue, const Vertex& start)
//	{
//		// Setup destination
//		m_destination = start;
//		auto optDstNode = m_navmesh.polygonAtPoint(start);
//		if (!optDstNode.has_value())
//			return std::nullopt;
//		m_destinationNode = optDstNode.value();
//
//		// Handle context reuse
//		if (m_dirty) {
//			if (m_lvl3DstNodes.empty() || trivialPathExists())
//				// Either: reachable area contains no lvl3 nodes, or dst is included in the trivial neighbourhood
//				return findTrivialPath();
//			// Update costs according to new heuristic cost
//			reorderFrontier();
//		}
//
//		// Find lvl3 nodes reachable from the dst
//		discoverLvl3DstNodes();
//		if (m_lvl3DstNodes.empty())
//			// There are no reachable lvl3 nodes
//			return findTrivialPath();
//		shouldContinue = true;
//		return std::nullopt;
//	}
//	
//	// @brief Adjust frontier order based on new h costs
//	void reorderFrontier();
//
//	bool trivialPathExists() const;
//
//	std::optional<Route> findTrivialPath();
//
//	void discoverLvl3DstNodes();
//
//	std::vector<Access> neighboursThrough(const Access& from) const
//	{
//		const auto node = m_navmesh.polygonOfEdge(from.edge);
//		if (!node.has_value())
//			return {};
//		return neighboursOf(node.value());
//	}
//
//	std::vector<Access> neighboursOf(Node node) const;
//
//private:
//	Navmesh& m_navmesh;
//	bool     m_dirty;
//
//	Vertex m_start;
//	Node   m_startNode;
//
//	Vertex m_destination;
//	Node   m_destinationNode;
//
//	set<Node>         m_lvl3DstNodes;
//	map<Node, Funnel> m_lvl3DstNodeFunnels;
//	Frontier          m_frontier;
//};
