/******************************************************************************
 Copyright (c) 2019, Lukas Bagaric
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 - Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************

This file defines a single template, spsc_queue, which implements a bounded
queue with at most one producer, and one consumer at the same time.

spsc_queue is intended to be used in environments, where heap-allocation must
never occur. While it is possible to use spsc_queue in real-time environments,
the implementation trades a worse worst-case for a significantly better
average-case.

spsc_queue has highest throughput under contention if:
  * you have small (register sized) elements OR
  * if the total size of the queue (size of element times number of elements)
    will not exceed the size of your processors fastest cache.

spsc_queue takes up to three template parameters:
  * T: The type of a single element
  * queue_size: The number of slots for elements within the queue.
                Note: Due to implementation details, one slot is reserved and
                      cannot be used.
  * align_log2: The number of bytes to align on, expressed as an exponent for
                two, so the actual alignment is (1 << align_log2) bytes. This
                number should be at least log2(alignof(size_t)). Ideal values
                avoid destructive hardware interference (false sharing).
                Default is 7.
                alignof(T) must not be greater than (1 << align_log2).

Interface:
  General:
    bool is_empty() const;
        Returns true if there is currently no object in the queue.
        Returns false otherwise.

    bool is_full() const;
        Returns true if no more objects can be added to the queue.
        Returns false otherwise.

  Enqueue:
    bool push(const T& elem);
    bool push(T&& elem);
        Tries to insert elem into the queue. Returns true if successful, false
        otherwise.

    size_type push_n(size_type count, const T& elem);
        Tries to insert count copies of elem into the queue. Returns the
        number of copies successfully inserted.

    template<typename Iterator>
    size_type write(Iterator beg, Iterator end);
        Tries to copy elements into the queue from beg, until end is reached.
        Returns the number of elements copied into the queue.

    template<typename Iterator>
    size_type write(size_type count, Iterator elems);
        Tries to copy count elements into the queue from elems until either the
        queue is full or all have been copied.
        Returns the number of elements copied into the queue.

    template<typename... Args>
    bool emplace(Args&&... args);
        Tries to insert an object of type T constructed from args into the
        queue. Returns true if successful, false otherwise.

    template<typename... Args>
    size_type emplace_n(size_type count, Args&&... args);
        Tries to insert count objects of type T constructed from args into
        the queue. Returns the number of objects successfully inserted.

    template<typename Callable>
    bool produce(Callable&& f);
        Tries to insert an object into the queue by calling Callable if there is
        space for an object. Returns true if there was space for an object, and
        Callable returned true. Returns false otherwise.
        Callable is an invocable with one parameter of type void*, and a return
        type of bool. Callable is expected to place a new object of type T at
        the address passed to it.

    template<typename Callable>
    size_type produce_n(size_type count, Callable&& f);
        Tries to insert count objects into the queue by calling Callable as long
        as there is space in the queue, or until Callable returns false once.
        Returns the number of times Callable was invoked and returned true.
        Callable is an invocable with one parameter of type void*, and a return
        type of bool. Callable is expected to place a new object of type T at
        the address passed to it.

  Dequeue:
    const T* front() const;
    T* front();
        Returns a pointer to the next object in the queue, if such an object
        exists. Returns nullptr if the queue is empty.

    void discard();
        Removes the next object from the queue. This function must not be called
        if the queue is empty.

    bool pop(T& out);
        Tries to move the next object in the queue into out, if such an object
        exists. Returns true if out contains a new object. Returns false if the
        queue was empty.

    template<typename Iterator>
    size_type read(Iterator beg, Iterator end)
        Tries to move elements out of the queue to [beg .. end), until either
        all have been moved or the queue is empty.
        Returns the number of elements that were moved.

    template<typename Iterator>
    size_type read(size_type count, Iterator elems)
        Tries to move elements out of the queue to [elems .. elems + count),
        until either count elements have been moved, or the queue is empty.
        Returns the number of elements that were moved.

    template<typename Callable>
    bool consume(Callable&& f);
        Tries to remove an object from the queue by calling Callable and passing
        the object to it. Returns true if there was an object in the queue and
        Callable returned true. Returns false otherwise.
        Callable is an invocable with one parameter of type T*, and a return
        type of bool.

    template<typename Callable>
    size_type consume_all(Callable&& f);
        Tries to remove all objects from the queue by calling Callable for each
        object, passing the address of each object to it, until either the queue
        is empty, or Callable returns false. Returns the number of times
        Callable was invoked and returned true.
        Callable is an invocable with one parameter of type T*, and a return
        type of bool.

******************************************************************************/
#pragma once

