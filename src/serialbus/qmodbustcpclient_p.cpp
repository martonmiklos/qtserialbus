/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include "qmodbustcpclient_p.h"
#include "private/qmodbusadu_p.h"

QT_BEGIN_NAMESPACE

void QModbusTcpClientPrivate::onError()
{
    Q_Q(QModbusTcpClient);
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        encapsulateRtu ? cleanupQueue() : cleanupTransactionStore();
        q->setState(QModbusDevice::UnconnectedState);
    }
    q->setError(QModbusClient::tr("TCP socket error (%1).").arg(m_socket->errorString()),
        QModbusDevice::ConnectionError);
}

void QModbusTcpClientPrivate::onConnected()
{
    qCDebug(QT_MODBUS) << "(TCP client) Connected to" << m_socket->peerAddress() << "on port"
        << m_socket->peerPort();
    responseBuffer.clear();
    q_func()->setState(QModbusDevice::ConnectedState);
}

void QModbusTcpClientPrivate::onDisconnected()
{
    qCDebug(QT_MODBUS) << "(TCP client) Connection closed.";
    q_func()->setState(QModbusDevice::UnconnectedState);
    encapsulateRtu ? cleanupQueue() : cleanupTransactionStore();
}

void QModbusTcpClientPrivate::cleanupQueue()
{
    //if (!m_element.timer.isNull())
    //    m_element.timer->stop();
    //m_wordTimer.stop();

    qCDebug(QT_MODBUS) << "(TCP) Cleanup of pending requests.";

    //if (!m_element.reply.isNull()) {
    //    m_element.reply->setError(
    //        QModbusDevice::ReplyAbortedError,
    //        QModbusClient::tr("Reply aborted due to connection closure."));
    //}
}

void QModbusTcpClientPrivate::cleanupTransactionStore()
{
    if (m_transactionStore.isEmpty())
        return;

    qCDebug(QT_MODBUS) << "(TCP client) Cleanup of pending requests.";

    for (const auto &elem : qAsConst(m_transactionStore)) {
        if (!elem.timer.isNull())
            elem.timer->stop();
        if (elem.reply.isNull())
            continue;
        elem.reply->setError(QModbusDevice::ReplyAbortedError,
            QModbusClient::tr("Reply aborted due to connection closure."));
    }
    m_transactionStore.clear();
}

/*
    // TODO: Review once we have a transport layer in place.
*/
bool QModbusTcpClientPrivate::isOpen() const
{
    if (m_socket)
        return m_socket->isOpen();
    return false;
}

QIODevice *QModbusTcpClientPrivate::device() const
{
    return m_socket;
}

bool QModbusTcpClientPrivate::writeToSocket(quint16 tId, const QModbusRequest &request, int address)
{
    QByteArray buffer;
    QDataStream output(&buffer, QIODevice::WriteOnly);
    if (encapsulateRtu) {
        output << QModbusSerialAdu::create(QModbusSerialAdu::Rtu, address, request);
        //output << quint8(address) << request;
        //output << QModbusSerialAdu::calculateCRC(buffer, buffer.size());
    } else {
        output << tId << quint16(0) << quint16(request.size() + 1) << quint8(address) << request;
    }

    int writtenBytes = m_socket->write(buffer);
    if (writtenBytes == -1 || writtenBytes < buffer.size()) {
        qCDebug(QT_MODBUS) << "(TCP client) Cannot write request to socket.";
        q_func()->setError(QModbusTcpClient::tr("Could not write request to socket."),
            QModbusDevice::WriteError);
        return false;
    }
    qCDebug(QT_MODBUS_LOW) << "(TCP client) Sent TCP ADU:" << buffer.toHex();
    qCDebug(QT_MODBUS) << "(TCP client) Sent TCP PDU:" << request << "with tId:" << Qt::hex << tId;
    return true;
}

QModbusReply *QModbusTcpClientPrivate::enqueueRequest(const QModbusRequest &request, int serverAddress,
    const QModbusDataUnit &unit, QModbusReply::ReplyType type)
{
    const quint16 tId = transactionId();
    if (!writeToSocket(tId, request, serverAddress))
        return nullptr;

    Q_Q(QModbusTcpClient);
    auto reply = new QModbusReply(type, serverAddress, q);
    const auto element = QueueElement { reply, request, unit, m_numberOfRetries,
        m_responseTimeoutDuration };
    m_transactionStore.insert(tId, element);

    q->connect(reply, &QObject::destroyed, q, [this, tId](QObject *) {
        if (!m_transactionStore.contains(tId))
            return;
        const QueueElement element = m_transactionStore.take(tId);
        if (element.timer)
            element.timer->stop();
    });

    if (element.timer) {
        q->connect(q, &QModbusClient::timeoutChanged,
            element.timer.data(), QOverload<int>::of(&QTimer::setInterval));
        QObject::connect(element.timer.data(), &QTimer::timeout, q, [this, tId]() {
            if (!m_transactionStore.contains(tId))
                return;

            QueueElement elem = m_transactionStore.take(tId);
            if (elem.reply.isNull())
                return;

            if (elem.numberOfRetries > 0) {
                elem.numberOfRetries--;
                if (!writeToSocket(tId, elem.requestPdu, elem.reply->serverAddress()))
                    return;
                m_transactionStore.insert(tId, elem);
                elem.timer->start();
                qCDebug(QT_MODBUS) << "(TCP client) Resend request with tId:" << Qt::hex << tId;
            } else {
                qCDebug(QT_MODBUS) << "(TCP client) Timeout of request with tId:" << Qt::hex << tId;
                elem.reply->setError(QModbusDevice::TimeoutError,
                    QModbusClient::tr("Request timeout."));
            }
        });
        element.timer->start();
    } else {
        qCWarning(QT_MODBUS) << "(TCP client) No response timeout timer for request with tId:"
            << Qt::hex << tId << ". Expected timeout:" << m_responseTimeoutDuration;
    }
    incrementTransactionId();

    return reply;
}

QT_END_NAMESPACE
