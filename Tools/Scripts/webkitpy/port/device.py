# Copyright (C) 2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import socket

class Device(object):
    def __init__(self, platform_device):
        self.platform_device = platform_device
        self.listening_socket = None

    def listening_port(self):
        if not self.listening_socket:
            return None
        return self.listening_socket.getsockname()[1]

    def install_app(self, app_path, env=None):
        return self.platform_device.install_app(app_path, env)

    def launch_app(self, bundle_id, args, env=None):
        return self.platform_device.launch_app(bundle_id, args, env)

    def prepare_for_testing(self):
        if not self.listening_socket:
            self.listening_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.listening_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.listening_socket.bind(('127.0.0.1', 0))

        if hasattr(self.platform_device, 'prepare_for_testing'):
            self.platform_device.prepare_for_testing()

    def finished_testing(self):
        if hasattr(self.platform_device, 'teardown'):
            self.platform_device.finished_testing()

        self.listening_socket = None

    @property
    def executive(self):
        return self.platform_device.executive

    @property
    def filesystem(self):
        return self.platform_device.filesystem

    @property
    def user(self):
        return self.platform_device.user

    @property
    def platform(self):
        return self.platform_device.platform

    @property
    def workspace(self):
        return self.platform_device.workspace

    @property
    def udid(self):
        return self.platform_device.udid

    def __nonzero__(self):
        return self.platform_device is not None

    def __eq__(self, other):
        return self.udid == other.udid

    def __ne__(self, other):
        return not self.__eq__(other)

    def __repr__(self):
        return str(self.platform_device)
