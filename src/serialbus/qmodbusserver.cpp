/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "qmodbusserver.h"
#include "qmodbusserver_p.h"
#include "qmodbus_symbols_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qloggingcategory.h>

#include <bitset>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_MODBUS)

/*!
    \class QModbusServer
    \inmodule QtSerialBus
    \since 5.6

    \brief The QModbusServer class is the interface to receive and process Modbus requests.

    Modbus networks can have multiple Modbus servers. Modbus Servers are read/written by a
    Modbus client represented by \l QModbusClient. QModbusServer communicates with a Modbus
    backend, providing users with a convenient API.
*/

/*!
    \enum QModbusServer::Option

    Each Modbus server has a set of values associated with it, each with its own option.

    The general purpose options (and the associated types) are:

    \value DiagnosticRegister       The diagnostic register of the server. \c quint16
    \value ExceptionStatusOffset    The exception status byte offset of the server. \c quint16

    User options:

    \value UserOption               The first option that can be used for user-specific purposes.

    For user options, it is up to the developer to decide which types to use and ensure that
    components use the correct types when accessing and setting values.
*/

/*!
    Constructs a Modbus server with the specified \a parent.
*/
QModbusServer::QModbusServer(QObject *parent) :
    QModbusDevice(*new QModbusServerPrivate, parent)
{
}

/*!
    \internal
*/
QModbusServer::~QModbusServer()
{
}

/*!
    \internal
*/
QModbusServer::QModbusServer(QModbusServerPrivate &dd, QObject *parent) :
    QModbusDevice(dd, parent)
{
}

/*!
    Sets the registered map structure for requests from other ModBus clients to \a map.
    The register values are initialized with zero. Returns \c true on success; otherwise \c false.

    If this function is not called before connecting, a default register with zero
    entries is setup.

    \note Calling this function discards any register value that was previously set.
*/
bool QModbusServer::setMap(const QModbusDataUnitMap &map)
{
    return d_func()->setMap(map);
}

/*!
    Sets the slave address for this Modbus server instance to \a slaveAddress.

    \sa slaveAddress()
*/
void QModbusServer::setSlaveAddress(int slaveAddress)
{
    Q_D(QModbusServer);
    d->m_slaveAddress = slaveAddress;
}

/*!
    Returns the slave address of this Mobus server instance.

    \sa setSlaveAddress()
*/
int QModbusServer::slaveAddress() const
{
    Q_D(const QModbusServer);

    return d->m_slaveAddress;
}

/*!
    Returns the value for \a option or an invalid \c QVariant if the option is
    not set.

    \table
        \header
            \li Option
            \li Description
        \row
            \li \l QModbusServer::DiagnosticRegister
            \li Returns the diagnostic register value of the server. The
                diagnostic register contains device specific contents where
                each bit has a specific meaning.
        \row
            \li \l QModbusServer::ExceptionStatusOffset
            \li Returns the offset address of the exception status byte
                location in the coils register.
        \row
            \li \l QModbusServer::UserOption
            \li Returns the value of a user option.

                \note For user options, it is up to the developer to decide
                which types to use and ensure that components use the correct
                types when accessing and setting values.
    \endtable
*/
QVariant QModbusServer::value(int option) const
{
    Q_D(const QModbusServer);

    switch (option) {
        case DiagnosticRegister:
            return d->m_diagnosticRegister;
        case ExceptionStatusOffset:
            return d->m_exceptionStatusOffset;
        default:
            break;
    };

    if (option < UserOption)
        return QVariant();
    return d->m_userOptions.value(option, QVariant());
}

