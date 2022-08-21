#include "future.h"
#include "msd/channel.hpp"
#include <iostream>
#include <memory>
#include <thread>

//  The last sender closes the channel, so the receiver can quit, otherwise
//  the receiver will block forever
template <class T> class Channel : std::enable_shared_from_this<Channel<T>> {

  template <class U> friend class Sender;
  template <class U> friend class Receiver;

public:
  [[nodiscard]] static std::shared_ptr<Channel<T>> create() {
    return std::shared_ptr<Channel<T>>(new Channel());
  }

private:
  void send(T item) { item >> ch; }
  T recv() {
    T item;
    item << ch;
    return item;
  }

  Channel() {
    const auto threads = std::thread::hardware_concurrency();
    msd::channel<T> msd_chan{threads};
  }
  msd::channel<T> ch;
};

template <class T> class Sender : std::enable_shared_from_this<Sender<T>> {
public:
  [[nodiscard]] static std::shared_ptr<Sender<T>>
  create(std::shared_ptr<Channel<T>> chan) {
    return std::shared_ptr<Sender<T>>(new Sender(chan));
  }
  ~Sender() { chan.get()->ch.close(); }
  void send(T item) { chan->send(item); }

private:
  Sender(std::shared_ptr<Channel<T>> chan) : chan(chan) {}
  std::shared_ptr<Channel<T>> chan;
};

template <class T> class Receiver : std::enable_shared_from_this<Receiver<T>> {
public:
  [[nodiscard]] static std::shared_ptr<Receiver<T>>
  create(std::shared_ptr<Channel<T>> chan) {
    return std::shared_ptr<Receiver<T>>(new Receiver(chan));
  }
  T recv() { return chan->recv(); }

  bool closed() { return chan.get()->ch.closed(); }

private:
  Receiver(std::shared_ptr<Channel<T>> chan) : chan(chan) {}
  std::shared_ptr<Channel<T>> chan;
};

class TimerFuture;

class Task : std::enable_shared_from_this<Task>, public Waker {
public:
  template <class... Arg>
  [[nodiscard]] static std::shared_ptr<Task> create(Arg &&...args) {
    return std::shared_ptr<Task>(new Task(std::forward<Arg>(args)...));
  };

  bool is_valid;
  std::unique_ptr<TimerFuture> future;
  std::shared_ptr<Sender<std::shared_ptr<Task>>> task_sender;

  // We create a new task from the current task,
  // after that the current task is no longer valid
  void wake() override {
    std::shared_ptr<Task> task =
        Task::create(std::move(this->future), this->task_sender);
    this->task_sender->send(task);
  }

private:
  Task() : future(nullptr), task_sender(nullptr), is_valid(true) {}
  Task(std::unique_ptr<TimerFuture> future,
       std::shared_ptr<Sender<std::shared_ptr<Task>>> chan)
      : future(std::move(future)), task_sender(chan), is_valid(true) {}
  Task(const Task &) = delete;
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
  int sec;
  SharedStatePtr shared_state;
  TimerFuture(int sec) : sec(sec) {
    shared_state = SharedState::create();
    std::thread t([=] {
      // this isolated thread must find a way to notify the world when it
      // finishes the job
      std::this_thread::sleep_for(std::chrono::seconds(sec));
      std::lock_guard<std::mutex> guard(shared_state->mutex);
      shared_state->completed = true;
      // this waker
      if (shared_state->waker.get()) {
        shared_state->waker->wake();
      }
    });
    t.detach();
  }
  // this method will be called by the executor.
  // here's a problem: when the task is not completed, from whom the waker
  // will be installed to the shared_state?
  // we can't get any useful information from TimeFuture unless we let
  // TimeFuture contain a reference to the task, but such design will make
  // things complicated, therefore we need an external waker when calling this
  // method(Rust's Context begins to make sense)
  Poll<int> poll(std::shared_ptr<Waker> waker) {
    std::lock_guard<std::mutex> guard(shared_state->mutex);
    if (shared_state->completed) {
      return Poll<int>(42, State::Ready);
    } else {
      shared_state->waker = waker;
      return Poll<int>(42, State::Pending);
    }
  }
};

class Executor {
public:
  Executor(std::shared_ptr<Receiver<TaskPtr>> chan) : chan(chan) {}
  void run() {
    while (!chan->closed()) {
      TaskPtr task = chan->recv();
      if (task->is_valid) {
        auto sec = task->future->sec;
        // this is tricky, we pass the task as Waker
        auto res = task->future->poll(task);
        if (res.state == State::Ready) {
          std::cout << sec << " seconds has expired!" << std::endl;
        } else {
        }
      } else {
        return;
      }
    }
  }

private:
  std::shared_ptr<Receiver<TaskPtr>> chan;
};

class Spawner {
public:
  Spawner(std::shared_ptr<Sender<TaskPtr>> chan) : chan(chan) {}
  void spawn(std::unique_ptr<TimerFuture> future) {
    TaskPtr task = Task::create(std::move(future), chan);
    chan->send(task);
  }

private:
  std::shared_ptr<Sender<TaskPtr>> chan;
};

int main() {
  auto chan = Channel<TaskPtr>::create();
  auto sender = Sender<TaskPtr>::create(chan);
  auto receiver = Receiver<TaskPtr>::create(chan);
  Executor executor(receiver);
  Spawner spawner(sender);
  sender.reset();

  auto future = std::make_unique<TimerFuture>(1);
  auto future2 = std::make_unique<TimerFuture>(3);
  auto future3 = std::make_unique<TimerFuture>(10);
  spawner.spawn(std::move(future));
  spawner.spawn(std::move(future2));
  spawner.spawn(std::move(future3));
  spawner.~Spawner();
  executor.run();
}
