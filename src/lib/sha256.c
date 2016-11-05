/*-
 * Copyright 2005,2007,2009 Colin Percival
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include "telehash.h"

static inline uint32_t
be32dec(const void *pp)
{
  const uint8_t *p = (uint8_t const *)pp;

  return ((uint32_t)(p[3]) + ((uint32_t)(p[2]) << 8) +
      ((uint32_t)(p[1]) << 16) + ((uint32_t)(p[0]) << 24));
}

static inline void
be32enc(void *pp, uint32_t x)
{
  uint8_t * p = (uint8_t *)pp;

  p[3] = x & 0xff;
  p[2] = (x >> 8) & 0xff;
  p[1] = (x >> 16) & 0xff;
  p[0] = (x >> 24) & 0xff;
}

/*
static inline uint64_t
be64dec(const void *pp)
{
  const uint8_t *p = (uint8_t const *)pp;

  return ((uint64_t)(p[7]) + ((uint64_t)(p[6]) << 8) +
      ((uint64_t)(p[5]) << 16) + ((uint64_t)(p[4]) << 24) +
      ((uint64_t)(p[3]) << 32) + ((uint64_t)(p[2]) << 40) +
      ((uint64_t)(p[1]) << 48) + ((uint64_t)(p[0]) << 56));
}

static inline void
be64enc(void *pp, uint64_t x)
{
  uint8_t * p = (uint8_t *)pp;

  p[7] = x & 0xff;
  p[6] = (x >> 8) & 0xff;
  p[5] = (x >> 16) & 0xff;
  p[4] = (x >> 24) & 0xff;
  p[3] = (x >> 32) & 0xff;
  p[2] = (x >> 40) & 0xff;
  p[1] = (x >> 48) & 0xff;
  p[0] = (x >> 56) & 0xff;
}

static inline uint32_t
le32dec(const void *pp)
{
  const uint8_t *p = (uint8_t const *)pp;

  return ((uint32_t)(p[0]) + ((uint32_t)(p[1]) << 8) +
      ((uint32_t)(p[2]) << 16) + ((uint32_t)(p[3]) << 24));
}

static inline void
le32enc(void *pp, uint32_t x)
{
  uint8_t * p = (uint8_t *)pp;

  p[0] = x & 0xff;
  p[1] = (x >> 8) & 0xff;
  p[2] = (x >> 16) & 0xff;
  p[3] = (x >> 24) & 0xff;
}

static inline uint64_t
le64dec(const void *pp)
{
  const uint8_t *p = (uint8_t const *)pp;

  return ((uint64_t)(p[0]) + ((uint64_t)(p[1]) << 8) +
      ((uint64_t)(p[2]) << 16) + ((uint64_t)(p[3]) << 24) +
      ((uint64_t)(p[4]) << 32) + ((uint64_t)(p[5]) << 40) +
      ((uint64_t)(p[6]) << 48) + ((uint64_t)(p[7]) << 56));
}

static inline void
le64enc(void *pp, uint64_t x)
{
  uint8_t * p = (uint8_t *)pp;

  p[0] = x & 0xff;
  p[1] = (x >> 8) & 0xff;
  p[2] = (x >> 16) & 0xff;
  p[3] = (x >> 24) & 0xff;
  p[4] = (x >> 32) & 0xff;
  p[5] = (x >> 40) & 0xff;
  p[6] = (x >> 48) & 0xff;
  p[7] = (x >> 56) & 0xff;
}
*/

typedef struct HMAC_SHA256Context {
  SHA256_CTX ictx;
  SHA256_CTX octx;
} HMAC_SHA256_CTX;

/*
 * Encode a length len/4 vector of (uint32_t) into a length len vector of
 * (unsigned char) in big-endian form.  Assumes len is a multiple of 4.
 */
static void
be32enc_vect(unsigned char *dst, const uint32_t *src, size_t len)
{
  size_t i;

  for (i = 0; i < len / 4; i++)
    be32enc(dst + i * 4, src[i]);
}

/*
 * Decode a big-endian length len vector of (unsigned char) into a length
 * len/4 vector of (uint32_t).  Assumes len is a multiple of 4.
 */
static void
be32dec_vect(uint32_t *dst, const unsigned char *src, size_t len)
{
  size_t i;

  for (i = 0; i < len / 4; i++)
    dst[i] = be32dec(src + i * 4);
}