/*!
    Sets the \a newValue for \a option and returns \c true on success; \c false
    otherwise.

    \table
        \header
            \li Key
            \li Description
        \row
            \li \l QModbusServer::DiagnosticRegister
            \li Sets the diagnostic register of the server in a device specific
                encoding to \a newValue. The default value preset is \c 0x0000.
                The bit values of the register need device specific documentation.
        \row
            \li \l QModbusServer::ExceptionStatusOffset
            \li Sets the exception status byte offset of the server to
                \a newValue which is the absolute offset address in the coils
                (0x register). Modbus register table starting with \c 0x0000h.
                The default value preset is \c 0x0000, using the exception
                status coils similar to Modicon 984 CPUs (coils 1-8).

                The function returns \c true if the coils register contains the
                8 bits required for storing and retrieving the status coils,
                otherwise \c false.
        \row
            \li \l QModbusServer::UserOption
            \li Sets the value of a user option to \a newValue.

                \note For user options, it is up to the developer to decide
                which types to use and ensure that components use the correct
                types when accessing and setting values.
    \endtable
*/
bool QModbusServer::setValue(int option, const QVariant &newValue)
{
    Q_D(QModbusServer);

    switch (option) {
    case DiagnosticRegister:
        if (!newValue.canConvert<quint16>())
            return false;
        d->m_diagnosticRegister = newValue.value<quint16>();
        return true;

    case ExceptionStatusOffset: {
        if (!newValue.canConvert<quint16>())
            return false;

        const quint16 tmp = newValue.value<quint16>();
        QModbusDataUnit coils(QModbusDataUnit::Coils, tmp, 8);
        if (!data(&coils))
            return false;
        d->m_exceptionStatusOffset = tmp;
        return true;
    }
    default:
        break;
    };

    if (option < UserOption)
        return false;
    d_func()->m_userOptions.insert(option, newValue);
    return true;
}

/*!
    \fn bool QModbusServer::processesBroadcast() const

    Subclasses should implement this function if the transport layer shall handle broadcasts.
    The implementation then should return \c true if the currently processed request is a
    broadcast request; otherwise \c false. The default implementation returns always \c false.

    \note The return value of this function only makes sense from within processRequest() or
    processPrivateModbusRequest(), otherwise it can only tell that the last request processed
    was a broadcast request.
*/

/*!
    Reads data stored in the Modbus server. A Modbus server has four tables (\a table) and each
    have a unique \a address field, which is used to read \a data from the desired field.
    See QModbusDataUnit::RegisterType for more information about the different tables.
    Returns \c false if address is outside of the map range or the register type is not even defined.

    \sa QModbusDataUnit::RegisterType, setData()
*/
bool QModbusServer::data(QModbusDataUnit::RegisterType table, quint16 address, quint16 *data) const
{
    QModbusDataUnit unit(table, address, 1u);
    if (data && readData(&unit)) {
        *data = unit.value(0);
        return true;
    }
    return false;
}

/*!
    Returns the values in the register range given by \a newData.

    \a newData must provide a valid register type, start address
    and valueCount. The returned \a newData will contain the register values
    associated with the given range.

    If \a newData contains a valid register type but a negative start address
    the entire register map is returned and \a newData appropriately sized.
*/
bool QModbusServer::data(QModbusDataUnit *newData) const
{
    return readData(newData);
}

/*!
    Writes data to the Modbus server. A Modbus server has four tables (\a table) and each have a
    unique \a address field, which is used to write \a data to the desired field.
    Returns \c false if address outside of the map range.

    If the call was successful the \l dataWritten() signal is emitted. Note that
    the signal is not emitted when \a data has not changed. Nevertheless this function
    returns \c true in such cases.

    \sa QModbusDataUnit::RegisterType, data(), dataWritten()
*/
bool QModbusServer::setData(QModbusDataUnit::RegisterType table, quint16 address, quint16 data)
{
    return writeData(QModbusDataUnit(table, address, QVector<quint16>() << data));
}

/*!
    Writes \a newData to the Modbus server map.
    Returns \c false if the \a newData range is outside of the map range.

    If the call was successful the \l dataWritten() signal is emitted. Note that
    the signal is not emitted when the addressed register has not changed. This
    may happen when \a newData contains exactly the same values as the
    register already. Nevertheless this function returns \c true in such cases.

    \sa data()
*/
bool QModbusServer::setData(const QModbusDataUnit &newData)
{
    return writeData(newData);
}

