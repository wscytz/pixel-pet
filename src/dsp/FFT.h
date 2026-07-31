#pragma once

#include <complex>

// 迭代式 Cooley-Tukey FFT,原地,n 须为 2 的幂。
namespace FFT {
void forward(std::complex<float>* data, int n);
}
