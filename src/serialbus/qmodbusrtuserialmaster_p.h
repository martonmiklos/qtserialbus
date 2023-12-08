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
#include <QtSerialBus/qmodbusrtuserialmaster.h>
#include <QtSerialPort/qserialport.h>

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


class QModbusRtuSerialMasterPrivate : public QModbusRtuMasterPrivate
{
    Q_DECLARE_PUBLIC(QModbusRtuSerialMaster)
public:

    void onError(QSerialPort::SerialPortError error)
    {
        if (error == QSerialPort::NoError)
            return;

        qCDebug(QT_MODBUS) << "(RTU server) QSerialPort error:" << error
            << (m_serialPort ? m_serialPort->errorString() : QString());

        Q_Q(QModbusRtuSerialMaster);

        switch (error) {
        case QSerialPort::DeviceNotFoundError:
            q->setError(QModbusDevice::tr("Referenced serial device does not exist."),
                QModbusDevice::ConnectionError);
            break;
        case QSerialPort::PermissionError:
            q->setError(QModbusDevice::tr("Cannot open serial device due to permissions."),
                QModbusDevice::ConnectionError);
            break;
        case QSerialPort::OpenError:
        case QSerialPort::NotOpenError:
            q->setError(QModbusDevice::tr("Cannot open serial device."),
                QModbusDevice::ConnectionError);
            break;
        case QSerialPort::WriteError:
            q->setError(QModbusDevice::tr("Write error."), QModbusDevice::WriteError);
            break;
        case QSerialPort::ReadError:
            q->setError(QModbusDevice::tr("Read error."), QModbusDevice::ReadError);
            break;
        case QSerialPort::ResourceError:
            q->setError(QModbusDevice::tr("Resource error."), QModbusDevice::ConnectionError);
            break;
        case QSerialPort::UnsupportedOperationError:
            q->setError(QModbusDevice::tr("Device operation is not supported error."),
                QModbusDevice::ConfigurationError);
            break;
        case QSerialPort::TimeoutError:
            q->setError(QModbusDevice::tr("Timeout error."), QModbusDevice::TimeoutError);
            break;
        case QSerialPort::UnknownError:
            q->setError(QModbusDevice::tr("Unknown error."), QModbusDevice::UnknownError);
            break;
        default:
            qCDebug(QT_MODBUS) << "(RTU server) Unhandled QSerialPort error" << error;
            break;
        }
    }

    void setupDevice() override
    {
        Q_Q(QModbusRtuSerialMaster);
        m_serialPort = new QSerialPort(q);
        m_ioDevice = m_serialPort;
        QModbusRtuMasterPrivate::setupDevice();
        QObject::connect(m_serialPort, &QSerialPort::errorOccurred,
                q, [this](QSerialPort::SerialPortError error) {
            onError(error);
        });
    }

    void clearDevice() override
    {
        m_serialPort->clear(QSerialPort::AllDirections);
    }

    void setupEnvironment() override
    {
        if (m_serialPort) {
            m_serialPort->setPortName(m_comPort);
            m_serialPort->setParity(m_parity);
            m_serialPort->setBaudRate(m_baudRate);
            m_serialPort->setDataBits(m_dataBits);
            m_serialPort->setStopBits(m_stopBits);
        }

        calculateInterFrameDelay();

        m_responseBuffer.clear();
        m_state = QModbusRtuSerialMasterPrivate::Idle;
    }

    QSerialPort *m_serialPort = nullptr;
};

QT_END_NAMESPACE

