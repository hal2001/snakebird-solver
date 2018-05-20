int level_35() {
    const char* base_map =
        "..........."
        ". ....... ."
        ".  .....  ."
        ".  .....  ."
        ".         ."
        ".      .  ."
        ".      #  ."
        ".       O ."
        ".    T .  ."
        ".      .  ."
        ".         ."
        ". * >>B   ."
        ".   ^.    ."
        ".    .    ."
        ".     T   ."
        ".    ##   ."
        ".    O    ."
        ".    ..   ."
        ".    ..   ."
        ".    ..   ."
        "~~~~~~~~~~~";

    using St = State<21, 11, 2, 1, 0, 0>;
    St::Map map(base_map);
    St st(map);
    st.print(map);

    return search(st, map);
}
