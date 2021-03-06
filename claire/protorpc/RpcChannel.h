// Copyright (c) 2013 The claire-protorpc Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors

#ifndef _CLAIRE_PROTORPC_RPCCHANNEL_H_
#define _CLAIRE_PROTORPC_RPCCHANNEL_H_

#include <google/protobuf/stubs/common.h>

#include <string>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

namespace google {
namespace protobuf {

class Descriptor;            // descriptor.h
class MethodDescriptor;      // descriptor.h
class ServiceDescriptor;     // descriptor.h

class Message;               // message.h
typedef ::boost::shared_ptr<Message> MessagePtr;

// When you upcast (that is, cast a pointer from type Foo to type
// SuperclassOfFoo), it's fine to use implicit_cast<>, since upcasts
// always succeed.  When you downcast (that is, cast a pointer from
// type Foo to type SubclassOfFoo), static_cast<> isn't safe, because
// how do you know the pointer is really of type SubclassOfFoo?  It
// could be a bare Foo, or of type DifferentSubclassOfFoo.  Thus,
// when you downcast, you should use this macro.  In debug mode, we
// use dynamic_cast<> to double-check the downcast is legal (we die
// if it's not).  In normal mode, we do the efficient static_cast<>
// instead.  Thus, it's important to test in debug mode to make sure
// the cast is legal!
//    This is the only place in the code we should use dynamic_cast<>.
// In particular, you SHOULDN'T be using dynamic_cast<> in order to
// do RTTI (eg code like this:
//    if (dynamic_cast<Subclass1>(foo)) HandleASubclass1Object(foo);
//    if (dynamic_cast<Subclass2>(foo)) HandleASubclass2Object(foo);
// You should design the code some other way not to need this.

template<typename To, typename From>     // use like this: down_pointer_cast<T>(foo);
inline ::boost::shared_ptr<To> down_pointer_cast(const ::boost::shared_ptr<From>& f)
{
    // so we only accept smart pointers
    // Ensures that To is a sub-type of From *.  This test is here only
    // for compile-time type checking, and has no overhead in an
    // optimized build at run-time, as it will be optimized away
    // completely.
    if (false)
    {
        implicit_cast<const From*, To*>(0);
    }

#if !defined(NDEBUG)
    assert(f == NULL || dynamic_cast<To*>(get_pointer(f)) != NULL);  // RTTI: debug mode only!
#endif
    return ::boost::static_pointer_cast<To>(f);
}

} // namespace protobuf
} // namespace google

namespace claire {

class EventLoop;
class InetAddress;
class RpcController;
typedef boost::shared_ptr<RpcController> RpcControllerPtr;

// Abstract interface for an RPC channel.  An RpcChannel represents a
// communication line to a Service which can be used to call that Service's
// methods.  The Service may be running on another machine.  Normally, you
// should not call an RpcChannel directly, but instead construct a stub Service
// wrapping it.  Example:
//   EventLoop loop;
//   RpcControllerPtr controller(new RpcController);
//   RpcChannel* channel = new RpcChannel(&loop);
//   channel->Connect("remotehost.example.com:1234");
//   MyService* service = new MyService::Stub(channel);
//   service->MyMethod(&controller, request, callback);
class RpcChannel : boost::noncopyable
{
public:
    struct Options
    {
        Options()
            : resolver_name("static"),
              loadbalancer_name("random")
        {}

        std::string resolver_name;
        std::string loadbalancer_name;
    };

    typedef boost::function<void (RpcControllerPtr&,
                                  const ::google::protobuf::MessagePtr&)> Callback;

    RpcChannel(EventLoop* loop);
    RpcChannel(EventLoop* loop, const Options& options);
    ~RpcChannel();

    void Connect(const std::string& server_address);
    void Connect(const InetAddress& server_address);
    void Shutdown();

    // Call the given method of the remote service.  The signature of this
    // procedure looks the same as Service::CallMethod(), but the requirements
    // are less strict in one important way:  the request and response objects
    // need not be of any specific class as long as their descriptors are
    // method->input_type() and method->output_type().
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                    RpcControllerPtr& controller,
                    const ::google::protobuf::Message& request,
                    const ::google::protobuf::Message* response_prototype,
                    const Callback& done);

    template<typename Output>
    static void downcastcall(const ::boost::function<void (RpcControllerPtr&, const boost::shared_ptr<Output>&)>& done,
                             RpcControllerPtr& controller,
                             const ::google::protobuf::MessagePtr& output)
    {
        done(controller, ::google::protobuf::down_pointer_cast<Output>(output));
    }

    template<typename Output>
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                    RpcControllerPtr& controller,
                    const ::google::protobuf::Message& request,
                    const ::google::protobuf::Message* response_prototype,
                    const boost::function<void (RpcControllerPtr&, const boost::shared_ptr<Output>&)>& done)
    {
        CallMethod(method,
                   controller,
                   request,
                   response_prototype,
                   boost::bind(&downcastcall<Output>, done, _1, _2));
    }

private:
    class Impl;
    boost::shared_ptr<Impl> impl_;
};

} // namespace claire

#endif  // _CLAIRE_PROTORPC_RPCCHANNEL_H_
