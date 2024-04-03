#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#ifdef __cplusplus

#include <string>
#include <algorithm>
#include <cctype>
#include <locale>

namespace {

// Trim from start (left)
static inline std::string &ltrim(std::string &s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
					[](unsigned char ch) { return !std::isspace(ch); }));
	return s;
}

// Trim from end (right)
static inline std::string &rtrim(std::string &s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(),
			     [](unsigned char ch) { return !std::isspace(ch); })
			.base(),
		s.end());
	return s;
}

// Trim from both ends
static inline std::string &trim(std::string &s)
{
	return ltrim(rtrim(s));
}

} // namespace

#endif

#endif
