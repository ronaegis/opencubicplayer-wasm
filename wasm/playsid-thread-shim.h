/*
 * Thread shim for WASM - provides sequential execution instead of threading
 * This header is automatically included before ReSIDfp files to avoid thread
 * creation which requires SharedArrayBuffer in browsers.
 *
 * We redefine std::thread to execute functions immediately in a sequential manner.
 */

#ifndef PLAYSID_THREAD_SHIM_H
#define PLAYSID_THREAD_SHIM_H

// Only apply to C++ files
#if defined(__EMSCRIPTEN__) && defined(__cplusplus)

// Prevent the real <thread> from being included by defining its include guard
#define _LIBCPP_THREAD
#define _LIBCPP___THREAD_THREAD_H

// Include utility for std::forward
#include <utility>

namespace std {
    // Sequential thread implementation that executes immediately
    class thread {
    public:
        // Default constructor
        thread() noexcept = default;

        // Constructor that takes a callable and executes it immediately
        template<typename Function, typename... Args>
        explicit thread(Function&& f, Args&&... args) {
            // Execute the function immediately in the current thread
            // We need to handle both lambdas and regular functions
            invoke_impl(std::forward<Function>(f), std::forward<Args>(args)...);
        }

        // Destructor
        ~thread() = default;

        // join() is a no-op since execution already completed
        void join() noexcept {}

        // Make it non-copyable like real thread
        thread(const thread&) = delete;
        thread& operator=(const thread&) = delete;

        // Allow move
        thread(thread&&) noexcept = default;
        thread& operator=(thread&&) noexcept = default;

    private:
        // Helper to invoke the function with arguments
        template<typename Function, typename... Args>
        void invoke_impl(Function&& f, Args&&... args) {
            f(std::forward<Args>(args)...);
        }
    };

    // Also provide jthread for C++20 compatibility (same as thread but auto-joins)
    #if defined(__cpp_lib_jthread) || defined(HAVE_CXX20)
    using jthread = thread;  // In our case, both are the same
    #endif
}

#endif // __EMSCRIPTEN__ && __cplusplus

#endif // PLAYSID_THREAD_SHIM_H
