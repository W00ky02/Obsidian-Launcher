#include "NativeArchive.h"
#include "Minecraft.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QtEndian>
#include <zlib.h>

namespace {
quint16 u16(const QByteArray &a, qsizetype p) { return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(a.constData()+p)); }
quint32 u32(const QByteArray &a, qsizetype p) { return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(a.constData()+p)); }
void need(bool ok, const QString &path) { if (!ok) throw Minecraft::Error{"Ungültiges/unsupported Native-JAR: " + path}; }
}
void NativeArchive::extract(const QString &archive, const QString &destination, const QStringList &excludes) {
    QFile file(archive);
    need(file.open(QIODevice::ReadOnly) && file.size() < 256*1024*1024, archive);
    const QByteArray data = file.readAll();
    qsizetype end = -1;
    for (qsizetype p = data.size()-22; p >= qMax<qsizetype>(0, data.size()-65557); --p) {
        if (u32(data,p) == 0x06054b50 && p+22+u16(data,p+20) == data.size()) { end = p; break; }
    }
    need(end >= 0, archive);
    const quint16 count = u16(data,end+10);
    need(u16(data,end+4)==0 && u16(data,end+6)==0 && u16(data,end+8)==count && count!=0xffff,archive);
    qsizetype cursor = u32(data,end+16);
    need(cursor <= end && u32(data,end+12) <= quint64(end-cursor), archive);
    quint64 total = 0;
    for (quint16 i=0; i<count; ++i) {
        need(cursor >= 0 && cursor+46 <= end && u32(data,cursor)==0x02014b50, archive);
        const quint16 flags=u16(data,cursor+8), method=u16(data,cursor+10);
        const quint32 crc=u32(data,cursor+16), packed=u32(data,cursor+20), size=u32(data,cursor+24), offset=u32(data,cursor+42);
        const quint16 names=u16(data,cursor+28), extra=u16(data,cursor+30), comment=u16(data,cursor+32);
        need(cursor+46+names+extra+comment <= end && !(flags&1) && packed!=0xffffffff && size!=0xffffffff && offset!=0xffffffff, archive);
        QString name=QString::fromUtf8(data.mid(cursor+46,names));
        cursor+=46+names+extra+comment;
        name.replace('\\','/');
        need(!name.startsWith('/') && !name.contains(':') && !name.split('/').contains("..") && !name.contains(QChar(0)),archive);
        bool excluded=name.startsWith("META-INF/") || name.endsWith('/');
        for (const auto &prefix:excludes) if (name.startsWith(prefix)) excluded=true;
        if (excluded) continue;
        need(size<=64*1024*1024 && (total+=size)<=256*1024*1024 && quint64(offset)+30<=quint64(data.size()),archive);
        need(u32(data,offset)==0x04034b50,archive);
        const quint64 start=quint64(offset)+30+u16(data,offset+26)+u16(data,offset+28);
        need(start+packed<=quint64(data.size()) && (method==0 || method==8),archive);
        QByteArray output;
        if (method==0) { need(packed==size,archive); output=data.mid(qsizetype(start),size); }
        else {
            output.resize(qsizetype(size)+1);
            z_stream stream{};
            stream.next_in=reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()+start)); stream.avail_in=packed;
            stream.next_out=reinterpret_cast<Bytef*>(output.data()); stream.avail_out=size+1;
            need(inflateInit2(&stream,-MAX_WBITS)==Z_OK,archive);
            const int result=inflate(&stream,Z_FINISH);
            const bool valid=result==Z_STREAM_END && stream.total_out==size && stream.total_in==packed;
            inflateEnd(&stream); need(valid,archive); output.resize(size);
        }
        need(crc32(0,reinterpret_cast<const Bytef*>(output.constData()),uInt(output.size()))==crc,archive);
        const QString target=QDir(destination).filePath(name);
        need(QDir().mkpath(QFileInfo(target).absolutePath()),archive);
        QFile out(target); need(out.open(QIODevice::WriteOnly) && out.write(output)==output.size(),archive);
    }
}
