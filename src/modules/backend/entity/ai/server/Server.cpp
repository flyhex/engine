/**
 * @file
 */

#include "Server.h"
#include "SelectHandler.h"
#include "PauseHandler.h"
#include "ResetHandler.h"
#include "StepHandler.h"
#include "ChangeHandler.h"
#include "AddNodeHandler.h"
#include "DeleteNodeHandler.h"
#include "UpdateNodeHandler.h"

#include "ai-shared/protocol/AIPauseMessage.h"
#include "ai-shared/protocol/AIStateMessage.h"
#include "ai-shared/protocol/AINamesMessage.h"
#include "ai-shared/protocol/AICharacterDetailsMessage.h"
#include "ai-shared/protocol/AICharacterStaticMessage.h"

#include "backend/entity/ai/condition/ConditionParser.h"
#include "backend/entity/ai/tree/TreeNodeParser.h"
#include "core/Trace.h"

namespace backend {

namespace {
const int SV_BROADCAST_CHRDETAILS = 1 << 0;
const int SV_BROADCAST_STATE      = 1 << 1;
}

Server::Server(AIRegistry& aiRegistry, short port, const core::String& hostname) :
		_aiRegistry(aiRegistry), _network(port, hostname), _selectedCharacterId(AI_NOTHING_SELECTED), _time(0L),
		_selectHandler(new SelectHandler(*this)), _pauseHandler(new PauseHandler(*this)), _resetHandler(new ResetHandler(*this)),
		_stepHandler(new StepHandler(*this)), _changeHandler(new ChangeHandler(*this)), _addNodeHandler(new AddNodeHandler(*this)),
		_deleteNodeHandler(new DeleteNodeHandler(*this)), _updateNodeHandler(new UpdateNodeHandler(*this)), _pause(false), _zone(nullptr) {
	_network.addListener(this);
	ai::ProtocolHandlerRegistry& r = ai::ProtocolHandlerRegistry::get();
	r.registerHandler(ai::PROTO_SELECT, _selectHandler);
	r.registerHandler(ai::PROTO_PAUSE, _pauseHandler);
	r.registerHandler(ai::PROTO_RESET, _resetHandler);
	r.registerHandler(ai::PROTO_STEP, _stepHandler);
	r.registerHandler(ai::PROTO_PING, &_nopHandler);
	r.registerHandler(ai::PROTO_CHANGE, _changeHandler);
	r.registerHandler(ai::PROTO_ADDNODE, _addNodeHandler);
	r.registerHandler(ai::PROTO_DELETENODE, _deleteNodeHandler);
	r.registerHandler(ai::PROTO_UPDATENODE, _updateNodeHandler);
}

Server::~Server() {
	delete _selectHandler;
	delete _pauseHandler;
	delete _resetHandler;
	delete _stepHandler;
	delete _changeHandler;
	delete _addNodeHandler;
	delete _deleteNodeHandler;
	delete _updateNodeHandler;
	_network.removeListener(this);
}

void Server::enqueueEvent(const Event& event) {
	core::ScopedLock scopedLock(_lock);
	_events.push_back(event);
}

void Server::onConnect(Client* client) {
	Event event;
	event.type = EV_NEWCONNECTION;
	event.data.newClient = client;
	enqueueEvent(event);
}

void Server::onDisconnect(Client* /*client*/) {
	Log::info("remote debugger disconnect (%i)", _network.getConnectedClients());
	Zone* zone = _zone;
	if (zone == nullptr) {
		return;
	}

	// if there are still connected clients left, don't disable the debug mode for the zone
	if (_network.getConnectedClients() > 0) {
		return;
	}

	zone->setDebug(false);
	if (_zone.compare_exchange(zone, nullptr) != nullptr) {
		// restore the zone state if no player is left for debugging
		const bool pauseState = _pause;
		if (pauseState) {
			pause(0, false);
		}

		// only if noone else already started a new debug session
		resetSelection();
	}
}

bool Server::start() {
	return _network.start();
}

void Server::addChildren(const TreeNodePtr& node, core::DynamicArray<ai::AIStateNodeStatic>& out) const {
	for (const TreeNodePtr& childNode : node->getChildren()) {
		const int32_t nodeId = childNode->getId();
		out.push_back(ai::AIStateNodeStatic(nodeId, childNode->getName(), childNode->getType(), childNode->getParameters(), childNode->getCondition()->getName(), childNode->getCondition()->getParameters()));
		addChildren(childNode, out);
	}
}

void Server::addChildren(const TreeNodePtr& node, ai::AIStateNode& parent, const AIPtr& ai) const {
	const TreeNodes& children = node->getChildren();
	std::vector<bool> currentlyRunning(children.size());
	node->getRunningChildren(ai, currentlyRunning);
	const int64_t aiTime = ai->_time;
	const size_t length = children.size();
	for (size_t i = 0u; i < length; ++i) {
		const TreeNodePtr& childNode = children[i];
		const int32_t id = childNode->getId();
		const ConditionPtr& condition = childNode->getCondition();
		const core::String conditionStr = condition ? condition->getNameWithConditions(ai) : "";
		const int64_t lastRun = childNode->getLastExecMillis(ai);
		const int64_t delta = lastRun == -1 ? -1 : aiTime - lastRun;
		ai::AIStateNode child(id, conditionStr, delta, childNode->getLastStatus(ai), currentlyRunning[i]);
		addChildren(childNode, child, ai);
		parent.addChildren(child);
	}
}

void Server::broadcastState(const Zone* zone) {
	core_trace_scoped(AIServerBroadcastState);
	_broadcastMask |= SV_BROADCAST_STATE;
	ai::AIStateMessage msg;
	auto func = [&] (const AIPtr& ai) {
		const ICharacterPtr& chr = ai->getCharacter();
		const ai::AIStateWorld b(chr->getId(), chr->getPosition(), chr->getOrientation(), chr->getAttributes());
		msg.addState(b);
	};
	zone->execute(func);
	_network.broadcast(msg);
}

void Server::broadcastStaticCharacterDetails(const Zone* zone) {
	const ai::CharacterId id = _selectedCharacterId;
	if (id == AI_NOTHING_SELECTED) {
		return;
	}

	static const auto func = [&] (const AIPtr& ai) {
		if (!ai) {
			return false;
		}
		core::DynamicArray<ai::AIStateNodeStatic> nodeStaticData;
		const TreeNodePtr& node = ai->getBehaviour();
		const int32_t nodeId = node->getId();
		nodeStaticData.push_back(ai::AIStateNodeStatic(nodeId, node->getName(), node->getType(), node->getParameters(), node->getCondition()->getName(), node->getCondition()->getParameters()));
		addChildren(node, nodeStaticData);

		const ai::AICharacterStaticMessage msgStatic(ai->getId(), nodeStaticData);
		_network.broadcast(msgStatic);
		return true;
	};
	if (!zone->execute(id, func)) {
		resetSelection();
	}
}

void Server::broadcastCharacterDetails(const Zone* zone) {
	core_trace_scoped(AIServerBroadcastCharacterDetails);
	_broadcastMask |= SV_BROADCAST_CHRDETAILS;
	const ai::CharacterId id = _selectedCharacterId;
	if (id == AI_NOTHING_SELECTED) {
		return;
	}

	static const auto func = [&] (const AIPtr& ai) {
		if (!ai) {
			return false;
		}
		const TreeNodePtr& node = ai->getBehaviour();
		const int32_t nodeId = node->getId();
		const ConditionPtr& condition = node->getCondition();
		const core::String conditionStr = condition ? condition->getNameWithConditions(ai) : "";
		ai::AIStateNode root(nodeId, conditionStr, _time - node->getLastExecMillis(ai), node->getLastStatus(ai), true);
		addChildren(node, root, ai);

		ai::AIStateAggro aggro;
		const AggroMgr::Entries& entries = ai->getAggroMgr().getEntries();
		aggro.reserve(entries.size());
		for (const Entry& e : entries) {
			aggro.addAggro(ai::AIStateAggroEntry(e.getCharacterId(), e.getAggro()));
		}

		const ai::AICharacterDetailsMessage msg(ai->getId(), aggro, root);
		_network.broadcast(msg);
		return true;
	};
	if (!zone->execute(id, func)) {
		resetSelection();
	}
}

void Server::handleEvents(Zone* zone, bool pauseState) {
	std::vector<Event> events;
	{
		core::ScopedLock scopedLock(_lock);
		events = std::move(_events);
		_events.clear();
	}
	for (Event& event : events) {
		switch (event.type) {
		case EV_SELECTION: {
			if (zone == nullptr || event.data.characterId == AI_NOTHING_SELECTED) {
				resetSelection();
			} else {
				_selectedCharacterId = event.data.characterId;
				broadcastStaticCharacterDetails(zone);
				if (pauseState) {
					broadcastState(zone);
					broadcastCharacterDetails(zone);
				}
			}
			break;
		}
		case EV_STEP: {
			const int64_t queuedStepMillis = event.data.stepMillis;
			auto func = [=] (const AIPtr& ai) {
				if (!ai->isPause())
					return;
				ai->setPause(false);
				ai->update(queuedStepMillis, true);
				ai->getBehaviour()->execute(ai, queuedStepMillis);
				ai->setPause(true);
			};
			if (zone != nullptr) {
				zone->executeParallel(func);
				broadcastState(zone);
				broadcastCharacterDetails(zone);
			}
			break;
		}
		case EV_RESET: {
			static auto func = [] (const AIPtr& ai) {
				ai->getBehaviour()->resetState(ai);
			};
			event.data.zone->executeParallel(func);
			break;
		}
		case EV_PAUSE: {
			const bool newPauseState = event.data.pauseState;
			_pause = newPauseState;
			if (zone != nullptr) {
				auto func = [=] (const AIPtr& ai) {
					ai->setPause(newPauseState);
				};
				zone->executeParallel(func);
				_network.broadcast(ai::AIPauseMessage(newPauseState));
				// send the last time the most recent state until we unpause
				if (newPauseState) {
					broadcastState(zone);
					broadcastCharacterDetails(zone);
				}
			}
			break;
		}
		case EV_UPDATESTATICCHRDETAILS: {
			broadcastStaticCharacterDetails(event.data.zone);
			break;
		}
		case EV_NEWCONNECTION: {
			_network.sendToClient(event.data.newClient, ai::AIPauseMessage(pauseState));
			_network.sendToClient(event.data.newClient, ai::AINamesMessage(_names));
			Log::info("new remote debugger connection (%i)", _network.getConnectedClients());
			break;
		}
		case EV_ZONEADD: {
			if (!_zones.insert(event.data.zone)) {
				return;
			}
			_names.clear();
			for (const auto& z : _zones) {
				_names.push_back(z->first->getName());
			}
			_network.broadcast(ai::AINamesMessage(_names));
			break;
		}
		case EV_ZONEREMOVE: {
			_zone.compare_exchange(event.data.zone, nullptr);
			if (_zones.remove(event.data.zone) != 1) {
				return;
			}
			_names.clear();
			for (const auto& z : _zones) {
				_names.push_back(z->first->getName());
			}
			_network.broadcast(ai::AINamesMessage(_names));
			break;
		}
		case EV_SETDEBUG: {
			if (_pause) {
				pause(0, false);
			}

			Zone* nullzone = nullptr;
			_zone = nullzone;
			resetSelection();

			for (const auto& iter : _zones) {
				Zone* z = iter->first;
				const bool debug = z->getName() == event.strData;
				if (!debug) {
					continue;
				}
				if (_zone.compare_exchange(nullzone, z)) {
					z->setDebug(debug);
				}
			}

			break;
		}
		case EV_MAX:
			break;
		}
	}
}

void Server::resetSelection() {
	_selectedCharacterId = AI_NOTHING_SELECTED;
}

bool Server::updateNode(const ai::CharacterId& characterId, int32_t nodeId, const core::String& name, const core::String& type, const core::String& condition) {
	Zone* zone = _zone;
	if (zone == nullptr) {
		return false;
	}
	const AIPtr& ai = zone->getAI(characterId);
	const TreeNodePtr& node = ai->getBehaviour()->getId() == nodeId ? ai->getBehaviour() : ai->getBehaviour()->getChild(nodeId);
	if (!node) {
		return false;
	}
	ConditionParser conditionParser(_aiRegistry, condition);
	const ConditionPtr& conditionPtr = conditionParser.getCondition();
	if (!conditionPtr) {
		Log::error("Failed to parse the condition '%s'", condition.c_str());
		return false;
	}
	TreeNodeParser treeNodeParser(_aiRegistry, type);
	TreeNodePtr newNode = treeNodeParser.getTreeNode(name);
	if (!newNode) {
		Log::error("Failed to parse the node '%s'", type.c_str());
		return false;
	}
	newNode->setCondition(conditionPtr);
	for (auto& child : node->getChildren()) {
		newNode->addChild(child);
	}

	const TreeNodePtr& root = ai->getBehaviour();
	if (node == root) {
		ai->setBehaviour(newNode);
	} else {
		const TreeNodePtr& parent = root->getParent(root, nodeId);
		if (!parent) {
			Log::error("No parent for non-root node '%i'", nodeId);
			return false;
		}
		parent->replaceChild(nodeId, newNode);
	}

	Event event;
	event.type = EV_UPDATESTATICCHRDETAILS;
	event.data.zone = zone;
	enqueueEvent(event);
	return true;
}

bool Server::addNode(const ai::CharacterId& characterId, int32_t parentNodeId, const core::String& name, const core::String& type, const core::String& condition) {
	Zone* zone = _zone;
	if (zone == nullptr) {
		return false;
	}
	const AIPtr& ai = zone->getAI(characterId);
	TreeNodePtr node = ai->getBehaviour();
	if (node->getId() != parentNodeId) {
		node = node->getChild(parentNodeId);
	}
	if (!node) {
		return false;
	}
	ConditionParser conditionParser(_aiRegistry, condition);
	const ConditionPtr& conditionPtr = conditionParser.getCondition();
	if (!conditionPtr) {
		Log::error("Failed to parse the condition '%s'", condition.c_str());
		return false;
	}
	TreeNodeParser treeNodeParser(_aiRegistry, type);
	TreeNodePtr newNode = treeNodeParser.getTreeNode(name);
	if (!newNode) {
		Log::error("Failed to parse the node '%s'", type.c_str());
		return false;
	}
	newNode->setCondition(conditionPtr);
	if (!node->addChild(newNode)) {
		return false;
	}

	Event event;
	event.type = EV_UPDATESTATICCHRDETAILS;
	event.data.zone = zone;
	enqueueEvent(event);
	return true;
}

bool Server::deleteNode(const ai::CharacterId& characterId, int32_t nodeId) {
	Zone* zone = _zone;
	if (zone == nullptr) {
		return false;
	}
	const AIPtr& ai = zone->getAI(characterId);
	// don't delete the root
	const TreeNodePtr& root = ai->getBehaviour();
	if (root->getId() == nodeId) {
		return false;
	}

	const TreeNodePtr& parent = root->getParent(root, nodeId);
	if (!parent) {
		Log::error("No parent for non-root node '%i'", nodeId);
		return false;
	}
	parent->replaceChild(nodeId, TreeNodePtr());
	Event event;
	event.type = EV_UPDATESTATICCHRDETAILS;
	event.data.zone = zone;
	enqueueEvent(event);
	return true;
}

void Server::addZone(Zone* zone) {
	Event event;
	event.type = EV_ZONEADD;
	event.data.zone = zone;
	enqueueEvent(event);
}

void Server::removeZone(Zone* zone) {
	Event event;
	event.type = EV_ZONEREMOVE;
	event.data.zone = zone;
	enqueueEvent(event);
}

void Server::setDebug(const core::String& zoneName) {
	Event event;
	event.type = EV_SETDEBUG;
	event.strData = zoneName;
	enqueueEvent(event);
}

void Server::reset() {
	Zone* zone = _zone;
	if (zone == nullptr) {
		return;
	}
	Event event;
	event.type = EV_RESET;
	event.data.zone = zone;
	enqueueEvent(event);
}

void Server::select(const ai::ClientId& /*clientId*/, const ai::CharacterId& id) {
	Event event;
	event.type = EV_SELECTION;
	event.data.characterId = id;
	enqueueEvent(event);
}

void Server::pause(const ai::ClientId& /*clientId*/, bool state) {
	Event event;
	event.type = EV_PAUSE;
	event.data.pauseState = state;
	enqueueEvent(event);
}

void Server::step(int64_t stepMillis) {
	Event event;
	event.type = EV_STEP;
	event.data.stepMillis = stepMillis;
	enqueueEvent(event);
}

void Server::update(int64_t deltaTime) {
	core_trace_scoped(AIServerUpdate);
	_time += deltaTime;
	const int clients = _network.getConnectedClients();
	Zone* zone = _zone;
	bool pauseState = _pause;
	_broadcastMask = 0u;

	handleEvents(zone, pauseState);

	if (clients > 0 && zone != nullptr) {
		if (!pauseState) {
			if ((_broadcastMask & SV_BROADCAST_STATE) == 0) {
				broadcastState(zone);
			}
			if ((_broadcastMask & SV_BROADCAST_CHRDETAILS) == 0) {
				broadcastCharacterDetails(zone);
			}
		}
	} else if (pauseState) {
		pause(1, false);
		resetSelection();
	}
	_network.update(deltaTime);
}

}
