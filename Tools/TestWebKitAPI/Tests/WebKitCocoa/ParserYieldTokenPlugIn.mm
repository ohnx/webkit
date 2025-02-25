/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "ParserYieldTokenTests.h"
#import <WebKit/WKDOMDocument.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInLoadDelegate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

static bool willStartProvisionalLoadForFrameCalled = false;

@interface ParserYieldTokenPlugIn : NSObject <WKWebProcessPlugIn, WKWebProcessPlugInLoadDelegate, ParserYieldTokenTestBundle>
@end

@implementation ParserYieldTokenPlugIn {
    RetainPtr<NSMutableArray<NSObject *>> _parserYieldTokens;
    RetainPtr<id<ParserYieldTokenTestRunner>> _testRunner;
    RetainPtr<WKWebProcessPlugInBrowserContextController> _controller;
    BOOL _loadCommitted;
    NSUInteger _numberOfTokensToTakeAfterComittingLoad;
}

- (void)takeDocumentParserTokenAfterCommittingLoad
{
    if (_loadCommitted)
        [_parserYieldTokens addObject:[_controller mainFrameDocument].parserYieldToken];
    else
        ++_numberOfTokensToTakeAfterComittingLoad;
}

- (void)releaseDocumentParserToken
{
    if (_loadCommitted)
        [_parserYieldTokens removeObjectAtIndex:0];
    else
        --_numberOfTokensToTakeAfterComittingLoad;
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController*)controller willStartProvisionalLoadForFrame:(WKWebProcessPlugInFrame *)frame completionHandler:(void(^)(void))completionHandler
{
    willStartProvisionalLoadForFrameCalled = true;
    completionHandler();
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didCommitLoadForFrame:(WKWebProcessPlugInFrame *)frame
{
    ASSERT(willStartProvisionalLoadForFrameCalled);

    _loadCommitted = YES;
    while (_numberOfTokensToTakeAfterComittingLoad) {
        [self takeDocumentParserTokenAfterCommittingLoad];
        --_numberOfTokensToTakeAfterComittingLoad;
    }
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    _parserYieldTokens = adoptNS([NSMutableArray new]);
    _controller = browserContextController;
    browserContextController.loadDelegate = self;
    [browserContextController._remoteObjectRegistry registerExportedObject:self interface:[_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ParserYieldTokenTestBundle)]];
    _testRunner = [browserContextController._remoteObjectRegistry remoteObjectProxyWithInterface:[_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ParserYieldTokenTestRunner)]];
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController*)controller didFinishDocumentLoadForFrame:(WKWebProcessPlugInFrame *)frame
{
    [_testRunner didFinishDocumentLoad];
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController*)controller didFinishLoadForFrame:(WKWebProcessPlugInFrame *)frame
{
    [_testRunner didFinishLoad];
}

@end

#endif // WK_API_ENABLED

