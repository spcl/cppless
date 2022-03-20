#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "fixed_string.hpp"

namespace json
{
class value;

class array_value
{
public:
  constexpr array_value() = default;
  constexpr ~array_value();

  constexpr array_value(const array_value& other);
  constexpr array_value(array_value&&) = delete;

  constexpr auto operator=(const array_value& other) -> array_value&;
  constexpr auto operator=(array_value&& other) -> array_value& = delete;

  constexpr auto size() const noexcept -> std::size_t
  {
    return m_size;
  }

  constexpr void operator()(const value& v);
  constexpr auto operator[](std::size_t i) const -> const value&;

  size_t m_size = 0;
  size_t m_capacity = 0;
  value* m_data = nullptr;
};

class object_value;
class object_attribute_accessor
{
public:
  constexpr object_attribute_accessor(object_value* parent, const char* key)
      : m_parent(parent)
      , m_key(key)
  {
  }

  constexpr auto operator=(const value& v) -> object_attribute_accessor&;

private:
  object_value* m_parent;
  const char* m_key;
};

class object_value
{
  using entry = std::pair<const char*, value>;

public:
  constexpr object_value() = default;
  constexpr ~object_value();

  constexpr object_value(const object_value& other);
  constexpr object_value(object_value&&) = delete;

  constexpr auto operator=(const object_value& other) -> object_value&;
  constexpr auto operator=(object_value&& other) -> array_value& = delete;

  constexpr auto attribute(const char* k, const value& v) -> void;
  [[nodiscard]] constexpr auto operator[](const char* k)
      -> object_attribute_accessor;
  [[nodiscard]] constexpr auto size() const noexcept -> std::size_t
  {
    return m_size;
  }
  [[nodiscard]] constexpr auto key(std::size_t i) const noexcept -> const char*;
  [[nodiscard]] constexpr auto value(std::size_t i) const noexcept
      -> const value&;

  size_t m_size = 0;
  size_t m_capacity = 0;
  entry* m_data = nullptr;
};

enum class value_kind
{
  null,
  boolean,
  number,
  string,
  array,
  object,
};

union value_storage
{
  double d;
  const char* s;
  array_value a;
  object_value o;

  constexpr value_storage()
      : d(0) {};
  constexpr explicit value_storage(double d)
      : d(d)
  {
  }
  constexpr explicit value_storage(const char* s)
      : s(s)
  {
  }
  constexpr explicit value_storage(const array_value& a)
      : a(a)
  {
  }
  constexpr explicit value_storage(const object_value& o)
      : o(o)
  {
  }
  constexpr ~value_storage();

  // Copy
  constexpr value_storage(const value_storage& v) = delete;
  // Move
  constexpr value_storage(value_storage&& v) = delete;

  // Copy assignment
  constexpr auto operator=(const value_storage& v) -> value_storage& = delete;
  // Move assignment
  constexpr auto operator=(value_storage&& v) -> value_storage& = delete;
};

class value
{
public:
  constexpr value() noexcept = default;
  constexpr value(double d) noexcept
      : m_kind(value_kind::number)
      , m_storage(d)
  {
  }
  constexpr value(const char* s) noexcept
      : m_kind(value_kind::string)
      , m_storage(s)
  {
  }
  constexpr value(const array_value& a) noexcept
      : m_kind(value_kind::array)
      , m_storage(a)
  {
  }
  constexpr value(const object_value& o) noexcept
      : m_kind(value_kind::object)
      , m_storage(o)
  {
  }

  constexpr ~value();

  // Copy
  constexpr value(const value& other);
  // Move
  constexpr value(value&& v) = delete;

  // Copy assignment
  constexpr auto operator=(const value& other) -> value&;
  // Move assignment
  constexpr auto operator=(value&& v) noexcept -> value& = delete;

  [[nodiscard]] constexpr auto is_null() const noexcept -> bool
  {
    return m_kind == value_kind::null;
  }

  [[nodiscard]] constexpr auto is_object() const noexcept -> bool
  {
    return m_kind == value_kind::object;
  }

  [[nodiscard]] constexpr auto is_array() const noexcept -> bool
  {
    return m_kind == value_kind::array;
  }

  [[nodiscard]] constexpr auto is_string() const noexcept -> bool
  {
    return m_kind == value_kind::string;
  }

  [[nodiscard]] constexpr auto is_number() const noexcept -> bool
  {
    return m_kind == value_kind::number;
  }

  [[nodiscard]] constexpr auto as_object() const noexcept -> const object_value&
  {
    return m_storage.o;
  }

  [[nodiscard]] constexpr auto as_array() const noexcept -> const array_value&
  {
    return m_storage.a;
  }

  [[nodiscard]] constexpr auto as_string() const noexcept -> const char*
  {
    return m_storage.s;
  }

