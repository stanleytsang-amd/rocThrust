// MIT License
//
// Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Google Test
#include "test_utils.hpp"
#include <gtest/gtest.h>

//#include <unittest/unittest.h>
#include <thrust/generate.h>
#include <thrust/iterator/discard_iterator.h>
#include <thrust/iterator/retag.h>

// HIP API
#if THRUST_DEVICE_COMPILER == THRUST_DEVICE_COMPILER_HCC

#include <hip/hip_runtime.h>
#include <hip/hip_runtime_api.h>

#include "test_assertions.hpp"
#include "test_utils.hpp"

#define HIP_CHECK(condition) ASSERT_EQ(condition, hipSuccess)
#endif // THRUST_DEVICE_COMPILER == THRUST_DEVICE_COMPILER_HCC

__THRUST_DISABLE_MSVC_POSSIBLE_LOSS_OF_DATA_WARNING_BEGIN

#if THRUST_DEVICE_COMPILER == THRUST_DEVICE_COMPILER_HCC

template<class InputType> struct Params
{
    using input_type = InputType;
};

template<class Params> class GenerateTests : public ::testing::Test
{
public:
    using input_type = typename Params::input_type;
};

template<class Params> class GenerateVectorTests : public ::testing::Test
{
public:
    using input_type = typename Params::input_type;
};

template<class Params> class GenerateVariablesTests : public ::testing::Test
{
public:
    using input_type = typename Params::input_type;
};

typedef ::testing::Types<
        Params<thrust::host_vector<short>>,
        Params<thrust::host_vector<int>>,
        Params<thrust::host_vector<long long>>,
        Params<thrust::host_vector<unsigned short>>,
        Params<thrust::host_vector<unsigned int>>,
        Params<thrust::host_vector<unsigned long long>>,
        Params<thrust::host_vector<float>>,
        Params<thrust::host_vector<double>>,
        Params<thrust::device_vector<short>>,
        Params<thrust::device_vector<int>>,
        Params<thrust::device_vector<long long>>,
        Params<thrust::device_vector<unsigned short>>,
        Params<thrust::device_vector<unsigned int>>,
        Params<thrust::device_vector<unsigned long long>>,
        Params<thrust::device_vector<float>>,
        Params<thrust::device_vector<double>>
> GenerateTestsParams;

typedef ::testing::Types<
        Params<thrust::host_vector<char>>,
        Params<thrust::host_vector<unsigned char>>,
        Params<thrust::host_vector<short>>,
        Params<thrust::host_vector<unsigned short>>,
        Params<thrust::host_vector<int>>,
        Params<thrust::host_vector<unsigned int>>,
        Params<thrust::host_vector<float>>
> GenerateTestsVariableParams;

typedef ::testing::Types<
        Params<thrust::host_vector<short>>,
        Params<thrust::host_vector<int>>
> VectorParams;

TYPED_TEST_CASE(GenerateTests, GenerateTestsParams);
TYPED_TEST_CASE(GenerateVectorTests, VectorParams);
TYPED_TEST_CASE(GenerateVariablesTests, GenerateTestsVariableParams);

TEST(ReplaceTests, UsingHip)
{
    ASSERT_EQ(THRUST_DEVICE_SYSTEM, THRUST_DEVICE_SYSTEM_HIP);
}

template<typename T> struct return_value
{
    T val;

    return_value(void)
    {
    }

    return_value(T v) :
            val(v)
    {
    }

    __host__ __device__ T operator()(void)
    {
        return val;
    }
};

TYPED_TEST(GenerateVectorTests, TestGenerateSimple)
{
    using Vector = typename TestFixture::input_type;
    using T = typename Vector::value_type;

    Vector result(5);

    T value = 13;

    return_value<T> f(value);

    thrust::generate(result.begin(), result.end(), f);

    ASSERT_EQ(result[0], value);
    ASSERT_EQ(result[1], value);
    ASSERT_EQ(result[2], value);
    ASSERT_EQ(result[3], value);
    ASSERT_EQ(result[4], value);
}

template<typename ForwardIterator, typename Generator> __host__ __device__ void
generate(my_system& system, ForwardIterator, ForwardIterator, Generator)
{
    system.validate_dispatch();
}

TEST(GenerateTests, TestGenerateDispatchExplicit)
{
    thrust::device_vector<int> vec(1);

    my_system sys(0);
    thrust::generate(sys, vec.begin(), vec.end(), 0);

    ASSERT_EQ(true, sys.is_valid());
}

template<typename ForwardIterator, typename Generator> __host__ __device__ void
generate(my_tag, ForwardIterator first, ForwardIterator, Generator)
{
    *first = 13;
}

TEST(GenerateTests, TestGenerateDispatchImplicit)
{
    thrust::device_vector<int> vec(1);

    thrust::generate(
            thrust::retag<my_tag>(vec.begin()), thrust::retag<my_tag>(vec.end()), 0);

    ASSERT_EQ(13, vec.front());
}

