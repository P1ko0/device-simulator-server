#include <QCoreApplication>
#include <QTcpServer>
#include <QHostAddress>
#include <QDebug>
#include <QTcpSocket>
#include <QTimer>
#include "Device.h"
#include "Protocol.h"

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
                        QByteArray line = client->readLine();
                        line.chop(1);

                        qInfo() << "Received:" << line;

                        Request request;
                        QString error;

                        bool success = parseRequest(
                            QString::fromUtf8(line),
                            request,
                            error
                            );

                        // 格式正确后，再检查设备是否存在
                        if (success && request.deviceId != 1)
                        {
                            error = "DEVICE_NOT_FOUND";
                            success = false;
                        }

                        if (!success)
                        {
                            QString reply = "ERR|" + error + "|"
                                            + QString::fromStdString(device.getStatus());

                            client->write((reply + "\n").toUtf8());

                            // 本条消息有问题，不执行后面的设备操作
                            continue;
                        }

                        QString command = request.command;

                        QString reply;

                        // 除 SET_SPEED 外，当前其他命令都不需要参数
                        if (command != "SET_SPEED" && !request.value.isEmpty())
                        {
                            QString errorReply = "ERR|UNEXPECTED_VALUE|"
                                                 + QString::fromStdString(device.getStatus());

                            client->write((errorReply + "\n").toUtf8());
                            continue;
                        }

                        if (command == "GET_STATUS")
                        {
                            reply = "STATE|"
                                    + QString::fromStdString(device.getStatus());
                        }
                        else if (command == "SET_SPEED")
                        {
                            bool valueOk = false;
                            int speed = request.value.toInt(&valueOk);

                            if (!valueOk)
                            {
                                reply = "ERR|INVALID_VALUE|"
                                        + QString::fromStdString(device.getStatus());
                            }
                            else if (!device.setSpeedSetpoint(speed))
                            {
                                reply = "ERR|OUT_OF_RANGE|"
                                        + QString::fromStdString(device.getStatus());
                            }
                            else
                            {
                                reply = "OK|"
                                        + QString::number(device.getSpeedSetpoint());
                            }
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
                            else if (command == "FAULT")
                            {
                                device.fault();
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

                        qInfo() << "Reply scheduled in 2 seconds:" << reply;

                        QTimer::singleShot(
                            2000,
                            client,
                            [client, data, reply]
                            {
                                if (client->state() != QAbstractSocket::ConnectedState)
                                {
                                    return;
                                }

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
                            );
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