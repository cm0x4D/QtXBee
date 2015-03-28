#include "global.h"
#include "xbee.h"

#include "digimeshframe.h"

#include "atcommandframe.h"
#include "atcommandqueueparamframe.h"
#include "transmitrequestframe.h"
#include "explicitadressingcommandframe.h"
#include "remoteatcommandrequestframe.h"

#include "atcommandresponseframe.h"
#include "modemstatusframe.h"
#include "transmitstatusframe.h"
#include "receivepacketframe.h"
#include "explicitrxindicatorframe.h"
#include "nodeidentificationindicatorframe.h"
#include "remoteatcommandresponseframe.h"

#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>

/**
 * @brief XBee's default constructor.
 *
 * Allocates and initializes all parameters to there default values.
 * @note At this stage, no serial communication is initialized, so you can't communicate with your physical XBee until the serial port is initialized.
 * @param parent parent object
 * @sa XBee::XBee(const QString &serialPort, QObject *parent)
 * @sa XBee::setSerialPort()
 */
XBee::XBee(QObject *parent) :
    QObject(parent),
    m_serial(NULL),
    xbeeFound(false),
    m_mode(TransparentMode),
    m_frameIdCounter(1),
    m_dh(0),
    m_dl(0),
    m_my(0),
    m_mp(0),
    m_nc(0),
    m_sh(0),
    m_sl(0),
    m_ni(QString()),
    m_se(0),
    m_de(0),
    m_ci(0),
    m_to(0),
    m_np(0),
    m_dd(0),
    m_cr(0)
{
}

/**
 * @brief XBee's constructor
 *
 * Allocates and initializes all parameters to there default values and try to open the serial port specified in argument.
 * @param serialPort serial port path (eg. /dev/ttyUSB0) on with the XBee is plugged.
 * @param parent parent object
 * @note In this case, the serial port configuration will ba set to :
 * - <b>Baud Rate</b> : 9600
 * - <b>Data Bits</b> : 8 bits
 * - <b>Parity</b> : No
 * - <b>Stop Bits</b> : One stop bit
 * - <b>Flow Control</b> : No flow control
 */
XBee::XBee(const QString &serialPort, QObject *parent) :
    QObject(parent),
    m_serial(NULL),
    xbeeFound(false),
    m_mode(TransparentMode),
    m_frameIdCounter(1),
    m_dh(0),
    m_dl(0),
    m_my(0),
    m_mp(0),
    m_nc(0),
    m_sh(0),
    m_sl(0),
    m_ni(QString()),
    m_se(0),
    m_de(0),
    m_ci(0),
    m_to(0),
    m_np(0),
    m_dd(0),
    m_cr(0)
{
    m_serial = new QSerialPort(serialPort, this);
    connect(m_serial, SIGNAL(readyRead()), SLOT(readData()));
    applyDefaultSerialPortConfig();
    initSerialConnection();
    //startupCheck();
}

/**
 * @brief Destroyes the XBee instance, frees allocated resources and close the serial port.
 */
XBee::~XBee()
{
    if(m_serial && m_serial->isOpen())
    {
        m_serial->close();
        qDebug() << "XBEE: Serial Port closed successfully";
    }
}

/**
 * @brief Opens the XBee' serial port
 * @return true if succeeded; false otherwise.
 * @sa XBee::close()
 * @sa XBee::setSerialPort()
 */
bool XBee::open()
{
    bool bRet = false;
    if(m_serial) {
        bRet = m_serial->open(QIODevice::ReadWrite);
    }
    return bRet;
}

/**
 * @brief Closes the XBee' serial port
 * @return true if succeeded; false otherwise
 */
bool XBee::close()
{
    if(m_serial) {
        m_serial->close();
    }
    return true;
}

