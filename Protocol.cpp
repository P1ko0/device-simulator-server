#include "Protocol.h"
#include <QStringList>

bool parseRequest(
    const QString& message,
    Request& request,
    QString& error
    )
{
    error.clear();

    QStringList fields = message.split('|', Qt::KeepEmptyParts);

    if (fields.size() != 3)
    {
        error = "INVALID_FORMAT";
        return false;
    }

    if (fields[0].isEmpty())
    {
        error = "EMPTY_COMMAND";
        return false;
    }

    bool idOk = false;
    int id = fields[1].toInt(&idOk);

    if (!idOk || id <= 0)
    {
        error = "INVALID_DEVICE_ID";
        return false;
    }

    // 基本检查全部通过后，再填写解析结果
    request.command = fields[0];
    request.deviceId = id;
    request.value = fields[2];

    return true;
}