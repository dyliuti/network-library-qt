#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <functional>
#include <chrono>

namespace Async {

class Scheduler
{
public:
    virtual ~Scheduler() {}

    /**
     * The following functions need not to be thread-safe.
     * It is nonsense that schedule callback and submit request are in different threads.
     * Do it like this:
     * e.g.
     *
     * // In this_loop thread.
     *
     * Future<Buffer> ft(ReadFileInSeparateThread(very_big_file));
     *
     * ft.then(&this_loop, [](const Buffer& file_contents) {
     *      // SUCCESS : process file_content;
     * })
     * .onTimeout(std::chrono::seconds(10), [=very_big_file]() {
     *      // FAILED OR TIMEOUT:
     *      printf("Read file %s failed\n", very_big_file);
     * },
     * &this_loop);
     */
    virtual void scheduleLater(std::chrono::milliseconds duration, std::function<void()> f) = 0;
    virtual void schedule(std::function<void()> f) = 0;
};

} // namespace Async

#endif
