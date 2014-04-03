// Copyright 2009-2013 Toby Schneider (https://launchpad.net/~tes)
//                     Massachusetts Institute of Technology (2007-)
//                     Woods Hole Oceanographic Institution (2007-)
//                     DCCL Developers Team (https://launchpad.net/~dccl-dev)
// 
//
// This file is part of the Dynamic Compact Control Language Library
// ("DCCL").
//
// DCCL is free software: you can redistribute them and/or modify
// them under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// DCCL is distributed in the hope that they will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DCCL.  If not, see <http://www.gnu.org/licenses/>.


// tests all protobuf types with _default codecs, repeat and non repeat

#include <fstream>

#include <google/protobuf/descriptor.pb.h>

#include "dccl/codec.h"
#include "dccl/codecs2/field_codec_default.h"

#include "test.pb.h"
#include "dccl/binary.h"

template <typename Msg>
void check();
dccl::Codec codec;
TestMsg msg_in;
TestMsgGroup msg_group_in;

int main(int argc, char* argv[])
{
//    dccl::dlog.connect(dccl::logger::ALL, &std::cerr);
    
    dccl::FieldCodecManager::add<dccl::v2::DefaultNumericFieldCodec<double, double, dccl::v2::PRESENCE_BIT> >("dccl.presence_bit");
    dccl::FieldCodecManager::add<dccl::v2::DefaultEnumCodec<dccl::v2::PRESENCE_BIT> >("dccl.presence_bit");
    dccl::FieldCodecManager::add<dccl::v2::DefaultMessageCodec, google::protobuf::FieldDescriptor::TYPE_MESSAGE>("dccl.presence_bit");

    dccl::FieldCodecManager::add<dccl::v2::DefaultNumericFieldCodec<double> >("dccl.default-1");
    dccl::FieldCodecManager::add<dccl::v2::DefaultEnumCodec<> >("dccl.default-1");
    dccl::FieldCodecManager::add<dccl::v2::DefaultMessageCodec, google::protobuf::FieldDescriptor::TYPE_MESSAGE>("dccl.default-1");

    
    check<TestMsg>();
    check<TestMsgGroup>();
    check<TestMsgVersion>();
    
    std::cout << "all tests passed" << std::endl;
}


template <typename Msg>
void check()
{
    Msg msg_in;
    int i = 0;
    msg_in.set_d(++i + 0.1);
    msg_in.add_d_repeat(12.1);
    msg_in.add_d_repeat(12.2);
    msg_in.add_d_repeat(12.3);
    
    msg_in.mutable_msg()->set_val(++i + 0.3);
    msg_in.mutable_msg()->mutable_msg()->set_val(++i);
    codec.info(msg_in.GetDescriptor(), &std::cout);

    std::cout << "Message in:\n" << msg_in.DebugString() << std::endl;
     
    codec.load(msg_in.GetDescriptor());

    std::cout << "Try encode..." << std::endl;
    std::string bytes;
    codec.encode(&bytes, msg_in);
    std::cout << "... got bytes (hex): " << dccl::hex_encode(bytes) << std::endl;

    std::cout << "Try decode..." << std::endl;
    
    Msg msg_out;
    codec.decode(bytes, &msg_out);
    
    std::cout << "... got Message out:\n" << msg_out.DebugString() << std::endl;
    
    assert(msg_in.SerializeAsString() == msg_out.SerializeAsString());
}
