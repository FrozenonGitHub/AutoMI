#ifndef GRAPHLAB_automi_bitvec_HPP
#define GRAPHLAB_automi_bitvec_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include <graphlab/logger/logger.hpp>
#include <graphlab/parallel/atomic_ops.hpp>
#include <graphlab/serialization/serialization_includes.hpp>

#include <immintrin.h>

/**
 * Define SSE constants for SIMD operation usage
 */
__mmask8 sseAllOnes8 = __mmask8(0xFF);
__mmask8 sseMasks8[8] = {
        __mmask8(0x1), __mmask8(0x2), __mmask8(0x4), __mmask8(0x8),
        __mmask8(0x10), __mmask8(0x20), __mmask8(0x40), __mmask8(0x80)
};
__m256 sseAllZeroPS256 = _mm256_setzero_ps();
__m256i sseAllZero256 = _mm256_setzero_si256();
__m256i sseAllOnes256 = _mm256_cmpeq_epi32(sseAllZero256, sseAllZero256);

/**
 * TODO:
 * (1) add SIMD support
 * (2) add fixed_xxx as friend class
 * (3) finish empty functions
 * (4) add necessary operators
 */

namespace graphlab {
    /**
     * Implements array of int | float | bit for DiMITra
     */
    /// Primary template class automi_bitvec
    template <typename dtype>
    class automi_bitvec {};

    /// specialized <dtype=bool> class automi_bitvec
    template<>
    class automi_bitvec<bool> {
    public:
        typedef __mmask8 element;
        automi_bitvec<bool>() : array(NULL), len(0), arrlen(0) {}

        /// Constructs a bitvec with 'size' elements. All elements are 0.
        explicit automi_bitvec<bool>(size_t size) : array(NULL), len(0), arrlen(0) {
            resize(size);
            clear();
        }

        /// Construct a copy of bitvec db
        automi_bitvec<bool>(const automi_bitvec<bool> &db) {
            array = NULL;
            len = 0;
            arrlen = 0;
            *this = db;
        }

        /// destructor
        ~automi_bitvec() { free(array); }

        /// Make a new copy of the bitvec db
        inline automi_bitvec<bool>& operator=(const automi_bitvec<bool>& db) {
            resize(db.size());
            len = db.len;
            arrlen = db.arrlen;
            memcpy(array, db.array, sizeof(__mmask8) * arrlen);
            return *this;
        }

        /**
         * If new size 'n' is larger than 'len', reallocate memory and set to 0;
         * if new size 'n' is smaller than 'len', leave 'deleted' memory unattended.
         */
        inline void resize(size_t n) {
            len = n;
            size_t prev_arrlen = arrlen;
            arrlen = n / 8;
            array = (__mmask8 *)realloc(array, sizeof(__mmask8) * arrlen);
            if (arrlen > prev_arrlen) {
                memset(&array[prev_arrlen], 0, sizeof(__mmask8) * (arrlen - prev_arrlen));
            }
        }

        /// Set all memory within bitvec to 0
        inline void clear() {
            memset(array, 0, sizeof(__mmask8) * arrlen);
        }

        /// Returns the number of elements in this bitvec
        inline size_t size() const {
            return len;
        }

        /// Set all element values using provided val
        inline void set_all(bool val) {
            if (val) {
                memset(array, sseAllOnes8, sizeof(__mmask8) * arrlen);
            }
            else {
                memset(array, 0, sizeof(__mmask8) * arrlen);
            }
        }

        /// Set one position using provided val and idx
        inline void set_single(bool val, size_t idx) {
            size_t arrpos, bitpos;
            bit_to_pos(idx, arrpos, bitpos);
            if (val) {
                array[arrpos] = array[arrpos] | sseMasks8[bitpos];
            }
            else {
                array[arrpos] = array[arrpos] & (~sseMasks8[bitpos]);
            }
        }

        inline void set_element(__mmask8 val, size_t b) {
            size_t arrpos = b;
            array[arrpos] = val;
        }

        inline void masked_set_element(__mmask8 val, size_t b, __mmask8 mask) {
            size_t arrpos = b;
            array[arrpos] = mask & val;
        }

        /// Get one single element using provided index
        element get_single(size_t b) const {
            size_t arrpos = b;  // THIS IS SPECIAL
            return array[arrpos];
        }

        // test if bit is set to 1
        bool test_bit(size_t b) const {
            size_t arrpos, bitpos;
            bit_to_pos(b, arrpos, bitpos);
            return (array[arrpos] & sseMasks8[bitpos]);
        }

        /// in-place pairwise Or Operator
        static void pair_op_or(automi_bitvec<bool>& a, const automi_bitvec<bool>& b) {
            // TODO: optimization?
            for (size_t i = 0; i < a.arrlen; i++) {
                a.array[i] = a.array[i] | b.array[i];
            }
        }

        inline void vec_op_set(const automi_bitvec<bool>& other, size_t b) {
            size_t arrpos = b;
            array[arrpos] = other.array[arrpos];
        }