#include <algorithm> // for std::copy_n
#include <array> // for std::array
#include <atomic> // for std::atomic<T> and std::atomic_thread_fence
#include <cstddef> // for std::byte
#include <functional> // for std::invoke
#include <iterator> // for std::iterator_traits
#include <new> // for std::launder and placement-new operator
#include <type_traits> // for std::forward, std::is_invocable_r, and
                       // std::is_constructible

namespace deaod {

namespace detail {

template<bool P, typename T, typename F>
using if_t = typename std::conditional<P, T, F>::type;

#if __cplusplus >= 201703L || __cpp_lib_byte > 0
using std::byte;
#else
using byte = unsigned char;
#endif

#if __cplusplus >= 201703L || __cpp_lib_void_t > 0
using std::void_t;
#else

template<typename...>
struct make_void {
    using type = void;
};

template<typename... Ts>
using void_t = typename make_void<Ts...>::type;

#endif

#if __cplusplus >= 201703L || __cpp_lib_launder > 0
using std::launder;
#else
template<typename T>
constexpr T* launder(T* p) noexcept {
    static_assert(
        std::is_function<T>::value == false && std::is_void<T>::value == false,
        "launder is invalid for function pointers and pointers to cv void"
    );
    return p;
}
#endif

template<typename T>
struct is_reference_wrapper : std::false_type {};

template<typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type{};

#if __cplusplus >= 201703L || __cpp_lib_invoke > 0

using std::invoke;

#else

struct fp_with_inst_ptr {};
struct fp_with_inst_val {};
struct fp_with_ref_wrap {};

struct dp_with_inst_ptr {};
struct dp_with_inst_val {};
struct dp_with_ref_wrap {};

template<typename Callable, typename... Args>
struct invoke_traits {
    using result_type =
        decltype(std::declval<Callable&&>()(std::declval<Args&&>()...));
};

template<typename Type, typename T, typename A1, typename... Args>
struct invoke_traits<Type T::*, A1, Args...> {
private:
    constexpr static bool _is_mem_func =
        std::is_member_function_pointer<Type T::*>::value;
    constexpr static bool _is_a1_a_ptr =
        std::is_base_of<T, typename std::decay<A1>::type>::value == false;
    constexpr static bool _is_a1_a_ref_wrap = is_reference_wrapper<A1>::value;

public:
    using tag_type = if_t<_is_mem_func,
        if_t<_is_a1_a_ptr,      fp_with_inst_ptr,
        if_t<_is_a1_a_ref_wrap, fp_with_ref_wrap,
        /* else */              fp_with_inst_val>>,
    /* else */
        if_t<_is_a1_a_ptr,      dp_with_inst_ptr,
        if_t<_is_a1_a_ref_wrap, dp_with_ref_wrap,
        /* else */              dp_with_inst_val>>
    >;

