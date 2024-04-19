#ifndef GRAPHLAB_DIMITRA_AUTOMATON_BK_HPP
#define GRAPHLAB_DIMITRA_AUTOMATON_BK_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include <graphlab/logger/logger.hpp>
#include <graphlab/parallel/atomic_ops.hpp>
#include <graphlab/serialization/serialization_includes.hpp>

#include <immintrin.h>
#include <bitset>

constexpr int MAX_AUTOMATON_STATES = 8;
const std::bitset<MAX_AUTOMATON_STATES> bitsmask(0xff);

namespace graphlab {
    /**
     * DiMITra Automaton and supporting data structures
     */
    class OneDimBits {
    public:
        std::bitset<MAX_AUTOMATON_STATES> bits;

        void reset() {
            bits.reset();
        }

        size_t count() { return bits.count(); }
        bool none() { return bits.none(); }

        OneDimBits& operator|=(const OneDimBits& other) {
            bits |= other.bits;
            return *this;
        }

        OneDimBits& operator&=(const OneDimBits& other) {
            bits &= other.bits;
            return *this;
        }

        void set_bit(int idx) { bits[idx] = 1; }
        void unset_bit(int idx) { bits[idx] = 0; }
        bool get_bit(int idx) { return bits[idx] == 1; }


        size_t _Find_first() const { return bits._Find_first(); }
        size_t _Find_next(int idx) const { return bits._Find_next(idx); }

        /* rely on C++11 to get raw byte */
        void save(graphlab::oarchive &oarc) const {
            __mmask8 bitsbyte = static_cast<__mmask8>((bits & bitsmask).to_ulong());
            oarc << bitsbyte;
        }

        void load(graphlab::iarchive& iarc) {
            __mmask8 bitsbyte;
            iarc >> bitsbyte;
            bits |= bitsbyte;
        }
    };

    typedef OneDimBits* TwoDimBits;
    typedef OneDimBits** ThreeDimBits;

    struct Automaton {
        ThreeDimBits automaton;
        int num_states;
        int start_state;
        int end_state;
        int start_edge_label;
        int end_edge_label;
    };

    void initializeTwoDimBits(TwoDimBits & data, int size){
        data = new OneDimBits [size];
        memset(data, 0, sizeof(OneDimBits) * size);
    }

    void resetTwoDimBits(TwoDimBits & data, int size){
        memset(data, 0, sizeof(OneDimBits) * size);
    }

    int getNumSetBits_TwoDimBits(TwoDimBits & data, int size) {
        int count = 0;
        for (int i = 0; i < size; ++i) {
            count += data[i].count();
        }
        return count;
    }

    void deleteTwoDimBits(TwoDimBits & data, int size) {
        delete [] data;
    }

    void initializeThreeDimBits(ThreeDimBits & data, int size1, int size2){
        data = new OneDimBits*[size1];
        for (int i = 0; i < size1; ++i) {
            data[i] = new OneDimBits[size2];
            memset(data[i], 0, sizeof(OneDimBits) * size2);
        }
    }

    void resetThreeDimBits(ThreeDimBits & data, int size1, int size2){
        for (int i = 0; i < size1; ++i) {
            memset(data[i], 0, sizeof(OneDimBits) * size2);
        }
    }

    int getNumSetBits_ThreeDimBits(ThreeDimBits & data, int size1, int size2){
        int count = 0;
        for (int i = 0; i < size1; ++i) {
            for (int j = 0; j < size2; ++j) {
                count += data[i][j].count();
            }
        }
        return count;
    }


    void deleteThreeDimBits(ThreeDimBits & data, int size1, int size2) {
        for (int i = 0; i < size1; ++i) {
            delete [] data[i];
        }
        delete [] data;
    }
}

#endif  // GRAPHLAB_DIMITRA_AUTOMATON_BK_HPP