/**
 * @brief Sets the XBee's serial port, which we be used to communicate with it.
 * @note Default serial port will be used :
 * <ul>
 * <li><b>Baud Rate</b> : 9600</li>
 * <li><b>Data Bits</b> : 8 bits</li>
 * <li><b>Parity</b> : No</li>
 * <li><b>Stop Bits</b> : One stop bit</li>
 * <li><b>Flow Control</b> : No flow control</li>
 * </ul>
 * @param serialPort path to the serial port
 * @return true if succeeded; false otherwise
 * @sa XBee::setSerialPort(const QString &serialPort, const QSerialPort::BaudRate baudRate, const QSerialPort::DataBits dataBits, const QSerialPort::Parity parity, const QSerialPort::StopBits stopBits, const QSerialPort::FlowControl flowControl)
 * @sa XBee::applyDefaultSerialPortConfig()
 */
bool XBee::setSerialPort(const QString &serialPort)
{
    bool bRet = false;
    if(m_serial) {
        m_serial->close();
        m_serial->setPortName(serialPort);
        m_serial->disconnect(this);
    }
    else {
        m_serial = new QSerialPort(serialPort, this);
        connect(m_serial, SIGNAL(readyRead()), SLOT(readData()));
    }
    if(applyDefaultSerialPortConfig()) {
        bRet = initSerialConnection();
    }
    //startupCheck();
    return bRet;
}

/**
 * @brief Configures the serial port used to communicate with the XBee.
 * @param serialPort path to the serial port
 * @param baudRate the baud rate
 * @param dataBits the data bits
 * @param parity the parity
 * @param stopBits the stop bits
 * @param flowControl the flow control
 * @return true if succeeded; false otherwise
 */
bool XBee::setSerialPort(const QString &serialPort, const QSerialPort::BaudRate baudRate, const QSerialPort::DataBits dataBits, const QSerialPort::Parity parity, const QSerialPort::StopBits stopBits, const QSerialPort::FlowControl flowControl)
{
    bool bRet = false;
    if(setSerialPort(serialPort)) {
        bRet = setSerialPortConfiguration(baudRate, dataBits, parity, stopBits, flowControl);
    }
    return bRet;
}

/**
 * @brief Configures the serial port used to communicate with the XBee.
 * @note This method must be called after have defining the serial port
 * @param baudRate the baud rate
 * @param dataBits the data bits
 * @param parity the parity
 * @param stopBits the stop bits
 * @param flowControl the flow control
 * @return true if succeeded; false otherwise.
 * @sa XBee::setSerialPort()
 */
bool XBee::setSerialPortConfiguration(const QSerialPort::BaudRate baudRate, const QSerialPort::DataBits dataBits, const QSerialPort::Parity parity, const QSerialPort::StopBits stopBits, const QSerialPort::FlowControl flowControl)
{
    if(m_serial == NULL)
        return false;

    return  m_serial->setBaudRate(baudRate) &&
            m_serial->setDataBits(dataBits) &&
            m_serial->setParity(parity) &&
            m_serial->setStopBits(stopBits) &&
            m_serial->setFlowControl(flowControl);
}

/**
 * @brief Applies the default configuration to the serial port.
 * <ul>
 * <li><b>Baud Rate</b> : 9600</li>
 * <li><b>Data Bits</b> : 8 bits</li>
 * <li><b>Parity</b> : No</li>
 * <li><b>Stop Bits</b> : One stop bit</li>
 * <li><b>Flow Control</b> : No flow control</li>
 * </ul>
 * @note You serial port must be set before calling this method
 * @return true if succeeded; false otherwise.
 * @sa XBee::setSerialPort()
 */
bool XBee::applyDefaultSerialPortConfig()
{
    if(m_serial == NULL)
        return false;

    return  m_serial->setBaudRate(QSerialPort::Baud9600) &&
            m_serial->setDataBits(QSerialPort::Data8) &&
            m_serial->setParity(QSerialPort::NoParity) &&
            m_serial->setStopBits(QSerialPort::OneStop) &&
            m_serial->setFlowControl(QSerialPort::NoFlowControl);
}

