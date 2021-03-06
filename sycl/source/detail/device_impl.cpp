//==----------------- device_impl.cpp - SYCL device ------------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <CL/sycl/device.hpp>
#include <detail/device_impl.hpp>

#include <algorithm>

__SYCL_INLINE_NAMESPACE(cl) {
namespace sycl {
namespace detail {

device_impl::device_impl()
    : MIsHostDevice(true),
      MPlatform(std::make_shared<platform_impl>(platform_impl())) {}

device_impl::device_impl(RT::PiDevice Device, PlatformImplPtr Platform)
    : device_impl(Device, Platform, Platform->getPlugin()) {}

device_impl::device_impl(RT::PiDevice Device, const plugin &Plugin)
    : device_impl(Device, nullptr, Plugin) {}

device_impl::device_impl(RT::PiDevice Device, PlatformImplPtr Platform,
                         const plugin &Plugin)
    : MDevice(Device), MIsHostDevice(false) {
  // TODO catch an exception and put it to list of asynchronous exceptions
  Plugin.call<PiApiKind::piDeviceGetInfo>(
      MDevice, PI_DEVICE_INFO_TYPE, sizeof(RT::PiDeviceType), &MType, nullptr);

  RT::PiDevice parent = nullptr;
  // TODO catch an exception and put it to list of asynchronous exceptions
  Plugin.call<PiApiKind::piDeviceGetInfo>(
      MDevice, PI_DEVICE_INFO_PARENT_DEVICE, sizeof(RT::PiDevice), &parent, nullptr);

  MIsRootDevice = (nullptr == parent);
  if (!MIsRootDevice) {
    // TODO catch an exception and put it to list of asynchronous exceptions
    Plugin.call<PiApiKind::piDeviceRetain>(MDevice);
  }

  // set MPlatform
  if (!Platform) {
    RT::PiPlatform plt = nullptr; // TODO catch an exception and put it to list
                                  // of asynchronous exceptions
    Plugin.call<PiApiKind::piDeviceGetInfo>(Device, PI_DEVICE_INFO_PLATFORM,
                                            sizeof(plt), &plt, nullptr);
    Platform = std::make_shared<platform_impl>(plt, Plugin);
  }
  MPlatform = Platform;
}

device_impl::~device_impl() {
  if (!MIsRootDevice && !MIsHostDevice) {
    // TODO catch an exception and put it to list of asynchronous exceptions
    const detail::plugin &Plugin = getPlugin();
    RT::PiResult Err = Plugin.call_nocheck<PiApiKind::piDeviceRelease>(MDevice);
    CHECK_OCL_CODE_NO_EXC(Err);
  }
}

bool device_impl::is_affinity_supported(
    info::partition_affinity_domain AffinityDomain) const {
  auto SupportedDomains = get_info<info::device::partition_affinity_domains>();
  return std::find(SupportedDomains.begin(), SupportedDomains.end(),
                   AffinityDomain) != SupportedDomains.end();
}

cl_device_id device_impl::get() const {
  if (MIsHostDevice)
    throw invalid_object_error("This instance of device is a host instance",
                               PI_INVALID_DEVICE);

  if (!MIsRootDevice) {
    // TODO catch an exception and put it to list of asynchronous exceptions
    const detail::plugin &Plugin = getPlugin();
    Plugin.call<PiApiKind::piDeviceRetain>(MDevice);
  }
  // TODO: check that device is an OpenCL interop one
  return pi::cast<cl_device_id>(MDevice);
}

platform device_impl::get_platform() const {
  return createSyclObjFromImpl<platform>(MPlatform);
}

bool device_impl::has_extension(const string_class &ExtensionName) const {
  if (MIsHostDevice)
    // TODO: implement extension management for host device;
    return false;

  string_class AllExtensionNames =
      get_device_info<string_class, info::device::extensions>::get(
          this->getHandleRef(), this->getPlugin());
  return (AllExtensionNames.find(ExtensionName) != std::string::npos);
}

bool device_impl::is_partition_supported(info::partition_property Prop) const {
  auto SupportedProperties = get_info<info::device::partition_properties>();
  return std::find(SupportedProperties.begin(), SupportedProperties.end(),
                   Prop) != SupportedProperties.end();
}

vector_class<device>
device_impl::create_sub_devices(const cl_device_partition_property *Properties,
                                size_t SubDevicesCount) const {

  vector_class<RT::PiDevice> SubDevices(SubDevicesCount);
  pi_uint32 ReturnedSubDevices = 0;
  const detail::plugin &Plugin = getPlugin();
  Plugin.call<PiApiKind::piDevicePartition>(MDevice, Properties,
                                            SubDevicesCount, SubDevices.data(),
                                            &ReturnedSubDevices);
  // TODO: check that returned number of sub-devices matches what was
  // requested, otherwise this walk below is wrong.
  //
  // TODO: Need to describe the subdevice model. Some sub_device management
  // may be necessary. What happens if create_sub_devices is called multiple
  // times with the same arguments?
  //
  vector_class<device> res;
  std::for_each(SubDevices.begin(), SubDevices.end(),
                [&res, this](const RT::PiDevice &a_pi_device) {
                  device sycl_device = detail::createSyclObjFromImpl<device>(
                      std::make_shared<device_impl>(a_pi_device, MPlatform));
                  res.push_back(sycl_device);
                });
  return res;
}

vector_class<device>
device_impl::create_sub_devices(size_t ComputeUnits) const {

  if (MIsHostDevice)
    // TODO: implement host device partitioning
    throw runtime_error(
        "Partitioning to subdevices of the host device is not implemented yet",
        PI_INVALID_DEVICE);

  if (!is_partition_supported(info::partition_property::partition_equally)) {
    throw cl::sycl::feature_not_supported();
  }
  size_t SubDevicesCount =
      get_info<info::device::max_compute_units>() / ComputeUnits;
  const cl_device_partition_property Properties[3] = {
      CL_DEVICE_PARTITION_EQUALLY, (cl_device_partition_property)ComputeUnits,
      0};
  return create_sub_devices(Properties, SubDevicesCount);
}

vector_class<device>
device_impl::create_sub_devices(const vector_class<size_t> &Counts) const {

  if (MIsHostDevice)
    // TODO: implement host device partitioning
    throw runtime_error(
        "Partitioning to subdevices of the host device is not implemented yet",
        PI_INVALID_DEVICE);

  if (!is_partition_supported(
          info::partition_property::partition_by_counts)) {
    throw cl::sycl::feature_not_supported();
  }
  static const cl_device_partition_property P[] = {
      CL_DEVICE_PARTITION_BY_COUNTS, CL_DEVICE_PARTITION_BY_COUNTS_LIST_END,
      0};
  vector_class<cl_device_partition_property> Properties(P, P + 3);
  Properties.insert(Properties.begin() + 1, Counts.begin(), Counts.end());
  return create_sub_devices(Properties.data(), Counts.size());
}

vector_class<device> device_impl::create_sub_devices(
    info::partition_affinity_domain AffinityDomain) const {

  if (MIsHostDevice)
    // TODO: implement host device partitioning
    throw runtime_error(
        "Partitioning to subdevices of the host device is not implemented yet",
        PI_INVALID_DEVICE);

  if (!is_partition_supported(
          info::partition_property::partition_by_affinity_domain) ||
      !is_affinity_supported(AffinityDomain)) {
    throw cl::sycl::feature_not_supported();
  }
  const cl_device_partition_property Properties[3] = {
      CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN,
      (cl_device_partition_property)AffinityDomain, 0};
  size_t SubDevicesCount =
      get_info<info::device::partition_max_sub_devices>();
  return create_sub_devices(Properties, SubDevicesCount);
}

} // namespace detail
} // namespace sycl
} // __SYCL_INLINE_NAMESPACE(cl)
