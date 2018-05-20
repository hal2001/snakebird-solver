#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <sparsehash/dense_hash_map>
#include <sparsehash/sparse_hash_map>
#include <third-party/cityhash/city.h>
#include <unordered_map>
#include <vector>

enum Direction {
    UP, RIGHT, DOWN, LEFT,
};

template<size_t Bits>
struct Packer {
    static const int Bytes = std::ceil(Bits / 8.0);

    Packer() {
    }

    template<typename T>
    size_t deposit(T data, size_t width, size_t at) {
        while (width) {
            size_t offset = (at % 8);
            int bits_to_deposit = std::min(width, 8 - offset);
            int deposit_at = at / 8;
            bytes_[deposit_at] |= data << offset;
            data >>= bits_to_deposit;
            at += bits_to_deposit;
            width -= bits_to_deposit;
        }

        return at;
    }

    template<typename T>
    uint64_t extract(T& data, size_t width, size_t at) const {
        T out = 0;
        size_t out_offset = 0;
        while (width) {
            int extract_from = at / 8;
            size_t offset = (at % 8);
            size_t bits_to_extract = std::min(width, 8 - offset);
            T extracted =
                (bytes_[extract_from] >> offset) &
                ((1 << bits_to_extract) - 1);
            out |= extracted << out_offset;
            out_offset += bits_to_extract;
            at += bits_to_extract;
            width -= bits_to_extract;
        }

        data = out;
        return at;
    }

    uint8_t bytes_[Bytes] = { 0 };
};

template<int H, int W, int MaxLen>
class Snake {
public:
    static const int kDirWidth = 2;
    static const int kDirMask = (1 << kDirWidth) - 1;
    // +1 since the head doesn't use up a slot in tail_
    using Tail32 = typename std::conditional<MaxLen <= 16 + 1, uint32_t, uint64_t>::type;
    using Tail16 = typename std::conditional<MaxLen <= 8 + 1, uint16_t, Tail32>::type;
    using Tail = typename std::conditional<MaxLen <= 4 + 1, uint8_t, Tail16>::type;
    using MapIndex = typename std::conditional<H*W < 255, uint8_t, uint16_t>::type;

    Snake() : tail_(0), i_(0), len_(0) {
    }

    Snake(int i)
        : tail_(0),
          i_(i),
          len_(1) {
        assert(i_ < H * W);
    }

    void grow(Direction dir) {
        i_ += apply_direction(dir);
        ++len_;
        tail_ = (tail_ << kDirWidth) | dir;
    }

    void move(Direction dir) {
        i_ += apply_direction(dir);
        tail_ &= ~(kDirMask << ((len_ - 2) * kDirWidth));
        tail_ = (tail_ << kDirWidth) | dir;
    }

    Direction tail(int i) const {
        return Direction((tail_ >> (i * kDirWidth)) & kDirMask);
    }

    static int apply_direction(Direction dir) {
        static int deltas[] = { -W, 1, W, -1 };
        return deltas[dir];
    }

    bool operator==(const Snake& other) const {
        return tail_ == other.tail_ &&
            i_ == other.i_ &&
            len_ == other.len_;
    }
    bool operator!=(const Snake& other) const {
        return !(*this == other);
    }

    bool operator<(const Snake& other) const {
        if (i_ != other.i_) {
            return i_ < other.i_;
        }
        if (len_ != other.len_) {
            return len_ < other.len_;
        }
        if (tail_ != other.tail_) {
            return tail_ < other.tail_;
        }
        return false;
    }

    Tail tail_;
    MapIndex i_;
    uint8_t len_;
} __attribute__((packed));

template<int H, int W>
class Gadget {
public:
    Gadget() : size_(0) {
    }

    void add(int i) {
        i_[size_++] = i;
    }

    uint16_t size_;
    uint16_t i_[8];
};

template<int H, int W, int FruitCount, int SnakeCount, int SnakeMaxLen,
         int GadgetCount=0, int TeleporterCount=0>
class State {
public:
    using Snake = typename ::Snake<H, W, SnakeMaxLen>;
    using Gadget = typename ::Gadget<H, W>;
    using Teleporter = typename std::pair<int, int>;

    static const int16_t kGadgetDeleted = 1 << 15;