/* Elementary functions used by SHA256 */
#define Ch(x, y, z)  ((x & (y ^ z)) ^ z)
#define Maj(x, y, z)  ((x & (y | z)) | (y & z))
#define SHR(x, n)  (x >> n)
#define ROTR(x, n)  ((x >> n) | (x << (32 - n)))
#define S0(x)    (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define S1(x)    (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define s0(x)    (ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define s1(x)    (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))

/* SHA256 round function */
#define RND(a, b, c, d, e, f, g, h, k)      \
  t0 = h + S1(e) + Ch(e, f, g) + k;    \
  t1 = S0(a) + Maj(a, b, c);      \
  d += t0;          \
  h  = t0 + t1;

/* Adjusted round function for rotating state */
#define RNDr(S, W, i, k)      \
  RND(S[(64 - i) % 8], S[(65 - i) % 8],  \
      S[(66 - i) % 8], S[(67 - i) % 8],  \
      S[(68 - i) % 8], S[(69 - i) % 8],  \
      S[(70 - i) % 8], S[(71 - i) % 8],  \
      W[i] + k)

/*
 * SHA256 block compression function.  The 256-bit state is transformed via
 * the 512-bit input block to produce a new state.
 */
static void
SHA256_Transform(uint32_t * state, const unsigned char block[64])
{
  uint32_t W[64];
  uint32_t S[8];
  uint32_t t0, t1;
  int i;

  /* 1. Prepare message schedule W. */
  be32dec_vect(W, block, 64);
  for (i = 16; i < 64; i++)
    W[i] = s1(W[i - 2]) + W[i - 7] + s0(W[i - 15]) + W[i - 16];

  /* 2. Initialize working variables. */
  memcpy(S, state, 32);

  /* 3. Mix. */
  RNDr(S, W, 0, 0x428a2f98);
  RNDr(S, W, 1, 0x71374491);
  RNDr(S, W, 2, 0xb5c0fbcf);
  RNDr(S, W, 3, 0xe9b5dba5);
  RNDr(S, W, 4, 0x3956c25b);
  RNDr(S, W, 5, 0x59f111f1);
  RNDr(S, W, 6, 0x923f82a4);
  RNDr(S, W, 7, 0xab1c5ed5);
  RNDr(S, W, 8, 0xd807aa98);
  RNDr(S, W, 9, 0x12835b01);
  RNDr(S, W, 10, 0x243185be);
  RNDr(S, W, 11, 0x550c7dc3);
  RNDr(S, W, 12, 0x72be5d74);
  RNDr(S, W, 13, 0x80deb1fe);
  RNDr(S, W, 14, 0x9bdc06a7);
  RNDr(S, W, 15, 0xc19bf174);
  RNDr(S, W, 16, 0xe49b69c1);
  RNDr(S, W, 17, 0xefbe4786);
  RNDr(S, W, 18, 0x0fc19dc6);
  RNDr(S, W, 19, 0x240ca1cc);
  RNDr(S, W, 20, 0x2de92c6f);
  RNDr(S, W, 21, 0x4a7484aa);
  RNDr(S, W, 22, 0x5cb0a9dc);
  RNDr(S, W, 23, 0x76f988da);
  RNDr(S, W, 24, 0x983e5152);
  RNDr(S, W, 25, 0xa831c66d);
  RNDr(S, W, 26, 0xb00327c8);
  RNDr(S, W, 27, 0xbf597fc7);
  RNDr(S, W, 28, 0xc6e00bf3);
  RNDr(S, W, 29, 0xd5a79147);
  RNDr(S, W, 30, 0x06ca6351);
  RNDr(S, W, 31, 0x14292967);
  RNDr(S, W, 32, 0x27b70a85);
  RNDr(S, W, 33, 0x2e1b2138);
  RNDr(S, W, 34, 0x4d2c6dfc);
  RNDr(S, W, 35, 0x53380d13);
  RNDr(S, W, 36, 0x650a7354);
  RNDr(S, W, 37, 0x766a0abb);
  RNDr(S, W, 38, 0x81c2c92e);
  RNDr(S, W, 39, 0x92722c85);
  RNDr(S, W, 40, 0xa2bfe8a1);
  RNDr(S, W, 41, 0xa81a664b);
  RNDr(S, W, 42, 0xc24b8b70);
  RNDr(S, W, 43, 0xc76c51a3);
  RNDr(S, W, 44, 0xd192e819);
  RNDr(S, W, 45, 0xd6990624);
  RNDr(S, W, 46, 0xf40e3585);
  RNDr(S, W, 47, 0x106aa070);
  RNDr(S, W, 48, 0x19a4c116);
  RNDr(S, W, 49, 0x1e376c08);
  RNDr(S, W, 50, 0x2748774c);
  RNDr(S, W, 51, 0x34b0bcb5);
  RNDr(S, W, 52, 0x391c0cb3);
  RNDr(S, W, 53, 0x4ed8aa4a);
  RNDr(S, W, 54, 0x5b9cca4f);
  RNDr(S, W, 55, 0x682e6ff3);
  RNDr(S, W, 56, 0x748f82ee);
  RNDr(S, W, 57, 0x78a5636f);
  RNDr(S, W, 58, 0x84c87814);
  RNDr(S, W, 59, 0x8cc70208);
  RNDr(S, W, 60, 0x90befffa);
  RNDr(S, W, 61, 0xa4506ceb);
  RNDr(S, W, 62, 0xbef9a3f7);
  RNDr(S, W, 63, 0xc67178f2);

  /* 4. Mix local working variables into global state */
  for (i = 0; i < 8; i++)
    state[i] += S[i];

  /* Clean the stack. */
  memset(W, 0, 256);
  memset(S, 0, 32);
  t0 = t1 = 0;
}

