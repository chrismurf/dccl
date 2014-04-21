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



#include "dccl/codec.h"
#include "dccl/codecs3/field_codec_default_message.h"

using dccl::dlog;

//
// DefaultMessageCodec
//

void dccl::v3::DefaultMessageCodec::any_encode(Bitset* bits, const boost::any& wire_value)
{    
    if(wire_value.empty())
    {
        *bits = Bitset(min_size());
    }
    else
    {
        *bits = traverse_const_message<Encoder, Bitset>(wire_value);
        
        if(is_optional())
            bits->push_front(true); // presence bit
        
    }  
}
  

 
unsigned dccl::v3::DefaultMessageCodec::any_size(const boost::any& wire_value)
{
    if(wire_value.empty())
    {
        return min_size();
    }
    else
    {
        unsigned size = traverse_const_message<Size, unsigned>(wire_value);
        if(is_optional())
        {
            const unsigned presence_bit = 1;
            size += presence_bit;
        }
        
        return size;
    }
}


void dccl::v3::DefaultMessageCodec::any_decode(Bitset* bits, boost::any* wire_value)
{
    try
    {
        
        google::protobuf::Message* msg = boost::any_cast<google::protobuf::Message* >(*wire_value);

        if(is_optional())      
        {
            if(!bits->to_ulong())
            {
                *wire_value = boost::any();
                return;
            }
            else
            {
                bits->pop_front(); // presence bit
            }
        }        
        
        const google::protobuf::Descriptor* desc = msg->GetDescriptor();
        const google::protobuf::Reflection* refl = msg->GetReflection();
        
        for(int i = 0, n = desc->field_count(); i < n; ++i)
        {
        
            const google::protobuf::FieldDescriptor* field_desc = desc->field(i);

            if(!check_field(field_desc))
                continue;

            
            boost::shared_ptr<FieldCodecBase> codec = find(field_desc);            
            boost::shared_ptr<FromProtoCppTypeBase> helper =
                TypeHelper::find(field_desc);

            if(field_desc->is_repeated())
            {   
                std::vector<boost::any> field_values;
                if(field_desc->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE)
                {
                    unsigned max_repeat = field_desc->options().GetExtension(dccl::field).max_repeat();
                    for(unsigned j = 0, m = max_repeat; j < m; ++j)
                        field_values.push_back(refl->AddMessage(msg, field_desc));

                    codec->field_decode_repeated(bits, &field_values, field_desc);

                    // remove the unused messages
                    for(int j = field_values.size(), m = max_repeat; j < m; ++j)
                    {
                        refl->RemoveLast(msg, field_desc);
                    }
                }
                else
                {
                    // for primitive types
                    codec->field_decode_repeated(bits, &field_values, field_desc);
                    for(int j = 0, m = field_values.size(); j < m; ++j)
                        helper->add_value(field_desc, msg, field_values[j]);
                }
            }
            else
            {
                boost::any field_value;
                if(field_desc->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE)
                {
                    // allows us to propagate pointers instead of making many copies of entire messages
                    field_value = refl->MutableMessage(msg, field_desc);
                    codec->field_decode(bits, &field_value, field_desc);
                    if(field_value.empty()) refl->ClearField(msg, field_desc);    
                }
                else
                {
                    // for primitive types
                    codec->field_decode(bits, &field_value, field_desc);
                    helper->set_value(field_desc, msg, field_value);
                }
            } 
        }

        std::vector< const google::protobuf::FieldDescriptor* > set_fields;
        refl->ListFields(*msg, &set_fields);
        if(set_fields.empty() && this_field()) *wire_value = boost::any();
        else *wire_value = msg;
    }
    catch(boost::bad_any_cast& e)
    {
        std::cout << (this_field() ? this_field()->DebugString() : "0") << std::endl;
        throw(Exception("Bad type given to traverse mutable, expecting google::protobuf::Message*, got " + std::string(wire_value->type().name())));
    }

}


unsigned dccl::v3::DefaultMessageCodec::max_size()
{
    unsigned u = 0;
    traverse_descriptor<MaxSize>(&u);

    if(is_optional())
    {
        const unsigned presence_bit = 1;
        u += presence_bit;
    }
    
    return u;
}

unsigned dccl::v3::DefaultMessageCodec::min_size()
{
    if(is_optional())
    {
        const unsigned presence_bit = 1;
        return presence_bit;
    }
    else
    {
        unsigned u = 0;
        traverse_descriptor<MinSize>(&u);
        return u;
    }
}


void dccl::v3::DefaultMessageCodec::validate()
{
    bool b = false;
    traverse_descriptor<Validate>(&b);
}

std::string dccl::v3::DefaultMessageCodec::info()
{
    std::stringstream ss;
    traverse_descriptor<Info>(&ss);
    return ss.str();
}

bool dccl::v3::DefaultMessageCodec::check_field(const google::protobuf::FieldDescriptor* field)
{
    if(!field)
    {
        return true;
    }
    else
    {
        dccl::DCCLFieldOptions dccl_field_options = field->options().GetExtension(dccl::field);
        if(dccl_field_options.omit()) // omit
        {
            return false;
        }
        else if(MessageStack::current_part() == MessageStack::UNKNOWN) // part not yet explicitly specified
        {
            if((part() == MessageStack::HEAD && !dccl_field_options.in_head())
               || (part() == MessageStack::BODY && dccl_field_options.in_head()))
                return false;
            else
                return true;
        }
        else if(MessageStack::current_part() != part()) // part specified and doesn't match
            return false;
        else
            return true;
    }    
}


