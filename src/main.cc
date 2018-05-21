#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <third-party/cityhash/city.h>
#include <vector>

#include "bit-packer.h"
#include "compress.h"
#include "file-backed-array.h"
#include "game.h"

template<class Key, class Value>
class Pair {
public:
    Pair() {
    }

    Pair(const Key& key, Value value)
        : key_(key), value_(value) {
    }

    bool operator<(const Key& key) const {
        return key_ < key;
    }
    bool operator==(const Key& key) const {
        return key_ == key;
    }

    bool operator<(const Pair& other) const {
        return key_ < other.key_;
    }
    bool operator==(const Pair& other) const {
        return key_ == other.key_;
    }

    uint64_t hash() const {
        return CityHash64((char*) key_.p_.bytes_,
                          sizeof(key_.p_.bytes_));
    }

    Key key_;
    Value value_;
};

template<class K, class V,
         class Keys, class Values,
         class NewKeys, class NewValues>
size_t dedup(Keys* seen_keys, Values* seen_values,
             const NewKeys& new_keys,
             const NewValues &new_values) {
    std::vector<bool> discard(new_keys.size());

    for (int run = 0; run < seen_keys->runs(); ++run) {
        auto runinfo = seen_keys->run(run);
        SortedStructDecompressor<sizeof(K::p_.bytes_)> stream(
            seen_keys->begin() + runinfo.first,
            seen_keys->begin() + runinfo.second);
        auto it = new_keys.begin();

        K key;
        for (size_t i = 0; stream.unpack(key.p_.bytes_); ) {
            while (it != new_keys.end() && *it < key) {
                ++it;
                ++i;
            }
            if (key == *it) {
                discard[i] = true;
            }
        }
    }

    SortedStructCompressor<sizeof(K::p_.bytes_), Keys> compress;

    size_t count = 0;
    seen_values->start_run();
    for (int i = 0; i < discard.size(); ++i) {
        if (!discard[i]) {
            ++count;
            compress.pack(new_keys[i].p_.bytes_);
            seen_values->push_back(new_values[i]);
        }
    }
    seen_values->end_run();

    seen_keys->thaw();
    seen_keys->start_run();
    compress.write(seen_keys);
    seen_keys->end_run();
    seen_keys->freeze();

    return count;
}

template<class T>
T* sort_dedup(T* start, T* end) {
    auto copy = new T[std::distance(start, end)];
    auto copy_end = copy + std::distance(start, end);
    std::copy(start, end, copy);
    std::sort(copy, copy_end);
    copy_end = std::unique(copy, copy_end);
    std::copy(copy, copy_end, start);
    delete[] copy;
    return start + std::distance(copy, copy_end);
}

