#include <QCoreApplication>
#include <QTcpServer>
#include <QHostAddress>
#include <QDebug>
#include <QTcpSocket>
#include "Device.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    Device device;

    QTcpServer server;

    QObject::connect(
        &server,
        &QTcpServer::newConnection,
        &app,
        [&server, &device]
        {
            QTcpSocket *client = server.nextPendingConnection();

            if (client == nullptr)
            {
                return;
            }

            qInfo() << "Client connected";

            QObject::connect(
                client,
                &QTcpSocket::readyRead,
                client,
                [client, &device]
                {
                    while (client->canReadLine())
                    {
                        QByteArray command = client->readLine();
                        command.chop(1);

                        qInfo() << "Received:" << command;

                        QString reply;

                        if (command == "GET_STATUS")
                        {
                            reply = "STATE|"
                                    + QString::fromStdString(device.getStatus());
                        }
                        else
                        {
                            // 保存执行前的状态，是复制，不是引用
                            auto oldStatus = device.getStatus();

                            bool knownCommand = true;

                            if (command == "START")
                            {
                                device.start();
                            }
                            else if (command == "STOP")
                            {
                                device.stop();
                            }
                            else if (command == "RESET")
                            {
                                device.reset();
                            }
                            else
                            {
                                knownCommand = false;
                            }

                            QString status =
                                QString::fromStdString(device.getStatus());

                            if (!knownCommand)
                            {
                                reply = "ERR|UNKNOWN_COMMAND|" + status;
                            }
                            else if (oldStatus != device.getStatus())
                            {
                                reply = "OK|" + status;
                            }
                            else
                            {
                                reply = "ERR|INVALID_STATE|" + status;
                            }
                        }

                        // 回复也使用换行符作为结束标记
                        QByteArray data = (reply + "\n").toUtf8();

                        if (client->write(data) == -1)
                        {
                            qWarning() << "Reply failed:"
                                       << client->errorString();
                        }
                        else
                        {
                            qInfo() << "Reply queued:" << reply;
                        }
                    }
                }
                );

            QObject::connect(
                client,
                &QTcpSocket::disconnected,
                client,
                [client]
                {
                    qInfo() << "Client disconnected";
                    client->deleteLater();
                }
                );
        }
        );

    bool success = server.listen(QHostAddress::LocalHost, 5000);

    if (!success)
    {
        qCritical() << "Listen failed:" << server.errorString();
        return 1;
    }

    qInfo() << "Listening on 127.0.0.1:5000";

    return app.exec();
}