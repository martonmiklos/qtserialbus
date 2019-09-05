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

#ifndef QMODBUSTCPRTUCLIENT_P_H
#define QMODBUSTCPRTUCLIENT_P_H

#include <QtCore/qloggingcategory.h>
#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qtcpsocket.h>
#include <QtSerialBus/qmodbustcprtuclient.h>

#include <private/qmodbusadu_p.h>
#include <private/qmodbusclient_p.h>
#include <private/qmodbus_symbols_p.h>

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

class QModbusTcpRtuClientPrivate : public QModbusClientPrivate
{
    Q_DECLARE_PUBLIC(QModbusTcpRtuClient)

public:
    bool canMatchRequestAndResponse(const QModbusResponse &response, int sendingServer) const
    {
        if (m_element.reply.isNull())
            return false;   // reply deleted
        if (m_element.reply->serverAddress() != sendingServer)
            return false;   // server mismatch
        if (m_element.requestPdu.functionCode() != response.functionCode())
            return false;   // request for different function code
        return true;
    }

    void setupTcpSocket()
    {
        Q_Q(QModbusTcpRtuClient);

        m_socket = new QTcpSocket(q);


        m_wordTimer.setInterval(m_responseTimeoutDuration / 4);
        QObject::connect(&m_wordTimer, &QTimer::timeout, q, [=]() {
            if (!m_element.timer->isActive()) {
                return;
            }

            const QModbusSerialAdu tmpAdu(QModbusSerialAdu::Rtu, m_responseBuffer);
            int pduSizeWithoutFcode = QModbusResponse::calculateDataSize(tmpAdu.pdu());
            if (pduSizeWithoutFcode < 0) {
                // wait for more data
                qCDebug(QT_MODBUS) << "(TCP/RTU client) Cannot calculate PDU size for function code:"
                    << tmpAdu.pdu().functionCode() << ", delaying pending frame";
                return;
            }

            // server address byte + function code byte + PDU size + 2 bytes CRC
            int aduSize = 2 + pduSizeWithoutFcode + 2;
            if (tmpAdu.rawSize() < aduSize) {
                qCDebug(QT_MODBUS) << "(TCP/RTU client) Incomplete ADU received, ignoring";
                return;
            }

            // Special case for Diagnostics:ReturnQueryData. The response has no
            // length indicator and is just a simple echo of what we have send.
            if (tmpAdu.pdu().functionCode() == QModbusPdu::Diagnostics) {
                const QModbusResponse response = tmpAdu.pdu();
                if (canMatchRequestAndResponse(response, tmpAdu.serverAddress())) {
                    quint16 subCode = 0xffff;
                    response.decodeData(&subCode);
                    if (subCode == Diagnostics::ReturnQueryData) {
                        if (response.data() != m_element.requestPdu.data())
                            return; // echo does not match request yet
                        aduSize = 2 + response.dataSize() + 2;
                        if (tmpAdu.rawSize() < aduSize)
                            return; // echo matches, probably checksum missing
                    }
                }
            }

            // stop the timer as soon as we know
            m_element.timer->stop();

            const QModbusSerialAdu adu(QModbusSerialAdu::Rtu, m_responseBuffer.left(aduSize));
            m_responseBuffer.remove(0, aduSize);

            qCDebug(QT_MODBUS) << "(RTU client) Received ADU:" << adu.rawData().toHex();
            if (QT_MODBUS().isDebugEnabled() && !m_responseBuffer.isEmpty())
                qCDebug(QT_MODBUS_LOW) << "(RTU client) Pending buffer:" << m_responseBuffer.toHex();

            // check CRC
            if (!adu.matchingChecksum()) {
                cleanupQueue();
                qCWarning(QT_MODBUS) << "(RTU client) Discarding response with wrong CRC, received:"
                    << adu.checksum<quint16>() << ", calculated CRC:"
                    << QModbusSerialAdu::calculateCRC(adu.data(), adu.size());
                return;
            }

            const QModbusResponse response = adu.pdu();
            if (!canMatchRequestAndResponse(response, adu.serverAddress())) {
                cleanupQueue();
                qCWarning(QT_MODBUS) << "(RTU client) Cannot match response with open request, "
                    "ignoring";
                return;
            }

            processQueueElement(response, m_element);
        });

        QObject::connect(m_socket, &QIODevice::readyRead, q, [this](){
            QByteArray data = m_socket->read(m_socket->bytesAvailable());
            qCDebug(QT_MODBUS_LOW) << "(TCP/RTU client) Response buffer:" << data.toHex();
            m_responseBuffer += data;
        });
    }

