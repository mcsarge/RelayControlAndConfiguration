#include <Arduino.h>
#include <esp32ModbusTCP.h>
#include "Log.h"
#include "ModbusStuff.h"
#include "ChargeControllerInfo.h"

int _currentRegister = 0;
uint16_t _currentRequestId = 0;

void modbus4100(uint8_t *data);
void modbus4360(uint8_t *data);
void modbus4163(uint8_t *data);
void modbus4243(uint8_t *data);
void modbus16386(uint8_t *data);

#define numBanks 5
static ModbusRegisterBank _registers[numBanks] = {
	{false, 4100, 44, modbus4100},    //0
	{false, 4360, 22, modbus4360},    //1
	{false, 4163, 2, modbus4163},     //2
	{false, 4243, 32, modbus4243},    //3
	{false, 16386, 4, modbus16386}};  //4

hw_timer_t *_watchdogTimer = NULL;
hw_timer_t *_modbusReadTimer = NULL;
int modbusRequestFailureCount = 0;
boolean doGather = false;
uint8_t _boilerPlateReadBitField = 0;

unsigned long _nextGatherTime = 0;
unsigned long _nextModbusPollTimeStamp = 0;
unsigned long _currentGatherRate = DEFAULT_GATHER_RATE;

esp32ModbusTCP *_pClassic;
ChargeControllerInfo _chargeControllerInfo;
chargerDataForRelayControl _chargerData;

void IRAM_ATTR clearModbusRead()
{
	_currentRequestId = 0; //clear a hanging read that appears to never come back.
	loge("Long MODBUS read, requestId cleared");
	//ets_printf("Long MODBUS read, requestId cleared\n");
}

void init_modbusReadTimer()
{
	if (_modbusReadTimer == NULL)
	{
		_modbusReadTimer = timerBegin(0, 80, true);					   //timer 0, div 80
		timerAttachInterrupt(_modbusReadTimer, &clearModbusRead, true);	  //attach callback
		timerAlarmWrite(_modbusReadTimer, MODBUS_READ_TIMEOUT * 1000, false); //set time in us
		timerAlarmEnable(_modbusReadTimer);							   //enable interrupt
	}
}

void IRAM_ATTR resetModule()
{
	//ets_printf("watchdog timer expired - rebooting\n");
	esp_restart();
}

void init_watchdog()
{
	if (_watchdogTimer == NULL)
	{
		_watchdogTimer = timerBegin(1, 80, true);					   //timer 0, div 80
		timerAttachInterrupt(_watchdogTimer, &resetModule, false);	  //attach callback
		timerAlarmWrite(_watchdogTimer, WATCHDOG_TIMER * 1000, false); //set time in us
		timerAlarmEnable(_watchdogTimer);							   //enable interrupt
	}
}

void feed_watchdog()
{
	if (_watchdogTimer != NULL)
	{
		timerWrite(_watchdogTimer, 0); // feed the watchdog
	}
}

void resetStart_modbusReadTimer()
{
	if (_modbusReadTimer != NULL)
	{
		timerWrite(_modbusReadTimer, 0); // reset the timer to start counting up again
		if (!timerStarted(_modbusReadTimer)){
			timerStart(_modbusReadTimer);
		}
	}
}

void stop_modbusReadTimer() 
{
	if (_modbusReadTimer != NULL)
	{
		timerStop(_modbusReadTimer); // reset the timer to start counting up again
	}
}

uint16_t Getuint16Value(int index, uint8_t *data)
{
	index *= 2;
	return (data[index] << 8 | data[index + 1]);
}

int16_t Getint16Value(int index, uint8_t *data)
{
	index *= 2;
	return (data[index] << 8 | data[index + 1]);
}

uint32_t Getuint32Value(int index, uint8_t *data)
{
	index *= 2;
	return data[index + 2] << 24 | data[index + 3] << 16 | data[index] << 8 | data[index + 1];
}

int32_t Getint32Value(int index, uint8_t *data)
{
	index *= 2;
	return data[index + 2] << 24 | data[index + 3] << 16 | data[index] << 8 | data[index + 1];
}

float GetFloatValue(int index, uint8_t *data, float div = 1.0)
{
	return Getint16Value(index, data) / div;
}

uint8_t GetMSBValue(int index, uint8_t *data)
{
	index *= 2;
	return (data[index]);
}

boolean GetFlagValue(int index, uint16_t mask, uint8_t *data)
{
	index *= 2;
	int16_t w =  data[index] << 8 | data[index + 1];
	boolean rVal = (w & mask) != 0;
	return rVal;
}

