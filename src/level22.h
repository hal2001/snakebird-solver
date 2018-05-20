int level_22() {
    const char* base_map =
        "............."
        ".     *     ."
        ".           ."
        ".           ."
        ".           ."
        ".           ."
        ".  >>R      ."
        ".   ..   00 ."
        ".   .. . 00 ."
        ".   ..   .. ."
        ".   ....... ."
        ".   ....... ."
        "~~~~~~~~~~~~~";

    using St = State<13, 13, 0, 1, 0, 1>;
    St::Map map(base_map);
    St st(map);
    st.process_gravity(map);
    st.print(map);

    return search(st, map);
}
