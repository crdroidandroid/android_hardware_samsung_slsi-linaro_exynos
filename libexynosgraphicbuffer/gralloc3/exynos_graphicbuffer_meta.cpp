/*
 * Copyright (C) 2020 Samsung Electronics Co. Ltd.
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

#include <log/log.h>
#include "ExynosGraphicBuffer.h"
#include "mali_gralloc_buffer.h"
#include "gralloc_buffer_priv.h"

#define UNUSED(x) ((void)x)

using namespace android;
using namespace vendor::graphics;

int ExynosGraphicBufferMeta::is_afbc(buffer_handle_t buffer_hnd_p)
{
	const private_handle_t *gralloc_hnd = static_cast<const private_handle_t *>(buffer_hnd_p);

	if (!gralloc_hnd)
		return 0;

	return gralloc_hnd->is_compressible;
}

#define GRALLOC_META_GETTER(__type__, __name__, __member__) \
__type__ ExynosGraphicBufferMeta::get_##__name__(buffer_handle_t hnd) \
{ \
	const private_handle_t *gralloc_hnd = static_cast<const private_handle_t *>(hnd); \
	if (!gralloc_hnd) return 0; \
	return gralloc_hnd->__member__; \
} \


GRALLOC_META_GETTER(uint32_t, format, format);
GRALLOC_META_GETTER(uint64_t, internal_format, internal_format);
GRALLOC_META_GETTER(uint64_t, frameworkFormat, req_format);

GRALLOC_META_GETTER(int, width, width);
GRALLOC_META_GETTER(int, height, height);
GRALLOC_META_GETTER(uint32_t, stride, stride);
GRALLOC_META_GETTER(uint32_t, vstride, plane_info[0].alloc_height);

GRALLOC_META_GETTER(uint64_t, producer_usage, producer_usage);
GRALLOC_META_GETTER(uint64_t, consumer_usage, consumer_usage);

GRALLOC_META_GETTER(uint64_t, flags, flags);

int ExynosGraphicBufferMeta::get_dataspace(buffer_handle_t hnd)
{
	const private_handle_t *gralloc_hnd = static_cast<const private_handle_t *>(hnd);

	if (!gralloc_hnd)
		return -1;

	int attr_fd = gralloc_hnd->get_share_attr_fd();
	if (attr_fd < 0)
	{
		ALOGE("Shared attribute region not available to be mapped");
		return -1;
	}
	attr_region* region;
	region = (attr_region *) mmap(NULL, sizeof(attr_region), PROT_READ, MAP_SHARED, attr_fd, 0);
	if (region == NULL)
		return -1;
	else if (region == MAP_FAILED)
		return -1;

	int dataspace = region->force_dataspace == -1 ? region->dataspace : region->force_dataspace;

	munmap(region, sizeof(attr_region));

	return dataspace;
}

int ExynosGraphicBufferMeta::get_fd(buffer_handle_t hnd, int num)
{
	const private_handle_t *gralloc_hnd = static_cast<const private_handle_t *>(hnd);

	if (!gralloc_hnd)
		return -1;

	if (num > 2)
		return -1;

	return gralloc_hnd->fds[num];
}

int ExynosGraphicBufferMeta::get_size(buffer_handle_t hnd, int num)
{
	const private_handle_t *gralloc_hnd = static_cast<const private_handle_t *>(hnd);

	if (!gralloc_hnd)
		return 0;

	if (num > 2)
		return 0;

	return gralloc_hnd->sizes[num];
}


uint64_t ExynosGraphicBufferMeta::get_usage(buffer_handle_t hnd)
{
	const private_handle_t *gralloc_hnd = static_cast<const private_handle_t *>(hnd);

	if (!gralloc_hnd)
		return 0;

	return gralloc_hnd->producer_usage | gralloc_hnd->consumer_usage;
}

void* ExynosGraphicBufferMeta::get_video_metadata(buffer_handle_t hnd)
{
	private_handle_t *gralloc_hnd =
		static_cast<private_handle_t *>(const_cast<native_handle_t *>(hnd));

	if (!gralloc_hnd)
		return nullptr;

	if (gralloc_hnd->flags &
		(private_handle_t::PRIV_FLAGS_USES_3PRIVATE_DATA | private_handle_t::PRIV_FLAGS_USES_2PRIVATE_DATA))
	{
		int idx = -1;

		if (gralloc_hnd->flags & private_handle_t::PRIV_FLAGS_USES_2PRIVATE_DATA)
			idx = 1;
		else if (gralloc_hnd->flags & private_handle_t::PRIV_FLAGS_USES_3PRIVATE_DATA)
			idx = 2;

		if (gralloc_hnd->bases[idx] == 0)
			ALOGE("buffer_handle(%p) did not map video metadata", hnd);

		return reinterpret_cast<void*>(gralloc_hnd->bases[idx]);
	}
	else
	{
		ALOGE("buffer_handle(%p) does not contain video metadata", hnd);
		return nullptr;
	}
}

int ExynosGraphicBufferMeta::get_video_metadata_fd(buffer_handle_t hnd)
{
	const private_handle_t *gralloc_hnd = static_cast<const private_handle_t *>(hnd);

	if (!gralloc_hnd)
		return -EINVAL;

	int idx = -1;

	if (gralloc_hnd->flags & ExynosGraphicBufferMeta::PRIV_FLAGS_USES_2PRIVATE_DATA)
		idx = 1;
	else if (gralloc_hnd->flags & ExynosGraphicBufferMeta::PRIV_FLAGS_USES_3PRIVATE_DATA)
		idx = 2;

	if (idx < 0)
		return -EINVAL;

	return gralloc_hnd->fds[idx];
}

/* This function is not supported with gralloc3. return nullptr */
void* ExynosGraphicBufferMeta::get_video_metadata_roiinfo(buffer_handle_t hnd)
{
	UNUSED(hnd);
	return nullptr;
}

void ExynosGraphicBufferMeta::init(const buffer_handle_t handle)
{
	const private_handle_t *gralloc_hnd = static_cast<const private_handle_t *>(handle);

	if (!gralloc_hnd)
		return;

	fd  = gralloc_hnd->fds[0];
	fd1 = gralloc_hnd->fds[1];
	fd2 = gralloc_hnd->fds[2];

	size  = gralloc_hnd->sizes[0];
	size1 = gralloc_hnd->sizes[1];
	size2 = gralloc_hnd->sizes[2];

	internal_format = gralloc_hnd->internal_format;
	frameworkFormat = gralloc_hnd->req_format;

	width  = gralloc_hnd->width;
	height = gralloc_hnd->height;
	stride  = gralloc_hnd->stride;;
	vstride = gralloc_hnd->plane_info[0].alloc_height;

	producer_usage = gralloc_hnd->producer_usage;
	consumer_usage = gralloc_hnd->consumer_usage;

	flags = gralloc_hnd->flags;
}

ExynosGraphicBufferMeta::ExynosGraphicBufferMeta(buffer_handle_t handle)
{
	init(handle);
}

