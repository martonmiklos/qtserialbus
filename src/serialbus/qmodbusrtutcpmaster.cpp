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

#include "qmodbusrtutcpmaster.h"
#include "qmodbusrtutcpmaster_p.h"

#include <QtCore/qloggingcategory.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_MODBUS)
Q_DECLARE_LOGGING_CATEGORY(QT_MODBUS_LOW)

/*!
    \class QModbusRtuTcpMaster
    \inmodule QtSerialBus
    \since 5.8

    \brief The QModbusRtuTcpMaster class represents a Modbus client
    that uses a serial bus for its communication with the Modbus server.

    Communication via Modbus requires the interaction between a single
    Modbus client instance and multiple Modbus servers. This class
    provides the client implementation via a serial port.
*/

/*!
    Constructs a serial Modbus master with the specified \a parent.
*/
QModbusRtuTcpMaster::QModbusRtuTcpMaster(QObject *parent)
    : QModbusRtuMaster(new QModbusRtuTcpMasterPrivate, parent)
{
    Q_D(QModbusRtuTcpMaster);
    d->setupDevice();
}

/*!
    \internal
*/
QModbusRtuTcpMaster::~QModbusRtuTcpMaster()
{
    close();
}

/*!
    \internal
*/
QModbusRtuTcpMaster::QModbusRtuTcpMaster(QModbusRtuTcpMasterPrivate &dd, QObject *parent)
    : QModbusRtuMaster(dd, parent)
{
    Q_D(QModbusRtuTcpMaster);
    d->setupDevice();
}

/*!
     \reimp

     \note When calling this function, existing buffered data is removed from
     the serial port.
*/
bool QModbusRtuTcpMaster::open()
{
    if (state() == QModbusDevice::ConnectedState)
        return true;

    Q_D(QModbusRtuTcpMaster);
    d->setupEnvironment(); // to be done before open
    if (d->m_socket->open(QIODevice::ReadWrite)) {
        setState(QModbusDevice::ConnectingState);
        const QUrl url = QUrl::fromUserInput(d->m_networkAddress + QStringLiteral(":")
                                             + QString::number(d->m_networkPort));

        if (!url.isValid()) {
            setError(tr("Invalid connection settings for TCP communication specified."),
                     QModbusDevice::ConnectionError);
            qCWarning(QT_MODBUS) << "(TCP/RTU client) Invalid host:" << url.host() << "or port:"
                                 << url.port();
            return false;
        }

        d->m_socket->connectToHost(url.host(), url.port());
        //d->m_socket->clear(); // only possible after open
    } else {
        setError(d->m_socket->errorString(), QModbusDevice::ConnectionError);
    }
    return (state() == QModbusDevice::ConnectingState);
}

/*!
     \reimp
*/
void QModbusRtuTcpMaster::close()
{
    if (state() == QModbusDevice::UnconnectedState)
        return;

    setState(QModbusDevice::ClosingState);

    Q_D(QModbusRtuTcpMaster);
    d->m_socket->disconnectFromHost();

    int numberOfAborts = 0;
    while (!d->m_queue.isEmpty()) {
        // Finish each open reply and forget them
        QModbusRtuTcpMasterPrivate::QueueElement elem = d->m_queue.dequeue();
        if (!elem.reply.isNull()) {
            elem.reply->setError(QModbusDevice::ReplyAbortedError,
                                 QModbusClient::tr("Reply aborted due to connection closure."));
            numberOfAborts++;
        }
    }

    if (numberOfAborts > 0)
        qCDebug(QT_MODBUS_LOW) << "(RTU client) Aborted replies:" << numberOfAborts;

    setState(QModbusDevice::UnconnectedState);
}

QT_END_NAMESPACE