/* SHA-256 initialization.  Begins a SHA-256 operation. */
void
SHA256_Init(SHA256_CTX * ctx)
{

  /* Zero bits processed so far */
  ctx->count[0] = ctx->count[1] = 0;

  /* Magic initialization constants */
  ctx->state[0] = 0x6A09E667;
  ctx->state[1] = 0xBB67AE85;
  ctx->state[2] = 0x3C6EF372;
  ctx->state[3] = 0xA54FF53A;
  ctx->state[4] = 0x510E527F;
  ctx->state[5] = 0x9B05688C;
  ctx->state[6] = 0x1F83D9AB;
  ctx->state[7] = 0x5BE0CD19;
}

/* Add bytes into the hash */
void
SHA256_Update(SHA256_CTX * ctx, const void *in, size_t len)
{
  uint32_t bitlen[2];
  uint32_t r;
  const unsigned char *src = in;

  /* Number of bytes left in the buffer from previous updates */
  r = (ctx->count[1] >> 3) & 0x3f;

  /* Convert the length into a number of bits */
  bitlen[1] = ((uint32_t)len) << 3;
  bitlen[0] = (uint32_t)(len >> 29);

  /* Update number of bits */
  if ((ctx->count[1] += bitlen[1]) < bitlen[1])
    ctx->count[0]++;
  ctx->count[0] += bitlen[0];

  /* Handle the case where we don't need to perform any transforms */
  if (len < 64 - r) {
    memcpy(&ctx->buf[r], src, len);
    return;
  }

  /* Finish the current block */
  memcpy(&ctx->buf[r], src, 64 - r);
  SHA256_Transform(ctx->state, ctx->buf);
  src += 64 - r;
  len -= 64 - r;

  /* Perform complete blocks */
  while (len >= 64) {
    SHA256_Transform(ctx->state, src);
    src += 64;
    len -= 64;
  }

  /* Copy left over data into buffer */
  memcpy(ctx->buf, src, len);
}

/* Add padding and terminating bit-count. */
static void
SHA256_Pad(SHA256_CTX * ctx)
{
  unsigned char len[8];
  uint32_t r, plen;
  unsigned char PAD[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };


  /*
   * Convert length to a vector of bytes -- we do this now rather
   * than later because the length will change after we pad.
   */
  be32enc_vect(len, ctx->count, 8);

  /* Add 1--64 bytes so that the resulting length is 56 mod 64 */
  r = (ctx->count[1] >> 3) & 0x3f;
  plen = (r < 56) ? (56 - r) : (120 - r);
  SHA256_Update(ctx, PAD, (size_t)plen);

  /* Add the terminating bit-count */
  SHA256_Update(ctx, len, 8);
}

/*
 * SHA-256 finalization.  Pads the input data, exports the hash value,
 * and clears the context state.
 */
