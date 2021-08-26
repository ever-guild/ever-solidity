/*
 * Copyright 2018-2019 TON DEV SOLUTIONS LTD.
 *
 * Licensed under the  terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 *
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the  GNU General Public License for more details at: https://www.gnu.org/licenses/gpl-3.0.html
 */
/**
 * @author TON Labs <connect@tonlabs.io>
 * @date 2021
 * Simulator of TVM execution
 */

#include "TVMSimulator.hpp"
#include "TVMCommons.hpp"

using namespace solidity::frontend;

void Simulator::run(const std::vector<Pointer<TvmAstNode>>::const_iterator _beg,
					const std::vector<Pointer<TvmAstNode>>::const_iterator _end) {
	auto iter = _beg;
	for (; iter != _end; ++iter) {
		auto nextIter = iter + 1;
		if (nextIter != _end && isPopAndDrop(*iter, *nextIter)) {
			solAssert(m_isDropped, "");
			solAssert(!m_unableToConvertOpcode, "");
			iter += 2;
			break;
		}

		auto node = iter->get();
		node->accept(*this);
		if (m_unableToConvertOpcode || m_isDropped) {
			++iter;
			break;
		}
		solAssert(m_stackSize >= 1, "");
	}
	if (success()) {
		for (; iter != _end; ++iter) {
			m_commands.emplace_back(*iter);
		}
	}
}

bool Simulator::visit(AsymGen &/*_node*/) {
	solUnimplemented("");
	return false;
}

bool Simulator::visit(DeclRetFlag &/*_node*/) {
	++m_stackSize;
	m_commands.emplace_back(createNode<DeclRetFlag>());
	return false;
}

bool Simulator::visit(Opaque &_node) {
	if (m_stackSize <= _node.take()) {
		m_unableToConvertOpcode = true;
	} else {
		m_stackSize += _node.ret() - _node.take();
		m_commands.emplace_back(createNode<Opaque>(_node.block(), _node.take(), _node.ret(), _node.isPure()));
	}
	return false;
}

bool Simulator::visit(HardCode &_node) {
	if (m_stackSize <= _node.take()) {
		m_unableToConvertOpcode = true;
	} else {
		m_stackSize += _node.ret() - _node.take();
		m_commands.emplace_back(createNode<HardCode>(_node.code(), _node.take(), _node.ret()));
	}
	return false;
}

bool Simulator::visit(Loc &_node) {
	m_commands.emplace_back(createNode<Loc>(_node.file(), _node.line()));
	return false;
}

bool Simulator::visit(TvmReturn &_node) {
	switch (_node.type()) {
		case TvmReturn::Type::RET:
			break;
		case TvmReturn::Type::IFRET:
		case TvmReturn::Type::IFNOTRET:
			solUnimplemented("Only in ReturnChecker");
			//if (m_stackSize == 1) {
			//m_unableToConvertOpcode = true;
			//--m_stackSize;
			break;
	}
	m_commands.emplace_back(createNode<TvmReturn>(_node.type()));
	return false;
}

bool Simulator::visit(ReturnOrBreakOrCont &_node) {
	m_wasReturnBreakContinue = true;
	Simulator sim{_node.body()->instructions().begin(), _node.body()->instructions().end(), m_stackSize};
	if (sim.m_unableToConvertOpcode) {
		m_unableToConvertOpcode = true;
	} else {
		if (sim.m_isDropped) {
			m_isDropped = true;
		}
		auto body = createNode<CodeBlock>(_node.body()->type(), sim.commands());
		m_commands.emplace_back(createNode<ReturnOrBreakOrCont>(body));
	}
	return false;
}

bool Simulator::visit(TvmException &_node) {
	if (_node.take() < m_stackSize) {
		m_stackSize -= _node.take();
		m_commands.emplace_back(createNode<TvmException>(_node.fullOpcode(), _node.take(), _node.ret()));
	} else {
		m_unableToConvertOpcode = true;
	}
	return false;
}