    class Map {
    public:
        explicit Map(const char* base_map) : exit_(0) {
            assert(strlen(base_map) == H * W);
            base_map_ = new char[H * W];
            int fruit_count = 0;
            int snake_count = 0;
            int teleporter_count = 0;
            int max_len = 0;
            std::unordered_map<int, int> half_teleporter;
            for (int i = 0; i < H * W; ++i) {
                const char c = base_map[i];
                if (c == 'O') {
                    if (FruitCount) {
                        fruit_[fruit_count++] = i;
                    }
                    base_map_[i] = ' ';
                } else if (c == '*') {
                    assert(!exit_);
                    base_map_[i] = ' ';
                    exit_ = i;
                } else if (c == 'T') {
                    if (half_teleporter[c]) {
                        teleporters_[teleporter_count++] =
                            std::make_pair(half_teleporter[c], i);
                    } else {
                        half_teleporter[c] = i;
                    }
                    base_map_[i] = ' ';
                } else if (c == 'R' || c == 'G' || c == 'B') {
                    base_map_[i] = ' ';
                    Snake snake = Snake(i);
                    int len = 0;
                    snake.tail_ = trace_tail(base_map, i, &len);
                    snake.len_ += len;
                    snakes_[snake_count++] = snake;
                    max_len = std::max(max_len, (int) snake.len_);
                } else if (isdigit(c)) {
                    base_map_[i] = ' ';
                    uint32_t index = c - '0';
                    if (index < GadgetCount) {
                        if (!gadgets_[index].size_) {
                            gadget_offset_[index] = i;
                        }
                        gadgets_[index].add(i - gadget_offset_[index]);
                    }
                } else if (c == '>' || c == '<' || c == '^' || c == 'v') {
                    base_map_[i] = ' ';
                } else {
                    base_map_[i] = c;
                }
            }

            if (SnakeMaxLen < max_len + FruitCount) {
                fprintf(stderr, "Expected SnakeMaxLen >= %d, got %d\n",
                        max_len + FruitCount,
                        SnakeMaxLen);
            }
            assert(fruit_count == FruitCount);
            assert(snake_count == SnakeCount);
            assert(teleporter_count == TeleporterCount);
            assert(exit_);
        }

        uint32_t trace_tail(const char* base_map, int i, int* len) const {
            if (base_map[i - 1] == '>') {
                ++*len;
                return RIGHT |
                    (trace_tail(base_map, i - 1, len) << Snake::kDirWidth);
            }
            if (base_map[i + 1] == '<') {
                ++*len;
                return LEFT |
                    (trace_tail(base_map, i + 1, len) << Snake::kDirWidth);
            }
            if (base_map[i - W] == 'v') {
                ++*len;
                return DOWN |
                    (trace_tail(base_map, i - W, len) << Snake::kDirWidth);
            }
            if (base_map[i + W] == '^') {
                ++*len;
                return UP |
                    (trace_tail(base_map, i + W, len) << Snake::kDirWidth);
            }

            return 0;
        }

        char operator[](int i) const {
            return this->base_map_[i];
        }

        char* base_map_;
        int exit_;
        int fruit_[FruitCount];
        Snake snakes_[SnakeCount];
        Gadget gadgets_[GadgetCount];
        int gadget_offset_[GadgetCount] = { };
        Teleporter teleporters_[TeleporterCount];
    };

    class ObjMap {
    public:
        ObjMap(const State& st, const Map& map, bool draw_tail) {
            draw_objs(st, map, draw_tail);
        }

        bool no_object_at(int i) const {
            return obj_map_[i] == empty_id();
        }

        int fruit_id() const { return (1 + SnakeCount + GadgetCount); }
        bool fruit_at(int i) const {
            return obj_map_[i] == fruit_id();
        }

        bool foreign_object_at(int i, int id) const {
            return !no_object_at(i) &&
                obj_map_[i] != id;
        }

        int id_at(int i) const {
            return obj_map_[i];
        }
        int mask_at(int i) const {
            if (id_at(i)) {
                return 1 << (id_at(i) - 1);
            }
            return 0;
        }

    private:
        void draw_objs(const State& st,
                       const Map& map,
                       bool draw_path) {
            memset(obj_map_, empty_id(), H * W);
            for (int si = 0; si < SnakeCount; ++si) {
                draw_snake(st, si, draw_path);
            }
            for (int i = 0; i < FruitCount; ++i) {
                if (st.fruit_active(i)) {
                    obj_map_[map.fruit_[i]] = fruit_id();
                }
            }
            for (int i = 0; i < GadgetCount; ++i) {
                int offset = st.gadget_offset_[i];
                if (offset != kGadgetDeleted) {
                    const auto& gadget = map.gadgets_[i];
                    for (int j = 0; j < gadget.size_; ++j) {
                        obj_map_[offset + gadget.i_[j]] = gadget_id(i);
                    }
                }
            }
        }

