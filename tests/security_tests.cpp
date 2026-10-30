// Test-only instrumentation. Include the ORIGINAL implementation verbatim.
// Windows declarations are parsed before macros; no hooks exist in the UI binary.
#include <windows.h>
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <QString>
#include <QCoreApplication>
#include <QSet>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>

namespace audit {
struct Watchdog : std::runtime_error { Watchdog():std::runtime_error("test watchdog") {} };
using Provider = std::function<int(unsigned char*, size_t)>;
Provider provider;
int calls=0, wipes=0, argErrors=0;
std::vector<SIZE_T> wipeSizes;
int nonzeroCandidateWipes=0;
bool wipeOK=true;
int random(EVP_RAND_CTX* ctx, unsigned char* b, size_t n, unsigned int strength, int prediction, const unsigned char* extra, size_t extraSize) {
    ++calls;
    if(!ctx || n!=256 || strength!=256 || prediction!=0 || extra || extraSize) ++argErrors;
    return provider ? provider(b,n) : ::EVP_RAND_generate(ctx,b,n,strength,prediction,extra,extraSize);
}
PVOID wipe(PVOID p, SIZE_T n) {
    wipeSizes.push_back(n);
    if(n!=256) for(SIZE_T i=0;i<n;++i) if(static_cast<unsigned char*>(p)[i]!=0) {++nonzeroCandidateWipes;break;}
    PVOID result=::SecureZeroMemory(p,n);
    ++wipes;
    for(SIZE_T i=0;i<n;++i) if(static_cast<unsigned char*>(p)[i]!=0) wipeOK=false;
    return result;
}
void reset(Provider p={}) {provider=std::move(p);calls=wipes=argErrors=nonzeroCandidateWipes=0;wipeOK=true;wipeSizes.clear();}
void require(bool ok, const char* msg) {if(!ok) throw std::runtime_error(msg);}
void throws(const std::function<void()>& fn, const char* text=nullptr) {
    bool caught=false;
    try {fn();} catch(const Watchdog&) {throw;} catch(const std::runtime_error& e) {
        caught=true;
        if(text) require(QString::fromUtf8(e.what()).contains(QString::fromUtf8(text)),"wrong error");
    }
    require(caught,"expected exception");
}
Provider stream(std::vector<unsigned char> bytes, unsigned char tail=0) {
    return [bytes=std::move(bytes),tail,pos=size_t(0)](unsigned char* b,size_t n) mutable {
        for(size_t i=0;i<n;++i) b[i]=pos<bytes.size()?bytes[pos++]:tail;
        return 1;
    };
}
}
#define EVP_RAND_generate audit::random
#undef SecureZeroMemory
#define SecureZeroMemory audit::wipe
#include "../src/secure_random.cpp"
#include PASS_CORE_SOURCE
#undef EVP_RAND_generate
#undef SecureZeroMemory