bool Simulator::visit(GenOpcode &_node) {
	if (m_stackSize <= _node.take()) {
		m_unableToConvertOpcode = true;
	} else {
		m_stackSize += _node.ret() - _node.take();
		m_commands.emplace_back(createNode<GenOpcode>(_node.fullOpcode(), _node.take(), _node.ret(), _node.isPure()));
	}
	return false;
}

bool Simulator::visit(PushCellOrSlice &_node) {
	++m_stackSize;
	m_commands.emplace_back(createNode<PushCellOrSlice>(_node.type(), _node.blob(), _node.child()));
	return false;
}

bool Simulator::visit(Glob &_node) {
	switch (_node.opcode()) {
		case Glob::Opcode::GetOrGetVar:
		case Glob::Opcode::PUSHROOT:
		case Glob::Opcode::PUSH_C3:
			++m_stackSize;
			break;

		case Glob::Opcode::SetOrSetVar:
		case Glob::Opcode::POPROOT:
		case Glob::Opcode::POP_C3:
			if (m_stackSize >= 2) {
				--m_stackSize;
			} else {
				m_unableToConvertOpcode = true;
			}
			break;
	}
	m_commands.emplace_back(createNode<Glob>(_node.opcode(), _node.index()));
	return false;
}

bool Simulator::visit(Stack &_node) {
	int i = _node.i();
	int j = _node.j();
	int k = _node.k();
	int firstIndex = m_stackSize - 1;
	int newi = i < firstIndex ? i : i - 1;
	int newj = j < firstIndex ? j : j - 1;
	int newk = k < firstIndex ? k : k - 1;
	switch (_node.opcode()) {
		case Stack::Opcode::PUSH_S: {
			if (i == firstIndex) {
				m_unableToConvertOpcode = true;
			} else if (i < firstIndex) {
				m_commands.emplace_back(createNode<Stack>(_node.opcode(), i));
				++m_stackSize;
			} else if (i > firstIndex) {
				m_commands.emplace_back(createNode<Stack>(_node.opcode(), i - 1));
				++m_stackSize;
			}
			break;
		}
		case Stack::Opcode::POP_S: {
			if (i == 1 && m_stackSize == 2) { // it equals to BLKDROP2 1, 1
				m_isDropped = true;
			} else if (m_stackSize == 1) {
				m_unableToConvertOpcode = true;
			} else if (i == firstIndex) {
				m_wasSet = true;
				--m_stackSize;
				// don't push to m_commands, just ignore the opcode
			} else if (i < firstIndex) {
				--m_stackSize;
				m_commands.emplace_back(createNode<Stack>(_node.opcode(), i));
			} else if (i > firstIndex) {
				// here _node.i() >= 2
				--m_stackSize;
				m_commands.emplace_back(createNode<Stack>(_node.opcode(), i - 1));
			} else {
				solUnimplemented("");
			}
			break;
		}
		case Stack::Opcode::DROP: {
			int n = i;
			if (m_stackSize <= n) {
				m_isDropped = true;
				if (n >= 2)
					m_commands.emplace_back(makeDROP(n - 1));
			} else {
				m_stackSize -= n;
				m_commands.emplace_back(makeDROP(n));
			}
			break;
		}

		case Stack::Opcode::BLKDROP2: {
			int drop = i;
			int rest = j;
			if (rest >= m_stackSize) {
				if (rest == 1) {
					m_commands.emplace_back(makeDROP(drop));
				} else {
					m_commands.emplace_back(makeBLKDROP2(drop, rest - 1));
				}
			} else if (rest + drop >= m_stackSize) {
				if (drop >= 2) {
					m_commands.emplace_back(makeBLKDROP2(drop - 1, rest));
				}
				m_isDropped = true;
			} else {
				m_commands.emplace_back(makeBLKDROP2(drop, rest));
				m_stackSize -= drop;
			}
			break;
		}

		case Stack::Opcode::BLKPUSH: {
			int n = i;
			int index = j;
			int lastIndex = index - n + 1; // include
			if (
				index < firstIndex ||
				(index > firstIndex && lastIndex > firstIndex))
			{
				m_commands.emplace_back(makeBLKPUSH(n, newj));
				m_stackSize += n;
			} else {
				m_unableToConvertOpcode = true;
			}
			break;
		}
		case Stack::Opcode::BLKSWAP:
			if (i + j < m_stackSize) {
				m_commands.emplace_back(makeBLKSWAP(i, j));
			} else {
				m_unableToConvertOpcode = true;
			}
			break;
		case Stack::Opcode::XCHG:
			if (i != firstIndex && j != firstIndex) {
				m_commands.emplace_back(createNode<Stack>(_node.opcode(), newi, newj));
			} else {
				m_unableToConvertOpcode = true;
			}
			break;
		case Stack::Opcode::PUSH2_S:
			if (i != firstIndex && j != firstIndex) {
				m_commands.emplace_back(createNode<Stack>(_node.opcode(), newi, newj));
			} else {
				m_unableToConvertOpcode = true;
			}
			m_stackSize += 2;
			break;
		case Stack::Opcode::PUSH3_S:
			if (i != firstIndex && j != firstIndex && k != firstIndex) {
				m_commands.emplace_back(createNode<Stack>(_node.opcode(), newi, newj, newk));
			} else {
				m_unableToConvertOpcode = true;
			}
			m_stackSize += 3;
			break;
		case Stack::Opcode::REVERSE: {
			int n = i;
			int startIndex = j;
			int lastIndex = startIndex + n - 1; // include
			if (startIndex <= firstIndex && firstIndex <= lastIndex) {
				m_unableToConvertOpcode = true;
			} else {
				m_commands.emplace_back(createNode<Stack>(_node.opcode(), i, j));
			}
			break;
		}
	}

	return false;
}

