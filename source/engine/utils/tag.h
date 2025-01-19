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
		std::string GetString() const;
		uint16_t GetTag() const { return m_tag; }
		static std::vector<Tag> GetAllTags();
		bool operator==(const Tag& other) const {
			return m_tag == other.m_tag;
		}
	private:
		uint16_t m_tag = -1;	// string data stored in static singleton. 0 or -1 are unused
	};
}

// hashing so we can have maps of tags
template<>
struct std::hash<R3::Tag>
{
	size_t operator()(const R3::Tag& t) const
	{
		return std::hash<uint16_t>{}(t.GetTag());
	}
};