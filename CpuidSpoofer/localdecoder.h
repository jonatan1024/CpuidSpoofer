#pragma once
#include "plugin.h"
#include <queue>

class LocalDecoder {
	enum class PositionType {
		DESIRED,
		OTHER,

		MAX_TYPES
	};

	struct QPosition {
		duint addr;
		PositionType type;

		QPosition(duint addr, PositionType type) {
			this->addr = addr;
			this->type = type;
		}
		constexpr bool operator<(const QPosition& that) const {
			return this->addr < that.addr;
		}
	};

	const int maxInstructionLength = 16;
	const int maxSteps = 256;

	duint addr;
	int instructionLength;
	std::priority_queue<QPosition> queue;
	int count[(int)PositionType::MAX_TYPES] = {};

	void QAdd(duint addr, PositionType type) {
		queue.emplace(addr, type);
		count[(int)type]++;
	}

	QPosition QGet() {
		QPosition position = queue.top();
		queue.pop();
		count[(int)position.type]--;
		return position;
	}

public:
	LocalDecoder(duint addr, int instructionLength) : addr(addr), instructionLength(instructionLength) {}

	bool IsDesired() {
		//check possible disassembly after
		BASIC_INSTRUCTION_INFO info;
		DbgDisasmFastAt(addr + instructionLength, &info);
		if(info.instruction[0] == '?')
			return false;

		//check possible disassemblies before
		QAdd(addr, PositionType::DESIRED);
		for(int offset = 1; offset <= maxInstructionLength; offset++) {
			DbgDisasmFastAt(addr - offset, &info);
			if(info.size > offset && info.instruction[0] != '?')
				QAdd(addr - offset, PositionType::OTHER);
		}
		int steps = 0;
		while(count[(int)PositionType::DESIRED] > 0 && count[(int)PositionType::OTHER] > 0 && steps < maxSteps) {
			QPosition pos = QGet();
			for(int offset = 1; offset <= maxInstructionLength; offset++) {
				memset(&info, 0, sizeof(info));
				DbgDisasmFastAt(pos.addr - offset, &info);
				if(info.size == offset && info.instruction[0] != '?')
					QAdd(pos.addr - offset, pos.type);
			}
			steps++;
		}
		if(steps >= maxSteps) {
			dprintf("Couldn't decide the decoding of an instuction at " DUINT_FMT " after %d steps! Please inspect the breakpoint manually.\n", addr, steps);
			return true;
		}
		return count[(int)PositionType::DESIRED] > 0;
	}
};