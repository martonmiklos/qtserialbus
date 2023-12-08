TARGET = QtSerialBus

QT = core-private
QT_FOR_PRIVATE = network

CONFIG += c++11

QMAKE_DOCS = $$PWD/doc/qtserialbus.qdocconf

PUBLIC_HEADERS += \
    qcanbusdevice.h \
    qcanbusdeviceinfo.h \
    qcanbusfactory.h \
    qcanbusframe.h \
    qcanbus.h \
    qtserialbusglobal.h \
    qmodbusserver.h \
    qmodbusdevice.h \
    qmodbusdataunit.h \
    qmodbusclient.h \
    qmodbusreply.h \
    qmodbustcpclient.h \
    qmodbustcprtuclient.h \
    qmodbustcpserver.h \
    qmodbuspdu.h \
    qmodbusdeviceidentification.h

PRIVATE_HEADERS += \
    qcanbusdevice_p.h \
    qcanbusdeviceinfo_p.h \
    qmodbusserver_p.h \
    qmodbusclient_p.h \
    qmodbusdevice_p.h \
    qmodbustcpclient_p.h \
    qmodbustcprtuclient_p.h \
    qmodbustcpserver_p.h \
    qmodbus_symbols_p.h \
    qmodbuscommevent_p.h \
    qmodbusadu_p.h \

PUBLIC_SOURCES += \
    qcanbusdevice.cpp \
    qcanbusdeviceinfo.cpp \
    qcanbus.cpp \
    qcanbusfactory.cpp \
    qcanbusframe.cpp \
    qmodbusserver.cpp \
    qmodbusdevice.cpp \
    qmodbusdataunit.cpp \
    qmodbusclient.cpp \
    qmodbusreply.cpp \
    qmodbustcpclient.cpp \
    qmodbustcprtuclient.cpp \
    qmodbustcpserver.cpp \
    qmodbuspdu.cpp \
    qmodbusdeviceidentification.cpp

PRIVATE_SOURCES += qmodbustcpclient_p.cpp

qtConfig(modbus-serialport) {
    QT_FOR_PRIVATE += serialport

    PUBLIC_HEADERS += \
        qmodbusrtuserialmaster.h \
        qmodbusrtuserialslave.h

    PRIVATE_HEADERS += \
        qmodbusrtuserialmaster_p.h \
        qmodbusrtuserialslave_p.h

    SOURCES += \
        qmodbusrtuserialmaster.cpp \
        qmodbusrtuserialslave.cpp
}
HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS \
    qmodbusrtumaster.h \
    qmodbusrtumaster_p.h \
    qmodbusrtutcpmaster.h \
    qmodbusrtutcpmaster_p.h
SOURCES += $$PUBLIC_SOURCES $$PRIVATE_SOURCES \
    qmodbusrtumaster.cpp \
    qmodbusrtutcpmaster.cpp

MODULE_PLUGIN_TYPES = \
    canbus
load(qt_module)