    using result_type = decltype(invoke(
        std::declval<tag_type>(),
        std::declval<Type T::*>(),
        std::declval<A1&&>(),
        std::declval<Args&&>()...
    ));
};

template<typename Callable, typename... Args>
auto invoke(Callable&& f, Args&& ... args)
-> decltype(std::forward<Callable>(f)(std::forward<Args>(args)...)) {
    return std::forward<Callable>(f)(std::forward<Args>(args)...);
}



template<typename Type, typename T, typename A1, typename... Args>
auto invoke(fp_with_inst_ptr, Type T::* f, A1&& a1, Args&& ... args)
-> decltype((*std::forward<A1>(a1).*f)(std::forward<Args>(args)...)) {
    return (*std::forward<A1>(a1).*f)(std::forward<Args>(args)...);
}

template<typename Type, typename T, typename A1, typename... Args>
auto invoke(fp_with_inst_val, Type T::* f, A1&& a1, Args&& ... args)
-> decltype((std::forward<A1>(a1).*f)(std::forward<Args>(args)...)) {
    return (std::forward<A1>(a1).*f)(std::forward<Args>(args)...);
}

template<typename Type, typename T, typename A1, typename... Args>
auto invoke(fp_with_ref_wrap, Type T::* f, A1&& a1, Args&& ... args)
-> decltype((a1.get().*f)(std::forward<Args>(args)...)) {
    return (a1.get().*f)(std::forward<Args>(args)...);
}

template<typename Type, typename T, typename A1, typename... Args>
auto invoke(dp_with_inst_ptr, Type T::* f, A1&& a1, Args&& ...)
-> typename std::decay<Type>::type {
    static_assert(sizeof...(Args) == 0,
        "invoke on data member pointer must not provide arguments other than "
        "instance pointer");
    return *std::forward<A1>(a1).*f;
}

template<typename Type, typename T, typename A1, typename... Args>
auto invoke(dp_with_inst_val, Type T::* f, A1&& a1, Args&& ...)
-> typename std::decay<Type>::type {
    static_assert(sizeof...(Args) == 0,
        "invoke on data member pointer must not provide arguments other than "
        "instance pointer");
    return std::forward<A1>(a1).*f;
}

template<typename Type, typename T, typename A1, typename... Args>
auto invoke(dp_with_ref_wrap, Type T::* f, A1&& a1, Args&& ...)
-> typename std::decay<Type>::type {
    static_assert(sizeof...(Args) == 0,
        "invoke on data member pointer must not provide arguments other than "
        "instance pointer");
    return (a1.get().*f);
}

template<typename Type, typename T, typename A1, typename... Args>
auto invoke(Type T::* f, A1&& a1, Args&& ... args)
-> typename invoke_traits<
    decltype(f),
    decltype(a1),
    decltype(args)...
>::result_type {
    typename invoke_traits<
        decltype(f),
        decltype(a1),
        decltype(args)...
    >::tag_type tag;

    return invoke(tag, f, std::forward<A1>(a1), std::forward<Args>(args)...);
}

#endif

#if __cplusplus >= 201703L || __cpp_lib_is_invocable > 0

using std::is_invocable;
using std::is_invocable_r;

#elif __has_include(<boost/callable_traits/is_invocable.hpp>)

#include <boost/callable_traits/is_invocable.hpp>
using boost::callable_traits::is_invocable;
using boost::callable_traits::is_invocable_r;

#else

// Dummy implementation because these are not used for correctness,
// only for better error messages
template<typename...>
struct is_invocable : std::true_type {};
template<typename...>
struct is_invocable_r : std::true_type {};

#endif

template<typename Callable>
struct scope_guard {
    scope_guard(Callable&& f) : _f(std::forward<Callable>(f)) {}
    ~scope_guard() {
        if (should_call()) {
            _f();
        }
    };
    
    scope_guard(const scope_guard&) = delete;
    scope_guard& operator=(const scope_guard&) = delete;

#if __cplusplus >= 201703L || __cpp_guaranteed_copy_elision > 0

private:
    bool should_call() const {
        return true;
    }

#else

    scope_guard(scope_guard&& other) : _f(std::move(other._f)) {
        other._ignore = true;
    }

    scope_guard& operator=(scope_guard&& other) {
        _ignore = false;
        _f = std::move(other._f);

        other._ignore = true;
    }

private:
    bool _ignore = false;
    bool should_call() const {
        return _ignore == false;
    }

#endif
    Callable _f;
};

template<typename Callable>
scope_guard<Callable> make_scope_guard(Callable&& f) {
    return scope_guard<Callable>(std::forward<Callable>(f));
}

} // namespace detail

template<typename T, size_t queue_size, int align_log2 = 7>
struct alignas((size_t)1 << align_log2) spsc_queue { // gcc bug 89683
    using value_type = T;
    using size_type = size_t;

    static const auto size = queue_size;
    static const auto align = size_t(1) << align_log2;

    static_assert(
        alignof(T) <= align,
        "Type T must not be more aligned than this queue"
    );

    spsc_queue() = default;

    ~spsc_queue() {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        consume_all([](T*) { return true; });
    }

    spsc_queue(const spsc_queue& other) {
        auto tail = 0;

        auto g = detail::make_scope_guard([&, this] {
            tail_cache = tail;
            _tail.store(tail);
        });

        auto src_tail = other._tail.load();
        auto src_head = other._head.load();

        while (src_head != src_tail) {
            new(_buffer.data() + tail * sizeof(T))
                T(*detail::launder(reinterpret_cast<T*>(
                    other._buffer.data() + src_head * sizeof(T)
                )));

            tail += 1;
            src_head += 1;
            if (src_head == size) src_head = 0;
        }
    }

