#pragma once
#include <vector>
#include <optional>
namespace MenuDetail {
/** CMenuStack::SetMenu/Update :147654/:147889 separate pending from active.
 * Page numbers are desktop routing IDs, not BIG resource handles. */
class CMenuStack {
public:
    enum class Operation { Push, Root, Pop };
    struct Request { unsigned target; Operation operation; };
    void Push(unsigned target, bool root) {
        Operation operation = Operation::Push;
        if (root) { operation = Operation::Root; }
        pending = Request{target, operation};
    }
    void Pop() {
        unsigned target = 0;
        if (!history.empty()) { target = history.back(); }
        pending = Request{target, Operation::Pop};
    }
    bool HasPending() const { return pending.has_value(); }
    const Request &Pending() const { return pending.value(); }
    bool Commit(bool busy) {
        if (!pending || busy) { return false; }
        const Request request = *pending;
        pending.reset();
        if (request.operation == Operation::Root) { history.clear(); }
        if (request.operation == Operation::Pop) {
            if (!history.empty()) { history.pop_back(); }
        } else if (request.operation == Operation::Push && request.target != page && page != 14) {
            history.push_back(page);
        }
        page = request.target;
        return true;
    }
    unsigned page = 0;
    std::vector<unsigned> history;
private:
    std::optional<Request> pending;
};
}
