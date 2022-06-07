#pragma once
#include <chrono>
#include <memory>
#include <optional>
#include <ratio>
#include <string>
#include <unordered_map>
#include <vector>

#include <cppless/utils/json.hpp>
#include <nlohmann/json.hpp>

namespace cppless
{

struct tracing_span
{
  std::string operation_name;
  std::chrono::steady_clock::time_point start_time;
  std::chrono::steady_clock::time_point end_time;
  std::unordered_map<std::string, std::string> tags;
  unsigned long parent;
  bool inline_children;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(operation_name, start_time, end_time, tags, parent, inline_children);
  }
};

class tracing_span_ref;

class tracing_span_container
{
public:
  [[nodiscard]] auto spans() const -> const std::vector<tracing_span>&
  {
    return m_spans;
  }

  [[nodiscard]] auto spans() -> std::vector<tracing_span>&
  {
    return m_spans;
  }

  [[nodiscard]] auto span(unsigned long id) -> tracing_span&
  {
    return m_spans[id];
  }

  [[nodiscard]] auto create_root(std::string operation_name)
      -> tracing_span_ref;

  [[nodiscard]] auto create_child(unsigned long parent,
                                  std::string operation_name)
      -> tracing_span_ref;

  [[nodiscard]] auto concat(
      const tracing_span_container& other,
      std::chrono::duration<long long, std::nano> clock_diff) -> unsigned long
  {
    auto offset = m_spans.size();
    m_spans.insert(m_spans.end(), other.m_spans.begin(), other.m_spans.end());
    for (unsigned long i = offset; i < m_spans.size(); ++i) {
      m_spans[i].parent += offset;
      m_spans[i].start_time += clock_diff;
      m_spans[i].end_time += clock_diff;
    }
    return offset;
  }

  template<class Archive>
  void serialize(Archive& archive)
  {
    archive(m_spans);
  }

private:
  std::vector<tracing_span> m_spans;
};

class tracing_span_ref
{
public:
  explicit tracing_span_ref(tracing_span_container& container,
                            unsigned long index)
      : m_container(container)
      , m_index(index)
  {
  }

  // Destructor
  ~tracing_span_ref() = default;

  // Copy constructor
  tracing_span_ref(const tracing_span_ref& other) = default;

  // Delete copy assignment operator
  auto operator=(const tracing_span_ref& other) -> tracing_span_ref& = delete;

  // Move constructor
  tracing_span_ref(tracing_span_ref&& other) = default;

  // Delete move assignment operator
  auto operator=(tracing_span_ref&& other) -> tracing_span_ref& = delete;

  [[nodiscard]] auto id() const -> unsigned long
  {
    return m_index;
  }

  auto start() -> tracing_span_ref
  {
    m_container.span(m_index).start_time = std::chrono::steady_clock::now();
    return *this;
  }

  auto end() -> tracing_span_ref
  {
    m_container.span(m_index).end_time = std::chrono::steady_clock::now();
    return *this;
  }

  auto inline_children() -> tracing_span_ref
  {
    m_container.span(m_index).inline_children = true;
    return *this;
  }

  auto set_tag(std::string key, std::string value) -> void
  {
    m_container.span(m_index).tags[key] = value;
  }

  [[nodiscard]] auto parent() const -> tracing_span_ref
  {
    return tracing_span_ref(m_container, m_container.span(m_index).parent);
  }

  [[nodiscard]] auto tags() const
      -> const std::unordered_map<std::string, std::string>&
  {
    return m_container.span(m_index).tags;
  }

  [[nodiscard]] auto create_child(std::string operation_name)
      -> tracing_span_ref
  {
    return m_container.create_child(m_index, std::move(operation_name));
  }

  auto insert(const tracing_span_container& other,
              unsigned long root_id,
              std::chrono::duration<long long, std::nano> clock_diff)
      -> tracing_span_ref
  {
    auto offset = m_container.concat(other, clock_diff);
    m_container.span(root_id + offset).parent = m_index;
    return tracing_span_ref(m_container, root_id + offset);
  }

private:
  tracing_span_container& m_container;
  unsigned long m_index;
};

class scoped_tracing_span
{
public:
  scoped_tracing_span(std::optional<tracing_span_ref> parent,
                      std::string operation_name)
  {
    if (parent) {
      m_ref.emplace(parent->create_child(operation_name).start());
    }
  }

  // Default copy constructor
  scoped_tracing_span(const scoped_tracing_span& other) = default;
  // Delete copy assignment
  auto operator=(const scoped_tracing_span& other)
      -> scoped_tracing_span& = delete;

  // Default move constructor
  scoped_tracing_span(scoped_tracing_span&& other) = default;
  // Delete move assignment
  auto operator=(scoped_tracing_span&& other) -> scoped_tracing_span& = delete;

  ~scoped_tracing_span()
  {
    if (m_ref) {
      m_ref->end();
    }
  }

private:
  std::optional<tracing_span_ref> m_ref;
};

inline auto tracing_span_container::create_root(std::string operation_name)
    -> tracing_span_ref
{
  unsigned long id = m_spans.size();
  auto& span = m_spans.emplace_back();
  span.operation_name = operation_name;
  span.parent = id;
  return tracing_span_ref {*this, id};
}

inline auto tracing_span_container::create_child(unsigned long parent,
                                                 std::string operation_name)
    -> tracing_span_ref
{
  unsigned long id = m_spans.size();
  auto& span = m_spans.emplace_back();
  span.operation_name = operation_name;
  span.parent = parent;
  return tracing_span_ref {*this, id};
}

// Serializer for tracing_span
inline void to_json(nlohmann::json& j, const tracing_span& span)
{
  j = nlohmann::json {
      {"name", span.operation_name},
      {"start_time", span.start_time},
      {"end_time", span.end_time},
      {"tags", span.tags},
      {"parent", span.parent},
      {"inline_children", span.inline_children},
  };
}

// Serializer for tracing_span_container
inline void to_json(nlohmann::json& j, const tracing_span_container& span)
{
  j = nlohmann::json {
      {"spans", span.spans()},
  };
}

}  // namespace cppless