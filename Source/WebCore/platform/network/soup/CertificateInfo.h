/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#pragma once

#include "CertificateInfoBase.h"
#include "NotImplemented.h"
#include <libsoup/soup.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace WebCore {

class ResourceError;
class ResourceResponse;

class CertificateInfo  : public CertificateInfoBase {
public:
    CertificateInfo();
    explicit CertificateInfo(const WebCore::ResourceResponse&);
    explicit CertificateInfo(const WebCore::ResourceError&);
    explicit CertificateInfo(GTlsCertificate*, GTlsCertificateFlags);
    ~CertificateInfo();

    GTlsCertificate* certificate() const { return m_certificate.get(); }
    void setCertificate(GTlsCertificate* certificate) { m_certificate = certificate; }
    GTlsCertificateFlags tlsErrors() const { return m_tlsErrors; }
    void setTLSErrors(GTlsCertificateFlags tlsErrors) { m_tlsErrors = tlsErrors; }

    bool containsNonRootSHA1SignedCertificate() const { notImplemented(); return false; }

    std::optional<SummaryInfo> summaryInfo() const { notImplemented(); return std::nullopt; }

    bool isEmpty() const { return !m_certificate; }

private:
    GRefPtr<GTlsCertificate> m_certificate;
    GTlsCertificateFlags m_tlsErrors;
};

} // namespace WebCore

namespace WTF {
namespace Persistence {

template<> struct Coder<GRefPtr<GByteArray>> {
    static void encode(Encoder &encoder, const GRefPtr<GByteArray>& byteArray)
    {
        encoder << static_cast<uint32_t>(byteArray->len);
        encoder.encodeFixedLengthData(byteArray->data, byteArray->len);
    }

    static bool decode(Decoder &decoder, GRefPtr<GByteArray>& byteArray)
    {
        uint32_t size;
        if (!decoder.decode(size))
            return false;

        byteArray = adoptGRef(g_byte_array_sized_new(size));
        g_byte_array_set_size(byteArray.get(), size);
        return decoder.decodeFixedLengthData(byteArray->data, size);
    }
};

static Vector<GRefPtr<GByteArray>> certificatesDataListFromCertificateInfo(const WebCore::CertificateInfo &certificateInfo)
{
    auto* certificate = certificateInfo.certificate();
    if (!certificate)
        return { };

    Vector<GRefPtr<GByteArray>> certificatesDataList;
    for (; certificate; certificate = g_tls_certificate_get_issuer(certificate)) {
        GByteArray* certificateData = nullptr;
        g_object_get(G_OBJECT(certificate), "certificate", &certificateData, nullptr);

        if (!certificateData) {
            certificatesDataList.clear();
            break;
        }
        certificatesDataList.append(adoptGRef(certificateData));
    }

    // Reverse so that the list starts from the rootmost certificate.
    certificatesDataList.reverse();

    return certificatesDataList;
}

static GRefPtr<GTlsCertificate> certificateFromCertificatesDataList(const Vector<GRefPtr<GByteArray>> &certificatesDataList)
{
    GType certificateType = g_tls_backend_get_certificate_type(g_tls_backend_get_default());
    GRefPtr<GTlsCertificate> certificate;
    for (auto& certificateData : certificatesDataList) {
        certificate = adoptGRef(G_TLS_CERTIFICATE(g_initable_new(
            certificateType, nullptr, nullptr, "certificate", certificateData.get(), "issuer", certificate.get(), nullptr)));
        if (!certificate)
            break;
    }

    return certificate;
}

template<> struct Coder<WebCore::CertificateInfo> {
    static void encode(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
    {
        auto certificatesDataList = certificatesDataListFromCertificateInfo(certificateInfo);

        encoder << certificatesDataList;

        if (certificatesDataList.isEmpty())
            return;

        encoder << static_cast<uint32_t>(certificateInfo.tlsErrors());
    }

    static bool decode(Decoder& decoder, WebCore::CertificateInfo& certificateInfo)
    {
        Vector<GRefPtr<GByteArray>> certificatesDataList;
        if (!decoder.decode(certificatesDataList))
            return false;

        if (certificatesDataList.isEmpty())
            return true;
        auto certificate = certificateFromCertificatesDataList(certificatesDataList);
        if (!certificate)
            return false;
        certificateInfo.setCertificate(certificate.get());

        uint32_t tlsErrors;
        if (!decoder.decode(tlsErrors))
            return false;
        certificateInfo.setTLSErrors(static_cast<GTlsCertificateFlags>(tlsErrors));

        return true;
    }
};

} // namespace WTF::Persistence
} // namespace WTF
