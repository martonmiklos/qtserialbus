/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QMODBUSTCPCLIENT_P_H
#define QMODBUSTCPCLIENT_P_H

#include <QtCore/qloggingcategory.h>
#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qtcpsocket.h>
#include "QtSerialBus/qmodbustcpclient.h"

#include "private/qmodbusclient_p.h"

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_MODBUS)
Q_DECLARE_LOGGING_CATEGORY(QT_MODBUS_LOW)

class QModbusTcpClientPrivate : public QModbusClientPrivate
{
    Q_DECLARE_PUBLIC(QModbusTcpClient)

public:
    void onError();
    void onConnected();
    void onDisconnected();

    void cleanupQueue();
    void cleanupTransactionStore();

    bool isOpen() const override;
    QIODevice *device() const override;

    bool writeToSocket(quint16 tId, const QModbusRequest &request, int address);
    QModbusReply *enqueueRequest(const QModbusRequest &request, int serverAddress,
        const QModbusDataUnit &unit, QModbusReply::ReplyType type) override;


    void setupTcpSocket()
    {
        Q_Q(QModbusTcpClient);

        m_socket = new QTcpSocket(q);

        QObject::connect(m_socket, &QAbstractSocket::connected, q, [this] { onConnected(); });
        QObject::connect(m_socket, &QAbstractSocket::disconnected, q, [this]() { onDisconnected(); });
        QObject::connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), q,
            [this](QAbstractSocket::SocketError /*error*/) { onError(); });

        QObject::connect(m_socket, &QIODevice::readyRead, q, [this]() {
            responseBuffer += m_socket->read(m_socket->bytesAvailable());
            qCDebug(QT_MODBUS_LOW) << "(TCP client) Response buffer:" << responseBuffer.toHex();

            while (!responseBuffer.isEmpty()) {
                // can we read enough for Modbus ADU header?
                if (responseBuffer.size() < mbpaHeaderSize) {
                    qCDebug(QT_MODBUS_LOW) << "(TCP client) Modbus ADU not complete";
                    return;
                }

                quint8 serverAddress;
                quint16 transactionId, bytesPdu, protocolId;
                QDataStream input(responseBuffer);
                input >> transactionId >> protocolId >> bytesPdu >> serverAddress;

                // stop the timer as soon as we know enough about the transaction
                const bool knownTransaction = m_transactionStore.contains(transactionId);
                if (knownTransaction && m_transactionStore[transactionId].timer)
                    m_transactionStore[transactionId].timer->stop();

                qCDebug(QT_MODBUS) << "(TCP client) tid:" << Qt::hex << transactionId << "size:"
                    << bytesPdu << "server address:" << serverAddress;

                // The length field is the byte count of the following fields, including the Unit
                // Identifier and the PDU, so we remove on byte.
                bytesPdu--;

                int tcpAduSize = mbpaHeaderSize + bytesPdu;
                if (responseBuffer.size() < tcpAduSize) {
                    qCDebug(QT_MODBUS) << "(TCP client) PDU too short. Waiting for more data";
                    return;
                }

                QModbusResponse responsePdu;
                input >> responsePdu;
                qCDebug(QT_MODBUS) << "(TCP client) Received PDU:" << responsePdu.functionCode()
                                   << responsePdu.data().toHex();

                responseBuffer.remove(0, tcpAduSize);

                if (!knownTransaction) {
                    qCDebug(QT_MODBUS) << "(TCP client) No pending request for response with "
                        "given transaction ID, ignoring response message.";
                } else {
                    processQueueElement(responsePdu, m_transactionStore[transactionId]);
                }
            }
        });
    }

    // This doesn't overflow, it rather "wraps around". Expected.
    inline void incrementTransactionId() { m_transactionId++; }
    inline qint16 transactionId() const { return m_transactionId; }

    QTcpSocket *m_socket = nullptr;
    QByteArray responseBuffer;
    QHash<quint16, QueueElement> m_transactionStore;
    int mbpaHeaderSize = 7;
    bool encapsulateRtu { false };

private:   // Private to avoid using the wrong id inside the timer lambda,
    quint16 m_transactionId = 0; // capturing 'this' will not copy the id.
};

QT_END_NAMESPACE

#endif // QMODBUSTCPCLIENT_P_H
