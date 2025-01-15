#include "asset_file.h"
#include "core/glm_headers.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace R3
{
    constexpr uint32_t c_currentVersion = 1;
    constexpr uint32_t c_minSupportedVersion = 1;
    constexpr uint64_t c_assetFileMarker = 0xA55A55B00BB00B13;

    struct AssetFileHeader
    {
        uint64_t m_marker = 0;  // should always match c_assetFileMarker
        uint32_t m_currentVersion = 0;
        uint32_t m_headerSize = 0;  // sizeof(AssetFileHeader)
        uint64_t m_timeWritten = 0; // time_t
        uint64_t m_jsonOffset = 0;
        uint64_t m_jsonSize = 0;
        uint64_t m_blobHeaderOffset = 0;
        uint64_t m_blobHeaderCount = 0;
    };

    struct AssetBlobHeader
    {
        char m_name[64];
        uint64_t m_startOffset = 0;
        uint64_t m_size = 0;
    };

    // pre-calculate any interesting offsets/sizes
    constexpr uint64_t c_headerSize = sizeof(AssetFileHeader);
    constexpr uint64_t c_blobHeadersOffset = AlignUpPow2(c_headerSize, 64ull);

    bool SaveAssetFile(AssetFile& asset, std::string_view path)
    {
        R3_PROF_EVENT();
        if (!std::filesystem::path(path).is_absolute())
        {
            LogError("Only absolute paths are accepted when writing files ('{}' is not valid)", path);
            return false;
        }
        
        const uint64_t blobHeadersEnd = c_blobHeadersOffset + asset.m_blobs.size() * sizeof(AssetBlobHeader);
        const uint64_t c_jsonOffset = AlignUpPow2(blobHeadersEnd, 64ull);

        // dump the json to text
        std::string jsonData = asset.m_header.dump();
        const uint64_t jsonEndOffset = c_jsonOffset + jsonData.size();

        // prepare blob headers now we know the size of the json
        std::vector<AssetBlobHeader> blobHeaders(asset.m_blobs.size());
        uint64_t blobOffset = AlignUpPow2(jsonEndOffset, 64ull);;
        for (int b = 0;b<asset.m_blobs.size();++b)
        {
            strcpy_s(blobHeaders[b].m_name, asset.m_blobs[b].m_name.c_str());
            blobHeaders[b].m_size = asset.m_blobs[b].m_data.size();
            blobHeaders[b].m_startOffset = blobOffset;
            blobOffset = AlignUpPow2(blobOffset + asset.m_blobs[b].m_data.size(), 64ull);
        }

        // prep the asset header
        AssetFileHeader newHeader;
        newHeader.m_marker = c_assetFileMarker;
        newHeader.m_currentVersion = c_currentVersion;
        newHeader.m_headerSize = sizeof(newHeader);
        newHeader.m_jsonOffset = c_jsonOffset;
        newHeader.m_jsonSize = jsonData.size();
        newHeader.m_blobHeaderOffset = c_blobHeadersOffset;
        newHeader.m_blobHeaderCount = blobHeaders.size();
        auto now = std::chrono::system_clock::now();
        newHeader.m_timeWritten = std::chrono::system_clock::to_time_t(now);

        // write to memory
        std::vector<uint8_t> dataOut(blobOffset);
        memcpy(dataOut.data(), &newHeader, sizeof(newHeader));
        memcpy(dataOut.data() + c_blobHeadersOffset, blobHeaders.data(), blobHeaders.size() * sizeof(AssetBlobHeader));
        memcpy(dataOut.data() + c_jsonOffset, jsonData.data(), jsonData.size());
        for (int b = 0; b < asset.m_blobs.size(); ++b)
        {
            memcpy(dataOut.data() + blobHeaders[b].m_startOffset, asset.m_blobs[b].m_data.data(), blobHeaders[b].m_size);
        }

        return FileIO::SaveBinaryFile(path, dataOut);
    }

    std::optional<AssetFile> LoadAssetFile(std::string_view path)
    {
        R3_PROF_EVENT();

        // should probably read in chunks!
        std::vector<uint8_t> rawBuffer;
        if (!FileIO::LoadBinaryFile(path, rawBuffer))
        {
            return {};
        }

        const auto fileHeader = reinterpret_cast<const AssetFileHeader*>(rawBuffer.data());
        if (fileHeader->m_marker != c_assetFileMarker)
        {
            LogError("{} is not an asset file", path);
            return {};
        }
        if (fileHeader->m_currentVersion < c_minSupportedVersion)
        {
            LogWarn("{} file version is too old (file = {}, current = {}", path, fileHeader->m_currentVersion, c_currentVersion);
            return {};
        }

        const AssetBlobHeader* blobHeaders = reinterpret_cast<const AssetBlobHeader*>(rawBuffer.data() + fileHeader->m_blobHeaderOffset);
        const auto blobCount = fileHeader->m_blobHeaderCount;
        
        // extract the json
        std::vector<uint8_t> jsonData;
        jsonData.resize(fileHeader->m_jsonSize);
        memcpy(jsonData.data(), rawBuffer.data() + fileHeader->m_jsonOffset, fileHeader->m_jsonSize);

        // extra the blobs
        AssetFile newAsset;
        newAsset.m_header = nlohmann::json::parse(jsonData);
        newAsset.m_blobs.resize(blobCount);
        for (int i = 0; i < blobCount; ++i)
        {
            newAsset.m_blobs[i].m_name = blobHeaders[i].m_name;
            newAsset.m_blobs[i].m_data.resize(blobHeaders[i].m_size);
            memcpy(newAsset.m_blobs[i].m_data.data(), rawBuffer.data() + blobHeaders[i].m_startOffset, blobHeaders[i].m_size);
        }

        return newAsset;
    }

    const AssetFile::Blob* AssetFile::GetBlob(std::string_view name)
    {
        auto found = std::find_if(m_blobs.begin(), m_blobs.end(), [name](const AssetFile::Blob& b) {
            return b.m_name == name;
        });
        return found == m_blobs.end() ? nullptr : &(*found);
    }
}