  [[nodiscard]] constexpr auto as_number() const noexcept -> double
  {
    return m_storage.d;
  }

  [[nodiscard]] constexpr auto kind() const noexcept -> value_kind
  {
    return m_kind;
  }

private:
  value_kind m_kind = value_kind::null;
  value_storage m_storage;
};

constexpr inline auto object_attribute_accessor::operator=(const value& v)
    -> object_attribute_accessor&
{
  m_parent->attribute(m_key, v);
  return *this;
}

constexpr inline array_value::array_value(const array_value& other)
    : m_size(other.m_size)
    , m_capacity(other.m_capacity)
    , m_data(new value[other.m_capacity])
{
  for (size_t i = 0; i < m_size; i++) {
    m_data[i] = other.m_data[i];
  }
}

constexpr array_value::~array_value()
{
  delete[] m_data;
}

constexpr inline auto array_value::operator=(const array_value& other)
    -> array_value&
{
  if (this == &other)
    return *this;
  m_size = other.m_size;
  m_capacity = other.m_capacity;

  delete[] m_data;
  m_data = new value[m_capacity];
  for (size_t i = 0; i < m_size; i++) {
    m_data[i] = other.m_data[i];
  }
  return *this;
}

constexpr inline void array_value::operator()(const value& v)
{
  if (m_size >= m_capacity) {
    m_capacity = (m_size + 1) * 3 / 2;
    auto* new_data = new value[m_capacity];
    for (size_t i = 0; i < m_size; i++) {
      new_data[i] = m_data[i];
    }
    delete[] m_data;
    m_data = new_data;  // NOLINT
  }
  m_data[m_size++] = v;  // NOLINT
}

constexpr inline auto array_value::operator[](size_t i) const -> const value&
{
  return m_data[i];
}

constexpr inline object_value::object_value(const object_value& other)
    : m_size(other.m_size)
    , m_capacity(other.m_capacity)
    , m_data(new entry[m_capacity])
{
  for (size_t i = 0; i < m_size; i++) {
    m_data[i] = other.m_data[i];
  }
}

constexpr inline object_value::~object_value()
{
  delete[] m_data;
}

constexpr inline auto object_value::operator=(const object_value& other)
    -> object_value&
{
  if (this == &other)
    return *this;
  m_size = other.m_size;
  m_capacity = other.m_capacity;

  delete[] m_data;
  m_data = new entry[m_capacity];
  for (size_t i = 0; i < m_size; i++) {
    m_data[i] = other.m_data[i];
  }
  return *this;
}

constexpr inline void object_value::attribute(const char* k,
                                              const class value& v)
{
  if (m_size >= m_capacity) {
    m_capacity = (m_size + 1) * 3 / 2;
    auto* new_data = new entry[m_capacity];
    for (size_t i = 0; i < m_size; i++) {
      new_data[i] = m_data[i];
    }
    delete[] m_data;
    m_data = new_data;  // NOLINT
  }
  m_data[m_size++] = {k, v};  // NOLINT
}

constexpr inline auto object_value::operator[](const char* k)
    -> object_attribute_accessor
{
  return {this, k};
}

constexpr inline auto object_value::key(std::size_t i) const noexcept -> const
    char*
{
  return m_data[i].first;
}

constexpr inline auto object_value::value(std::size_t i) const noexcept -> const
    class value&
{
  return m_data[i].second;
}

constexpr value_storage::~value_storage()  // NOLINT
{
  // Do nothing
}

constexpr inline value::value(const value& other)
    : m_kind(other.m_kind)
{
  if (m_kind == value_kind::null) {
    // Do nothing
  } else if (m_kind == value_kind::number) {
    m_storage.d = other.m_storage.d;
  } else if (m_kind == value_kind::string) {
    m_storage.s = other.m_storage.s;
  } else if (m_kind == value_kind::array) {
    m_storage.a = other.m_storage.a;
  } else if (m_kind == value_kind::object) {
    m_storage.o = other.m_storage.o;
  }
}

constexpr inline value::~value()
{
  if (m_kind == value_kind::array) {
    m_storage.a.~array_value();  // NOLINT
  } else if (m_kind == value_kind::object) {
    m_storage.o.~object_value();  // NOLINT
  }
}

constexpr inline auto value::operator=(const value& other) -> value&
{
  if (this != &other) {
    if (m_kind == value_kind::array) {
      m_storage.a.~array_value();  // NOLINT
    } else if (m_kind == value_kind::object) {
      m_storage.o.~object_value();  // NOLINT
    }

    m_kind = other.m_kind;
    if (m_kind == value_kind::number) {
      m_storage.d = other.m_storage.d;
    } else if (m_kind == value_kind::string) {
      m_storage.s = other.m_storage.s;
    } else if (m_kind == value_kind::array) {
      // new (&m_storage.a) array_value();
      m_storage.a = array_value {};
      m_storage.a = other.m_storage.a;
    } else if (m_kind == value_kind::object) {
      // new (&m_storage.a) object_value();
      m_storage.o = object_value {};
      m_storage.o = other.m_storage.o;
    }
  }
  return *this;
}

template<class L>
constexpr auto array(L l) -> value
{
  array_value a;
  l(a);
  return value {a};
}

template<class L>
constexpr auto object(L l) -> value
{
  object_value o;
  l(o);
  return value {o};
}

constexpr auto null = value {};

namespace serialization
{

class string_builder
{
public:
  constexpr string_builder() = default;
  constexpr ~string_builder()
  {
    delete[] m_data;
  }

