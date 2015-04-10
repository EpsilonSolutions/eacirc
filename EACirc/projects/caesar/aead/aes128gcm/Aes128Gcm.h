#ifndef AES128GCM_H
#define AES128GCM_H

#include "../../CaesarInterface.h"

class Aes128Gcm : public CaesarInterface {
    const int maxNumRounds = 10;
public:
    Aes128Gcm(int numRounds);
    ~Aes128Gcm();
    int encrypt(bits_t *c, length_t *clen, const bits_t *m, length_t mlen,
                        const bits_t *ad, length_t adlen, const bits_t *nsec, const bits_t *npub,
                        const bits_t *k);
    int decrypt(bits_t *m, length_t *outputmlen, bits_t *nsec,
                        const bits_t *c, length_t clen, const bits_t *ad, length_t adlen,
                        const bits_t *npub, const bits_t *k);
    std::string shortDescription() const;
};

#endif // AES128GCM_H
