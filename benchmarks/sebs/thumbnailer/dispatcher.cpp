#include <fstream>
#include <random>
#include <vector>

#include <cppless/dispatcher/aws-lambda.hpp>
#include <cereal/types/vector.hpp>

#include "function.hpp"

using dispatcher = cppless::aws_lambda_nghttp2_dispatcher<>::from_env;

auto main(int argc, char* argv[]) -> int
{
  std::ifstream input(argv[1], std::ios::binary | std::ios::ate);

  if(input.fail())
  {
    std::cerr << "Couldnt read file " << argv[1] << std::endl;
    return 1;
  }

  std::streamsize size = input.tellg();
  input.seekg(0, std::ios::beg);
  std::vector<unsigned char> vectordata;
  vectordata.resize(size);

  if (!input.read((char*)vectordata.data(), size))
  {
    std::cerr << "Couldnt read file " << argv[1] << std::endl;
    return 1;
  }

  std::cerr << "Input size of " << size << " " << vectordata.size() << std::endl;

  dispatcher aws;
  auto instance = aws.create_instance();

  std::vector<unsigned char> out_buffer;
  auto fn = [](std::vector<unsigned char> data) { 
      cv::Mat image = imdecode(cv::Mat(data), 1);
      cv::Mat image2;

      thumbnailer(image, image2);

      std::vector<unsigned char> out_buffer;
      cv::imencode(".jpg", image2, out_buffer);

      return out_buffer;
    };
  auto id = cppless::dispatch(
    instance,
    fn,
    out_buffer,
    vectordata
  );
  instance.wait_one();
  
  std::cout << "Received " << out_buffer.size() << " bytes back " << std::endl;

  std::ofstream output(std::string{argv[2]}, std::ios::binary );
  std::copy(out_buffer.data(), out_buffer.data() + out_buffer.size(), std::ostreambuf_iterator<char>(output));
  output.close();

  return 0;
}
