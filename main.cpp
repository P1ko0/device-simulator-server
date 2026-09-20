#include <QCoreApplication>
#include <QTcpServer>
#include <QHostAddress>
#include <QDebug>
#include <QTcpSocket>
#include <QModbusTcpServer>
#include <QModbusDataUnit>
#include <QVariant>
#include <QTimer>
#include "Device.h"
#include "Protocol.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    Device device;

    QModbusTcpServer modbusServer;

    // 定义保持寄存器
    QModbusDataUnit holdingRegisters(
        QModbusDataUnit::HoldingRegisters,
        0,
        2
        );

    // 定义输入寄存器：从地址0开始，共3个
    QModbusDataUnit inputRegisters(
        QModbusDataUnit::InputRegisters,
        0,
        3
        );

    // 定义线圈
    QModbusDataUnit coils(
        QModbusDataUnit::Coils,
        0,
        1
        );

    QModbusDataUnitMap registerMap;

    registerMap.insert(
        QModbusDataUnit::InputRegisters,
        inputRegisters
        );

    registerMap.insert(
        QModbusDataUnit::HoldingRegisters,
        holdingRegisters
        );

    registerMap.insert(
        QModbusDataUnit::Coils,
        coils
        );

    if (!modbusServer.setMap(registerMap))
    {
        qCritical() << "Modbus register map setup failed";
        return 1;
    }

    //设定保持寄存器测试值
    holdingRegisters.setValue(0, 1800);
    holdingRegisters.setValue(1, 60);

    // 设置固定测试值
    inputRegisters.setValue(0, 2200);
    inputRegisters.setValue(1, 30);
    inputRegisters.setValue(2, 2);

    if(!modbusServer.setData(holdingRegisters))
    {
        qCritical() << "保持寄存器初始化失败";
        return 1;
    }

    if (!modbusServer.setData(inputRegisters))
    {
        qCritical() << "Modbus test data setup failed";
        return 1;
    }

    auto syncModbusStatus = [&modbusServer, &device]
    {
        auto status = device.getStatus();
        quint16 statusCode = 0;

        if(status == "idle")
        {
            statusCode = 0;
        }
        else if(status == "running")
        {
            statusCode = 1;
        }
        else if(status == "stopped")
        {
            statusCode = 2;
        }
        else if(status == "error")
        {
            statusCode = 3;
        }
        else
        {
            qWarning() << "未知设备状态";
            return;
        }

        if(!modbusServer.setData(QModbusDataUnit::InputRegisters,2,statusCode))
        {
            qWarning() << "同步设备状态失败";
            return;
        }
    };

    syncModbusStatus();

    QObject::connect(
        &modbusServer,
        &QModbusServer::dataWritten,
        &app,
        [&device, &modbusServer, &syncModbusStatus](QModbusDataUnit::RegisterType table, int address, int size)
        {
            if(table != QModbusDataUnit::Coils || address != 0 ||size != 1)
            {
                return;
            }
            else
            {
                quint16 coilValue = 0;
                if(!modbusServer.data(QModbusDataUnit::Coils,0,&coilValue))
                {
                    qWarning() << "读取启动线圈失败";
                    return;
                }
                if(!coilValue)
                {
                    return;
                }
                else
                {
                    device.start();

                    syncModbusStatus();

                    if(!modbusServer.setData(QModbusDataUnit::Coils, 0, 0))
                    {
                        qWarning() << "启动线圈清零失败";
                    }
                }
            }
        }
        );

    // 设置监听地址、端口和设备编号
    modbusServer.setConnectionParameter(
        QModbusDevice::NetworkAddressParameter,
        QStringLiteral("127.0.0.1")
        );

    modbusServer.setConnectionParameter(
        QModbusDevice::NetworkPortParameter,
        1502
        );

    modbusServer.setServerAddress(1);

    if (!modbusServer.connectDevice())
    {
        qCritical() << "Modbus server start failed:"
                    << modbusServer.errorString();
        return 1;
    }

    qInfo() << "Modbus server listening on 127.0.0.1:1502";

    QTcpServer server;

    QObject::connect(
        &server,
        &QTcpServer::newConnection,
        &app,
        [&server, &device, &syncModbusStatus]
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
                [client, &device, &syncModbusStatus]
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

                            syncModbusStatus();

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