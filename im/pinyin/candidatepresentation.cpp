/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "candidatepresentation.h"
#include "pinyincandidate.h"
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace fcitx {

PinyinCandidatePresentation::PinyinCandidatePresentation(
    std::shared_ptr<CommonCandidateList> original, const KeyList &selectionKeys,
    int configuredPageSize, int firstPageSize)
    : original_(std::move(original)),
      configuredPageSize_(configuredPageSize),
      requestedPageSize_(firstPageSize), pageSize_(configuredPageSize) {
    for (const auto &key : selectionKeys) {
        labels_.emplace_back(key.toString());
    }
    setBulk(this);
    setModifiable(this);
    setPageable(this);
    setBulkCursor(this);
    setCursorModifiable(this);
    setCursorMovable(this);
    setActionable(original_->toActionable());
    setTabbed(original_->toTabbed());
    refresh();
}

PinyinCandidatePresentation::~PinyinCandidatePresentation() = default;

PinyinCandidatePresentation::RawWordState
PinyinCandidatePresentation::rawState(const CandidateWord *word) const {
    const auto *symbol = dynamic_cast<const SymbolCandidateWord *>(word);
    return {word, word->text().toString(), word->comment().toString(),
            word->sourceCandidate() ? word->sourceCandidate()->text().toString() : std::string{},
            word->isPlaceHolder(), symbol ? symbol->foldedSymbols()
                                         : std::vector<std::string>{}};
}

bool PinyinCandidatePresentation::dirty() const {
    if (dirty_ || rawState_.size() != static_cast<size_t>(original_->totalSize()))
        return true;
    for (int index = 0; index < original_->totalSize(); ++index) {
        if (rawState_[index] != rawState(&original_->candidateFromAll(index)))
            return true;
    }
    return false;
}

uint64_t PinyinCandidatePresentation::generation() const {
    if (dirty() && !dirtyGenerationSeen_) {
        ++generation_;
        dirtyGenerationSeen_ = true;
    }
    return generation_;
}

void PinyinCandidatePresentation::filterOriginal(
    const std::function<bool(const CandidateWord &)> &filter) {
    if (removedWords_.empty() && !filter) {
        original_->clearFilter();
    } else {
        original_->setFilter([this, filter](const CandidateWord &word) {
            return !removedWords_.contains(&word) && (!filter || filter(word));
        });
    }
}

PinyinCandidatePresentation::WordIdentity
PinyinCandidatePresentation::identity(const CandidateWord *word) const {
    if (const auto *symbol = dynamic_cast<const SymbolCandidateWord *>(word)) {
        return {symbol->sourceCandidate(), symbol->text().toString()};
    }
    return {word, {}};
}

bool PinyinCandidatePresentation::setViewport(
    int firstPageSize, const std::vector<const CandidateWord *> &firstPage,
    const CandidateWord *firstSymbolSource, bool automaticSymbols) {
    std::vector<ViewportEntry> stable;
    for (const auto *word : firstPage) {
        const bool symbol = dynamic_cast<const SymbolCandidateWord *>(word);
        stable.emplace_back(symbol ? word->sourceCandidate() : word, symbol);
    }
    if (!dirty() && requestedPageSize_ == firstPageSize &&
        firstPage_ == stable && firstSymbolSource_ == firstSymbolSource &&
        automaticSymbols_ == automaticSymbols) {
        return false;
    }
    requestedPageSize_ = firstPageSize;
    firstPage_ = std::move(stable);
    firstSymbolSource_ = firstSymbolSource;
    automaticSymbols_ = automaticSymbols;
    refresh(false);
    return true;
}

void PinyinCandidatePresentation::refresh(bool resetPage) {
    const int oldPage = page_;
    const auto oldAnchor = !resetPage && size() > 0
                               ? identity(&candidate(0)) : WordIdentity{};
    const auto oldCursor = cursor_ >= 0 && cursor_ < totalSize()
                               ? identity(words_[cursor_])
                               : WordIdentity{};
    struct Symbol {
        const SymbolCandidateWord *original;
        std::string text;
    };
    struct Group {
        const CandidateWord *source;
        std::vector<Symbol> symbols;
    };
    std::vector<const CandidateWord *> ordinary;
    std::vector<Group> groups;
    std::unordered_map<const CandidateWord *, size_t> groupIndices;
    rawState_.clear();
    for (int index = 0; index < original_->totalSize(); ++index) {
        const auto *word = &original_->candidateFromAll(index);
        rawState_.push_back(rawState(word));
        const auto *symbol = dynamic_cast<const SymbolCandidateWord *>(word);
        if (!symbol || !symbol->sourceCandidate()) {
            ordinary.push_back(word);
            continue;
        }
        const auto *source = symbol->sourceCandidate();
        auto [position, added] = groupIndices.emplace(source, groups.size());
        if (added) groups.push_back({source, {}});
        auto &group = groups[position->second];
        group.symbols.push_back({symbol, symbol->text().toString()});
        for (const auto &text : symbol->foldedSymbols()) {
            group.symbols.push_back({symbol, text});
        }
    }

    pageSize_ = std::max(1, requestedPageSize_ > 0 ? requestedPageSize_
                                                : configuredPageSize_);
    if (!automaticSymbols_ &&
        std::none_of(firstPage_.begin(), firstPage_.end(), [&](const ViewportEntry &entry) {
            return entry.second ? groupIndices.contains(entry.first)
                                : std::find(ordinary.begin(), ordinary.end(), entry.first) != ordinary.end();
        })) {
        firstPage_.clear();
        firstSymbolSource_ = nullptr;
        automaticSymbols_ = true;
    }
    std::unordered_set<const CandidateWord *> frontSources;
    size_t explicitBoundary = 0;
    bool hasExplicitBoundary = false;
    if (!automaticSymbols_) {
        for (const auto &[word, symbol] : firstPage_) {
            if (!symbol) frontSources.insert(word);
            auto position = std::find(ordinary.begin(), ordinary.end(), word);
            if (position != ordinary.end()) {
                explicitBoundary = std::max(
                    explicitBoundary,
                    static_cast<size_t>(position - ordinary.begin()) + 1);
                hasExplicitBoundary = true;
            }
        }
    }
    words_.clear();
    symbols_.clear();
    symbolOrigins_.clear();
    std::vector<size_t> deferred;
    std::unordered_set<size_t> emitted;
    const auto appendGroup = [&](size_t groupIndex, bool folded, bool sourceComment) {
        const auto &group = groups[groupIndex];
        const size_t count = folded ? 1 : group.symbols.size();
        for (size_t index = 0; index < count; ++index) {
            const auto &symbol = group.symbols[index];
            auto copy = symbol.original->cloneSymbol(symbol.text);
            if (folded) {
                std::vector<std::string> rest;
                for (size_t next = 1; next < group.symbols.size(); ++next)
                    rest.push_back(group.symbols[next].text);
                copy->setFoldedSymbols(std::move(rest));
            } else {
                copy->setFoldedSymbols({});
                if (sourceComment)
                    copy->setSourceComment(group.source->text().toString());
            }
            symbolOrigins_.emplace_back(copy.get(), symbol.original);
            words_.push_back(copy.get());
            symbols_.push_back(std::move(copy));
        }
        emitted.insert(groupIndex);
    };
    bool frontFinished = false;
    bool firstSymbolPlaced = false;
    firstPageLength_ = 0;
    const auto finishFront = [&] {
        firstPageLength_ = static_cast<int>(words_.size());
        for (const auto index : deferred) appendGroup(index, false, true);
        deferred.clear();
        frontFinished = true;
    };
    for (size_t index = 0; index < ordinary.size(); ++index) {
        const auto *word = ordinary[index];
        words_.push_back(word);
        const bool onFront = automaticSymbols_
                                 ? !frontFinished
                                 : frontSources.contains(word);
        if (auto group = groupIndices.find(word); group != groupIndices.end()) {
            const bool fold = onFront && !firstSymbolPlaced &&
                              (automaticSymbols_
                                   ? static_cast<int>(words_.size()) < pageSize_
                                   : firstSymbolSource_ == word);
            if (fold) {
                appendGroup(group->second, true, false);
                firstSymbolPlaced = true;
            } else if (onFront) {
                deferred.push_back(group->second);
            } else {
                appendGroup(group->second, false, false);
            }
        }
        if (!frontFinished &&
            (automaticSymbols_
                 ? static_cast<int>(words_.size()) >= pageSize_
                 : hasExplicitBoundary && index + 1 >= explicitBoundary)) {
            finishFront();
        }
    }
    if (!frontFinished) finishFront();
    for (size_t index = 0; index < groups.size(); ++index) {
        if (!emitted.contains(index)) appendGroup(index, false, true);
    }
    // A configured capacity is an upper bound. A short first page must not
    // pull the deferred symbols back into it; later pages retain the capacity.
    pageIndices_.clear();
    std::vector<bool> used(words_.size(), false);
    if (automaticSymbols_) {
        firstPageLength_ = std::min(firstPageLength_, pageSize_);
        for (int index = 0; index < firstPageLength_; ++index) {
            pageIndices_.push_back(index);
            used[index] = true;
        }
    } else {
        for (const auto &[word, symbol] : firstPage_) {
            for (size_t index = 0; index < words_.size(); ++index) {
                const auto *candidate = words_[index];
                const bool match = symbol
                                       ? dynamic_cast<const SymbolCandidateWord *>(candidate) &&
                                             candidate->sourceCandidate() == word
                                       : candidate == word;
                if (match && !used[index]) {
                    pageIndices_.push_back(static_cast<int>(index));
                    used[index] = true;
                    break;
                }
            }
        }
        firstPageLength_ = static_cast<int>(pageIndices_.size());
    }
    for (size_t index = 0; index < words_.size(); ++index) {
        if (!used[index]) pageIndices_.push_back(static_cast<int>(index));
    }
    dirty_ = false;
    ++generation_;
    dirtyGenerationSeen_ = false;
    page_ = 0;
    cursor_ = words_.empty() ? -1 : pageIndices_.front();
    if (!resetPage && oldCursor.word) {
        for (size_t index = 0; index < words_.size(); ++index) {
            const auto current = identity(words_[index]);
            if (current.word == oldCursor.word &&
                current.symbol == oldCursor.symbol) {
                cursor_ = static_cast<int>(index);
                break;
            }
        }
    }
    if (!resetPage && oldPage > 0 && oldAnchor.word) {
        for (size_t index = 0; index < words_.size(); ++index) {
            const auto current = identity(words_[index]);
            if (current.word == oldAnchor.word && current.symbol == oldAnchor.symbol) {
                page_ = pageForIndex(static_cast<int>(index));
                break;
            }
        }
    }
}

int PinyinCandidatePresentation::pageStart(int page) const {
    return page == 0 ? 0 : firstPageLength_ + (page - 1) * pageSize_;
}
int PinyinCandidatePresentation::pageForIndex(int index) const {
    const auto position = std::find(pageIndices_.begin(), pageIndices_.end(), index);
    const int ordinal = static_cast<int>(position - pageIndices_.begin());
    return ordinal < firstPageLength_ ? 0 : 1 + (ordinal - firstPageLength_) / pageSize_;
}
const Text &PinyinCandidatePresentation::label(int index) const {
    const auto &word = candidate(index);
    return word.hasCustomLabel() ? word.customLabel()
                                : index < static_cast<int>(labels_.size())
                                      ? labels_[index] : emptyLabel_;
}
const CandidateWord &PinyinCandidatePresentation::candidate(int index) const {
    if (index < 0 || index >= size())
        throw std::invalid_argument("Pinyin candidate page index");
    return candidateFromAll(pageIndices_[pageStart(page_) + index]);
}
int PinyinCandidatePresentation::size() const {
    return std::max(0, std::min(page_ == 0 ? firstPageLength_ : pageSize_,
                                totalSize() - pageStart(page_)));
}
int PinyinCandidatePresentation::cursorIndex() const {
    const int begin = pageStart(page_);
    for (int index = 0; index < size(); ++index) {
        if (pageIndices_[begin + index] == cursor_) return index;
    }
    return -1;
}
CandidateLayoutHint PinyinCandidatePresentation::layoutHint() const {
    return original_->layoutHint();
}
const CandidateWord &PinyinCandidatePresentation::candidateFromAll(int index) const {
    if (index < 0 || index >= totalSize())
        throw std::invalid_argument("Pinyin candidate global index");
    return *words_[index];
}
int PinyinCandidatePresentation::totalSize() const { return words_.size(); }
bool PinyinCandidatePresentation::hasPrev() const { return page_ > 0; }
bool PinyinCandidatePresentation::hasNext() const { return page_ + 1 < totalPages(); }
void PinyinCandidatePresentation::prev() { if (hasPrev()) setPage(page_ - 1); }
void PinyinCandidatePresentation::next() {
    if (hasNext()) { usedNext_ = true; setPage(page_ + 1); }
}
bool PinyinCandidatePresentation::usedNextBefore() const { return usedNext_; }
int PinyinCandidatePresentation::totalPages() const {
    const int remaining = totalSize() - firstPageLength_;
    return totalSize() == 0 ? 0 : 1 + std::max(0, (remaining + pageSize_ - 1) / pageSize_);
}
int PinyinCandidatePresentation::currentPage() const { return page_; }
void PinyinCandidatePresentation::setPage(int page) {
    if (page < 0 || page >= totalPages()) return;
    page_ = page;
    if (cursor_ >= 0 && size() > 0) cursor_ = pageIndices_[pageStart(page_)];
}
int PinyinCandidatePresentation::globalCursorIndex() const { return cursor_; }
void PinyinCandidatePresentation::setGlobalCursorIndex(int index) {
    if (index < 0) { cursor_ = -1; return; }
    candidateFromAll(index);
    cursor_ = index;
}
void PinyinCandidatePresentation::setCursorIndex(int index) {
    candidate(index);
    cursor_ = pageIndices_[pageStart(page_) + index];
}
void PinyinCandidatePresentation::moveCursor(bool previous) {
    if (empty()) return;
    const auto current = std::find(pageIndices_.begin(), pageIndices_.end(), cursor_);
    int ordinal = cursorIndex() < 0
                      ? pageStart(page_) + (previous ? size() - 1 : 0)
                      : (static_cast<int>(current - pageIndices_.begin()) +
                         (previous ? -1 : 1) + totalSize()) % totalSize();
    const int first = ordinal;
    while (words_[pageIndices_[ordinal]]->isPlaceHolder()) {
        ordinal = (ordinal + (previous ? -1 : 1) + totalSize()) % totalSize();
        if (ordinal == first) return;
    }
    cursor_ = pageIndices_[ordinal];
    page_ = pageForIndex(cursor_);
}
void PinyinCandidatePresentation::prevCandidate() { moveCursor(true); }
void PinyinCandidatePresentation::nextCandidate() { moveCursor(false); }

size_t PinyinCandidatePresentation::originalIndex(const CandidateWord *word) const {
    for (const auto &[copy, original] : symbolOrigins_) {
        if (copy == word) { word = original; break; }
    }
    for (size_t index = 0; index < original_->originSize(); ++index) {
        if (&original_->originCandidate(index) == word) return index;
    }
    throw std::invalid_argument("Pinyin original candidate index");
}
void PinyinCandidatePresentation::invalidateViewport() {
    firstPage_.clear();
    firstSymbolSource_ = nullptr;
    automaticSymbols_ = true;
    dirty_ = true;
}
void PinyinCandidatePresentation::insert(int index, std::unique_ptr<CandidateWord> word) {
    if (index < 0 || index > totalSize()) throw std::invalid_argument("Pinyin insertion index");
    const auto origin = index == totalSize() ? original_->originSize() : originalIndex(words_[index]);
    const auto *pointer = word.get();
    original_->insert(origin, std::move(word));
    words_.insert(words_.begin() + index, pointer);
    invalidateViewport();
}
void PinyinCandidatePresentation::remove(int index) {
    const auto *word = &candidateFromAll(index);
    bool hasChildren = false;
    for (size_t origin = 0; origin < original_->originSize(); ++origin)
        hasChildren |= original_->originCandidate(origin).sourceCandidate() == word;
    words_.erase(words_.begin() + index);
    if (hasChildren) {
        // Keep the source object alive for its generated candidates. The visible
        // removal is still exactly one slot, as chained cloud updates require.
        removedWords_.insert(word);
    } else {
        original_->remove(static_cast<int>(originalIndex(word)));
    }
    invalidateViewport();
}
void PinyinCandidatePresentation::replace(int index, std::unique_ptr<CandidateWord> word) {
    remove(index);
    insert(index, std::move(word));
}
void PinyinCandidatePresentation::move(int from, int to) {
    const auto *word = &candidateFromAll(from);
    const auto originFrom = originalIndex(word);
    const auto originTo = originalIndex(&candidateFromAll(to));
    original_->move(originFrom, originTo);
    words_.erase(words_.begin() + from);
    words_.insert(words_.begin() + to, word);
    invalidateViewport();
}

} // namespace fcitx