        void draw_snake(const State& st, int si, bool draw_path) {
            const Snake& snake = st.snakes_[si];
            int i = snake.i_;
            for (int j = 0; j < snake.len_; ++j) {
                if (j == 0 || !draw_path) {
                    obj_map_[i] = snake_id(si);
                } else {
                    switch (snake.tail(j - 1)) {
                    case UP: obj_map_[i] = '^'; break;
                    case DOWN: obj_map_[i] = 'v'; break;
                    case LEFT: obj_map_[i] = '<'; break;
                    case RIGHT: obj_map_[i] = '>'; break;
                    }
                }
                i -= Snake::apply_direction(snake.tail(j));
            }
        }

        char obj_map_[H * W];
    };
    static int empty_id() { return 0; }
    static int snake_id(int si) { return (1 + si); }
    static int snake_mask(int si) { return 1 << si; }
    static int gadget_id(int i) { return (1 + i + SnakeCount); }
    static int gadget_mask(int i) { return 1 << (SnakeCount + i); }

    State() :
        win_(0),
        fruit_((1 << FruitCount) - 1) {
        for (int i = 0; i < GadgetCount; ++i) {
            gadget_offset_[i] = 0;
        }
    }

    State(const Map& map) : State() {
        for (int i = 0; i < SnakeCount; ++i) {
            snakes_[i] = map.snakes_[i];
        }
        for (int i = 0; i < GadgetCount; ++i) {
            gadget_offset_[i] = map.gadget_offset_[i];
        }
    }

    void print(const Map& map) const {
        ObjMap obj_map(*this, map, true);

#if 0
        for (auto snake : snakes_) {
            printf("len=%d, i=%d, tail=", snake.len_, snake.i_);
            for (int j = 0; j < snake.len_ - 1; ++j) {
                printf(" %d", snake.tail(j));
            }
            printf("\n");
        }
#endif

        for (int i = 0; i < H; ++i) {
            for (int j = 0; j < W; ++j) {
                int l = i * W + j;
                bool teleport = false;
                for (auto t : map.teleporters_) {
                    if (t.first == l || t.second == l) {
                        teleport = true;
                    }
                }
                if (!obj_map.no_object_at(l)) {
                    int id = obj_map.id_at(l);
                    char c;
                    if (id < (SnakeCount + 1)) {
                        c = 'A' + (id - 1);
                    } else if (id < (SnakeCount + GadgetCount + 1)) {
                        c = '0' + (id - 1 - SnakeCount);
                    } else if (id == obj_map.fruit_id()) {
                        c = 'Q';
                    } else {
                        c = id;
                    }
                    printf("%c", c);
                } else if (l == map.exit_) {
                    printf("*");
                } else if (teleport) {
                    printf("X");
                } else {
                    printf("%c", map[l]);
                }
            }
            printf("\n");
        }
        printf("\n");
    }

    void delete_fruit(int i) {
        fruit_ = fruit_ & ~(1 << i);
    }

    bool fruit_active(int i) const {
        return (fruit_ & (1 << i)) != 0;
    }