        inline void vec_op_set_mask(const automi_bitvec<bool>& other, const automi_bitvec<bool>& mask, size_t b) {
            size_t arrpos = b;
            __mmask8 set_val = other.array[arrpos] & mask.array[arrpos];
            array[arrpos] = set_val;
        }

        inline void vec_op_set(const automi_bitvec<bool>& other) {
            for (size_t i = 0; i < arrlen; i++) {
                vec_op_set(other, i);
            }
        }

        inline void vec_op_set_mask(const automi_bitvec<bool>& mask, const automi_bitvec<bool>& other) {
            for (size_t i = 0; i < arrlen; i++) {
                vec_op_set_mask(other, mask, i);
            }
        }

        inline void vec_op_set(const automi_bitvec<bool>& mask, bool val) {
            __m256i valVec = val ? sseAllOnes256 : sseAllZero256;
            *reinterpret_cast<__m256i*>(array) = valVec;
        }

        inline void vec_op_set_mask(const automi_bitvec<bool>& mask, bool val) {
            __m256i maskVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(mask.array));
            __m256i valVec = val ? sseAllOnes256 : sseAllZero256;

            *reinterpret_cast<__m256i*>(array) = _mm256_blendv_epi8(*reinterpret_cast<__m256i*>(array), valVec, maskVec);
        }

        /**
         * Returns true if bit in 'this' changes after `bitwise_or` with 'other'
         * set 'this' = ''
         * Returns false if no such elements exists.
         */
        inline bool vec_op_or_update(const automi_bitvec<bool>& other, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 prev_val = array[arrpos];
            // array[arrpos] = _kor_mask8(array[arrpos], other.array[arrpos]);
            array[arrpos] = array[arrpos] | other.array[arrpos];
            // __mmask8 cmpor_mask = _kandn_mask8(prev_val, array[arrpos]);
            __mmask8 cmpor_mask = (~prev_val) & array[arrpos];
            return bool(cmpor_mask);
        }

        inline bool vec_op_or_update(const automi_bitvec<bool>& other) {
            for (size_t arrpos = 0; arrpos < len; arrpos++) {
                // array[arrpos] = _kor_mask8(array[arrpos], other.array[arrpos]);
                array[arrpos] = array[arrpos] | other.array[arrpos];
                // __mmask8 cmpor_mask = _kandn_mask8(prev_val, array[arrpos]);
            }
            return true;
        }

        inline bool vec_op_or_update_mask(const automi_bitvec<bool>& mask, const automi_bitvec<bool>& other) {
            for (size_t arrpos = 0; arrpos < len; arrpos++) {
                // array[arrpos] = _kor_mask8(array[arrpos], other.array[arrpos]);
                __mmask8 masked_val = mask.array[arrpos] & other.array[arrpos];
                array[arrpos] = array[arrpos] | masked_val;
                // __mmask8 cmpor_mask = _kandn_mask8(prev_val, array[arrpos]);
            }
            return true;
        }

        inline bool vec_op_or_update(const automi_bitvec<bool>& other1, const __mmask8& other2, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 prev_val = other1.array[arrpos];
            // array[arrpos] = _kor_mask8(other1.array[arrpos], other2);
            // __mmask8 cmpor_mask = _kandn_mask8(prev_val, array[arrpos]);
            array[arrpos] = other1.array[arrpos] | other2;
            __mmask8 cmpor_mask = (~prev_val) & array[arrpos];
            return bool(cmpor_mask);
        }

        inline bool vec_op_or_update(const automi_bitvec<bool>& other1, const automi_bitvec<bool>& other2, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 prev_val = other1.array[arrpos];
            array[arrpos] = other1.array[arrpos] | other2.array[arrpos];
            __mmask8 cmpor_mask = (~prev_val) & array[arrpos];
            return bool(cmpor_mask);
        }

        inline bool vec_op_or_update_mask(const automi_bitvec<bool>& other, const automi_bitvec<bool>& mask, size_t b) {
            size_t arrpos = b; // THIS IS SPECIAL!
            __mmask8 tmp_mask = mask.array[arrpos];
            if (!tmp_mask)
                return false;
            __mmask8 prev_val = array[arrpos];
            // __mmask8 other_val = _kand_mask8(tmp_mask, other.array[arrpos]);
            // array[arrpos] = _kor_mask8(array[arrpos], other_val);
            // __mmask8 cmpor_mask = _kandn_mask8(prev_val, array[arrpos]);
            __mmask8 other_val = tmp_mask & other.array[arrpos];
            array[arrpos] = array[arrpos] | other_val;
            __mmask8 cmpor_mask = (~prev_val) & array[arrpos];
            return bool(cmpor_mask);
        }

