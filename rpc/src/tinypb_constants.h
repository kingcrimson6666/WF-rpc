#ifndef WF_RPC_TINYPB_CONSTANTS_H
#define WF_RPC_TINYPB_CONSTANTS_H

#include <stdint.h>

namespace wf_rpc
{

static const uint8_t TINYPB_START = 0x02;
static const uint8_t TINYPB_END = 0x03;
static const uint32_t TINYPB_CHECKSUM_VALUE = 1;

static const size_t TINYPB_PK_LEN_SIZE = 4;
static const size_t TINYPB_MSG_REQ_LEN_SIZE = 4;
static const size_t TINYPB_SERVICE_NAME_LEN_SIZE = 4;
static const size_t TINYPB_ERR_CODE_SIZE = 4;
static const size_t TINYPB_ERR_INFO_LEN_SIZE = 4;
static const size_t TINYPB_CHECKSUM_SIZE = 4;

static const size_t TINYPB_FIXED_HEADER_SIZE = 1 + 4 + 4 + 4 + 4 + 4 + 4 + 1;

}

#endif