    void do_valid_moves(const Map& map,
                        std::function<bool(State,
                                           const Snake&,
                                           Direction)> fun) const {
        static Direction dirs[] = {
            UP, RIGHT, DOWN, LEFT,
        };
        ObjMap obj_map(*this, map, false);
        uint32_t tele_mask = teleporter_overlap(map, obj_map);
        for (int si = 0; si < SnakeCount; ++si) {
            if (!snakes_[si].len_) {
                continue;
            }
            // There has to be a cleaner way to do this...
            State push_st(*this);
            push_st.snakes_[si].len_--;
            ObjMap push_map(push_st, map, false);
            for (auto dir : dirs) {
                int delta = Snake::apply_direction(dir);
                int to = snakes_[si].i_ + delta;
                int pushed_ids = 0;
                int fruit_index = 0;
                if (is_valid_grow(map, to, &fruit_index)) {
                    State new_state(*this);
                    new_state.snakes_[si].grow(dir);
                    new_state.delete_fruit(fruit_index);
                    if (new_state.process_gravity(map, tele_mask)) {
                        new_state.canonicalize();
                        if (fun(new_state, snakes_[si], dir)) {
                            return;
                        }
                    }
                } if (is_valid_move(map, obj_map, to)) {
                    State new_state(*this);
                    new_state.snakes_[si].move(dir);
                    if (new_state.process_gravity(map, tele_mask)) {
                        new_state.canonicalize();
                        if (fun(new_state, snakes_[si], dir)) {
                            return;
                        }
                    }
                } else if (is_valid_push(map, push_map,
                                         snake_id(si),
                                         snakes_[si].i_,
                                         delta,
                                         &pushed_ids) &&
                           !(pushed_ids & snake_mask(si))) {
                    State new_state(*this);
                    new_state.snakes_[si].move(dir);
                    new_state.do_pushes(obj_map, pushed_ids, delta);
                    // printf("++\n");
                    // print(map);
                    // new_state.print(map);
                    if (new_state.process_gravity(map, tele_mask)) {
                        new_state.canonicalize();
                        if (fun(new_state, snakes_[si], dir)) {
                            return;
                        }
                    }
                }
            }
        }
    }

    void canonicalize() {
        std::sort(&snakes_[0], &snakes_[SnakeCount]);
    }

    uint32_t teleporter_overlap(const Map& map, const ObjMap& objmap) const {
        uint32_t mask = 0;
        const uint32_t width = SnakeCount + GadgetCount;
        for (int i = 0; i < TeleporterCount; ++i) {
            mask |=
                ((objmap.mask_at(map.teleporters_[i].first)) |
                 (objmap.mask_at(map.teleporters_[i].second) << width))
                << (width * 2 * i);
        }
        return mask;
    }

    void do_pushes(const ObjMap& obj_map, int pushed_ids, int push_delta) {
        for (int i = 0; i < SnakeCount; ++i) {
            if (pushed_ids & snake_mask(i)) {
                snakes_[i].i_ += push_delta;
            }
        }
        for (int i = 0; i < GadgetCount; ++i) {
            if (pushed_ids & gadget_mask(i)) {
                gadget_offset_[i] += push_delta;
            }
        }
    }

    bool destroy_if_intersects_hazard(const Map& map,
                                      const ObjMap& obj_map,
                                      int pushed_ids) {
        for (int si = 0; si < SnakeCount; ++si) {
            if (pushed_ids & snake_mask(si)) {
                if (snake_intersects_hazard(map, snakes_[si]))
                    return true;
            }
        }
        for (int i = 0; i < GadgetCount; ++i) {
            if (pushed_ids & gadget_mask(i)) {
                if (gadget_intersects_hazard(map, i)) {
                    gadget_offset_[i] = kGadgetDeleted;
                }
            }
        }
        return false;
    }

    bool is_valid_grow(const Map& map,
                       int to,
                       int* fruit_index) const {
        for (int i = 0; i < FruitCount; ++i) {
            int fruit = map.fruit_[i];
            if (fruit_active(i) &&
                fruit == to) {
                *fruit_index = i;
                return true;
            }
        }

        return false;
    }

    bool is_valid_move(const Map& map,
                       const ObjMap& obj_map,
                       int to) const {
        if (obj_map.no_object_at(to) && empty_terrain_at(map, to)) {
            return true;
        }

        return false;
    }

    bool empty_terrain_at(const Map& map, int i) const {
        return map[i] == ' ';
    }

    bool is_valid_push(const Map& map,
                       const ObjMap& obj_map,
                       int pusher_id,
                       int push_at,
                       int delta,
                       int* pushed_ids) const {
        int to = push_at + delta;

        if (obj_map.no_object_at(to) ||
            obj_map.id_at(to) == pusher_id ||
            obj_map.fruit_at(to)) {
            return false;
        }

        *pushed_ids = obj_map.mask_at(to);
        bool again = true;

        while (again) {
            again = false;
            for (int si = 0; si < SnakeCount; ++si) {
                if (*pushed_ids & snake_mask(si)) {
                    int new_pushed_ids = 0;
                    if (!snake_can_be_pushed(map, obj_map,
                                             si,
                                             delta,
                                             &new_pushed_ids)) {
                        return false;
                    }
                    if (new_pushed_ids & ~(*pushed_ids)) {
                        *pushed_ids |= new_pushed_ids;
                        again = true;
                    }
                }
            }
            for (int i = 0; i < GadgetCount; ++i) {
                if (*pushed_ids & gadget_mask(i)) {
                    int new_pushed_ids = 0;
                    if (!gadget_can_be_pushed(map,
                                              obj_map,
                                              i,
                                              delta,
                                              &new_pushed_ids)) {
                        return false;
                    }
                    if (new_pushed_ids & ~(*pushed_ids)) {
                        *pushed_ids |= new_pushed_ids;
                        again = true;
                    }
                }
            }
        }

        return true;
    }

