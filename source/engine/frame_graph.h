#pragma once
#include "systems.h"

namespace R3
{
	// Represents the entire frame as a graph of functions to be called
	// Async node -> runs first child on current thread, other children on jobs + waits for completion
	// Sequence node -> run all children sequentially on current thread
	// FixedUpdateSequenceNode -> runs all children sequentially one or more times for fixed update
	// Fn node -> calls out to systems/lambdas/whatever you want
	class FrameGraph
	{
	public:
		struct SequenceNode;
		struct AsyncNode;
		struct FnNode;
		struct FixedUpdateSequenceNode;
		class Node {
		public:
			Node() = default;
			Node(Node&&) = default;
			virtual bool Run() = 0;
			Node* FindFirst(std::string name);
			FixedUpdateSequenceNode& AddFixedUpdateSequence(std::string name);
			SequenceNode& AddSequence(std::string name);
			AsyncNode& AddAsync(std::string name);
			Node& AddFn(std::string name);
		protected:
			Node* FindInternal(Node* parent, std::string_view name);
			std::string m_displayName = "";
			std::vector<std::unique_ptr<Node>> m_children;
			FixedUpdateSequenceNode* AddFixedUpdateInternal(std::string name);
			SequenceNode* AddSequenceInternal(std::string name);
			AsyncNode* AddAsyncInternal(std::string name);
			FnNode* AddFnInternal(std::string name);
		};
		struct FixedUpdateSequenceNode : public Node {
			virtual bool Run();
		};
		struct SequenceNode : public Node {
			virtual bool Run();
		};
		struct AsyncNode : public Node {
			virtual bool Run();
		};
		struct FnNode : public Node {
			std::function<bool()> m_fn;
			virtual bool Run();
		};

		SequenceNode m_root;
	};
}