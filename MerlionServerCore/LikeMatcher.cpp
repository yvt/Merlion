/**
 * Copyright (C) 2014 yvt <i@yvt.jp>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Prefix.pch"
#include "LikeMatcher.hpp"
#include "Exceptions.hpp"
#include <cctype>

namespace mcore
{
	namespace
	{
		bool isValidLiteralChar(char c)
		{
			if(std::isalnum(c) || c == '_' || c == '.' || c == ' ' || c == '-') {
				return true;
			}
			return false;
		}
	}
	
	LikeMatcher::LikeMatcher(const std::string &pattern)
	{
		std::vector<char> str;
		
		std::size_t i = 0;
		int numUniversals = 0;
		while (i < pattern.size()) {
			char c = pattern[i];
			if (c == '[') {
				str.push_back('['); ++i;
				std::size_t start = i;
				while (true) {
					if (i >= pattern.size()) {
						MSCThrow(InvalidFormatException());
					}
					c = pattern[i];
					if (c == ']' && i > start) {
						str.push_back('['); ++i;
						break;
					} else if (c == '-' && i > start) {
						str.push_back('-'); ++i;
						if (i >= pattern.size() || !isValidLiteralChar(pattern[i])) {
							MSCThrow(InvalidFormatException());
						}
						str.push_back(pattern[i]); ++i;
					} else if (c == '^' && i == start) {
						str.push_back('^'); ++i;
					} else {
						if (isValidLiteralChar(c)) {
							str.push_back(c); ++i;
						} else {
							MSCThrow(InvalidFormatException());
						}
					}
				}
			} else if (c == '_') {
				str.push_back('.'); ++i;
			} else if (c == '%') {
				str.push_back('.'); str.push_back('*'); ++i;
				++numUniversals;
				if (numUniversals > 8)
					MSCThrow(InvalidFormatException());
			} else if (c == '\\') {
				++i;
				if (i >= pattern.size()) {
					MSCThrow(InvalidFormatException());
				}
				c = pattern[i];
				switch (c) {
					case 'n': c = '\n'; break;
					case 'r': c = '\r'; break;
					case 't': c = '\t'; break;
				}
				if (isValidLiteralChar(c)) {
					str.push_back(c); ++i;
				} else {
					MSCThrow(InvalidFormatException());
				}
			} else {
				if (isValidLiteralChar(c)) {
					str.push_back(c); ++i;
				} else {
					MSCThrow(InvalidFormatException());
				}
			}
		}
		
		regex.reset(new std::regex(str.data(), str.size(),
								   std::regex_constants::ECMAScript));
		
	}
	
	bool LikeMatcher::match(const std::string &subject)
	{
		return std::regex_match(subject, *regex);
	}
}
