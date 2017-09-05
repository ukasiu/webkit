/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(REMOTE_INSPECTOR)

#include<mutex>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <plist/String.h>
#include <plist/Dictionary.h>
#include "webinspector.h"

namespace Inspector {

class RemoteInspectorTCPServer : public ThreadSafeRefCounted<RemoteInspectorTCPServer> {
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void processReceivedPList(PList::Dictionary * dict) = 0;
        virtual void clientConnectionDied() = 0;
    };

    RemoteInspectorTCPServer(Client*);
    virtual ~RemoteInspectorTCPServer();

    void close();
    void sendMessage(std::string messageName, PList::Dictionary & userInfo);

private:
    Lock m_mutex;

    int m_serverSockFd;
    int m_clientSockFd;
    Client* m_client;
    bool m_closed { false };
    wi_t m_wi;
};

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
