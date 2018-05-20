int level_01() {
    const char* base_map =
        ".........."
        ".    *   ."
        ".        ."
        ". .      ."
        ". O  .O. ."
        ".        ."
        ".  .>G   ."
        ".  ....  ."
        ".  ....  ."
        ".  ...   ."
        "~~~~~~~~~~";

    using St = State<Setup<11, 10, 2, 1, 4, 0>>;
    St::Map map(base_map);
    St st(map);
    st.print(map);

    return search(st, map);
}
