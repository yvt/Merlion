#pragma once

namespace mcore
{
	class LikeMatcher
	{
		boost::xpressive::sregex regex;
	public:
		LikeMatcher(const std::string &pattern);
		
		bool match(const std::string &subject);
	};
}