        inline bool vec_op_or_update_mask(const automi_bitvec<bool>& other1, const automi_bitvec<bool>& mask, const __mmask8& other2, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 tmp_mask = mask.array[arrpos];
            if (!tmp_mask)
                return false;
            __mmask8 prev_val = other1.array[arrpos];
            // __mmask8 other_val = _kand_mask8(tmp_mask, other2);
            // array[arrpos] = _kor_mask8(other1.array[arrpos], other_val);
            // __mmask8 cmpor_mask = _kandn_mask8(prev_val, array[arrpos]);
            __mmask8 other_val = tmp_mask & other2;
            array[arrpos] = other1.array[arrpos] | other_val;
            __mmask8 cmpor_mask = (~prev_val) & array[arrpos];
            return bool(cmpor_mask);
        }

        inline void vec_op_andnot_update(const automi_bitvec<bool>& other1, const automi_bitvec<bool>& other2) {
            *reinterpret_cast<__m256i*>(array) = _mm256_andnot_epi64(
                *reinterpret_cast<__m256i*>(other1.array), *reinterpret_cast<__m256i*>(other2.array));
        }

        inline void vec_op_andnot_update_mask(const automi_bitvec<bool>& mask, const automi_bitvec<bool>& other1, const automi_bitvec<bool>& other2) {
            *reinterpret_cast<__m256i*>(array) = _mm256_mask_andnot_si256(
                *reinterpret_cast<__m256i*>(mask.array),
                *reinterpret_cast<__m256i*>(other1.array), *reinterpret_cast<__m256i*>(other2.array));
        }

        inline bool vec_all_zeros() const {
            return _mm256_testz_si256(*reinterpret_cast<__m256i*>(array), sseAllOnes256);
        }

        inline void vec_op_negate(automi_bitvec<bool>& ret) {
            *reinterpret_cast<__m256i*>(ret.array) = _mm256_xor_si256(*reinterpret_cast<__m256i*>(array), sseAllOnes256);
        }

        inline void vec_op_negate_mask(const automi_bitvec<bool>& mask, automi_bitvec<bool>& ret) {
            *reinterpret_cast<__m256i*>(ret.array) = _mm256_and_si256(*reinterpret_cast<__m256i*>(mask.array),_mm256_xor_epi64(*reinterpret_cast<__m256i*>(array), sseAllOnes256));
        }

