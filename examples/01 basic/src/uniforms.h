#pragma once

#include <devkit/gfx/shader.h>

#include <array>
#include <cfloat>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <rfl.hpp>

namespace gui_uniform {

template <typename T>
struct Base {
	using OptionalDefault = rfl::Rename<"default", std::optional<T>>;
	std::string       name;
	OptionalDefault default_value = std::nullopt;

	T defaultValue() const
	{
		return default_value.get().value_or(T{});
	}
};

template <typename T, ImGuiDataType ImGuiType>
bool imguiEditScalar(const char* label, T& value, const std::array<T, 2>& range, float step)
{
	return ImGui::DragScalar(label, ImGuiType, &value, step, &range[0], &range[1]);
}

template <int Components>
bool imguiEditVector(const char* label, glm::vec<Components, float>& value,
	const std::array<float, 2>& range, float step)
{
	return ImGui::DragScalarN(label, ImGuiDataType_Float, glm::value_ptr(value), Components,
		step, &range[0], &range[1], "%.3f");
}

template <int Components>
bool imguiEditMatrix(const char* label, glm::mat<Components, Components, float>& value,
	const std::array<float, 2>& range, float step)
{
	bool changed = false;
	ImGui::PushID(label);
	for (int column = 0; column < Components; ++column) {
		ImGui::PushID(column);
		ImGui::Text("%s column %d", label, column);
		ImGui::SameLine();
		changed |= ImGui::DragScalarN("##values", ImGuiDataType_Float,
			glm::value_ptr(value[column]), Components, step, &range[0], &range[1], "%.3f");
		ImGui::PopID();
	}
	ImGui::PopID();
	return changed;
}

#define DECL_GUI_UNIFORM_SCALAR_TYPE(type_name, type, display_name, imgui_type) \
struct type_name {                                                              \
	using Tag = rfl::Literal<display_name>;                                       \
	using Value = type;                                                           \
	rfl::Flatten<Base<Value>> base;                                               \
	std::array<Value, 2> range{};                                                 \
	float step = 1.f;                                                             \
	Value defaultValue() const { return base.get().defaultValue(); }              \
	const std::string& name() const { return base.get().name; }                    \
	bool imgui_edit(const char* label, Value& value) const                        \
	{ return imguiEditScalar<Value, imgui_type>(label, value, range, step); }     \
};                                                                              \
/* end of macro */

DECL_GUI_UNIFORM_SCALAR_TYPE(IntType, int, "int", ImGuiDataType_S32);
DECL_GUI_UNIFORM_SCALAR_TYPE(FloatType, float, "float", ImGuiDataType_Float);

struct BoolType {
	using Tag = rfl::Literal<"bool">;
	using Value = bool;
	rfl::Flatten<Base<Value>> base;

	Value defaultValue() const { return base.get().defaultValue(); }
	const std::string& name() const { return base.get().name; }

	bool imgui_edit(const char* label, Value& value) const
	{
		return ImGui::Checkbox(label, &value);
	}
};

#define DECL_GUI_UNIFORM_VECTOR_TYPE(type_name, type, display_name, components) \
struct type_name {                                                              \
	using Tag = rfl::Literal<display_name>;                                       \
	using Value = type;                                                           \
	rfl::Flatten<Base<Value>> base;                                               \
	std::array<float, 2> range{ -FLT_MAX, FLT_MAX };                              \
	float step = 0.01f;                                                           \
	Value defaultValue() const { return base.get().defaultValue(); }              \
	const std::string& name() const { return base.get().name; }                   \
	bool imgui_edit(const char* label, Value& value) const                        \
	{ return imguiEditVector<components>(label, value, range, step); }            \
};                                                                              \
/* end of macro */

DECL_GUI_UNIFORM_VECTOR_TYPE(Vec2Type, glm::vec2, "vec2", 2);
DECL_GUI_UNIFORM_VECTOR_TYPE(Vec3Type, glm::vec3, "vec3", 3);
DECL_GUI_UNIFORM_VECTOR_TYPE(Vec4Type, glm::vec4, "vec4", 4);

#define DECL_GUI_UNIFORM_MATRIX_TYPE(type_name, type, display_name, components) \
struct type_name {                                                              \
	using Tag = rfl::Literal<display_name>;                                       \
	using Value = type;                                                           \
	rfl::Flatten<Base<Value>> base;                                               \
	std::array<float, 2> range{ -FLT_MAX, FLT_MAX };                              \
	float step = 0.01f;                                                           \
	Value defaultValue() const { return base.get().defaultValue(); }              \
	const std::string& name() const { return base.get().name; }                   \
	bool imgui_edit(const char* label, Value& value) const                        \
	{ return imguiEditMatrix<components>(label, value, range, step); }            \
};                                                                              \
/* end of macro */

DECL_GUI_UNIFORM_MATRIX_TYPE(Mat3Type, glm::mat3, "mat3", 3);
DECL_GUI_UNIFORM_MATRIX_TYPE(Mat4Type, glm::mat4, "mat4", 4);

#undef DECL_GUI_UNIFORM_MATRIX_TYPE
#undef DECL_GUI_UNIFORM_VECTOR_TYPE

} // namespace gui_uniform

using GuiUniform = rfl::TaggedUnion<"type",
	gui_uniform::IntType,
	gui_uniform::FloatType,
	gui_uniform::BoolType,
	gui_uniform::Vec2Type,
	gui_uniform::Vec3Type,
	gui_uniform::Vec4Type,
	gui_uniform::Mat3Type,
	gui_uniform::Mat4Type
>;

class UniformValueBase {
public:
	virtual void bind_to(const std::string& uniform, dk::gfx::Shader& shader) const = 0;
	virtual void imgui_edit(const char* label) = 0;
	virtual ~UniformValueBase() = default;

	template <typename ImGuiEditConfig>
	static std::unique_ptr<UniformValueBase> create_gui_editable(const ImGuiEditConfig& config);

	template <typename T>
	static std::unique_ptr<UniformValueBase> create_builtin(T value);
};

template <typename T, typename ImGuiEditConfig>
class GuiUniformValue final : public UniformValueBase {
public:
	explicit GuiUniformValue(const ImGuiEditConfig& config)
		: m_value(config.defaultValue()), m_config(config)
	{ }

	void bind_to(const std::string& uniform, dk::gfx::Shader& shader) const override
	{
		shader.uniforms().set(uniform, m_value);
	}

	void imgui_edit(const char* label) override
	{
		m_config.imgui_edit(label, m_value);
	}

private:
	T               m_value;
	ImGuiEditConfig m_config;
};

template <typename T>
class BuiltinUniformValue final : public UniformValueBase {
public:
	explicit BuiltinUniformValue(T value)
		: m_value(std::move(value))
	{ }

	void bind_to(const std::string& uniform, dk::gfx::Shader& shader) const override
	{
		shader.uniforms().set(uniform, m_value);
	}

	void imgui_edit(const char*) override { }

private:
	T m_value;
};

template <typename ImGuiEditConfig>
std::unique_ptr<UniformValueBase> UniformValueBase::create_gui_editable(const ImGuiEditConfig& config)
{
	using Value = typename ImGuiEditConfig::Value;
	return std::make_unique<GuiUniformValue<Value, ImGuiEditConfig>>(config);
}

template <typename T>
std::unique_ptr<UniformValueBase> UniformValueBase::create_builtin(T value)
{
	return std::make_unique<BuiltinUniformValue<T>>(std::move(value));
}
