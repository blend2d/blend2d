#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <atomic>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// This is a stupid hash function finder that maps uint32_t inputs into a set of predefined consecutive IDs.
//
// Why stupid? Because it's a brute force approach and totally anti computer science - there is no theory behind
// this except for trying to find a constant that when multiplied with input generates the least number of
// collisions.
namespace StupidHash {

// Utility Functions
// =================

static uint32_t alignUpToPowerOf2(uint32_t n) {
  n -= 1;

  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;

  return n + 1;
}

static uint32_t count_trailing_bits(uint32_t n) {
  for (uint32_t i = 0; i < 32; i++)
    if (n & (1u << i))
      return i;
  return 32;
}

static size_t replace_in_string(std::string& str, const std::string& pattern, const std::string& replacement) {
  size_t pos = 0;
  size_t count = 0;

  for (;;) {
    pos = str.find(pattern, pos);
    if (pos == std::string::npos)
      return count;
    str.replace(pos, pattern.length(), replacement);
    pos += replacement.length();
    count++;
  }
}

static inline uint32_t mul64Op1(uint32_t value, uint32_t adder, uint64_t multiplier, uint32_t shift) noexcept {
  return uint32_t(((value + adder) * multiplier) >> shift);
}

static inline uint32_t mulOp1(uint32_t value, uint32_t multiplier, uint32_t shift) noexcept {
  return (value * multiplier) >> shift;
}

static inline uint32_t mulOp2(uint32_t value, uint32_t multiplier, uint32_t shift) noexcept {
  return ((value * multiplier + (13 << shift)) >> shift);
}

// [Pseudo] Random Number Generator
// ================================

// A pseudo random number generator based on a paper by Sebastiano Vigna:
//   http://vigna.di.unimi.it/ftp/papers/xorshiftplus.pdf
class Random {
public:
  // Constants suggested as `23/18/5`.
  enum Steps : uint32_t {
    kStep1_SHL = 23,
    kStep2_SHR = 18,
    kStep3_SHR = 5
  };

  inline explicit Random(uint64_t seed = 0) noexcept { reset(seed); }
  inline Random(const Random& other) noexcept = default;

  inline void reset(uint64_t seed = 0) noexcept {
    // The number is arbitrary, it means nothing.
    constexpr uint64_t kZeroSeed = 0x1F0A2BE71D163FA0u;

    // Generate the state data by using splitmix64.
    for (uint32_t i = 0; i < 2; i++) {
      seed += 0x9E3779B97F4A7C15u;
      uint64_t x = seed;
      x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9u;
      x = (x ^ (x >> 27)) * 0x94D049BB133111EBu;
      x = (x ^ (x >> 31));
      _state[i] = x != 0 ? x : kZeroSeed;
    }
  }

  inline uint32_t next_uint32() noexcept {
    return uint32_t(next_uint64() >> 32);
  }

  inline uint64_t next_uint64() noexcept {
    uint64_t x = _state[0];
    uint64_t y = _state[1];

    x ^= x << kStep1_SHL;
    y ^= y >> kStep3_SHR;
    x ^= x >> kStep2_SHR;
    x ^= y;

    _state[0] = y;
    _state[1] = x;
    return x + y;
  }

  uint64_t _state[2];
};

// Bit Array
// =========

class BitArray {
  std::vector<size_t> _bits;
  enum : size_t { kBitWordSize = sizeof(size_t) * 8u };

public:
  inline void resize(size_t size) {
    size_t size_in_words = (size + kBitWordSize - 1) / kBitWordSize;
    _bits.resize(size_in_words);
    clear();
  }

  inline void clear() {
    std::fill(_bits.begin(), _bits.end(), size_t(0));
  }

  inline bool has_bit(size_t bit_index) const {
    size_t word_index = bit_index / kBitWordSize;
    size_t mask = size_t(1) << (bit_index % kBitWordSize);
    return (_bits[word_index] & mask) != 0;
  }

