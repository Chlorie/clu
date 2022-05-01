Coroutines
==========

The concept of coroutine is nothing new, the first occurence of which dates back to the 1950s. It is an extension on the "subroutine" (or "function") concept, supporting "suspension" (or "pause") and "resumption" of execution between routines. A coroutine can be viewed as a kind of syntactic sugar of a state machine. Below is a simple illustration of coroutine execution in pseudo-C++ code.

.. code-block:: cpp
    :linenos:

    Coro coroutine(int& value)
    {
        $suspend(); // suspension point #1
        value = 42;
        $suspend(); // suspension point #2
        return;
    }

    int main()
    {
        int value = 0;

        // coro runs to #1 and suspends, 
        // control flow returns to main
        Coro coro(value);

        // Resume the execution of coro,
        // from where it was suspended (#1).
        // It will set value to 42 and suspend again.
        coro.$resume();

        assert(value == 42);

        // This time coro resumes from #2,
        // and runs until the end (return statement).
        coro.$resume();

        return 0;
    }

In this example the control flow ping-pongs between the coroutine and the main function, which is a typical case for generator coroutines.

Generators
----------

A generator is a lazy range of values, in which each value is generated (yielded) on demand. Many other languages have some concrete implementation of generators, for example C# functions returning ``IEnumerable<T>`` with ``yield return`` statements in the function body, or Python generators with ``yield`` statements.

C++20 only provides us with the most bare-bone API of coroutines, with all the gnarly details exposed for coroutine frame manipulations and but support from the library side for user-facing APIs. There is currently a new ``std::generator`` class template proposed for the C++23 standard, and if you don't want to wait for another who-knows-how-many years, you can use the generator coroutine implementation in this library.

This library provides ``clu::generator<T>`` which supports yielding values from the coroutine body using the ``co_yield`` keyword. The following example shows how to use it.

.. code-block:: cpp
    :linenos:

    #include <iostream>
    #include <clu/generator.h>

    // A never-ending sequence of Fibonacci numbers
    clu::generator<int> fibonacci()
    {
        int a = 1, b = 1;
        while (true)
        {
            co_yield a; // Yielding a value
            a = b;
            b = a + b;
        }
    }

    int main()
    {
        // Generators are input ranges,
        // we can use them in range-based for loops
        for (const int i : fibonacci())
        {
            std::cout << i << ' ';
            if (i > 100)
                return 0;
        }
    }

For more details, see the documentation of ``clu::generator``.

.. warning:: There is no documentation yet...

In the generator case, the coroutine execution is managed by the caller, since each time the caller needs another value from the generator, it resumes the coroutine from the last suspension point.

Asynchronous Tasks
------------------

A coroutine can also put its handle into some other execution context, for example, into a thread pool, such that the coroutine could be scheduled to run in parallel with other coroutines. This is the typical case for asynchronous tasks. This is typically implemented in other languages with the async-await construct.

If there were an async network library supporting coroutines, the code below would be an example of how to use it.

.. code-block:: cpp
    :linenos:

    #include <awesome_coro_lib.h>

    // A coroutine that performs an asynchronous task.
    // C++ coroutines don't need special `async` keyword,
    // any function that contains `co_await` `co_yield` or
    // `co_return` will be considered a coroutine.
    task<std::string> get_response(const std::string& url)
    {
        http_client client;
        // Use the co_await keyword to suspend the coroutine,
        // and after the request is completed, the coroutine
        // will the resumed with the response.
        std::string response = co_await client.get_async(url);
        co_return response;
    }

In this example, ``client.get_async(url)`` would return an "awaitable" object. Applying ``co_await`` on the awaitable object will suspend the coroutine and wait for someone to resume. For asynchronous operations like this, the suspended coroutine's resumption is typically registered as a "callback", called after the asynchronous operation completes.

The clu library provides ``clu::task<T>`` to support this kind of asynchronous usage.

Note that the semantics of ``task`` here differs from that of some other languages that supports the async-await construct, such as C#. The C# ``Task<T>`` type starts eagerly, in the meaning that once a function returning ``Task<T>`` is called, the task is already running and runs parallel with the caller. This kind of detached execution means that we can no longer pass references to local variables as parameters to eager tasks, since the caller might never await on the callee task and thus might return sooner than the callee.

.. code-block:: cpp
    :linenos:

    eager_task<void> callee(const std::string& str)
    {
        co_await thread_pool.schedule_after(30s);
        // Now str is dangling...
        std::cout << str << '\n'; // Boom!
    }

    eager_task<void> caller()
    {
        std::string message = "Hello world!";
        callee(message);

        // Now we exit the coroutine, destroying
        // the message string, leaving a dangling
        // reference to the callee, since callee
        // is still waiting for the 30s delay.
        co_return;
    }

We don't want to pay the cost of copying the parameters to every coroutine we call, nor do we want to reference count everything. Thus we should practice the idiom of "structured-concurrency", make sure that the callee is finished before the caller is done with it. ``clu::task`` is a lazy task type, which means that the callee is not started until the caller awaits on it.

.. code-block:: cpp
    :linenos:

    clu::task<void> callee(const std::string& str)
    {
        co_await thread_pool.schedule_after(30s);
        std::cout << str << '\n';
    }

    clu::task<void> caller()
    {
        std::string message = "Hello world!";

        // Since clu::task is lazy, the following does nothing.
        // callee(message);

        co_await callee(message);
        // Now we can safely destroy the message string.
    }
