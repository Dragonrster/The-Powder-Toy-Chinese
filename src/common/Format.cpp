
#include <algorithm>
#include <ctime>
#include <cstring>
#include <string>
#include <stdexcept>
#include <iostream>
#include <iterator>
#include <zlib.h>
#include <cstdio>
#include "Format.h"
#include "graphics/Pixel.h"
#include "graphics/VideoBuffer.h"


std::string Format::ToLower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), ::tolower);
	return text;
}

std::string Format::ToUpper(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), ::toupper);
	return text;
}

std::string Format::URLEncode(std::string source)
{
	char * src = (char *)source.c_str();
	char * dst = new char[(source.length()*3)+2];
	std::fill(dst, dst+(source.length()*3)+2, 0);

	char *d;
	for (d = dst; *d; d++) ;

	for (unsigned char *s = (unsigned char *)src; *s; s++)
	{
		if ((*s>='0' && *s<='9') ||
		        (*s>='a' && *s<='z') ||
		        (*s>='A' && *s<='Z'))
			*(d++) = *s;
		else
		{
			*(d++) = '%';
			*(d++) = hex[*s>>4];
			*(d++) = hex[*s&15];
		}
	}
	*d = 0;

	std::string finalString(dst);
	delete[] dst;
	return finalString;
}

std::string Format::UnixtimeToDate(time_t unixtime, std::string dateFormat)
{
	struct tm * timeData;
	char buffer[128];

	timeData = localtime(&unixtime);

	strftime(buffer, 128, dateFormat.c_str(), timeData);
	return std::string(buffer);
}

std::string Format::UnixtimeToDateMini(time_t unixtime)
{
	time_t currentTime = time(NULL);
	struct tm currentTimeData = *localtime(&currentTime);
	struct tm timeData = *localtime(&unixtime);

	if(currentTimeData.tm_year != timeData.tm_year)
	{
		return UnixtimeToDate(unixtime, "%b %Y");
	}
	else if(currentTimeData.tm_mon != timeData.tm_mon || currentTimeData.tm_mday != timeData.tm_mday)
	{
		return UnixtimeToDate(unixtime, "%d %B");
	}
	else
	{
		return UnixtimeToDate(unixtime, "%H:%M:%S");
	}
}


// Strips stuff from a string. Can strip all non ascii characters (excluding color and newlines), strip all color, strip all newlines, or strip all non 0-9 characters
std::string Format::CleanString(std::string dirtyString, bool ascii, bool color, bool newlines, bool numeric, bool icons)
{
	for (size_t i = 0; i < dirtyString.size(); i++)
	{
		switch(dirtyString[i])
		{
		case '\b':
			if (color)
			{
				dirtyString.erase(i, 2);
				i--;
			}
			else
				i++;
			break;
		case '\x0E':
			if (color)
			{
				dirtyString.erase(i, 1);
				i--;
			}
			break;
		case '\x0F':
			if (color)
			{
				dirtyString.erase(i, 4);
				i--;
			}
			else
				i += 3;
			break;
		// erase these without question, first two are control characters used for the cursor
		// second is used internally to denote automatically inserted newline
		case '\x01':
		case '\x02':
		case '\r':
			dirtyString.erase(i, 1);
			i--;
			break;
		case '\n':
			if (newlines)
				dirtyString[i] = ' ';
			break;
		case '\xEE':
		{
			if (!icons || ConvertFontIcon(dirtyString, i) == 0)
			{
				dirtyString.erase(i, 3);
				i--;
			}
			else
			{
				i += 2;
			}
			break;
		}
		default:
			if (icons && dirtyString[i] >= (char)0x80 && dirtyString[i] <= (char)0xF0)
			{
				break;
			}
			// Allow UTF-8 multi-byte sequences through ASCII filter
			if (ascii && (dirtyString[i] & 0x80))
			{
				int len = 0;
				unsigned char c = (unsigned char)dirtyString[i];
				if      ((c & 0xE0) == 0xC0) len = 1;
				else if ((c & 0xF0) == 0xE0) len = 2;
				else if ((c & 0xF8) == 0xF0) len = 3;
				if (len > 0)
				{
					bool valid = true;
					for (int j = 1; j <= len && (i + j) < (int)dirtyString.size(); j++)
						if (((unsigned char)dirtyString[i + j] & 0xC0) != 0x80)
							{ valid = false; break; }
					if (valid)
					{
						i += len;
						break;
					}
				}
				dirtyString.erase(i, 1);
				i--;
				break;
			}
			if (numeric && (dirtyString[i] < '0' || dirtyString[i] > '9'))
			{
				dirtyString.erase(i, 1);
				i--;
			}
			// if less than ascii 20 or greater than ascii 126, delete
			else if (ascii && (dirtyString[i] < ' ' || dirtyString[i] > '~'))
			{
				dirtyString.erase(i, 1);
				i--;
			}
			break;
		}
	}
	return dirtyString;
}

