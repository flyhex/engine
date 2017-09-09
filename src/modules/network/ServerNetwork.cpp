#include "ServerNetwork.h"
#include "core/Trace.h"
#include "core/Log.h"
#include "ClientMessages_generated.h"

namespace network {

ServerNetwork::ServerNetwork(const ProtocolHandlerRegistryPtr& protocolHandlerRegistry, const core::EventBusPtr& eventBus) :
		Super(protocolHandlerRegistry, eventBus) {
}

bool ServerNetwork::packetReceived(ENetEvent& event) {
	flatbuffers::Verifier v(event.packet->data, event.packet->dataLength);

	if (!VerifyClientMessageBuffer(v)) {
		Log::error("Illegal client packet received with length: %i", (int)event.packet->dataLength);
		return false;
	}
	const ClientMessage *req = GetClientMessage(event.packet->data);
	ClientMsgType type = req->data_type();
	const char *clientMsgType = EnumNameClientMsgType(type);
	ProtocolHandlerPtr handler = _protocolHandlerRegistry->getHandler(clientMsgType);
	if (!handler) {
		Log::error("No handler for client msg type %s", clientMsgType);
		return false;
	}
	Log::debug("Received %s", clientMsgType);
	handler->execute(event.peer, reinterpret_cast<const flatbuffers::Table*>(req->data()));
	return true;
}

bool ServerNetwork::bind(uint16_t port, const std::string& hostname, int maxPeers, int maxChannels) {
	if (_server) {
		return false;
	}
	if (maxPeers <= 0) {
		return false;
	}
	if (maxChannels <= 0) {
		return false;
	}
	ENetAddress address;
	if (hostname.empty()) {
		address.host = ENET_HOST_ANY;
	} else {
		enet_address_set_host(&address, hostname.c_str());
	}
	address.port = port;
	_server = enet_host_create(
			&address,
			maxPeers,
			maxChannels,
			0, /* assume any amount of incoming bandwidth */
			0 /* assume any amount of outgoing bandwidth */
			);
	if (_server == nullptr) {
		return false;
	}
	enet_host_compress_with_range_coder(_server);
	return true;
}

void ServerNetwork::shutdown() {
	if (_server != nullptr) {
		enet_host_flush(_server);
		enet_host_destroy(_server);
	}
	_server = nullptr;
	Super::shutdown();
}

void ServerNetwork::update() {
	core_trace_scoped(Network);
	updateHost(_server);
}

}
