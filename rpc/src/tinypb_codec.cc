#include "tinypb_codec.h"
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

namespace wf_rpc
{

uint32_t TinyPbCodec::encode_uint32(uint32_t hostlong)
{
    return ::htonl(hostlong);
}

uint32_t TinyPbCodec::decode_uint32(uint32_t netlong)
{
    return ::ntohl(netlong);
}

int TinyPbCodec::encode(const TinyPbStruct& data, std::string& out)
{
    size_t msg_req_len = data.msg_req.size();
    size_t service_name_len = data.service_full_name.size();
    size_t err_info_len = data.err_info.size();
    size_t pb_data_len = data.pb_data.size();
    
    const size_t header_size = 1 + 4 + 4 + 4 + 4 + 4 + 4 + 1;
    const size_t variable_size = msg_req_len + service_name_len + err_info_len + pb_data_len;
    
    if (variable_size > SIZE_MAX - header_size)
    {
        errno = EMSGSIZE;
        return -1;
    }
    
    size_t total_len = header_size + variable_size;
    
    out.resize(total_len);
    char* ptr = &out[0];
    
    *ptr++ = TINYPB_START;
    
    uint32_t pk_len = encode_uint32((uint32_t)(total_len - 1 - 1));
    memcpy(ptr, &pk_len, 4);
    ptr += 4;
    
    uint32_t req_len = encode_uint32((uint32_t)msg_req_len);
    memcpy(ptr, &req_len, 4);
    ptr += 4;
    
    memcpy(ptr, data.msg_req.data(), msg_req_len);
    ptr += msg_req_len;
    
    uint32_t service_len = encode_uint32((uint32_t)service_name_len);
    memcpy(ptr, &service_len, 4);
    ptr += 4;
    
    memcpy(ptr, data.service_full_name.data(), service_name_len);
    ptr += service_name_len;
    
    uint32_t err_code = encode_uint32((uint32_t)data.err_code);
    memcpy(ptr, &err_code, 4);
    ptr += 4;
    
    uint32_t info_len = encode_uint32((uint32_t)err_info_len);
    memcpy(ptr, &info_len, 4);
    ptr += 4;
    
    memcpy(ptr, data.err_info.data(), err_info_len);
    ptr += err_info_len;
    
    memcpy(ptr, data.pb_data.data(), pb_data_len);
    ptr += pb_data_len;
    
    uint32_t checksum = encode_uint32(TINYPB_CHECKSUM_VALUE);
    memcpy(ptr, &checksum, 4);
    ptr += 4;
    
    *ptr = TINYPB_END;
    
    return 0;
}

int TinyPbCodec::decode(const std::string& in, TinyPbStruct& data)
{
    if (in.size() < TINYPB_FIXED_HEADER_SIZE)
    {
        errno = EINVAL;
        return -1;
    }
    
    const char* ptr = in.data();
    size_t pos = 0;
    
    if (ptr[pos++] != TINYPB_START)
    {
        errno = EBADMSG;
        return -1;
    }
    
    uint32_t pk_len;
    memcpy(&pk_len, ptr + pos, 4);
    pk_len = decode_uint32(pk_len);
    pos += 4;
    
    if (pk_len + 2 != in.size())
    {
        errno = EBADMSG;
        return -1;
    }
    
    uint32_t msg_req_len;
    memcpy(&msg_req_len, ptr + pos, 4);
    msg_req_len = decode_uint32(msg_req_len);
    pos += 4;
    
    data.msg_req.assign(ptr + pos, msg_req_len);
    pos += msg_req_len;
    
    uint32_t service_name_len;
    memcpy(&service_name_len, ptr + pos, 4);
    service_name_len = decode_uint32(service_name_len);
    pos += 4;
    
    data.service_full_name.assign(ptr + pos, service_name_len);
    pos += service_name_len;
    
    uint32_t err_code;
    memcpy(&err_code, ptr + pos, 4);
    data.err_code = decode_uint32(err_code);
    pos += 4;
    
    uint32_t err_info_len;
    memcpy(&err_info_len, ptr + pos, 4);
    err_info_len = decode_uint32(err_info_len);
    pos += 4;
    
    data.err_info.assign(ptr + pos, err_info_len);
    pos += err_info_len;
    
    uint32_t checksum;
    size_t pb_data_end = in.size() - 4 - 1;
    data.pb_data.assign(ptr + pos, pb_data_end - pos);
    pos = pb_data_end;
    
    memcpy(&checksum, ptr + pos, 4);
    checksum = decode_uint32(checksum);
    pos += 4;
    
    if (checksum != TINYPB_CHECKSUM_VALUE)
    {
        errno = EBADMSG;
        return -1;
    }
    
    if (ptr[pos] != TINYPB_END)
    {
        errno = EBADMSG;
        return -1;
    }
    
    return 0;
}

}