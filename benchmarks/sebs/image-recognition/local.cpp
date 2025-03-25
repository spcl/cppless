#include <fstream>
#include <random>
#include <vector>

#include "function.hpp"

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

  cv::Mat image = imdecode(cv::Mat(vectordata), 1);
  int output = recognition(image);
 
  std::cout << "Received classification " << output << std::endl;

  return 0;
}