std::string Format::CleanString(const char * dirtyData, bool ascii, bool color, bool newlines, bool numeric, bool icons)
{
	return Format::CleanString(std::string(dirtyData), ascii, color, newlines, numeric, icons);
}

char Format::ConvertFontIcon(const std::string &s, size_t pos)
{
	if (pos + 2 < s.length())
	{
		int b1 = (unsigned char)s[pos + 1];
		int b2 = (unsigned char)s[pos + 2];
		if ((b1 == 0x80 || b1 == 0x81) && b2 >= 0x80 && b2 < 0xC0)
		{
			if (b1 == 0x80)
			{
				// Delete icon is deliberately swapped in my mod
				if (b2 == 0x85)
					return (char)0x86;
				else if (b2 == 0x86)
					return (char)0x85;
				return (char)b2;
			}
			else
			{
				int c = b2 + 0x40;
				// Some of the later icons aren't ordered the same as vanilla does
				if (c >= 0xE2 && c < 0xE6)
					c++;
				else if (c == 0xE7)
					return (char)0xE2;
				else if (c == 0xEA)
					return (char)0xF0;
				else if (c >= 0xE6)
					return 0;
				return (char)c;
			}
		}
	}

	return 0;
}

std::vector<char> Format::VideoBufferToBMP(const gfx::VideoBuffer & vidBuf)
{
	const int width = vidBuf.GetWidth();
	const int height = vidBuf.GetHeight();
	pixel * vid = vidBuf.GetVid();

	std::vector<char> data;
	char buffer[54] = "BM";
	int padding = 3 - (width * 3 + 3) % 4;
	unsigned int fileSize = (width * 3 + padding) * height + 54;
	*(int *)(buffer + 2) = fileSize;
	*(short int *)(buffer + 6) = 0; // reserved;
	*(short int *)(buffer + 8) = 0; // reserved;
	*(int *)(buffer + 10) = 0x36; // 54 bytes from start to data
	*(int *)(buffer + 14) = 0x28; // 40 bytes in info header
	*(int *)(buffer + 18) = width;
	*(int *)(buffer + 22) = height;
	*(short int *)(buffer + 26) = 1; // 1 plane
	*(short int *)(buffer + 28) = 24; // 24 bits per pixel
	*(int *)(buffer + 30) = 0; // no compression
	*(int *)(buffer + 34) = fileSize - 54; // bitmap size
	*(int *)(buffer + 38) = 0xB13; // 72dpi
	*(int *)(buffer + 42) = 0xB13; // 72dpi
	*(int *)(buffer + 46) = 0; // all colors used
	*(int *)(buffer + 50) = 0; // all colors important

	data.insert(data.end(), buffer, buffer+54);

	char *currentRow = new char[(width * 3) + padding];
	for(int y = height - 1; y >= 0; y--)
	{
		int rowPos = 0;
		for(int x = 0; x < width; x++)
		{
			currentRow[rowPos++] = PIXB(vid[(y*width)+x]);
			currentRow[rowPos++] = PIXG(vid[(y*width)+x]);
			currentRow[rowPos++] = PIXR(vid[(y*width)+x]);
		}
		data.insert(data.end(), currentRow, currentRow+(width*3)+padding);
	}
	delete[] currentRow;

	return data;
}

std::vector<char> Format::VideoBufferToPPM(const gfx::VideoBuffer & vidBuf)
{
	int width = vidBuf.GetWidth();
	int height = vidBuf.GetHeight();
	pixel * vid = vidBuf.GetVid();

	std::vector<char> data;
	char buffer[256];
	sprintf(buffer, "P6\n%d %d\n255\n", width, height);
	data.insert(data.end(), buffer, buffer+strlen(buffer));

	unsigned char * currentRow = new unsigned char[width*3];
	for(int y = 0; y < height; y++)
	{
		int rowPos = 0;
		for(int x = 0; x < width; x++)
		{
			currentRow[rowPos++] = PIXR(vid[(y*width)+x]);
			currentRow[rowPos++] = PIXG(vid[(y*width)+x]);
			currentRow[rowPos++] = PIXB(vid[(y*width)+x]);
		}
		data.insert(data.end(), currentRow, currentRow+(width*3));
	}
	delete [] currentRow;

	return data;
}

struct PNGChunk
{
	int Length;
	char Name[4];
	char * Data;

	//char[4] CRC();