    spsc_queue& operator=(const spsc_queue& other) {
        if (this == &other) return *this;

        {
            auto head = _head.load();
            auto tail = _tail.load();

            auto g = detail::make_scope_guard([&, this] {
                head_cache = head;
                _head.store(head);
            });

            while (head != tail) {
                auto elem = detail::launder(
                    reinterpret_cast<T*>(_buffer.data() + head * sizeof(T))
                );
                elem->~T();

                head += 1;
                if (head == size) head = 0;
            }
        }

        _tail.store(0);
        head_cache = 0;
        _head.store(0);
        tail_cache = 0;

        {
            auto tail = 0;

            auto g = detail::make_scope_guard([&, this] {
                tail_cache = tail;
                _tail.store(tail);
            });

            auto src_tail = other._tail.load();
            auto src_head = other._head.load();

            while (src_head != src_tail) {
                new(_buffer.data() + tail * sizeof(T))
                    T(*detail::launder(reinterpret_cast<T*>(
                        other._buffer.data() + src_head * sizeof(T)
                    )));

                tail += 1;
                src_head += 1;
                if (src_head == size) src_head = 0;
            }
        }

        return *this;
    }

    bool is_empty() const {
        auto head = _head.load(std::memory_order_acquire);
        auto tail = _tail.load(std::memory_order_acquire);

        return head == tail;
    }

    bool is_full() const {
        auto head = _head.load(std::memory_order_acquire);
        auto tail = _tail.load(std::memory_order_acquire) + 1;
        if (tail == size) tail = 0;

        return head == tail;
    }

    // copies elem into queue, if theres space
    // returns true if successful, false otherwise
    bool push(const T& elem) {
        return this->emplace(elem);
    }

    // tries to move elem into queue, if theres space
    // returns true if successful, false otherwise
    bool push(T&& elem) {
        return this->emplace(std::move(elem));
    }

    // tries to copy count elements into the queue
    // returns the number of elements that actually got copied
    size_type push_n(size_type count, const T& elem) {
        return this->emplace_n(count, elem);
    }


    // copies elements into queue until end is reached or queue is full,
    // whichever happens first
    // returns the number of elements copied into the queue
    template<typename Iterator>
    size_type write(Iterator beg, Iterator end) {
        static_assert(
            std::is_constructible<T, decltype(*beg)>::value,
            "T must be constructible from Iterator::reference"
        );

        using traits = std::iterator_traits<Iterator>;

        constexpr bool is_random_access = std::is_same<
            typename traits::iterator_category,
            std::random_access_iterator_tag
        >::value;

        // std::contiguous_iterator_tag is a feature of C++20, so try to be
        // compatible with it. Fall back on an approximate implementation for
        // C++17 or earlier. The value to compare against was chosen such that
        // compilers that implement some features of future standards and
        // indicate that using the value of __cplusplus dont accidentally fall
        // into the requirement to implement std::contiguous_iterator_tag.
#if __cplusplus > 202000L
        constexpr bool is_contiguous = std::is_same<
            typename traits::iterator_category,
            std::contiguous_iterator_tag
        >::value;
#else
        constexpr bool is_contiguous = std::is_pointer<Iterator>::value;
#endif

        readwrite_tag<
            is_random_access || is_contiguous,
            std::is_trivially_constructible<T, decltype(*beg)>::value
        > tag;

        return this->write_fwd(tag, beg, end);
    }

    // copies elements into queue until count elements have been copied or
    // queue is full, whichever happens first
    // returns the number of elements copied into queue
    template<typename Iterator>
    size_type write(size_type count, Iterator elems) {
        static_assert(
            std::is_constructible<T, decltype(*elems)>::value,
            "T must be constructible from Iterator::reference"
        );

        readwrite_tag<
            true,
            std::is_trivially_constructible<T, decltype(*elems)>::value
        > tag;
        
        return this->write_fwd(tag, count, elems);
    }

private:
    template<bool is_contiguous, bool is_trivial>
    struct readwrite_tag {};

    template<typename Iterator>
    size_type write_fwd(
        readwrite_tag<false, false>,
        Iterator beg,
        Iterator end)
    {
        return this->write_internal(beg, end);
    }

    template<typename Iterator>
    size_type write_fwd(
        readwrite_tag<false, true>,
        Iterator beg,
        Iterator end)
    {
        return this->write_internal(beg, end);
    }

    template<typename Iterator>
    size_type write_fwd(
        readwrite_tag<true, false>,
        Iterator beg,
        Iterator end)
    {
        return this->write_copy(end - beg, beg);
    }

