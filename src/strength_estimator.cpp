#include "strength_estimator.h"
#include <windows.h>
#include <cstdlib>
#include <cmath>
#include <new>
#include <vector>
#include <array>
#include <cstring>
#include <zxcvbn.h>

namespace {
#ifdef PASS_ESTIMATOR_TESTING
thread_local int faultAfter=-1, allocationCalls=0, liveAllocations=0, wipedAllocations=0;
thread_local bool zeroVerified=true;
#endif
struct Allocation { void* data; size_t bytes; };
void wipeAndFree(const Allocation& a) {
    SecureZeroMemory(a.data,a.bytes);
#ifdef PASS_ESTIMATOR_TESTING
    for(size_t i=0;i<a.bytes;++i)if(static_cast<unsigned char*>(a.data)[i])zeroVerified=false;
    --liveAllocations;++wipedAllocations;
#endif
    std::free(a.data);
}
// Upstream allocates a reversed password and match graph. Track every allocation
// so an allocation failure also unwinds and wipes the entire partial graph.
struct Arena {
    std::vector<Allocation> allocations;
    ~Arena() { for(const auto& a:allocations)wipeAndFree(a); }
};
thread_local Arena* arena=nullptr;
template<class T> T* allocate(size_t count) {
#ifdef PASS_ESTIMATOR_TESTING
    if(allocationCalls++==faultAfter)throw std::bad_alloc();
#endif
    if(count>SIZE_MAX/sizeof(T))throw std::bad_alloc();
    const size_t bytes=count*sizeof(T);
    void* p=std::malloc(bytes);
    if(!p)throw std::bad_alloc();
    try {arena->allocations.push_back({p,bytes});}
    catch(...) {SecureZeroMemory(p,bytes);std::free(p);throw;}
#ifdef PASS_ESTIMATOR_TESTING
    ++liveAllocations;
#endif
    return static_cast<T*>(p);
}
void release(void* p) {
    for(auto it=arena->allocations.begin();it!=arena->allocations.end();++it)if(it->data==p){
        wipeAndFree(*it);arena->allocations.erase(it);return;
    }
}
struct CallScope {
    Arena owned;
    Arena* previous=arena;
    CallScope(){arena=&owned;}
    ~CallScope(){arena=previous;}
};
}
#undef MallocFn
#undef FreeFn
#define MallocFn(T,N) allocate<T>(N)
#define FreeFn(P) release(P)
// UTF-8 for a 128 UTF-16-unit input fits this bound; no prefix-only estimates.
#define ZXCVBN_DETAIL_LEN 512
#include "../third_party/zxcvbn-c/zxcvbn.c"

namespace pass {
int dictionaryScoreLimit(const QString& text) noexcept {
    if(text.isEmpty() || text.size()>128 || text.contains(QChar(0)))return 0;
    try {
        CallScope scope;
        auto utf8=text.toUtf8();
        struct Wipe { QByteArray& b; ~Wipe(){SecureZeroMemory(b.data(),static_cast<SIZE_T>(b.size()));} } wipe{utf8};
        // Public tutorial examples are already known strings, irrespective of
        // word entropy. Fold case, common leet substitutions and separators.
        std::array<char,129> compact{};
        struct WipeCompact { std::array<char,129>& b; ~WipeCompact(){SecureZeroMemory(b.data(),b.size());} } wipeCompact{compact};
        size_t length=0;
        const QString leet=QStringLiteral("013457@$!");
        const QString plain=QStringLiteral("oieastasi");
        for(QChar c:text){
            c=c.toLower();const auto k=leet.indexOf(c);if(k>=0)c=plain[k];
            if(c>='a' && c<='z')compact[length++]=static_cast<char>(c.unicode());
            else if(!c.isSpace() && !c.isPunct() && !c.isSymbol() && !c.isDigit())compact[length++]='?';
        }
        const char* publicExamples[]={"correcthorsebatterystaple","thisisaverysecurepassword","thequickbrownfoxjumpsoverthelazydog",nullptr};
        // A known phrase within a longer input receives credit only for its
        // unmatched suffix/prefix; do not blacklist unrelated random contents.
        const double bits=ZxcvbnMatch(utf8.constData(),publicExamples,nullptr);
        int score=std::isfinite(bits)?qBound(0,static_cast<int>(bits),100):0;
        for(const char** phrase=publicExamples;*phrase;++phrase) {
            const auto size=std::strlen(*phrase);
            if(length>=size && length<=size+8){
                for(size_t start=0;start+size<=length;++start)
                    if(std::memcmp(compact.data()+start,*phrase,size)==0)score=qMin(score,length==size?20:34);
            }
        }
        return score;
    } catch(...) {return 0;} // Never turn estimator failure into a strong score.
}
}