  constexpr string_builder(const string_builder&) = delete;
  constexpr string_builder(string_builder&& other) noexcept
      : m_data(other.m_data)
      , m_size(other.m_size)
      , m_capacity(other.m_capacity)
  {
    other.m_data = nullptr;
  }

  constexpr auto operator=(const string_builder&) -> string_builder& = delete;
  constexpr auto operator=(string_builder&&) -> string_builder& = delete;

  constexpr auto push_back(char c) -> void;
  [[nodiscard]] constexpr auto size() const noexcept -> size_t
  {
    return m_size;
  }
  [[nodiscard]] constexpr auto data() const noexcept -> const char*
  {
    return m_data;
  }

private:
  char* m_data = nullptr;
  size_t m_size = 0;
  size_t m_capacity = 0;
};

constexpr auto string_builder::push_back(char c) -> void
{
  if (m_size >= m_capacity) {
    m_capacity = (m_size + 1) * 3 / 2;
    auto* new_data = new char[m_capacity];  // NOLINT
    for (size_t i = 0; i < m_size; i++) {
      new_data[i] = m_data[i];  // NOLINT
    }
    delete[] m_data;
    m_data = new_data;
  }
  m_data[m_size++] = c;  // NOLINT
}

constexpr auto encode_string(const char* s, string_builder& b) -> void
{
  b.push_back('"');
  for (const char* c = s; *c != 0; c++) {
    if (*c == '"') {
      b.push_back('\\');
      b.push_back('"');
    } else if (*c == '\\') {
      b.push_back('\\');
      b.push_back('\\');
    } else if (*c == '\b') {
      b.push_back('\\');
      b.push_back('b');
    } else if (*c == '\f') {
      b.push_back('\\');
      b.push_back('f');
    } else if (*c == '\n') {
      b.push_back('\\');
      b.push_back('n');
    } else if (*c == '\r') {
      b.push_back('\\');
      b.push_back('r');
    } else if (*c == '\t') {
      b.push_back('\\');
      b.push_back('t');
    } else if (*c < 0x20) {
      b.push_back('\\');
      b.push_back('u');
      b.push_back('0');
      b.push_back('0');
      b.push_back('0');
      b.push_back('0' + static_cast<char>(*c / 16));
      b.push_back('0' + static_cast<char>(*c % 16));
    } else {
      b.push_back(*c);
    }
  }
  b.push_back('"');
}

constexpr auto encode_number(double d, string_builder& b) -> void
{
  b.push_back('0');
}

constexpr auto encode_value(const value& v, string_builder& b) -> void
{
  if (v.is_null()) {
    b.push_back('n');
    b.push_back('u');
    b.push_back('l');
    b.push_back('l');
  } else if (v.is_number()) {
    encode_number(v.as_number(), b);
  } else if (v.is_string()) {
    encode_string(v.as_string(), b);
  } else if (v.is_array()) {
    auto a = v.as_array();
    b.push_back('[');
    for (size_t i = 0; i < a.size(); i++) {
      if (i != 0) {
        b.push_back(',');
      }
      encode_value(a[i], b);
    }
    b.push_back(']');
  } else if (v.is_object()) {
    auto o = v.as_object();
    b.push_back('{');
    for (size_t i = 0; i < o.size(); i++) {
      if (i != 0) {
        b.push_back(',');
      }
      encode_string(o.key(i), b);
      b.push_back(':');
      encode_value(o.value(i), b);
    }
    b.push_back('}');
  }
}

constexpr auto encode_to_sb(const value& v) -> string_builder
{
  string_builder b;
  encode_value(v, b);
  return b;
}

constexpr auto encode(const value& v)
{
  auto sb = encode_to_sb(v);
  return sb.data();
}

}  // namespace serialization

}  // namespace json

constexpr inline auto test()
{
  using namespace json;  // NOLINT
  constexpr auto v = array(
      [](array_value& a)
      {
        a(null);
        a(12);
        a(object([](object_value& o) { o["test"] = 123; }));
      });
  return serialization::encode(v);
}

auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  auto m = test();
}
