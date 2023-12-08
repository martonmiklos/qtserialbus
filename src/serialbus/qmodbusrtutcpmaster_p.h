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
#pragma once

#include "qmodbusrtumaster_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qmath.h>
#include <QtCore/qpointer.h>
#include <QtCore/qqueue.h>
#include <QtCore/qtimer.h>
#include <QtSerialBus/qmodbusrtutcpmaster.h>
#include <QTcpSocket>
#include <QHostAddress>

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


class QModbusRtuTcpMasterPrivate : public QModbusRtuMasterPrivate
{
    Q_DECLARE_PUBLIC(QModbusRtuTcpMaster)
public:

    void onError(QAbstractSocket::SocketError error)
    {
        qCDebug(QT_MODBUS) << "(RTU server) QTcpSocket error:" << error
                           << (m_socket ? m_socket->errorString() : QString());
    }


    void onConnected()
    {
        qCDebug(QT_MODBUS) << "(TCP client) Connected to" << m_socket->peerAddress() << "on port"
                           << m_socket->peerPort();
        q_func()->setState(QModbusDevice::ConnectedState);
    }

    void onDisconnected()
    {
        qCDebug(QT_MODBUS) << "(TCP client) Connection closed.";
        q_func()->setState(QModbusDevice::UnconnectedState);
    }


    void setupDevice() override
    {
        Q_Q(QModbusRtuTcpMaster);
        m_socket = new QTcpSocket(q);
        m_ioDevice = m_socket;
        QModbusRtuMasterPrivate::setupDevice();
        QObject::connect(m_socket, &QAbstractSocket::connected, q, [this] { onConnected(); });
        QObject::connect(m_socket, &QAbstractSocket::disconnected, q, [this]() { onDisconnected(); });
        QObject::connect(m_socket, &QTcpSocket::errorOccurred,
                         q, [this](QAbstractSocket::SocketError error) {
                             onError(error);
                         });

    }

    void clearDevice() override
    {
        //m_socket->clear(QSerialPort::AllDirections);
    }

    void setupEnvironment() override
    {
        calculateInterFrameDelay();

        m_responseBuffer.clear();
        m_state = QModbusRtuMasterPrivate::Idle;
    }

    QTcpSocket *m_socket = nullptr;
};

QT_END_NAMESPACE

