auto main() -> int
{
  auto q = 12;
  auto l = [=]() { return q; };
}