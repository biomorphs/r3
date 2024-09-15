#pragma once
#include "tag.h"
#include <array>
#include <cassert>
#include <string>

namespace R3
{
	// A static sized container of unique tags
	template<int MaxSize>
	class TagSet
	{
	public:
		TagSet() {}
		TagSet(std::string_view str);	// comma-separated list of tags e.g. "tag 0, tag 1, tag3"
		void Add(Tag t);	// use this instead of touching members directly
		bool Contains(Tag t);
		template<int OtherSize>
		bool ContainsAll(const TagSet<OtherSize>& ts);
		template<int OtherSize>
		bool ContainsAny(const TagSet<OtherSize>& ts);
		std::array<Tag, MaxSize> m_tags;
		uint8_t m_count = 0;
	};

	template<int MaxSize>
	TagSet<MaxSize>::TagSet(std::string_view str)
	{
		std::string tagStrCopy(str);		// strip whitespace
		tagStrCopy.erase(std::remove_if(tagStrCopy.begin(), tagStrCopy.end(), isspace), tagStrCopy.end());
		size_t firstChar = 0;
		while (firstChar < tagStrCopy.length())
		{
			size_t nextSeparator = std::string_view(tagStrCopy.data() + firstChar).find_first_of(',');
			if (nextSeparator == std::string_view::npos)
			{
				nextSeparator = tagStrCopy.length() - firstChar;
			}
			auto tagStr = tagStrCopy.substr(firstChar, nextSeparator);
			Add(Tag(tagStr));
			firstChar += nextSeparator + 1;
		}
	}

	template<int MaxSize>
	inline void TagSet<MaxSize>::Add(Tag t)
	{
		if (m_count < MaxSize && m_count < 254)
		{
			auto found = std::find(m_tags.begin(), m_tags.begin() + m_count, t);
			if (found == (m_tags.begin() + m_count))
			{
				m_tags[m_count++] = t;
			}
		}
		else
		{
			assert(!"Out of space for tags");
		}
	}

	template<int MaxSize>
	inline bool TagSet<MaxSize>::Contains(Tag t)
	{
		// simd is easy here if needed
		for (auto i = 0; i < m_count; ++i)
		{
			if (m_tags[i] == t)
			{
				return true;
			}
		}
		return false;
	}
	
	template<int MaxSize>
	template<int OtherSize>
	bool TagSet<MaxSize>::ContainsAll(const TagSet<OtherSize>& ts)
	{
		assert(OtherSize <= MaxSize);
		for (int io = 0; io < ts.m_count; ++io)
		{
			if (!Contains(ts.m_tags[io]))
			{
				return false;
			}
		}
		return true;
	}

	template<int MaxSize>
	template<int OtherSize>
	bool TagSet<MaxSize>::ContainsAny(const TagSet<OtherSize>& ts)
	{
		assert(OtherSize <= MaxSize);
		for (int io = 0; io < ts.m_count; ++io)
		{
			if (Contains(ts.m_tags[io]))
			{
				return true;
			}
		}
		return false;
	}
}