void
SHA256_Final(unsigned char digest[32], SHA256_CTX * ctx)
{

  /* Add padding */
  SHA256_Pad(ctx);

  /* Write the hash */
  be32enc_vect(digest, ctx->state, 32);

  /* Clear the context state */
  memset((void *)ctx, 0, sizeof(*ctx));
}

/* Initialize an HMAC-SHA256 operation with the given key. */
void
HMAC_SHA256_Init(HMAC_SHA256_CTX * ctx, const void * _K, size_t Klen)
{
  unsigned char pad[64];
  unsigned char khash[32];
  const unsigned char * K = _K;
  size_t i;

  /* If Klen > 64, the key is really SHA256(K). */
  if (Klen > 64) {
    SHA256_Init(&ctx->ictx);
    SHA256_Update(&ctx->ictx, K, Klen);
    SHA256_Final(khash, &ctx->ictx);
    K = khash;
    Klen = 32;
  }

  /* Inner SHA256 operation is SHA256(K xor [block of 0x36] || data). */
  SHA256_Init(&ctx->ictx);
  memset(pad, 0x36, 64);
  for (i = 0; i < Klen; i++)
    pad[i] ^= K[i];
  SHA256_Update(&ctx->ictx, pad, 64);

  /* Outer SHA256 operation is SHA256(K xor [block of 0x5c] || hash). */
  SHA256_Init(&ctx->octx);
  memset(pad, 0x5c, 64);
  for (i = 0; i < Klen; i++)
    pad[i] ^= K[i];
  SHA256_Update(&ctx->octx, pad, 64);

  /* Clean the stack. */
  memset(khash, 0, 32);
}

/* Add bytes to the HMAC-SHA256 operation. */
void
HMAC_SHA256_Update(HMAC_SHA256_CTX * ctx, const void *in, size_t len)
{

  /* Feed data to the inner SHA256 operation. */
  SHA256_Update(&ctx->ictx, in, len);
}

/* Finish an HMAC-SHA256 operation. */
void
HMAC_SHA256_Final(unsigned char digest[32], HMAC_SHA256_CTX * ctx)
{
  unsigned char ihash[32];

  /* Finish the inner SHA256 operation. */
  SHA256_Final(ihash, &ctx->ictx);

  /* Feed the inner hash to the outer SHA256 operation. */
  SHA256_Update(&ctx->octx, ihash, 32);

  /* Finish the outer SHA256 operation. */
  SHA256_Final(digest, &ctx->octx);

  /* Clean the stack. */
  memset(ihash, 0, 32);
}

/**
 * PBKDF2_SHA256(passwd, passwdlen, salt, saltlen, c, buf, dkLen):
 * Compute PBKDF2(passwd, salt, c, dkLen) using HMAC-SHA256 as the PRF, and
 * write the output to buf.  The value dkLen must be at most 32 * (2^32 - 1).
 */
void
PBKDF2_SHA256(const uint8_t * passwd, size_t passwdlen, const uint8_t * salt,
    size_t saltlen, uint64_t c, uint8_t * buf, size_t dkLen)
{
  HMAC_SHA256_CTX PShctx, hctx;
  size_t i;
  uint8_t ivec[4];
  uint8_t U[32];
  uint8_t T[32];
  uint64_t j;
  int k;
  size_t clen;

  /* Compute HMAC state after processing P and S. */
  HMAC_SHA256_Init(&PShctx, passwd, passwdlen);
  HMAC_SHA256_Update(&PShctx, salt, saltlen);

  /* Iterate through the blocks. */
  for (i = 0; i * 32 < dkLen; i++) {
    /* Generate INT(i + 1). */
    be32enc(ivec, (uint32_t)(i + 1));

    /* Compute U_1 = PRF(P, S || INT(i)). */
    memcpy(&hctx, &PShctx, sizeof(HMAC_SHA256_CTX));
    HMAC_SHA256_Update(&hctx, ivec, 4);
    HMAC_SHA256_Final(U, &hctx);

    /* T_i = U_1 ... */
    memcpy(T, U, 32);

    for (j = 2; j <= c; j++) {
      /* Compute U_j. */
      HMAC_SHA256_Init(&hctx, passwd, passwdlen);
      HMAC_SHA256_Update(&hctx, U, 32);
      HMAC_SHA256_Final(U, &hctx);

      /* ... xor U_j ... */
      for (k = 0; k < 32; k++)
        T[k] ^= U[k];
    }

    /* Copy as many bytes as necessary into buf. */
    clen = dkLen - i * 32;
    if (clen > 32)
      clen = 32;
    memcpy(&buf[i * 32], T, clen);
  }

  /* Clean PShctx, since we never called _Final on it. */
  memset(&PShctx, 0, sizeof(HMAC_SHA256_CTX));
}
void sha256( const unsigned char *input, size_t ilen,
             unsigned char output[32], int is224)
{
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, input, ilen);
  SHA256_Final(output, &ctx);
}

