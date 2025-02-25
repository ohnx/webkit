/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Location.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "NavigationScheduler.h"
#include <wtf/URL.h>
#include "SecurityOrigin.h"

namespace WebCore {

Location::Location(DOMWindow& window)
    : DOMWindowProperty(&window)
{
}

inline const URL& Location::url() const
{
    if (!frame())
        return WTF::blankURL();

    const URL& url = frame()->document()->url();
    if (!url.isValid())
        return WTF::blankURL(); // Use "about:blank" while the page is still loading (before we have a frame).

    return url;
}

String Location::href() const
{
    auto& url = this->url();

    if (!url.hasUsername() && !url.hasPassword())
        return url.string();

    URL urlWithoutCredentials(url);
    urlWithoutCredentials.setUser(WTF::emptyString());
    urlWithoutCredentials.setPass(WTF::emptyString());
    return urlWithoutCredentials.string();
}

String Location::protocol() const
{
    return makeString(url().protocol(), ":");
}

String Location::host() const
{
    // Note: this is the IE spec. The NS spec swaps the two, it says
    // "The hostname property is the concatenation of the host and port properties, separated by a colon."
    return url().hostAndPort();
}

String Location::hostname() const
{
    return url().host().toString();
}

String Location::port() const
{
    const URL& url = this->url();
    return url.port() ? String::number(url.port().value()) : emptyString();
}

String Location::pathname() const
{
    const URL& url = this->url();
    return url.path().isEmpty() ? "/" : url.path();
}

String Location::search() const
{
    const URL& url = this->url();
    return url.query().isEmpty() ? emptyString() : "?" + url.query();
}

String Location::origin() const
{
    return SecurityOrigin::create(url())->toString();
}

Ref<DOMStringList> Location::ancestorOrigins() const
{
    auto origins = DOMStringList::create();
    auto* frame = this->frame();
    if (!frame)
        return origins;
    for (auto* ancestor = frame->tree().parent(); ancestor; ancestor = ancestor->tree().parent())
        origins->append(ancestor->document()->securityOrigin().toString());
    return origins;
}

String Location::hash() const
{
    const String& fragmentIdentifier = url().fragmentIdentifier();
    return fragmentIdentifier.isEmpty() ? emptyString() : "#" + fragmentIdentifier;
}

ExceptionOr<void> Location::setHref(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& url)
{
    if (!frame())
        return { };
    return setLocation(activeWindow, firstWindow, url);
}

ExceptionOr<void> Location::setProtocol(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& protocol)
{
    auto* frame = this->frame();
    if (!frame)
        return { };
    URL url = frame->document()->url();
    if (!url.setProtocol(protocol))
        return Exception { SyntaxError };
    return setLocation(activeWindow, firstWindow, url.string());
}

ExceptionOr<void> Location::setHost(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& host)
{
    auto* frame = this->frame();
    if (!frame)
        return { };
    URL url = frame->document()->url();
    url.setHostAndPort(host);
    return setLocation(activeWindow, firstWindow, url.string());
}

ExceptionOr<void> Location::setHostname(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& hostname)
{
    auto* frame = this->frame();
    if (!frame)
        return { };
    URL url = frame->document()->url();
    url.setHost(hostname);
    return setLocation(activeWindow, firstWindow, url.string());
}

ExceptionOr<void> Location::setPort(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& portString)
{
    auto* frame = this->frame();
    if (!frame)
        return { };
    URL url = frame->document()->url();
    int port = portString.toInt();
    if (port < 0 || port > 0xFFFF || portString.isEmpty())
        url.removePort();
    else
        url.setPort(port);
    return setLocation(activeWindow, firstWindow, url.string());
}

ExceptionOr<void> Location::setPathname(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& pathname)
{
    auto* frame = this->frame();
    if (!frame)
        return { };
    URL url = frame->document()->url();
    url.setPath(pathname);
    return setLocation(activeWindow, firstWindow, url.string());
}

ExceptionOr<void> Location::setSearch(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& search)
{
    auto* frame = this->frame();
    if (!frame)
        return { };
    URL url = frame->document()->url();
    url.setQuery(search);
    return setLocation(activeWindow, firstWindow, url.string());
}

ExceptionOr<void> Location::setHash(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& hash)
{
    auto* frame = this->frame();
    if (!frame)
        return { };
    ASSERT(frame->document());
    auto url = frame->document()->url();
    auto oldFragmentIdentifier = url.fragmentIdentifier();
    auto newFragmentIdentifier = hash;
    if (hash[0] == '#')
        newFragmentIdentifier = hash.substring(1);
    url.setFragmentIdentifier(newFragmentIdentifier);
    // Note that by parsing the URL and *then* comparing fragments, we are 
    // comparing fragments post-canonicalization, and so this handles the 
    // cases where fragment identifiers are ignored or invalid. 
    if (equalIgnoringNullity(oldFragmentIdentifier, url.fragmentIdentifier()))
        return { };
    return setLocation(activeWindow, firstWindow, url.string());
}

ExceptionOr<void> Location::assign(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& url)
{
    if (!frame())
        return { };
    return setLocation(activeWindow, firstWindow, url);
}

void Location::replace(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& url)
{
    auto* frame = this->frame();
    if (!frame)
        return;
    ASSERT(frame->document());
    ASSERT(frame->document()->domWindow());
    // We call DOMWindow::setLocation directly here because replace() always operates on the current frame.
    frame->document()->domWindow()->setLocation(activeWindow, firstWindow, url, LockHistoryAndBackForwardList);
}

void Location::reload(DOMWindow& activeWindow)
{
    auto* frame = this->frame();
    if (!frame)
        return;

    ASSERT(activeWindow.document());
    ASSERT(frame->document());
    ASSERT(frame->document()->domWindow());

    auto& activeDocument = *activeWindow.document();
    auto& targetDocument = *frame->document();

    // FIXME: It's not clear this cross-origin security check is valuable.
    // We allow one page to change the location of another. Why block attempts to reload?
    // Other location operations simply block use of JavaScript URLs cross origin.
    if (!activeDocument.securityOrigin().canAccess(targetDocument.securityOrigin())) {
        auto& targetWindow = *targetDocument.domWindow();
        targetWindow.printErrorMessage(targetWindow.crossDomainAccessErrorMessage(activeWindow, IncludeTargetOrigin::Yes));
        return;
    }

    if (WTF::protocolIsJavaScript(targetDocument.url()))
        return;

    frame->navigationScheduler().scheduleRefresh(activeDocument);
}

ExceptionOr<void> Location::setLocation(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& url)
{
    auto* frame = this->frame();
    ASSERT(frame);
    if (!activeWindow.document()->canNavigate(frame))
        return Exception { SecurityError };
    ASSERT(frame->document());
    ASSERT(frame->document()->domWindow());
    frame->document()->domWindow()->setLocation(activeWindow, firstWindow, url);
    return { };
}

} // namespace WebCore
