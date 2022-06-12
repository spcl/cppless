#include <limits>

const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.1415926535897932385;

// Utility Functions

inline auto degrees_to_radians(double degrees) -> double
{
  return degrees * pi / 180.0;
}
