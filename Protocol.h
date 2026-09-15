#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>

struct Request
{
    QString command;
    int deviceId = 0;
    QString value;
};

bool parseRequest(
    const QString& message,
    Request& request,
    QString& error
    );

#endif // PROTOCOL_H