void sha256_hmac( const unsigned char *key, size_t keylen,
                  const unsigned char *input, size_t ilen,
                  unsigned char output[32], int is224 )
{
  HMAC_SHA256_CTX hctx;
  HMAC_SHA256_Init(&hctx, key, keylen);
  HMAC_SHA256_Update(&hctx, input, ilen);
  HMAC_SHA256_Final(output, &hctx);
}

void hmac_256(const unsigned char *key, size_t keylen, const unsigned char *input, size_t ilen, unsigned char output[32])
{
  sha256_hmac(key, keylen, input, ilen, output, 0);
}

/*
   Implements the HKDF algorithm (HMAC-based Extract-and-Expand Key
   Derivation Function, RFC 5869).
   This implementation is adapted from IETF's HKDF implementation,
   see associated license below.
*/
/*
   Copyright (c) 2011 IETF Trust and the persons identified as
   authors of the code.  All rights reserved.
   Redistribution and use in source and binary forms, with or
   without modification, are permitted provided that the following
   conditions are met:
   - Redistributions of source code must retain the above
     copyright notice, this list of conditions and
     the following disclaimer.
   - Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.
   - Neither the name of Internet Society, IETF or IETF Trust, nor
     the names of specific contributors, may be used to endorse or
     promote products derived from this software without specific
     prior written permission.
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
   EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define SHA256_DIGEST_SIZE 32

/*
 *  hkdf_sha256_extract
 *
 *  Description:
 *      This function will perform HKDF extraction with SHA256.
 *
 *  Parameters:
 *      salt: [in]
 *          The optional salt value (a non-secret random value);
 *          if not provided (salt == NULL), it is set internally
 *          to a string of HashLen(hmac_algorithm) zeros.
 *      salt_len: [in]
 *          The length of the salt value.  (Ignored if salt == NULL.)
 *      ikm: [in]
 *          Input keying material.
 *      ikm_len: [in]
 *          The length of the input keying material.
 *      prk: [out]
 *          Array where the HKDF extraction is to be stored.
 *          Must be euqual to or larger than 32 bytes
 *
 *  Returns:
 *      0 on success, -1 on error.
 *
 */
static int hkdf_sha256_extract(
uint8_t  *salt,
uint32_t      salt_len,
uint8_t  *ikm,
uint32_t      ikm_len,
uint8_t       *prk
)
{
  uint8_t null_salt[SHA256_DIGEST_SIZE];

  if (ikm == NULL)
  {
    LOG_DEBUG("Error: incorrect input parameter for hkdf_sha256_extract");
    return -1;
  }

  if (salt == NULL) {
    salt = null_salt;
    salt_len = SHA256_DIGEST_SIZE;
    memset(null_salt, '\0', salt_len);
  }

  hmac_256(salt, salt_len,
              ikm,  ikm_len,
              prk);
  return 0;
}

/*
 *  hkdf_sha256_expand
 *
 *  Description:
 *      This function will perform HKDF with SHA256 expansion.
 *
 *  Parameters:
 *      prk: [in]
 *          The pseudo-random key to be expanded; either obtained
 *          directly from a cryptographically strong, uniformly
 *          distributed pseudo-random number generator, or as the
 *          output from hkdfExtract().
 *      prk_len: [in]
 *          The length of the pseudo-random key in prk;
 *          should at least be equal to HashSize(hmac_algorithm).
 *      info: [in]
 *          The optional context and application specific information.
 *          If info == NULL or a zero-length string, it is ignored.
 *      info_len: [in]
 *          The length of the optional context and application specific
 *          information. (Ignored if info == NULL.)
 *      okm: [out]
 *          Where the HKDF is to be stored.
 *      okm_len: [in]
 *          The length of the buffer to hold okm.
 *          okm_len must be <= 255 * SHA256_DIGEST_SIZE
 *
 *  Returns:
 *      0 on success, -1 on error.
 *
 */
