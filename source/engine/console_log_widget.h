#pragma once
#include "core/mutex.h"
#include <vector>
#include <string>

namespace R3
{
	enum class LogType;
	// A log (and eventually console) widget
	class ConsoleLogWidget
	{
	public:
		ConsoleLogWidget();
		~ConsoleLogWidget();
		ConsoleLogWidget(const ConsoleLogWidget& other) = delete;
		ConsoleLogWidget(ConsoleLogWidget&&) = default;

		void Update(bool embedAsChild = false);

		// options for different behaviours
		bool m_isDisplayed = false;
		bool m_displayOnError = true;
		bool m_displayOnWarning = true;
		bool m_displayOnInfo = false;
	private:
		uint64_t m_logCallbackToken = -1;
		struct LogHistoryEntry {
			LogType m_type;	// error/warn/info
			std::string m_txt;
		};
		Mutex m_historyMutex;
		std::vector<LogHistoryEntry> m_logHistory;
	};
}