void XBee::displayATCommandResponse(ATCommandResponseFrame *digiMeshPacket){
    qDebug() << "*********************************************";
    qDebug() << "Received ATCommandResponse: ";
    qDebug() << qPrintable(digiMeshPacket->toString());
    qDebug() << "*********************************************";
}
void XBee::displayModemStatus(ModemStatusFrame *digiMeshPacket){
    qDebug() << "Received ModemStatus: " << digiMeshPacket->packet().toHex();
}
void XBee::displayTransmitStatus(TransmitStatusFrame *digiMeshPacket){
    qDebug() << "Received TransmitStatus: " << digiMeshPacket->packet().toHex();
}
void XBee::displayRXIndicator(ReceivePacketFrame *digiMeshPacket){
    qDebug() << "Received RXIndicator: " << digiMeshPacket->data().toHex();
}
void XBee::displayRXIndicatorExplicit(ExplicitRxIndicatorFrame *digiMeshPacket){
    qDebug() << "Received RXIndicatorExplicit: " << digiMeshPacket->packet().toHex();
}
void XBee::displayNodeIdentificationIndicator(NodeIdentificationIndicatorFrame *digiMeshPacket){
    qDebug() << "Received NodeIdentificationIndicator: " << digiMeshPacket->packet().toHex();
}
void XBee::displayRemoteCommandResponse(RemoteATCommandResponseFrame *digiMeshPacket){
    qDebug() << "Received RemoteCommandResponse: " << digiMeshPacket->packet().toHex();
}

void XBee::sendATCommandAsync(DigiMeshFrame *command)
{
    command->assemblePacket();
    if(xbeeFound && m_serial->isOpen())
    {
        command->setFrameId(m_frameIdCounter);
        if(m_frameIdCounter >= 255)
            m_frameIdCounter = 1;
        else m_frameIdCounter++;

        qDebug() << Q_FUNC_INFO << "Transmit: " << QString("0x").append(command->packet().toHex());
        m_serial->write(command->packet());
        m_serial->flush();
    }
    else
    {
        qDebug() << "XBEE: Cannot write to Serial Port" << m_serial->portName();
    }
}

void XBee::setATCommandAsync(const QByteArray &data)
{
    Q_UNUSED(data);
}

ATCommandResponseFrame * XBee::sendATCommandSync(DigiMeshFrame *command)
{
    Q_ASSERT(command);
    ATCommandResponseFrame * rep = NULL;
    QByteArray repPacket;

    command->setFrameId(m_frameIdCounter);
    if(m_frameIdCounter >= 255)
        m_frameIdCounter = 1;
    else m_frameIdCounter++;

    command->assemblePacket();

    m_serial->blockSignals(true);

    m_serial->write(command->packet());
    m_serial->flush();
    m_serial->waitForBytesWritten(100);
    m_serial->waitForReadyRead(1000);
    while(!m_serial->atEnd())
        repPacket.append(m_serial->readAll());

    m_serial->blockSignals(false);

    if(repPacket.size() > 0) {
        rep = new ATCommandResponseFrame();
        rep->setPacket(repPacket);
    }
    return rep;
}

ATCommandResponseFrame * XBee::sendATCommandSync(const QByteArray &data)
{
    ATCommandResponseFrame * rep = NULL;
    if(data.size() >= 2)
    {
        ATCommandFrame at;
        at.setCommand(data.mid(0, 2));
        if(data.size() > 2) {
            at.setParameter(data.mid(2, data.size()-2));
        }
        rep = sendATCommandSync(&at);
    }
    else {
        qWarning() << Q_FUNC_INFO << "bad command" << data;
    }
    return rep;
}

void XBee::broadcast(QString data)
{
    TransmitRequestFrame request;
    request.setData(data.toLatin1());
    sendATCommandAsync(&request);
}

void XBee::unicast(QByteArray address, QString data)
{
    TransmitRequestFrame request;
    request.setDestAddr64(address);
    request.setData(data.toLatin1());
    sendATCommandAsync(&request);
}

