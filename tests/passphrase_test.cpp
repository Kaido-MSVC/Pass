#include "passphrase.h"
#include "wordlist.h"
#include "password.h"
#include "generation.h"
#include <QtTest>
#include <QCryptographicHash>
#include <limits>
#include <cmath>
class PassphraseTests : public QObject {
    Q_OBJECT
private slots:
    void sha512WordRules(){
        pass::PassphraseRules r;r.algorithm=pass::RandomAlgorithm::Hmac512;
        for(int count:{4,6,12})for(QChar sep:{QChar('-'),QChar('_'),QChar(' ')}){
            r.words=count;r.separator=sep;
            for(int i=0;i<20;++i){auto text=pass::generate(r);QVERIFY(pass::matches(text,r));wipeText(text);}
        }
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(r,[]{return true;}));
    }
    void officialIntegrity() {
        QFile file(":/wordlists/eff_large_wordlist.txt");
        QVERIFY(pass::wordlist().available()); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto raw = file.readAll(); QCOMPARE(QCryptographicHash::hash(raw,QCryptographicHash::Sha256).toHex(),QByteArray(pass::WordlistSha256));
        QCOMPARE(pass::wordlist().words.size(),7776); QCOMPARE(pass::wordlist().members.size(),7776);
        QCOMPARE(pass::wordlist().words.first(),QStringLiteral("abacus")); QCOMPARE(pass::wordlist().words.last(),QStringLiteral("zoom"));
        QCOMPARE(pass::wordlist().maxLength,9);
        const auto rows = raw.trimmed().split('\n'); QSet<QByteArray> codes;
        for (int i=0;i<7776;++i) { const auto fields=rows[i].trimmed().split('\t'); QCOMPARE(fields.size(),2); codes.insert(fields[0]); QCOMPARE(QString::fromLatin1(fields[1]),pass::wordlist().words[i]); }
        QCOMPARE(codes.size(),7776);
    }
    void corruptedDataFailsClosed() {
        QFile file(":/wordlists/eff_large_wordlist.txt"); QVERIFY(file.open(QIODevice::ReadOnly)); auto raw=file.readAll();
        QVERIFY(!pass::validateWordlist({}).available());
        auto modified=raw; modified[8]='X'; auto invalid=pass::validateWordlist(modified);
        QVERIFY(!invalid.available()); QVERIFY(invalid.words.isEmpty()); QVERIFY(invalid.members.isEmpty());
        QVERIFY(!pass::validateWordlist(raw.left(raw.size()-20)).available());
        auto rows=raw.split('\n'); rows.swapItemsAt(0,1); QVERIFY(!pass::validateWordlist(rows.join('\n')).available());
        rows=raw.split('\n'); rows[1]=rows[0]; QVERIFY(!pass::validateWordlist(rows.join('\n')).available());
        QVERIFY(pass::matches(pass::generate(pass::Rules{}),pass::Rules{}));
    }
    void wordCountsAndSeparators() {
        for (int count : {4,6,12}) for (QChar sep : {QChar('-'),QChar('_'),QChar(' ')}) {
            pass::PassphraseRules r; r.words=count; r.separator=sep;
            for (int i=0;i<40;++i) { QString phrase=pass::generate(r); QVERIFY(pass::matches(phrase,r)); QVERIFY(phrase.size()<=count*9+count-1); wipeText(phrase); }
            QCOMPARE(pass::maxLength(r),count*9+count-1);
            QVERIFY(qAbs(pass::entropy(r)-count*std::log2(7776.0))<1e-10);
        }
    }
    void hyphenWordsAndRepeatedWords() {
        pass::PassphraseRules r; r.words=4;
        QVERIFY(pass::matches(QStringLiteral("drop-down-felt-tip-t-shirt-yo-yo"),r));
        QVERIFY(pass::matches(QStringLiteral("abacus-abacus-abacus-abacus"),r));
        QVERIFY(!pass::matches(QStringLiteral("abacus-abacus-abacus"),r));
        QVERIFY(!pass::matches(QStringLiteral("abacus-abacus-abacus-nonwordxyz"),r));
        QVERIFY(!pass::matches(QStringLiteral("Abacus-abacus-abacus-abacus"),r));
        r.separator=' '; QVERIFY(pass::matches(QStringLiteral("drop-down felt-tip t-shirt yo-yo"),r));
    }
    void invalidRules() {
        for (int count : {-1,0,3,13,INT_MAX}) { pass::PassphraseRules r; r.words=count; QVERIFY(!pass::validate(r).isEmpty()); QCOMPARE(pass::entropy(r),0.0); QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(r)); }
        for (QChar sep : {QChar('/'),QChar(0),QChar(0x4e00)}) { pass::PassphraseRules r; r.separator=sep; QVERIFY(!pass::validate(r).isEmpty()); QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(r)); }
    }
    void cancellation() { QVERIFY_THROWS_EXCEPTION(std::runtime_error,pass::generate(pass::PassphraseRules{},[]{return true;})); }
    void batchCapacityBounds() {
        QCOMPARE(batchCapacity(119,100),size_t(11999)); QCOMPARE(batchCapacity(128,100),size_t(12899)); QCOMPARE(batchCapacity(119,1),size_t(119));
        for (int count : {-1,0,101,INT_MAX}) QVERIFY_THROWS_EXCEPTION(std::invalid_argument,batchCapacity(119,count));
        QVERIFY_THROWS_EXCEPTION(std::length_error,batchCapacity(0,1));
        QVERIFY_THROWS_EXCEPTION(std::length_error,batchCapacity((std::numeric_limits<size_t>::max)(),100));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,startGeneration(pass::PassphraseRules{},101));
        QVERIFY(!generationBusy.load());
    }
    void batch100() {
        pass::PassphraseRules r; r.words=12; auto job=startGeneration(r,100); QVERIFY(job);
        QTRY_VERIFY_WITH_TIMEOUT(job->done.load(),10000); QVERIFY(job->error.isEmpty());
        QString text=job->secret.text(); const auto lines=text.split('\n'); QCOMPARE(lines.size(),100);
        for (const auto& line:lines) QVERIFY(pass::matches(line,r));
        QVERIFY(text.size()<=11999); wipeText(text); job->abandon(); QVERIFY(job->secret.empty());
    }
};
QTEST_GUILESS_MAIN(PassphraseTests)
#include "passphrase_test.moc"
