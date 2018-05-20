int level_14() {
    const char* base_map =
        "............."
        ".           ."
        ".           ."
        ".         ~ ."
        ".  *   >B   ."
        ".      >R . ."
        ".      ^. . ."
        ".   .   . . ."
        ".   .   . . ."
        ".   .   . . ."
        "~~~~~~~~~~~~~";

    using St = State<Setup<11, 13, 0, 2, 3>>;
    St::Map map(base_map);
    St st(map);
    st.print(map);

    return search(st, map);
}
