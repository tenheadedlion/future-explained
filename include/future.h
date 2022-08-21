enum State { Pending, Ready };

template <class T> struct Poll {
  Poll(T i): data(i), state(State::Pending) {}
  T data;
  State state;
};

template <class T> class Future {
public:
  virtual Poll<T> poll() = 0;
};
