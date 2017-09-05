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

#include "config.h"
#include "RemoteInspectorTCPServer.h"

#if ENABLE(REMOTE_INSPECTOR)

#include <sys/socket.h>
#include <netinet/in.h>
#include <inspector/remote/RemoteInspector.h>
#include <unistd.h>

#define RECV_BUFFER_SIZE 4096

namespace Inspector {

// Constants private to this file for message serialization on both ends.
#define RemoteInspectorTCPConnectionMessageNameKey "messageName"
#define RemoteInspectorTCPConnectionSerializedMessageKey "msgData"

    RemoteInspectorTCPServer::RemoteInspectorTCPServer(Client *client)
            : m_client(client) {
        // TODO error handling
        m_wi = wi_new(false);
        m_wi->recv_plist = RemoteInspector::receivedPList;
        m_wi->context = client;
        m_clientSockFd = 0;

        sockaddr_in serverAddress;
       m_serverSockFd = socket(AF_INET, SOCK_STREAM, 0);
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        serverAddress.sin_port = htons(9123);

        bind(m_serverSockFd, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
        listen(m_serverSockFd, 5);

        std::thread([&]() {
            sockaddr_in clientAddress;
            char *recv_buffer = static_cast<char *>(malloc(RECV_BUFFER_SIZE));
            while (true) {
                socklen_t socketSize = sizeof(clientAddress);
                m_clientSockFd = accept(m_serverSockFd, (struct sockaddr *) &clientAddress, &socketSize);

                while (true) {
                    ssize_t len = recv(m_clientSockFd, recv_buffer, RECV_BUFFER_SIZE, NULL);
                    if (len == 0) {
                        m_client->clientConnectionDied();
                        ::close(m_clientSockFd);
                        m_clientSockFd = 0;
                        break;
                    } else {
                        m_wi->on_recv(m_wi, recv_buffer, len);
                    }
                }
            }
        }).detach();
    }

    RemoteInspectorTCPServer::~RemoteInspectorTCPServer() {
        ASSERT(!m_client);
        ASSERT(!m_serverSockFd);
        ASSERT(m_closed);
    }

    void RemoteInspectorTCPServer::close() {
        if(m_serverSockFd != 0)
            ::close(m_serverSockFd);
    }

    void RemoteInspectorTCPServer::sendMessage(std::string messageName, PList::Dictionary &userInfo) {
        ASSERT(!m_closed);
        if (m_closed)
            return;
        if (m_clientSockFd == 0) {
            return;
        }

        PList::Dictionary dictionary = PList::Dictionary();
        dictionary.Set(RemoteInspectorTCPConnectionMessageNameKey, PList::String(messageName));
        dictionary.Set(RemoteInspectorTCPConnectionSerializedMessageKey, userInfo);

        char *rpc_bin = NULL;
        uint32_t rpc_len = 0;
        plist_t dict = dictionary.GetPlist();
        plist_to_bin(dict, &rpc_bin, &rpc_len);

        wi_status ret = WI_ERROR;

        size_t length = rpc_len + 4;
        char *out_head = (char *) malloc(length * sizeof(char));
        if (!out_head) {
            return;
        }
        char *out_tail = out_head;

        // write big-endian int
        *out_tail++ = ((rpc_len >> 24) & 0xFF);
        *out_tail++ = ((rpc_len >> 16) & 0xFF);
        *out_tail++ = ((rpc_len >> 8) & 0xFF);
        *out_tail++ = (rpc_len & 0xFF);

        // write data
        memcpy(out_tail, rpc_bin, rpc_len);

        send(m_clientSockFd, out_head, length, NULL);
        free(out_head);
    }

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