    template<typename Iterator>
    size_type write_fwd(
        readwrite_tag<true, true>,
        Iterator beg,
        Iterator end)
    {
        return this->write_trivial(end - beg, beg);
    }

    template<typename Iterator>
    size_type write_fwd(
        readwrite_tag<true, false>,
        size_type count,
        Iterator elems)
    {
        return this->write_copy(count, elems);
    }

    template<typename Iterator>
    size_type write_fwd(
        readwrite_tag<true, true>,
        size_type count,
        Iterator elems)
    {
        return this->write_trivial(count, elems);
    }

    template<typename Iterator>
    size_type write_trivial(size_type count, Iterator elems) {
        auto tail = _tail.load(std::memory_order_relaxed);
        auto head = head_cache;
        auto free = size - (tail - head);
        if (free > size) free -= size;

        if (count >= free) {
            head = head_cache = _head.load(std::memory_order_acquire);
            free = size - (tail - head);
            if (free > size) free -= size;

            if (count >= free) {
                count = free - 1;
            }
        }

        auto next = tail + count;
        if (next >= size) {
            next -= size;
            auto split_pos = count - next;
            std::copy_n(
                elems,
                split_pos,
                reinterpret_cast<T*>(_buffer.data() + tail * sizeof(T))
            );
            std::copy_n(
                elems + split_pos,
                next,
                reinterpret_cast<T*>(_buffer.data())
            );
        } else {
            std::copy_n(
                elems,
                count,
                reinterpret_cast<T*>(_buffer.data() + tail * sizeof(T))
            );
        }

        _tail.store(next, std::memory_order_release);
        return count;
    }

    template<typename Iterator>
    size_type write_copy(size_type count, Iterator elems) {
        auto tail = _tail.load(std::memory_order_relaxed);
        auto head = head_cache;
        auto free = size - (tail - head);
        if (free > size) free -= size;

        if (count >= free) {
            head = head_cache = _head.load(std::memory_order_acquire);
            free = size - (tail - head);
            if (free > size) free -= size;

            if (count >= free) {
                count = free - 1;
            }
        }

        auto next = tail + count;
        if (next >= size) next -= size;

        auto g = detail::make_scope_guard([&, this] {
            _tail.store(tail, std::memory_order_release);
        });

        while (tail != next) {
            new(_buffer.data() + tail * sizeof(T)) T(*elems);

            ++elems;
            tail += 1;
            if (tail == size) tail = 0;
        }

        return count;
    }

    template<typename Iterator>
    size_type write_internal(Iterator beg, Iterator end) {
        auto tail = _tail.load(std::memory_order_relaxed);

        auto g = detail::make_scope_guard([&, this] {
            _tail.store(tail, std::memory_order_release);
        });

        auto count = size_type(0);
        for (; beg != end; ++beg) {
            auto next = tail + 1;
            if (next == size) next = 0;

            auto head = head_cache;
            if (next == head) {
                head = head_cache = _head.load(std::memory_order_acquire);
                if (next == head) {
                    break;
                }
            }

            new(_buffer.data() + tail * sizeof(T)) T(*beg);
            tail = next;
            count += 1;
        }

        return count;
    }

public:
    // constructs an element of type T in place using Args
    // returns true if successful, false otherwise
    template<typename... Args>
    bool emplace(Args&&... args) {
        static_assert(
            std::is_constructible<value_type, Args...>::value,
            "Type T must be constructible from Args..."
        );

        auto tail = _tail.load(std::memory_order_relaxed);
        auto next = tail + 1;
        if (next == size) next = 0;

        auto head = head_cache;
        if (next == head) {
            head = head_cache = _head.load(std::memory_order_acquire);
            if (next == head) {
                return false;
            }
        }

        new(_buffer.data() + tail * sizeof(T)) T{ std::forward<Args>(args)... };

        _tail.store(next, std::memory_order_release);
        return true;
    }

    // tries to construct count elements of type T in place using Args
    // returns the number of elements that got constructed
    template<typename... Args>
    size_type emplace_n(size_type count, Args&&... args) {
        static_assert(
            std::is_constructible<value_type, Args...>::value,
            "Type T must be constructible from Args..."
        );

        auto tail = _tail.load(std::memory_order_relaxed);
        auto head = head_cache;
        auto free = size - (tail - head);
        if (free > size) free -= size;

        if (count >= free) {
            head = head_cache = _head.load(std::memory_order_acquire);
            free = size - (tail - head);
            if (free > size) free -= size;

            if (count >= free) {
                count = free - 1;
            }
        }

        auto next = tail + count;
        if (next >= size) next -= size;

        auto g = detail::make_scope_guard([&, this] {
            _tail.store(tail, std::memory_order_release);
        });

        while (tail != next) {
            new(_buffer.data() + tail * sizeof(T)) T{ args... };

            tail += 1;
            if (tail == size) tail = 0;
        }

        return count;
    }

