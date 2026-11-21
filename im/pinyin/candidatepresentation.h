/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef FCITX_PINYIN_CANDIDATEPRESENTATION_H
#define FCITX_PINYIN_CANDIDATEPRESENTATION_H

#include <fcitx-utils/key.h>
#include <fcitx/candidatelist.h>
#include <memory>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

namespace fcitx {

class SymbolCandidateWord;

// Keeps concrete original word types and tab state while placing symbols in
// the current viewport. Only the recreated symbol candidates are owned here.
class PinyinCandidatePresentation final : public CandidateList,
                                         public ModifiableCandidateList,
                                         public PageableCandidateList,
                                         public BulkCursorCandidateList,
                                         public CursorModifiableCandidateList,
                                         public CursorMovableCandidateList {
public:
    PinyinCandidatePresentation(std::shared_ptr<CommonCandidateList> original,
                                const KeyList &selectionKeys,
                                int configuredPageSize, int firstPageSize = 0);
    ~PinyinCandidatePresentation() override;

    const std::shared_ptr<CommonCandidateList> &original() const {
        return original_;
    }
    bool dirty() const;
    bool modified() const { return dirty_; }
    uint64_t generation() const;
    void refresh(bool resetPage = true);
    void filterOriginal(const std::function<bool(const CandidateWord &)> &filter = {});
    bool setViewport(int firstPageSize,
                     const std::vector<const CandidateWord *> &firstPage,
                     const CandidateWord *firstSymbolSource,
                     bool automaticSymbols);

    const Text &label(int index) const override;
    const CandidateWord &candidate(int index) const override;
    int size() const override;
    int cursorIndex() const override;
    CandidateLayoutHint layoutHint() const override;
    const CandidateWord &candidateFromAll(int index) const override;
    int totalSize() const override;

    bool hasPrev() const override;
    bool hasNext() const override;
    void prev() override;
    void next() override;
    bool usedNextBefore() const override;
    int totalPages() const override;
    int currentPage() const override;
    void setPage(int page) override;

    int globalCursorIndex() const override;
    void setGlobalCursorIndex(int index) override;
    void setCursorIndex(int index) override;
    void prevCandidate() override;
    void nextCandidate() override;

    void insert(int index, std::unique_ptr<CandidateWord> word) override;
    void remove(int index) override;
    void replace(int index, std::unique_ptr<CandidateWord> word) override;
    void move(int from, int to) override;

private:
    using ViewportEntry = std::pair<const CandidateWord *, bool>;
    struct WordIdentity {
        const CandidateWord *word = nullptr;
        std::string symbol;
    };
    struct RawWordState {
        const CandidateWord *word;
        std::string text;
        std::string comment;
        std::string sourceText;
        bool placeholder;
        std::vector<std::string> foldedSymbols;
        bool operator==(const RawWordState &) const = default;
    };
    RawWordState rawState(const CandidateWord *word) const;
    int pageStart(int page) const;
    int pageForIndex(int index) const;
    void moveCursor(bool previous);
    void invalidateViewport();
    size_t originalIndex(const CandidateWord *word) const;
    WordIdentity identity(const CandidateWord *word) const;

    std::shared_ptr<CommonCandidateList> original_;
    std::vector<const CandidateWord *> words_;
    // Page order may promote sparse first-row members without changing bulk
    // word order or their absolute native indices.
    std::vector<int> pageIndices_;
    std::vector<std::unique_ptr<SymbolCandidateWord>> symbols_;
    std::vector<std::pair<const CandidateWord *, const CandidateWord *>>
        symbolOrigins_;
    std::vector<Text> labels_;
    Text emptyLabel_;
    int configuredPageSize_;
    int requestedPageSize_ = 0;
    int pageSize_;
    int firstPageLength_ = 0;
    int page_ = 0;
    int cursor_ = -1;
    bool usedNext_ = false;
    bool dirty_ = false;
    bool automaticSymbols_ = true;
    const CandidateWord *firstSymbolSource_ = nullptr;
    std::vector<ViewportEntry> firstPage_;
    std::vector<RawWordState> rawState_;
    std::unordered_set<const CandidateWord *> removedWords_;
    mutable uint64_t generation_ = 0;
    mutable bool dirtyGenerationSeen_ = false;
};

} // namespace fcitx
#endif
