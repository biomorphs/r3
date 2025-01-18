#include "timestamp_queries_handler.h"
#include "vulkan_helpers.h"
#include "device.h"
#include "core/profiler.h"
#include "core/log.h"
#include <algorithm>

namespace R3
{
	bool TimestampQueriesHandler::Initialise(Device& d, uint32_t maxQueries)
	{
		R3_PROF_EVENT();

		VkQueryPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		poolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		poolCreateInfo.queryCount = maxQueries;
		if (!VulkanHelpers::CheckResult(vkCreateQueryPool(d.GetVkDevice(), &poolCreateInfo, nullptr, &m_queryPool)))
		{
			LogError("Failed to create query pool");
			return false;
		}
		m_maxQueries = maxQueries;

		return true;
	}

	void TimestampQueriesHandler::CollectResults(Device& d)
	{
		R3_PROF_EVENT();

		m_previousScopedResults.clear();
		m_previousResults.resize(m_queries.size());

		if (m_queries.size() == 0)
		{
			return;
		}

		if (!VulkanHelpers::CheckResult(vkGetQueryPoolResults(d.GetVkDevice(), m_queryPool, 0, (uint32_t)m_queries.size(),
			m_previousResults.size() * sizeof(TimestampQueryResult), m_previousResults.data(), sizeof(TimestampQueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT)))
		{
			LogError("Failed to get query pool results");
			m_previousResults.clear();
		}

		// we now have the raw query results, convert them to usable values + fill in the rest of the result
		const double timePeriodNanoseconds = d.GetPhysicalDevice().m_properties.limits.timestampPeriod;
		for (int i=0;i<m_queries.size();++i)
		{
			m_previousResults[i].m_valueInMilliseconds = (m_previousResults[i].m_rawValue * timePeriodNanoseconds) / 1000000.0;
			m_previousResults[i].m_queryID = m_queries[i].m_queryID;
		}
		std::sort(m_previousResults.begin(), m_previousResults.end(), [](const TimestampQueryResult& t0, const TimestampQueryResult& t1) {
			return t0.m_valueInMilliseconds < t1.m_valueInMilliseconds;
		});

		// copy matching result values to existing scoped queries
		m_previousScopedResults.clear();
		for (int i = 0; i < m_scopedQueries.size(); ++i)
		{
			uint32_t startID = m_scopedQueries[i].m_startQueryID;
			uint32_t endID = m_scopedQueries[i].m_endQueryID;
			auto startResult = std::find_if(m_previousResults.begin(), m_previousResults.end(), [startID](const TimestampQueryResult & r) {
				return r.m_queryID == startID;
			});
			auto endResult = std::find_if(m_previousResults.begin(), m_previousResults.end(), [endID](const TimestampQueryResult& r) {
				return r.m_queryID == endID;
			});
			if (startResult != m_previousResults.end() && endResult != m_previousResults.end())
			{
				ScopedTimestampResult result;
				result.m_startTime = startResult->m_valueInMilliseconds;
				result.m_endTime = endResult->m_valueInMilliseconds;
				result.m_name = m_scopedQueries[i].m_name;
				m_previousScopedResults.push_back(result);
			}
		}
		std::sort(m_previousScopedResults.begin(), m_previousScopedResults.end(), [](const ScopedTimestampResult& t0, const ScopedTimestampResult& t1) {
			return t0.m_startTime < t1.m_startTime;
		});
	}

	void TimestampQueriesHandler::Reset(VkCommandBuffer cmds)
	{
		R3_PROF_EVENT();

		vkCmdResetQueryPool(cmds, m_queryPool, 0, m_maxQueries);
		m_nextQueryID = 0;
		m_currentCmds = cmds;
		m_queries.clear();
		m_scopedQueries.clear();
	}

	void TimestampQueriesHandler::Cleanup(Device& d)
	{
		R3_PROF_EVENT();

		vkDestroyQueryPool(d.GetVkDevice(), m_queryPool, nullptr);
		m_queryPool = VK_NULL_HANDLE;
	}

	uint32_t TimestampQueriesHandler::WriteTimestamp(VkPipelineStageFlagBits pipelineStage)
	{
		R3_PROF_EVENT();

		if (m_currentCmds == VK_NULL_HANDLE)
		{
			LogError("Reset() was not called with a valid command buffer");
			return -1;
		}

		uint32_t thisQueryID = m_nextQueryID++;
		TimestampQuery newQuery;
		newQuery.m_queryID = thisQueryID;
		m_queries.push_back(newQuery);

		vkCmdWriteTimestamp(m_currentCmds, pipelineStage, m_queryPool, thisQueryID);

		return thisQueryID;
	}

	TimestampQueriesHandler::ScopedQuery TimestampQueriesHandler::MakeScopedQuery(std::string_view name)
	{
		return TimestampQueriesHandler::ScopedQuery(name, this);
	}

	TimestampQueriesHandler::ScopedQuery::ScopedQuery(std::string_view name, TimestampQueriesHandler* handler)
		: m_handler(handler)
		, m_name(name)
	{
		m_startQuery = handler->WriteTimestamp(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	}

	TimestampQueriesHandler::ScopedQuery::~ScopedQuery()
	{
		TimestampQueriesHandler::ScopedTimestampQuery newQuery;
		newQuery.m_startQueryID = m_startQuery;
		newQuery.m_endQueryID = m_handler->WriteTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
		newQuery.m_name = m_name;
		m_handler->m_scopedQueries.push_back(newQuery);
	}
}