bool Simulator::visit(CodeBlock &/*_node*/) {
	solUnimplemented("Don't call me");
}

bool Simulator::visit(SubProgram &_node) {
	m_stackSize -= _node.take();
	if (m_stackSize <= 0) {
		m_unableToConvertOpcode = true;
	}

	std::optional<Pointer<CodeBlock>> res = trySimulate(*_node.block(), m_stackSize + _node.take(), m_stackSize + _node.ret());
	if (!res) {
		return false;
	}
	m_stackSize += _node.ret();
	m_commands.emplace_back(createNode<SubProgram>(_node.take(), _node.ret(), _node.type(), res.value()));
	return false;
}

bool Simulator::visit(TvmCondition &_node) {
	if (m_stackSize == 1) {
		m_unableToConvertOpcode = true;
		return false;
	}
	--m_stackSize;

	std::array<Pointer<CodeBlock>, 2> bodies = {_node.trueBody(), _node.falseBody()};
	for (Pointer<CodeBlock>& body : bodies) {
		if (body) {
			std::optional<Pointer<CodeBlock>> res = trySimulate(*body, m_stackSize, m_stackSize + _node.ret());
			if (!res) {
				return false;
			}
			body = res.value();
		}
	}
	m_stackSize += _node.ret();
	m_commands.emplace_back(createNode<TvmCondition>(bodies.at(0), bodies.at(1), _node.ret()));
	return false;
}

bool Simulator::visit(LogCircuit &_node) {
	if (m_stackSize == 1) {
		m_unableToConvertOpcode = true;
		return false;
	}
	m_stackSize -= 2;

	std::optional<Pointer<CodeBlock>> res = trySimulate(*_node.body(), m_stackSize + 1, m_stackSize + 1);
	if (!res) {
		return false;
	}
	++m_stackSize;
	m_commands.emplace_back(createNode<LogCircuit>(_node.type(), res.value()));
	return false;
}

