#ifndef WF_RPC_TINYPB_STRUCT_H
#define WF_RPC_TINYPB_STRUCT_H

#include <string>

namespace wf_rpc
{

struct TinyPbStruct
{
    std::string msg_req;
    std::string service_full_name;
    uint32_t err_code;
    std::string err_info;
    std::string pb_data;

    TinyPbStruct() : err_code(0) {}
    
    TinyPbStruct(const std::string& req, const std::string& service, uint32_t code, 
                 const std::string& info, const std::string& data)
        : msg_req(req), service_full_name(service), err_code(code), 
          err_info(info), pb_data(data) {}
};

}

#endif