/*!
    Writes \a newData to the Modbus server map. Returns \c true on success,
    or \c false if the \a newData range is outside of the map range or the
    registerType() does not exist.

    \note Sub-classes that implement writing to a different backing store
    then default one, also need to implement setMap() and readData(). The
    dataWritten() signal needs to be emitted from within the functions
    implementation as well.

    \sa setMap(), readData(), dataWritten()
*/
bool QModbusServer::writeData(const QModbusDataUnit &newData)
{
    Q_D(QModbusServer);
    if (!d->m_modbusDataUnitMap.contains(newData.registerType()))
        return false;

    QModbusDataUnit &current = d->m_modbusDataUnitMap[newData.registerType()];
    if (!current.isValid())
        return false;

    // check range start is within internal map range
    int internalRangeEndAddress = current.startAddress() + current.valueCount() - 1;
    if (newData.startAddress() < current.startAddress()
        || newData.startAddress() > internalRangeEndAddress) {
        return false;
    }

    // check range end is within internal map range
    int rangeEndAddress = newData.startAddress() + newData.valueCount() - 1;
    if (rangeEndAddress < current.startAddress() || rangeEndAddress > internalRangeEndAddress)
        return false;

    bool changeRequired = false;
    for (int i = newData.startAddress(); i <= rangeEndAddress; i++) {
        quint16 newValue = newData.value(i - newData.startAddress());
        changeRequired |= (current.value(i) != newValue);
        current.setValue(i, newValue);
    }

    if (changeRequired)
        emit dataWritten(newData.registerType(), newData.startAddress(), newData.valueCount());
    return true;
}

/*!
    Reads the values in the register range given by \a newData and writes the
    data back to \a newData. Returns \c true on success or \c false if
    \a newData is \c 0, the \a newData range is outside of the map range or the
    registerType() does not exist.

    \note Sub-classes that implement reading from a different backing store
    then default one, also need to implement setMap() and writeData().

    \sa setMap(), writeData()
*/
bool QModbusServer::readData(QModbusDataUnit *newData) const
{
    Q_D(const QModbusServer);

    if ((!newData) || (!d->m_modbusDataUnitMap.contains(newData->registerType())))
        return false;

    const QModbusDataUnit &current = d->m_modbusDataUnitMap.value(newData->registerType());
    if (!current.isValid())
        return false;

     // return entire map for given type
    if (newData->startAddress() < 0) {
        *newData = current;
        return true;
    }

    // check range start is within internal map range
    int internalRangeEndAddress = current.startAddress() + current.valueCount() - 1;
    if (newData->startAddress() < current.startAddress()
        || newData->startAddress() > internalRangeEndAddress) {
        return false;
    }

    // check range end is within internal map range
    const int rangeEndAddress = newData->startAddress() + newData->valueCount() - 1;
    if (rangeEndAddress < current.startAddress() || rangeEndAddress > internalRangeEndAddress)
        return false;

    newData->setValues(current.values().mid(newData->startAddress(), newData->valueCount()));
    return true;
}

/*!
    \fn void QModbusServer::dataWritten(QModbusDataUnit::RegisterType register, int address, int size)

    This signal is emitted when a Modbus client has written one or more fields of data to the
    Modbus server. The signal contains information about the fields that were written:
    \list
        \li \a register type that was written,
        \li \a address of the first field that was written,
        \li and \a size of consecutive fields that were written starting from \a address.
    \endlist

    The signal is not emitted when the to-be-written fields have not changed
    due to no change in value.
*/

/*!
    Processes a Modbus client \a request and returns a Modbus response.
    This function returns a \l QModbusResponse or \l QModbusExceptionResponse depending
    on the nature of the request.

    The default implementation of this function handles all standard Modbus
    function codes as defined by the Modbus Application Protocol Specification 1.1b.
    All other Modbus function codes not included in the specification are forwarded to
    \l processPrivateModbusRequest().

    The default handling of the standard Modbus function code requests can be overwritten
    by reimplementing this function. The override must handle the request type
    in question and return the appropriate \l QModbusResponse. A common reason might be to
    filter out function code requests for data values to limit read/write access and
    function codes not desired in particular implementations such as serial line diagnostics
    on ethernet or Modbus Plus transport layers. Every other request type should be
    forwarded to this default implementation.

    \note This function should not be overridden to provide a custom implementation for
    non-standard Modbus request types.

    \sa processPrivateModbusRequest()
*/
QModbusResponse QModbusServer::processRequest(const QModbusPdu &request)
{
    return d_func()->processRequest(request);
}

