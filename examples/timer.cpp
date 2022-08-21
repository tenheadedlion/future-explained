#include "future.h"
#include "msd/channel.hpp"
#include <iostream>
#include <memory>
#include <thread>

struct Task;
template <class T> class Channel : std::enable_shared_from_this<Channel<T>> {
public:
  std::shared_ptr<Channel<T>> getptr() { return this->shared_from_this(); }
  [[nodiscard]] static std::shared_ptr<Channel<T>> create() {
    return std::shared_ptr<Channel<T>>(new Channel());
  }
  void send(T item) { item >> ch; }
  T recv() {
    T item;
    item << ch;
    return item;
  }

private:
  Channel() {
    const auto threads = std::thread::hardware_concurrency();
    msd::channel<Task> msd_chan{threads};
  }
  msd::channel<T> ch;
};

class TimerFuture : Future<int> {
public:
  Poll<int> poll() { return 42; }
};

struct Task {
  Task() : task_sender(nullptr), is_valid(true) {}
  Task(TimerFuture future, std::shared_ptr<Channel<Task>> chan)
      : future(future), task_sender(chan) {}
  void set_eot() { is_valid = false; }
  bool is_valid;
  TimerFuture future;
  std::shared_ptr<Channel<Task>> task_sender;
};

template <class T> class Executor {
public:
  Executor(std::shared_ptr<Channel<Task>> chan) : chan(chan) {}
  void run() {
    while (true) {
      Task task = chan->recv();
      if (task.is_valid) {
        std::cout << task.future.poll().data << std::endl;
      } else {
        return;
      }
    }
  }

private:
  std::shared_ptr<Channel<Task>> chan;
};

template <class T> class Spawner {
public:
  Spawner(std::shared_ptr<Channel<Task>> chan) : chan(chan) {}
  void spawn(TimerFuture future) {
    Task task(future, chan);
    // out of band data
    Task eot(future, nullptr);
    eot.set_eot();
    chan->send(task);
    chan->send(eot);
  }

private:
  std::shared_ptr<Channel<Task>> chan;
};

int main() {
  TimerFuture tf;
  auto chan = Channel<Task>::create();
  Executor<Task> executor(chan);
  Spawner<Task> spawner(chan);

  TimerFuture future;
  spawner.spawn(future);

  executor.run();
}
