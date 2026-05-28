/*
 *                                                                                                  geany_encoding=koi8-r
 * astrosib.cpp
 *
 * Copyright 2017 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */
/*
   INDI Developers Manual
   Tutorial #2

   "Simple Telescope Driver"

   We develop a simple telescope simulator.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simplescope.cpp
    \brief Construct a basic INDI telescope device that simulates GOTO commands.
    \author Jasem Mutlaq

    \example simplescope.cpp
    A simple GOTO telescope that simulator slewing operation.
*/

#include "astrosib.h"

#include "indicom.h"

#include <cmath>
#include <memory>
#include <string.h>

// timer polling time
#define POLLMS 1000

static const char *FOCUSER_TAB = "Focuser management";
static const char *COOLER_TAB = "Cooler management";
static const char *HEATER_TAB = "Heater management";

std::unique_ptr<AstroSib> aSib(new AstroSib());

/**************************************************************************************
** Return properties of device.
***************************************************************************************/
void ISGetProperties(const char *dev){
    aSib->ISGetProperties(dev);
}

/**************************************************************************************
** Process new switch from client
***************************************************************************************/
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n){
    aSib->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
** Process new text from client
***************************************************************************************/
void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n){
    aSib->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** Process new number from client
***************************************************************************************/
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n){
    aSib->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
** Process new blob from client
***************************************************************************************/
void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n){
    aSib->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/**************************************************************************************
** Process snooped property from another driver
***************************************************************************************/
void ISSnoopDevice(XMLEle *root){
    INDI_UNUSED(root);
}

AstroSib::AstroSib(){
    /* code should be here */
    //DBG_LEVEL = INDI::Logger::getInstance().addDebugLevel("Astrosib Verbose", "SCOPE");
}

AstroSib::~AstroSib(){
    /* code should be here */
    if (isConnected()) AstroSib::Disconnect();
}

/**************************************************************************************
** We init our properties here. The only thing we want to init are the Debug controls
***************************************************************************************/
bool AstroSib::initProperties(){
    // ALWAYS call initProperties() of parent first
    INDI::DefaultDevice::initProperties();

    // Shutter leaves
    IUFillSwitch(&ShtrsS[SHTR_OPENED], "SHTR_OPENED", "Open Scope", ISS_OFF);
    IUFillSwitch(&ShtrsS[SHTR_CLOSED], "SHTR_CLOSED", "Close Scope", ISS_ON);
    IUFillSwitchVector(&ShtrsSP, ShtrsS, 2, getDeviceName(), "SHTR_CONTROL", "Shutter Control",
                        MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 1, IPS_OK);

    // Mirror and ambient temperature
    IUFillNumber(&TempN[0], "MIRROR_TEMP", "Mirror temperature", "%.1f", -100., 100., 0.1, 0.);
    IUFillNumber(&TempN[1], "AMBIENT_TEMP", "Ambient temperature", "%.1f", -100., 100., 0.1, 0.);
    IUFillNumber(&TempN[2], "FOCUSER_TEMP", "Focuser temperature", "%.1f", -100., 100., 0.1, 0.);
    IUFillNumberVector(&TempNP, TempN, 3, getDeviceName(), "TEMPERATURE", "Temperature",
                        MAIN_CONTROL_TAB, IP_RO, 1, IPS_IDLE);

    // Focuser: current, relative and absolute position
    IUFillNumber(&FocuserN[0], "FOC_CUR", "Current focuser position", "%.0f", 0, 65536, 1, 0);
    IUFillNumber(&FocuserN[1], "FOC_REL", "Relative focuser position", "%.0f", -65536, 65536, 1, 0);
    IUFillNumber(&FocuserN[2], "FOC_ABS", "Absolute focuser position", "%.0f", 0, 65536, 1, 0);
    IUFillNumberVector(&FocuserNPcur, FocuserN, 1, getDeviceName(), "FOCUS_CUR", "Focuser current",
                        FOCUSER_TAB, IP_RO, 1, IPS_IDLE);
    FocuserNPcur.s = IPS_OK;
    IUFillNumberVector(&FocuserNP, &FocuserN[1], 2, getDeviceName(), "FOCUS_SET", "Focuser set",
                        FOCUSER_TAB, IP_RW, 1, IPS_IDLE);
    DEBUGF(INDI::Logger::DBG_SESSION, "FOCINIT, cur: %g, rel: %g, abs: %g", FocuserN[0].value, FocuserN[1].value, FocuserN[2].value);
    // Mirror cooler  ON/OFF
    IUFillSwitch(&CoolerS[_ON], "COOLER_ON", "Cooler ON", ISS_OFF);
    IUFillSwitch(&CoolerS[_OFF], "COOLER_OFF", "Cooler OFF", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "COOLER_CONTROL", "Cooler Control",
                        COOLER_TAB, IP_RW, ISR_1OFMANY, 1, IPS_IDLE);
    IUFillSwitch(&ACoolerS[_ON], "ACOOLER_ON", "Cooler auto ON", ISS_OFF);
    IUFillSwitch(&ACoolerS[_OFF], "ACOOLER_OFF", "Cooler auto OFF", ISS_ON);
    IUFillSwitchVector(&ACoolerSP, ACoolerS, 2, getDeviceName(), "ACOOLER_CONTROL", "Auto Cooler Control",
                        COOLER_TAB, IP_RW, ISR_1OFMANY, 1, IPS_IDLE);
    // Secondary mirror / Primary focus corrector Heater
    IUFillSwitch(&AHeaterS[_ON], "HTR_ON", "Auto Heater ON", ISS_OFF);
    IUFillSwitch(&AHeaterS[_OFF], "HTR_OFF", "Auto Heater OFF", ISS_ON);
    IUFillSwitchVector(&AHeaterSP, AHeaterS, 2, getDeviceName(), "HEATER_CONTROL", "Heater Control",
                        HEATER_TAB, IP_RW, ISR_1OFMANY, 1, IPS_IDLE);
    IUFillNumber(&PHeaterN, "PHTR", "Set heater power", "%.0f", 0, 100, 1, 0);
    IUFillNumberVector(&PHeaterNP, &PHeaterN, 1, getDeviceName(), "PHTR_CTRL", "Heater power",
                        HEATER_TAB, IP_RW, 1, IPS_IDLE);
    return true;
}

bool AstroSib::updateProperties(){
    // We must ALWAYS call the parent class updateProperties() first
    DefaultDevice::updateProperties();

    // If we are connected, we define the property to the client.
    if (isConnected()){
        defineSwitch(&ShtrsSP);
        defineNumber(&TempNP);
        defineNumber(&FocuserNPcur);
        defineNumber(&FocuserNP);
        defineSwitch(&CoolerSP);
        defineSwitch(&ACoolerSP);
        defineSwitch(&AHeaterSP);
        defineNumber(&PHeaterNP);
        SetTimer(POLLMS);
    // Otherwise, we delete the property from the client
    }else{
        deleteProperty(ShtrsSP.name);
        deleteProperty(TempNP.name);
        deleteProperty(FocuserNPcur.name);
        deleteProperty(FocuserNP.name);
        deleteProperty(CoolerSP.name);
        deleteProperty(ACoolerSP.name);
        deleteProperty(AHeaterSP.name);
        deleteProperty(PHeaterNP.name);
    }

    return true;
}

/**************************************************************************************
** Process new switch from client
***************************************************************************************/
bool AstroSib::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n){
    // Mak sure the call is for our device
    if(!strcmp(dev,getDeviceName())){
        // Check if the call for shutters
        if (!strcmp(name, ShtrsSP.name)){
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // If door is the same state as actionName, then we do nothing. i.e. if actionName is SHTR_OPENED and our shutter is already open, we return
            int currentIndex = IUFindOnSwitchIndex(&ShtrsSP);
            if (!strcmp(actionName, ShtrsS[currentIndex].name)){
                DEBUGF(INDI::Logger::DBG_SESSION, "Shutter is already %s", ShtrsS[currentIndex].label);
                ShtrsSP.s = IPS_OK;
                IDSetSwitch(&ShtrsSP, NULL);
                return true;
            }
            // Otherwise, let us update the switch state
            IUUpdateSwitch(&ShtrsSP, states, names, n);
            DEBUGF(INDI::Logger::DBG_SESSION, "Shutter is going to %s", ShtrsS[currentIndex].label);
            ShtrsSP.s = IPS_BUSY;
            IDSetSwitch(&ShtrsSP, NULL);
            return ShutterSetState(currentIndex);
        }else if (!strcmp(name, CoolerSP.name)){
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            int currentIndex = IUFindOnSwitchIndex(&CoolerSP);
            CoolerSP.s = IPS_OK;
            if (!strcmp(actionName, CoolerS[currentIndex].name)){
                DEBUGF(INDI::Logger::DBG_SESSION, "Cooler is already %s", CoolerS[currentIndex].label);
                IDSetSwitch(&CoolerSP, NULL);
                return true;
            }
            IUUpdateSwitch(&CoolerSP, states, names, n);
            currentIndex = IUFindOnSwitchIndex(&CoolerSP);
            DEBUGF(INDI::Logger::DBG_SESSION, "Cooler is now %s", CoolerS[currentIndex].label);
            IDSetSwitch(&CoolerSP, NULL);
            return CoolerSetState(currentIndex);
        }else if (!strcmp(name, ACoolerSP.name)){
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            int currentIndex = IUFindOnSwitchIndex(&ACoolerSP);
            ACoolerSP.s = IPS_OK;
            if (!strcmp(actionName, ACoolerS[currentIndex].name)){
                DEBUGF(INDI::Logger::DBG_SESSION, "Auto Cooler is already %s", ACoolerS[currentIndex].label);
                IDSetSwitch(&ACoolerSP, NULL);
                return true;
            }
            IUUpdateSwitch(&ACoolerSP, states, names, n);
            currentIndex = IUFindOnSwitchIndex(&ACoolerSP);
            DEBUGF(INDI::Logger::DBG_SESSION, "Auto Cooler is now %s", ACoolerS[currentIndex].label);
            IDSetSwitch(&ACoolerSP, NULL);
            return ACoolerSetState(currentIndex);
        }else if (!strcmp(name, AHeaterSP.name)){
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            int currentIndex = IUFindOnSwitchIndex(&AHeaterSP);
            AHeaterSP.s = IPS_OK;
            if (!strcmp(actionName, AHeaterS[currentIndex].name)){
                DEBUGF(INDI::Logger::DBG_SESSION, "Auto Heater is already %s", AHeaterS[currentIndex].label);
                IDSetSwitch(&AHeaterSP, NULL);
                return true;
            }
            IUUpdateSwitch(&AHeaterSP, states, names, n);
            currentIndex = IUFindOnSwitchIndex(&AHeaterSP);
            DEBUGF(INDI::Logger::DBG_SESSION, "Auto Heater is now %s", AHeaterS[currentIndex].label);
            IDSetSwitch(&AHeaterSP, NULL);
            return AHeaterSetState(currentIndex);
        }
    }
    // If we did not process the switch, let us pass it to the parent class to process it
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool AstroSib::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    DEBUGF(INDI::Logger::DBG_SESSION, "NEW NUM, name: %s, value=%g, names = %s, n: %d", name, values[0], names[0], n);
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0){
        if(strcmp(name, FocuserNP.name) == 0){
            if(FocInProgress){ // Already moving
                DEBUG(INDI::Logger::DBG_ERROR, "Wait still focus moving ends");
                return false;
            }
            if(IUUpdateNumber(&FocuserNP, values, names, n)){
                DEBUG(INDI::Logger::DBG_ERROR, "Can't set focuser value");
                FocuserN[1].value = 0;
                FocuserN[2].value = focabs;
                FocuserNP.s = IPS_ALERT;
                IDSetNumber(&FocuserNP, nullptr);
                return false;
            }
            DEBUGF(INDI::Logger::DBG_SESSION, "NEW NUMBER, rel: %g, abs: %g", FocuserN[1].value, FocuserN[2].value);
            if(FocuserN[1].value){ // relative
                focabs = foccur + FocuserN[1].value;
            }else if(FocuserN[2].value){ // absolute
                focabs = FocuserN[2].value;
            }
            if(focabs < 0){
                DEBUG(INDI::Logger::DBG_ERROR, "Can't move focuser to negative values");
                focabs = foccur;
                FocuserNP.s = IPS_ALERT;
                IDSetNumber(&FocuserNP, nullptr);
                return false;
            }
            FocInProgress = true;
            FocuserNP.s = IPS_BUSY;
            FocuserN[1].value = focabs - foccur;
            FocuserN[2].value = focabs;
            IDSetNumber(&FocuserNP, nullptr);
            return FocuserSetVal(focabs);
        }else if(strcmp(name, PHeaterNP.name) == 0){
            float oldval = PHeaterN.value;
            if(IUUpdateNumber(&PHeaterNP, values, names, n)){
                DEBUG(INDI::Logger::DBG_ERROR, "Can't set heater power value");
                return false;
            }
            bool stat = HeaterSetPower(*values);
            if(!stat) PHeaterN.value = oldval;
            PHeaterNP.s = (stat) ? IPS_OK : IPS_ALERT;
            IDSetNumber(&PHeaterNP, nullptr);
            return stat;
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}


/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *AstroSib::getDefaultName(){
    return "AstroSib telescope";
}


// Timer events
void AstroSib::TimerHit(){
    static int tensecond = 0;
    ++tensecond;
    if (isConnected()){
        // once per 10 seconds check cooler & heater status
        if(tensecond == 10){
            CoolerChk();
            HeaterChk();
            tensecond = 0;
        }
        // check shutter if(ShtrInProgress)
        if(ShtrInProgress){
            ShutterChk();
        }
        // once per second check temperatures
        TemperatureChk();
        // check focuser if(FocInProgress)
        if(FocInProgress){
            FocuserChk();
        }
        SetTimer(POLLMS);
    }
}

/**
 * Private functions working with RS-232
 */
void AstroSib::FocuserChk(){
    if(!FocInProgress){
        int cur = foccur, abs = focabs, del = cur - abs;
        if(del){
            if(cur > abs){
                cur -= 10;
                if(cur < abs) cur = abs;
            }else{
                cur += 10;
                if(cur > abs) cur = abs;
            }
            foccur = cur;
            focabs = abs;
            //DEBUGF(INDI::Logger::DBG_SESSION, "move, cur: %d, abs: %d", foccur, focabs);
        }
        if(foccur == focabs){
            FocInProgress = false;
            FocuserNP.s = IPS_IDLE;
            IDSetNumber(&FocuserNPcur, "Position reached");
        }else FocuserNP.s = IPS_BUSY;
    }else{
        FocuserNP.s = IPS_IDLE;
    }
    FocuserNPcur.s = IPS_OK;
    FocuserN[0].value = foccur;
    FocuserN[1].value = focabs - foccur;
    FocuserN[2].value = focabs;
    IDSetNumber(&FocuserNPcur, nullptr);
    IDSetNumber(&FocuserNP, nullptr);
}

void AstroSib::TemperatureChk(){
mirrtemp += 0.01; if(mirrtemp > 20.) mirrtemp = -20.;
ambtemp += 0.05; if(ambtemp > 20.) ambtemp = -20;
    TempN[0].value = mirrtemp;
    TempN[1].value = ambtemp;
    TempN[2].value = focusert;
    TempNP.s = IPS_OK;
    IDSetNumber(&TempNP, nullptr);
}

void AstroSib::ShutterChk(){
    if(ShtrInProgress){
        static bool closed = false;
        if(closed){
            ShtrsS[SHTR_OPENED].s = ISS_OFF;
            ShtrsS[SHTR_CLOSED].s = ISS_ON;
        }else{
            ShtrsS[SHTR_OPENED].s = ISS_ON;
            ShtrsS[SHTR_CLOSED].s = ISS_OFF;
        }
        closed = !closed;
    }
    ShtrInProgress = false;
    ShtrsSP.s = IPS_OK;
    IDSetSwitch(&ShtrsSP, NULL);
}

void AstroSib::CoolerChk(){
}

void AstroSib::HeaterChk(){
}

bool AstroSib::ShutterSetState(int newstate){
    (void) newstate;
    ShtrInProgress = true;
    return true;
}

bool AstroSib::CoolerSetState(int newstate){
    (void) newstate;
    return true;
}

bool AstroSib::ACoolerSetState(int newstate){
    (void) newstate;
    return true;
}

bool AstroSib::AHeaterSetState(int newstate){
    (void) newstate;
    return true;
}

bool AstroSib::AstroSib::FocuserSetVal(int focval){
    (void) focval;
    return true;
}

bool AstroSib::HeaterSetPower(int pwr){
    (void) pwr;
    return true;
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool AstroSib::Connect(){
    /* code should be here */
    IDMessage(getDeviceName(), "Astrosib telescope connected successfully!");
    SetTimer(POLLMS);
    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool AstroSib::Disconnect(){
    /* code should be here */
    IDMessage(getDeviceName(), "Astrosib telescope disconnected successfully!");
    return true;
}


/**************************************************************************************
** INDI is asking us to check communication with the device via a handshake
***************************************************************************************/
bool AstroSib::Handshake(){
    // When communicating with a real scope, we check here if commands are receieved
    // and acknolowedged by the mount.
    /* code should be here */
    return true;
}