/*!
    This function should be implemented by custom Modbus servers. It is
    called by \l processRequest() if the given \a request is not a standard
    Modbus request.

    Overwriting this function allows handling of additional function codes and
    subfunction-codes not specified in the Modbus Application Protocol
    Specification 1.1b. Reimplementations should call this function again to
    ensure an exception response is returned for all unknown function codes the
    custom Modbus implementation does not handle.

    This default implementation returns a \c QModbusExceptionResponse with the
    \a request function code and error code set to illegal function.

    \sa processRequest()
*/
QModbusResponse QModbusServer::processPrivateModbusRequest(const QModbusPdu &request)
{
    return QModbusExceptionResponse(request.functionCode(),
        QModbusExceptionResponse::IllegalFunction);
}

// -- QModbusServerPrivate

bool QModbusServerPrivate::setMap(const QModbusDataUnitMap &map)
{
    m_modbusDataUnitMap = map;
    return true;
}

/*
    TODO: implement
*/
QModbusResponse QModbusServerPrivate::processRequest(const QModbusPdu &request)
{
    switch (request.functionCode()) {
    case QModbusRequest::ReadCoils:
        return processReadCoilsRequest(request);
    case QModbusRequest::ReadDiscreteInputs:
        return processReadDiscreteInputsRequest(request);
    case QModbusRequest::ReadHoldingRegisters:
        return processReadHoldingRegistersRequest(request);
    case QModbusRequest::ReadInputRegisters:
        return processReadInputRegistersRequest(request);
    case QModbusRequest::WriteSingleCoil:
        return processWriteSingleCoilRequest(request);
    case QModbusRequest::WriteSingleRegister:
        return processWriteSingleRegisterRequest(request);
    case QModbusRequest::ReadExceptionStatus:
        return processReadExceptionStatus(request);
    case QModbusRequest::Diagnostics:
        return processDiagnostics(request);
    case QModbusRequest::GetCommEventCounter:
        return processGetCommEventCounter(request);
    case QModbusRequest::GetCommEventLog:
        return processGetCommEventLog(request);
    case QModbusRequest::WriteMultipleCoils:
        return processWriteMultipleCoilsRequest(request);
    case QModbusRequest::WriteMultipleRegisters:
        return processWriteMultipleRegistersRequest(request);
    case QModbusRequest::ReportServerId:
    case QModbusRequest::ReadFileRecord:
    case QModbusRequest::WriteFileRecord:
        return q_func()->processPrivateModbusRequest(request);
    case QModbusRequest::MaskWriteRegister:
        return processMaskWriteRegister(request);
    case QModbusRequest::ReadWriteMultipleRegisters:
        return processReadWriteMultipleRegistersRequest(request);
    case QModbusRequest::ReadFifoQueue:
        return processReadFifoQueue(request);
    case QModbusRequest::EncapsulatedInterfaceTransport:
    default:
        break;
    }
    return q_func()->processPrivateModbusRequest(request);
}

#define CHECK_SIZE_EQUALS(req) \
    do { \
        if (req.dataSize() != QModbusRequest::minimumDataSize(req.functionCode())) { \
            qCDebug(QT_MODBUS) << "The request's data size does not equal the expected size."; \
            return QModbusExceptionResponse(req.functionCode(), \
                                            QModbusExceptionResponse::IllegalDataValue); \
        } \
    } while (0)

#define CHECK_SIZE_LESS_THAN(req) \
    do { \
        if (req.dataSize() < QModbusRequest::minimumDataSize(req.functionCode())) { \
            qCDebug(QT_MODBUS) << "The request's data size is less than the expected size."; \
            return QModbusExceptionResponse(req.functionCode(), \
                                            QModbusExceptionResponse::IllegalDataValue); \
        } \
    } while (0)

QModbusResponse QModbusServerPrivate::processReadCoilsRequest(const QModbusRequest &request)
{
    return readBits(request, QModbusDataUnit::Coils);
}

QModbusResponse QModbusServerPrivate::processReadDiscreteInputsRequest(const QModbusRequest &rqst)
{
    return readBits(rqst, QModbusDataUnit::DiscreteInputs);
}