    QModbusReply *enqueueRequest(const QModbusRequest &request, int serverAddress,
                                 const QModbusDataUnit &unit,
                                 QModbusReply::ReplyType type) override
    {
        auto writeToSocket = [this](const QModbusRequest &request, int address) {
            QByteArray buffer;
            QDataStream output(&buffer, QIODevice::WriteOnly);
            output << quint8(address) << request;
            output << QModbusSerialAdu::calculateCRC(buffer, buffer.size());

            int writtenBytes = m_socket->write(buffer);
            if (writtenBytes == -1 || writtenBytes < buffer.size()) {
                Q_Q(QModbusTcpRtuClient);
                qCDebug(QT_MODBUS) << "(TCP/RTU client) Cannot write request to socket.";
                q->setError(QModbusTcpRtuClient::tr("Could not write request to socket."),
                            QModbusDevice::WriteError);
                return false;
            }
            qCDebug(QT_MODBUS_LOW) << "(TCP/RTU client) Sent TCP ADU:" << buffer.toHex();
            qCDebug(QT_MODBUS) << "(TCP/RTU client) Sent TCP PDU:" << request;
            return true;
        };

        if (!writeToSocket(request, serverAddress))
            return nullptr;

        Q_Q(QModbusTcpRtuClient);
        auto reply = new QModbusReply(type, serverAddress, q);
        const auto element = QueueElement{
            reply, request, unit, m_numberOfRetries, m_responseTimeoutDuration };
        m_element = element;

        q->connect(reply, &QObject::destroyed, q, [this](QObject *) {
            const QueueElement element = m_element;
            if (element.timer)
                element.timer->stop();
        });

        if (element.timer) {
            q->connect(q, &QModbusClient::timeoutChanged,
                       element.timer.data(), QOverload<int>::of(&QTimer::setInterval));
            q->connect(q, &QModbusClient::timeoutChanged,
                &m_wordTimer, [=](int newTimeout) {
                    m_wordTimer.setInterval(newTimeout / 4);
                });
            QObject::connect(element.timer.data(), &QTimer::timeout, q, [this, writeToSocket]() {
                QueueElement elem = m_element;
                if (elem.reply.isNull())
                    return;

                if (elem.numberOfRetries > 0) {
                    elem.numberOfRetries--;
                    if (!writeToSocket(elem.requestPdu, elem.reply->serverAddress()))
                        return;
                    elem.timer->start();
                    m_wordTimer.start();
                    qCDebug(QT_MODBUS) << "(TCP/RTU client) Resend request";
                } else {
                    qCDebug(QT_MODBUS) << "(TCP/RTU client) Timeout of request";
                    elem.reply->setError(QModbusDevice::TimeoutError,
                        QModbusClient::tr("Request timeout."));
                }
            });
            element.timer->start();
            m_wordTimer.start();
        } else {
            qCWarning(QT_MODBUS) << "(TCP/RTU client) No response timeout timer for request."
                << "Expected timeout:" << m_responseTimeoutDuration;
        }

        return reply;
    }

      void cleanupQueue()
    {
    //    if (!m_element.timer.isNull())
    //        m_element.timer->stop();
    //    m_wordTimer.stop();
    //    m_responseBuffer.clear();
    //    qCDebug(QT_MODBUS) << "(TCP/RTU client) Cleanup of pending requests";

    //    if (!m_element.reply.isNull()) {
    //        m_element.reply->setError(
    //                QModbusDevice::ReplyAbortedError,
    //                QModbusClient::tr("Reply aborted due to connection closure."));
    //    }
    }

    QTimer       m_wordTimer;
    QueueElement m_element;
    QByteArray   m_responseBuffer;
    QTcpSocket  *m_socket = nullptr;
    int mbpaHeaderSize = 7;
};

QT_END_NAMESPACE

#endif // QMODBUSTCPRTUCLIENT_P_H