    bool snake_can_be_pushed(const Map& map,
                             const ObjMap& obj_map,
                             int si,
                             int delta,
                             int* pushed_ids) const {
        const Snake& snake = snakes_[si];
        // The space the Snake's head would be pushed to.
        int to = snake.i_ + delta;

        for (int j = 0; j < snake.len_; ++j) {
            if (!empty_terrain_at(map, to)) {
                return false;
            }
            if (obj_map.fruit_at(to)) {
                return false;
            }
            if (obj_map.foreign_object_at(to, snake_id(si))) {
                *pushed_ids |= obj_map.mask_at(to);
            }
            to -= Snake::apply_direction(snake.tail(j));
        }

        return true;
    }

    bool gadget_can_be_pushed(const Map& map,
                              const ObjMap& obj_map,
                              int gadget_index,
                              int delta,
                              int* pushed_ids) const {
        const auto& gadget = map.gadgets_[gadget_index];
        int offset = gadget_offset_[gadget_index];

        for (int j = 0; j < gadget.size_; ++j) {
            int i = gadget.i_[j] + offset + delta;
            if (!empty_terrain_at(map, i)) {
                return false;
            }
            if (obj_map.fruit_at(i)) {
                return false;
            }
            if (!obj_map.no_object_at(i)) {
                *pushed_ids |= obj_map.mask_at(i);
            }
        }

        return true;
    }

    bool process_teleports(const Map& map, const ObjMap& obj_map,
                           uint32_t orig_tele_mask,
                           uint32_t new_tele_mask) {
        uint32_t only_new = new_tele_mask & ~orig_tele_mask;
        int test = 1;
        bool teleported = false;
        // This is over-engineered for the possibility of multiple
        // teleporters. But those don't actually appear in the game,
        // and there are some interesting semantic problems with them
        // with two different teleporter pairs being triggered at the
        // same time, so it's just a guess that this is how they'd
        // work.
        for (int i = 0; i < TeleporterCount; ++i) {
            int delta = map.teleporters_[i].second -
                map.teleporters_[i].first;
            for (int dir = 0; dir < 2; ++dir) {
                for (int si = 0; si < SnakeCount; ++si) {
                    if (test & only_new) {
                        if (try_snake_teleport(map, obj_map, si, delta)) {
                            teleported = true;
                        }
                    }
                    test <<= 1;
                }
                for (int gi = 0; gi < GadgetCount; ++gi) {
                    if (test & only_new) {
                        if (try_gadget_teleport(map, obj_map, gi, delta)) {
                            teleported = true;
                        }
                    }
                    test <<= 1;
                }
                // Delta was from A to B, just negate it for
                // handling the B to A case.
                delta = -delta;
            }
        }

        return teleported;
    }

    bool try_snake_teleport(const Map& map,
                            const ObjMap& obj_map,
                            int si, int delta) {
        const Snake& snake = snakes_[si];
        // The space where the head teleports to
        int to = snake.i_ + delta;

        for (int j = 0; j < snake.len_; ++j) {
            if (map[to] != ' ') {
                return false;
            }
            // If segment X of a snake would teleport into the space
            // occupied by segment Y of the same snake pre-teleport,
            // is the teleport blocked? I'm assuming yes. If not,
            // this should be a foreign_object_at instead.
            if (!obj_map.no_object_at(to)) {
                return false;
            }

            to -= Snake::apply_direction(snake.tail(j));
        }

        snakes_[si].i_ += delta;
        return true;
    }

