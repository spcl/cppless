#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>

#include "../../include/simplified_benchmark.hpp"
#include <cppless/dispatcher/common.hpp>

#include <argparse/argparse.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

class some_data
{
public:
  unsigned int x;
  unsigned int y;
  std::string q;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(x, y, q);
  }

  bool operator==(const some_data & other) const
  {
    return x == other.x && y == other.y && q == other.q;
  }
};

template<typename T>
std::vector<double> benchmark_binary_encode(int repetitions, bool flush_cache, std::vector<T>& input)
{
  // Encode binary
  std::string out;
  out.resize(sizeof(T)*input.size() + 128);

  auto times = benchmark::microbenchmark(
    repetitions, flush_cache,
    [&]() {
      boost::interprocess::bufferstream stream(out.data(), out.size());
      {
        cereal::BinaryOutputArchive oar(stream);
        oar(input);
      }
      benchmark::do_not_optimize(out);
    },
    reinterpret_cast<char*>(input.data()),
    sizeof(T)*input.size(),
    reinterpret_cast<char*>(out.data()),
    sizeof(char)*out.size()
  );

  return times;
}

template<typename T>
std::vector<double> benchmark_binary_decode(int repetitions, bool flush_cache, std::vector<T>& input)
{
  std::stringstream ss;
  {
    cereal::BinaryOutputArchive oar(ss);
    oar(input);
  }
  auto encoded = ss.str();

  std::vector<T> decoded;
  decoded.reserve(input.size());

  auto times = benchmark::microbenchmark(
    repetitions, flush_cache,
    [&]() {

      boost::iostreams::stream<boost::iostreams::array_source> stream(
        encoded.c_str(), encoded.size()
      );
      {
        cereal::BinaryInputArchive iar(stream);
        iar(decoded);
      }
      benchmark::do_not_optimize(decoded);
    },
    reinterpret_cast<char*>(encoded.data()),
    sizeof(char)*encoded.size(),
    reinterpret_cast<char*>(decoded.data()),
    sizeof(T)*input.size()
  );

  bool equal = std::equal(input.begin(), input.end(), decoded.begin());
  if(!equal) {
    std::cerr << "Incorrect result of binary decode!" << std::endl;
  }

  return times;
}

template<typename T>
std::vector<double> benchmark_binary_json_encode(int repetitions, bool flush_cache, std::vector<T>& input)
{
  // Encode binary
  std::string out;
  out.resize(sizeof(T)*input.size() + 128);

  // Overapproximation to allocate data
  auto encoded_size = boost::beast::detail::base64::encoded_size(out.size());
  std::string encoded;
  encoded.resize(encoded_size + 2);

  // First serialize, then base64 encode
  auto times = benchmark::microbenchmark(
    repetitions, flush_cache,
    [&]() {
      boost::interprocess::bufferstream stream(out.data(), out.size());
      {
        cereal::BinaryOutputArchive oar(stream);
        oar(input);
      }
      encoded[0] = '"';
      boost::beast::detail::base64::encode(encoded.data() + 1, out.data(), out.size());
      encoded[encoded_size + 1] = '"';
      benchmark::do_not_optimize(encoded);
    },
    reinterpret_cast<char*>(input.data()),
    sizeof(T)*input.size(),
    reinterpret_cast<char*>(out.data()),
    sizeof(char)*out.size(),
    reinterpret_cast<char*>(encoded.data()),
    sizeof(char)*encoded.size()
  );

  return times;
}

