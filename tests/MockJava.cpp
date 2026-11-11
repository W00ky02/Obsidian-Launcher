#include <QCoreApplication>
#include <QFile>
#include <QThread>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); const auto args=app.arguments();
    QFile out,err; out.open(stdout,QIODevice::WriteOnly); err.open(stderr,QIODevice::WriteOnly);
    if (args.contains("-version")) {err.write("openjdk version \"21.0.1\"\nOpenJDK 64-Bit Server VM\n"); err.flush(); return 0;}
    const auto token=args.value(args.indexOf("--accessToken")+1).toUtf8();
    out.write("[Render thread/INFO] Minecraft test child started\n"); out.flush();
    out.write(token.left(token.size()/2)); out.flush(); QThread::msleep(80);
    out.write(token.mid(token.size()/2)+"\n"); out.flush();
    err.write("[Render thread/WARN] Sample stderr message\n"); err.flush();
    QThread::msleep(180);
    return args.value(args.indexOf("--username")+1)=="FailPlayer"?42:0;
}
