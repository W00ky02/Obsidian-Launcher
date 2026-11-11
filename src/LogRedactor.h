#pragma once
#include <QString>
#include <QStringList>
#include <QMap>

// Per-channel buffering prevents a secret split across read events leaking to UI.
// Only a suffix that can still become a secret is retained.
class LogRedactor {
public:
    void setSecrets(const QStringList &values) { clear(); for (const auto &v:values) if (!v.isEmpty()) secrets.append(v); }
    QString feed(const QString &channel, const QString &text, bool final=false) {
        QString input=pending.take(channel)+text, output;
        qsizetype position=0;
        while (position<input.size()) {
            bool matched=false;
            for (const auto &secret:secrets) {
                if (QStringView(input).mid(position).startsWith(secret)) { output+="[REDACTED]"; position+=secret.size(); matched=true; break; }
            }
            if (matched) continue;
            bool prefix=false;
            if (!final) for (const auto &secret:secrets) if (QStringView(secret).startsWith(QStringView(input).mid(position))) { prefix=true; break; }
            if (prefix) { pending[channel]=input.mid(position); break; }
            output+=input[position++];
        }
        return output;
    }
    void clear() { pending.clear(); secrets.clear(); }
private:
    QStringList secrets;
    QMap<QString,QString> pending;
};