template<typename T>
std::vector<double> benchmark_binary_json_decode(int repetitions, bool flush_cache, std::vector<T>& input)
{
  // Serialize data
  std::stringstream ss;
  {
    cereal::BinaryOutputArchive oar(ss);
    oar(input);
  }
  auto serialized = ss.str();

  // Encode input into base64
  auto encoded_size = boost::beast::detail::base64::encoded_size(serialized.size());
  std::string encoded;
  encoded.resize(encoded_size + 2);
  encoded[0] = '"';
  boost::beast::detail::base64::encode(encoded.data() + 1, serialized.data(), serialized.size());
  encoded[encoded_size + 1] = '"';

  // Preallocate memory for decoded string
  unsigned int size = encoded.size();
  auto decoded_size = boost::beast::detail::base64::decoded_size(size - 2);
  std::string decoded;
  decoded.resize(decoded_size);

  // Preallocate memory for result
  std::vector<T> deserialized;
  deserialized.reserve(input.size());

  // First base64 decode, then serialize
  auto times = benchmark::microbenchmark(
    repetitions, flush_cache,
    [&]() {

      boost::beast::detail::base64::decode(
        decoded.data(), encoded.data() + 1, encoded.size() - 2
      );

      boost::iostreams::stream<boost::iostreams::array_source> stream(
        decoded.c_str(), decoded.size()
      );
      {
        cereal::BinaryInputArchive iar(stream);
        iar(deserialized);
      }
      benchmark::do_not_optimize(deserialized);
    },
    reinterpret_cast<char*>(encoded.data()),
    sizeof(char)*encoded.size(),
    reinterpret_cast<char*>(deserialized.data()),
    sizeof(T)*input.size(),
    reinterpret_cast<char*>(decoded.data()),
    sizeof(char)*decoded.size()
  );

  bool equal = std::equal(input.begin(), input.end(), deserialized.begin());
  if(!equal) {
    std::cerr << "Incorrect result of binary decode!" << std::endl;
  }

  return times;
}

template<typename T>
std::vector<double> benchmark_json_encode(int repetitions, bool flush_cache, std::vector<T>& input)
{
  // Encode binary
  // Overapproximate memory size
  std::string out;
  {
    std::stringstream ss;
    cereal::JSONOutputArchive oar(ss, cereal::JSONOutputArchive::Options::NoIndent());
    oar(input);
    out = ss.str();
  }

  // First serialize, then base64 encode
  auto times = benchmark::microbenchmark(
    repetitions, flush_cache,
    [&]() {
      boost::interprocess::bufferstream stream(out.data(), out.size());
      {
        cereal::JSONOutputArchive oar(stream, cereal::JSONOutputArchive::Options::NoIndent());
        oar(input);
      }
      benchmark::do_not_optimize(out);
    },
    reinterpret_cast<char*>(input.data()),
    sizeof(T)*input.size(),
    reinterpret_cast<char*>(out.data()),
    sizeof(char)*out.size()
  );

  return times;
}

template<typename T>
std::vector<double> benchmark_json_decode(int repetitions, bool flush_cache, std::vector<T>& input)
{
  // Serialize data
  std::string serialized;
  {
    std::stringstream ss;
    {
      cereal::JSONOutputArchive oar(ss, cereal::JSONOutputArchive::Options::NoIndent());
      oar(input);
    }
    serialized = ss.str();
  }

  // Preallocate memory for result
  std::vector<T> deserialized;
  deserialized.reserve(input.size());

  // First base64 decode, then serialize
  auto times = benchmark::microbenchmark(
    repetitions, flush_cache,
    [&]() {

      boost::iostreams::stream<boost::iostreams::array_source> stream(
        serialized.c_str(), serialized.size()
      );
      {
        cereal::JSONInputArchive iar(stream);
        iar(deserialized);
      }
      benchmark::do_not_optimize(deserialized);
    },
    reinterpret_cast<char*>(serialized.data()),
    sizeof(char)*serialized.size(),
    reinterpret_cast<char*>(deserialized.data()),
    sizeof(T)*input.size()
  );

  bool equal = std::equal(input.begin(), input.end(), deserialized.begin());
  if(!equal) {
    std::cerr << "Incorrect result of binary decode!" << std::endl;
  }

  return times;
}