int readModbus()
{
	boolean retVal = 0;
	boolean done = false;

	if (_currentRequestId != 0) return 3; //waiting on another read so do nothing.

	while (!done)
	{
		if (_currentRegister < numBanks)
		{
			if (_registers[_currentRegister].received == false)
			{
				logd("About to request, cursor= %d; Requesting %d for %d registers", _currentRegister, _registers[_currentRegister].address, _registers[_currentRegister].numberOfRegisters);
				uint16_t reqId = _pClassic->readHoldingRegisters(_registers[_currentRegister].address, _registers[_currentRegister].numberOfRegisters);
				if ( reqId != 0)
				{
					logd("Request Id %d for register %d Requesting %d for %d registers. ", reqId, _currentRegister, _registers[_currentRegister].address, _registers[_currentRegister].numberOfRegisters);
					//Only return true if a request was actually made.
					retVal = 1; //Made request
					_currentRequestId = reqId;
					resetStart_modbusReadTimer();
				}
				else
				{
					loge("Request %d failed\n", _registers[_currentRegister].address);
					retVal = 0; //error
				}
				done = true;
			} else {
				retVal = 2; //Have data, skip
			}
			done = true;
			_currentRegister++;
		}
		else
		{
			_currentRegister = 0;
		}
	}
	return retVal;
}

void modbusErrorCallback(uint16_t packetId, MBError error)
{
	logd("Error - packetId[%d], requestId[%d]", packetId, _currentRequestId);
	modbusRequestFailureCount++;
	//Only hanging 1 at a time, so clear it out.
	_currentRequestId = 0;
	stop_modbusReadTimer();

	String text;
	switch (error)
	{
	case 0x00:
		text = "SUCCESS";
		break;
	case 0x01:
		text = "ILLEGAL_FUNCTION";
		break;
	case 0x02:
		text = "ILLEGAL_DATA_ADDRESS";
		break;
	case 0x03:
		text = "ILLEGAL_DATA_VALUE";
		break;
	case 0x04:
		text = "SERVER_DEVICE_FAILURE";
		break;
	case 0x05:
		text = "ACKNOWLEDGE";
		break;
	case 0x06:
		text = "SERVER_DEVICE_BUSY";
		break;
	case 0x07:
		text = "NEGATIVE_ACKNOWLEDGE";
		break;
	case 0x08:
		text = "MEMORY_PARITY_ERROR";
		break;
	case 0xE0:
		text = "TIMEOUT";
		break;
	case 0xE1:
		text = "INVALID_SLAVE";
		break;
	case 0xE2:
		text = "INVALID_FUNCTION";
		break;
	case 0xE3:
		text = "CRC_ERROR";
		break;
	case 0xE4:
		text = "COMM_ERROR";
		break;
	}
	loge("packetId[0x%x], error[%s]", packetId, text);
}

void modbus4100 (uint8_t *data) //Register 0
{
	_chargeControllerInfo.BatVoltage = GetFloatValue(14, data, 10.0);
	_chargeControllerInfo.PVVoltage = GetFloatValue(15, data, 10.0);
	_chargeControllerInfo.BatCurrent = GetFloatValue(16, data, 10.0);
	_chargeControllerInfo.EnergyToday = GetFloatValue(17, data, 10.0);
	_chargeControllerInfo.Power = GetFloatValue(18, data);
	_chargeControllerInfo.ChargeState = GetMSBValue(19, data);
	_chargeControllerInfo.PVCurrent = GetFloatValue(20, data, 10.0);
	_chargeControllerInfo.TotalEnergy = Getuint32Value(25, data) / 10.0;
	_chargeControllerInfo.InfoFlagsBits = Getuint32Value(29, data);
	_chargeControllerInfo.BatTemperature = GetFloatValue(31, data, 10.0);
	_chargeControllerInfo.FETTemperature = GetFloatValue(32, data, 10.0);
	_chargeControllerInfo.PCBTemperature = GetFloatValue(33, data, 10.0);
	_chargeControllerInfo.FloatTimeTodaySeconds = Getuint16Value(37, data);
	_chargeControllerInfo.AbsorbTime = Getuint16Value(38, data);
	_chargeControllerInfo.EqualizeTime = Getuint16Value(42, data);
	_chargeControllerInfo.Aux1 = GetFlagValue(29, 0x4000, data);
	_chargeControllerInfo.Aux2 = GetFlagValue(29, 0x8000, data);

	if ((_boilerPlateReadBitField & 0x1) == 0)
	{
		_boilerPlateReadBitField |= 0x1;
		uint16_t reg1 = Getuint16Value(0, data);
		char buf[32];
		sprintf(buf, "Classic %d (rev %d)", reg1 & 0x00ff, reg1 >> 8);
		_chargeControllerInfo.model = buf;
		int buildYear = Getuint16Value(1, data);
		int buildMonthDay = Getuint16Value(2, data);
		sprintf(buf, "%d%02d%02d", buildYear, (buildMonthDay >> 8), (buildMonthDay & 0x00ff));
		_chargeControllerInfo.buildDate = buf;
		_chargeControllerInfo.lastVOC = GetFloatValue(21, data, 10.0);
		_chargeControllerInfo.unitID = Getuint32Value(10, data);
		short reg6 = Getuint16Value(5, data);
		short reg7 = Getuint16Value(6, data);
		short reg8 = Getuint16Value(7, data);
		char mac[32];
		sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", reg8 >> 8, reg8 & 0x00ff, reg7 >> 8, reg7 & 0x00ff, reg6 >> 8, reg6 & 0x00ff);
		_chargeControllerInfo.macAddress = mac;
	}
}

