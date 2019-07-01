/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CROS_GRALLOC1_MODULE_H
#define CROS_GRALLOC1_MODULE_H

#include <memory.h>

#include "../cros_gralloc_driver.h"

#include <hardware/gralloc1.h>
#include <utils/Log.h>
#include <unistd.h>

struct cros_gralloc_module;

namespace android
{

typedef enum {
	CROS_GRALLOC_ERROR_NONE = 0,
	CROS_GRALLOC_ERROR_BAD_DESCRIPTOR = 1,
	CROS_GRALLOC_ERROR_BAD_HANDLE = 2,
	CROS_GRALLOC_ERROR_BAD_VALUE = 3,
	CROS_GRALLOC_ERROR_NOT_SHARED = 4,
	CROS_GRALLOC_ERROR_NO_RESOURCES = 5,
	CROS_GRALLOC_ERROR_UNDEFINED = 6,
	CROS_GRALLOC_ERROR_UNSUPPORTED = 7,
} cros_gralloc_error_t;

class CrosGralloc1 : public gralloc1_device_t
{
      public:
	CrosGralloc1();
	~CrosGralloc1();

	bool Init();

	static int HookDevOpen(const struct hw_module_t *mod, const char *name,
			       struct hw_device_t **device);
	static int HookDevClose(hw_device_t *dev);

      private:
	static inline CrosGralloc1 *getAdapter(gralloc1_device_t *device)
	{
		return static_cast<CrosGralloc1 *>(device);
	}

	// getCapabilities

	void doGetCapabilities(uint32_t *outCount,
			       int32_t * /*gralloc1_capability_t*/ outCapabilities);
	static void getCapabilitiesHook(gralloc1_device_t *device, uint32_t *outCount,
					int32_t * /*gralloc1_capability_t*/ outCapabilities)
	{
		getAdapter(device)->doGetCapabilities(outCount, outCapabilities);
	};

	// getFunction

	gralloc1_function_pointer_t
	    doGetFunction(int32_t /*gralloc1_function_descriptor_t*/ descriptor);
	static gralloc1_function_pointer_t
	getFunctionHook(gralloc1_device_t *device,
			int32_t /*gralloc1_function_descriptor_t*/ descriptor)
	{
		return getAdapter(device)->doGetFunction(descriptor);
	}

	// dump

	void dump(uint32_t *outSize, char *outBuffer);
	static void dumpHook(gralloc1_device_t *device, uint32_t *outSize, char *outBuffer)
	{
		return getAdapter(device)->dump(outSize, outBuffer);
	}

	// Buffer descriptor functions

	int32_t setConsumerUsage(gralloc1_buffer_descriptor_t descriptorId, uint64_t intUsage);

	int32_t setProducerUsage(gralloc1_buffer_descriptor_t descriptorId, uint64_t intUsage);

	int32_t setDimensions(gralloc1_buffer_descriptor_t descriptorId, uint32_t width,
			      uint32_t height);

	int32_t setFormat(gralloc1_buffer_descriptor_t descriptorId, int32_t format);
        int32_t setInterlace(buffer_handle_t buffer, uint32_t interlace);
        int32_t setProtectionInfo(buffer_handle_t buffer, uint32_t protection_info);

	int32_t createDescriptor(gralloc1_buffer_descriptor_t *outDescriptor);
	static int32_t createDescriptorHook(gralloc1_device_t *device,
					    gralloc1_buffer_descriptor_t *outDescriptor)
	{
		return getAdapter(device)->createDescriptor(outDescriptor);
	}

	int32_t destroyDescriptor(gralloc1_buffer_descriptor_t descriptor);
	static int32_t destroyDescriptorHook(gralloc1_device_t *device,
					     gralloc1_buffer_descriptor_t descriptor)
	{
		return getAdapter(device)->destroyDescriptor(descriptor);
	}

	static int32_t setConsumerUsageHook(gralloc1_device_t *device,
					    gralloc1_buffer_descriptor_t descriptorId,
					    uint64_t intUsage)
	{
		return getAdapter(device)->setConsumerUsage(descriptorId, intUsage);
	}

	static int32_t setDimensionsHook(gralloc1_device_t *device,
					 gralloc1_buffer_descriptor_t descriptorId, uint32_t width,
					 uint32_t height)
	{
		return getAdapter(device)->setDimensions(descriptorId, width, height);
	}

	static int32_t setFormatHook(gralloc1_device_t *device,
				     gralloc1_buffer_descriptor_t descriptorId, int32_t format)
	{
		return getAdapter(device)->setFormat(descriptorId, format);
	}

        static int32_t setInterlaceHook(gralloc1_device_t *device,
                                     buffer_handle_t buffer, uint32_t interlace)
        {
                return getAdapter(device)->setInterlace(buffer, interlace);
        }

        static int32_t setProtectionInfoHook(gralloc1_device_t *device,
                                     buffer_handle_t buffer, uint32_t protection_info)
        {
                return getAdapter(device)->setProtectionInfo(buffer, protection_info);
        }

