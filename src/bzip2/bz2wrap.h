#pragma once
#include <cstddef>
#include <span>
#include <vector>

enum BZ2WCompressResult
{
	BZ2WCompressOk,
	BZ2WCompressNomem,
	BZ2WCompressLimit,
};
BZ2WCompressResult BZ2WCompress(std::vector<char> &dest, std::span<const char> srcData, size_t maxSize = 0);

enum BZ2WDecompressResult
{
	BZ2WDecompressOk,
	BZ2WDecompressNomem,
	BZ2WDecompressLimit,
	BZ2WDecompressType,
	BZ2WDecompressBad,
	BZ2WDecompressEof,
};
BZ2WDecompressResult BZ2WDecompress(std::vector<char> &dest, std::span<const char> srcData, size_t maxSize = 0);
