#ifndef WF_RPC_MESSAGE_H
#define WF_RPC_MESSAGE_H

#include <stdint.h>
#include <string>
#include <utility>
#include "workflow/ProtocolMessage.h"

namespace wf_rpc
{

static const uint32_t RPC_MAGIC = 0x57525043U; /* WRPC */
static const uint16_t RPC_VERSION = 1;
static const size_t RPC_HEADER_SIZE = 32;

class RpcMessage : public protocol::ProtocolMessage
{
public:
	RpcMessage();
	RpcMessage(RpcMessage&& other);
	RpcMessage& operator = (RpcMessage&& other);
	virtual ~RpcMessage() { }

	void set_sequence(uint64_t seq) { this->sequence_ = seq; }
	uint64_t get_sequence() const { return this->sequence_; }

	void set_flags(uint16_t flags) { this->flags_ = flags; }
	uint16_t get_flags() const { return this->flags_; }

	void set_status(uint32_t status) { this->status_ = status; }
	uint32_t get_status() const { return this->status_; }

	void set_payload(std::string payload) { this->payload_ = std::move(payload); }
	const std::string& payload() const { return this->payload_; }

	int set_service_method(const std::string& service, const std::string& method);
	bool get_service_method(std::string *service, std::string *method) const;

protected:
	virtual int encode(struct iovec vectors[], int max);
	virtual int append(const void *buf, size_t *size);
	virtual int append(const void *buf, size_t size);

private:
	void reset_parse_state();
	void encode_header();
	int decode_header();

private:
	char send_header_[RPC_HEADER_SIZE];
	char recv_header_[RPC_HEADER_SIZE];
	size_t recv_header_bytes_;
	size_t recv_meta_bytes_;
	size_t recv_payload_bytes_;
	uint32_t expected_meta_len_;
	uint32_t expected_payload_len_;
	bool header_ready_;

	uint64_t sequence_;
	uint16_t flags_;
	uint32_t status_;
	std::string meta_;
	std::string payload_;
};

using RpcRequest = RpcMessage;
using RpcResponse = RpcMessage;

} // namespace wf_rpc

#endif
