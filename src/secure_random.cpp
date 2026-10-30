#include "secure_random.h"
#include <windows.h>
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <stdexcept>
namespace pass {
namespace {
[[noreturn]] void randomError(RandomAlgorithm algorithm, const char* stage) {
    const auto code = ERR_get_error();
    char detail[256]{};
    if (code) ERR_error_string_n(code, detail, sizeof(detail));
    const char* name = algorithmName(algorithm);
    throw std::runtime_error((QStringLiteral("OpenSSL 安全亂數服務失敗，已停止產生。[%1 / %2] %3")
        .arg(QString::fromLatin1(name ? name : "invalid"), QString::fromLatin1(stage), QString::fromLatin1(detail))).toUtf8().constData());
}
struct Library {
    OSSL_LIB_CTX* context = nullptr;
    OSSL_PROVIDER* provider = nullptr;
    ~Library() { OSSL_PROVIDER_unload(provider); OSSL_LIB_CTX_free(context); }
};
std::shared_ptr<Library> randomLibrary(RandomAlgorithm algorithm) {
    // Cache provider algorithms per thread, never seed or DRBG state.
    // Each State retains ownership until its child and parent have been freed.
    static thread_local std::shared_ptr<Library> cached;
    if (!cached) {
        auto pending = std::make_shared<Library>();
        pending->context = OSSL_LIB_CTX_new();
        if (!pending->context) randomError(algorithm, "library context");
        pending->provider = OSSL_PROVIDER_load(pending->context, "default");
        if (!pending->provider) randomError(algorithm, "default provider");
        cached = std::move(pending);
    }
    return cached;
}
}
struct Random::State {
    std::shared_ptr<Library> library;
    EVP_RAND_CTX* seed = nullptr;
    EVP_RAND_CTX* drbg = nullptr;
    ~State() {
        if (drbg) { EVP_RAND_uninstantiate(drbg); EVP_RAND_CTX_free(drbg); }
        if (seed) { EVP_RAND_uninstantiate(seed); EVP_RAND_CTX_free(seed); }
    }
    void initialize(RandomAlgorithm algorithm) {
        ERR_clear_error();
        if (!algorithmName(algorithm)) randomError(algorithm, "selection");
        // A private library context is never configured from openssl.cnf or env.
        // The built-in default provider supplies both OS entropy and all supported DRBGs.
        library = randomLibrary(algorithm);
        EVP_RAND* method = EVP_RAND_fetch(library->context, "SEED-SRC", "provider=default");
        if (!method) randomError(algorithm, "entropy fetch");
        seed = EVP_RAND_CTX_new(method, nullptr);
        EVP_RAND_free(method);
        if (!seed || EVP_RAND_instantiate(seed, 256, 0, nullptr, 0, nullptr) != 1)
            randomError(algorithm, "entropy instantiate");
        method = EVP_RAND_fetch(library->context, algorithmName(algorithm), "provider=default");
        if (!method) randomError(algorithm, "DRBG fetch");
        drbg = EVP_RAND_CTX_new(method, seed);
        EVP_RAND_free(method);
        if (!drbg) randomError(algorithm, "DRBG context");
        char cipher[] = "AES-256-CTR", digest[] = "SHA256", digest512[] = "SHA512";
        int derivation = 1;
        OSSL_PARAM params[3];
        if (algorithm == RandomAlgorithm::Ctr) {
            params[0] = OSSL_PARAM_construct_utf8_string(OSSL_DRBG_PARAM_CIPHER, cipher, 0);
            params[1] = OSSL_PARAM_construct_int(OSSL_DRBG_PARAM_USE_DF, &derivation);
            params[2] = OSSL_PARAM_construct_end();
        } else {
            params[0] = OSSL_PARAM_construct_utf8_string(OSSL_DRBG_PARAM_DIGEST, algorithm == RandomAlgorithm::Hmac512 ? digest512 : digest, 0);
            params[1] = OSSL_PARAM_construct_end();
        }
        const unsigned char personalization[] = "Pass-SSL password generator";
        if (EVP_RAND_instantiate(drbg, 256, 0, personalization, sizeof(personalization)-1, params) != 1
            || EVP_RAND_get_strength(drbg) < 256)
            randomError(algorithm, "DRBG instantiate");
    }
};
Random::Random(RandomAlgorithm selected) : algorithm(selected) {}
Random::~Random() { SecureZeroMemory(buffer.data(), buffer.size()); }
unsigned char Random::byte() {
    if (failed) randomError(algorithm, "previous failure");
    if (index == buffer.size()) {
        try {
            if (!state) {
                auto pending = std::make_unique<State>();
                pending->initialize(algorithm);
                state = std::move(pending);
            }
            if(refills && refills % 4096 == 0)reseed();
            ERR_clear_error();
            if(EVP_RAND_get_state(state->drbg)!=EVP_RAND_STATE_READY)randomError(algorithm,"state before generate");
            if (EVP_RAND_generate(state->drbg, buffer.data(), buffer.size(), 256, 0, nullptr, 0) != 1)
                randomError(algorithm, "generate");
            if(EVP_RAND_get_state(state->drbg)!=EVP_RAND_STATE_READY)randomError(algorithm,"state after generate");
            ++refills;
            index = 0;
        } catch (...) {
            failed = true;
            SecureZeroMemory(buffer.data(), buffer.size());
            state.reset();
            throw;
        }
    }
    return buffer[index++];
}
void Random::reseed() {
    if(failed)randomError(algorithm,"previous failure");
    SecureZeroMemory(buffer.data(),buffer.size());index=buffer.size();
    try {
        if(!state){auto pending=std::make_unique<State>();pending->initialize(algorithm);state=std::move(pending);}
        ERR_clear_error();
        if(EVP_RAND_get_state(state->drbg)!=EVP_RAND_STATE_READY)randomError(algorithm,"state before reseed");
        if(EVP_RAND_reseed(state->drbg,0,nullptr,0,nullptr,0)!=1)randomError(algorithm,"reseed");
        if(EVP_RAND_get_state(state->drbg)!=EVP_RAND_STATE_READY)randomError(algorithm,"state after reseed");
    }catch(...){failed=true;SecureZeroMemory(buffer.data(),buffer.size());state.reset();throw;}
}
int Random::below(int n, const std::function<bool()>& cancelled) {
        if (n < 1 || n > 256) throw std::runtime_error("亂數取樣範圍無效。");
        const int limit = 256 - 256 % n;
        // Bound each draw without accepting rejected bytes or changing their weights.
        // Public pools have at most 94 characters: 1024 rejections are extraordinarily unlikely.
        for (int attempt = 0; attempt < 1024; ++attempt) {
            if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
            const int x = byte();
            if (x < limit) return x % n;
        }
        throw std::runtime_error("安全亂數拒絕取樣次數已達上限，已停止產生。");
    }
int Random::below16(int n, const std::function<bool()>& cancelled) {
    if (n < 1 || n > 65536) throw std::runtime_error("亂數取樣範圍無效。");
    const int limit = 65536 - 65536 % n;
    for (int attempt = 0; attempt < 1024; ++attempt) {
        if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
        unsigned int value = byte();
        struct Wiper { unsigned int& value; ~Wiper() { SecureZeroMemory(&value, sizeof(value)); } } wipe{value};
        if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
        value |= static_cast<unsigned int>(byte()) << 8;
        if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
        if (value < static_cast<unsigned int>(limit)) return static_cast<int>(value % n);
    }
    throw std::runtime_error("安全亂數拒絕取樣次數已達上限，已停止產生。");
}
}
