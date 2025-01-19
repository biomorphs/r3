#pragma once
#include <string>
#include <vector>
#include <optional>
#include "engine/serialiser.h"

namespace R3
{
	// Has a json header + a list of named binary blobs
	class AssetFile
	{
	public:
		struct Blob
		{
			std::string m_name;
			std::vector<uint8_t> m_data;
		};
		nlohmann::json m_header;
		std::vector<Blob> m_blobs;

		const Blob* GetBlob(std::string_view name);
	};

	std::optional<AssetFile> LoadAssetFile(std::string_view path);
	bool SaveAssetFile(AssetFile& asset, std::string_view path);
}