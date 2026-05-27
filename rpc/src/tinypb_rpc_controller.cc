#include "tinypb_rpc_controller.h"

namespace wf_rpc
{

TinyPbRpcController::TinyPbRpcController()
    : failed_(false), error_code_(0), timeout_ms_(5000), canceled_(false)
{
}

void TinyPbRpcController::Reset()
{
    failed_ = false;
    error_text_.clear();
    error_code_ = 0;
    timeout_ms_ = 5000;
    msg_req_.clear();
    canceled_ = false;
}

bool TinyPbRpcController::Failed() const
{
    return failed_;
}

std::string TinyPbRpcController::ErrorText() const
{
    return error_text_;
}

void TinyPbRpcController::SetFailed(const std::string& reason)
{
    failed_ = true;
    error_text_ = reason;
}

void TinyPbRpcController::StartCancel()
{
    canceled_ = true;
}

bool TinyPbRpcController::IsCanceled() const
{
    return canceled_;
}

void TinyPbRpcController::NotifyOnCancel(google::protobuf::Closure* callback)
{
    (void)callback;
}

void TinyPbRpcController::SetTimeout(int ms)
{
    timeout_ms_ = ms;
}

int TinyPbRpcController::GetTimeout() const
{
    return timeout_ms_;
}

void TinyPbRpcController::SetErrorCode(int code)
{
    error_code_ = code;
}

int TinyPbRpcController::GetErrorCode() const
{
    return error_code_;
}

void TinyPbRpcController::SetMsgReq(const std::string& msg_req)
{
    msg_req_ = msg_req;
}

std::string TinyPbRpcController::GetMsgReq() const
{
    return msg_req_;
}

}