    bool try_gadget_teleport(const Map& map,
                             const ObjMap& obj_map,
                             int gi,
                             int delta) {
        const auto& gadget = map.gadgets_[gi];
        int offset = gadget_offset_[gi] + delta;

        for (int j = 0; j < gadget.size_; ++j) {
            int to = gadget.i_[j] + offset;
            if (map[to] != ' ') {
                return false;
            }
            if (!obj_map.no_object_at(to)) {
                return false;
            }
        }

        // There's a funny thing here where a sparse gadget could
        // theoretically teleport halfway over a map edge, since
        // the solid border tile protection doesn't work there.
        // But if a solution ended up abusing that, it'd be easy to
        // fix by just adding more padding to the map.

        gadget_offset_[gi] += delta;
        return true;
    }

    bool process_gravity(const Map& map, uint32_t orig_tele_mask) {
    again:
        // FIXME. Figure out if exits and teleporters have different
        // priority. Is it possible to construct a case where that
        // matters?
        check_exits(map);
        // FIXME: The teleporter + gravity interaction doesn't quite
        // match the actual game. There you can have the scenario where
        // snake A supports snake B. Then:
        // 1. A moves, and goes through a teleporter
        // 2. Both A and B drop due to gravity
        // 3. B is now on a teleporter. The remote side is not blocked
        //    by A. But B does not teleport.
        // Constructing more exact test cases is proving tricky.
        ObjMap obj_map(*this, map, false);
        uint32_t new_tele_mask = teleporter_overlap(map, obj_map);
        if (new_tele_mask & ~orig_tele_mask) {
            if (process_teleports(map, obj_map, orig_tele_mask,
                                  new_tele_mask)) {
                orig_tele_mask = teleporter_overlap(map,
                                                    ObjMap(*this, map, false));
                goto again;
            }
        }
        orig_tele_mask = new_tele_mask;

        for (int si = 0; si < SnakeCount; ++si) {
            if (snakes_[si].len_) {
                int falling = is_snake_falling(map, obj_map, si);
                if (falling) {
                    do_pushes(obj_map, falling, W);
                    if (destroy_if_intersects_hazard(map, obj_map, falling))
                        return false;
                    goto again;
                }
            }
        }

        for (int i = 0; i < GadgetCount; ++i) {
            int offset = gadget_offset_[i];
            if (offset != kGadgetDeleted) {
                int falling = is_gadget_falling(map, obj_map, i);
                if (falling) {
                    do_pushes(obj_map, falling, W);
                    if (destroy_if_intersects_hazard(map, obj_map, falling))
                        return false;
                    goto again;
                }
            }
        }

        return true;
    }

    void check_exits(const Map& map) {
        if (fruit_) {
            // Can't use exits until all fruit are eaten.
            return;
        }

        for (auto& snake : snakes_) {
            if (snake.len_) {
                if (snake_head_at_exit(map, snake)) {
                    snake.len_ = 0;
                    snake.i_ = 0;
                    snake.tail_ = 0;
                    update_win();
                }
            }
        }
    }

    void update_win() {
        for (int si = 0; si < SnakeCount; ++si) {
            if (snakes_[si].len_) {
                win_ = 0;
                return;
            }
        }

        win_ = 1;
    }

    int is_snake_falling(const Map& map,
                         const ObjMap& obj_map,
                         int si) const {
        const Snake& snake = snakes_[si];
        // The space below the snake's head.
        int below = snake.i_ + W;
        int pushed_ids = snake_mask(si);

        for (int j = 0; j < snake.len_; ++j) {
            if (map[below] == '.') {
                return 0;
            }
            if (obj_map.foreign_object_at(below, snake_id(si))) {
                int new_pushed_ids = 0;
                if (is_valid_push(map, obj_map,
                                  snake_id(si),
                                  below - W, W,
                                  &new_pushed_ids)) {
                    pushed_ids |= new_pushed_ids;
                } else {
                    return 0;
                }
            }

            below -= Snake::apply_direction(snake.tail(j));
        }

        return pushed_ids;
    }

    int is_gadget_falling(const Map& map,
                          const ObjMap& obj_map,
                          int gadget_index) const {
        const auto& gadget = map.gadgets_[gadget_index];

        int pushed_ids = gadget_mask(gadget_index);
        int id = gadget_id(gadget_index);

        for (int j = 0; j < gadget.size_; ++j) {
            int at = gadget.i_[j] + gadget_offset_[gadget_index];
            int below = at + W;
            if (map[below] == '.') {
                return 0;
            }
            if (map[below] == '#') {
                return 0;
            }
            if (obj_map.foreign_object_at(below, id)) {
                int new_pushed_ids = 0;
                if (is_valid_push(map, obj_map, id, at, W, &new_pushed_ids)) {
                    pushed_ids |= new_pushed_ids;
                } else {
                    return 0;
                }
            }
        }

        return pushed_ids;
    }

