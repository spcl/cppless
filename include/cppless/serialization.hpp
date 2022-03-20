#pragma once

#include <stdexcept>
#include <vector>

typedef std::vector<unsigned char> byte_sink;
struct byte_source : std::vector<unsigned char>::const_iterator
{
public:
  byte_source(std::vector<unsigned char>::iterator begin,
              std::vector<unsigned char>::iterator end)
      : std::vector<unsigned char>::const_iterator(begin)
      , end(end)
  {
  }

  const std::vector<unsigned char>::iterator end;
};

// unsigned char
inline byte_sink& operator<<(byte_sink& os, unsigned char c)
{
  os.push_back(c);
  return os;
}

inline byte_source& operator>>(byte_source& is, unsigned char& c)
{
  if (is.end == is)
    throw std::runtime_error("EOF");
  c = *is;
  is++;
  return is;
}

// int
// Ints are serialized and deserialized as 32 bit
inline byte_sink& operator<<(byte_sink& os, int i)
{
  unsigned int ui = (unsigned int)i;
  os << (unsigned char)((ui & 0x000000ffU) >> 0);
  os << (unsigned char)((ui & 0x0000ff00U) >> 8);
  os << (unsigned char)((ui & 0x00ff0000U) >> 16);
  os << (unsigned char)((ui & 0xff000000U) >> 24);
  return os;
}

inline byte_source& operator>>(byte_source& is, int& i)
{
  unsigned char b0, b1, b2, b3;
  is >> b0 >> b1 >> b2 >> b3;
  i = (b0 & 0x000000ff);
  i |= (b1 & 0x000000ff) << 8;
  i |= (b2 & 0x000000ff) << 16;
  i |= (b3 & 0x000000ff) << 24;
  return is;
}

template<typename T, typename... Ts>
inline void serialize_all_helper(byte_sink& dst,
                                 T const& first,
                                 Ts const&... rest)
{
  dst << first;
  if constexpr (sizeof...(rest) > 0) {
    serialize_all(dst, rest...);
  }
}

template<typename... Ts>
inline void serialize_all(byte_sink& dst, Ts const&... rest)
{
  if constexpr (sizeof...(rest) > 0) {
    serialize_all_helper(dst, rest...);
  }
}

template<typename T, typename... Ts>
inline void deserialize_all_helper(byte_source& src, T& first, Ts&... rest)
{
  src >> first;
  if constexpr (sizeof...(rest) > 0) {
    deserialize_all(src, rest...);
  }
}

template<typename... Ts>
inline void deserialize_all(byte_source& src, Ts&... rest)
{
  if constexpr (sizeof...(rest) > 0) {
    deserialize_all_helper(src, rest...);
  }
}