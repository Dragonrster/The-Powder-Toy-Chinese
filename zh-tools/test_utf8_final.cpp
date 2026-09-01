#include "common/String.h"

#include <iostream>

int main()
{
	int failures = 0;
	auto check = [&](bool condition, char const *message) {
		if (!condition)
		{
			std::cerr << "FAIL: " << message << '\n';
			++failures;
		}
	};

	String ascii = "Powder Toy";
	check(ascii.size() == 10, "ASCII literal length changed");
	check(ascii.ToUtf8() == "Powder Toy", "ASCII round trip failed");

	String chinese = "中文显示";
	check(chinese.size() == 4, "Chinese UTF-8 literal was not decoded to four codepoints");
	check(chinese[0] == U'中' && chinese[3] == U'示', "Chinese codepoints are incorrect");
	check(chinese.ToUtf8() == ByteString("中文显示"), "Chinese round trip failed");

	String supplementary = ByteString("\xF0\x9F\x98\x80").FromUtf8(false);
	check(supplementary.size() == 1, "four-byte UTF-8 produced the wrong length");
	check(supplementary.size() && supplementary[0] == 0x1F600, "four-byte UTF-8 decoded to the wrong codepoint");
	check(supplementary.ToUtf8() == ByteString("\xF0\x9F\x98\x80"), "four-byte UTF-8 round trip failed");

	String rgbControl = ByteString("\x0F\xC2\xA0\x41").FromUtf8(false);
	check(rgbControl.size() == 4, "RGB control bytes were collapsed as UTF-8");
	check(rgbControl.size() == 4 && rgbControl[0] == 0x0F && rgbControl[1] == 0xC2 &&
			rgbControl[2] == 0xA0 && rgbControl[3] == 0x41,
		"RGB control bytes were not preserved verbatim");

	std::cout
	std::cout << "UTF-8 tests: " << failures << " failure(s)\n";
	return failures ? 1 : 0;
}