    // Callable is an invocable that takes void* and returns bool
    // Callable must use placement new to construct an object of type T at the
    // pointer passed to it. If it cannot do so, it must return false. If it
    // returns false, an object of type T must not have been constructed.
    // 
    // This function returns true if there was space for at least one element,
    // and Callable returned true. Otherwise, false will be returned.
    template<typename Callable>
    bool produce(Callable&& f) {
        static_assert(
            detail::is_invocable_r<bool, Callable&&, void*>::value,
            "Callable must return bool, and take void*"
        );

        auto tail = _tail.load(std::memory_order_relaxed);
        auto next = tail + 1;
        if (next == size) next = 0;

        auto head = head_cache;
        if (next == head) {
            head = head_cache = _head.load(std::memory_order_acquire);
            if (next == head) {
                return false;
            }
        }

        void* storage = _buffer.data() + tail * sizeof(T);
        if (detail::invoke(std::forward<Callable>(f), storage)) {
            _tail.store(next, std::memory_order_release);
            return true;
        }

        return false;
    }

    // Callable is an invocable that takes void* and returns bool
    // Callable must use placement new to construct an object of type T at the
    // pointer passed to it. If it cannot do so, it must return false. If it
    // returns false, an object of type T must not have been constructed.
    // 
    // This function tries to construct count elements by calling Callable for
    // each address where an object can be constructed. This function returns
    // the number of elements that were successfully constructed, that is the
    // number of times Callable returned true.
    template<typename Callable>
    size_type produce_n(size_type count, Callable&& f) {
        static_assert(
            detail::is_invocable_r<bool, Callable&&, void*>::value,
            "Callable must return bool, and take void*"
        );

        auto tail = _tail.load(std::memory_order_relaxed);
        auto head = head_cache;
        auto free = size - (tail - head);
        if (free > size) free -= size;

        if (count >= free) {
            head = head_cache = _head.load(std::memory_order_acquire);
            free = size - (tail - head);
            if (free > size) free -= size;

            if (count >= free) {
                count = free - 1;
            }
        }

        auto next = tail + count;
        if (next >= size) next -= size;

        auto g = detail::make_scope_guard([&, this] {
            _tail.store(tail, std::memory_order_release);
        });

        while (tail != next) {
            void* storage = _buffer.data() + tail * sizeof(T);
            if (!detail::invoke(f, storage)) {
                auto ret = next - tail;
                if (ret < 0) ret += size;
                return ret;
            }

            tail += 1;
            if (tail == size) tail = 0;
        }

        return count;
    }

    // Returns a pointer to the next element that can be dequeued, or nullptr
    // if the queue is empty.
    const T* front() const {
        auto head = _head.load(std::memory_order_relaxed);
        auto tail = tail_cache;

        if (head == tail) {
            tail = tail_cache = _tail.load(std::memory_order_acquire);
            if (head == tail) {
                return nullptr;
            }
        }

        return detail::launder(
            reinterpret_cast<const T*>(_buffer.data() + head * sizeof(T))
        );
    }

    // Returns a pointer to the next element that can be dequeued, or nullptr
    // if the queue is empty.
    T* front() {
        auto head = _head.load(std::memory_order_relaxed);
        auto tail = tail_cache;

        if (head == tail) {
            tail = tail_cache = _tail.load(std::memory_order_acquire);
            if (head == tail) {
                return nullptr;
            }
        }

        return detail::launder(
            reinterpret_cast<T*>(_buffer.data() + head * sizeof(T))
        );
    }

    // Discards the next element to be dequeued. The queue must contain at
    // least one element before calling this function.
    void discard() {
        auto head = _head.load(std::memory_order_relaxed);

        auto elem = detail::launder(
            reinterpret_cast<T*>(_buffer.data() + head * sizeof(T))
        );
        elem->~T();

        auto next = head + 1;
        if (next == size) next = 0;
        _head.store(next, std::memory_order_release);
    }

