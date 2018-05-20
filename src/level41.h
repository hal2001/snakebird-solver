int level_41() {
    const char* base_map =
        "...................."
        ".                  ."
        ".     *        .   ."
        ".              .   ."
        ".                  ."
        ".  >B    G<   .    ."
        ".  ^O    O^   ..   ."
        ".     .       ..   ."
        ".     .       .. . ."
        ".     .       .. . ."
        "~~~~~~~~~~~~~~~~~~~~";

    using St = State<11, 20, 2, 2, 0, 0>;
    St::Map map(base_map);
    St st(map);
    st.print(map);

    return search(st, map);
}
