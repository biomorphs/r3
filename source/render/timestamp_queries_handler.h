#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <string_view>
#include <vector>

namespace R3
{
	class Device;

	// an object that owns a vulkan timestamp query pool + manages timestamps submitted to it
	class TimestampQueriesHandler
	{
	public:
		class ScopedQuery	// records start + end time of render commands in scope of this object
		{
		public:
			ScopedQuery(std::string_view name, TimestampQueriesHandler* handler);
			~ScopedQuery();
		private:
			TimestampQueriesHandler* m_handler = nullptr;
			std::string m_name;
			uint32_t m_startQuery = -1;
		};
		ScopedQuery MakeScopedQuery(std::string_view name);

		bool Initialise(Device&, uint32_t maxQueries);
		void Reset(VkCommandBuffer cmds);	// call this after vkBeginCommandBuffer and before adding any new queries for a command buffer. clears m_queries + m_nextQueryID
		void CollectResults(Device&);	// call this before calling Reset(), blocks until results ready (be careful!)
		void Cleanup(Device&);

		// use VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT to record before any cmds ran
		// use VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT to record after any cmds ran
		// returns the query ID
		uint32_t WriteTimestamp(VkPipelineStageFlagBits pipelineStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

		// raw results API
		struct TimestampQueryResult
		{
			uint64_t m_rawValue = 0;
			double m_valueInMilliseconds = 0.0;
			uint32_t m_queryID = -1;
		};
		std::vector<TimestampQueryResult> GetResults() { return m_previousResults; }

		// scoped results API
		struct ScopedTimestampResult
		{
			double m_startTime = 0.0;
			double m_endTime = 0.0;
			std::string m_name;
		};
		std::vector<ScopedTimestampResult> GetScopedResults() { return m_previousScopedResults; }

	private:
		struct TimestampQuery
		{
			uint32_t m_queryID;	// passed to vkCmdWriteTimestamp
		};
		struct ScopedTimestampQuery
		{
			uint32_t m_startQueryID = -1;
			uint32_t m_endQueryID = -1;
			std::string m_name;
		};
		std::vector<TimestampQuery> m_queries;	// queries submitted this frame
		std::vector<ScopedTimestampQuery> m_scopedQueries;	// scoped queries for this frame
		std::vector<TimestampQueryResult> m_previousResults;	// previous frame results
		std::vector<ScopedTimestampResult> m_previousScopedResults;	// previous frame scoped results
		uint32_t m_maxQueries = 0;
		uint32_t m_nextQueryID = 0;
		VkQueryPool m_queryPool = VK_NULL_HANDLE;
		VkCommandBuffer m_currentCmds = VK_NULL_HANDLE;	// set every time Reset is called
	};
}