  inline void set_bit(size_t bit_index) {
    size_t word_index = bit_index / kBitWordSize;
    size_t mask = size_t(1) << (bit_index % kBitWordSize);
    _bits[word_index] |= mask;
  }
};

// Hash Function
// =============

class HashFunction {
public:
  struct Param {
    bool used;
    uint32_t multiplier;
    uint32_t shift;
    std::vector<uint32_t> table;
  };

  Param params[2];
  std::vector<std::pair<uint32_t, uint32_t>> remaining;

  std::string body(const std::string& prototype, const std::string& input_value, const std::string& check_id_before, const std::string& check_id_after) const {
    // Single hash table.
    const char function_template_1[] =
      "{\n"
      "  static const $TABLE_TYPE_A$ hash_table[$TABLE_SIZE_A$] = {\n"
      "$TABLE_VALUES_A$\n"
      "  };\n"
      "\n"
      "  uint32_t h1 = ($INPUT_VALUE$ * $HASH_MULTIPLIER_A$u) >> $HASH_SHIFT_A$u;\n"
      "  uint32_t i1 = hash_table[h1];\n"
      "  uint32_t index = 0xFFFFFFFFu;\n"
      "\n"
      "  if ($CHECK_ID_BEFORE$i1$CHECK_ID_AFTER$ == $INPUT_VALUE$)\n"
      "    index = i1;\n"
      "$REMAINING_CHECKS$"
      "\n"
      "  return index;\n"
      "}\n";

    // Single hash table, two hash functions.
    const char function_template_2[] =
      "{\n"
      "  static const $TABLE_TYPE_A$ hash_table[$TABLE_SIZE_A$] = {\n"
      "$TABLE_VALUES_A$\n"
      "  };\n"
      "\n"
      "  uint32_t h1 = ($INPUT_VALUE$ * $HASH_MULTIPLIER_A$u) >> $HASH_SHIFT_A$u;\n"
      "  uint32_t h2 = ($INPUT_VALUE$ * $HASH_MULTIPLIER_B$u) >> $HASH_SHIFT_B$u;\n"
      "\n"
      "  uint32_t i1 = hash_table[h1];\n"
      "  uint32_t i2 = hash_table[h2];\n"
      "\n"
      "  uint32_t index = 0xFFFFFFFFu;\n"
      "\n"
      "  if ($CHECK_ID_BEFORE$i1$CHECK_ID_AFTER$ == $INPUT_VALUE$)\n"
      "    index = i1;\n"
      "\n"
      "  if ($CHECK_ID_BEFORE$i2$CHECK_ID_AFTER$ == $INPUT_VALUE$)\n"
      "    index = i2;\n"
      "$REMAINING_CHECKS$"
      "\n"
      "  return index;\n"
      "}\n";

    // Two hash tables, two hash functions.
    const char function_template_3[] =
      "{\n"
      "  static const $TABLE_TYPE_A$ hashTable1[$TABLE_SIZE_A$] = {\n"
      "$TABLE_VALUES_A$\n"
      "  };\n"
      "\n"
      "  static const $TABLE_TYPE_B$ hashTable2[$TABLE_SIZE_B$] = {\n"
      "$TABLE_VALUES_B$\n"
      "  };\n"
      "\n"
      "  uint32_t h1 = ($INPUT_VALUE$ * $HASH_MULTIPLIER_A$u) >> $HASH_SHIFT_A$u;\n"
      "  uint32_t h2 = ($INPUT_VALUE$ * $HASH_MULTIPLIER_B$u) >> $HASH_SHIFT_B$u;\n"
      "\n"
      "  uint32_t i1 = hashTable1[h1];\n"
      "  uint32_t i2 = hashTable2[h2];\n"
      "\n"
      "  uint32_t index = 0xFFFFFFFFu;\n"
      "\n"
      "  if ($CHECK_ID_BEFORE$i1$CHECK_ID_AFTER$ == $INPUT_VALUE$)\n"
      "    index = i1;\n"
      "\n"
      "  if ($CHECK_ID_BEFORE$i2$CHECK_ID_AFTER$ == $INPUT_VALUE$)\n"
      "    index = i2;\n"
      "$REMAINING_CHECKS$"
      "\n"
      "  return index;\n"
      "}\n";

    std::string body;

    auto format_table = [](const std::vector<uint32_t>& t) {
      std::string s("    ");
      for (size_t i = 0; i < t.size(); i++) {
        if (i != 0) {
          if ((i % 24) == 0)
            s.append(",\n    ");
          else
            s.append(", ");
        }
        s.append(std::to_string(t[i] != 0xFFFFFFFFu ? t[i] : uint32_t(0u)));
      }
      return s;
    };

    auto value_type_of_table = [](const std::vector<uint32_t>& t) {
      uint32_t greatest = 0;
      for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == 0xFFFFFFFFu)
          continue;
        greatest = std::max(greatest, t[i]);
      }

