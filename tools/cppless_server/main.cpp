#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <streambuf>
#include <unordered_map>
#include <utility>

#include <boost/container/small_vector.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nghttp2/asio_http2.h>
#include <nghttp2/asio_http2_server.h>
#include <sys/stat.h>

const int status_ok = 200;
const int status_not_found = 404;
const int status_method_not_allowed = 405;

class temporary_file
{
public:
  template<class Generator>
  explicit temporary_file(Generator& generator)
      : m_uuid(generator())
  {
    std::error_code ec;

    std::filesystem::create_directories(
        std::filesystem::temp_directory_path() / "cppless-server", ec);
    if (ec) {
      throw std::runtime_error(ec.message());
    }
    m_path = std::filesystem::temp_directory_path(ec) / "cppless-server"
        / boost::uuids::to_string(m_uuid);
    std::cout << "temporary_file: " << m_path << std::endl;
    if (ec) {
      throw std::runtime_error(ec.message());
    }
    m_stream = std::ofstream(m_path, std::ios::binary);
  }

  ~temporary_file()
  {
    m_stream.close();
    std::error_code ec;
    std::filesystem::remove(m_path, ec);
  }

  // Delete copy constructor and assignment operator.
  temporary_file(const temporary_file&) = delete;
  auto operator=(const temporary_file&) -> temporary_file& = delete;

  // Move constructor and assignment operator.
  temporary_file(temporary_file&&) = default;
  auto operator=(temporary_file&&) -> temporary_file& = default;

  auto write_stream() -> std::ofstream&
  {
    return m_stream;
  }

  auto path() const -> const std::filesystem::path&
  {
    return m_path;
  }

  auto uuid() const -> boost::uuids::uuid
  {
    return m_uuid;
  }

private:
  std::ofstream m_stream;
  boost::uuids::uuid m_uuid;
  std::filesystem::path m_path;
};

template<>
struct std::hash<boost::uuids::uuid>
{
  auto operator()(const boost::uuids::uuid& k) const -> size_t
  {
    std::hash<uint8_t> hasher;
    size_t result = 0;
    for (unsigned char i : k.data) {
      result = result * 31 + hasher(i);
    }
    return result;
  }
};

class temporary_file_collection
{
public:
  temporary_file_collection() = default;
  ~temporary_file_collection() = default;

  // Delete copy constructor and assignment operator.
  temporary_file_collection(const temporary_file_collection&) = delete;
  auto operator=(const temporary_file_collection&)
      -> temporary_file_collection& = delete;

  // Move constructor and assignment operator.
  temporary_file_collection(temporary_file_collection&&) = default;
  auto operator=(temporary_file_collection&&)
      -> temporary_file_collection& = default;

  auto create_file() -> std::shared_ptr<temporary_file>
  {
    auto file = std::make_shared<temporary_file>(m_generator);
    m_files.push_back(file);
    m_uuid_map[file->uuid()] = file;
    return file;
  }

  auto find_file(const boost::uuids::uuid& uuid)
      -> std::shared_ptr<temporary_file>
  {
    auto it = m_uuid_map.find(uuid);
    if (it == m_uuid_map.end()) {
      return nullptr;
    }
    return it->second;
  }

private:
  boost::uuids::random_generator m_generator;
  std::vector<std::shared_ptr<temporary_file>> m_files;
  std::unordered_map<boost::uuids::uuid, std::shared_ptr<temporary_file>>
      m_uuid_map;
};

template<class EOFCallback>
class ostream_cb
{
public:
  explicit ostream_cb(std::ostream& os, EOFCallback cb)
      : m_os(os)
      , m_cb(std::move(cb))
  {
  }
  void operator()(const uint8_t* data, size_t len)
  {
    std::cout << "Wrote: " << len << std::endl;
    if (len == 0) {
      m_cb();
      return;
    }
    m_os.write(reinterpret_cast<const char*>(data),  // NOLINT
               static_cast<std::streamsize>(len));
  }

private:
  std::ostream& m_os;
  EOFCallback m_cb;
};

class router
{
public:
  using request_cb =
      std::function<void(const nghttp2::asio_http2::server::request&,
                         const nghttp2::asio_http2::server::response&,
                         const std::vector<std::string>& dynamic_segments)>;

  router() = default;