QModbusResponse QModbusServerPrivate::readBits(const QModbusPdu &request,
                                               QModbusDataUnit::RegisterType unitType)
{
    CHECK_SIZE_EQUALS(request);
    quint16 address, count;
    request.decodeData(&address, &count);

    if ((count < 0x0001) || (count > 0x07D0)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataValue);
    }

    // Get the requested range out of the registers.
    QModbusDataUnit unit(unitType, address, count);
    if (!q_func()->data(&unit)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataAddress);
    }

    quint8 byteCount = count / 8;
    if ((count % 8) != 0) {
        byteCount += 1;
        // If the range is not a multiple of 8, resize.
        unit.setValueCount(byteCount * 8);
    }

    address = 0; // The data range now starts with zero.
    QVector<quint8> bytes;
    for (int i = 0; i < byteCount; ++i) {
        std::bitset<8> byte;
        // According to the spec: If the returned quantity is not a multiple of eight,
        // the remaining bits in the final data byte will be padded with zeros.
        for (int currentBit = 0; currentBit < 8; ++currentBit)
            byte[currentBit] = unit.value(address++); // The padding happens inside value().
        bytes.append(static_cast<quint8> (byte.to_ulong()));
    }

    return QModbusResponse(request.functionCode(), byteCount, bytes);
}

QModbusResponse QModbusServerPrivate::processReadHoldingRegistersRequest(const QModbusRequest &rqst)
{
    return readBytes(rqst, QModbusDataUnit::HoldingRegisters);
}

QModbusResponse QModbusServerPrivate::processReadInputRegistersRequest(const QModbusRequest &rqst)
{
    return readBytes(rqst, QModbusDataUnit::InputRegisters);
}

QModbusResponse QModbusServerPrivate::readBytes(const QModbusPdu &request,
                                                QModbusDataUnit::RegisterType unitType)
{
    CHECK_SIZE_EQUALS(request);
    quint16 address, count;
    request.decodeData(&address, &count);

    if ((count < 0x0001) || (count > 0x007D)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataValue);
    }

    // Get the requested range out of the registers.
    QModbusDataUnit unit(unitType, address, count);
    if (!q_func()->data(&unit)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataAddress);
    }

    return QModbusResponse(request.functionCode(), quint8(count * 2), unit.values());
}

QModbusResponse QModbusServerPrivate::processWriteSingleCoilRequest(const QModbusRequest &request)
{
    return writeSingle(request, QModbusDataUnit::Coils);
}

QModbusResponse QModbusServerPrivate::processWriteSingleRegisterRequest(const QModbusRequest &rqst)
{
    return writeSingle(rqst, QModbusDataUnit::HoldingRegisters);
}

QModbusResponse QModbusServerPrivate::writeSingle(const QModbusPdu &request,
                                                  QModbusDataUnit::RegisterType unitType)
{
    CHECK_SIZE_EQUALS(request);
    quint16 address, value;
    request.decodeData(&address, &value);

    if ((unitType == QModbusDataUnit::Coils) && ((value != Coil::Off) && (value != Coil::On))) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataValue);
    }

    quint16 reg;   // Get the requested register, but deliberately ignore.
    if (!q_func()->data(unitType, address, &reg)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataAddress);
    }

    if (!q_func()->setData(unitType, address, value)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::ServerDeviceFailure);
    }

    return QModbusResponse(request.functionCode(), address, value);
}

QModbusResponse QModbusServerPrivate::processReadExceptionStatus(const QModbusRequest &request)
{
    CHECK_SIZE_EQUALS(request);

    // Get the requested range out of the registers.
    const QVariant tmp = q_func()->value(QModbusServer::ExceptionStatusOffset);
    if (tmp.isNull() || (!tmp.isValid())) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::ServerDeviceFailure);
    }
    const quint16 exceptionStatusOffset = tmp.value<quint16>();
    QModbusDataUnit coils(QModbusDataUnit::Coils, exceptionStatusOffset, 8);
    if (!q_func()->data(&coils)) {
        return QModbusExceptionResponse(request.functionCode(),
                                        QModbusExceptionResponse::IllegalDataAddress);
    }

    quint16 address = 0;
    QVector<quint8> bytes;
    std::bitset<8> byte;
    for (int currentBit = 0; currentBit < 8; ++currentBit)
        byte[currentBit] = coils.value(address++); // The padding happens inside value().
    bytes.append(static_cast<quint8> (byte.to_ulong()));

    return QModbusResponse(request.functionCode(), bytes);
}

