#ifndef GRAPHLAB_DIMITRA_AUTOMATON_HPP
#define GRAPHLAB_DIMITRA_AUTOMATON_HPP

// #include <cstdio>
// #include <cstdlib>
// #include <cstring>
// #include <stdint.h>
// #include <graphlab/logger/logger.hpp>
// #include <graphlab/parallel/atomic_ops.hpp>
// #include <graphlab/serialization/serialization_includes.hpp>

// #include <immintrin.h>

#include <graphlab/util/dimitra_bitvec.hpp>

namespace graphlab {
    /**
     * TODO: support variants of MAX_AUTOMATON_STATES
     */
    class one_dim_bits {
    public:
        __mmask8 bits;

        one_dim_bits() { bits = 0; }
        one_dim_bits(__mmask8 val) { bits = val; }
        void reset() { bits = 0; }

        bool none() { return bits == 0; }

        one_dim_bits& operator|=(const one_dim_bits& other) {
            bits = bits | other.bits;
            return *this;
        }

        one_dim_bits& operator&=(const one_dim_bits& other) {
            bits = bits & other.bits;
            return *this;
        }

        void set_bit(int idx) { 
            bits = bits | sseMasks8[idx];
        }

        void unset_bit(int idx) {
            bits = bits & (~sseMasks8[idx]);
        }

        bool test_bit(int idx) const {
            return (bits & sseMasks8[idx]);
        }

        void save(graphlab::oarchive &oarc) const {
            oarc << bits;
        }
        
        void load(graphlab::iarchive &iarc) {
            iarc >> bits;
        }
    };

    /* Finite State Automata */
    class Automata {
    public:
        __mmask8** array;
        int num_state;
        int num_label;

        Automata(int num_state_in = 4, int num_label_in = 8) {
            num_state = num_state_in;
            num_label = num_label_in;
            array = new __mmask8*[num_state];
            for (int i = 0; i < num_state; i++) {
                array[i] = new __mmask8[num_label];
                memset(array[i], 0, sizeof(__mmask8) * num_label);
            }
        }

        void reset() {
            for (int i = 0; i < num_state; i++) {
                memset(array[i], 0, sizeof(__mmask8) * num_label);
            }
        }

        __mmask8 get_bits(int state_idx, int label_idx) {
            return array[state_idx][label_idx];
        }

        void set_bit(int state_idx, int label_idx, int pos) {
            array[state_idx][label_idx] |= sseMasks8[pos];
        }

        void unset_bit(int state_idx, int label_idx, int pos) {
            array[state_idx][label_idx] &= (~sseMasks8[pos]);
        }

        void set_or_bits(int state_idx, int label_idx, __mmask8 val) {
            array[state_idx][label_idx] |= val;
        }

        void set_and_bits(int state_idx, int label_idx, __mmask8 val) {
            array[state_idx][label_idx] &= val;
        }

        void save(graphlab::oarchive &oarc) const {
            oarc << num_state;
            oarc << num_label;
            for (int i = 0; i < num_state; i++) {
                serialize(oarc, array[i], sizeof(__mmask8) * num_label);
            }
        }

        void load(graphlab::iarchive &iarc) {
            iarc >> num_state;
            iarc >> num_label;
            for (int i = 0; i < num_state; i++) {
                deserialize(iarc, array[i], sizeof(__mmask8) * num_label);
            }
        }
    };
}

#endif  // GRAPHLAB_DIMITRA_AUTOMATON_HPP