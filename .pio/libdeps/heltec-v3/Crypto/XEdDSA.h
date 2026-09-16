//#include <Arduino.h>
#include "Ed25519.h"

// imported from SUPERCOP by Daniel J. Bernstein which is public domain

// Byte-representation of the scalar value of 0 on the Ed25519 curve. Needed by `sc_neg`.
static const uint8_t ZERO[32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Byte-representation of the scalar value of -1 on the Ed25519 curve. Needed by `sc_neg`.
static const uint8_t MINUS_ONE[32] = {0xec, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7,
                                      0xa2, 0xde, 0xf9, 0xde, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};

typedef uint64_t crypto_uint64;
typedef int64_t crypto_int64;

typedef int32_t crypto_int32;

typedef crypto_int32 fe[10];

static crypto_uint64 load_3(const unsigned char *in);

static crypto_uint64 load_4(const unsigned char *in);

void sc_muladd(unsigned char *s, const unsigned char *a, const unsigned char *b, const unsigned char *c);

void fe_sub(fe h, fe f, fe g);

void fe_frombytes(fe h, const unsigned char *s);

void fe_1(fe h);

void fe_add(fe h, const fe f, const fe g);

void fe_sq(fe h, const fe f);

void fe_mul(fe h, const fe f, const fe g);

void fe_invert(fe out, const fe z);

void fe_tobytes(unsigned char *s, const fe h);

class XEdDSA : public Ed25519
{
public:
    static void priv_curve_to_ed_keys(uint8_t *curve_privkey, uint8_t *ed_privkey, uint8_t *ed_pubkey);
    static void sign(uint8_t signature[64], const uint8_t privateKey[32],
                   const uint8_t publicKey[32], const void *message, size_t len);
private:
        static void sc_muladd(unsigned char *s, const unsigned char *a, const unsigned char *b, const unsigned char *c);
        static void deriveKeys(SHA512 *hash, limb_t *a, const uint8_t privateKey[32]);


};