bool Simulator::visit(TvmIfElse &_node) {
	if (m_stackSize == 1) {
		m_unableToConvertOpcode = true;
		return false;
	}
	--m_stackSize;

	std::array<Pointer<CodeBlock>, 2> bodies = {_node.trueBody(), _node.falseBody()};
	for (Pointer<CodeBlock>& body : bodies) {
		if (body) {
			std::optional<Pointer<CodeBlock>> res = trySimulate(*body, m_stackSize, m_stackSize);
			if (!res) {
				return false;
			}
			body = res.value();
		}
	}
	m_commands.emplace_back(createNode<TvmIfElse>(_node.type(), bodies.at(0), bodies.at(1)));
	return false;
}

bool Simulator::visit(TvmRepeat &_node) {
	if (m_stackSize == 1) {
		m_unableToConvertOpcode = true;
		return false;
	}
	--m_stackSize;

	std::optional<Pointer<CodeBlock>> res = trySimulate(*_node.body(), m_stackSize, m_stackSize);
	if (!res) {
		return false;
	}

	m_commands.emplace_back(createNode<TvmRepeat>(res.value()));
	return false;
}

bool Simulator::visit(TvmUntil &_node) {
	if (m_stackSize == 1) {
		m_unableToConvertOpcode = true;
		return false;
	}

	std::optional<Pointer<CodeBlock>> res = trySimulate(*_node.body(), m_stackSize, m_stackSize + 1);
	if (!res) {
		return false;
	}
	m_commands.emplace_back(createNode<TvmUntil>(res.value()));
	return false;
}

bool Simulator::visit(While &_node) {
	// condition
	Pointer<CodeBlock> condition = _node.condition();
	{
		std::optional<Pointer<CodeBlock>> res = trySimulate(*condition, m_stackSize, m_stackSize + 1);
		if (!res) {
			return false;
		}
		condition = res.value();
	}
	// body
	Pointer<CodeBlock> body = _node.body();
	{
		std::optional<Pointer<CodeBlock>> res = trySimulate(*body, m_stackSize, m_stackSize);
		if (!res) {
			return false;
		}
		body = res.value();
	}
	m_commands.emplace_back(createNode<While>(condition, body));
	return false;
}

bool Simulator::visit(Contract &/*_node*/) {
	solUnimplemented("");
}

bool Simulator::visit(Function &/*_node*/) {
	solUnimplemented("");
}

void Simulator::endVisit(CodeBlock &/*_node*/) {

}

std::optional<Pointer<CodeBlock>> Simulator::trySimulate(CodeBlock const& body, int begStackSize, int endStackSize) {
	Simulator sim{body.instructions().begin(), body.instructions().end(), begStackSize};
	if (sim.m_unableToConvertOpcode || sim.m_wasSet) {
		m_unableToConvertOpcode = true;
		return {};
	}
	if (sim.m_isDropped && !sim.m_wasReturnBreakContinue) {
		m_unableToConvertOpcode = true;
		return {};
	}
	solAssert(sim.m_wasReturnBreakContinue || sim.m_stackSize == endStackSize, "");
	return createNode<CodeBlock>(body.type(), sim.commands());
}

bool Simulator::isPopAndDrop(Pointer<TvmAstNode> const& a, Pointer<TvmAstNode> const& b) {
	auto pop = isPOP(a);
	auto drop = isDrop(b);
	if (pop && drop && pop.value() == m_stackSize - 1 && pop.value() == drop.value() + 1) {
		m_isDropped = true;
		m_commands.emplace_back(makeBLKDROP2(drop.value(), 1));
		return true;
	}
	return false;
}

bool Simulator::success() const {
	if (m_unableToConvertOpcode) {
		return false;
	}
	return m_isDropped && !m_wasSet;
}

bool Simulator::visitNode(TvmAstNode const&) {
	solUnimplemented("");
	return false;
}

void Simulator::endVisitNode(TvmAstNode const&) {
}