void modbus4360(uint8_t *data)// whizbang readings  Register 1
{ 
	_chargeControllerInfo.PositiveAmpHours = Getuint32Value(4, data);
	_chargeControllerInfo.NegativeAmpHours = abs((int)Getuint32Value(6, data));
	_chargeControllerInfo.NetAmpHours = 0; //Getint32Value(8, data); // todo causing deserialization exception in android
	_chargeControllerInfo.ShuntTemperature = (Getuint16Value(11, data) & 0x00ff) - 50.0f;
	_chargeControllerInfo.WhizbangBatCurrent = GetFloatValue(10, data, 10.0);
	_chargeControllerInfo.SOC = Getuint16Value(12, data);
	_chargeControllerInfo.RemainingAmpHours = Getuint16Value(16, data);
	_chargeControllerInfo.TotalAmpHours = Getuint16Value(20, data);
}

void modbus4163(uint8_t *data) // boilerplate data Register 2
{
	if ((_boilerPlateReadBitField & 0x02) == 0)
	{
		_boilerPlateReadBitField |= 0x02;
		_chargeControllerInfo.mpptMode = Getuint16Value(0, data);
		int Aux12FunctionS = (Getuint16Value(1, data) & 0x3f00) >> 8;
		_chargeControllerInfo.hasWhizbang = Aux12FunctionS == 18;
	}
}
void modbus4243(uint8_t *data) //Register 3
{
	if ((_boilerPlateReadBitField & 0x04) == 0)
	{
		_boilerPlateReadBitField |= 0x04;
		_chargeControllerInfo.VbattRegSetPTmpComp = GetFloatValue(0, data, 10.0);
		_chargeControllerInfo.nominalBatteryVoltage = Getuint16Value(1, data);
		_chargeControllerInfo.endingAmps = GetFloatValue(2, data, 10.0);
		_chargeControllerInfo.ReasonForResting = Getuint16Value(31, data);
	}
}
void modbus16386(uint8_t *data)  //Register 4
{
	if ((_boilerPlateReadBitField & 0x08) == 0)
	{
		_boilerPlateReadBitField |= 0x08;
		short reg16387 = Getuint16Value(0, data);
		short reg16388 = Getuint16Value(1, data);
		short reg16389 = Getuint16Value(2, data);
		short reg16390 = Getuint16Value(3, data);
		char unit[16];
		snprintf_P(unit, sizeof(unit), "%d", (reg16388 << 16) + reg16387);
		_chargeControllerInfo.appVersion = unit;
		snprintf_P(unit, sizeof(unit), "%d", (reg16390 << 16) + reg16389);
		_chargeControllerInfo.netVersion = unit;
	}
}

void modbusCallback(uint16_t packetId, uint8_t slaveAddress, MBFunctionCode functionCode, uint8_t *data, uint16_t byteCount)
{
	int regCount = byteCount / 2;
	logd("packetId[%d], currentRequesId[%d], slaveAddress[%d], functionCode[%d], numberOfRegisters[%d]", packetId, _currentRequestId, slaveAddress, functionCode, regCount);
	_currentRequestId = 0; 
	stop_modbusReadTimer();

	for (int i = 0; i < numBanks; i++)
	{
		if (_registers[i].numberOfRegisters == regCount)
		{
			_registers[i].received = true; // received data for this set of registers
			_registers[i].func(data);
			feed_watchdog();
			break;
		}
	}
}