static int hkdf_sha256_expand
(
uint8_t   prk[],
uint32_t      prk_len,
uint8_t   *info,
uint32_t      info_len,
uint8_t       okm[],
uint32_t      okm_len
)
{
  uint32_t hash_len;
  uint32_t N;
  uint8_t T[SHA256_DIGEST_SIZE];
  uint32_t Tlen;
  uint32_t pos;
  uint32_t i;
  HMAC_SHA256_CTX* pctx;
  uint8_t c;

  if( ( prk_len == 0 ) || ( okm_len == 0 ) || ( okm == NULL ) )
  {
      LOG_DEBUG("Error: incorrect input parameter for hkdf_sha256");
    return -1;
  }

  pctx = malloc(sizeof(HMAC_SHA256_CTX));
  if(NULL == pctx)
  {
      LOG_DEBUG("Error: fail to allocate memory for hmac_sha256");
      return -2;
  }

  if (info == NULL) {
    info = (uint8_t *)"";
    info_len = 0;
  }

  hash_len = SHA256_DIGEST_SIZE;
  if (prk_len < hash_len)
  {
      LOG_DEBUG("Error: prk size (%d) is smaller than hash size (%d)", prk_len, hash_len);
      return -3;
  }
  N = okm_len / hash_len;
  if (okm_len % hash_len)
    N++;
  if (N > 255)
  {
      LOG_DEBUG("Error: incorrect input size (%d) for hkdf_sha256", N);
      free(pctx);
      return -4;
  }

  Tlen = 0;
  pos = 0;
  for (i = 1; i <= N; i++) {
    c = i;
    memset( pctx, 0, sizeof(HMAC_SHA256_CTX) );

    HMAC_SHA256_Init(pctx, prk, prk_len);
    HMAC_SHA256_Update(pctx, T, Tlen);
    HMAC_SHA256_Update(pctx, info, info_len);
    HMAC_SHA256_Update(pctx, &c, 1);
    HMAC_SHA256_Final(T, pctx);

    memcpy(okm + pos, T, (i != N) ? hash_len : (okm_len - pos));
    pos += hash_len;
    Tlen = hash_len;
  }
  memset( pctx, 0, sizeof(HMAC_SHA256_CTX) );
  free(pctx);
  return 0;
}

/*
 *  hkdf_sha256
 *
 *  Description:
 *      This function will generate keying material using HKDF with SHA256.
 *
 *  Parameters:
 *      salt: [in]
 *          The optional salt value (a non-secret random value);
 *          if not provided (salt == NULL), it is set internally
 *          to a string of HashLen(hmac_algorithm) zeros.
 *      salt_len: [in]
 *          The length of the salt value. (Ignored if salt == NULL.)
 *      ikm: [in]
 *          Input keying material.
 *      ikm_len: [in]
 *          The length of the input keying material.
 *      info: [in]
 *          The optional context and application specific information.
 *          If info == NULL or a zero-length string, it is ignored.
 *      info_len: [in]
 *          The length of the optional context and application specific
 *          information. (Ignored if info == NULL.)
 *      okm: [out]
 *          Where the HKDF is to be stored.
 *      okm_len: [in]
 *          The length of the buffer to hold okm.
 *          okm_len must be <= 255 * 32
 *
 *  Notes:
 *      Calls hkdfExtract() and hkdfExpand().
 *
 *  Returns:
 *      0 on success, <0 on error.
 *
 */
int hkdf_sha256( uint8_t *salt, uint32_t salt_len,
    uint8_t *ikm, uint32_t ikm_len,
    uint8_t *info, uint32_t info_len,
    uint8_t *okm, uint32_t okm_len)
{
  int32_t i_ret;
  uint8_t prk[SHA256_DIGEST_SIZE];

  i_ret = hkdf_sha256_extract(salt, salt_len, ikm, ikm_len, prk);
  if(0 != i_ret)
  {
    LOG_DEBUG("Error: fail to execute hkdf_sha256_extract()");
    return -1;
  }

  i_ret =  hkdf_sha256_expand(prk, SHA256_DIGEST_SIZE, info, info_len, okm, okm_len);
  if(0 != i_ret)
  {
    LOG_DEBUG("Error: fail to execute hkdf_sha256_expand()");
    return -2;
  }

  return 0;
}

