#ifndef WF_RPC_TINYPB_CODEC_H
#define WF_RPC_TINYPB_CODEC_H

#include <string>
#include "tinypb_struct.h"
#include "tinypb_constants.h"

namespace wf_rpc
{

class TinyPbCodec
{
public:
    static int encode(const TinyPbStruct& data, std::string& out);
    static int decode(const std::string& in, TinyPbStruct& data);
    
private:
	static uint32_t encode_uint32(uint32_t hostlong);
	static uint32_t decode_uint32(uint32_t netlong);
};

}

#endif