    bool snake_head_at_exit(const Map& map, const Snake& snake) const {
        // Only the head of the snake will trigger an exit
        return snake.i_ == map.exit_;
    }

    bool snake_intersects_hazard(const Map& map, const Snake& snake) const {
        int i = snake.i_;
        for (int j = 0; j < snake.len_; ++j) {
            if (map[i] == '~' || map[i] == '#')
                return true;
            i -= Snake::apply_direction(snake.tail(j));
        }

        return false;
    }

    bool gadget_intersects_hazard(const Map& map,
                                  int gadget_index) const {
        int offset = gadget_offset_[gadget_index];
        if (offset == kGadgetDeleted)
            return false;
        const auto& gadget = map.gadgets_[gadget_index];
        for (int j = 0; j < gadget.size_; ++j) {
            // Spikes aren't a hazard for gadgets.
            if (map[gadget.i_[j] + offset] == '~')
                return true;
        }

        return false;
    }

    bool operator==(const State& other) const {
        for (int i = 0; i < SnakeCount; ++i) {
            if (!(snakes_[i] == other.snakes_[i])) {
                return false;
            }
        }
        for (int i = 0; i < GadgetCount; ++i) {
            if (gadget_offset_[i] != other.gadget_offset_[i]) {
                return false;
            }
        }
        if (win_ != other.win_ ||
            fruit_ != other.fruit_) {
            return false;
        }
        return true;
    }

    // -1 for the win_ flag.
    using Flags32 = typename std::conditional<FruitCount <= 32 - 1, uint32_t, uint64_t>::type;
    using Flags16 = typename std::conditional<FruitCount <= 16 - 1, uint16_t, Flags32>::type;
    using Flags = typename std::conditional<FruitCount <= 8 - 1, uint8_t, Flags16>::type;

    Snake snakes_[SnakeCount];
    int16_t gadget_offset_[GadgetCount];
    Flags win_ : 1,
          fruit_ : (sizeof(Flags) * 8 - 1);
} __attribute__((packed));

template<class T>
struct hash {
    uint64_t operator()(const T& key) const {
        return CityHash64(reinterpret_cast<const char*>(&key),
                          sizeof(key));
        // return CityHash64(reinterpret_cast<const char*>(&key.snakes_),
        //                   sizeof(key.snakes_));
        // CityHash64(reinterpret_cast<const char*>(&key.fruit_),
        //            sizeof(key.fruit_));
        // const uint32_t* data = reinterpret_cast<const uint32_t*>(&key);
        // return xxhash32<sizeof(key) / 4>::scalar(data, 0x8fc5249f);
    }
};

template<class T>
struct eq {
    bool operator()(const T& a, const T& b) const {
        return a == b;
    }
};

template<class St, class Map>
int search(St start_state, const Map& map) {
    printf("%ld\n", sizeof(St));
    St null_state;

    // Just in case the starting state is invalid.
    start_state.process_gravity(map, 0);

    // BFS state
    std::deque<St> todo;
    // std::unordered_map<St, St, hash<St>, eq<St>> seen_states;
    // google::dense_hash_map<St, St, hash<St>, eq<St>> seen_states;
    // seen_states.set_empty_key(null_state);
    google::sparse_hash_map<St, St, hash<St>, eq<St>> seen_states;
    size_t steps = 0;
    St win_state = null_state;
    bool win = false;

    todo.push_back(start_state);
    seen_states[start_state] = null_state;

    while (!todo.empty() && !win) {
        auto st = todo.front();
        todo.pop_front();

        ++steps;
        if (!(steps & 0xffff)) {
            printf(".");
            fflush(stdout);
        }

        // printf("%lx [%lx]\n", hash<St>()(st),
        //        hash<St>()(seen_states[st]));
        // st.print(map);

        st.do_valid_moves(map,
                          [&st, &todo, &seen_states, &win, &map,
                           &win_state](St new_state,
                                       const typename St::Snake& snake,
                                       Direction dir) {
                if (seen_states.find(new_state) != seen_states.end()) {
                    return false;
                }

                seen_states[new_state] = st;
                todo.push_back(new_state);

                if (new_state.win_) {
                    win_state = new_state;
                    win = true;
                    return true;
                }
                return false;
            });
    }

    printf("%s\n",
           win ? "Win" : "No solution");

    int moves = 0;
    if (win) {
        while (!(win_state == start_state)) {
            win_state.print(map);
            win_state = seen_states[win_state];
            ++moves;
        }
    }

    printf("%ld states, %d moves\n", steps, moves);

    return moves;
}

