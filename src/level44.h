int level_44() {
    const char* base_map =
        ".............."
        ".            ."
        ".    T       ."
        ".      O   * ."
        ".  #O        ."
        ".    #       ."
        ".   >R T     ."
        ".   G<       ."
        ".  #....  #. ."
        ".   .. .   . ."
        ".    . .   . ."
        "~~~~~~~~~~~~~~";

    using St = State<Setup<12, 14, 2, 2, 4, 0, 1>>;
    St::Map map(base_map);
    St st(map);
    st.print(map);

    return search(st, map);
}
