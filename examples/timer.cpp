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

class TimerFuture;
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

struct Waker : std::enable_shared_from_this<Waker> {
  [[nodiscard]] static std::shared_ptr<Waker> create() {
    return std::shared_ptr<Waker>(new Waker());
  }
  void wake() {}

private:
  Waker() = default;
};

struct SharedState : std::enable_shared_from_this<SharedState> {
  [[nodiscard]] static std::shared_ptr<SharedState> create() {
    return std::shared_ptr<SharedState>(new SharedState());
  }
  bool completed;
  std::shared_ptr<Waker> waker;
  std::mutex mutex;

private:
  SharedState() : completed(false), waker(nullptr) {}
};

template <class T> using ChannelPtr = std::shared_ptr<Channel<T>>;
using TaskPtr = std::shared_ptr<Task>;
using SharedStatePtr = std::shared_ptr<SharedState>;

class TimerFuture : Future<int> {
public:
  SharedStatePtr shared_state;
  TimerFuture(int sec) {
    shared_state = SharedState::create();
    std::thread t([=] {
      // this isolated thread must find a way to notify the world when it
      // finishes the job
      std::this_thread::sleep_for(std::chrono::seconds(sec));
      std::lock_guard<std::mutex> guard(shared_state->mutex);
      shared_state->completed = false;
      // this waker
      if (shared_state->waker.get()) {
        shared_state->waker->wake();
      }
    });
    t.detach();
  }
  // this method will be called by the executor.
  // here's one problem, when the task is not completed, from whom the waker
  // will be installed to the shared_state?
  // this is simply a design choice, we make it different from the Rust
  // strategy, we define the waker in TimeFuture, and we don't need `context`
  // here
  Poll<int> poll() {
    std::lock_guard<std::mutex> guard(shared_state->mutex);
    if (shared_state->completed) {
      return Poll<int>(42, State::Ready);
    } else {
      auto waker = Waker::create();
      shared_state->waker = waker;
      return Poll<int>(42, State::Pending);
    }
  }
};

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

  auto future = std::make_unique<TimerFuture>(3);
  auto future2 = std::make_unique<TimerFuture>(4);
  spawner.spawn(std::move(future));
  spawner.spawn(std::move(future2));
  spawner.drop();

  executor.run();
}