#include "level00.h"
#include "level01.h"
#include "level02.h"
#include "level03.h"
#include "level04.h"
#include "level05.h"
#include "level06.h"
#include "level07.h"
#include "level08.h"
#include "level09.h"
#include "level10.h"
#include "level11.h"
#include "level12.h"
#include "level13.h"
#include "level14.h"
#include "level15.h"
#include "level16.h"
#include "level17.h"
#include "level18.h"
#include "level19.h"
#include "level20.h"
#include "level21.h"
#include "level22.h"
#include "level23.h"
#include "level24.h"
#include "level25.h"
#include "level26.h"
#include "level27.h"
#include "level28.h"
#include "level29.h"
#include "level30.h"
#include "level31.h"
#include "level32.h"
#include "level33.h"
#include "level34.h"
#include "level35.h"
#include "level36.h"
#include "level37.h"
#include "level38.h"
#include "level39.h"
#include "level40.h"
#include "level41.h"
#include "level42.h"
#include "level43.h"
#include "level44.h"
#include "level45.h"
#include "levelstar1.h"
#include "levelstar2.h"
#include "levelstar3.h"
#include "levelstar4.h"
#include "levelstar5.h"
#include "levelstar6.h"

int main() {
#define EXPECT_EQ(wanted, actual)                                       \
    do {                                                                \
        auto tmp = actual;                                              \
        if (tmp != wanted) {                                            \
            fprintf(stderr, "Error: expected %s => %d, got %d\n", #actual, wanted, tmp); \
        } \
    } while (0);

    EXPECT_EQ(29, level_00());
    EXPECT_EQ(16, level_01());
    EXPECT_EQ(25, level_02());
    EXPECT_EQ(27, level_03());
    EXPECT_EQ(30, level_04());
    EXPECT_EQ(24, level_05());
    EXPECT_EQ(36, level_06());
    EXPECT_EQ(43, level_07());
    EXPECT_EQ(29, level_08());
    EXPECT_EQ(37, level_09());
    EXPECT_EQ(33, level_10());
    EXPECT_EQ(35, level_11());
    EXPECT_EQ(52, level_12());
    EXPECT_EQ(44, level_13());
    EXPECT_EQ(24, level_14());
    EXPECT_EQ(34, level_15());
    EXPECT_EQ(65, level_16());
    EXPECT_EQ(68, level_17());
    EXPECT_EQ(35, level_18());
    EXPECT_EQ(47, level_19());
    EXPECT_EQ(50, level_20());
    EXPECT_EQ(39, level_21());
    EXPECT_EQ(45, level_22());
    EXPECT_EQ(53, level_23());
    EXPECT_EQ(26, level_24());
    EXPECT_EQ(35, level_25());
    EXPECT_EQ(35, level_26());
    EXPECT_EQ(49, level_27());
    EXPECT_EQ(49, level_28());
    // // OOM
    // EXPECT_EQ(0, level_29());
    EXPECT_EQ(15, level_30());
    EXPECT_EQ(8, level_31());
    EXPECT_EQ(21, level_32());
    EXPECT_EQ(42, level_33());
    EXPECT_EQ(17, level_34());
    EXPECT_EQ(29, level_35());
    EXPECT_EQ(29, level_36());
    EXPECT_EQ(16, level_37());
    EXPECT_EQ(28, level_38());
    EXPECT_EQ(53, level_39());
    EXPECT_EQ(51, level_40());
    EXPECT_EQ(34, level_41());
    EXPECT_EQ(42, level_42());
    EXPECT_EQ(36, level_43());
    EXPECT_EQ(36, level_44());
    EXPECT_EQ(77, level_45());
    // OOM
    // EXPECT_EQ(0, level_star1());
    EXPECT_EQ(60, level_star2());
    // OOM
    // EXPECT_EQ(0, level_star3());
    EXPECT_EQ(44, level_star4());
    EXPECT_EQ(69, level_star5());
    // OOM
    // EXPECT_EQ(0, level_star6());

    return 0;
}
