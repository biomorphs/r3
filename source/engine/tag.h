#pragma once
#include <string>
#include <vector>

namespace R3
{
	class Tag
	{
	public:
		Tag() {}
		Tag(std::string_view v);
		Tag(uint16_t v) : m_tag(v) {}
		Tag(const Tag& other) : m_tag(other.m_tag) {}
		void SerialiseJson(class JsonSerialiser& s);
		std::string GetString();
		uint16_t GetTag() { return m_tag; }
		static std::vector<Tag> GetAllTags();
		bool operator==(const Tag& other) const {
			return m_tag == other.m_tag;
		}
	private:
		uint16_t m_tag = -1;	// string data stored in static singleton. 0 or -1 are unused
	};
}