	static int32_t setProducerUsageHook(gralloc1_device_t *device,
					    gralloc1_buffer_descriptor_t descriptorId,
					    uint64_t intUsage)
	{
		return getAdapter(device)->setProducerUsage(descriptorId, intUsage);
	}

	int32_t getNumFlexPlanes(buffer_handle_t buffer, uint32_t *outNumPlanes);
	static int32_t getNumFlexPlanesHook(gralloc1_device_t *device, buffer_handle_t buffer,
					    uint32_t *outNumPlanes)
	{
		return getAdapter(device)->getNumFlexPlanes(buffer, outNumPlanes);
	}

	int32_t getBackingStore(buffer_handle_t buffer, gralloc1_backing_store_t *outStore);
	static int32_t getBackingStoreHook(gralloc1_device_t *device, buffer_handle_t buffer,
					   gralloc1_backing_store_t *outStore)
	{
		return getAdapter(device)->getBackingStore(buffer, outStore);
	}

	int32_t getConsumerUsage(buffer_handle_t buffer,
				 uint64_t * /*gralloc1_consumer_usage_t*/ outUsage);
	static int32_t getConsumerUsageHook(gralloc1_device_t *device, buffer_handle_t buffer,
					    uint64_t * /*gralloc1_consumer_usage_t*/ outUsage)
	{
		return getAdapter(device)->getConsumerUsage(buffer, outUsage);
	}

	int32_t getDimensions(buffer_handle_t buffer, uint32_t *outWidth, uint32_t *outHeight);
	static int32_t getDimensionsHook(gralloc1_device_t *device, buffer_handle_t buffer,
					 uint32_t *outWidth, uint32_t *outHeight)
	{
		return getAdapter(device)->getDimensions(buffer, outWidth, outHeight);
	}

	int32_t getFormat(buffer_handle_t buffer, int32_t *outFormat);
	static int32_t getFormatHook(gralloc1_device_t *device, buffer_handle_t buffer,
				     int32_t *outFormat)
	{
		return getAdapter(device)->getFormat(buffer, outFormat);
	}

	int32_t validateBufferSize(buffer_handle_t buffer,
				   const gralloc1_buffer_descriptor_info_t *descriptorInfo,
				   uint32_t stride);
	static int32_t
	validateBufferSizeHook(gralloc1_device_t *device, buffer_handle_t buffer,
			       const gralloc1_buffer_descriptor_info_t *descriptorInfo,
			       uint32_t stride)
	{
		return getAdapter(device)->validateBufferSize(buffer, descriptorInfo, stride);
	}

	int32_t getTransportSize(buffer_handle_t buffer, uint32_t *outNumFds, uint32_t *outNumInts);
	static int32_t getTransportSizeHook(gralloc1_device_t *device, buffer_handle_t buffer,
					    uint32_t *outNumFds, uint32_t *outNumInts)
	{
		return getAdapter(device)->getTransportSize(buffer, outNumFds, outNumInts);
	}

	int32_t importBuffer(const buffer_handle_t rawHandle, buffer_handle_t *outBuffer);
	static int32_t importBufferHook(gralloc1_device_t *device, const buffer_handle_t rawHandle,
					buffer_handle_t *outBuffer)
	{
		return getAdapter(device)->importBuffer(rawHandle, outBuffer);
	}

	int32_t getProducerUsage(buffer_handle_t buffer,
				 uint64_t * /*gralloc1_producer_usage_t*/ outUsage);
	static int32_t getProducerUsageHook(gralloc1_device_t *device, buffer_handle_t buffer,
					    uint64_t * /*gralloc1_producer_usage_t*/ outUsage)
	{
		return getAdapter(device)->getProducerUsage(buffer, outUsage);
	}

	int32_t getStride(buffer_handle_t buffer, uint32_t *outStride);
	static int32_t getStrideHook(gralloc1_device_t *device, buffer_handle_t buffer,
				     uint32_t *outStride)
	{
		return getAdapter(device)->getStride(buffer, outStride);
	}

	int32_t getPrime(buffer_handle_t buffer, uint32_t *prime);
	static int32_t getPrimeHook(gralloc1_device_t *device, buffer_handle_t buffer,
				     uint32_t *prime)
	{
		return getAdapter(device)->getPrime(buffer, prime);
	}

	int32_t getByteStride(buffer_handle_t buffer, uint32_t *outStride, uint32_t size);
	static int32_t getByteStrideHook(gralloc1_device_t *device, buffer_handle_t buffer,
				     uint32_t *outStride, uint32_t size)
	{
		return getAdapter(device)->getByteStride(buffer, outStride, size);
	}

	// Buffer Management functions
	int32_t allocate(struct cros_gralloc_buffer_descriptor *descriptor,
			 buffer_handle_t *outBufferHandle);
	static int32_t allocateBuffers(gralloc1_device_t *device, uint32_t numDescriptors,
				       const gralloc1_buffer_descriptor_t *descriptors,
				       buffer_handle_t *outBuffers);