TYPED_TEST(GenerateVariablesTests, TestGenerate)
{
    using Vector = typename TestFixture::input_type;
    using T = typename Vector::value_type;

    const std::vector<size_t> sizes = get_sizes();
    for (auto size : sizes)
    {
        thrust::host_vector<T> h_result(size);
        thrust::device_vector<T> d_result(size);

        T value = 13;
        return_value<T> f(value);

        thrust::generate(h_result.begin(), h_result.end(), f);
        thrust::generate(d_result.begin(), d_result.end(), f);

        ASSERT_EQ(h_result, d_result);
    }
}

TYPED_TEST(GenerateVariablesTests, TestGenerateToDiscardIterator)
{
    using Vector = typename TestFixture::input_type;
    using T = typename Vector::value_type;

    T value = 13;
    return_value<T> f(value);

    thrust::discard_iterator<thrust::host_system_tag> h_first;
    thrust::generate(h_first, h_first + 10, f);

    thrust::discard_iterator<thrust::device_system_tag> d_first;
    thrust::generate(d_first, d_first + 10, f);

    // there's nothing to actually check except that it compiles
}

TYPED_TEST(GenerateVectorTests, TestGenerateNSimple)
{
    using Vector = typename TestFixture::input_type;
    using T = typename Vector::value_type;

    Vector result(5);

    T value = 13;

    return_value<T> f(value);

    thrust::generate_n(result.begin(), result.size(), f);

    ASSERT_EQ(result[0], value);
    ASSERT_EQ(result[1], value);
    ASSERT_EQ(result[2], value);
    ASSERT_EQ(result[3], value);
    ASSERT_EQ(result[4], value);
}

template<typename ForwardIterator, typename Size, typename Generator>
__host__ __device__ ForwardIterator
generate_n(my_system& system, ForwardIterator first, Size, Generator)
{
    system.validate_dispatch();
    return first;
}

TEST(GenerateTests, TestGenerateNDispatchExplicit)
{
    thrust::device_vector<int> vec(1);

    my_system sys(0);
    thrust::generate_n(sys, vec.begin(), vec.size(), 0);

    ASSERT_EQ(true, sys.is_valid());
}

template<typename ForwardIterator, typename Size, typename Generator>
__host__ __device__ ForwardIterator
generate_n(my_tag, ForwardIterator first, Size, Generator)
{
    *first = 13;
    return first;
}

TEST(GenerateTests, TestGenerateNDispatchImplicit)
{
    thrust::device_vector<int> vec(1);

    thrust::generate_n(thrust::retag<my_tag>(vec.begin()), vec.size(), 0);

    ASSERT_EQ(13, vec.front());
}

TYPED_TEST(GenerateVariablesTests, TestGenerateNToDiscardIterator)
{
    using Vector = typename TestFixture::input_type;
    using T = typename Vector::value_type;

    const std::vector<size_t> sizes = get_sizes();
    for (auto size : sizes)
    {

        T value = 13;
        return_value<T> f(value);

        thrust::discard_iterator<thrust::host_system_tag> h_result = thrust::generate_n(
                thrust::discard_iterator<thrust::host_system_tag>(), size, f);

        thrust::discard_iterator<thrust::device_system_tag> d_result = thrust::generate_n(
                thrust::discard_iterator<thrust::device_system_tag>(), size, f);

        thrust::discard_iterator<> reference(size);

        ASSERT_EQ_QUIET(reference, h_result);
        ASSERT_EQ_QUIET(reference, d_result);
    }
}

TYPED_TEST(GenerateVectorTests, TestGenerateZipIterator)
{
    using Vector = typename TestFixture::input_type;
    using T = typename Vector::value_type;

    Vector v1(3, T(0));
    Vector v2(3, T(0));

    thrust::generate(
            thrust::make_zip_iterator(thrust::make_tuple(v1.begin(), v2.begin())),
            thrust::make_zip_iterator(thrust::make_tuple(v1.end(), v2.end())),
            return_value<thrust::tuple<T, T>>(thrust::tuple<T, T>(4, 7)));

    ASSERT_EQ(v1[0], 4);
    ASSERT_EQ(v1[1], 4);
    ASSERT_EQ(v1[2], 4);
    ASSERT_EQ(v2[0], 7);
    ASSERT_EQ(v2[1], 7);
    ASSERT_EQ(v2[2], 7);
}

TEST(GenerateTests, TestGenerateTuple)
{
    using T = int;
    using Tuple = thrust::tuple<T, T>;

    thrust::host_vector<Tuple> h(3, Tuple(0, 0));
    thrust::device_vector<Tuple> d(3, Tuple(0, 0));

    thrust::generate(h.begin(), h.end(), return_value<Tuple>(Tuple(4, 7)));
    thrust::generate(d.begin(), d.end(), return_value<Tuple>(Tuple(4, 7)));

    ASSERT_EQ_QUIET(h, d);
}

#endif // THRUST_DEVICE_COMPILER == THRUST_DEVICE_COMPILER_HCC

__THRUST_DISABLE_MSVC_POSSIBLE_LOSS_OF_DATA_WARNING_END