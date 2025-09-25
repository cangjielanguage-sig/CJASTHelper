#include <functional>

// ====== 函数与回调 ======
template <typename Signature> using Function = std::function<Signature>;

using Callback = std::function<void()>;
template <typename T> using Predicate = std::function<bool(const T&)>;