      if (greatest > 65535u)
        return "uint32_t";
      else if (greatest > 255u)
        return "uint16_t";
      else
        return "uint8_t";
    };

    uint32_t bitMask0 = (1u << (32u - params[0].shift)) - 1u;
    uint32_t bitMask1 = (1u << (32u - params[1].shift)) - 1u;

    if (params[1].used)
      body = params[1].table.size() == 0 ? function_template_2 : function_template_3;
    else
      body = function_template_1;

    std::string remaining_checks;
    if (!remaining.empty()) {
      for (auto p : remaining) {
        std::string condition("\n"
                              "  if ($INPUT_VALUE$ == $KEY$)\n"
                              "    index = $KEY_VALUE$;\n");
        replace_in_string(condition, std::string("$INPUT_VALUE$"), input_value);
        replace_in_string(condition, std::string("$KEY$"), std::to_string(p.first));
        replace_in_string(condition, std::string("$KEY_VALUE$"), std::to_string(p.second));
        remaining_checks += condition;
      }
    }

    replace_in_string(body, std::string("$INPUT_VALUE$"), input_value);
    replace_in_string(body, std::string("$CHECK_ID_BEFORE$"), check_id_before);
    replace_in_string(body, std::string("$CHECK_ID_AFTER$"), check_id_after);

    replace_in_string(body, std::string("$HASH_MULTIPLIER_A$"), std::to_string(params[0].multiplier));
    replace_in_string(body, std::string("$HASH_MULTIPLIER_B$"), std::to_string(params[1].multiplier));
    replace_in_string(body, std::string("$HASH_SHIFT_A$"), std::to_string(params[0].shift));
    replace_in_string(body, std::string("$HASH_SHIFT_B$"), std::to_string(params[1].shift));
    replace_in_string(body, std::string("$HASH_MASK_A$"), std::to_string(bitMask0));
    replace_in_string(body, std::string("$HASH_MASK_B$"), std::to_string(bitMask1));
    replace_in_string(body, std::string("$TABLE_TYPE_A$"), value_type_of_table(params[0].table));
    replace_in_string(body, std::string("$TABLE_TYPE_B$"), value_type_of_table(params[1].table));
    replace_in_string(body, std::string("$TABLE_SIZE_A$"), std::to_string(params[0].table.size()));
    replace_in_string(body, std::string("$TABLE_SIZE_B$"), std::to_string(params[1].table.size()));
    replace_in_string(body, std::string("$TABLE_VALUES_A$"), format_table(params[0].table));
    replace_in_string(body, std::string("$TABLE_VALUES_B$"), format_table(params[1].table));
    replace_in_string(body, std::string("$REMAINING_CHECKS$"), remaining_checks);
    body = std::string(prototype) + " " + body;

    return body;
  }
};

template<typename Lambda>
static void run_async(Lambda&& fn, size_t thread_count) {
  std::vector<std::thread> threads;

  for (size_t thread_id = 0; thread_id < thread_count; thread_id++)
    threads.push_back(std::thread(fn));

  for (std::thread& thread : threads)
    thread.join();
}

// Hash Function Finder
// --------------------

class Finder {
public:
  const uint32_t* _values {};
  uint32_t _size {};
  HashFunction _hf {};