template<class St, class Map>
int search(St start_state, const Map& map) {
    using Packed = typename St::Packed;
    using st_pair = Pair<Packed, uint8_t>;

    using Keys = file_backed_mmap_array<uint8_t>;
    using Values = file_backed_mmap_array<uint8_t>;
    using NewStates = file_backed_mmap_array<st_pair>;

    // Just in case the starting state is invalid.
    start_state.process_gravity(map, 0);

    St null_state;

    // BFS state
    Keys seen_keys;
    Values seen_values;
    size_t steps = 0;
    st_pair win_state = st_pair(null_state, 0);
    bool win = false;

    size_t shards = 1;

    std::vector<NewStates> outputs { shards };

    printf("bits=%ld packed_bytes=%ld\n",
           St::packed_width(),
           sizeof(Packed));

    {
        auto pair = st_pair(start_state, 0xfe);
        outputs[0].push_back(st_pair(start_state, 0));
        St(pair.key_).print(map);
    }
    seen_keys.freeze();

    size_t total_states = 0;
    size_t depth = 0;
    double dedup_flush_s = 0, dedup_sort_s = 0, dedup_merge_shards_s = 0,
        dedup_merge_s = 0, search_s = 0, print_s = 0;
    while (1) {
        size_t seen_states_size = 0;
        size_t new_states_size = 0;
        for (auto& output : outputs) {
            MeasureTime<> timer(&dedup_flush_s);
            output.flush();
        }
        size_t state_bytes = 0;

        for (auto& new_states : outputs) {
            printf(","); fflush(stdout);
            {
                MeasureTime<> timer(&dedup_flush_s);
                new_states.snapshot();
            }

            total_states += new_states.size();
            new_states_size += new_states.size();

            st_pair* new_end = NULL;

            // Sort and dedup the shard.
            MeasureTime<> timer(&dedup_sort_s);
            new_end = sort_dedup(new_states.begin(), new_states.end());
            new_states.resize(new_end - new_states.begin());
        }

        file_backed_mmap_array<Packed> new_keys;
        file_backed_mmap_array<uint8_t> new_values;
        {
            printf(";"); fflush(stdout);
            MeasureTime<> timer(&dedup_merge_shards_s);
            MultiMerge<st_pair> merge_shards(
                [&new_keys, &new_values] (const st_pair& pair) {
                    new_keys.push_back(pair.key_);
                    new_values.push_back(pair.value_);
                });
            for (auto& o : outputs) {
                merge_shards.add_input_source(o.begin(), o.end());
            }
            merge_shards.merge();
            for (auto& o : outputs) {
                o.reset();
            }
            new_keys.freeze();
            new_values.freeze();
        }

        size_t new_unique = 0;
        {
            printf(":"); fflush(stdout);
            MeasureTime<> timer(&dedup_merge_s);
            new_unique = dedup<Packed, uint8_t>(&seen_keys, &seen_values,
                                                new_keys,
                                                new_values);
            seen_states_size = seen_values.size();
            state_bytes = seen_keys.size();
            new_keys.reset();
            new_values.reset();
        }

        if (new_states_size / shards > 100000000) {
            shards *= 2;
            outputs.resize(shards);
        } else if (shards > 1 && new_states_size / shards < 10000000) {
            shards /= 2;
            outputs.resize(shards);
            outputs.shrink_to_fit();
        }

        printf("depth: %ld unique: %ld, delta %ld (total: %ld, delta %ld), bytes: %ld\n",
               depth++,
               seen_states_size, new_unique,
               total_states, new_states_size,
               state_bytes);
        printf("timing: dedup: %lfs+%lfs+%lfs+%lfs search: %lfs = total: %lfs\n",
               dedup_flush_s, dedup_sort_s, dedup_merge_shards_s,
               dedup_merge_s,
               search_s,
               dedup_flush_s + dedup_sort_s + dedup_merge_shards_s +
               dedup_merge_s + search_s);

        if (win || !new_unique) {
            break;
        }

        MeasureTime<> timer(&search_s);

        auto torun = seen_keys.run(seen_keys.runs() - 1);
        SortedStructDecompressor<sizeof(Packed::p_.bytes_)> stream(
            seen_keys.begin() + torun.first,
            seen_keys.begin() + torun.second);

        Packed packed;
        while (stream.unpack(packed.p_.bytes_)) {
            St st(packed);

            if (!(++steps & 0x7ffff)) {
                printf(".");
                fflush(stdout);
            }
            auto parent_hash = st_pair(packed, 0).hash();

            st.do_valid_moves(map,
                              [&depth, &outputs, &win, &map, &parent_hash,
                               &shards,
                               &win_state](St new_state,
                                           int si,
                                           Direction dir) {
                                  new_state.canonicalize(map);
                                  st_pair pair(new_state,
                                               parent_hash & 0xff);

                                  auto hash = pair.hash();
                                  outputs[hash & (shards - 1)].push_back(pair);
                                  if (new_state.win()) {
                                      win_state = pair;
                                      win = true;
                                      return true;
                                  }
                                  return false;
                              });
        }
    }

    printf("%s\n",
           win ? "Win" : "No solution");

    size_t total_bytes = 0;
    if (win) {
        MeasureTime<> timer(&print_s);
        St(win_state.key_).print(map);

        st_pair target = win_state;

        seen_keys.freeze();
        seen_values.freeze();
        total_bytes += seen_keys.size();
        total_bytes += seen_values.size();

        for (int i = depth - 1; i > 0; --i) {
            auto runinfo = seen_keys.run(i - 1);
            SortedStructDecompressor<sizeof(Packed::p_.bytes_)> stream(
                seen_keys.begin() + runinfo.first,
                seen_keys.begin() + runinfo.second);
            printf("Move %d\n", i);

            Packed key;
            for (int j = 0; stream.unpack(key.p_.bytes_); ++j) {
                st_pair current(key, 0);
                if ((current.hash() & 0xff) != (target.value_ & 0xff)) {
                    continue;
                }
                St st(key);
                if (st.do_valid_moves(map,
                                      [&target, &map](St new_state,
                                                      int si,
                                                      Direction dir) {
                                          new_state.canonicalize(map);
                                          Packed p(new_state);
                                          if (p == target.key_) {
                                              return true;
                                          }
                                          return false;
                                      })) {
                    const auto& value = seen_values[seen_values.run(i - 1).first + j];
                    target = st_pair(key, value);
                    st.print(map);
                    break;
                }
            }
        }
    }

    printf("%ld states, %ld moves, %ld bytes\n", steps, depth,
           total_bytes);
    printf("timing: dedup: %lfs search: %lfs print: %lfs= total: %lfs\n",
           dedup_sort_s + dedup_merge_shards_s + dedup_merge_s,
           search_s, print_s,
           dedup_sort_s + dedup_merge_shards_s + dedup_merge_s +
           search_s + print_s);

    return win ? depth - 1 : 0;
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
#include "levelvoid.h"

int main() {
#define EXPECT_EQ(wanted, actual)                                       \
    do {                                                                \
        printf("Running %s\n", #actual);                                \
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
    EXPECT_EQ(45, level_29());
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
    EXPECT_EQ(75, level_star1());   // 5GB, 20 minutes
    EXPECT_EQ(60, level_star2());
    EXPECT_EQ(63, level_star3());   // 2.2GB
    EXPECT_EQ(44, level_star4());
    EXPECT_EQ(69, level_star5());
    EXPECT_EQ(90, level_star6());   // 40GB, 10 hours
    EXPECT_EQ(52, level_void());


    return 0;
}
