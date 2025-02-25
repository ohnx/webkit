/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DictationCommandIOS.h"

#if PLATFORM(IOS_FAMILY)

#include "Document.h"
#include "DocumentMarkerController.h"
#include "Element.h"
#include "Position.h"
#include "Range.h"
#include "SmartReplace.h"
#include "TextIterator.h"
#include "VisibleUnits.h"

namespace WebCore {

DictationCommandIOS::DictationCommandIOS(Document& document, Vector<Vector<String>>&& dictationPhrases, RetainPtr<id> metadata)
    : CompositeEditCommand(document, EditAction::Dictation)
    , m_dictationPhrases(WTFMove(dictationPhrases))
    , m_metadata(WTFMove(metadata))
{
}

void DictationCommandIOS::doApply()
{
    VisiblePosition insertionPosition(startingSelection().visibleStart());

    unsigned resultLength = 0;
    for (auto& interpretations : m_dictationPhrases) {
        const String& firstInterpretation = interpretations[0];
        resultLength += firstInterpretation.length();
        inputText(firstInterpretation, true);

        if (interpretations.size() > 1)
            document().markers().addDictationPhraseWithAlternativesMarker(endingSelection().toNormalizedRange().get(), interpretations);

        setEndingSelection(VisibleSelection(endingSelection().visibleEnd()));
    }

    VisiblePosition afterResults(endingSelection().visibleEnd());

    Element* root = afterResults.rootEditableElement();

    // FIXME: Add the result marker using a Position cached before results are inserted, instead of relying on TextIterators.
    RefPtr<Range> rangeToEnd = Range::create(document(), createLegacyEditingPosition((Node *)root, 0), afterResults.deepEquivalent());
    int endIndex = TextIterator::rangeLength(rangeToEnd.get(), true);
    int startIndex = endIndex - resultLength;

    if (startIndex >= 0) {
        RefPtr<Range> resultRange = TextIterator::rangeFromLocationAndLength(document().documentElement(), startIndex, endIndex, true);
        document().markers().addDictationResultMarker(resultRange.get(), m_metadata);
    }
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