  Finder(const uint32_t* values, uint32_t size) {
    _values = values;
    _size = size;
  }
/*
  void try_another(uint32_t bucketCount1) {
    BitArray occupied1;
    BitArray occupied2;

    std::vector<uint32_t> hits;

    occupied1.resize(bucketCount1);
    occupied2.resize(bucketCount1);
    hits.resize(bucketCount1);

    const uint32_t* values = _values;
    uint32_t size = _size;
    uint32_t localBestCollisions1 = 0xFFFFFFFFu;
    uint32_t localBestCollisions2 = 0xFFFFFFFFu;
    uint32_t best_score = 0xFFFFFFFFu;

    uint32_t shift1 = 64 - count_trailing_bits(bucketCount1);
    Random r;

    uint64_t m1_base = r.next_uint64();
    uint32_t attempt = 0;

    for (;;) {
      uint64_t bestM1 = m1_base;

      uint64_t pattern1;
      uint64_t pattern2;

      if (attempt == 0) {
        pattern1 = 0x1;
        pattern2 = 0x1;
      }
      else if (attempt % 16 == 0) {
        pattern1 = r.next_uint64() & 0x00FF00FF00FF00FF;
        pattern2 = r.next_uint64() & 0xFF00FF00FF00FF00;
      }
      else {
        pattern1 = r.next_uint64() & 0xF;
        pattern2 = r.next_uint64() & 0xF000000000000000;
      }

      for (uint32_t a = 0; a < 100; a++) {
        for (uint32_t bit1 = 0; bit1 < 64; bit1++) {
          for (uint32_t bit2 = 0; bit2 < 64; bit2++) {
            uint64_t m1 = m1_base ^ (uint64_t(pattern1) << bit1)
                                 ^ (uint64_t(pattern2) >> bit2);
            uint32_t collisions = 0;
            uint32_t moreThan2Collisions = 0;

            occupied1.clear();
            occupied2.clear();
            uint32_t score = uint32_t(_size);

            for (size_t i = 0; i < size; i++) {
              uint32_t index = mul64Op1(values[i], 0, m1, shift1);
              if (occupied1.has_bit(index)) {
                collisions++;
                moreThan2Collisions += occupied2.has_bit(index);
                occupied2.set_bit(index);
                if (hits[index] > 1)
                  score += 1000;
                else
                  score += 1;
                hits[index]++;
              }
              else {
                occupied1.set_bit(index);
                hits[index] = 1;
                score--;
              }
            }

            if (score < best_score) {
              localBestCollisions1 = collisions;

              if (score < best_score)
                best_score = score;

              bestM1 = m1;
              attempt = 0;

              _hf.params[0].used = true;
              _hf.params[0].multiplier = m1;
              _hf.params[0].shift = shift1;

              // best_collisions = collisions;
              printf("  Found (mul=0x%08llX) (collisions=%u) %s\n", (unsigned long long)bestM1, collisions, moreThan2Collisions ? "" : "(max 2 collisions per bucket)");

              if (collisions == 0) {
                return;
              }
            }
          }
        }
      }

      if (m1_base == bestM1) {
        // m1_base += r.next_uint64();
        if (++attempt > 10000000) {
          printf("Maximum attempts reached\n");
          return;
        }
      }
      else {
        m1_base = bestM1;
      }
    }
  }
*/
  bool find_hash_function(uint32_t bucketCount1) {
    constexpr uint32_t thread_count = 30;
    constexpr uint32_t mStep = 0x00100000u;

    uint32_t mGlobal = 0;
    uint32_t mMaxGlobal = 0x7FFFFFFFu;

    uint32_t best_collisions = 0xFFFFFFFFu;
    uint32_t bucketCount2 = 0;
    std::mutex mutex;

    auto stop_workers = [&]() { mGlobal = 0xFFFFFFFFu; };

    auto next_multiplier_base = [&]() -> uint32_t {
      std::lock_guard<std::mutex> guard(mutex);
      if (mGlobal == 0xFFFFFFFFu)
        return 0xFFFFFFFFu;

      uint32_t m1_base = mGlobal;
      mGlobal += mStep;
      if (mGlobal >= mMaxGlobal)
        mGlobal = 0xFFFFFFFFu;

      return m1_base;
    };

    auto reset_multiplier = [&](uint32_t bucket_count, bool guess_max_global = false) {
      mGlobal = 0;
      uint32_t t = count_trailing_bits(bucket_count);
      if (t <= 5 || !guess_max_global)
        mMaxGlobal = 0x7FFFFFFFu;
      else
        mMaxGlobal = 0xFFFFFFFFu >> (t - 5);
    };

    printf("Finder::find_hash_function() - Trying to find a first hash function for %u values [%u buckets]\n", _size, bucketCount1);
    best_collisions = 0xFFFFFFFFu;
    reset_multiplier(bucketCount1, true);

    uint32_t shift1 = 32 - count_trailing_bits(bucketCount1);

    run_async([&]() {
      BitArray occupied1;
      BitArray occupied2;

      occupied1.resize(bucketCount1);
      occupied2.resize(bucketCount1);

      const uint32_t* values = _values;
      uint32_t size = _size;
      uint32_t localBestCollisions1 = 0xFFFFFFFFu;
      uint32_t localBestCollisions2 = 0xFFFFFFFFu;

      for (;;) {
        uint32_t m1_base = next_multiplier_base();
        if (m1_base == 0xFFFFFFFFu)
          return;

        for (uint32_t m1_index = 0; m1_index < mStep; m1_index++) {
          uint32_t m1 = m1_base + m1_index;
          uint32_t collisions = 0;
          uint32_t moreThan2Collisions = 0;

          occupied1.clear();
          occupied2.clear();

          for (size_t i = 0; i < size; i++) {
            uint32_t index = mulOp1(values[i], m1, shift1);
            if (occupied1.has_bit(index)) {
              collisions++;
              moreThan2Collisions += occupied2.has_bit(index);
              occupied2.set_bit(index);
            }
            occupied1.set_bit(index);
          }

          if (collisions < localBestCollisions2 && moreThan2Collisions == 0) {
            std::lock_guard<std::mutex> guard(mutex);
            localBestCollisions2 = collisions;
            printf("  Found 0x%08X (collisions=%u) - no third collision\n", m1, collisions);
          }

          if (collisions < localBestCollisions1) {
            std::lock_guard<std::mutex> guard(mutex);
            localBestCollisions1 = best_collisions;

            if (collisions < best_collisions) {
              _hf.params[0].used = true;
              _hf.params[0].multiplier = m1;
              _hf.params[0].shift = shift1;

              best_collisions = collisions;
              printf("  Found 0x%08X (collisions=%u)\n", m1, collisions);

              if (collisions == 0)
                stop_workers();
            }
          }
        }
      }
    }, thread_count);

    printf("Finder::find_hash_function() - Found a hash function with %u collision(s)\n", best_collisions);
    std::vector<std::pair<uint32_t, uint32_t>> remaining_pairs;
    std::vector<uint32_t> remaining_values;

    BitArray occupied1;
    occupied1.resize(bucketCount1);

    {
      for (uint32_t i = 0; i < _size; i++) {
        uint32_t index = (_values[i] * _hf.params[0].multiplier) >> _hf.params[0].shift;
        if (occupied1.has_bit(index)) {
          remaining_pairs.push_back(std::pair<uint32_t, uint32_t>(_values[i], i));
          remaining_values.push_back(_values[i]);
        }
        else {
          occupied1.set_bit(index);
        }
      }
    }

    // Try to find another hash function that would use the same table.
    bool m2_found = false;
    bool m2_same_bucket_table = false;

    // Don't create a secondary hash table for 1 value.
    if (remaining_values.size() == 1) {
      _hf.remaining = std::move(remaining_pairs);
      m2_found = true;
    }

    if (!m2_found && best_collisions > 0) {
      printf("Finder::find_hash_function() - Trying to find a second hash function using the same bucket list [%u buckets]\n", bucketCount1);

      reset_multiplier(bucketCount1);
      run_async([&]() {
        const uint32_t* values_data = remaining_values.data();
        size_t values_count = remaining_values.size();

        BitArray occupied;
        occupied.resize(bucketCount1);

        for (;;) {
          uint32_t m2_base = next_multiplier_base();
          if (m2_base == 0xFFFFFFFFu)
            return;

          for (uint32_t m2_index = 0; m2_index < mStep; m2_index++) {
            bool found = true;
            uint32_t m2 = m2_base + m2_index;
            occupied.clear();

            for (size_t i = 0; i < values_count; i++) {
              uint32_t index = mulOp2(values_data[i], m2, shift1);
              if (occupied1.has_bit(index) || occupied.has_bit(index)) {
                found = false;
                break;
              }
              occupied.set_bit(index);
            }

            if (found) {
              std::lock_guard<std::mutex> guard(mutex);
              if (!m2_found) {
                printf("FOUND\n");
                m2_found = true;
                m2_same_bucket_table = true;
                stop_workers();

                _hf.params[1].used = true;
                _hf.params[1].multiplier = m2;
                _hf.params[1].shift = shift1;
                break;
              }
            }
          }
        }
      }, thread_count);
    }

    // Reset the global multiplier - we want to start from zero again, to find the second hash function M.
    if (!m2_found && best_collisions > 0) {
      bucketCount2 = alignUpToPowerOf2(best_collisions);
      for (;;) {
        printf("Finder::find_hash_function() - Trying to find a second hash function [%u buckets]\n", bucketCount2);

        reset_multiplier(bucketCount2);
        uint32_t shift2 = 32 - count_trailing_bits(bucketCount2);
        bool m2_found = false;

        run_async([&]() {
          const uint32_t* values_data = remaining_values.data();
          size_t values_count = remaining_values.size();

          BitArray occupied;
          occupied.resize(bucketCount2);

          for (;;) {
            uint32_t m2_base = next_multiplier_base();
            if (m2_base == 0xFFFFFFFFu)
              return;

            for (uint32_t m2_index = 0; m2_index < mStep; m2_index++) {
              bool found = true;
              uint32_t m2 = m2_base + m2_index;
              occupied.clear();

              for (size_t i = 0; i < values_count; i++) {
                uint32_t index = mulOp2(values_data[i], m2, shift2);
                if (occupied.has_bit(index)) {
                  found = false;
                  break;
                }
                occupied.set_bit(index);
              }

              if (found) {
                std::lock_guard<std::mutex> guard(mutex);
                if (!m2_found) {
                  m2_found = true;
                  stop_workers();

                  _hf.params[1].used = true;
                  _hf.params[1].multiplier = m2;
                  _hf.params[1].shift = shift2;
                  break;
                }
              }
            }
          }
        }, thread_count);

        if (m2_found)
          break;
        bucketCount2 *= 2u;
      }
    }

    // Build tables.
    {
      BitArray occupied;
      std::vector<uint32_t> remaining_indexes;

      occupied.resize(bucketCount1);
      _hf.params[0].table.resize(bucketCount1, uint32_t(0xFFFFFFFFu));
      _hf.params[1].table.resize(bucketCount2, uint32_t(0xFFFFFFFFu));

      {
        std::vector<uint32_t>& table = _hf.params[0].table;
        for (uint32_t i = 0; i < _size; i++) {
          uint32_t index1 = (_values[i] * _hf.params[0].multiplier) >> _hf.params[0].shift;
          if (!occupied.has_bit(index1)) {
            table[index1] = i;
            occupied.set_bit(index1);
          }
          else {
            remaining_indexes.push_back(i);
          }
        }
      }

      if (m2_same_bucket_table || bucketCount2) {
        std::vector<uint32_t>& table = m2_same_bucket_table ? _hf.params[0].table : _hf.params[1].table;
        for (uint32_t i : remaining_indexes) {
          uint32_t index2 = (_values[i] * _hf.params[1].multiplier) >> _hf.params[1].shift;
          if (table[index2] == 0xFFFFFFFFu)
            table[index2] = i;
        }
      }
    }

    return best_collisions != 0xFFFFFFFFu;
  }

  bool find_solution() {
    uint32_t bucket_count = alignUpToPowerOf2(_size);
    for (;;) {
      if (find_hash_function(bucket_count))
        return true;

      bucket_count <<= 1;
      if (bucket_count > _size * 8)
        return false;
    }
  }
};

} // {StupidHash}