bool setupModbus(String ip_addr, String port_number, WiFiClass _wifi){
    IPAddress ip;

    //test to see if this is an IP stored
    if (!ip.fromString(ip_addr)){
		//Try to get the Ip address using the name
		int err = WiFi.hostByName(ip_addr.c_str(), ip) ;
		if(err != 1){
			Serial.print("Not able to convert ip_addr to ip address, Error code: ");
			Serial.println(err);
			return false;
		}		
	}

	Serial.println("\nStarting connection to server...");
	int port = atoi(port_number.c_str());
	Serial.print("ip_addr=");
	Serial.print(ip_addr);
	Serial.print(" IP=");
	Serial.print(ip.toString());
	Serial.print(" Port=");
	Serial.println(port);

	_pClassic = new esp32ModbusTCP(10, ip, port);
	_pClassic->onData(modbusCallback);
	_pClassic->onError(modbusErrorCallback);
	
	_chargerData.gatherMillis = 0;
	_chargerData.SOC = 0;
	_chargerData.BatVoltage = 0.0;
	_chargerData.BatCurrent = 0.0;
	_chargerData.PVVoltage = 0.0;
	_chargerData.PVCurrent = 0.0;
	
	_nextGatherTime = millis() + INITIAL_MODBUS_COLLECTION_DELAY;

	return true;
}

bool gatherModbusData() {
    //Is it gather time now?
    if (millis() > _nextGatherTime)
    {
        doGather = true;
        _nextGatherTime = millis() + _currentGatherRate;
        _registers[0].received = false;
        _registers[1].received = false;
        _registers[3].received = false;
        modbusRequestFailureCount = 0;
    }

    //doGather remains true until all are gathered or failure.
    if (doGather){
        //Is it time to read from the Modbus again?
        //This will automatically read from the areas that need to be read from.
        int status = readModbus(); //0=failed, 1=request success, 2=request not needed; 3 = waiting
        
        if (status != 3 && status != 0) {
            logd("MODBUS Read Request status = %d", status);
        }

        if ( status == 0) {
            modbusRequestFailureCount++; //if failed, up the count
            loge("modbusRequestFailureCount %d", modbusRequestFailureCount);
		}

		//If failed to get readings from the modbus too many times, skip until next publish time
		if (modbusRequestFailureCount >= MAX_MODBUS_READ_ATTEMPTS){
			//skip this whole request
			doGather = false;
			_currentGatherRate = 2*_currentGatherRate; //halve the rate when getting errors.
			_nextGatherTime = millis() + _currentGatherRate; 
			_registers[0].received = false;
			_registers[1].received = false;
			_registers[3].received = false;
			loge("MODBUS failures causing publish skip");
		} else {
			//if we have them all, return true
			if (_registers[0].received && _registers[1].received && _registers[2].received ) {

				//Save the data
				_chargerData.SOC = _chargeControllerInfo.SOC;
				_chargerData.BatVoltage = _chargeControllerInfo.BatVoltage;
				_chargerData.BatCurrent = _chargeControllerInfo.WhizbangBatCurrent;
				_chargerData.PVVoltage = _chargeControllerInfo.PVVoltage;
				_chargerData.PVCurrent = _chargeControllerInfo.PVCurrent;
				_chargerData.gatherMillis = millis();
			    time(&_chargerData.timeDataWasGathered); //store the gather time.

				doGather = false;
				_currentGatherRate = DEFAULT_GATHER_RATE;
				return true;
			}
		}
	}
    return false;
}

void printModbusData(){
    if ((millis() - _chargerData.gatherMillis) < _currentGatherRate*2) {
        Serial.print("SOC = ");
        Serial.println(_chargerData.SOC);
        Serial.print("Battery Voltage = ");
        Serial.println(_chargerData.BatVoltage);
        Serial.print("Battery Current = ");
        Serial.println(_chargerData.BatCurrent);
        Serial.print("PV Voltage = ");
        Serial.println(_chargerData.PVVoltage);
        Serial.print("PV Current = ");
        Serial.println(_chargerData.PVCurrent);
        Serial.print("GatherMillis = ");
        Serial.println(_chargerData.gatherMillis);
    } else {
        Serial.println("The data is too OLD");
    }

}

chargerDataForRelayControl getChargerData(){
    return _chargerData;
}
