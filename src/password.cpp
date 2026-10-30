#include "password.h"
#include "strength_estimator.h"
#include "secure_random.h"
#include "secret_text.h"
#include <windows.h>
#include <array>
#include <cmath>
#include <stdexcept>
#include <QPoint>
namespace pass {
namespace {
const QString upper = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
const QString lower = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
const QString digits = QStringLiteral("0123456789");
const QString symbols = QStringLiteral("!@#$%^&*()-_=+[]{};:,.?/");
QString unique(QString text) { QString result; for (QChar c : text) if (!result.contains(c)) result += c; return result; }
struct Pool { QString alphabet; QVector<QString> groups; QVector<int> minima; };
Pool pool(const Rules& r) {
    Pool p;
    auto add = [&](bool enabled, const QString& chars, int minimum) {
        if (!enabled) return;
        QString filtered;
        for (QChar c : chars) if (!r.excluded.contains(c) && !(r.excludeAmbiguous && QStringLiteral("Il1O0o|").contains(c))) filtered += c;
        p.groups += filtered; p.minima += minimum; p.alphabet += filtered;
    };
    add(r.upper, upper, r.minUpper); add(r.lower, lower, r.minLower);
    add(r.digits, digits, r.minDigits); add(r.symbols, symbols, r.minSymbols);
    for (QChar c : r.custom) if (!r.excluded.contains(c) && !(r.excludeAmbiguous && QStringLiteral("Il1O0o|").contains(c))) p.alphabet += c;
    p.alphabet = unique(p.alphabet);
    return p;
}
bool accepts(const QString& text, const Pool& p) {
    for (int i = 0; i < p.groups.size(); ++i) {
        int count = 0; for (QChar c : text) if (p.groups[i].contains(c)) ++count;
        if (count < p.minima[i]) return false;
    }
    return true;
}
double probability(const Pool& p, int length) {
    // Dynamic programming counts probability of meeting capped category minima.
    // Built-in categories are disjoint; custom characters belong to their built-in category if applicable.
    QVector<int> strides; int states = 1;
    for (int m : p.minima) { strides += states; states *= m + 1; }
    QVector<double> current(states, 0.0), next(states, 0.0); current[0] = 1.0;
    QVector<double> chances; int covered = 0;
    for (const auto& group : p.groups) { chances += double(group.size()) / p.alphabet.size(); covered += group.size(); }
    const double other = double(p.alphabet.size() - covered) / p.alphabet.size();
    for (int step = 0; step < length; ++step) {
        next.fill(0.0);
        for (int s = 0; s < states; ++s) {
            if (!current[s]) continue;
            next[s] += current[s] * other;
            for (int g = 0; g < p.groups.size(); ++g) {
                int count = (s / strides[g]) % (p.minima[g] + 1);
                int target = s + (count < p.minima[g] ? strides[g] : 0);
                next[target] += current[s] * chances[g];
            }
        }
        current.swap(next);
    }
    return current[states - 1];
}
}
QString validate(const Rules& r) {
    if (r.length < 4 || r.length > 128) return QStringLiteral("長度必須介於 4 到 128。");
    for (QChar c : r.custom + r.excluded) if (c.unicode() < 33 || c.unicode() > 126) return QStringLiteral("自訂及排除字元限可見 ASCII 字元，不含空白。");
    Pool p = pool(r); int total = 0;
    for (int i = 0; i < p.minima.size(); ++i) {
        if (p.minima[i] < 0 || p.minima[i] > 8) return QStringLiteral("每類最低數量必須介於 0 到 8。");
        if (p.groups[i].isEmpty() && p.minima[i] > 0) return QStringLiteral("某類字元已全部被排除，無法滿足最低數量。");
        total += p.minima[i];
    }
    if (total > r.length) return QStringLiteral("各類最低數量總和不可超過密碼長度。");
    if (p.alphabet.size() < 2) return QStringLiteral("請保留至少兩個不同的可用字元。");
    if (probability(p, r.length) < 0.001) return QStringLiteral("目前規則的有效組合比例太低；請增加長度、降低最低數量或調整字元池。");
    return {};
}
bool matches(const QString& text, const Rules& r) {
    if (!validate(r).isEmpty() || text.size() != r.length) return false;
    Pool p = pool(r); for (QChar c : text) if (!p.alphabet.contains(c)) return false;
    return accepts(text, p);
}
QString generate(const Rules& r, const std::function<bool()>& cancelled) {
    QString error = validate(r); if (!error.isEmpty()) throw std::runtime_error(error.toUtf8().constData());
    Pool p = pool(r); Random rng(r.algorithm);
    for (int attempt = 0; attempt < 100000; ++attempt) {
        if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
        // Allocate once and avoid reallocations or shared QString copies of partial secrets.
        QString candidate(r.length, QChar(0));
        CandidateWiper wipe(candidate);
        for (int i = 0; i < r.length; ++i) candidate[i] = p.alphabet[rng.below(static_cast<int>(p.alphabet.size()), cancelled)];
        if (cancelled && cancelled()) throw std::runtime_error("已取消產生。");
        // Reject whole candidates: all passwords satisfying the rules remain equally likely.
        if (accepts(candidate, p)) { wipe.release(); return candidate; }
    }
    throw std::runtime_error("規則重試次數已達上限，請調整規則。");
}
double entropy(const Rules& r) {
    if (!validate(r).isEmpty()) return 0;
    Pool p = pool(r);
    return r.length * std::log2(double(p.alphabet.size())) + std::log2(probability(p, r.length));
}
namespace {
// US QWERTY physical keys: Shift and letter case do not add a new path.
QChar physicalKey(QChar c) {
    const QString shifted = QStringLiteral("~!@#$%^&*()_+{}|:\"<>?");
    const QString plain = QStringLiteral("`1234567890-=[]\\;',./");
    const auto index = shifted.indexOf(c);
    return index < 0 ? c.toLower() : plain[index];
}
bool adjacent(QChar a, QChar b, bool keypad) {
    if (a == b) return false;
    const QStringList rows = keypad
        ? QStringList{QStringLiteral("789"),QStringLiteral("456"),QStringLiteral("123"),QStringLiteral(" 0 ")}
        : QStringList{QStringLiteral("`1234567890-="),QStringLiteral("qwertyuiop[]\\"),QStringLiteral("asdfghjkl;'"),QStringLiteral("zxcvbnm,./")};
    const int offsets[] = {0,1,2,4};
    auto position = [&](QChar c) {
        for (int row=0; row<rows.size(); ++row) {
            const auto col=rows[row].indexOf(c);
            if (col>=0 && c!=' ') return QPoint(int(col)*4+(keypad?0:offsets[row]),row);
        }
        return QPoint(-100,-100);
    };
    const auto x=position(a), y=position(b);
    return x.x()!=-100 && y.x()!=-100 && qAbs(x.x()-y.x())<=4 && qAbs(x.y()-y.y())<=1;
}
QChar dictionaryChar(QChar c) {
    c=c.toLower();
    const QString from=QStringLiteral("@4301!$57"), to=QStringLiteral("aaeoiisst");
    const auto index=from.indexOf(c);
    return index<0?c:to[index];
}
}
Analysis analyze(const QString& text) {
    if (text.isEmpty()) return {0, QStringLiteral("尚未輸入"), {}};
    // Match the UI limit; never truncate and silently rate a prefix.
    if (text.size()>128) return {0,QStringLiteral("尚未評估"),{QStringLiteral("強度檢查最多支援 128 個字元。")}};
    if (text.contains(QChar(0))) return {0,QStringLiteral("尚未評估"),{}};
    bool u=false,l=false,d=false,s=false,other=false;
    for (QChar c : text) { u |= upper.contains(c); l |= lower.contains(c); d |= digits.contains(c); s |= symbols.contains(c); other |= !upper.contains(c) && !lower.contains(c) && !digits.contains(c) && !symbols.contains(c); }
    const double literalCost=std::log2(double(qMax(2,int((u?26:0)+(l?26:0)+(d?10:0)+(s?symbols.size():0)+(other?20:0)))));
    // Allocate once so normalization never frees an unwiped partial buffer.
    QString keys(text.size(),QChar(0)), folded(text.size(),QChar(0));
    CandidateWiper wipeKeys(keys), wipeFolded(folded);
    for (int i=0;i<text.size();++i) {keys[i]=physicalKey(text[i]);folded[i]=dictionaryChar(text[i]);}
    const QStringList common={QStringLiteral("password"),QStringLiteral("letmein"),QStringLiteral("admin"),QStringLiteral("welcome"),QStringLiteral("qwerty"),QStringLiteral("dragon"),QStringLiteral("monkey"),QStringLiteral("football"),QStringLiteral("iloveyou"),QStringLiteral("login"),QStringLiteral("secret"),QStringLiteral("changeme"),QStringLiteral("abc123"),QStringLiteral("密碼"),QStringLiteral("passwords")};
    const int n=int(text.size());
    QVector<double> costs(n+1,1e9); QVector<int> coverage(n+1,0), flags(n+1,0);
    costs[0]=0;
    // Minimum-cost non-overlapping segmentation. These are heuristic score units,
    // not entropy or a calibrated estimate of guesses. Unmatched text keeps credit.
    for(int start=0;start<n;++start) {
        auto relax=[&](int end,double cost,int kind) {
            const double candidate=costs[start]+cost;
            if(candidate<costs[end]) {costs[end]=candidate;coverage[end]=coverage[start]+(kind?end-start:0);flags[end]=flags[start]|kind;}
        };
        relax(start+1,literalCost,0);
        bool keyboard=true,keypad=true,sequence=true;
        int delta=0;
        for(int end=start+1;end<=n;++end) {
            const int length=end-start;
            if(length>=2) {
                const auto a=keys[end-2],b=keys[end-1];
                keyboard &= adjacent(a,b,false); keypad &= adjacent(a,b,true);
                const int step=b.unicode()-a.unicode();
                const bool sameClass=(a>='a' && a<='z' && b>='a' && b<='z') || (a>='0' && a<='9' && b>='0' && b<='9');
                if(length==2)delta=step;
                sequence &= sameClass && step==delta && qAbs(step)>=1 && qAbs(step)<=2;
            }
            if(length>=4 && (keyboard || keypad)) relax(end,10+std::log2(double(length)),1);
            if(length>=3 && sequence) relax(end,6+std::log2(double(length)),2);
            // Exact motif repeats only: case/Shift changes are not erased here.
            for(int period=1;period<=length/2;++period) {
                if(length%period || (period==1?length<3:length<6))continue;
                bool repeated=true;
                for(int k=start+period;k<end && repeated;++k)repeated=text[k]==text[start+(k-start)%period];
                if(repeated)relax(end,period*literalCost+std::log2(double(length/period)),2);
            }
            for(const auto& word:common) if(word.size()==length) {
                bool match=true;
                for(int k=0;k<length && match;++k)match=folded[start+k]==dictionaryChar(word[k]);
                if(match)relax(end,10+std::log2(double(length)),4);
            }
        }
    }
    int score=qBound(0,int(costs[n]),100);
    // A string largely assembled from recognized patterns cannot earn a strong
    // label merely by concatenating more such pieces. Isolated matches do not cap it.
    if(coverage[n]>=8 && coverage[n]*5>=n*3)score=qMin(score,59);
    if(coverage[n]>=8 && coverage[n]*5>=n*4)score=qMin(score,34);
    // Already-weak inputs need no expensive dictionary graph. This also keeps
    // repeated 128-character inputs responsive at the established UI limit.
    const int dictionaryLimit=score<35?score:dictionaryScoreLimit(text);
    // Preserve the established values of already-weak keyboard/repeat cases.
    if((score>=35 || dictionaryLimit==0) && dictionaryLimit<score){score=dictionaryLimit;flags[n]|=4;}
    QStringList feedback;
    if(flags[n]&1)feedback+=QStringLiteral("包含鍵盤相鄰路徑（含大小寫或 Shift 變化）。");
    if(flags[n]&2)feedback+=QStringLiteral("包含重複或連續字元。");
    if(flags[n]&4)feedback+=QStringLiteral("包含常見弱密碼詞或其替換形式。");
    if(text.size()<16)feedback+=QStringLiteral("建議增加長度至至少 16 個字元。");
    const QString label=score<35?QStringLiteral("弱"):score<60?QStringLiteral("普通"):score<80?QStringLiteral("強"):QStringLiteral("很強");
    feedback+=QStringLiteral("離線啟發式分數，非精確熵或安全保證；未查詢外洩資料庫，無法判定亂數來源。");
    return {score,label,feedback};
}
}
