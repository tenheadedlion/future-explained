enum State { Pending, Ready };

template <class T> struct Poll {
  Poll(T i, State state = State::Pending) : data(i), state(state) {}
  T data;
  State state;
};

template <class T> class Future {
public:
  virtual Poll<T> poll() = 0;
};
