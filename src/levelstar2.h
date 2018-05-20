int level_star2() {
    const char* base_map =
        "..................."
        ".     ...         ."
        ".   .......       ."
        ".   . O O ..      ."
        ".  ..O.O.O.. ...  ."
        ".  .OOOOOOO...... ."
        ". .. .O.O. R<< *. ."
        ". ..OOOOOOO.....  ."
        ". ...O.O.O....    ."
        ".   . O O .       ."
        ".    ......       ."
        ".    ......       ."
        ".    ...          ."
        "~~~~~~~~~~~~~~~~~~~";

    using St = State<14, 19, 26, 1, 0, 0>;
    St::Map map(base_map);
    St st(map);
    st.print(map);

    return search(st, map);
}