    // tries to move the next element to be dequeued into out.
    // Returns true if out was assigned to, false otherwise.
    bool pop(T& out) {
        auto head = _head.load(std::memory_order_relaxed);
        auto tail = tail_cache;

        if (head == tail) {
            tail = tail_cache = _tail.load(std::memory_order_acquire);
            if (head == tail) {
                return false;
            }
        }

        auto elem = detail::launder(
            reinterpret_cast<T*>(_buffer.data() + head * sizeof(T))
        );

        out = std::move(*elem);
        elem->~T();

        auto next = head + 1;
        if (next == size) next = 0;
        _head.store(next, std::memory_order_release);
        return true;
    }

    // tries to move elements to [beg .. end), or until the queue is empty
    // returns the number of elements moved
    template<typename Iterator>
    size_type read(Iterator beg, Iterator end) {
        static_assert(
            std::is_assignable<decltype(*beg), T&&>::value,
            "You must be able to assign T&& to Iterator::reference"
        );

        using traits = std::iterator_traits<Iterator>;

        constexpr bool is_random_access = std::is_same<
            typename traits::iterator_category,
            std::random_access_iterator_tag
        >::value;

        // std::contiguous_iterator_tag is a feature of C++20, so try to be
        // compatible with it. Fall back on an approximate implementation for
        // C++17 or earlier. The value to compare against was chosen such that
        // compilers that implement some features of future standards and
        // indicate that using the value of __cplusplus dont accidentally fall
        // into the requirement to implement std::contiguous_iterator_tag.
#if __cplusplus > 202000L
        constexpr bool is_contiguous = std::is_same<
            typename traits::iterator_category,
            std::contiguous_iterator_tag
        >::value;
#else
        constexpr bool is_contiguous = std::is_pointer<Iterator>::value;
#endif

        readwrite_tag<
            is_random_access || is_contiguous,
            std::is_trivially_constructible<T, decltype(*beg)>::value
        > tag;

        return this->read_fwd(tag, beg, end);
    }

    // tries to move elements to [elems .. elems + count) or until the queue is
    // empty
    // returns the number of elements moved
    template<typename Iterator>
    size_type read(size_type count, Iterator elems) {
        static_assert(
            std::is_assignable<decltype(*elems), T&&>::value,
            "You must be able to assign T&& to Iterator::reference"
        );

        readwrite_tag<
            true,
            std::is_trivially_constructible<T, decltype(*elems)>::value
        > tag;
        
        return this->read_fwd(tag, count, elems);
    }

private:
    template<typename Iterator>
    size_type read_fwd(
        readwrite_tag<false, false>,
        Iterator beg,
        Iterator end)
    {
        return this->read_internal(beg, end);
    }

    template<typename Iterator>
    size_type read_fwd(
        readwrite_tag<false, true>,
        Iterator beg,
        Iterator end)
    {
        return this->read_internal(beg, end);
    }

    template<typename Iterator>
    size_type read_fwd(
        readwrite_tag<true, false>,
        Iterator beg,
        Iterator end)
    {
        return this->read_copy(end - beg, beg);
    }

    template<typename Iterator>
    size_type read_fwd(
        readwrite_tag<true, true>,
        Iterator beg,
        Iterator end)
    {
        return this->read_trivial(end - beg, beg);
    }

    template<typename Iterator>
    size_type read_fwd(
        readwrite_tag<true, false>,
        size_type count,
        Iterator elems)
    {
        return this->read_copy(count, elems);
    }

    template<typename Iterator>
    size_type read_fwd(
        readwrite_tag<true, true>,
        size_type count,
        Iterator elems)
    {
        return this->read_trivial(count, elems);
    }

    template<typename Iterator>
    size_type read_trivial(size_type count, Iterator elems) {
        auto head = _head.load(std::memory_order_relaxed);
        auto tail = tail_cache;
        auto filled = (tail - head);
        if (filled > size) filled += size;

        if (count >= filled) {
            tail = tail_cache = _tail.load(std::memory_order_acquire);
            filled = (tail - head);
            if (filled > size) filled += size;

            if (count >= filled) {
                count = filled;
            }
        }

        auto next = head + count;
        if (next >= size) {
            next -= size;
            auto split_pos = count - next;
            std::copy_n(
                elems,
                split_pos,
                detail::launder(
                    reinterpret_cast<T*>(_buffer.data() + head * sizeof(T))
                )
            );
            std::copy_n(
                elems + split_pos,
                next,
                detail::launder(reinterpret_cast<T*>(_buffer.data()))
            );
        } else {
            std::copy_n(
                elems,
                count,
                detail::launder(
                    reinterpret_cast<T*>(_buffer.data() + head * sizeof(T))
                )
            );
        }

        _head.store(next, std::memory_order_release);
        return count;
    }

