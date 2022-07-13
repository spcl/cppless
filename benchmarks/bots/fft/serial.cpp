#include <complex>
#include <iostream>
#include <span>
#include <vector>

void fft_rec(std::span<std::complex<double>> x);

void fft(std::span<int> x_in, std::span<std::complex<double>> x_out)
{
  unsigned int n = x_in.size();

  // Make copy of array and apply window
  for (unsigned int i = 0; i < n; i++) {
    x_out[i] = std::complex<double>(x_in[i], 0);
    x_out[i] *= 1;  // Window
  }

  // Start recursion
  fft_rec(x_out);
}

void fft_rec(std::span<std::complex<double>> x)
{
  unsigned int n = x.size();
  // Check if it is splitted enough
  if (n <= 1) {
    return;
  }

  // Split even and odd
  std::vector<std::complex<double>> odd(n / 2);
  std::vector<std::complex<double>> even(n / 2);
  for (int i = 0; i < n / 2; i++) {
    even[i] = x[i * 2];
    odd[i] = x[i * 2 + 1];
  }

  // Split on tasks
  fft_rec(even);
  fft_rec(odd);

  // Calculate DFT
  for (int k = 0; k < n / 2; k++) {
    std::complex<double> t =
        exp(std::complex<double>(0, -2 * M_PI * k / n)) * odd[k];
    x[k] = even[k] + t;
    x[n / 2 + k] = even[k] - t;
  }
}

int main(int argc, char* argv[])
{
  int n = 100;
  std::vector<int> input(n);
  std::vector<std::complex<double>> output(n);
  for (int i = 0; i < n; i++) {
    input[i] = i;
  }
  fft(input, output);
  for (int i = 0; i < n; i++) {
    std::cout << output[i].real() << " " << output[i].imag() << std::endl;
  }
}