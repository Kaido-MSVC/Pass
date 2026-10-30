#include <windows.h>
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <QtTest>
#include "wordlist.h"
#include <functional>
namespace probe {
int calls=0, secretWipes=0, nonzeroSecretWipes=0;
bool zero=true;
std::function<int(unsigned char*,size_t)> provider;
int random(EVP_RAND_CTX* ctx, unsigned char* b, size_t n, unsigned int strength, int prediction, const unsigned char* extra, size_t extraSize) {
    ++calls;
    if(!ctx || n!=256 || strength!=256 || prediction!=0 || extra || extraSize) throw std::runtime_error("OpenSSL contract");
    return provider ? provider(b,n) : ::EVP_RAND_generate(ctx,b,n,strength,prediction,extra,extraSize);
}
PVOID wipe(PVOID b,SIZE_T n) {
    if(n==238) { ++secretWipes; for(SIZE_T i=0;i<n;++i) if(static_cast<unsigned char*>(b)[i]) {++nonzeroSecretWipes;break;} }
    auto result=::SecureZeroMemory(b,n);
    for(SIZE_T i=0;i<n;++i) if(static_cast<unsigned char*>(b)[i]) zero=false;
    return result;
}
}
#define EVP_RAND_generate probe::random
#undef SecureZeroMemory
#define SecureZeroMemory probe::wipe
#include "../src/secure_random.cpp"
#include "../src/passphrase.cpp"
#undef EVP_RAND_generate
#undef SecureZeroMemory
class PhraseSecurity : public QObject {
    Q_OBJECT
private slots:
    void init() { probe::calls=probe::secretWipes=probe::nonzeroSecretWipes=0; probe::zero=true; probe::provider={}; }
    void failureWipesCandidate() {
        pass::PassphraseRules r; r.words=12;
        probe::provider=[](unsigned char*,size_t){return 0;};
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(r));
        QCOMPARE(probe::secretWipes,1); QVERIFY(probe::zero);
    }
    void partialCandidateWipedOnRefillFailure() {
        pass::PassphraseRules r; r.words=12;
        // First word accepted; fill the rest with rejected 65535 values, then fail refill.
        probe::provider=[](unsigned char* b,size_t n){ if(probe::calls==2)return 0; std::fill(b,b+n,255); b[0]=b[1]=0; return 1; };
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(r));
        QCOMPARE(probe::calls,2); QCOMPARE(probe::secretWipes,1); QVERIFY(probe::zero);
        QCOMPARE(probe::nonzeroSecretWipes,1);
    }
    void cancellationWipesPartialAndCompletedCandidates() {
        pass::PassphraseRules r; r.words=12;
        for (int stop : {1,7,61}) {
            int polls=0; probe::secretWipes=probe::nonzeroSecretWipes=0;
            probe::provider=[](unsigned char* b,size_t n){std::fill(b,b+n,0);return 1;};
            QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(r,[&]{return ++polls==stop;}));
            QCOMPARE(probe::secretWipes,1); QVERIFY(probe::zero);
            QCOMPARE(probe::nonzeroSecretWipes,stop==1?0:1);
        }
    }
    void throwingCancellationWipesCandidate() {
        pass::PassphraseRules r; r.words=12;
        probe::provider=[](unsigned char* b,size_t n){std::fill(b,b+n,0);return 1;};
        int polls=0;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(r,[&]()->bool{if(++polls==7)throw std::runtime_error("callback");return false;}));
        QCOMPARE(probe::secretWipes,1); QVERIFY(probe::zero);
    }
    void rejectionBudgetWipesCandidate() {
        pass::PassphraseRules r; r.words=12;
        probe::provider=[](unsigned char* b,size_t n){std::fill(b,b+n,255);return 1;};
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(r));
        QCOMPARE(probe::calls,8); QCOMPARE(probe::secretWipes,1); QVERIFY(probe::zero);
    }
    void successRetainsResultAndAllowsRepeats() {
        pass::PassphraseRules r; r.words=12;
        probe::provider=[](unsigned char* b,size_t n){std::fill(b,b+n,0);return 1;};
        QString result=pass::generate(r); QCOMPARE(result,QStringList(12,QStringLiteral("abacus")).join('-'));
        QCOMPARE(probe::secretWipes,0); QVERIFY(probe::zero); wipeText(result); QVERIFY(result.isEmpty());
    }
};
QTEST_GUILESS_MAIN(PhraseSecurity)
#include "passphrase_security_test.moc"