	PNGChunk(int length, std::string name)
	{
		if (name.length()!=4)
			throw std::runtime_error("Invalid chunk name");
		std::copy(name.begin(), name.begin()+4, Name);
		Length = length;
		if (length)
		{
			Data = new char[length];
			std::fill(Data, Data+length, 0);
		}
		else
		{
			Data = NULL;
		}
	}
	unsigned long CRC()
	{
		if (!Data)
		{
			return Format::CalculateCRC((unsigned char*)Name, 4);
		}
		else
		{
			unsigned char * temp = new unsigned char[4+Length];
			std::copy(Name, Name+4, temp);
			std::copy(Data, Data+Length, temp+4);
			unsigned long tempRet = Format::CalculateCRC(temp, 4+Length);
			delete[] temp;
			return tempRet;
		}
	}
	~PNGChunk()
	{
		delete[] Data;
	}
};

struct PngException: public std::exception
{
	std::string message;
public:
	PngException(std::string message): message(message)
	{
		std::cout << "Error creating png: " << message << std::endl;
	}

	const char * what() const throw() override
	{
		return message.c_str();
	}
	~PngException() throw() {}
};

std::vector<char> Format::VideoBufferToPNG(const gfx::VideoBuffer & vidBuf)
{
	int width = vidBuf.GetWidth();
	int height = vidBuf.GetHeight();
	pixel * vid = vidBuf.GetVid();

	std::vector<PNGChunk*> chunks;

	//Begin IHDR (Image header) chunk (Image size and depth)
	PNGChunk *IHDRChunk = new PNGChunk(13, "IHDR");

	//Image Width
	IHDRChunk->Data[0] = (width>>24)&0xFF;
	IHDRChunk->Data[1] = (width>>16)&0xFF;
	IHDRChunk->Data[2] = (width>>8)&0xFF;
	IHDRChunk->Data[3] = (width)&0xFF;

	//Image Height
	IHDRChunk->Data[4] = (height>>24)&0xFF;
	IHDRChunk->Data[5] = (height>>16)&0xFF;
	IHDRChunk->Data[6] = (height>>8)&0xFF;
	IHDRChunk->Data[7] = (height)&0xFF;

	//Bit depth
	IHDRChunk->Data[8] = 8; //8bits per channel or 24bpp

	//Colour type
	IHDRChunk->Data[9] = 2; //RGB triple

	//Everything else is default
	chunks.push_back(IHDRChunk);

	//Begin image data, format is 8bit RGB (24bit pixel)
	int dataPos = 0;
	unsigned char * uncompressedData = new unsigned char[(width*height*3)+height];

	//Byte ordering and filtering
	unsigned char * previousRow = new unsigned char[width*3];
	std::fill(previousRow, previousRow+(width*3), 0);
	unsigned char * currentRow = new unsigned char[width*3];
	for(int y = 0; y < height; y++)
	{
		int rowPos = 0;
		for(int x = 0; x < width; x++)
		{
			currentRow[rowPos++] = PIXR(vid[(y*width)+x]);
			currentRow[rowPos++] = PIXG(vid[(y*width)+x]);
			currentRow[rowPos++] = PIXB(vid[(y*width)+x]);
		}

		uncompressedData[dataPos++] = 2; //Up Sub(x) filter 
		for(int b = 0; b < rowPos; b++)
		{
			int filteredByte = (currentRow[b]-previousRow[b])&0xFF;
			uncompressedData[dataPos++] = filteredByte;
		}

		unsigned char * tempRow = previousRow;
		previousRow = currentRow;
		currentRow = tempRow;
	}
	delete[] currentRow;
	delete[] previousRow;

	//Compression
	int compressedBufferSize = (width*height*3)*2;
	unsigned char * compressedData = new unsigned char[compressedBufferSize];

	int result;
    z_stream zipStream; 
    zipStream.zalloc = Z_NULL;
    zipStream.zfree = Z_NULL;
    zipStream.opaque = Z_NULL;

    result = deflateInit2(&zipStream,
            9,              // level
            Z_DEFLATED,     // method
            10,             // windowBits
            1,              // memLevel
            Z_DEFAULT_STRATEGY // strategy
    );

    if (result != Z_OK)
		throw PngException("zlib deflate error (not Z_OK): " + NumberToString<int>(result));

    zipStream.next_in = uncompressedData;
    zipStream.avail_in = dataPos;

    zipStream.next_out = compressedData;
    zipStream.avail_out = compressedBufferSize;


    result = deflate(&zipStream, Z_FINISH);
    if (result != Z_STREAM_END)
		throw PngException("zlib deflate error (not Z_STREAM_END): " + NumberToString<int>(result));

    int compressedSize = compressedBufferSize-zipStream.avail_out;
	PNGChunk *IDATChunk = new PNGChunk(compressedSize, "IDAT");
	std::copy(compressedData, compressedData+compressedSize, IDATChunk->Data);
	chunks.push_back(IDATChunk);

	deflateEnd(&zipStream);

	delete[] compressedData;
	delete[] uncompressedData;

	PNGChunk *IENDChunk = new PNGChunk(0, "IEND");
	chunks.push_back(IENDChunk);

	//Write chunks to output buffer
	int finalDataSize = 8;
	for(std::vector<PNGChunk*>::iterator iter = chunks.begin(), end = chunks.end(); iter != end; ++iter)
	{
		PNGChunk * cChunk = *iter;
		finalDataSize += 4 + 4 + 4;
		finalDataSize += cChunk->Length;
	}
	unsigned char * finalData = new unsigned char[finalDataSize];
	int finalDataPos = 0;

	//PNG File header
	finalData[finalDataPos++] = 0x89;
	finalData[finalDataPos++] = 0x50;
	finalData[finalDataPos++] = 0x4E;
	finalData[finalDataPos++] = 0x47;
	finalData[finalDataPos++] = 0x0D;
	finalData[finalDataPos++] = 0x0A;
	finalData[finalDataPos++] = 0x1A;
	finalData[finalDataPos++] = 0x0A;

	for(std::vector<PNGChunk*>::iterator iter = chunks.begin(), end = chunks.end(); iter != end; ++iter)
	{
		PNGChunk * cChunk = *iter;

		//Chunk length
		finalData[finalDataPos++] = (cChunk->Length>>24)&0xFF;
		finalData[finalDataPos++] = (cChunk->Length>>16)&0xFF;
		finalData[finalDataPos++] = (cChunk->Length>>8)&0xFF;
		finalData[finalDataPos++] = (cChunk->Length)&0xFF;

		//Chunk name
		std::copy(cChunk->Name, cChunk->Name+4, finalData+finalDataPos);
		finalDataPos += 4;

		//Chunk data
		if(cChunk->Data)
		{
			std::copy(cChunk->Data, cChunk->Data+cChunk->Length, finalData+finalDataPos);
			finalDataPos += cChunk->Length;
		}

		//Chunk CRC
		unsigned long tempCRC = cChunk->CRC();
		finalData[finalDataPos++] = (tempCRC>>24)&0xFF;
		finalData[finalDataPos++] = (tempCRC>>16)&0xFF;
		finalData[finalDataPos++] = (tempCRC>>8)&0xFF;
		finalData[finalDataPos++] = (tempCRC)&0xFF;

		delete cChunk;
	}

	std::vector<char> outputData(finalData, finalData+finalDataPos);

	delete[] finalData;

	return outputData;
}

