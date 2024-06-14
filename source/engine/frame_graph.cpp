#include "frame_graph.h"
#include "systems.h"
#include "systems/time_system.h"
#include "systems/job_system.h"
#include "core/profiler.h"

namespace R3
{
	bool FrameGraph::FixedUpdateSequenceNode::Run() {
		auto timeSys = Systems::GetSystem<TimeSystem>();
		double startTime = timeSys->GetElapsedTimeReal();
		const double c_maxUpdateTime = timeSys->GetFixedUpdateDelta();
		while (timeSys->GetFixedUpdateCatchupTime() >= timeSys->GetFixedUpdateDelta() &&
			(timeSys->GetElapsedTimeReal() - startTime) < c_maxUpdateTime)
		{
			char debugName[1024] = { '\0' };
			sprintf_s(debugName, "%s", m_displayName.c_str());
			R3_PROF_EVENT_DYN(debugName);
			bool result = true;
			for (auto& it : m_children)
			{
				if (!it->Run())
				{
					return false;
				}
			}
			timeSys->OnFixedUpdateEnd();
		}
		return true;
	}

	bool FrameGraph::SequenceNode::Run() {
		char debugName[1024] = { '\0' };
		sprintf_s(debugName, "%s", m_displayName.c_str());
		R3_PROF_EVENT_DYN(debugName);
		bool result = true;
		for (auto& it : m_children)
		{
			result &= it->Run();
			if (!result)
			{
				break;
			}
		}
		return result;
	}

	bool FrameGraph::AsyncNode::Run() {
		bool result = true;
		char debugName[1024] = { '\0' };
		sprintf_s(debugName, "%s", m_displayName.c_str());
		R3_PROF_EVENT_DYN(debugName);
		if (m_children.size() == 0)
		{
			return true;
		}
		auto jobs = Systems::GetSystem<JobSystem>();
		// kick off jobs asap
		struct JobDesc {
			bool m_ran = false;
			bool m_result = false;
		};
		std::vector<JobDesc> jobDescs;
		std::atomic<int> jobRunCount = std::max((int)m_children.size() - 1, 0);
		if (m_children.size() > 1)
		{
			jobDescs.resize(m_children.size() - 1);
			for (int j = 1; j < m_children.size(); ++j)
			{
				jobs->PushJob(JobSystem::ThreadPool::FastJobs, [&jobDescs, j, &jobRunCount, this]() {
					jobDescs[j - 1].m_result = m_children[j]->Run();
					jobDescs[j - 1].m_ran = true;
					jobRunCount--;
				});
			}
		}

		// run first job on current thread but after jobs were submitted
		if (!m_children[0]->Run())
		{
			result = false;
		}
		
		// wait for everything to finish
		{
			R3_PROF_STALL("Wait for completion");
			while (jobRunCount > 0)
			{
				jobs->ProcessJobImmediate(JobSystem::ThreadPool::FastJobs);
			}
		}
		
		// collect results
		for (int i = 0; i < jobDescs.size() && result == true; ++i)
		{
			result &= (jobDescs[i].m_ran == true && jobDescs[i].m_result == true);
		}
		return result;
	}

	bool FrameGraph::FnNode::Run() 
	{
		return m_fn();
	}

	FrameGraph::FixedUpdateSequenceNode& FrameGraph::Node::AddFixedUpdateSequence(std::string name)
	{
		return *AddFixedUpdateInternal(name);
	}
	
	FrameGraph::SequenceNode& FrameGraph::Node::AddSequence(std::string name)
	{
		return *AddSequenceInternal(name);
	}
	
	FrameGraph::AsyncNode& FrameGraph::Node::AddAsync(std::string name)
	{
		return *AddAsyncInternal(name);
	}
	
	FrameGraph::Node& FrameGraph::Node::AddFn(std::string name, bool addToFront)
	{
		AddFnInternal(name, addToFront);
		return *this;
	}

	FrameGraph::Node* FrameGraph::Node::FindInternal(Node* parent, std::string_view name)
	{
		if (parent->m_displayName == name)
		{
			return parent;
		}
		for (auto& it : parent->m_children)
		{
			Node* foundInChild = FindInternal(it.get(), name);
			if (foundInChild)
			{
				return foundInChild;
			}
		}
		return nullptr;
	}

	FrameGraph::Node* FrameGraph::Node::FindFirst(std::string name)
	{
		return FindInternal(this, name);
	}

	FrameGraph::FixedUpdateSequenceNode* FrameGraph::Node::AddFixedUpdateInternal(std::string name)
	{
		R3_PROF_EVENT();
		auto newSeq = std::make_unique<FixedUpdateSequenceNode>();
		auto returnPtr = newSeq.get();
		newSeq->m_displayName = "FixedUpdateSequence - " + name;
		m_children.push_back(std::move(newSeq));
		return returnPtr;
	}

	FrameGraph::SequenceNode* FrameGraph::Node::AddSequenceInternal(std::string name)
	{
		R3_PROF_EVENT();
		auto newSeq = std::make_unique<SequenceNode>();
		auto returnPtr = newSeq.get();
		newSeq->m_displayName = "Sequence - " + name;
		m_children.push_back(std::move(newSeq));
		return returnPtr;
	}

	FrameGraph::AsyncNode* FrameGraph::Node::AddAsyncInternal(std::string name)
	{
		R3_PROF_EVENT();
		auto newAsync = std::make_unique<AsyncNode>();
		auto returnPtr = newAsync.get();
		newAsync->m_displayName = "Async - " + name;
		m_children.push_back(std::move(newAsync));
		return returnPtr;
	}

	FrameGraph::FnNode* FrameGraph::Node::AddFnInternal(std::string name, bool addToFront)
	{
		R3_PROF_EVENT();
		auto newFn = std::make_unique<FnNode>();
		auto returnPtr = newFn.get();
		newFn->m_displayName = "Fn - " + name;
		newFn->m_fn = Systems::GetInstance().GetTick(name);
		if (addToFront)
		{
			m_children.insert(m_children.begin(), std::move(newFn));
		}
		else
		{
			m_children.push_back(std::move(newFn));
		}
		return returnPtr;
	}
}