/**
 * @brief Loads the XBee's addressing properties
 * <ul>
 * <li>DH</li>
 * <li>DL</li>
 * <li>MY</li>
 * <li>MP</li>
 * <li>NC</li>
 * <li>SH</li>
 * <li>SL</li>
 * <li>NI</li>
 * <li>SE</li>
 * <li>DE</li>
 * <li>CI</li>
 * <li>TO</li>
 * <li>NP</li>
 * <li>DD</li>
 * <li>CR</li>
 * </ul>
 */
void XBee::loadAddressingProperties() {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_DH); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_DL); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_MY); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_MP); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_NC); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_SH); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_SL); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_NI); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_SE); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_DE); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_CI); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_TO); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_NP); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_DD); sendATCommandAsync(&at);
    at.setCommand(ATCommandFrame::Command_CR); sendATCommandAsync(&at);
}

// Adressing
bool XBee::setDH(const quint32 dh) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_DH);
    at.setParameter(QByteArray::number(dh));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setDL(const quint32 dl) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_DL);
    at.setParameter(QByteArray::number(dl));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setMY(const quint16 my) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_MY);
    at.setParameter(QByteArray::number(my));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setMP(const quint16 mp) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_MP);
    at.setParameter(QByteArray::number(mp));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setNC(const quint32 nc) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_NC);
    at.setParameter(QByteArray::number(nc));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setSH(const quint32 sh) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_SH);
    at.setParameter(QByteArray::number(sh));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setSL(const quint32 sl) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_SL);
    at.setParameter(QByteArray::number(sl));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setNI(const QString & ni) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_NI);
    at.setParameter(ni.toUtf8());
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setSE(const quint8 se) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_SE);
    at.setParameter(QByteArray::number(se));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setDE(const quint8 de) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_DE);
    at.setParameter(QByteArray::number(de));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setCI(const quint8 ci) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_CI);
    at.setParameter(QByteArray::number(ci));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setTO(const quint8 to) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_TO);
    at.setParameter(QByteArray::number(to));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setNP(const quint8 np) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_NP);
    at.setParameter(QByteArray::number(np));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setDD(const quint16 dd) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_DD);
    at.setParameter(QByteArray::number(dd));
    sendATCommandAsync(&at);
    return true;
}

bool XBee::setCR(const quint8 cr) {
    ATCommandFrame at;
    at.setCommand(ATCommandFrame::Command_CR);
    at.setParameter(QByteArray::number(cr));
    sendATCommandAsync(&at);
    return true;
}

//_________________________________________________________________________________________________
//___________________________________________PRIVATE API___________________________________________
//_________________________________________________________________________________________________
void XBee::readData()
{
    unsigned startDelimiter = 0x7E;

    QByteArray data = m_serial->readAll();
    QByteArray packet;
    buffer.append(data);

    if(m_mode == TransparentMode) {
        if(buffer.endsWith(13)) {
            emit rawDataReceived(buffer);
            buffer.clear();
        }
    }
    else {

        while(!buffer.isEmpty() && (unsigned char)buffer.at(0) != (unsigned char)startDelimiter) {
            buffer.remove(0, 1);
        }
        if(buffer.size() > 2) {
            unsigned length = buffer.at(2)+4;
            if((unsigned char)buffer.size() >= (unsigned char)length){
                packet.append(buffer.left(length));
                qDebug() << Q_FUNC_INFO << QString("0x").append(packet.toHex());
                processPacket(packet);
                buffer.remove(0, length);
            }
        }
    }
}

