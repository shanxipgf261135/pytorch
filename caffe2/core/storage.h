#ifndef CAFFE2_CORE_STORAGE_H_
#define CAFFE2_CORE_STORAGE_H_

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include "caffe2/core/allocator.h"
#include "caffe2/core/common.h"
#include "caffe2/core/context.h"
#include "caffe2/core/flags.h"
#include "caffe2/core/logging.h"
#include "caffe2/core/typeid.h"

namespace caffe2 {

using DataType = TypeMeta;
// TODO: changed to DataPtr in Aten when shared folder
// is ready
using DataPtr = std::shared_ptr<void>;

class StorageImpl;
using Storage = std::shared_ptr<StorageImpl>;

class CAFFE2_API StorageImpl {
 public:
  StorageImpl() = delete;
  StorageImpl(const StorageImpl&) = delete;
  StorageImpl& operator=(const StorageImpl&) = delete;

  explicit StorageImpl(DeviceType device_type) : device_type_(device_type) {}
  StorageImpl(DeviceType device_type, TypeMeta data_type)
      : data_type_(data_type), device_type_(device_type) {}
  template <typename Deleter = MemoryDeleter>
  StorageImpl(
      DeviceType device_type,
      TypeMeta data_type,
      void* src,
      size_t capacity,
      Deleter d = nullptr)
      : data_type_(data_type), device_type_(device_type) {
    CAFFE_ENFORCE_WITH_CALLER(
        data_type_.id() != TypeIdentifier::uninitialized(),
        "To create storage with a raw external pointer you need to pass in an "
        "initialized data_type(TypeMeta).");
    // Check if the deleter is a MemoryDeleter and is a simple nullptr.
    if (std::is_same<MemoryDeleter, Deleter>::value &&
        reinterpret_cast<MemoryDeleter*>(static_cast<void*>(&d))[0] ==
            nullptr) {
      // Use aliasing constructor trick to avoid calling the destructor.
      data_ptr_ = std::shared_ptr<void>(std::shared_ptr<void>(), src);
    } else {
      data_ptr_.reset(src, d);
    }
    capacity_ = capacity;
  }

  void reset() {
    data_ptr_.reset();
    capacity_ = 0;
  }

  template <typename T>
  inline bool IsType() const {
    return data_type_.Match<T>();
  }

  const void* data() const {
    return data_ptr_.get();
  }

  void* data() {
    return data_ptr_.get();
  }

  DataPtr& data_ptr() {
    return data_ptr_;
  }

  const DataPtr& data_ptr() const {
    return data_ptr_;
  }

  void set_dtype(const DataType& data_type) {
    data_type_ = data_type;
  }

  const DataType& dtype() const {
    return data_type_;
  }

  size_t capacity() const {
    return capacity_;
  }

  int64_t numel() const {
    return capacity_ / itemsize();
  }

  // TODO: remove later
  void set_numel(int64_t numel) {
    capacity_ = numel * itemsize();
  }

  inline DeviceType device_type() const {
    return device_type_;
  }

  inline size_t itemsize() const {
    return data_type_.itemsize();
  }

  // Rule of Five
  StorageImpl(StorageImpl&&) = default;
  ~StorageImpl() = default;
  StorageImpl& operator=(StorageImpl&&) = default;

  /**
   * Can only be called when use_count is 1
   */
  template <typename Deleter = MemoryDeleter>
  void SingleUseStorageShareExternalPointer(
      void* src,
      const DataType& data_type,
      size_t capacity,
      Deleter d = nullptr) {
    // TODO: this will be added in the intrusive_ptr diff
    // CAFFE_ENFORCE_WITH_CALLER(
    //     use_count() == 1,
    //     "SingleUseStorageShareExternalPointer can only be called when
    //     use_count == 1");
    data_type_ = data_type;
    CAFFE_ENFORCE_WITH_CALLER(
        data_type_.id() != TypeIdentifier::uninitialized(),
        "To share with a raw external pointer you need to have meta "
        "already set.");
    CAFFE_ENFORCE_WITH_CALLER(
        capacity >= 0, "capacity must be valid for ShareExternalPointer");
    // Check if the deleter is a MemoryDeleter and is a simple nullptr.
    if (std::is_same<MemoryDeleter, Deleter>::value &&
        reinterpret_cast<MemoryDeleter*>(&d)[0] == nullptr) {
      // Use aliasing constructor trick to avoid calling the destructor.
      data_ptr_ = std::shared_ptr<void>(std::shared_ptr<void>(), src);
    } else {
      data_ptr_.reset(src, d);
    }
    capacity_ = capacity;
  }

 private:
  int64_t capacity_ = 0;
  DataType data_type_;
  DataPtr data_ptr_;
  // allocator_ takes precedence over StaticContext from device_type_
  // Allocator* allocator_;
  DeviceType device_type_ = CPU;

};

/**
 * Create a Storage given an external pointer `src`.
 * `device_type`: the device type of the storage
 * `capacity`: the capacity of the Tensor
 */
template <typename T, typename Deleter = MemoryDeleter>
Storage CreateStorage(
    T* src,
    DeviceType device_type,
    size_t capacity = 0,
    Deleter d = nullptr) {
  return CreateStorage(src, device_type, TypeMeta::Make<T>(), capacity, d);
}

template <typename Deleter = MemoryDeleter>
Storage CreateStorage(
    void* src,
    DeviceType device_type,
    const TypeMeta& meta,
    size_t capacity, /* need to specify capacity of the storage */
    Deleter d = nullptr) {
  // We include capacity here because this will be a public function.
  return std::make_shared<StorageImpl>(device_type, meta, src, capacity, d);
}

} // namespace caffe2

#endif // CAFFE2_CORE_STORAGE_H_