    template<typename Iterator>
    size_type read_copy(size_type count, Iterator elems) {
        auto head = _head.load(std::memory_order_relaxed);
        auto tail = tail_cache;
        auto filled = (tail - head);
        if (filled > size) filled += size;

        if (count >= filled) {
            tail = tail_cache = _tail.load(std::memory_order_acquire);
            filled = (tail - head);
            if (filled > size) filled += size;

            if (count >= filled) {
                count = filled;
            }
        }

        auto next = head + count;
        if (next >= size) next -= size;

        auto g = detail::make_scope_guard([&, this] {
            _head.store(head, std::memory_order_release);
        });

        while (head != next) {
            auto elem = detail::launder(
                reinterpret_cast<T*>(_buffer.data() + head * sizeof(T))
            );

            *elems = std::move(elem);
            elem->~T();

            head += 1;
            if (head == size) head = 0;
        }

        return count;
    }

    template<typename Iterator>
    size_type read_internal(Iterator beg, Iterator end) {
        auto head = _head.load(std::memory_order_relaxed);

        auto g = detail::make_scope_guard([&, this] {
            _head.store(head, std::memory_order_release);
        });

        auto count = size_type(0);
        for (; beg != end; ++beg) {
            auto tail = tail_cache;
            if (head == tail) {
                tail = tail_cache = _tail.load(std::memory_order_acquire);
                if (head == tail) {
                    break;
                }
            }

            auto elem = detail::launder(
                reinterpret_cast<T*>(_buffer.data() + head * sizeof(T))
            );
            
            *beg = std::move(elem);
            elem->~T();

            head += 1;
            if (head == size) head = 0;
            count += 1;
        }

        return count;
    }

public:
    // Callable is an invocable that takes T* and returns bool
    //
    // This function calls Callable with the address of the next element to be
    // dequeued, if the queue is not empty. If Callable returns true, the
    // element is removed from the queue and this function returns true.
    // Otherwise this function returns false.
    template<typename Callable>
    bool consume(Callable&& f) {
        static_assert(
            detail::is_invocable_r<bool, Callable&&, T*>::value,
            "Callable must return bool, and take T*"
        );

        auto head = _head.load(std::memory_order_relaxed);
        auto tail = tail_cache;

        if (head == tail) {
            tail = tail_cache = _tail.load(std::memory_order_acquire);
            if (head == tail) {
                return false;
            }
        }

        auto elem = detail::launder(
            reinterpret_cast<T*>(_buffer.data() + head * sizeof(T))
        );

        if (detail::invoke(std::forward<Callable>(f), elem)) {
            elem->~T();
            auto next = head + 1;
            if (next == size) next = 0;
            _head.store(next, std::memory_order_release);
            return true;
        }

        return false;
    }

    // Callable is an invocable that takes T* and returns bool
    //
    // This function calls Callable for each element currently in the queue,
    // with the address of that element. If Callable returns true, the element
    // is removed from the queue. If Callable returns false, the element is not
    // removed, and this function returns. This function always returns the
    // number of times Callable returned true.
    template<typename Callable>
    size_type consume_all(Callable&& f) {
        static_assert(
            detail::is_invocable_r<bool, Callable&&, T*>::value,
            "Callable must return bool, and take T*"
        );

        auto head = _head.load(std::memory_order_relaxed);
        auto tail = tail_cache = _tail.load(std::memory_order_acquire);
        auto old_head = head;

        auto g = detail::make_scope_guard([&, this] {
            _head.store(head, std::memory_order_release);
        });

        while (head != tail) {
            auto elem = detail::launder(
                reinterpret_cast<T*>(_buffer.data() + head * sizeof(T))
            );

            if (!detail::invoke(f, elem)) {
                break;
            }

            elem->~T();
            head += 1;
            if (head == size) head = 0;
        }

        ptrdiff_t ret = head - old_head;
        if (ret < 0) ret += size;
        return ret;
    }

private:
    alignas(align) std::array<detail::byte, size * sizeof(T)> _buffer;

    alignas(align) std::atomic<size_t> _tail{ 0 };
    mutable size_t head_cache{ 0 };

    alignas(align) std::atomic<size_t> _head{ 0 };
    mutable size_t tail_cache{ 0 };
};

} // namespace deaod
