/**
* @file       SerialNumberSignatureOfKnowledge.cpp
*
* @brief      SerialNumberSignatureOfKnowledge class for the Zerocoin library.
*
* @author     Ian Miers, Christina Garman and Matthew Green
* @date       June 2013
*
* @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
* @license    This project is released under the MIT license.
**/
// Copyright (c) 2017 The PIVX developers
// Copyright (c) 2018 The Navcoin Core developers
// Copyright (c) 2019 The DeVault Core developers

#include <streams.h>
#include "SerialNumberSignatureOfKnowledge.h"

namespace libzerocoin {

SerialNumberSignatureOfKnowledge::SerialNumberSignatureOfKnowledge(const ZerocoinParams* p): params(p) { }

// Use one 256 bit seed and concatenate 4 unique 256 bit hashes to make a 1024 bit hash
CBigNum SeedTo1024(uint256 hashSeed) {
    CHashWriter hasher(0,0);
    hasher << hashSeed;

    vector<unsigned char> vResult;
    vector<unsigned char> vHash = CBigNum(hasher.GetHash()).getvch();
    vResult.insert(vResult.end(), vHash.begin(), vHash.end());
    for (int i = 0; i < 3; i ++) {
        hasher << vResult;
        vHash = CBigNum(hasher.GetHash()).getvch();
        vResult.insert(vResult.end(), vHash.begin(), vHash.end());
    }

    CBigNum bnResult;
    bnResult.setvch(vResult);
    return bnResult;
}

SerialNumberSignatureOfKnowledge::SerialNumberSignatureOfKnowledge(const
                                                                   ZerocoinParams* p, const PrivateCoin& coin, const Commitment& commitmentToCoin, const Commitment& coinValuePublic,
                                                                   uint256 msghash, CBigNum obfuscatedRandomness):params(p),
    s_notprime(p->zkp_iterations),
    sprime(p->zkp_iterations){

    // Sanity check: verify that the order of the "accumulatedValueCommitmentGroup" is
    // equal to the modulus of "coinCommitmentGroup". Otherwise we will produce invalid
    // proofs.
    if (params->coinCommitmentGroup.modulus != params->serialNumberSoKCommitmentGroup.groupOrder) {
        throw std::runtime_error("Groups are not structured correctly.");
    }

    CBigNum a = params->coinCommitmentGroup.g;
    CBigNum b = params->coinCommitmentGroup.h;
    CBigNum g = params->serialNumberSoKCommitmentGroup.g;
    CBigNum h = params->serialNumberSoKCommitmentGroup.h;

    CHashWriter hasher(0,0);
    hasher << *params << commitmentToCoin.getCommitmentValue() << coinValuePublic.getCommitmentValue() << msghash;

    vector<CBigNum> r(params->zkp_iterations);
    vector<CBigNum> r2(params->zkp_iterations);
    vector<CBigNum> v_seed(params->zkp_iterations);
    vector<CBigNum> v_expanded(params->zkp_iterations);
    vector<CBigNum> c(params->zkp_iterations);

    for(uint32_t i=0; i < params->zkp_iterations; i++) {
        r[i] = CBigNum::randBignum(params->coinCommitmentGroup.groupOrder);
        r2[i] = CBigNum::randBignum(params->coinCommitmentGroup.groupOrder);

        //use a random 256 bit seed that expands to 1024 bit for v[i]
        while (true) {
            uint256 hashRand = CBigNum::randBignum(CBigNum(~uint256(0))).getuint256();
            CBigNum bnExpanded = SeedTo1024(hashRand);

            if(bnExpanded > params->serialNumberSoKCommitmentGroup.groupOrder)
                continue;

            v_seed[i] = CBigNum(hashRand);
            v_expanded[i] = bnExpanded;
            break;
        }
    }

    for(uint32_t i=0; i < params->zkp_iterations; i++) {
        // compute g^{ {a^x b^r} h^v} mod p2
        c[i] = challengeCalculation(g, coinValuePublic.getCommitmentValue(), 1, b, r[i], h, v_expanded[i]);
    }

    // We can't hash data in parallel either
    // because OPENMP cannot not guarantee loops
    // execute in order.
    for(uint32_t i=0; i < params->zkp_iterations; i++) {
        hasher << c[i];
    }
    this->hash = hasher.GetHash();
    unsigned char *hashbytes =  (unsigned char*) &hash;

    for(uint32_t i = 0; i < params->zkp_iterations; i++) {
        int bit = i % 8;
        int byte = i / 8;

        bool challenge_bit = ((hashbytes[byte] >> bit) & 0x01);
        if (challenge_bit) {
            s_notprime[i]       = r[i];
            sprime[i]           = v_seed[i];
        } else {
            s_notprime[i]       = r[i] - obfuscatedRandomness;
            sprime[i]           = v_expanded[i] - (commitmentToCoin.getRandomness() *
                                  b.pow_mod(r[i] - obfuscatedRandomness, params->serialNumberSoKCommitmentGroup.groupOrder));
        }
    }
}

inline CBigNum SerialNumberSignatureOfKnowledge::challengeCalculation(const CBigNum& g, const CBigNum& a,const CBigNum& a_exp,const CBigNum& b,const CBigNum& b_exp,
                                                                      const CBigNum& h, const CBigNum& h_exp) const
{
    return (g.pow_mod(challengeCalculation(a,a_exp,b,b_exp), params->serialNumberSoKCommitmentGroup.modulus) * h.pow_mod(h_exp, params->serialNumberSoKCommitmentGroup.modulus)) % params->serialNumberSoKCommitmentGroup.modulus;
}

inline CBigNum SerialNumberSignatureOfKnowledge::challengeCalculation(const CBigNum& a,const CBigNum& a_exp,const CBigNum& b,const CBigNum& b_exp) const
{
    return (a.pow_mod(a_exp, params->serialNumberSoKCommitmentGroup.groupOrder) * b.pow_mod(b_exp, params->serialNumberSoKCommitmentGroup.groupOrder)) % params->serialNumberSoKCommitmentGroup.groupOrder;
}

bool SerialNumberSignatureOfKnowledge::Verify(const CBigNum& valueOfCommitmentToCoin, const CBigNum& valueOfPublicCoin,
                                              const uint256 msghash) const {
    CBigNum a = params->coinCommitmentGroup.g;
    CBigNum b = params->coinCommitmentGroup.h;
    CBigNum g = params->serialNumberSoKCommitmentGroup.g;
    CBigNum h = params->serialNumberSoKCommitmentGroup.h;
    CHashWriter hasher(0,0);
    hasher << *params << valueOfCommitmentToCoin << valueOfPublicCoin << msghash;

    vector<CBigNum> tprime(params->zkp_iterations);
    unsigned char *hashbytes = (unsigned char*) &this->hash;

    for(uint32_t i = 0; i < params->zkp_iterations; i++) {
        int bit = i % 8;
        int byte = i / 8;
        bool challenge_bit = ((hashbytes[byte] >> bit) & 0x01);
        if(challenge_bit) {
            tprime[i] = challengeCalculation(g, valueOfPublicCoin, 1, b, s_notprime[i], h, SeedTo1024(sprime[i].getuint256()));
        } else {
            CBigNum exp = b.pow_mod(s_notprime[i], params->serialNumberSoKCommitmentGroup.groupOrder);
            tprime[i] = ((valueOfCommitmentToCoin.pow_mod(exp, params->serialNumberSoKCommitmentGroup.modulus) % params->serialNumberSoKCommitmentGroup.modulus) *
                         (h.pow_mod(sprime[i], params->serialNumberSoKCommitmentGroup.modulus) % params->serialNumberSoKCommitmentGroup.modulus)) %
                        params->serialNumberSoKCommitmentGroup.modulus;
        }
    }
    for(uint32_t i = 0; i < params->zkp_iterations; i++) {
        hasher << tprime[i];
    }
    return hasher.GetHash() == hash;
}

} /* namespace libzerocoin */