template<typename T>
std::vector<double> run_benchmark(
  int repetitions, bool flush_cache,
  const std::string& scenario, T && t
)
{
  if(scenario == "binary-encode") {
    return benchmark_binary_encode(
      repetitions, flush_cache,
      std::forward<T>(t)
    );
  } else if(scenario == "binary-decode") {
    return benchmark_binary_decode(
      repetitions, flush_cache,
      std::forward<T>(t)
    );
  } else if(scenario == "binary-json-encode") {
    return benchmark_binary_json_encode(
      repetitions, flush_cache,
      std::forward<T>(t)
    );
  } else if(scenario == "binary-json-decode") {
    return benchmark_binary_json_decode(
      repetitions, flush_cache,
      std::forward<T>(t)
    );
  } else if(scenario == "json-encode") {
    return benchmark_json_encode(
      repetitions, flush_cache,
      std::forward<T>(t)
    );
  } else if(scenario == "json-decode") {
    return benchmark_json_decode(
      repetitions, flush_cache,
      std::forward<T>(t)
    );
  } else {
    std::cerr << "Unknown scenario " << scenario << std::endl;
    exit(1);
  }
}

auto main(int argc, char* argv[]) -> int
{
  int input_size;
  bool flush_cache;
  int repetitions;
  std::string input;
  std::string scenario;
  std::string output;
  argparse::ArgumentParser program("serialization");
  benchmark::parse_args(program, argc, argv, input_size, flush_cache, repetitions, input, scenario, output);

  std::vector<double> time_results;
  size_t data_size = 0;

  if(input == "integers") {

    std::vector<uint32_t> data;
    data_size = sizeof(uint32_t);
    const auto size = input_size;
    for (unsigned int i = 0; i < size; i++) {
      data.push_back(i);
    }

    time_results = run_benchmark(repetitions, flush_cache, scenario, data);

  } else if(input == "structures") {

    std::vector<some_data> data;
    data_size = sizeof(some_data);
    const auto size = input_size;
    std::string test_string{42, 'c'};
    for (unsigned int i = 0; i < size; i++) {
      data.push_back(some_data{i, i+1, test_string});
    }

    time_results = run_benchmark(repetitions, flush_cache, scenario, data);

  } else {
    std::cerr << "Unknown input " << input << std::endl;
    exit(1);
  }

  {
    std::ofstream output_file{output, std::ios::out};
    output_file << "repetition,time" << std::endl;
    for(int i = 1; i < time_results.size(); ++i)
      output_file << time_results[i] << std::endl;
  }

  auto sum = std::accumulate(time_results.begin() + 1, time_results.end(), 0.0);
  auto avg = sum / repetitions / 1000.0;
  std::clog << "Average time: " << avg << " [us]" << std::endl;

  // Megabytes per second 
  auto total_size = data_size * input_size / 1000.0 / 1000.0;
  std::clog << "Average BW: " << total_size / (avg / 1000.0 / 1000.0) << " [MB/s]" << std::endl;

  //benchmark::benchmark("encode / binary_json") = [&](auto body)
  //{
  //  body = [&]
  //  {
  //    auto encoded = cppless::json_binary_archive::serialize(something);
  //    benchmark::do_not_optimize(encoded);
  //  };
  //};
  //benchmark::benchmark("decode / binary_json") = [&](auto body)
  //{
  //  auto encoded = cppless::json_binary_archive::serialize(something);
  //  body = [&]
  //  {
  //    //std::vector<some_data> decoded;
  //    std::vector<uint8_t> decoded;
  //    cppless::json_binary_archive::deserialize(encoded, decoded);
  //    benchmark::do_not_optimize(decoded);
  //  };
  //};

  //benchmark::benchmark("encode / json") = [&](auto body)
  //{
  //  body = [&]
  //  {
  //    auto encoded = cppless::json_structured_archive::serialize(something);
  //    benchmark::do_not_optimize(encoded);
  //  };
  //};
  //benchmark::benchmark("decode / json") = [&](auto body)
  //{
  //  auto encoded = cppless::json_structured_archive::serialize(something);
  //  body = [&]
  //  {
  //    //std::vector<some_data> decoded;
  //    std::vector<uint8_t> decoded;

  //    cppless::json_structured_archive::deserialize(encoded, decoded);
  //    benchmark::do_not_optimize(decoded);
  //  };
  //};
}
