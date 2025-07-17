//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// Note - The x86 and x64 versions do _not_ produce the same results, as the
// algorithms are optimized for their respective platforms. You can still
// compile and run any of them on any platform, but your performance with the
// non-native version will be less than optimal.

#include "murmur.h"

#define	FORCE_INLINE inline static

/****
 *
 * Rotate left operation for 64-bit integers
 *
 * Performs a circular left rotation on a 64-bit value by the specified
 * number of bits. This is a core operation used in the MurmurHash algorithm
 * to mix bits and achieve good avalanche properties.
 *
 * Arguments:
 *   x - 64-bit value to rotate
 *   r - Number of bits to rotate left (0-63)
 *
 * Returns:
 *   64-bit value rotated left by r bits
 *
 ****/
FORCE_INLINE uint64_t rotl64 ( uint64_t x, int8_t r )
{
	return (x << r) | (x >> (64 - r));
}

#define ROTL64(x,y)	rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

#define getblock(x, i) (x[i])

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

/****
 *
 * Finalization mix for 64-bit hash values
 *
 * Applies a finalization mix to force all bits of a hash block to avalanche.
 * This function ensures that small changes in input produce large changes in
 * output by applying multiple rounds of bit mixing operations.
 *
 * Arguments:
 *   k - 64-bit hash value to finalize
 *
 * Returns:
 *   64-bit finalized hash value with improved avalanche properties
 *
 ****/
FORCE_INLINE uint64_t fmix64(uint64_t k)
{
	k ^= k >> 33;
	k *= BIG_CONSTANT(0xff51afd7ed558ccd);
	k ^= k >> 33;
	k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
	k ^= k >> 33;

	return k;
}

//-----------------------------------------------------------------------------

/****
 *
 * MurmurHash3 128-bit hash function optimized for x64 platforms
 *
 * Computes a 128-bit hash value using the MurmurHash3 algorithm optimized
 * for 64-bit processors. This implementation provides excellent distribution
 * and speed characteristics for hash table and data deduplication use cases.
 * The algorithm processes data in 16-byte blocks with a finalization step
 * for remaining bytes.
 *
 * Arguments:
 *   key - Pointer to the data to hash
 *   len - Length of the data in bytes
 *   seed - 32-bit seed value for hash initialization
 *   out - Pointer to 128-bit output buffer (16 bytes)
 *
 * Returns:
 *   Nothing (void) - result is stored in the out parameter
 *
 ****/
void MurmurHash3_x64_128 ( const void * key, const int len,
		const uint32_t seed, void * out )
{
	const uint8_t * data = (const uint8_t*)key;
	const int nblocks = len / 16;

	uint64_t h1 = seed;
	uint64_t h2 = seed;

	uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
	uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);

	int i;

	//----------
	// body

	const uint64_t * blocks = (const uint64_t *)(data);

	for(i = 0; i < nblocks; i++) {
		uint64_t k1 = getblock(blocks,i*2+0);
		uint64_t k2 = getblock(blocks,i*2+1);

		k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;

		h1 = ROTL64(h1,27); h1 += h2; h1 = h1*5+0x52dce729;

		k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

		h2 = ROTL64(h2,31); h2 += h1; h2 = h2*5+0x38495ab5;
	}

	//----------
	// tail

	const uint8_t * tail = (const uint8_t*)(data + nblocks*16);

	uint64_t k1 = 0;
	uint64_t k2 = 0;

	switch(len & 15) {
		case 15: k2 ^= ((uint64_t)tail[14]) << 48;
		case 14: k2 ^= ((uint64_t)tail[13]) << 40;
		case 13: k2 ^= ((uint64_t)tail[12]) << 32;
		case 12: k2 ^= ((uint64_t)tail[11]) << 24;
		case 11: k2 ^= ((uint64_t)tail[10]) << 16;
		case 10: k2 ^= ((uint64_t)tail[ 9]) << 8;
		case  9: k2 ^= ((uint64_t)tail[ 8]) << 0;
				 k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

		case  8: k1 ^= ((uint64_t)tail[ 7]) << 56;
		case  7: k1 ^= ((uint64_t)tail[ 6]) << 48;
		case  6: k1 ^= ((uint64_t)tail[ 5]) << 40;
		case  5: k1 ^= ((uint64_t)tail[ 4]) << 32;
		case  4: k1 ^= ((uint64_t)tail[ 3]) << 24;
		case  3: k1 ^= ((uint64_t)tail[ 2]) << 16;
		case  2: k1 ^= ((uint64_t)tail[ 1]) << 8;
		case  1: k1 ^= ((uint64_t)tail[ 0]) << 0;
				 k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;
	}

	//----------
	// finalization

	h1 ^= len; h2 ^= len;

	h1 += h2;
	h2 += h1;

	h1 = fmix64(h1);
	h2 = fmix64(h2);

	h1 += h2;
	h2 += h1;

	((uint64_t*)out)[0] = h1;
	((uint64_t*)out)[1] = h2;
}

//-----------------------------------------------------------------------------
