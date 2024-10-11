/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 3.0
//       Copyright (2020) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY NTESS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NTESS OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Christian R. Trott (crtrott@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#include "common.hpp"

#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <Kokkos_Core.hpp>

//----------------------------------------------------------------------------//
//
//        The Kokkos::complex dtypes
//
//----------------------------------------------------------------------------//

namespace Kokkos {

namespace {
float re_float_offset;
float im_float_offset;
double re_double_offset;
double im_double_offset;
}  // namespace

// Need to explicitly do both float and double since we cannot
// partially specialize function templates
template <>
constexpr const float&& get<2, float>(const complex<float>&&) noexcept {
  static_assert(std::is_standard_layout_v<complex<float>>);
  re_float_offset = static_cast<float>(offsetof(complex<float>, re_));
  return std::move(re_float_offset);
}

template <>
constexpr const float&& get<3, float>(const complex<float>&&) noexcept {
  static_assert(std::is_standard_layout_v<complex<float>>);
  im_float_offset = static_cast<float>(offsetof(complex<float>, im_));
  return std::move(im_float_offset);
}

template <>
constexpr const double&& get<2, double>(const complex<double>&&) noexcept {
  static_assert(std::is_standard_layout_v<complex<double>>);
  re_double_offset = static_cast<double>(offsetof(complex<double>, re_));
  return std::move(re_double_offset);
}

template <>
constexpr const double&& get<3, double>(const complex<double>&&) noexcept {
  static_assert(std::is_standard_layout_v<complex<double>>);
  im_double_offset = static_cast<double>(offsetof(complex<double>, im_));
  return std::move(im_double_offset);
}
}  // namespace Kokkos

#define PYBIND11_FIELD_DESCRIPTOR_EX_WORKAROUND(Name, Offset, Type)            \
  ::pybind11::detail::field_descriptor {                                       \
    Name, Offset, sizeof(Type), ::pybind11::format_descriptor<Type>::format(), \
        ::pybind11::detail::npy_format_descriptor<Type>::dtype()               \
  }

template <typename Tp>
void register_complex_as_numpy_dtype() {
  /* This function registers Kokkos::complex<Tp> as a numpy datatype
   * which is needed to cast Kokkos views of complex numbers to numpy
   * arrays. Ideally we would just call this macro
   *
   * `PYBIND11_NUMPY_DTYPE(ComplexTp, re_, im_);`
   *
   * which builds a vector of field descriptors of the complex type.
   * However this will not work because re_ and im_ are private member
   * variables. The macro needs to extract their type and their offset
   * within the class to work properly.
   *
   * Getting the type is easy since it can only be a float or double.
   * Getting the offset requires calling
   * `offsetof(Kokkos::complex<Tp>, re_)`, which will not work since
   * we cannot access private member variables. The solution is to
   * create a context in which we can access them and return the
   * offset from there. This is possible by specializing a templated
   * member function or friend function to Kokkos::complex since they
   * can access private variables (see
   * http://www.gotw.ca/gotw/076.htm).
   *
   * Looking at Kokkos::complex, there is the get() template function
   * which we can specialize. We select this overload
   *
   * ```
   * template <size_t I, typename RT>
   * friend constexpr const RT&& get(const complex<RT>&&) noexcept;
   * ```
   *
   * And specialize it for I == 2 for re_ and I == 3 for im_. Each
   * specialization calls offsetof for the corresponding member
   * variables and returns it. The original get function only works
   * for I == 0 and I == 1, so these specializations will not
   * interfere with it. Since the functions return rvalue references,
   * we store the offsets in global variables and move them when
   * returning.
   */

  using ComplexTp = Kokkos::complex<Tp>;

  py::ssize_t re_offset = static_cast<py::ssize_t>(
      Kokkos::get<2, Tp>(static_cast<const ComplexTp&&>(ComplexTp{0.0, 0.0})));
  py::ssize_t im_offset = static_cast<py::ssize_t>(
      Kokkos::get<3, Tp>(static_cast<const ComplexTp&&>(ComplexTp{0.0, 0.0})));

  ::pybind11::detail::npy_format_descriptor<ComplexTp>::register_dtype(
      ::std::vector<::pybind11::detail::field_descriptor>{
          PYBIND11_FIELD_DESCRIPTOR_EX_WORKAROUND("re_", re_offset, Tp),
          PYBIND11_FIELD_DESCRIPTOR_EX_WORKAROUND("im_", im_offset, Tp)});
}

template <typename Tp>
void generate_complex_dtype(py::module& kokkos, const std::string& _name) {
  using ComplexTp = Kokkos::complex<Tp>;

  py::class_<ComplexTp>(kokkos, _name.c_str())
      .def(py::init<Tp>())      // Constructor for real part only
      .def(py::init<Tp, Tp>())  // Constructor for real and imaginary parts
      .def("imag_mutable", py::overload_cast<>(&ComplexTp::imag))
      .def("imag_const", py::overload_cast<>(&ComplexTp::imag, py::const_))
      .def("imag_set", py::overload_cast<Tp>(&ComplexTp::imag))
      .def("real_mutable", py::overload_cast<>(&ComplexTp::real))
      .def("real_const", py::overload_cast<>(&ComplexTp::real, py::const_))
      .def("real_set", py::overload_cast<Tp>(&ComplexTp::real))
      .def(py::self += py::self)
      .def(py::self += Tp())
      .def(py::self -= py::self)
      .def(py::self -= Tp())
      .def(py::self *= py::self)
      .def(py::self *= Tp())
      .def(py::self /= py::self)
      .def(py::self /= Tp());
}

void generate_complex_dtypes(py::module& kokkos) {
  generate_complex_dtype<float>(kokkos, "complex_float32");
  generate_complex_dtype<double>(kokkos, "complex_float64");

  register_complex_as_numpy_dtype<float>();
  register_complex_as_numpy_dtype<double>();
}