QModbusResponse QModbusServerPrivate::processDiagnostics(const QModbusRequest &request)
{
#define CHECK_SIZE_AND_CONDITION(req, condition) \
    CHECK_SIZE_EQUALS(req); \
    do { \
        if ((condition)) { \
            return QModbusExceptionResponse(req.functionCode(), \
                                            QModbusExceptionResponse::IllegalDataValue); \
        } \
    } while (0)

    quint16 subFunctionCode, data = 0xffff;
    request.decodeData(&subFunctionCode, &data);

    switch (subFunctionCode) {
    case Diagnostics::ReturnQueryData:
        return QModbusResponse(request.functionCode(), request.data());

    case Diagnostics::RestartCommunicationsOption: {
        CHECK_SIZE_AND_CONDITION(request, ((data != 0xff00) && (data != 0x0000)));
        if (!restartCommunicationsOption(data == 0xff00)) {
            qCWarning(QT_MODBUS) << "Cannot restart server communication";
            return QModbusExceptionResponse(request.functionCode(),
                                            QModbusExceptionResponse::ServerDeviceFailure);
        }
        return QModbusResponse(request.functionCode(), request.data());
    }   break;

    case Diagnostics::ChangeAsciiInputDelimiter: {
        const QByteArray data = request.data().mid(2, 2);
        CHECK_SIZE_AND_CONDITION(request, (data[1] != 0x00));
        // TODO: This changes the variable m_asciiInputDelimiter only for now.
        // Cite PI-MBUS-300.pdf: The character 'CHAR' passed in the query data field becomes the
        // end of message delimiter for future messages (replacing the default LF character). This
        // function is useful in cases where a Line Feed is not wanted at the end of ASCII messages.
        m_asciiInputDelimiter = data[0];
        return QModbusResponse(request.functionCode(), request.data());
    }   break;

    case Diagnostics::ForceListenOnlyMode:
        CHECK_SIZE_AND_CONDITION(request, (data != 0x0000));
        m_forceListenOnlyMode = true;
        storeModbusCommEvent(QModbusCommEvent::EnteredListenOnlyMode);
        return QModbusResponse();

    case Diagnostics::ClearCountersAndDiagnosticRegister:
        CHECK_SIZE_AND_CONDITION(request, (data != 0x0000));
        // TODO: According to PI_MBUS_300 specification, the clearing of the diagnostic
        // register is dependent on the device model. For legacy support to fulfill this
        // requirement, a server configuration variable could be added to check if the
        // diagnostic register should be cleared or not.
        resetCommunicationCounters();
        q_func()->setValue(QModbusServer::DiagnosticRegister, 0x0000);
        return QModbusResponse(request.functionCode(), request.data());

    case Diagnostics::ReturnDiagnosticRegister:
    case Diagnostics::ReturnBusMessageCount:
    case Diagnostics::ReturnBusCommunicationErrorCount:
    case Diagnostics::ReturnBusExceptionErrorCount:
    case Diagnostics::ReturnServerMessageCount:
    case Diagnostics::ReturnServerNoResponseCount:
    case Diagnostics::ReturnServerNAKCount:
    case Diagnostics::ReturnServerBusyCount:
    case Diagnostics::ReturnBusCharacterOverrunCount:
        CHECK_SIZE_AND_CONDITION(request, (data != 0x0000));
        return QModbusResponse(request.functionCode(), subFunctionCode,
                               m_counters[static_cast<Counter> (subFunctionCode)]);
    }
    return q_func()->processPrivateModbusRequest(request);

#undef CHECK_SIZE_AND_CONDITION
}

QModbusResponse QModbusServerPrivate::processGetCommEventCounter(const QModbusRequest &request)
{
    CHECK_SIZE_EQUALS(request);
    return QModbusResponse(request.functionCode(), m_deviceBusy, m_counters[Counter::CommEvent]);
}

QModbusResponse QModbusServerPrivate::processGetCommEventLog(const QModbusRequest &request)
{
    CHECK_SIZE_EQUALS(request);
    m_commEventLog.normalizeIndexes();
    QVector<quint16> eventLog(m_commEventLog.count());
    for (int i = 0; i < m_commEventLog.count(); ++i)
        eventLog[i] = m_commEventLog[i];

    // 6 -> 3 x 2 Bytes (Status, Event Count and Message Count)
    return QModbusResponse(request.functionCode(), quint8(m_commEventLog.count() + 6),
                           m_deviceBusy, m_counters[Counter::CommEvent],
                           m_counters[Counter::BusMessage], eventLog);
}

