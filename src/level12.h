int level_12() {
    const char* base_map =
        "..............."
        ".    .        ."
        ".    .        ."
        ".    ~        ."
        ".   v B       ."
        ".   >>^       ."
        ".    .        ."
        ".         ~   ."
        ".           * ."
        ". ~.~  ~~~.   ."
        ".        ..   ."
        ".             ."
        ". O   ~       ."
        ".    .~O      ."
        ".    ...      ."
        ".    ...      ."
        "~~~~~~~~~~~~~~~";

    using St = State<Setup<17, 15, 2, 1, 7>>;
    St::Map map(base_map);
    St st(map);
    st.print(map);

    return search(st, map);
}
