/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if WK_API_ENABLED

#import "RenderedImageWithOptionsProtocol.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInLoadDelegate.h>
#import <WebKit/WKWebProcessPlugInNodeHandle.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

@interface RenderedImageWithOptionsPlugIn : NSObject <WKWebProcessPlugIn, WKWebProcessPlugInLoadDelegate>
@end

@implementation RenderedImageWithOptionsPlugIn {
    RetainPtr<id <RenderedImageWithOptionsProtocol>> _remoteObject;
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    browserContextController.loadDelegate = self;

    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(RenderedImageWithOptionsProtocol)];
    _remoteObject = [browserContextController._remoteObjectRegistry remoteObjectProxyWithInterface:interface];
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didFinishLoadForFrame:(WKWebProcessPlugInFrame *)frame
{
    JSContext *context = [frame jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]];
    JSValue *containerValue = [context evaluateScript:@"document.querySelector('.container')"];
    WKWebProcessPlugInNodeHandle *containerNode = [WKWebProcessPlugInNodeHandle nodeHandleWithJSValue:containerValue inContext:context];
#if PLATFORM(IOS)
    UIImage *image = [containerNode renderedImageWithOptions:kWKSnapshotOptionsExcludeOverflow];
#elif PLATFORM(MAC)
    NSImage *image = [containerNode renderedImageWithOptions:kWKSnapshotOptionsExcludeOverflow];
#endif
    [_remoteObject didRenderImageWithSize:image.size];
}

@end

#endif // WK_API_ENABLED