QModbusResponse QModbusServerPrivate::processWriteMultipleCoilsRequest(const QModbusRequest &request)
{
    CHECK_SIZE_LESS_THAN(request);
    quint16 address, numberOfCoils;
    quint8 byteCount;
    request.decodeData(&address, &numberOfCoils, &byteCount);

    // byte count does not match number of data bytes following
    if (byteCount != (request.dataSize() - 5 )) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataValue);
    }

    quint8 expectedBytes = numberOfCoils / 8;
    if ((numberOfCoils % 8) != 0)
        expectedBytes += 1;

    if ((numberOfCoils < 0x0001) || (numberOfCoils > 0x07B0) || (expectedBytes != byteCount)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataValue);
    }

    // Get the requested range out of the registers.
    QModbusDataUnit coils(QModbusDataUnit::Coils, address, numberOfCoils);
    if (!q_func()->data(&coils)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataAddress);
    }

    QVector<std::bitset<8>> bytes;
    const QByteArray payload = request.data().mid(5);
    for (qint32 i = payload.size() - 1; i >= 0; --i)
        bytes.append(quint8(payload[i]));

    // Since we picked the coils at start address, data
    // range is numberOfCoils and therefore index too.
    quint16 coil = numberOfCoils;
    qint32 currentBit = 8 - ((byteCount * 8) - numberOfCoils);
    foreach (const auto &currentByte, bytes) {
        for (currentBit -= 1; currentBit >= 0; --currentBit)
            coils.setValue(--coil, currentByte[currentBit]);
        currentBit = 8;
    }

    if (!q_func()->setData(coils)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::ServerDeviceFailure);
    }

    return QModbusResponse(request.functionCode(), address, numberOfCoils);
}

QModbusResponse QModbusServerPrivate::processWriteMultipleRegistersRequest(
    const QModbusRequest &request)
{
    CHECK_SIZE_LESS_THAN(request);
    quint16 address, numberOfRegisters;
    quint8 byteCount;
    request.decodeData(&address, &numberOfRegisters, &byteCount);

    // byte count does not match number of data bytes following or register count
    if ((byteCount != (request.dataSize() - 5 )) || (byteCount != (numberOfRegisters * 2))) {
        return QModbusExceptionResponse(request.functionCode(),
                                        QModbusExceptionResponse::IllegalDataValue);
    }

    if ((numberOfRegisters < 0x0001) || (numberOfRegisters > 0x007B)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataValue);
    }

    // Get the requested range out of the registers.
    QModbusDataUnit registers(QModbusDataUnit::HoldingRegisters, address, numberOfRegisters);
    if (!q_func()->data(&registers)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataAddress);
    }

    const QByteArray pduData = request.data().remove(0,5);
    QDataStream stream(pduData);

    QVector<quint16> values;
    quint16 tmp;
    for (int i = 0; i < numberOfRegisters; i++) {
        stream >> tmp;
        values.append(tmp);
    }

    registers.setValues(values);

    if (!q_func()->setData(registers)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::ServerDeviceFailure);
    }

    return QModbusResponse(request.functionCode(), address, numberOfRegisters);
}

QModbusResponse QModbusServerPrivate::processMaskWriteRegister(const QModbusRequest &request)
{
    CHECK_SIZE_EQUALS(request);
    quint16 address, andMask, orMask;
    request.decodeData(&address, &andMask, &orMask);

    quint16 reg;
    if (!q_func()->data(QModbusDataUnit::HoldingRegisters, address, &reg)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataAddress);
    }

    const quint16 result = (reg & andMask) | (orMask & (~ andMask));
    if (!q_func()->setData(QModbusDataUnit::HoldingRegisters, address, result)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::ServerDeviceFailure);
    }
    return QModbusResponse(request.functionCode(), request.data());
}

