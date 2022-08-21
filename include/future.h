#include <memory>

enum State { Pending, Ready };

template <class T> struct Poll {
  Poll(T i, State state = State::Pending) : data(i), state(state) {}
  T data;
  State state;
};

struct Waker {
  // we want the waker to send the task back to the executor.
  // to archive that, the waker should hold the handle of the task,
  // so we might as well let Task inherit Waker, and implement the wake() method
  // itself
  virtual void wake() = 0;
};

template <class T> class Future {
public:
  virtual Poll<T> poll(std::shared_ptr<Waker> waker) = 0;
};
