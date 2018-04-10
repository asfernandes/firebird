/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtom.org
 */
#include "tomcrypt.h"

/**
  @file ctr_test.c
  CTR implementation, Tests again RFC 3686, Tom St Denis
*/

#ifdef LTC_CTR_MODE

int ctr_test(void)
{
#ifdef LTC_NO_TEST
   return CRYPT_NOP;
#else
   static const struct {
      int keylen, msglen;
      unsigned char key[32], IV[16], pt[64], ct[64];
   } tests[] = {
/* 128-bit key, 16-byte pt */
{
   16, 16,
   {0xAE,0x68,0x52,0xF8,0x12,0x10,0x67,0xCC,0x4B,0xF7,0xA5,0x76,0x55,0x77,0xF3,0x9E },
   {0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },
   {0x53,0x69,0x6E,0x67,0x6C,0x65,0x20,0x62,0x6C,0x6F,0x63,0x6B,0x20,0x6D,0x73,0x67 },
   {0xE4,0x09,0x5D,0x4F,0xB7,0xA7,0xB3,0x79,0x2D,0x61,0x75,0xA3,0x26,0x13,0x11,0xB8 },
},

/* 128-bit key, 36-byte pt */
{
   16, 36,
   {0x76,0x91,0xBE,0x03,0x5E,0x50,0x20,0xA8,0xAC,0x6E,0x61,0x85,0x29,0xF9,0xA0,0xDC },
   {0x00,0xE0,0x01,0x7B,0x27,0x77,0x7F,0x3F,0x4A,0x17,0x86,0xF0,0x00,0x00,0x00,0x00 },
   {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
    0x20,0x21,0x22,0x23},
   {0xC1,0xCF,0x48,0xA8,0x9F,0x2F,0xFD,0xD9,0xCF,0x46,0x52,0xE9,0xEF,0xDB,0x72,0xD7,
    0x45,0x40,0xA4,0x2B,0xDE,0x6D,0x78,0x36,0xD5,0x9A,0x5C,0xEA,0xAE,0xF3,0x10,0x53,
    0x25,0xB2,0x07,0x2F },
},
};
  int idx, err, x;
  unsigned char buf[64];
  symmetric_CTR ctr;

  /* AES can be under rijndael or aes... try to find it */ 
  if ((idx = find_cipher("aes")) == -1) {
     if ((idx = find_cipher("rijndael")) == -1) {
        return CRYPT_NOP;
     }
  }

  for (x = 0; x < (int)(sizeof(tests)/sizeof(tests[0])); x++) {
     if ((err = ctr_start(idx, tests[x].IV, tests[x].key, tests[x].keylen, 0, CTR_COUNTER_BIG_ENDIAN|LTC_CTR_RFC3686, &ctr)) != CRYPT_OK) {
        return err;
     }
     if ((err = ctr_encrypt(tests[x].pt, buf, tests[x].msglen, &ctr)) != CRYPT_OK) {
        return err;
     }
     ctr_done(&ctr);
     if (XMEMCMP(buf, tests[x].ct, tests[x].msglen)) {
        return CRYPT_FAIL_TESTVECTOR;
     }
  }
  return CRYPT_OK;
#endif
}

#endif

/* $Source$ */
/* $Revision$ */
/* $Date$ */



