#include <gtest/gtest.h>
#include "tinypb_codec.h"
#include "tinypb_struct.h"

TEST(TinyPbCodecTest, EncodeDecode)
{
    wf_rpc::TinyPbStruct request;
    request.msg_req = "test_req_001";
    request.service_full_name = "EchoService.Echo";
    request.err_code = 0;
    request.err_info = "";
    request.pb_data = "test_protobuf_data";
    
    std::string encoded;
    int ret = wf_rpc::TinyPbCodec::encode(request, encoded);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(encoded.empty());
    
    wf_rpc::TinyPbStruct response;
    ret = wf_rpc::TinyPbCodec::decode(encoded, response);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(response.msg_req, "test_req_001");
    ASSERT_EQ(response.service_full_name, "EchoService.Echo");
    ASSERT_EQ(response.err_code, 0);
    ASSERT_EQ(response.err_info, "");
    ASSERT_EQ(response.pb_data, "test_protobuf_data");
}

TEST(TinyPbCodecTest, ErrorResponse)
{
    wf_rpc::TinyPbStruct request;
    request.msg_req = "test_req_002";
    request.service_full_name = "TestService.TestMethod";
    request.err_code = 404;
    request.err_info = "service not found";
    request.pb_data = "";
    
    std::string encoded;
    int ret = wf_rpc::TinyPbCodec::encode(request, encoded);
    ASSERT_EQ(ret, 0);
    
    wf_rpc::TinyPbStruct response;
    ret = wf_rpc::TinyPbCodec::decode(encoded, response);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(response.err_code, 404);
    ASSERT_EQ(response.err_info, "service not found");
}

TEST(TinyPbCodecTest, InvalidData)
{
    std::string invalid_data = "invalid data without proper format";
    wf_rpc::TinyPbStruct response;
    int ret = wf_rpc::TinyPbCodec::decode(invalid_data, response);
    ASSERT_NE(ret, 0);
}

TEST(TinyPbControllerTest, BasicOperations)
{
    wf_rpc::TinyPbRpcController controller;
    
    ASSERT_FALSE(controller.Failed());
    ASSERT_EQ(controller.ErrorText(), "");
    ASSERT_EQ(controller.GetTimeout(), 5000);
    
    controller.SetTimeout(10000);
    ASSERT_EQ(controller.GetTimeout(), 10000);
    
    controller.SetFailed("test error");
    ASSERT_TRUE(controller.Failed());
    ASSERT_EQ(controller.ErrorText(), "test error");
    
    controller.Reset();
    ASSERT_FALSE(controller.Failed());
    ASSERT_EQ(controller.ErrorText(), "");
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}