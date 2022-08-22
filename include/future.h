#include <memory>

enum State { Pending, Ready };

template <class T> struct Poll {
  Poll(T i, State state = State::Pending) : data(i), state(state) {}
  T data;
  State state;
};

struct Waker {
  virtual void wake() = 0;
};

template <class T> class Future {
public:
  virtual Poll<T> poll(std::shared_ptr<Waker> waker) = 0;
};
