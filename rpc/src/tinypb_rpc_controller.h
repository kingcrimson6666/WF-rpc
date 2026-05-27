#ifndef WF_RPC_TINYPB_RPC_CONTROLLER_H
#define WF_RPC_TINYPB_RPC_CONTROLLER_H

#include <google/protobuf/service.h>
#include <string>

namespace wf_rpc
{

class TinyPbRpcController : public google::protobuf::RpcController
{
public:
    TinyPbRpcController();
    ~TinyPbRpcController() override = default;
    
    void Reset() override;
    bool Failed() const override;
    std::string ErrorText() const override;
    void SetFailed(const std::string& reason) override;
    
    void StartCancel() override;
    bool IsCanceled() const override;
    void NotifyOnCancel(google::protobuf::Closure* callback) override;
    
    void SetTimeout(int ms);
    int GetTimeout() const;
    
    void SetErrorCode(int code);
    int GetErrorCode() const;
    
    void SetMsgReq(const std::string& msg_req);
    std::string GetMsgReq() const;

private:
    bool failed_;
    std::string error_text_;
    int error_code_;
    int timeout_ms_;
    std::string msg_req_;
    bool canceled_;
};

}

#endif