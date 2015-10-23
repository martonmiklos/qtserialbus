/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtSerialBus module.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtSerialBus/qmodbus.h>
#include <QtSerialBus/qmodbusrtuserialslave.h>
#include <QtSerialBus/qmodbustcpserver.h>
#include <QRegularExpression>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , modbusDevice(Q_NULLPTR)
{
    ui->setupUi(this);
    setupWidgetContainers();

    ui->connectType->setCurrentIndex(0);
    on_connectType_currentIndexChanged(0);
}

MainWindow::~MainWindow()
{
    if (modbusDevice)
        modbusDevice->disconnectDevice();
    delete modbusDevice;

    delete ui;
}

void MainWindow::on_connectType_currentIndexChanged(int index)
{
    if (modbusDevice) {
        modbusDevice->disconnect();
        delete modbusDevice;
        modbusDevice = Q_NULLPTR;
    }

    QModbusDevice::ModbusConnection type = static_cast<QModbusDevice::ModbusConnection> (index);
    if (type == QModbusDevice::Serial) {
        modbusDevice = new QModbusRtuSerialSlave(this);
    } else if (type == QModbusDevice::Tcp) {
        modbusDevice = new QModbusTcpServer(this);
        if (ui->portEdit->text().isEmpty())
            ui->portEdit->setText(QLatin1Literal("127.0.0.1:502"));
    }

    if (!modbusDevice) {
        ui->connectButton->setDisabled(true);
        if (type == QModbusDevice::Serial)
            ui->errorLabel->setText(tr("Could not create Modbus slave."));
        else
            ui->errorLabel->setText(tr("Could not create Modbus server."));
    } else {
        connect(modbusDevice, &QModbusClient::stateChanged,
                this, &MainWindow::onStateChanged);
        connect(modbusDevice, &QModbusServer::errorOccurred,
                this, &MainWindow::handleDeviceError);
    }
}

void MainWindow::handleDeviceError(QModbusDevice::ModbusError newError)
{
    if (newError == QModbusDevice::NoError || !modbusDevice)
        return;

    ui->errorLabel->setText(modbusDevice->errorString());
}

void MainWindow::on_connectButton_clicked()
{
    bool intendToConnect = (modbusDevice->state() == QModbusDevice::UnconnectedState);

    if (modbusDevice && intendToConnect) {
        QModbusDataUnitMap reg;
        reg.insert(QModbusDataUnit::Coils, { QModbusDataUnit::Coils, 0, 10 });
        reg.insert(QModbusDataUnit::DiscreteInputs, { QModbusDataUnit::DiscreteInputs, 0, 10 });
        reg.insert(QModbusDataUnit::InputRegisters, { QModbusDataUnit::InputRegisters, 0, 10 });
        reg.insert(QModbusDataUnit::HoldingRegisters, { QModbusDataUnit::HoldingRegisters, 0, 10 });

        modbusDevice->setMap(reg);

        connect(modbusDevice, &QModbusServer::dataWritten,
                this, &MainWindow::updateWidgets);
        connect(modbusDevice, &QModbusServer::stateChanged,
                this, &MainWindow::onStateChanged);
    }

    ui->errorLabel->setText(QString());

    if (intendToConnect) {
        modbusDevice->setPortName(ui->portEdit->text());
        modbusDevice->setSlaveId(ui->slaveEdit->text().toInt());
        if (modbusDevice->connectDevice()) {
            setupDeviceData();
        } else {
            ui->errorLabel->setText(tr("Connect failed: ") + modbusDevice->errorString());
        }
    } else {
        modbusDevice->disconnectDevice();
    }
}

void MainWindow::onStateChanged(int state)
{
    if (state == QModbusDevice::UnconnectedState)
        ui->connectButton->setText(tr("Connect"));
    else if (state == QModbusDevice::ConnectedState)
        ui->connectButton->setText(tr("Disconnect"));
}

void MainWindow::coilChanged(int id)
{
    QAbstractButton *button = coilButtons.button(id);
    bitChanged(id, QModbusDataUnit::Coils, button->isChecked());
}

void MainWindow::discreteInputChanged(int id)
{
    QAbstractButton *button = discreteButtons.button(id);
    bitChanged(id, QModbusDataUnit::DiscreteInputs, button->isChecked());
}