void XBee::processPacket(QByteArray packet)
{
    unsigned packetType = (unsigned char)packet.at(3);
    qDebug() << Q_FUNC_INFO << "packet type :" << QString::number(packetType, 16).prepend("0x");

    switch (packetType) {
    case DigiMeshFrame::ATCommandResponseFrame : {
        ATCommandResponseFrame *response = new ATCommandResponseFrame(packet);
        processATCommandRespone(response);
        response->deleteLater();
        break;
    }
    case DigiMeshFrame::ModemStatusFrame : {
        ModemStatusFrame *response = new ModemStatusFrame(packet);
        emit receivedModemStatus(response);
        response->deleteLater();
        break;
    }
    case DigiMeshFrame::TransmitStatusFrame : {
        TransmitStatusFrame *response = new TransmitStatusFrame(this);
        response->readPacket(packet);
        emit receivedTransmitStatus(response);
        break;
    }
    case DigiMeshFrame::RXIndicatorFrame : {
        ReceivePacketFrame *response = new ReceivePacketFrame(this);
        response->readPacket(packet);
        emit receivedRXIndicator(response);
        break;
    }
    case DigiMeshFrame::ExplicitRxIndicatorFrame : {
        ExplicitRxIndicatorFrame *response = new ExplicitRxIndicatorFrame(this);
        response->readPacket(packet);
        emit receivedRXIndicatorExplicit(response);
        break;
    }
    case DigiMeshFrame::NodeIdentificationIndicatorFrame : {
        NodeIdentificationIndicatorFrame *response = new NodeIdentificationIndicatorFrame(this);
        response->setPacket(packet);
        emit receivedNodeIdentificationIndicator(response);
        break;
    }
    case DigiMeshFrame::RemoteATCommandResponseFrame : {
        RemoteATCommandResponseFrame *response = new RemoteATCommandResponseFrame(packet);
        emit receivedRemoteCommandResponse(response);
        response->deleteLater();
        break;
    }
    default:
        qDebug() << Q_FUNC_INFO << qPrintable(QString("Error: Unknown or Unhandled Packet (type=%1): 0x%2").
                                              arg(packetType,0,16).
                                              arg(QString(packet.toHex())));
    }
}

#include "nodediscoveryresponseparser.h"
void XBee::processATCommandRespone(ATCommandResponseFrame *rep) {
    Q_ASSERT(rep);
    ATCommandFrame::ATCommand at = rep->atCommand();
    QByteArray data = rep->data().toHex();
    quint32 dataInt = QString(data).toInt(0, 16);

    qDebug() << Q_FUNC_INFO << "AT command" << ATCommandFrame::atCommandToString(at) << QString("0x%1").arg(at , 0, 16) << " : " << data << dataInt;

    switch(at) {
    // Addressing
    case ATCommandFrame::Command_DH : m_dh = dataInt; emit DHChanged(m_dh); break;
    case ATCommandFrame::Command_DL : m_dl = dataInt; emit DLChanged(m_dl); break;
    case ATCommandFrame::Command_MY : m_my = dataInt; emit MYChanged(m_my); break;
    case ATCommandFrame::Command_MP : m_mp = dataInt; emit MPChanged(m_mp); break;
    case ATCommandFrame::Command_NC : m_nc = dataInt; emit NCChanged(m_nc); break;
    case ATCommandFrame::Command_SH : m_sh = dataInt; emit SHChanged(m_sh); break;
    case ATCommandFrame::Command_SL : m_sl = dataInt; emit SLChanged(m_sl); break;
    case ATCommandFrame::Command_NI : m_ni = data; emit NIChanged(m_ni); break;
    case ATCommandFrame::Command_SE : m_se = dataInt; emit SEChanged(m_se); break;
    case ATCommandFrame::Command_DE : m_de = dataInt; emit DEChanged(m_de); break;
    case ATCommandFrame::Command_CI : m_ci = dataInt; emit CIChanged(m_ci); break;
    case ATCommandFrame::Command_TO : m_to = dataInt; emit TOChanged(m_to); break;
    case ATCommandFrame::Command_NP : m_np = dataInt; emit NPChanged(m_np); break;
    case ATCommandFrame::Command_DD : m_dd = dataInt; emit DDChanged(m_dd); break;
    case ATCommandFrame::Command_CR : m_cr = dataInt; emit CRChanged(m_cr); break;

    case ATCommandFrame::Command_ND : {
        NodeDiscoveryResponseParser nd;
        nd.setPacketData(rep->data());
        break;
    }
    default:
        qWarning() << Q_FUNC_INFO << "Unhandled AT command" <<  QString("0x%1 (%2)").arg(at , 0, 16).arg(ATCommandFrame::atCommandToString(at));
    }
    emit receivedATCommandResponse(rep);
}

