#include "tag.h"
#include "serialiser.h"
#include "core/mutex.h"

namespace R3
{
	namespace TagInternals {
		struct TagStrings {
			Mutex m_mutex;
			uint16_t m_nextTag = 1;		// tag 0 or -1 are invalid
			std::unordered_map<std::string, uint16_t> m_stringToTag;
			std::unordered_map<uint16_t, std::string> m_tagToString;
		};
		static TagStrings& GetTagStrings() {
			static TagStrings s_tagStrings;
			return s_tagStrings;
		}
	}

	Tag::Tag(std::string_view v)
		: m_tag(-1)
	{
		if (!v.empty())
		{
			auto& ts = TagInternals::GetTagStrings();
			ScopedLock lock(ts.m_mutex);
			std::string vKey(v);
			auto found = ts.m_stringToTag.find(vKey);
			if (found == ts.m_stringToTag.end())
			{
				assert(ts.m_nextTag < (UINT16_MAX - 1));
				auto newIndex = ts.m_nextTag++;
				ts.m_stringToTag[vKey] = newIndex;
				ts.m_tagToString[newIndex] = std::string(v);
				m_tag = newIndex;
			}
			else
			{
				m_tag = found->second;
			}
		}
	}

	std::string Tag::GetString()
	{
		std::string result;
		if (m_tag != 0 && m_tag != -1)
		{
			auto& ts = TagInternals::GetTagStrings();
			ScopedLock lock(ts.m_mutex);
			return ts.m_tagToString[m_tag];
		}
		return result;
	}

	std::vector<Tag> Tag::GetAllTags()
	{
		std::vector<Tag> results;
		auto& ts = TagInternals::GetTagStrings();
		ScopedLock lock(ts.m_mutex);
		for (const auto& it : ts.m_tagToString)
		{
			results.push_back({it.first});
		}
		return results;
	}

	void Tag::SerialiseJson(JsonSerialiser& s)
	{
		std::string str;
		if (s.GetMode() == JsonSerialiser::Mode::Write)
		{
			str = GetString();
			s("Str", str);
		}
		else
		{
			s("Str", str);
			*this = Tag(str);	// lazy but effective
		}
	}
}