#include "StdAfx.h"
#include "EterBase/MapLoadTrace.h"
#include "ImageDecoder.h"
#include "EterImageLib/DDSImageData.h"
#include <stb_image.h>
#include <limits>
#include <memory>

bool CImageDecoder::DecodeImage(const void* pData, size_t dataSize, TDecodedImageData& outImage)
{
	if (!pData || dataSize == 0)
		return false;

	outImage.Clear();

	if (DecodeDDS(pData, dataSize, outImage))
		return true;

	if (DecodeSTB(pData, dataSize, outImage))
		return true;

	return false;
}

bool CImageDecoder::DecodeDDS(const void* pData, size_t dataSize, TDecodedImageData& outImage)
{
	if (dataSize < 4)
		return false;

	const uint32_t DDS_MAGIC = 0x20534444;
	uint32_t magic;
	memcpy(&magic, pData, sizeof(magic));

	if (magic != DDS_MAGIC)
		return false;

	if (dataSize < 128)
		return false;

	struct DDSHeader
	{
		uint32_t magic;
		uint32_t size;
		uint32_t flags;
		uint32_t height;
		uint32_t width;
		uint32_t pitchOrLinearSize;
		uint32_t depth;
		uint32_t mipMapCount;
		uint32_t reserved1[11];
	};

	const DDSHeader* header = (const DDSHeader*)pData;

	outImage.width = header->width;
	outImage.height = header->height;
	outImage.mipLevels = (header->mipMapCount > 0) ? header->mipMapCount : 1;
	outImage.isDDS = true;
	outImage.format = TDecodedImageData::FORMAT_DDS;

	outImage.pixels.resize(dataSize);
	memcpy(outImage.pixels.data(), pData, dataSize);

	return true;
}

bool CImageDecoder::DecodeSTB(const void* pData, size_t dataSize, TDecodedImageData& outImage)
{
    MapLoadTrace::Scope p0lScope("Textures","PNG TGA JPG decode","cpu");

	if (!pData || dataSize > size_t((std::numeric_limits<int>::max)())) return false;
	int width = 0, height = 0, channels = 0;
	if (!stbi_info_from_memory(static_cast<const stbi_uc*>(pData), static_cast<int>(dataSize), &width, &height, &channels) ||
		width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
		size_t(width) > (256u * 1024u * 1024u) / 4u / size_t(height)) return false;

	unsigned char* imageData = stbi_load_from_memory(
		(const stbi_uc*)pData,
		(int)dataSize,
		&width,
		&height,
		&channels,
		4
	);

	if (!imageData)
		return false;
	const std::unique_ptr<unsigned char, decltype(&stbi_image_free)> ownedImage(imageData, stbi_image_free);

	outImage.width = width;
	outImage.height = height;
	outImage.format = TDecodedImageData::FORMAT_RGBA8;
	outImage.isDDS = false;
	outImage.mipLevels = 1;

	size_t pixelDataSize = size_t(width) * size_t(height) * 4u;
	outImage.pixels.resize(pixelDataSize);
	memcpy(outImage.pixels.data(), imageData, pixelDataSize);

	return true;
}