QModbusResponse QModbusServerPrivate::processReadWriteMultipleRegistersRequest(
    const QModbusRequest &request)
{
    CHECK_SIZE_LESS_THAN(request);
    quint16 readStartAddress, readQuantity, writeStartAddress, writeQuantity;
    quint8 byteCount;
    request.decodeData(&readStartAddress, &readQuantity,
                       &writeStartAddress, &writeQuantity, &byteCount);

    // byte count does not match number of data bytes following or register count
    if ((byteCount != (request.dataSize() - 9 )) || (byteCount != (writeQuantity * 2))) {
        return QModbusExceptionResponse(request.functionCode(),
                                        QModbusExceptionResponse::IllegalDataValue);
    }

    if ((readQuantity < 0x0001) || (readQuantity > 0x007B)
            || (writeQuantity < 0x0001) || (writeQuantity > 0x0079)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataValue);
    }

    // According to spec, write operation is executed before the read operation
    // Get the requested range out of the registers.
    QModbusDataUnit writeRegisters(QModbusDataUnit::HoldingRegisters, writeStartAddress,
                                   writeQuantity);
    if (!q_func()->data(&writeRegisters)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataAddress);
    }

    const QByteArray pduData = request.data().remove(0,9);
    QDataStream stream(pduData);

    QVector<quint16> values;
    quint16 tmp;
    for (int i = 0; i < writeQuantity; i++) {
        stream >> tmp;
        values.append(tmp);
    }

    writeRegisters.setValues(values);

    if (!q_func()->setData(writeRegisters)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::ServerDeviceFailure);
    }

    // Get the requested range out of the registers.
    QModbusDataUnit readRegisters(QModbusDataUnit::HoldingRegisters, readStartAddress,
                                  readQuantity);
    if (!q_func()->data(&readRegisters)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataAddress);
    }

    return QModbusResponse(request.functionCode(), quint8(readQuantity * 2),
                           readRegisters.values());
}

QModbusResponse QModbusServerPrivate::processReadFifoQueue(const QModbusRequest &request)
{
    CHECK_SIZE_LESS_THAN(request);
    quint16 address;
    request.decodeData(&address);

    quint16 fifoCount;
    if (!q_func()->data(QModbusDataUnit::HoldingRegisters, address, &fifoCount)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataAddress);
    }

    if (fifoCount > 31u) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataValue);
    }

    QModbusDataUnit fifoRegisters(QModbusDataUnit::HoldingRegisters, address + 1u, fifoCount);
    if (!q_func()->data(&fifoRegisters)) {
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalDataAddress);
    }

    return QModbusResponse(request.functionCode(), quint16((fifoCount * 2) + 2u), fifoCount,
                           fifoRegisters.values());
}

/*!
    \internal

    Access function of the device for Modbus Diagnostic Function Code 0x08, Subcode 01
    (0x08, 0x0001 hex)

    Restarts the communication by closing the connection and re-opening. After closing,
    all communication event counters are cleared and the listen only mode set to false.
    If \a clearEventLog is set to true, the event log history gets cleared also.

    \sa forceListenOnlyMode(), getCommEventLog()
*/
bool QModbusServerPrivate::restartCommunicationsOption(bool clearLog)
{
    q_func()->disconnectDevice();
    if (clearLog)
        m_commEventLog.clear();

    resetCommunicationCounters();
    m_forceListenOnlyMode = false;
    storeModbusCommEvent(QModbusCommEvent::InitiatedCommunicationRestart);

    return q_func()->connectDevice();
}

/*!
    \internal

    Stores an event byte into the Modbus event log history table (0-64 bytes) at
    the first position (byte 0) and pushes all other events back. The communication
    event counter is increased for each event stored in the event log.

    A communication event is encoded into one byte with the four types:
    \list
     \li Remote Device Modbus Receive Event
     \li Remote Device Modbus Send Event
     \li Remote Device Entered Listen Only Mode
     \li Remote Device Initiated Communication Restart
    \endlist

    \sa getCommEventLog(), restartCommunicationsOption()
*/
void QModbusServerPrivate::storeModbusCommEvent(const QModbusCommEvent &eventByte)
{
    m_commEventLog.prepend(eventByte);
}

#undef CHECK_SIZE_EQUALS
#undef CHECK_SIZE_LESS_THAN

QT_END_NAMESPACE
