/*
 * Modern effects for a modern Streamer
 * Copyright (C) 2020 Michael Fabian Dirks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "nvidia-cuda.hpp"
#include <mutex>
#include "util/util-logging.hpp"

#ifdef _DEBUG
#define ST_PREFIX "<%s> "
#define D_LOG_ERROR(x, ...) P_LOG_ERROR(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_WARNING(x, ...) P_LOG_WARN(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_INFO(x, ...) P_LOG_INFO(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_DEBUG(x, ...) P_LOG_DEBUG(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#else
#define ST_PREFIX "<nvidia::cuda::cuda> "
#define D_LOG_ERROR(...) P_LOG_ERROR(ST_PREFIX __VA_ARGS__)
#define D_LOG_WARNING(...) P_LOG_WARN(ST_PREFIX __VA_ARGS__)
#define D_LOG_INFO(...) P_LOG_INFO(ST_PREFIX __VA_ARGS__)
#define D_LOG_DEBUG(...) P_LOG_DEBUG(ST_PREFIX __VA_ARGS__)
#endif

#if defined(_WIN32) || defined(_WIN64)
#define CUDA_NAME "nvcuda.dll"
#else
#define CUDA_NAME "libcuda.so.1"
#endif

#define CUDA_LOAD_SYMBOL(NAME)                                                            \
	{                                                                                     \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#NAME));            \
		if (!NAME)                                                                        \
			throw std::runtime_error("Failed to load '" #NAME "' from '" CUDA_NAME "'."); \
	}
#define CUDA_LOAD_SYMBOL_OPT(NAME)                                              \
	{                                                                           \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#NAME));  \
		if (!NAME)                                                              \
			D_LOG_WARNING("Loading of optional symbol '" #NAME "' failed.", 0); \
	}

#define CUDA_LOAD_SYMBOL_EX(NAME, OVERRIDE)                                               \
	{                                                                                     \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#OVERRIDE));        \
		if (!NAME)                                                                        \
			throw std::runtime_error("Failed to load '" #NAME "' from '" CUDA_NAME "'."); \
	}
#define CUDA_LOAD_SYMBOL_OPT_EX(NAME, OVERRIDE)                                    \
	{                                                                              \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#OVERRIDE)); \
		if (!NAME)                                                                 \
			D_LOG_WARNING("Loading of optional symbol '" #NAME "' failed.", 0);    \
	}

#define CUDA_LOAD_SYMBOL_V2(NAME)                                                         \
	{                                                                                     \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#NAME "_v2"));      \
		if (!NAME)                                                                        \
			throw std::runtime_error("Failed to load '" #NAME "' from '" CUDA_NAME "'."); \
	}
#define CUDA_LOAD_SYMBOL_OPT_V2(NAME)                                                \
	{                                                                                \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#NAME "_v2")); \
		if (!NAME)                                                                   \
			D_LOG_WARNING("Loading of optional symbol '" #NAME "' failed.", 0);      \
	}

nvidia::cuda::cuda::~cuda()
{
	D_LOG_DEBUG("Finalizing... (Addr: 0x%" PRIuPTR ")", this);
}

nvidia::cuda::cuda::cuda() : _library()
{
	int32_t cuda_version = 0;

	D_LOG_DEBUG("Initialization... (Addr: 0x%" PRIuPTR ")", this);

	_library = util::library::load(std::string_view(CUDA_NAME));

	{ // 1. Load critical initialization functions.
		// Initialization
		CUDA_LOAD_SYMBOL(cuInit);

		// Version Management
		CUDA_LOAD_SYMBOL(cuDriverGetVersion);
	}

	{ // 2. Get the CUDA Driver version and log it.
		if (cuDriverGetVersion(&cuda_version) == result::SUCCESS) {
			int32_t major = cuda_version / 1000;
			int32_t minor = (cuda_version % 1000) / 10;
			int32_t patch = (cuda_version % 10);
			D_LOG_INFO("Driver reported CUDA version: %" PRId32 ".%" PRId32 ".%" PRId32, major, minor, patch);
		} else {
			D_LOG_WARNING("Failed to query NVIDIA CUDA Driver for version.", 0);
		}
	}

	{ // 3. Load remaining functions.
		// Device Management
		// - Not yet needed.

		// Primary Context Management
		CUDA_LOAD_SYMBOL(cuDevicePrimaryCtxRetain);
		CUDA_LOAD_SYMBOL_V2(cuDevicePrimaryCtxRelease);
		CUDA_LOAD_SYMBOL_OPT_V2(cuDevicePrimaryCtxSetFlags);

		// Context Management
		CUDA_LOAD_SYMBOL_V2(cuCtxCreate);
		CUDA_LOAD_SYMBOL_V2(cuCtxDestroy);
		CUDA_LOAD_SYMBOL_V2(cuCtxPushCurrent);
		CUDA_LOAD_SYMBOL_V2(cuCtxPopCurrent);
		CUDA_LOAD_SYMBOL_OPT(cuCtxGetCurrent);
		CUDA_LOAD_SYMBOL_OPT(cuCtxSetCurrent);
		CUDA_LOAD_SYMBOL(cuCtxGetStreamPriorityRange);
		CUDA_LOAD_SYMBOL(cuCtxSynchronize);

		// Module Management
		// - Not yet needed.

		// Memory Management
		CUDA_LOAD_SYMBOL_V2(cuMemAlloc);
		CUDA_LOAD_SYMBOL_V2(cuMemAllocPitch);
		CUDA_LOAD_SYMBOL_V2(cuMemFree);
		CUDA_LOAD_SYMBOL(cuMemcpy);
		CUDA_LOAD_SYMBOL_V2(cuMemcpy2D);
		CUDA_LOAD_SYMBOL_V2(cuMemcpy2DAsync);
		CUDA_LOAD_SYMBOL_OPT_V2(cuArrayGetDescriptor);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyAtoA);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyAtoD);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyAtoH);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyAtoHAsync);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyDtoA);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyDtoD);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyDtoH);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyDtoHAsync);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyHtoA);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyHtoAAsync);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyHtoD);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyHtoDAsync);
		CUDA_LOAD_SYMBOL_OPT_V2(cuMemHostGetDevicePointer);

		// Virtual Memory Management
		// - Not yet needed.

		// Stream Ordered Memory Allocator
		// - Not yet needed.

		// Unified Addressing
		// - Not yet needed.

		// Stream Management
		CUDA_LOAD_SYMBOL(cuStreamCreate);
		CUDA_LOAD_SYMBOL_V2(cuStreamDestroy);
		CUDA_LOAD_SYMBOL(cuStreamSynchronize);
		CUDA_LOAD_SYMBOL_OPT(cuStreamCreateWithPriority);
		CUDA_LOAD_SYMBOL_OPT(cuStreamGetPriority);

		// Event Management
		// - Not yet needed.

		// External Resource Interoperability (CUDA 11.1+)
		// - Not yet needed.

		// Stream Memory Operations
		// - Not yet needed.

		// Execution Control
		// - Not yet needed.

		// Graph Management
		// - Not yet needed.

		// Occupancy
		// - Not yet needed.

		// Texture Object Management
		// - Not yet needed.

		// Surface Object Management
		// - Not yet needed.

		// Peer Context Memory Access
		// - Not yet needed.

		// Graphics Interoperability
		CUDA_LOAD_SYMBOL(cuGraphicsMapResources);
		CUDA_LOAD_SYMBOL(cuGraphicsSubResourceGetMappedArray);
		CUDA_LOAD_SYMBOL(cuGraphicsUnmapResources);
		CUDA_LOAD_SYMBOL(cuGraphicsUnregisterResource);

		// Driver Entry Point Access
		// - Not yet needed.

		// Profiler Control
		// - Not yet needed.

		// OpenGL Interoperability
		// - Not yet needed.

		// VDPAU Interoperability
		// - Not yet needed.

		// EGL Interoperability
		// - Not yet needed.

#ifdef WIN32
		// Direct3D9 Interoperability
		// - Not yet needed.

		// Direct3D10 Interoperability
		CUDA_LOAD_SYMBOL(cuD3D10GetDevice);
		CUDA_LOAD_SYMBOL_OPT(cuGraphicsD3D10RegisterResource);

		// Direct3D11 Interoperability
		CUDA_LOAD_SYMBOL(cuD3D11GetDevice);
		CUDA_LOAD_SYMBOL_OPT(cuGraphicsD3D11RegisterResource);
#endif
	}

	// Initialize CUDA
	cuInit(0);
}

int32_t nvidia::cuda::cuda::version()
{
	int32_t v = 0;
	cuDriverGetVersion(&v);
	return v;
}

std::shared_ptr<nvidia::cuda::cuda> nvidia::cuda::cuda::get()
{
	static std::weak_ptr<nvidia::cuda::cuda> instance;
	static std::mutex                        lock;

	std::unique_lock<std::mutex> ul(lock);
	if (instance.expired()) {
		auto hard_instance = std::make_shared<nvidia::cuda::cuda>();
		instance           = hard_instance;
		return hard_instance;
	}
	return instance.lock();
}