void MainWindow::bitChanged(int id, QModbusDataUnit::RegisterType table, bool value)
{
    if (!modbusDevice || modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    if (!modbusDevice->setData(table, id, value))
        ui->errorLabel->setText(tr("Could not set data: ") + modbusDevice->errorString());
}

void MainWindow::setRegister(const QString &value)
{
    if (!modbusDevice || modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    const QString objectName = QObject::sender()->objectName();
    if (registers.contains(objectName)) {
        bool ok = true;
        const int id = QObject::sender()->property("ID").toInt();
        if (objectName.startsWith(QStringLiteral("inReg")))
            ok = modbusDevice->setData(QModbusDataUnit::InputRegisters, id, value.toInt(&ok, 16));
        else if (objectName.startsWith(QStringLiteral("holdReg")))
            ok = modbusDevice->setData(QModbusDataUnit::HoldingRegisters, id, value.toInt(&ok, 16));

        if (!ok)
            ui->errorLabel->setText(tr("Could not set register: ") + modbusDevice->errorString());
    }
}

void MainWindow::updateWidgets(QModbusDataUnit::RegisterType table, int address, int size)
{
    for (int i = 0; i < size; ++i) {
        quint16 value;
        QString text;
        switch (table) {
        case QModbusDataUnit::Coils:
            modbusDevice->data(QModbusDataUnit::Coils, address + i, &value);
            coilButtons.button(address + i)->setChecked(value);
            break;
        case QModbusDataUnit::HoldingRegisters:
            modbusDevice->data(QModbusDataUnit::HoldingRegisters, address + i, &value);
            registers.value(QStringLiteral("holdReg_%1").arg(address + i))->setText(text
                .setNum(value, 16));
            break;
        default:
            break;
        }
    }
}

// -- private

void MainWindow::setupDeviceData()
{
    if (!modbusDevice || modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    for (int i = 0; i < coilButtons.buttons().count(); ++i)
        modbusDevice->setData(QModbusDataUnit::Coils, i, coilButtons.button(i)->isChecked());

    for (int i = 0; i < discreteButtons.buttons().count(); ++i) {
        modbusDevice->setData(QModbusDataUnit::DiscreteInputs, i,
            discreteButtons.button(i)->isChecked());
    }

    bool ok;
    foreach (QLineEdit *widget, registers) {
        if (widget->objectName().startsWith(QStringLiteral("inReg"))) {
            modbusDevice->setData(QModbusDataUnit::InputRegisters, widget->property("ID").toInt(),
                widget->text().toInt(&ok, 16));
        } else if (widget->objectName().startsWith(QStringLiteral("holdReg"))) {
            modbusDevice->setData(QModbusDataUnit::HoldingRegisters, widget->property("ID").toInt(),
                widget->text().toInt(&ok, 16));
        }
    }
}

void MainWindow::setupWidgetContainers()
{
    coilButtons.setExclusive(false);
    discreteButtons.setExclusive(false);

    QRegularExpression regexp(QStringLiteral("coils_(?<ID>\\d+)"));
    const QList<QCheckBox*> coils = findChildren<QCheckBox*>(regexp);
    foreach (QCheckBox *cbx, coils)
        coilButtons.addButton(cbx, regexp.match(cbx->objectName()).captured("ID").toInt());
    connect(&coilButtons, SIGNAL(buttonClicked(int)), this, SLOT(coilChanged(int)));

    regexp.setPattern(QStringLiteral("disc_(?<ID>\\d+)"));
    const QList<QCheckBox*> discs = findChildren<QCheckBox*>(regexp);
    foreach (QCheckBox *cbx, discs)
        discreteButtons.addButton(cbx, regexp.match(cbx->objectName()).captured("ID").toInt());
    connect(&discreteButtons, SIGNAL(buttonClicked(int)), this, SLOT(discreteInputChanged(int)));

    regexp.setPattern(QLatin1String("(in|hold)Reg_(?<ID>\\d+)"));
    const QList<QLineEdit*> qle = findChildren<QLineEdit*>(regexp);
    foreach (QLineEdit *lineEdit, qle) {
        registers.insert(lineEdit->objectName(), lineEdit);
        lineEdit->setProperty("ID", regexp.match(lineEdit->objectName()).captured("ID").toInt());
        lineEdit->setValidator(new QRegExpValidator(QRegExp(QStringLiteral("[0-9a-f]{0,4}"),
            Qt::CaseInsensitive), this));
        connect(lineEdit, &QLineEdit::textChanged, this, &MainWindow::setRegister);
    }
}