bool XBee::initSerialConnection()
{
    if(!m_serial)
        return false;

    if (m_serial->open(QIODevice::ReadWrite))
    {
        if(m_serial->isOpen())
        {
            qDebug() << "XBEE: Connected successfully";
            qDebug() << "XBEE: Serial Port Name: " << m_serial->portName();
            xbeeFound = true;
            startupCheck();
            return true;
        }
    }
    else
    {
        qDebug() << "XBEE: Serial Port" << m_serial->portName() << "could not be opened";
    }


    return false;
}

bool XBee::startupCheck()
{
    bool bRet = false;
    ATCommandFrame at;
    ATCommandResponseFrame * rep = NULL;
    if(xbeeFound)
    {
        at.setCommand("AP");
        rep = sendATCommandSync(&at);
        // Check AP mode
        if(rep)
        {
            if(rep->commandStatus() == ATCommandResponseFrame::Ok) {
                bool ok = false;
                int apmode = rep->data().toHex().toInt(&ok,16);
                if(ok) {
                    if(apmode != 1) {
                        qDebug() << Q_FUNC_INFO << "XBee radio is not in API mode without escape characters (AP=1). Try to set AP=1";
                        at.setParameter("1");
                        delete rep;
                        rep = sendATCommandSync(&at);
                        if(rep) {
                            if(rep->commandStatus() == ATCommandResponseFrame::Ok) {
                                qDebug() << Q_FUNC_INFO << "XBee in API mode (AP=1) : OK";
                                bRet = true;
                                delete rep;
                                rep = NULL;
                            }
                            else {
                                qWarning() << Q_FUNC_INFO << "Failed to set AP=1 !";
                            }
                        }
                        else {
                            qWarning() << Q_FUNC_INFO << "Failed to set AP=1 !";
                        }
                    }
                    else {
                        qDebug() << Q_FUNC_INFO << "XBee in API mode (AP=1) : OK";
                        bRet = true;
                    }
                }
                else {
                    qWarning() << Q_FUNC_INFO << "Failed to retreive AP parameter from received response !";
                }
            }
            else {
                qWarning() << Q_FUNC_INFO << "AP command failed !";
            }
        }
        else
        {
            qDebug() << Q_FUNC_INFO << "Failed to get AP parameter";
        }
        // Check HardWare Rev
        ATCommandFrame hv;
        hv.setCommand("HV");
        rep = sendATCommandSync(&hv);
        if(rep)
        {
            if(rep->commandStatus() == ATCommandResponseFrame::Ok) {
                bool ok = false;
                int hv = rep->data().toHex().toInt(&ok,16);
                if(ok) {
                    if(hv == QtXBee::XBeeSerie1 || QtXBee::XBeeSerie1Pro) {
                        qDebug() << Q_FUNC_INFO << "XBee Serie 1/1Pro : OK";
                        bRet &= true;
                    }
                    else {
                        qDebug() << Q_FUNC_INFO << "XBee Serie 1/1Pro : KO (unsuported hardware version)";
                        bRet = false;
                    }
                }
                else {
                    qWarning() << Q_FUNC_INFO << "Failed to retreive HV parameter from received response !";
                    bRet = false;
                }
            }
            else {
                qWarning() << Q_FUNC_INFO << "HV command failed !";
                bRet = false;
            }
        }
        else
        {
            qDebug() << Q_FUNC_INFO << "Failed to get HV parameter";
            bRet = false;
        }

    }
    if(rep) {
        delete rep;
    }
    return bRet;
}