//CRC functions, copypasta from W3 PNG spec.

/* Table of CRCs of all 8-bit messages. */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* Make the table for a fast CRC. */
void make_crc_table(void)
{
 unsigned long c;
 int n, k;

 for (n = 0; n < 256; n++) {
   c = (unsigned long) n;
   for (k = 0; k < 8; k++) {
     if (c & 1)
       c = 0xedb88320L ^ (c >> 1);
     else
       c = c >> 1;
   }
   crc_table[n] = c;
 }
 crc_table_computed = 1;
}

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
  should be initialized to all 1's, and the transmitted value
  is the 1's complement of the final running CRC (see the
  crc() routine below)). */

unsigned long update_crc(unsigned long crc, unsigned char *buf, int len)
{
	unsigned long c = crc;
	int n;

	if (!crc_table_computed)
		make_crc_table();
	for (n = 0; n < len; n++)
	{
		c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
	}
	return c;
}

unsigned long Format::CalculateCRC(unsigned char * data, int len)
{
	return update_crc(0xffffffffL, data, len) ^ 0xffffffffL;
}

std::string Format::TemperatureToString(float temp, int scale, int precision)
{
	std::stringstream ret;
	ret << std::fixed;
	ret.precision(precision);
	switch (scale)
	{
	case 1:
		ret << (temp - 273.15f) << "C";
		break;
	case 2:
		ret << (temp - 273.15f) * 1.8f + 32.0f << "F";
		break;
	default:
		ret << temp << "K";
		break;
	}

	return ret.str();
}

float Format::StringToTemperature(const std::string &str, int defaultScale)
{
	int scale = defaultScale;
	bool hasScaleChar = false;
	if (str.size())
	{
		char last = str.at(str.size() - 1);
		if (last == 'K')
		{
			scale = 0;
			hasScaleChar = true;
		}
		else if (last == 'C')
		{
			scale = 1;
			hasScaleChar = true;
		}
		else if (last == 'F')
		{
			scale = 2;
			hasScaleChar = true;
		}
	}
	if (!str.size())
	{
		throw std::out_of_range("empty string");
	}
	float out = Format::StringToNumberThrowing<float>(hasScaleChar ? str.substr(0, str.size() - 1) : str);
	switch (scale)
	{
	case 1:
		out = out + 273.15;
		break;
	case 2:
		out = (out - 32.0f) / 1.8f + 273.15f;
		break;
	}

	return out;
}