namespace audit {
pass::Rules custom(QString chars, int length=4) {
    pass::Rules r;r.upper=r.lower=r.digits=r.symbols=false;
    r.excludeAmbiguous=false;r.custom=chars;r.length=length;return r;
}
const QString U="ABCDEFGHIJKLMNOPQRSTUVWXYZ", L="abcdefghijklmnopqrstuvwxyz", D="0123456789", S="!@#$%^&*()-_=+[]{};:,.?/";
// Independent oracle: do not call production pool(), accepts() or matches().
QString alphabet(const pass::Rules& r) {
    QString all=(r.upper?U:QString())+(r.lower?L:QString())+(r.digits?D:QString())+(r.symbols?S:QString())+r.custom;
    QString out;
    for(QChar c:all) if(!out.contains(c) && !r.excluded.contains(c) && !(r.excludeAmbiguous && QString("Il1O0o|").contains(c))) out+=c;
    return out;
}
bool oracle(const QString& p, const pass::Rules& r) {
    if(p.size()!=r.length) return false;
    const QString a=alphabet(r);
    for(QChar c:p) if(!a.contains(c)) return false;
    const std::array<QString,4> groups={U,L,D,S};
    const std::array<bool,4> enabled={r.upper,r.lower,r.digits,r.symbols};
    const std::array<int,4> minimum={r.minUpper,r.minLower,r.minDigits,r.minSymbols};
    for(int g=0;g<4;++g) if(enabled[g]) {
        int count=0;for(QChar c:p) if(groups[g].contains(c)) ++count;
        if(count<minimum[g]) return false;
    }
    return true;
}
double chi(const std::vector<int>& counts) {
    double total=0;for(int x:counts) total+=x;
    double expected=total/counts.size(), result=0;
    for(int x:counts) result+=(x-expected)*(x-expected)/expected;
    return result;
}
// Laurent-Massart conservative upper threshold, alpha=1e-6 per comparison.
double threshold(int df) {double x=std::log(1e6);return df+2*std::sqrt(df*x)+2*x;}
int passed=0, failed=0, findings=0;
void test(const char* name,const std::function<void()>& fn) {
    reset();try {fn();++passed;std::cout<<"PASS "<<name<<"\n";}
    catch(const std::exception& e) {++failed;std::cout<<"FAIL "<<name<<": "<<e.what()<<"\n";}
}
}

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    std::cout << std::unitbuf;
    using namespace audit;
    test("OpenSSL call contract and real provider",[]{
        auto p=pass::generate(custom("abc",128));require(oracle(p,custom("abc",128)),"bad password");
        require(calls>=1 && argErrors==0 && wipes==1 && wipeOK,"API contract/cleanup");
    });
    test("initial RNG failures fail closed and zero buffer",[]{
        for(int status:{0,-1,-2}) {
            reset([status](unsigned char* b,size_t n){std::fill(b,b+n,0x41);return status;});
            throws([]{pass::generate(custom("ab"));},"安全亂數服務失敗");
            require(calls==1 && wipes==3 && wipeOK && argErrors==0,"failure cleanup");
        }
    });
    test("later refill failure returns no candidate",[]{
        int refill=0;reset([&](unsigned char* b,size_t n){std::fill(b,b+n,255);return ++refill==1?1:0;});
        throws([]{pass::generate(custom("abc"));},"安全亂數服務失敗");
        require(calls==2 && wipes==3 && wipeOK,"refill fail behavior");
    });
    test("buffer boundary and no stale byte reuse",[]{
        int refill=0;reset([&](unsigned char* b,size_t n){std::fill(b,b+n,++refill==1?0:1);return 1;});
        {pass::Random r;for(int i=0;i<256;++i) require(r.byte()==0,"first buffer");require(calls==1,"early refill");require(r.byte()==1 && calls==2,"boundary refill");}
        require(wipes==1 && wipeOK,"cleanup");
    });
    test("byte rejection thresholds exhaustive n=2..94",[]{
        for(int n=2;n<=94;++n) {
            const int limit=256-256%n;std::vector<int> counts(n);
            for(int b=0;b<256;++b) {
                reset(stream({static_cast<unsigned char>(b),0,17}));
                pass::Random r;int value=r.below(n);
                require(value==(b<limit?b%n:0),"wrong accepted/rejected value");
                require(r.byte()==(b<limit?0:17),"wrong byte consumption");
                if(b<limit) ++counts[value];
            }
            for(int x:counts) require(x==limit/n,"modulo bias");
            if(limit<256) {
                reset(stream({static_cast<unsigned char>(limit-1),static_cast<unsigned char>(limit),255,1,7}));
                pass::Random r;require(r.below(n)==(limit-1)%n,"last accepted");require(r.below(n)==1%n,"first rejected or 255 not discarded");require(r.byte()==7,"wrong consumption");
            }
        }
    });
    test("whole candidate rejection keeps next valid candidate",[]{
        auto r=custom("a");r.digits=true;r.excluded="23456789";r.minDigits=1;
        reset(stream({2,2,2,2,0,2,2,2}));
        require(pass::generate(r)=="0aaa","candidate not rejected intact");
        require(wipes==2 && nonzeroCandidateWipes==1 && wipeSizes[0]==8 && wipeOK,"rejected candidate not securely wiped");
    });
    test("100000 candidate limit and cleanup",[]{
        auto r=custom("a");r.digits=true;r.excluded="23456789";r.minDigits=1;
        reset(stream({},2));int polls=0;
        throws([&]{pass::generate(r,[&]{++polls;return false;});},"重試次數已達上限");
        require(polls==600000 && calls==1563 && wipes==100001 && nonzeroCandidateWipes==100000 && wipeOK,"attempt/refill count or candidate cleanup");
    });
    test("cancellation before RNG",[]{
        throws([]{pass::generate(custom("ab"),[]{return true;});},"已取消");require(calls==0,"RNG called after cancellation");
    });
    test("cancellation between rejected candidates",[]{
        auto r=custom("a");r.digits=true;r.excluded="23456789";r.minDigits=1;
        reset(stream({},2));int polls=0;
        throws([&]{pass::generate(r,[&]{return ++polls==7;});},"已取消");require(polls==7 && calls==1 && nonzeroCandidateWipes==1 && wipeOK,"cancel poll or cleanup");
    });
    test("invalid lengths INT_MIN,-1,0,3,129,INT_MAX rejected before RNG",[]{
        for(int n:{std::numeric_limits<int>::min(),-1,0,3,129,std::numeric_limits<int>::max()}) {
            auto r=custom("ab",n);require(!pass::validate(r).isEmpty(),"accepted length");
            throws([&]{pass::generate(r);});require(pass::entropy(r)==0 && !pass::matches("abab",r),"invalid APIs");
        }require(calls==0,"invalid input used RNG");
    });
    test("valid lengths 4 and 128",[]{
        for(int n:{4,128}) {auto r=custom("ab",n);require(pass::validate(r).isEmpty(),"boundary invalid");require(oracle(pass::generate(r),r),"length boundary");}
    });
    test("minima enabled bounds and sum",[]{
        for(int g=0;g<4;++g) for(int m:{-1,9,std::numeric_limits<int>::max()}) {
            pass::Rules r;int* values[]={&r.minUpper,&r.minLower,&r.minDigits,&r.minSymbols};*values[g]=m;
            require(!pass::validate(r).isEmpty(),"invalid minimum accepted");throws([&]{pass::generate(r);});
        }
        pass::Rules r;r.length=4;r.minUpper=2;r.minLower=2;r.minDigits=1;r.minSymbols=0;
        require(!pass::validate(r).isEmpty(),"sum accepted");require(calls==0,"invalid minima RNG");
    });
    test("minima 0 and 8 and disabled category semantics",[]{
        pass::Rules r;r.length=128;r.minUpper=r.minLower=r.minDigits=r.minSymbols=8;
        require(pass::validate(r).isEmpty(),"maximum minima invalid");require(oracle(pass::generate(r),r),"maximum minima unmet");
        r.minUpper=r.minLower=r.minDigits=r.minSymbols=0;
        require(pass::validate(r).isEmpty() && oracle(pass::generate(r),r),"zero minima invalid");
        r=custom("ab");r.minUpper=-1;r.minLower=9;r.minDigits=999;r.minSymbols=-999;
        require(pass::validate(r).isEmpty(),"disabled minima should be ignored");require(oracle(pass::generate(r),r),"disabled semantics");
    });
    test("empty singleton and fully excluded groups",[]{
        for(QString chars:{QString(),QString("a"),QString("aaaa")}) require(!pass::validate(custom(chars)).isEmpty(),"small pool accepted");
        pass::Rules r;r.excluded=U;require(!pass::validate(r).isEmpty(),"required empty group accepted");
        r.minUpper=0;require(pass::validate(r).isEmpty(),"empty optional group rejected");require(oracle(pass::generate(r),r),"optional group");
        r=custom("ab");r.excluded="ab";require(!pass::validate(r).isEmpty(),"fully excluded pool");
    });
    test("ASCII boundaries controls Unicode and surrogate rejected",[]{
        for(ushort c:{ushort(0),ushort(9),ushort(10),ushort(32),ushort(127),ushort(128),ushort(0x4e2d),ushort(0xd800)}) {
            auto r=custom("ab");r.custom+=QChar(c);require(!pass::validate(r).isEmpty(),"bad custom accepted");
            r=custom("ab");r.excluded=QChar(c);require(!pass::validate(r).isEmpty(),"bad exclude accepted");
        }
        auto r=custom("!~");require(pass::validate(r).isEmpty() && oracle(pass::generate(r),r),"ASCII endpoints");
    });
    test("duplicates and custom overlap cannot add weight",[]{
        auto r=custom("aabbcc");require(std::abs(pass::entropy(r)-4*std::log2(3.0))<1e-10,"duplicate entropy");
        reset(stream({0,1,2,0}));require(pass::generate(r)=="abca","dedup order");
        pass::Rules a;a.excludeAmbiguous=false;auto b=a;b.custom=U+L+D+S+U;
        require(std::abs(pass::entropy(a)-pass::entropy(b))<1e-10,"overlap changes entropy");
    });
    test("exclusions win over custom and ambiguous switch",[]{
        auto r=custom("Il1O0o|ab",128);r.excludeAmbiguous=true;
        require(alphabet(r)=="ab" && oracle(pass::generate(r),r),"ambiguous custom");
        r=custom("abc",128);r.excluded="b";require(oracle(pass::generate(r),r),"excluded custom");
        r=custom("Il1O0o|",4);require(pass::validate(r).isEmpty(),"ambiguous off");
    });
    test("matches checked by exhaustive independent oracle; entropy 80 combinations",[]{
        auto r=custom("a");r.digits=true;r.excluded="23456789";r.minDigits=1;
        int accepted=0;
        for(int v=0;v<81;++v) {int x=v;QString p;for(int i=0;i<4;++i){p+="01a"[x%3];x/=3;}
            bool good=oracle(p,r);require(pass::matches(p,r)==good,"oracle disagreement");accepted+=good;
        }
        require(accepted==80 && std::abs(pass::entropy(r)-std::log2(80.0))<1e-10,"entropy count");
        require(!pass::matches("0aa",r) && !pass::matches("0aaab",r) && !pass::matches("0aab",r),"matches boundary");
    });
    test("rare-rule threshold either side of 0.001",[]{
        auto r=custom("abcd",4);r.digits=true;r.excluded="123456789";r.minDigits=4;
        require(pass::validate(r).isEmpty(),"p=1/625 should pass");
        r.custom="abcde";require(!pass::validate(r).isEmpty(),"p=1/1296 should fail");throws([&]{pass::generate(r);});
        require(calls==0,"rare invalid used RNG");
    });
    test("entropy exact four-category 24 combinations and full ASCII",[]{
        pass::Rules r;r.length=4;r.excludeAmbiguous=false;
        r.excluded=U.mid(1)+L.mid(1)+D.mid(1)+S.mid(1);
        int valid=0;
        for(int v=0;v<256;++v) {int x=v;QString p;for(int i=0;i<4;++i){p+="Aa0!"[x%4];x/=4;}
            bool good=oracle(p,r);require(pass::matches(p,r)==good,"four-category oracle");valid+=good;}
        require(valid==24 && std::abs(pass::entropy(r)-std::log2(24.0))<1e-10,"24 combinations entropy");
        QString all;for(int c=33;c<=126;++c) all+=QChar(c);
        r=custom(all,128);require(std::abs(pass::entropy(r)-128*std::log2(94.0))<1e-10,"full alphabet entropy");
    });
    test("all 16 category masks x ambiguous on/off, independent rules",[]{
        for(int mask=0;mask<16;++mask) for(bool ambiguous:{false,true}) {
            pass::Rules r;r.length=32;r.upper=mask&1;r.lower=mask&2;r.digits=mask&4;r.symbols=mask&8;
            r.excludeAmbiguous=ambiguous;r.custom="~<>aA0!aa";r.excluded="Xx5!";
            require(pass::validate(r).isEmpty(),"matrix invalid");
            for(int i=0;i<20;++i) {auto p=pass::generate(r);require(oracle(p,r),"rule matrix oracle");}
        }
    });
    test("unconstrained live distributions per character and position n=2,3,10,26,94",[]{
        QString all;for(int c=33;c<=126;++c) all+=QChar(c);
        for(QString chars:{QString("ab"),QString("abc"),D,L,all}) {
            auto r=custom(chars,4);int n=int(chars.size());std::vector<int> total(n);std::array<std::vector<int>,4> pos;
            for(auto& row:pos) row.resize(n);
            for(int i=0;i<50000;++i) {QString p=pass::generate(r);require(oracle(p,r),"bad distribution password");
                for(int j=0;j<4;++j){int k=int(chars.indexOf(p[j]));++total[k];++pos[j][k];}}
            double maxPosition=0;for(auto& row:pos) maxPosition=std::max(maxPosition,chi(row));
            std::cout<<"STAT live n="<<n<<" samples=50000 chars=200000 chi2="<<chi(total)<<" max_position_chi2="<<maxPosition<<" limit="<<threshold(n-1)<<"\n";
            require(chi(total)<threshold(n-1),"character chi2");for(auto& row:pos) require(chi(row)<threshold(n-1),"position chi2");
        }
    });
    test("conditioned live distribution over all 80 valid passwords",[]{
        auto r=custom("a");r.digits=true;r.excluded="23456789";r.minDigits=1;
        std::map<QString,int> counts;
        for(int v=0;v<81;++v) {int x=v;QString p;for(int i=0;i<4;++i){p+="01a"[x%3];x/=3;}if(oracle(p,r)) counts[p]=0;}
        for(int i=0;i<80000;++i) {auto p=pass::generate(r);require(oracle(p,r) && counts.count(p),"invalid conditional sample");++counts[p];}
        std::vector<int> bins;for(auto& item:counts) bins.push_back(item.second);
        std::cout<<"STAT conditional samples=80000 valid=80 chi2="<<chi(bins)<<" limit="<<threshold(79)<<"\n";
        require(chi(bins)<threshold(79),"conditional chi2");
    });
    test("default real rules 2000 passwords independent oracle",[]{
        pass::Rules r;for(int i=0;i<2000;++i) require(oracle(pass::generate(r),r),"default rule failure");
    });
    test("strength common examples and score boundaries",[]{
        require(pass::analyze("password123").score<35,"common strength");require(pass::analyze("aaaaaaaaaaaaaaaa").score<35,"repeated strength");
        require(pass::analyze("").score==0,"empty strength");for(QString p:{QString("a"),QString(128,'a'),QString::fromUtf8("中文測試")}) {
            auto a=pass::analyze(p);require(a.score>=0 && a.score<=100,"score range");}
    });
    // Findings are deliberate safety requirements that current code violates.
    // Return failure so automation cannot mistake known defects for a clean audit.
    test("FINDING F1 cancellation must interrupt byte rejection",[]{
        reset([](unsigned char* b,size_t n){if(calls>4) throw Watchdog();std::fill(b,b+n,255);return 1;});
        int polls=0;bool watchdog=false;
        try {pass::generate(custom("abc"),[&]{return ++polls>3;});}catch(const Watchdog&) {watchdog=true;}
        catch(const std::runtime_error&) {}
        std::cout<<"PROBE F1 refills="<<calls<<" cancel_polls="<<polls<<" watchdog="<<watchdog<<"\n";
        if(watchdog && polls==1) ++findings;
        require(!watchdog && polls==4 && calls==1 && wipeOK,"cancel never re-polled during byte rejection; test watchdog stopped loop");
    });
    test("OpenSSL diagnostic retains error queue reason",[]{
        reset([](unsigned char*,size_t){ERR_raise(ERR_LIB_RAND,ERR_R_INTERNAL_ERROR);return 0;});
        throws([]{pass::generate(custom("ab"));},"internal error");
    });
    test("F1 rejection budget ends after exactly 1024 rejected bytes",[]{
        reset([](unsigned char* b,size_t n){if(calls>4) throw Watchdog();std::fill(b,b+n,255);return 1;});
        throws([]{pass::generate(custom("abc"));},"拒絕取樣次數已達上限");
        require(calls==4 && wipes==2 && wipeOK,"rejection budget or cleanup");
    });
    test("budget last byte accepted and next draw has fresh budget",[]{
        std::vector<unsigned char> bytes(1023,255);bytes.push_back(1);
        bytes.insert(bytes.end(),1023,255);bytes.push_back(2);
        reset(stream(std::move(bytes)));
        {pass::Random rng;require(rng.below(3)==1 && rng.below(3)==2 && calls==8,"budget boundary or reset");}
        require(wipes==1 && wipeOK,"buffer cleanup");
    });
    test("private sampler rejects invalid bounds without RNG",[]{
        pass::Random rng;for(int n:{-1,0,257}) throws([&]{rng.below(n);},"範圍無效");
        require(calls==0,"invalid sampler called RNG");
    });
    test("only OpenSSL return value 1 is success",[]{
        for(int status:{0,-1,-2,2}) {
            reset([status](unsigned char*,size_t){return status;});
            throws([]{pass::generate(custom("ab"));},"OpenSSL 安全亂數服務失敗");
            require(wipes==3 && wipeOK,"diagnostic cleanup");
        }
        reset([](unsigned char* b,size_t n){std::fill(b,b+n,0);return 1;});
        require(pass::generate(custom("ab"))=="aaaa","OpenSSL success semantics");
    });
    test("partial candidate wiped on later RNG failure",[]{
        reset([](unsigned char* b,size_t n){std::fill(b,b+n,255);if(calls==1){b[0]=0;b[1]=1;return 1;}return 0;});
        throws([]{pass::generate(custom("abc"));},"OpenSSL 安全亂數服務失敗");
        require(calls==2 && wipes==3 && nonzeroCandidateWipes==1 && wipeSizes[0]==256 && wipeOK,"partial candidate exception cleanup");
    });
    test("partial candidate wiped on cancellation and throwing callback",[]{
        for(bool throwing:{false,true}) {
            reset(stream({0,1,2,0}));int polls=0;
            throws([&]{pass::generate(custom("abc"),[&]{if(++polls==4){if(throwing)throw std::runtime_error("callback failed");return true;}return false;});},throwing?"callback failed":"已取消");
            require(calls==1 && wipes==2 && nonzeroCandidateWipes==1 && wipeOK,"partial candidate cancellation cleanup");
        }
    });
    test("completed candidate wiped if cancellation arrives before return",[]{
        reset(stream({0,1,2,0}));int polls=0;
        throws([&]{pass::generate(custom("abc"),[&]{return ++polls==6;});},"已取消");
        require(wipes==2 && nonzeroCandidateWipes==1 && wipeOK,"completed candidate cleanup");
    });
    test("partial candidate wiped on rejection budget failure",[]{
        reset([](unsigned char* b,size_t n){std::fill(b,b+n,255);if(calls==1)b[0]=1;return 1;});
        throws([]{pass::generate(custom("abc"));},"拒絕取樣次數已達上限");
        require(calls==5 && wipes==2 && nonzeroCandidateWipes==1 && wipeOK,"budget failure partial candidate cleanup");
    });
    test("successful candidate ownership survives RNG cleanup",[]{
        reset(stream({0,1,2,0}));const QString p=pass::generate(custom("abc"));
        require(p=="abca" && wipes==1 && wipeSizes[0]==256 && wipeOK,"success result was wiped");
    });
    test("16-bit exhaustive boundaries and eight equal preimages for 7776",[]{
        std::vector<int> counts(7776,0); int rejected=0;
        for (unsigned int x=0;x<65536;++x) {
            reset(stream({static_cast<unsigned char>(x),static_cast<unsigned char>(x>>8),1,0,7}));
            pass::Random rng; const int result=rng.below16(7776);
            if(x<62208) { require(result==static_cast<int>(x%7776),"accepted index"); ++counts[result]; require(rng.byte()==1,"accepted byte count"); }
            else { ++rejected; require(result==1 && rng.byte()==7,"rejected pair accepted or wrong consumption"); }
        }
        for(int count:counts)require(count==8,"biased index"); require(rejected==3328,"reject interval");
    });
    test("16-bit range boundaries without RNG on invalid input",[]{
        pass::Random rng;
        for(int n:{-1,0,65537,INT_MAX})throws([&]{rng.below16(n);},"範圍無效");
        require(calls==0,"invalid bound used RNG");
        reset(stream({255,255,255,255}));require(rng.below16(65536)==65535,"full range");require(rng.below16(1)==0,"singleton");
    });
    test("16-bit rejection budget 1024 pairs",[]{
        reset(stream({},255)); {pass::Random rng;throws([&]{rng.below16(7776);},"拒絕取樣次數已達上限");}
        require(calls==8 && wipes==1025 && wipeOK,"16-bit attempt/refill/cleanup count");
    });
    test("16-bit acceptance on last attempt and fresh next budget",[]{
        std::vector<unsigned char> bytes(2046,255);bytes.insert(bytes.end(),{1,0,2,0});reset(stream(bytes,255));
        pass::Random rng;require(rng.below16(7776)==1 && rng.below16(7776)==2,"attempt budget boundary");
    });
    test("16-bit cancellation before, between bytes and on rejection",[]{
        for(int stop:{1,2,3,4}) {
            reset(stream({},255));int polls=0; {pass::Random rng;throws([&]{rng.below16(7776,[&]{return ++polls==stop;});},"已取消");}
            require(polls==stop && calls==(stop==1?0:1) && wipeOK,"16-bit cancellation/cleanup");
        }
    });
    test("16-bit OpenSSL failure preserves status and cleanup",[]{
        reset([](unsigned char* b,size_t n){std::fill(b,b+n,91);return 0;});
        {pass::Random rng;throws([&]{rng.below16(7776);},"OpenSSL 安全亂數服務失敗");}
        require(calls==1 && argErrors==0 && wipeOK,"16-bit API failure cleanup");
    });
    std::cout<<"SUMMARY passed="<<passed<<" failed="<<failed<<" reproduced_findings="<<findings<<"\n";
    return failed?1:0;
}