  auto operator()(const nghttp2::asio_http2::server::request& req,
                  const nghttp2::asio_http2::server::response& res) -> void
  {
    std::filesystem::path path(req.uri().path,
                               std::filesystem::path::format::generic_format);
    if (path.empty()) {
      return;
    }
    std::vector<std::string> segments(++path.begin(), path.end());
    std::vector<std::string> dynamic_segments;
    std::string& last_segment = segments.back();
    node* current_node = &m_nodes[0];
    size_t index = 0;
    for (std::string& p : segments) {
      bool last = index++ == segments.size() - 1;
      if (last) {
        break;
      }

      bool found = false;
      for (const auto& child : current_node->m_children) {
        if (child.first == p) {
          current_node = &m_nodes[child.second];
          found = true;
          break;
        }
      }
      if (found) {
        continue;
      }

      if (current_node->m_wildcard_child > 0) {
        dynamic_segments.push_back(p);
        current_node = &m_nodes[current_node->m_wildcard_child];
        continue;
      }

      current_node = nullptr;
    }

    if (current_node != nullptr) {
      if (last_segment.empty() && current_node->m_collection_callback) {
        current_node->m_collection_callback(req, res, dynamic_segments);
        return;
      }
      for (const auto& child : current_node->m_element_callbacks) {
        if (child.first == last_segment) {
          child.second(req, res, dynamic_segments);
          return;
        }
      }
      if (current_node->m_wildcard_callback) {
        dynamic_segments.push_back(last_segment);
        current_node->m_wildcard_callback(req, res, dynamic_segments);
        return;
      }
    }

    res.write_head(status_not_found);
    res.end("Not Found");
  }

  auto add_handler(const std::string& pattern, const request_cb& cb) -> void
  {
    std::filesystem::path path(pattern,
                               std::filesystem::path::format::generic_format);
    if (path.empty()) {
      return;
    }
    std::vector<std::string> segments(++path.begin(), path.end());
    std::string& last_segment = segments.back();
    node* current_node = &m_nodes[0];
    size_t index = 0;
    for (std::string& p : segments) {
      bool last = index++ == segments.size() - 1;
      if (last) {
        break;
      }

      if (p.starts_with(":")) {
        if (current_node->m_wildcard_child > 0) {
          current_node = &m_nodes[current_node->m_wildcard_child];
        } else {
          current_node->m_wildcard_child = m_nodes.size();
          current_node = &m_nodes.emplace_back();
        }
      } else {
        bool found = false;
        for (const auto& child : current_node->m_children) {
          if (child.first == p) {
            current_node = &m_nodes[child.second];
            found = true;
            break;
          }
        }

        if (!found) {
          current_node->m_children.emplace_back(p, m_nodes.size());
          current_node = &m_nodes.emplace_back();
        }
      }
    }

    if (last_segment.empty()) {
      current_node->m_collection_callback = cb;
    } else if (last_segment.starts_with(":")) {
      current_node->m_wildcard_callback = cb;
    } else {
      current_node->m_element_callbacks.emplace_back(
          std::make_pair(last_segment, cb));
    }
  }

private:
  class node
  {
  public:
    constexpr static int n = 8;

    node() = default;

    // Default copy constructor and assignment operator are fine.
    node(const node&) = default;
    auto operator=(const node&) -> node& = default;

    boost::container::small_vector<std::pair<std::string, size_t>, n>
        m_children;
    size_t m_wildcard_child = 0;
    boost::container::small_vector<std::pair<std::string, request_cb>, n>
        m_element_callbacks;
    request_cb m_wildcard_callback = nullptr;
    request_cb m_collection_callback = nullptr;
  };

  std::vector<node> m_nodes {node()};
};

auto main(int /*argc*/, char* /*argv*/[]) -> int
{
  using nghttp2::asio_http2::server::http2,
      nghttp2::asio_http2::server::request,
      nghttp2::asio_http2::server::response;
  boost::system::error_code ec;
  http2 server;

  temporary_file_collection files;
  router r;

  auto fn_collection_cb =
      [&files](const request& req,
               const response& res,
               const std::vector<std::string>& /*dynamic_segments*/)
  {
    if (req.method() == "POST") {
      auto file = files.create_file();
      auto eof_cb = [&, file]()
      {
        res.write_head(status_ok);
        res.end(boost::uuids::to_string(file->uuid()));
        file->write_stream() << std::flush;
      };
      ostream_cb cb(file->write_stream(), eof_cb);
      req.on_data(cb);

    } else {
      res.write_head(status_method_not_allowed, {{"Allow", {"POST", false}}});
      res.end();
    }
  };
  r.add_handler("/functions/", fn_collection_cb);

  auto fn_element_cb =
      [&files](const request& req,
               const response& res,
               const std::vector<std::string>& dynamic_segments)
  {
    if (req.method() == "GET") {
      auto id = dynamic_segments[0];
      boost::uuids::string_generator gen;
      auto uuid = gen(id);
      auto file = files.find_file(uuid);
      if (file) {
        res.write_head(status_ok);
        res.end(nghttp2::asio_http2::file_generator(file->path()));
      } else {
        res.write_head(status_not_found);
        res.end("Not Found");
      }
    } else {
      res.write_head(status_method_not_allowed, {{"Allow", {"GET", false}}});
      res.end();
    }
  };
  r.add_handler("/functions/:function_id", fn_element_cb);

  server.handle("/", r);

  server.num_threads(4);
  if (server.listen_and_serve(ec, "localhost", "3000")) {
    std::cerr << "error: " << ec.message() << std::endl;
  }
}