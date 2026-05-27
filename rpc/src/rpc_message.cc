#include "rpc_message.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <algorithm>

namespace wf_rpc
{

static uint64_t htonll64(uint64_t v)
{
	uint32_t hi = htonl((uint32_t)(v >> 32));
	uint32_t lo = htonl((uint32_t)(v & 0xffffffffU));
	return ((uint64_t)lo << 32) | hi;
}

static uint64_t ntohll64(uint64_t v)
{
	uint32_t hi = ntohl((uint32_t)(v >> 32));
	uint32_t lo = ntohl((uint32_t)(v & 0xffffffffU));
	return ((uint64_t)lo << 32) | hi;
}

RpcMessage::RpcMessage()
{
	this->sequence_ = 0;
	this->flags_ = 0;
	this->status_ = 0;
	this->reset_parse_state();
}

RpcMessage::RpcMessage(RpcMessage&& other) : ProtocolMessage(std::move(other))
{
	memcpy(this->send_header_, other.send_header_, RPC_HEADER_SIZE);
	memcpy(this->recv_header_, other.recv_header_, RPC_HEADER_SIZE);
	this->recv_header_bytes_ = other.recv_header_bytes_;
	this->recv_meta_bytes_ = other.recv_meta_bytes_;
	this->recv_payload_bytes_ = other.recv_payload_bytes_;
	this->expected_meta_len_ = other.expected_meta_len_;
	this->expected_payload_len_ = other.expected_payload_len_;
	this->header_ready_ = other.header_ready_;
	this->sequence_ = other.sequence_;
	this->flags_ = other.flags_;
	this->status_ = other.status_;
	this->meta_ = std::move(other.meta_);
	this->payload_ = std::move(other.payload_);

	other.reset_parse_state();
	other.sequence_ = 0;
	other.flags_ = 0;
	other.status_ = 0;
}

RpcMessage& RpcMessage::operator = (RpcMessage&& other)
{
	if (this != &other)
	{
		*(ProtocolMessage *)this = std::move(other);
		memcpy(this->send_header_, other.send_header_, RPC_HEADER_SIZE);
		memcpy(this->recv_header_, other.recv_header_, RPC_HEADER_SIZE);
		this->recv_header_bytes_ = other.recv_header_bytes_;
		this->recv_meta_bytes_ = other.recv_meta_bytes_;
		this->recv_payload_bytes_ = other.recv_payload_bytes_;
		this->expected_meta_len_ = other.expected_meta_len_;
		this->expected_payload_len_ = other.expected_payload_len_;
		this->header_ready_ = other.header_ready_;
		this->sequence_ = other.sequence_;
		this->flags_ = other.flags_;
		this->status_ = other.status_;
		this->meta_ = std::move(other.meta_);
		this->payload_ = std::move(other.payload_);

		other.reset_parse_state();
		other.sequence_ = 0;
		other.flags_ = 0;
		other.status_ = 0;
	}

	return *this;
}

void RpcMessage::reset_parse_state()
{
	this->recv_header_bytes_ = 0;
	this->recv_meta_bytes_ = 0;
	this->recv_payload_bytes_ = 0;
	this->expected_meta_len_ = 0;
	this->expected_payload_len_ = 0;
	this->header_ready_ = false;
	this->meta_.clear();
	this->payload_.clear();
}

void RpcMessage::encode_header()
{
	uint32_t magic = htonl(RPC_MAGIC);
	uint16_t version = htons(RPC_VERSION);
	uint16_t flags = htons(this->flags_);
	uint64_t seq = htonll64(this->sequence_);
	uint32_t meta_len = htonl((uint32_t)this->meta_.size());
	uint32_t payload_len = htonl((uint32_t)this->payload_.size());
	uint32_t status = htonl(this->status_);
	uint32_t reserved = 0;

	memcpy(this->send_header_, &magic, 4);
	memcpy(this->send_header_ + 4, &version, 2);
	memcpy(this->send_header_ + 6, &flags, 2);
	memcpy(this->send_header_ + 8, &seq, 8);
	memcpy(this->send_header_ + 16, &meta_len, 4);
	memcpy(this->send_header_ + 20, &payload_len, 4);
	memcpy(this->send_header_ + 24, &status, 4);
	memcpy(this->send_header_ + 28, &reserved, 4);
}

int RpcMessage::decode_header()
{
	uint32_t magic;
	uint16_t version;
	uint16_t flags;
	uint64_t seq;
	uint32_t meta_len;
	uint32_t payload_len;
	uint32_t status;

	memcpy(&magic, this->recv_header_, 4);
	memcpy(&version, this->recv_header_ + 4, 2);
	memcpy(&flags, this->recv_header_ + 6, 2);
	memcpy(&seq, this->recv_header_ + 8, 8);
	memcpy(&meta_len, this->recv_header_ + 16, 4);
	memcpy(&payload_len, this->recv_header_ + 20, 4);
	memcpy(&status, this->recv_header_ + 24, 4);

	magic = ntohl(magic);
	version = ntohs(version);
	flags = ntohs(flags);
	seq = ntohll64(seq);
	meta_len = ntohl(meta_len);
	payload_len = ntohl(payload_len);
	status = ntohl(status);

	if (magic != RPC_MAGIC || version != RPC_VERSION)
	{
		errno = EBADMSG;
		return -1;
	}

	size_t body_size = (size_t)meta_len + (size_t)payload_len;
	if (body_size > this->size_limit)
	{
		errno = EMSGSIZE;
		return -1;
	}

	this->flags_ = flags;
	this->sequence_ = seq;
	this->status_ = status;
	this->expected_meta_len_ = meta_len;
	this->expected_payload_len_ = payload_len;
	this->meta_.assign(meta_len, '\0');
	this->payload_.assign(payload_len, '\0');
	this->recv_meta_bytes_ = 0;
	this->recv_payload_bytes_ = 0;
	this->header_ready_ = true;
	return 0;
}

int RpcMessage::encode(struct iovec vectors[], int max)
{
	int cnt = 1;
	if (!this->meta_.empty())
		cnt++;
	if (!this->payload_.empty())
		cnt++;

	if (cnt > max)
	{
		errno = EOVERFLOW;
		return -1;
	}

	this->encode_header();
	vectors[0].iov_base = this->send_header_;
	vectors[0].iov_len = RPC_HEADER_SIZE;

	int idx = 1;
	if (!this->meta_.empty())
	{
		vectors[idx].iov_base = (void *)this->meta_.data();
		vectors[idx].iov_len = this->meta_.size();
		idx++;
	}

	if (!this->payload_.empty())
	{
		vectors[idx].iov_base = (void *)this->payload_.data();
		vectors[idx].iov_len = this->payload_.size();
		idx++;
	}

	return idx;
}

int RpcMessage::append(const void *buf, size_t *size)
{
	const char *p = (const char *)buf;
	size_t remain = *size;
	size_t used = 0;

	while (remain > 0)
	{
		if (!this->header_ready_)
		{
			size_t n = std::min(remain, (size_t)RPC_HEADER_SIZE - this->recv_header_bytes_);
			memcpy(this->recv_header_ + this->recv_header_bytes_, p, n);
			this->recv_header_bytes_ += n;
			p += n;
			remain -= n;
			used += n;

			if (this->recv_header_bytes_ < RPC_HEADER_SIZE)
				break;

			if (this->decode_header() < 0)
			{
				*size = used;
				return -1;
			}

			if (this->expected_meta_len_ == 0 && this->expected_payload_len_ == 0)
			{
				*size = used;
				return 1;
			}
		}

		if (this->recv_meta_bytes_ < this->expected_meta_len_)
		{
			size_t n = std::min(remain, (size_t)this->expected_meta_len_ - this->recv_meta_bytes_);
			memcpy(&this->meta_[this->recv_meta_bytes_], p, n);
			this->recv_meta_bytes_ += n;
			p += n;
			remain -= n;
			used += n;

			if (this->recv_meta_bytes_ < this->expected_meta_len_)
				break;
		}

		if (this->recv_payload_bytes_ < this->expected_payload_len_)
		{
			size_t n = std::min(remain, (size_t)this->expected_payload_len_ - this->recv_payload_bytes_);
			memcpy(&this->payload_[this->recv_payload_bytes_], p, n);
			this->recv_payload_bytes_ += n;
			p += n;
			remain -= n;
			used += n;

			if (this->recv_payload_bytes_ < this->expected_payload_len_)
				break;
		}

		if (this->recv_meta_bytes_ == this->expected_meta_len_ &&
			this->recv_payload_bytes_ == this->expected_payload_len_)
		{
			*size = used;
			return 1;
		}
	}

	*size = used;
	return 0;
}

int RpcMessage::append(const void *buf, size_t size)
{
	size_t used = size;
	int ret = this->append(buf, &used);

	return ret;
}

int RpcMessage::set_service_method(const std::string& service, const std::string& method)
{
	if (service.size() > 0xffff || method.size() > 0xffff)
	{
		errno = EINVAL;
		return -1;
	}

	uint16_t s_len = htons((uint16_t)service.size());
	uint16_t m_len = htons((uint16_t)method.size());
	this->meta_.resize(4 + service.size() + method.size());
	memcpy(&this->meta_[0], &s_len, 2);
	memcpy(&this->meta_[2], &m_len, 2);
	memcpy(&this->meta_[4], service.data(), service.size());
	memcpy(&this->meta_[4 + service.size()], method.data(), method.size());
	return 0;
}

bool RpcMessage::get_service_method(std::string *service, std::string *method) const
{
	if (this->meta_.size() < 4)
		return false;

	uint16_t s_len;
	uint16_t m_len;
	memcpy(&s_len, &this->meta_[0], 2);
	memcpy(&m_len, &this->meta_[2], 2);
	s_len = ntohs(s_len);
	m_len = ntohs(m_len);

	size_t expect = 4 + (size_t)s_len + (size_t)m_len;
	if (expect != this->meta_.size())
		return false;

	if (service)
		service->assign(this->meta_.data() + 4, s_len);
	if (method)
		method->assign(this->meta_.data() + 4 + s_len, m_len);
	return true;
}

} // namespace wf_rpc
