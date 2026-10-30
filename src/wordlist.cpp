#include "wordlist.h"
#include <QCryptographicHash>
#include <QFile>
#include <QRegularExpression>
#include <QResource>

// Explicit reference keeps the resource object in static-library builds.
static void initWordlistResource() { Q_INIT_RESOURCE(wordlists); }
namespace pass {
Wordlist validateWordlist(const QByteArray& original) {
    Wordlist result;
    auto fail = [&result](const QString& error) {
        result.words.clear(); result.members.clear(); result.maxLength = 0;
        result.error = error; return result;
    };
    if (QCryptographicHash::hash(original, QCryptographicHash::Sha256).toHex() != WordlistSha256)
        return fail(QStringLiteral("EFF 詞表完整性檢查失敗；單詞模式已停用。"));
    auto rows = original.split('\n');
    if (!rows.isEmpty() && rows.last().isEmpty()) rows.removeLast();
    if (rows.size() != WordCount) return fail(QStringLiteral("EFF 詞表筆數無效。"));
    const QRegularExpression format(QStringLiteral("^([1-6]{5})\\t([a-z]+(?:-[a-z]+)*)$"));
    for (int i = 0; i < WordCount; ++i) {
        auto row = rows[i]; if (row.endsWith('\r')) row.chop(1);
        const auto match = format.match(QString::fromLatin1(row));
        if (!match.hasMatch()) return fail(QStringLiteral("EFF 詞表格式無效。"));
        QString code(5, QLatin1Char('1')); int ordinal = i;
        for (int j = 4; j >= 0; --j) { code[j] = QChar('1' + ordinal % 6); ordinal /= 6; }
        if (match.captured(1) != code) return fail(QStringLiteral("EFF 詞表骰碼或順序無效。"));
        const QString word = match.captured(2);
        if (result.members.contains(word)) return fail(QStringLiteral("EFF 詞表出現重複單詞。"));
        result.maxLength = qMax(result.maxLength, static_cast<int>(word.size()));
        result.members.insert(word); result.words.append(word);
    }
    return result;
}
const Wordlist& wordlist() {
    static const Wordlist list = [] {
        initWordlistResource();
        QFile file(QStringLiteral(":/wordlists/eff_large_wordlist.txt"));
        if (!file.open(QIODevice::ReadOnly)) {
            Wordlist unavailable; unavailable.error = QStringLiteral("無法載入 EFF 詞表；單詞模式已停用。"); return unavailable;
        }
        return validateWordlist(file.readAll());
    }();
    return list;
}
}