        inline void vec_op_cmpgt_update(const automi_bitvec<int>& other1, const automi_bitvec<int>& other2) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_cmpgt_epi32_mask(other1.array[i], other2.array[i]);
            }
        }

        inline void vec_op_cmpgt_update_mask(const automi_bitvec<bool>& mask, const automi_bitvec<int>& other1, const automi_bitvec<int>& other2) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mask_cmpgt_epi32_mask(mask, other1.array[i], other2.array[i]);
            }
        }

        inline void vec_op_cmpneq_update(const automi_bitvec<int>& other1, const automi_bitvec<int>& other2) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_cmpneq_epi32_mask(other1.array[i], other2.array[i]);
            }
        }

        /// Serializes this bitvec to an archive
        inline void save(oarchive& oarc) const {
            oarc << len << arrlen;
            if (arrlen > 0)
                serialize(oarc, array, arrlen * sizeof(__mmask8));
        }

        /// Masked Serialization (I don't think masking can save traffic)
        inline void masked_save(oarchive& oarc, const automi_bitvec<bool>& mask) const {
            oarc << len << arrlen;
            if (arrlen > 0)
                serialize(oarc, array, arrlen * sizeof(__mmask8));
        }

        /// Deserializes this bitvec from an archive
        inline void load(iarchive& iarc) {
            // I don't think free-up memory used by array is necessary
            size_t prev_arrlen = arrlen;
            iarc >> len >> arrlen;
            if (arrlen > 0) {
                if (arrlen != prev_arrlen) {
                    resize(len);
                }
                deserialize(iarc, array, arrlen * sizeof(__mmask8));
            }
        }

        /// Masked Serialization (coupled with masked_save)
        inline void masked_load(iarchive& iarc, const automi_bitvec<bool>& mask) {
            // I don't think free-up memory used by array is necessary
            size_t prev_arrlen = arrlen;
            iarc >> len >> arrlen;
            if (arrlen > 0) {
                if (arrlen != prev_arrlen) {
                    resize(len);
                }
                deserialize(iarc, array, arrlen * sizeof(__mmask8));
            }
        }

        __mmask8* array;
        size_t len;
        size_t arrlen;
    private:
        inline static void bit_to_pos(size_t b, size_t& arrpos, size_t& bitpos) {
            arrpos = b / 8;
            bitpos = b % 8;
        }
    };

    /// specialized <dtype=int> class automi_bitvec
    template<>
    class automi_bitvec<int> {
    public:
        typedef __m256i element;
        /// Constructs an empty automi_bitvec
        automi_bitvec() : array(NULL), len(0), arrlen(0) {}

        /// Constructs a bitvec with 'size' elements. All elements are 0.
        explicit automi_bitvec(size_t size) : array(NULL), len(0), arrlen(0) {
            resize(size);
            clear();
        }

        /// Construct a copy of bitvec db
        automi_bitvec(const automi_bitvec<int> &db) {
            array = NULL;
            len = 0;
            arrlen = 0;
            *this = db;
        }

        /// destructor
        ~automi_bitvec() { free(array); }

        /// Make a new copy of the bitvec db
        inline automi_bitvec<int>& operator=(const automi_bitvec<int>& db) {
            resize(db.size());
            len = db.len;
            arrlen = db.arrlen;
            memcpy(array, db.array, sizeof(__m256i) * arrlen);
            return *this;
        }

        /// Overload operator+= for gather function
        inline automi_bitvec<int>& operator+=(const automi_bitvec<int>& other) {
            // assume "other" and "this" has same size
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_add_epi32(array[i], other.array[i]);
            }
            return *this;
        }

        /**
         * If new size 'n' is larger than 'len', reallocate memory and set to 0;
         * if new size 'n' is smaller than 'len', leave 'deleted' memory unattended.
         */
        inline void resize(size_t n) {
            len = n;
            size_t prev_arrlen = arrlen;
            arrlen = n / 8;
            array = (__m256i*)realloc(array, sizeof(__m256i) * arrlen);
            // NOT SURE: set newly allocated memory to 0
            if (arrlen > prev_arrlen) {
                memset(&array[prev_arrlen], 0, sizeof(__m256i) * (arrlen - prev_arrlen));
            }
        }

        /// Set all memory within bitvec to 0
        inline void clear() {
            memset(array, 0, sizeof(__m256i) * arrlen);
        }

        /// Returns the number of elements in this bitvec
        inline size_t size() const {
            return len;
        }

        /// Set all elements value using provided val
        inline void set_all(int val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_set1_epi32(val);
            }
        }

        /// Set one lane value using provided val and idx
        inline void set_single(int val, size_t idx) {
            // size_t arrpos, bitpos;
            // bit_to_pos(idx, arrpos, bitpos);
            // array[arrpos] = _mm256_mask_set1_epi32(array[arrpos], sseMasks8[bitpos], val);
            int *tmp_ptr = (int *)(array);
            tmp_ptr[idx] = val;
        }

        /// Set lanes to value `val' using provided mask
        inline void set_mask(automi_bitvec<bool>& m, int val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mask_set1_epi32(array[i], m.array[i], val);
            }
        }

        inline void set_element(__m256i val, size_t b) {
            size_t arrpos = b;
            array[arrpos] = val;
        }

        inline void masked_set_element(__m256i val, size_t b, __mmask8 mask) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_blend_epi32(mask, array[arrpos], val);
        }

        /// Get one single lane in bitvec element values
        inline int get_single(size_t b) {
            int *tmp_ptr = (int *)(array);
            return tmp_ptr[b];
        }

        /// in-place pairwise Min Operator
        static void pair_op_min(automi_bitvec<int>& a, const automi_bitvec<int>& b) {
            for (size_t i = 0; i < a.arrlen; i++) {
                a.array[i] = _mm256_min_epi32(a.array[i], b.array[i]);
            }
        }

        /// in-place pairwise Max Operator
        static void pair_op_max(automi_bitvec<int>& a, const automi_bitvec<int>& b) {
            for (size_t i = 0; i < a.arrlen; i++) {
                a.array[i] = _mm256_max_epi32(a.array[i], b.array[i]);
            }
        }

        static void pair_op_add(automi_bitvec<int>& a, const automi_bitvec<int>& b) {
            for (size_t i = 0; i < a.arrlen; i++) {
                a.array[i] = _mm256_add_epi32(a.array[i], b.array[i]);
            }
        }

        static void pair_op_mul(automi_bitvec<int>& a, const automi_bitvec<int>& b) {
            for (size_t i = 0; i < a.arrlen; i++) {
                a.array[i] = _mm256_mul_epi32(a.array[i], b.array[i]);
            }
        }

        inline void vec_op_mul_update(const automi_bitvec<int>& other, int val, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            array[arrpos] = _mm256_mul_epi32(other.array[arrpos], _mm256_set1_epi32(val));
        }

        inline void vec_op_mul_update(const automi_bitvec<int>& other, int val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mul_epi32(other.array[i], _mm256_set1_epi32(val));
            }
        }

        inline void vec_op_masked_mul_update(const automi_bitvec<bool>& mask, const automi_bitvec<int>& other, int val, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_mul_epi32(array[arrpos], mask.array[arrpos], other.array[arrpos], _mm256_set1_epi32(val));
        }

        /// Serializes this bitvec to an archive
        inline void save(oarchive& oarc) const {
            oarc << len << arrlen;
            if (arrlen > 0)
                serialize(oarc, array, arrlen * sizeof(__m256i));
        }

        /// Deserializes this bitvec from an archive
        inline void load(iarchive& iarc) {
            // I don't think free-up memory used by array is necessary
            size_t new_arrlen;
            iarc >> len >> new_arrlen;
            if (arrlen > 0) {
                if (arrlen != new_arrlen) {
                    resize(len);
                }
                deserialize(iarc, array, arrlen * sizeof(__m256i));
            }
        }

        /// Masked Serialization
        inline void masked_save(oarchive& oarc, const automi_bitvec<bool>& mask) const {
            // TODO: need optimization here!
            oarc << len << arrlen;
            int *array_ptr;
            for (size_t i = 0; i < arrlen; i++) {
                array_ptr = (int *)&array[i];
                for (int j = 0; j < 8; j++) {
                    if ((mask.array[i] >> j) & 1) {
                        oarc << array_ptr[j];
                    }
                }
            }
        }

        /// Masked Deserialization
        inline void masked_load(iarchive& iarc, const automi_bitvec<bool>& mask) {
            // TODO: need optimization here!
            size_t new_arrlen;
            iarc >> len >> arrlen;
            if (arrlen != new_arrlen) {
                resize(len);
            }
            for (size_t i = 0; i < arrlen; i++) {
                int tmp_val;
                for (int j = 0; j < 8; j++) {
                    if ((mask.array[i] >> j) & 1) {
                        iarc >> tmp_val;
                        array[i] = _mm256_insert_epi32(array[i], tmp_val, j);
                    }
                }
            }
        }

        inline void vec_op_set(const automi_bitvec<int>& other, size_t b) {
            size_t arrpos = b;
            array[arrpos] = other.array[arrpos];
        }

        inline void vec_op_set_mask(const automi_bitvec<int>& other, const automi_bitvec<bool>& mask, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_blend_epi32(mask.array[arrpos], array[arrpos], other.array[arrpos]);
        }

        inline void vec_op_set(const automi_bitvec<int>& other) {
            for (size_t i = 0; i < arrlen; i++) {
                vec_op_set(other, i);
            }
        }

        inline void vec_op_set_mask(const automi_bitvec<bool>& mask, const automi_bitvec<int>& other) {
            for (size_t i = 0; i < arrlen; i++) {
                vec_op_set_mask(other, mask, i);
            }
        }

        inline void vec_op_set(const automi_bitvec<bool>& mask, int val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_set1_epi32(val);
            }
        }

        inline void vec_op_set_mask(const automi_bitvec<bool>& mask, int val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mask_set1_epi32(array[i], mask.array[i], 1);
            }
        }

        /**
         * Returns true if 'this' contain elements greater than / less than 'other', set those lanes to other's value.
         * Returns false if no such elements exists.
         */
        inline bool vec_op_cmpgt_update(const automi_bitvec<int>& other, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 cmpgt_mask = _mm256_cmpgt_epi32_mask(array[arrpos], other.array[arrpos]);
            array[arrpos] = _mm256_min_epi32(array[arrpos], other.array[arrpos]);
            return bool(cmpgt_mask);    // Does this work?
        }

        inline bool vec_op_masked_cmpgt_update(const automi_bitvec<bool>& mask, const automi_bitvec<int>& other, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 cmpgt_mask = _mm256_cmpgt_epi32_mask(array[arrpos], other.array[arrpos]);
            cmpgt_mask = cmpgt_mask & mask.array[arrpos];
            array[arrpos] = _mm256_mask_min_epi32(array[arrpos], cmpgt_mask, array[arrpos], other.array[arrpos]);
            return bool(cmpgt_mask); // Does this work?
        }

        inline bool vec_op_masked_cmpgt_update(const automi_bitvec<bool>& mask, const automi_bitvec<int>& other1, const __m256i& other2, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 cmpgt_mask = _mm256_cmpgt_epi32_mask(other1.array[arrpos], other2);
            cmpgt_mask = cmpgt_mask & mask.array[arrpos];
            array[arrpos] = _mm256_mask_min_epi32(array[arrpos], cmpgt_mask, other1.array[arrpos], other2);
            return bool(cmpgt_mask); // Does this work?
        }

        inline bool vec_op_cmplt_update(const automi_bitvec<int>& other, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 cmplt_mask = _mm256_cmplt_epi32_mask(array[arrpos], other.array[arrpos]);
            array[arrpos] = _mm256_max_epi32(array[arrpos], other.array[arrpos]);
            return bool(cmplt_mask);    // Does this work?
        }

        inline bool vec_op_cmpgt_update(const automi_bitvec<int>& other1, const __m256i& other2, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 cmpgt_mask = _mm256_cmpgt_epi32_mask(other1.array[arrpos], other2);
            array[arrpos] = _mm256_min_epi32(other1.array[arrpos], other2);
            return bool(cmpgt_mask);
        }

        inline bool vec_op_cmplt_update(const automi_bitvec<int>& other1, const __m256i& other2, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 cmpgt_mask = _mm256_cmplt_epi32_mask(other1.array[arrpos], other2);
            array[arrpos] = _mm256_max_epi32(other1.array[arrpos], other2);
            return bool(cmpgt_mask);
        }

        // TODO: Mask operator also have push and pull stle?
        inline bool vec_op_cmpgt_update_mask(const automi_bitvec<int>& other, automi_bitvec<bool>& mask, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 tmp_mask = _mm256_cmpgt_epi32_mask(array[arrpos], other.array[arrpos]);
            array[arrpos] = _mm256_min_epi32(array[arrpos], other.array[arrpos]);   // TODO: should we add mask here?
            // mask.array[arrpos] = mask.array[arrpos] | tmp_mask;
            mask.array[arrpos] = tmp_mask;
            return bool(tmp_mask);
        }

        inline bool vec_op_cmplt_update_mask(const automi_bitvec<int>& other, automi_bitvec<bool>& mask, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            __mmask8 tmp_mask = _mm256_cmplt_epi32_mask(array[arrpos], other.array[arrpos]);
            array[arrpos] = _mm256_max_epi32(array[arrpos], other.array[arrpos]);   // TODO: should we add mask here?
            // mask.array[arrpos] = mask.array[arrpos] | tmp_mask;
            mask.array[arrpos] = tmp_mask;
            return bool(tmp_mask);
        }

        // inline bool vec_op_cmpgt_update_mask(const automi_bitvec<int>& other1, const __m256i& other2, automi_bitvec<bool>& mask, size_t b) {
        //     size_t arrpos = b;  // THIS IS SPECIAL!
        //     __mmask8 cmpgt_mask = _mm256_cmpgt_epi32_mask(other1.array[arrpos], other2);
        //     array[arrpos] = _mm256_min_epi32(other1.array[arrpos], other2); // TODO: should we add mask here?
        //     mask.array[arrpos] = mask.array[arrpos] | cmpgt_mask;
        //     return bool(cmpgt_mask);
        // }

        inline void vec_op_masked_add_update(__mmask8 mask, int val, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_add_epi32(array[arrpos], mask, 
                array[arrpos], _mm256_set1_epi32(val));
        }

        inline void vec_op_masked_add_update(const automi_bitvec<bool>& mask, int val, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_add_epi32(array[arrpos], mask.array[arrpos], array[arrpos], _mm256_set1_epi32(val));
        }

        inline void vec_op_masked_add_update(const automi_bitvec<bool>& mask, const automi_bitvec<int>& other, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_add_epi32(array[arrpos], mask.array[arrpos], array[arrpos], other.array[arrpos]);
        }

        inline void vec_op_add_update(const automi_bitvec<int>& other, int val, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_add_epi32(other.array[arrpos], _mm256_set1_epi32(val));
        }

        inline void vec_op_add_update(const automi_bitvec<int>& other1, const automi_bitvec<int>& other2, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_add_epi32(other1.array[arrpos], other2.array[arrpos]);
        }

        inline void vec_op_add_update(const automi_bitvec<int>& other, int val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_add_epi32(other.array[i], _mm256_set1_epi32(val));
            }
        }

        inline void vec_op_add_update_mask(const automi_bitvec<bool>& mask, const automi_bitvec<int>& other, int val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mask_add_epi32(array[i], mask.array[i], array[i], _mm256_set1_epi32(val));
            }
        }

        inline void vec_op_add_update_mask(const automi_bitvec<bool>& mask, const automi_bitvec<int>& other1, const automi_bitvec<int>& other2) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mask_add_epi32(array[i], mask.array[i], other1.array[i], other2.array[i]);
            }
        }

        /**
         * Return 
         */
        element vec_op_add_val(size_t b, int val) const {
            size_t arrpos = b;
            return _mm256_add_epi32(array[arrpos], _mm256_set1_epi32(val));
        }

        /**
         * Return 
         */
        element vec_op_min_val(size_t b, int val) const {
            size_t arrpos = b;
            return _mm256_min_epi32(array[arrpos], _mm256_set1_epi32(val));
        }

        __m256i* array;
        size_t len;
        size_t arrlen;
    private:
        inline static void bit_to_pos(size_t b, size_t& arrpos, size_t& bitpos) {
            arrpos = b / 8;
            bitpos = b % 8;
        }
    };


    /// specialized <dtype=float> class automi_bitvec
    template<>
    class automi_bitvec<float> {
    public:
        typedef __m256 element;
        /// Constructs an empty automi_bitvec
        automi_bitvec() : array(NULL), len(0), arrlen(0) {}

        /// Constructs a bitvec with 'size' elements. All elements are 0.
        explicit automi_bitvec(size_t size) : array(NULL), len(0), arrlen(0) {
            resize(size);
            clear();
        }

        /// Construct a copy of bitvec db
        automi_bitvec(const automi_bitvec<float> &db) {
            array = NULL;
            len = 0;
            arrlen = 0;
            *this = db;
        }

        /// destructor
        ~automi_bitvec() { free(array); }

        /// Make a new copy of the bitvec db
        inline automi_bitvec<float>& operator=(const automi_bitvec<float>& db) {
            resize(db.size());
            len = db.len;
            arrlen = db.arrlen;
            memcpy(array, db.array, sizeof(__m256) * arrlen);
            return *this;
        }

        /// Overload operator+= for gather function
        inline automi_bitvec<float>& operator+=(const automi_bitvec<float>& other) {
            // assume "other" and "this" has same size
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_add_ps(array[i], other.array[i]);
            }
            return *this;
        }

        /**
         * If new size 'n' is larger than 'len', reallocate memory and set to 0;
         * if new size 'n' is smaller than 'len', leave 'deleted' memory unattended.
         */
        inline void resize(size_t n) {
            len = n;
            size_t prev_arrlen = arrlen;
            arrlen = n / 8;
            array = (__m256*)realloc(array, sizeof(__m256) * arrlen);
            // NOT SURE: set newly allocated memory to 0
            if (arrlen > prev_arrlen) {
                memset(&array[prev_arrlen], 0, sizeof(__m256) * (arrlen - prev_arrlen));
            }
        }

        /// Set all memory within bitvec to 0
        inline void clear() {
            memset(array, 0, sizeof(__m256) * arrlen);
        }

        /// Returns the number of elements in this bitvec
        inline size_t size() const {
            return len;
        }

        /// Set all elements value using provided val
        inline void set_all(float val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_set1_ps(val);
            }
        }

        /// Set one lane value using provided val and idx
        inline void set_single(float val, size_t idx) {
            // size_t arrpos, bitpos;
            // bit_to_pos(idx, arrpos, bitpos);
            // array[arrpos] = _mm256_mask_set1_epi32(array[arrpos], sseMasks8[bitpos], val);
            float *tmp_ptr = (float *)(array);
            tmp_ptr[idx] = val;
        }

        /// Set lanes to value `val' using provided mask
        inline void set_mask(automi_bitvec<bool>& m, float val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mask_or_ps(array[i], m.array[i],
                    sseAllZeroPS256, _mm256_set1_ps(val));
            }
        }

        inline void set_element(__m256 val, size_t b) {
            size_t arrpos = b;
            array[arrpos] = val;
        }

        inline void masked_set_element(__m256 val, size_t b, __mmask8 mask) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_blend_ps(mask, array[arrpos], val);
        }

        /// Get one single lane in bitvec element values
        inline float get_single(size_t b) {
            float *tmp_ptr = (float *)(array);
            return tmp_ptr[b];
        }

        /// in-place pairwise Min Operator
        static void pair_op_min(automi_bitvec<float>& a, const automi_bitvec<float>& b) {
            for (size_t i = 0; i < a.arrlen; i++) {
                a.array[i] = _mm256_min_ps(a.array[i], b.array[i]);
            }
        }

        /// in-place pairwise Max Operator
        static void pair_op_max(automi_bitvec<float>& a, const automi_bitvec<float>& b) {
            for (size_t i = 0; i < a.arrlen; i++) {
                a.array[i] = _mm256_max_ps(a.array[i], b.array[i]);
            }
        }

        static void pair_op_add(automi_bitvec<float>& a, const automi_bitvec<float>& b) {
            for (size_t i = 0; i < a.arrlen; i++) {
                a.array[i] = _mm256_add_ps(a.array[i], b.array[i]);
            }
        }

        static void pair_op_mul(automi_bitvec<float>& a, const automi_bitvec<float>& b) {
            for (size_t i = 0; i < a.arrlen; i++) {
                a.array[i] = _mm256_mul_ps(a.array[i], b.array[i]);
            }
        }

        /// Serializes this bitvec to an archive
        inline void save(oarchive& oarc) const {
            oarc << len << arrlen;
            if (arrlen > 0)
                serialize(oarc, array, arrlen * sizeof(__m256));
        }

        /// Deserializes this bitvec from an archive
        inline void load(iarchive& iarc) {
            // I don't think free-up memory used by array is necessary
            size_t new_arrlen;
            iarc >> len >> new_arrlen;
            if (arrlen > 0) {
                if (arrlen != new_arrlen) {
                    resize(len);
                }
                deserialize(iarc, array, arrlen * sizeof(__m256));
            }
        }

        /// Masked Serialization
        inline void masked_save(oarchive& oarc, const automi_bitvec<bool>& mask) const {
            // TODO: need optimization here!
            oarc << len << arrlen;
            float *array_ptr;
            for (size_t i = 0; i < arrlen; i++) {
                array_ptr = (float *)&array[i];
                for (int j = 0; j < 8; j++) {
                    if ((mask.array[i] >> j) & 1) {
                        oarc << array_ptr[j];
                    }
                }
            }
        }

        /// Masked Deserialization
        inline void masked_load(iarchive& iarc, const automi_bitvec<bool>& mask) {
            // TODO: need optimization here!
            size_t new_arrlen;
            iarc >> len >> arrlen;
            if (arrlen != new_arrlen) {
                resize(len);
            }
            float *array_ptr;
            for (size_t i = 0; i < arrlen; i++) {
                array_ptr = (float *)&array[i];
                float tmp_val;
                for (int j = 0; j < 8; j++) {
                    if ((mask.array[i] >> j) & 1) {
                        iarc >> tmp_val;
                        array_ptr[j] = tmp_val;
                    }
                }
            }
        }

        inline void vec_op_set(const automi_bitvec<float>& other, size_t b) {
            size_t arrpos = b;
            array[arrpos] = other.array[arrpos];
        }

        // TODO: rename operators, put `masked` after `vec_op` to differentiate with `mask`

        inline void vec_op_set_mask(const automi_bitvec<float>& other, const automi_bitvec<bool>& mask, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_blend_ps(mask.array[arrpos], array[arrpos], other.array[arrpos]);
        }

        inline void vec_op_set(const automi_bitvec<float>& other) {
            for (size_t i = 0; i < arrlen; i++) {
                vec_op_set(other, i);
            }
        }

        inline void vec_op_set_mask(const automi_bitvec<bool>& mask, const automi_bitvec<float>& other) {
            for (size_t i = 0; i < arrlen; i++) {
                vec_op_set_mask(other, mask, i);
            }
        }

        inline void vec_op_set(float val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_set1_ps(val);
            }
        }

        inline void vec_op_set_mask(const automi_bitvec<bool>& mask, float val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mask_blend_ps(array[i], mask.array[i], _mm256_set1_ps(val));
            }
        }

        inline void vec_op_div_update(const automi_bitvec<float>& other, float val, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            array[arrpos] = _mm256_div_ps(other.array[arrpos], _mm256_set1_ps(val));
        }

        inline void vec_op_div_update(const automi_bitvec<float>& other, float val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_div_ps(other.array[i], _mm256_set1_ps(val));
            }
        }

        inline void vec_op_div_update(const automi_bitvec<float>& other1, const automi_bitvec<float>& other2, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            array[arrpos] = _mm256_div_ps(other1.array[arrpos], other2.array[arrpos]);
        }

        inline void vec_op_masked_div_update(const automi_bitvec<bool>& mask, const automi_bitvec<float>& other, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_div_ps(array[arrpos], mask.array[arrpos], array[arrpos], other.array[arrpos]);
        }

        inline void vec_op_mul_update(const automi_bitvec<float>& other, float val, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            array[arrpos] = _mm256_mul_ps(other.array[arrpos], _mm256_set1_ps(val));
        }

        inline void vec_op_mul_update(const automi_bitvec<float>& other, float val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mul_ps(other.array[i], _mm256_set1_ps(val));
            }
        }

        inline void vec_op_mul_update_mask(const automi_bitvec<bool>& mask, const automi_bitvec<float>& other, float val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mask_mul_ps(array[i], mask.array[i], other.array[i], _mm256_set1_ps(val));
            }
        }

        inline void vec_op_masked_mul_update(const automi_bitvec<bool>& mask, const automi_bitvec<float>& other, float val, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_mul_ps(array[arrpos], mask.array[arrpos], other.array[arrpos], _mm256_set1_ps(val));
        }

        inline void vec_op_mul_val_update(float val, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            array[arrpos] = _mm256_mul_ps(array[arrpos], _mm256_set1_ps(val));
        }

        inline void vec_op_masked_add_update(const automi_bitvec<bool>& mask, float val, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_add_ps(array[arrpos], mask.array[arrpos], array[arrpos], _mm256_set1_ps(val));
        }

        inline void vec_op_masked_add_update(const automi_bitvec<bool>& mask, const automi_bitvec<float>& other, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_mask_add_ps(array[arrpos], mask.array[arrpos], array[arrpos], other.array[arrpos]);
        }

        inline void vec_op_add_update(element val, size_t b) {
            size_t arrpos = b;  // THIS IS SPECIAL!
            array[arrpos] = _mm256_add_ps(array[arrpos], val);
        }

        inline void vec_op_add_update(const automi_bitvec<float>& other, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_add_ps(array[arrpos], other.array[arrpos]);
        }

        inline void vec_op_add_update_mask(const automi_bitvec<bool>& mask, float val) {
            for (size_t i = 0; i < arrlen; i++) {
                array[i] = _mm256_mask_add_ps(array[i], mask.array[i], array[i], _mm256_set1_ps(val));
            }
        }

        inline void vec_op_add_update(const automi_bitvec<int>& other, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_add_ps(array[arrpos], _mm256_cvtepi32_ps(other.array[arrpos]));
        }

        inline void vec_op_add_update(const automi_bitvec<bool>& other, size_t b) {
            size_t arrpos = b;
            array[arrpos] = _mm256_add_ps(array[arrpos], _mm256_set1_ps(other.array[arrpos]));
        }

        /**
         * Return 
         */
        element vec_op_add_val(size_t b, float val) const {
            size_t arrpos = b;
            return _mm256_add_ps(array[arrpos], _mm256_set1_ps(val));
        }

        /**
         * Return 
         */
        element vec_op_min_val(size_t b, float val) const {
            size_t arrpos = b;
            return _mm256_min_ps(array[arrpos], _mm256_set1_ps(val));
        }

        __m256* array;
        size_t len;
        size_t arrlen;
    private:
        inline static void bit_to_pos(size_t b, size_t& arrpos, size_t& bitpos) {
            arrpos = b / 8;
            bitpos = b % 8;
        }
    };

}

#endif  // GRAPHLAB_automi_bitvec_HPP
