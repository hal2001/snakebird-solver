bool level_36() {
    const char* base_map =
        "...................."
        ".                  ."
        ".  ....        ... ."
        ".  ....        ... ."
        ".   ..             ."
        ". .             *  ."
        ". .                ."
        ". . >>R   #    ... ."
        ". T>^G<<     ###   ."
        ". ......##T  #     ."
        ".   ..   ###.#     ."
        ".           .      ."
        "~~~~~~~~~~~~~~~~~~~~";

    using St = State<13, 20, 0, 2, 0, 0>;
    St::Map map(base_map);
    St st(map);
    st.print(map);

    return search(st, map);
}
