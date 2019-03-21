#pragma once

#if THRUST_DEVICE_COMPILER == THRUST_DEVICE_COMPILER_HCC
#include <thrust/system/hip/detail/util.h>

#include <thrust/system/hip/execution_policy.h>
#include <thrust/system/hip/detail/par_to_seq.h>
#include <thrust/system/hip/detail/memory_buffer.h>
#include <thrust/detail/mpl/math.h>
#include <thrust/detail/minmax.h>
#include <thrust/distance.h>

// rocprim include
#include <rocprim/rocprim.hpp>

BEGIN_NS_THRUST
namespace hip_rocprim {

namespace __scan_by_key {

template<class Policy,
         class KeysInputIterator,
         class ValuesInputIterator,
         class ValuesOutputIterator,
         class KeyCompareFunction = ::rocprim::equal_to<typename std::iterator_traits<KeysInputIterator>::value_type>,
         class BinaryFunction = ::rocprim::plus<typename std::iterator_traits<ValuesInputIterator>::value_type>
>
ValuesOutputIterator THRUST_HIP_RUNTIME_FUNCTION
inclusive_scan_by_key(Policy                    &policy,
                      KeysInputIterator         key_first,
                      KeysInputIterator         key_last,
                      ValuesInputIterator       value_first,
                      ValuesOutputIterator      value_result,
                      KeyCompareFunction        key_compare_op,
                      BinaryFunction            scan_op)
{
    size_t       num_items          = static_cast<size_t>(thrust::distance(key_first, key_last));
    void *       d_temp_storage     = nullptr;
    size_t       temp_storage_bytes = 0;
    hipStream_t  stream             = hip_rocprim::stream(policy);
    bool         debug_sync         = THRUST_HIP_DEBUG_SYNC_FLAG;

    if (num_items == 0)
        return value_result;

    // Determine temporary device storage requirements.
    hip_rocprim::throw_on_error(
            rocprim::inclusive_scan_by_key(d_temp_storage,
                                           temp_storage_bytes,
                                           key_first,
                                           value_first,
                                           value_result,
                                           num_items,
                                           scan_op,
                                           key_compare_op,
                                           stream,
                                           debug_sync),
            "scan_by_key failed on 1st step");

    // Allocate temporary storage.
    temp_storage_bytes = rocprim::detail::align_size(temp_storage_bytes);
    d_temp_storage = hip_rocprim::get_memory_buffer(policy, temp_storage_bytes);
    hip_rocprim::throw_on_error(hipGetLastError(),
                                "scan_by_key failed to get memory buffer");

    // Run scan.
    hip_rocprim::throw_on_error(
            rocprim::inclusive_scan_by_key(d_temp_storage,
                                           temp_storage_bytes,
                                           key_first,
                                           value_first,
                                           value_result,
                                           num_items,
                                           scan_op,
                                           key_compare_op,
                                           stream,
                                           debug_sync),
            "scan_by_key failed on 2nd step");

    hip_rocprim::return_memory_buffer(policy, d_temp_storage);
    hip_rocprim::throw_on_error(hipGetLastError(),
                                "scan_by_key failed to return memory buffer");

    return value_result + num_items;
}

template<
        class Policy,
        class KeysInputIterator,
        class ValuesInputIterator,
        class ValuesOutputIterator,
        class InitialValueType,
        class KeyCompareFunction = ::rocprim::equal_to<typename std::iterator_traits<KeysInputIterator>::value_type>,
        class BinaryFunction = ::rocprim::plus<typename std::iterator_traits<ValuesInputIterator>::value_type>
>
ValuesOutputIterator THRUST_HIP_RUNTIME_FUNCTION
exclusive_scan_by_key(Policy                    &policy,
                      KeysInputIterator         key_first,
                      KeysInputIterator         key_last,
                      ValuesInputIterator       value_first,
                      ValuesOutputIterator      value_result,
                      InitialValueType          init,
                      KeyCompareFunction        key_compare_op,
                      BinaryFunction            scan_op)
{
    size_t       num_items          = static_cast<size_t>(thrust::distance(key_first, key_last));
    void *       d_temp_storage     = nullptr;
    size_t       temp_storage_bytes = 0;
    hipStream_t  stream             = hip_rocprim::stream(policy);
    bool         debug_sync         = THRUST_HIP_DEBUG_SYNC_FLAG;

    if (num_items == 0)
        return value_result;

    // Determine temporary device storage requirements.
    hip_rocprim::throw_on_error(
            rocprim::exclusive_scan_by_key(d_temp_storage,
                                           temp_storage_bytes,
                                           key_first,
                                           value_first,
                                           value_result,
                                           init,
                                           num_items,
                                           scan_op,
                                           key_compare_op,
                                           stream,
                                           debug_sync),
            "scan_by_key failed on 1st step");

    // Allocate temporary storage.
    temp_storage_bytes = rocprim::detail::align_size(temp_storage_bytes);
    d_temp_storage = hip_rocprim::get_memory_buffer(policy, temp_storage_bytes);
    hip_rocprim::throw_on_error(hipGetLastError(),
                                "scan_by_key failed to get memory buffer");

    // Run scan.
    hip_rocprim::throw_on_error(
            rocprim::exclusive_scan_by_key(d_temp_storage,
                                           temp_storage_bytes,
                                           key_first,
                                           value_first,
                                           value_result,
                                           init,
                                           num_items,
                                           scan_op,
                                           key_compare_op,
                                           stream,
                                           debug_sync),
            "scan_by_key failed on 2nd step");

    hip_rocprim::return_memory_buffer(policy, d_temp_storage);
    hip_rocprim::throw_on_error(hipGetLastError(),
                                "scan_by_key failed to return memory buffer");

    return value_result + num_items;
}

}    // namspace scan_by_key

//-------------------------
// Thrust API entry points
//-------------------------

//---------------------------
//   Inclusive scan
//---------------------------

__thrust_exec_check_disable__
template <class Derived,
          class KeyInputIt,
          class ValInputIt,
          class ValOutputIt,
          class BinaryPred,
          class ScanOp>
ValOutputIt THRUST_HIP_FUNCTION
inclusive_scan_by_key(execution_policy<Derived> &policy,
                      KeyInputIt                 key_first,
                      KeyInputIt                 key_last,
                      ValInputIt                 value_first,
                      ValOutputIt                value_result,
                      BinaryPred                 binary_pred,
                      ScanOp                     scan_op)
{
  ValOutputIt ret = value_result;
  THRUST_HIP_PRESERVE_KERNELS_WORKAROUND((
      __scan_by_key::inclusive_scan_by_key<Derived, KeyInputIt, ValInputIt, ValOutputIt, BinaryPred, ScanOp>
  ));
#if __THRUST_HAS_HIPRT__
    ret = __scan_by_key::inclusive_scan_by_key(policy,
                                               key_first,
                                               key_last,
                                               value_first,
                                               value_result,
                                               binary_pred,
                                               scan_op);
#else
    ret = thrust::inclusive_scan_by_key(cvt_to_seq(derived_cast(policy)),
                                        key_first,
                                        key_last,
                                        value_first,
                                        value_result,
                                        binary_pred,
                                        scan_op);
#endif
  return ret;
}

template <class Derived,
          class KeyInputIt,
          class ValInputIt,
          class ValOutputIt,
          class BinaryPred>
ValOutputIt THRUST_HIP_FUNCTION
inclusive_scan_by_key(execution_policy<Derived> &policy,
                      KeyInputIt                 key_first,
                      KeyInputIt                 key_last,
                      ValInputIt                 value_first,
                      ValOutputIt                value_result,
                      BinaryPred                 binary_pred)
{
  typedef typename thrust::iterator_traits<ValOutputIt>::value_type value_type;
  return hip_rocprim::inclusive_scan_by_key(policy,
                                            key_first,
                                            key_last,
                                            value_first,
                                            value_result,
                                            binary_pred,
                                            plus<value_type>());
}

template <class Derived,
          class KeyInputIt,
          class ValInputIt,
          class ValOutputIt>
ValOutputIt THRUST_HIP_FUNCTION
inclusive_scan_by_key(execution_policy<Derived> &policy,
                      KeyInputIt                 key_first,
                      KeyInputIt                 key_last,
                      ValInputIt                 value_first,
                      ValOutputIt                value_result)
{
  typedef typename thrust::iterator_traits<KeyInputIt>::value_type key_type;
  return hip_rocprim::inclusive_scan_by_key(policy,
                                            key_first,
                                            key_last,
                                            value_first,
                                            value_result,
                                            equal_to<key_type>());
}


//---------------------------
//   Exclusive scan
//---------------------------

__thrust_exec_check_disable__
template <class Derived,
          class KeyInputIt,
          class ValInputIt,
          class ValOutputIt,
          class Init,
          class BinaryPred,
          class ScanOp>
ValOutputIt THRUST_HIP_FUNCTION
exclusive_scan_by_key(execution_policy<Derived> &policy,
                      KeyInputIt                 key_first,
                      KeyInputIt                 key_last,
                      ValInputIt                 value_first,
                      ValOutputIt                value_result,
                      Init                       init,
                      BinaryPred                 binary_pred,
                      ScanOp                     scan_op)
{
    ValOutputIt ret = value_result;
    THRUST_HIP_PRESERVE_KERNELS_WORKAROUND((
        __scan_by_key::exclusive_scan_by_key<Derived, KeyInputIt, ValInputIt, ValOutputIt, Init, BinaryPred, ScanOp>
    ));
#if __THRUST_HAS_HIPRT__
      ret = __scan_by_key::exclusive_scan_by_key(policy,
                                                 key_first,
                                                 key_last,
                                                 value_first,
                                                 value_result,
                                                 init,
                                                 binary_pred,
                                                 scan_op);
#else
    ret = thrust::exclusive_scan_by_key(cvt_to_seq(derived_cast(policy)),
                                        key_first,
                                        key_last,
                                        value_first,
                                        value_result,
                                        init,
                                        binary_pred,
                                        scan_op);
#endif // __THRUST_HAS_HIPRT__
    return ret;
}

template <class Derived,
          class KeyInputIt,
          class ValInputIt,
          class ValOutputIt,
          class Init,
          class BinaryPred>
ValOutputIt THRUST_HIP_FUNCTION
exclusive_scan_by_key(execution_policy<Derived> &policy,
                      KeyInputIt                 key_first,
                      KeyInputIt                 key_last,
                      ValInputIt                 value_first,
                      ValOutputIt                value_result,
                      Init                       init,
                      BinaryPred                 binary_pred)
{
  return hip_rocprim::exclusive_scan_by_key(policy,
                                            key_first,
                                            key_last,
                                            value_first,
                                            value_result,
                                            init,
                                            binary_pred,
                                            plus<Init>());
}

template <class Derived,
          class KeyInputIt,
          class ValInputIt,
          class ValOutputIt,
          class Init>
ValOutputIt THRUST_HIP_FUNCTION
exclusive_scan_by_key(execution_policy<Derived> &policy,
                      KeyInputIt                 key_first,
                      KeyInputIt                 key_last,
                      ValInputIt                 value_first,
                      ValOutputIt                value_result,
                      Init                       init)
{
  typedef typename iterator_traits<KeyInputIt>::value_type key_type;
  return hip_rocprim::exclusive_scan_by_key(policy,
                                            key_first,
                                            key_last,
                                            value_first,
                                            value_result,
                                            init,
                                            equal_to<key_type>());
}


template <class Derived,
          class KeyInputIt,
          class ValInputIt,
          class ValOutputIt>
ValOutputIt THRUST_HIP_FUNCTION
exclusive_scan_by_key(execution_policy<Derived> &policy,
                      KeyInputIt                 key_first,
                      KeyInputIt                 key_last,
                      ValInputIt                 value_first,
                      ValOutputIt                value_result)
{
  typedef typename iterator_traits<ValOutputIt>::value_type value_type;
  return hip_rocprim::exclusive_scan_by_key(policy,
                                            key_first,
                                            key_last,
                                            value_first,
                                            value_result,
                                            value_type(0));
}

}    // namespace hip_rocprim
END_NS_THRUST

#include <thrust/scan.h>

#endif