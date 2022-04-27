#include <array>
#include <cstdio>
#include <iostream>
#include <streambuf>
#include <cstring>

#include <unistd.h>

namespace cppless
{

/* The following code is taken from the book
 * "The C++ Standard Library - A Tutorial and Reference"
 * by Nicolai M. Josuttis, Addison-Wesley, 1999
 *
 * (C) Copyright Nicolai M. Josuttis 1999.
 * Permission to copy, use, modify, sell and distribute this software
 * is granted provided this copyright notice appears in all copies.
 * This software is provided "as is" without express or implied
 * warranty, and with no claim as to its suitability for any purpose.
 */

class fdoutbuf : public std::streambuf
{
protected:
  int m_fd;  // file descriptor
public:
  // constructor
  explicit fdoutbuf(int fd)
      : m_fd(fd)
  {
  }

protected:
  // write one character
  auto overflow(int_type c) -> int_type override
  {
    if (c != EOF) {
      char z = static_cast<char>(c);
      if (write(m_fd, &z, 1) != 1) {
        return EOF;
      }
    }
    return c;
  }
  // write multiple characters

  auto xsputn(const char* s, std::streamsize num) -> std::streamsize override
  {
    return write(m_fd, s, static_cast<size_t>(num));
  }
};

/**
 * @brief A stream which writes to a file descriptor
 */
class fdostream : public std::ostream
{
protected:
  fdoutbuf m_buf;

public:
  explicit fdostream(int fd)
      : std::ostream(nullptr)
      , m_buf(fd)
  {
    rdbuf(&m_buf);
  }
};

class fdinbuf : public std::streambuf
{
protected:
  int m_fd;  // file descriptor

  /* data buffer:
   * - at most, pbSize characters in putback area plus
   * - at most, bufSize characters in ordinary read buffer
   */
  static constexpr int pb_size = 4;  // size of putback area
  static constexpr int buf_size = 1024;  // size of the data buffer
  std::array<char, buf_size + pb_size> m_buffer;  // data buffer

public:
  /* constructor
   * - initialize file descriptor
   * - initialize empty data buffer
   * - no putback area
   * => force underflow()
   */
  fdinbuf(int fd)
      : m_fd(fd)
  {
    setg(&m_buffer[pb_size],  // beginning of putback area
         &m_buffer[pb_size],  // read position
         &m_buffer[pb_size]);  // end position
  }

protected:
  // insert new characters into the buffer
  auto underflow() -> int_type override
  {
    // is read position before end of buffer?
    if (gptr() < egptr()) {
      return traits_type::to_int_type(*gptr());
    }

    /* process size of putback area
     * - use number of characters read
     * - but at most size of putback area
     */
    auto num_putback = gptr() - eback();
    if (num_putback > pb_size) {
      num_putback = pb_size;
    }

    /* copy up to pbSize characters previously read into
     * the putback area
     */
    std::memmove(&m_buffer[static_cast<size_t>(pb_size - num_putback)],
            gptr() - num_putback,
            static_cast<size_t>(num_putback));

    // read at most bufSize new characters
    ssize_t num = read(m_fd, &m_buffer[pb_size], buf_size);
    if (num <= 0) {
      // ERROR or EOF
      return EOF;
    }

    // reset buffer pointers
    setg(&m_buffer[static_cast<size_t>(
             pb_size - num_putback)],  // beginning of putback area
         &m_buffer[pb_size],  // read position
         &m_buffer[static_cast<size_t>(pb_size + num)]);  // end of buffer

    // return next character
    return traits_type::to_int_type(*gptr());
  }
};
/**
 * @brief A stream which reads from a file descriptor
 */
class fdistream : public std::istream
{
protected:
  fdinbuf m_buf;

public:
  explicit fdistream(int fd)
      : std::istream(nullptr)
      , m_buf(fd)
  {
    rdbuf(&m_buf);
  }
};

}  // namespace cppless