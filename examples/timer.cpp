#include "future.h"
#include "msd/channel.hpp"
#include <iostream>
#include <memory>
#include <thread>

template <class T> class Channel : std::enable_shared_from_this<Channel<T>> {
public:
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
    msd::channel<T> msd_chan{threads};
  }
  msd::channel<T> ch;
};

class TimerFuture : Future<int> {
public:
  Poll<int> poll() { return 42; }
};

class Task : std::enable_shared_from_this<Task> {
public:
  template <class... Arg>
  [[nodiscard]] static std::shared_ptr<Task> create(Arg &&...args) {
    return std::shared_ptr<Task>(new Task(std::forward<Arg>(args)...));
  };

  bool is_valid;
  std::unique_ptr<TimerFuture> future;
  std::shared_ptr<Channel<std::shared_ptr<Task>>> task_sender;

private:
  Task() : future(nullptr), task_sender(nullptr), is_valid(true) {}
  Task(std::unique_ptr<TimerFuture> future,
       std::shared_ptr<Channel<std::shared_ptr<Task>>> chan)
      : future(std::move(future)), task_sender(chan), is_valid(true) {}
  Task(const Task &) = delete;
};

template <class T> using ChannelPtr = std::shared_ptr<Channel<T>>;
using TaskPtr = std::shared_ptr<Task>;

class Executor {
public:
  Executor(ChannelPtr<TaskPtr> chan) : chan(chan) {}
  void run() {
    while (true) {
      TaskPtr task = chan->recv();
      if (task->is_valid) {
        std::cout << task->future->poll().data << std::endl;
      } else {
        return;
      }
    }
  }

private:
  ChannelPtr<TaskPtr> chan;
};

class Spawner {
public:
  Spawner(ChannelPtr<TaskPtr> chan) : chan(chan) {}
  void drop() {
    // out of band data
    TaskPtr eot = Task::create();
    eot->is_valid = false;
    chan->send(eot);
  }
  void spawn(std::unique_ptr<TimerFuture> future) {
    TaskPtr task = Task::create(std::move(future), chan);
    chan->send(task);
  }

private:
  ChannelPtr<TaskPtr> chan;
};

int main() {
  auto chan = Channel<TaskPtr>::create();
  Executor executor(chan);
  Spawner spawner(chan);

  auto future = std::make_unique<TimerFuture>();
  auto future2 = std::make_unique<TimerFuture>();
  spawner.spawn(std::move(future));
  spawner.spawn(std::move(future2));
  spawner.drop();

  executor.run();
}
