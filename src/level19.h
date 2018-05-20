bool level_19() {
    const char* base_map =
        "................"        
        ".              ."
        ".   ~          ."
        ".~  .          ."
        ".. *          O."
        ".              ."
        ".              ."
        ".              ."
        ".   .          ."
        ".   .          ."
        ".   .          ."
        ".   .          ."
        "~~~~~~~~~~~~~~~~";

    using St = State<13, 16, 1, 3, 7>;
    St::Map map(base_map);
    St st;

    St::Snake red { 'R', 10, 5 };
    red.grow(St::Snake::UP);
    red.grow(St::Snake::UP);
    red.grow(St::Snake::UP);
    red.grow(St::Snake::LEFT);
    st.add_snake(red, 0);

    St::Snake green { 'G', 9, 3 };
    green.grow(St::Snake::UP);
    green.grow(St::Snake::UP);
    green.grow(St::Snake::UP);
    green.grow(St::Snake::RIGHT);
    green.grow(St::Snake::RIGHT);
    st.add_snake(green, 1);

    St::Snake blue { 'B', 5, 5 };
    blue.grow(St::Snake::LEFT);
    st.add_snake(blue, 2);

    st.print(map);

    return search(st, map);
}