	int32_t release(buffer_handle_t bufferHandle);
	int32_t retain(buffer_handle_t bufferHandle);

	// Member function pointer 'member' will either be retain or release
	template <int32_t (CrosGralloc1::*member)(buffer_handle_t bufferHandle)>
	static int32_t managementHook(gralloc1_device_t *device, buffer_handle_t bufferHandle)
	{
		auto adapter = getAdapter(device);
		return ((*adapter).*member)(bufferHandle);
	}

	// Buffer access functions
	int32_t lock(buffer_handle_t bufferHandle, gralloc1_producer_usage_t producerUsage,
		     gralloc1_consumer_usage_t consumerUsage, const gralloc1_rect_t &accessRegion,
		     void **outData, int32_t acquireFence);
	int32_t lockFlex(buffer_handle_t bufferHandle, gralloc1_producer_usage_t producerUsage,
			 gralloc1_consumer_usage_t consumerUsage,
			 const gralloc1_rect_t &accessRegion, struct android_flex_layout *outFlex,
			 int32_t acquireFence);
	int32_t lockYCbCr(buffer_handle_t bufferHandle, gralloc1_producer_usage_t producerUsage,
			  gralloc1_consumer_usage_t consumerUsage,
			  const gralloc1_rect_t &accessRegion, struct android_ycbcr *outFlex,
			  int32_t acquireFence);

	template <typename OUT,
		  int32_t (CrosGralloc1::*member)(
		      buffer_handle_t bufferHandle, gralloc1_producer_usage_t,
		      gralloc1_consumer_usage_t, const gralloc1_rect_t &, OUT *, int32_t)>
	static int32_t lockHook(gralloc1_device_t *device, buffer_handle_t bufferHandle,
				uint64_t /*gralloc1_producer_usage_t*/ uintProducerUsage,
				uint64_t /*gralloc1_consumer_usage_t*/ uintConsumerUsage,
				const gralloc1_rect_t *accessRegion, OUT *outData,
				int32_t acquireFenceFd)
	{
		auto adapter = getAdapter(device);

		// Exactly one of producer and consumer usage must be *_USAGE_NONE,
		// but we can't check this until the upper levels of the framework
		// correctly distinguish between producer and consumer usage
		/*
		bool hasProducerUsage =
			uintProducerUsage != GRALLOC1_PRODUCER_USAGE_NONE;
		bool hasConsumerUsage =
			uintConsumerUsage != GRALLOC1_CONSUMER_USAGE_NONE;
		if (hasProducerUsage && hasConsumerUsage ||
			!hasProducerUsage && !hasConsumerUsage) {
		    return static_cast<int32_t>(GRALLOC1_ERROR_BAD_VALUE);
		}
		*/

		auto producerUsage = static_cast<gralloc1_producer_usage_t>(uintProducerUsage);
		auto consumerUsage = static_cast<gralloc1_consumer_usage_t>(uintConsumerUsage);

		if (!outData) {
			const auto producerCpuUsage =
			    GRALLOC1_PRODUCER_USAGE_CPU_READ | GRALLOC1_PRODUCER_USAGE_CPU_WRITE;
			if (producerUsage & (producerCpuUsage != 0)) {
				return CROS_GRALLOC_ERROR_BAD_VALUE;
			}
			if (consumerUsage & (GRALLOC1_CONSUMER_USAGE_CPU_READ != 0)) {
				return CROS_GRALLOC_ERROR_BAD_VALUE;
			}
		}

		if (!accessRegion) {
			ALOGE("accessRegion is null");
			return CROS_GRALLOC_ERROR_BAD_VALUE;
		}

		return ((*adapter).*member)(bufferHandle, producerUsage, consumerUsage,
					    *accessRegion, outData, acquireFenceFd);
	}

	int32_t unlock(buffer_handle_t bufferHandle, int32_t *outReleaseFence);
	static int32_t unlockHook(gralloc1_device_t *device, buffer_handle_t bufferHandle,
				  int32_t *outReleaseFenceFd)
	{
		auto adapter = getAdapter(device);
		*outReleaseFenceFd = -1;
		int32_t releaseFence;
		auto error = adapter->unlock(bufferHandle, &releaseFence);
		if (error == CROS_GRALLOC_ERROR_NONE && releaseFence > 0) {
			*outReleaseFenceFd = dup(releaseFence);
		}
		return error;
	}

	int32_t setModifier(gralloc1_buffer_descriptor_t descriptor, uint64_t modifier);
	static int32_t setModifierHook(gralloc1_device_t *device,
				       gralloc1_buffer_descriptor_t descriptor, uint64_t modifier)
	{
		return getAdapter(device)->setModifier(descriptor, modifier);
	}

	// Adapter internals
	std::unique_ptr<cros_gralloc_driver> driver;
};

} // namespace android

#endif
