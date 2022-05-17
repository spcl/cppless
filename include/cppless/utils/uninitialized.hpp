#pragma once

#include <memory>
namespace cppless
{
template<class T>
union uninitialized_data
{
  T m_self;
  std::aligned_storage_t<sizeof(T), alignof(T)> m_data;
  /**
   * @brief The constructor initializes `m_data`, thus m_self is keeps being
   * uninitialized. This operation is save when we assume that the
   * unarchiving of the lambda restores all internal fields
   */
  uninitialized_data()
      : m_data()
  {
  }
  /**
   * @brief When the destructor is called we assume that the object has been
   * initialized, thus we call the destructor of `m_self`.
   */
  ~uninitialized_data()
  {
    m_self.~T();
  }

  // Delete copy and move constructors and assignment operators
  uninitialized_data(const uninitialized_data&) = delete;
  uninitialized_data(uninitialized_data&&) = delete;

  auto operator=(const uninitialized_data&) -> uninitialized_data& = delete;
  auto operator=(uninitialized_data&&) -> uninitialized_data& = delete;
};
}  // namespace cppless