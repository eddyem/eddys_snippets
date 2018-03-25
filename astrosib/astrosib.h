/*
 *                                                                                                  geany_encoding=koi8-r
 * astrosib.h
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

/** \file simplescope.h
    \brief Construct a basic INDI telescope device that simulates GOTO commands.
    \author Jasem Mutlaq

    \example simplescope.h
    A simple GOTO telescope that simulator slewing operation.
*/
#pragma once

#include "inditelescope.h"

class AstroSib : public INDI::DefaultDevice{
  public:
    AstroSib();
    ~AstroSib();
    bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    void TimerHit();

  protected:
    bool Connect();
    bool Disconnect();

    bool Handshake();
    const char *getDefaultName();
    bool initProperties();
    bool updateProperties();

  private:
    enum {_ON, _OFF};
    // last ambient & mirror temperature values
    float ambtemp {0}, mirrtemp {0}, focusert{0};
    INumber TempN[3]; // mirror, ambient and focuser
    INumberVectorProperty TempNP;
    void TemperatureChk();
    // shutter leaves state
    enum {SHTR_OPENED, SHTR_CLOSED};
    bool ShtrInProgress{false};
    ISwitch ShtrsS[2]; // roof shutters switch
    ISwitchVectorProperty ShtrsSP;
    bool ShutterSetState(int newstate);
    void ShutterChk();
    // focuser current/relative/absolute position
    bool FocInProgress{false};
    int foccur{0}, focabs{0};
    INumber FocuserN[3];
    INumberVectorProperty FocuserNP, FocuserNPcur;
    bool FocuserSetVal(int focval);
    void FocuserChk();
    // mirror cooler
    ISwitch CoolerS[2];
    ISwitchVectorProperty CoolerSP;
    // cooler auto
    ISwitch ACoolerS[2];
    ISwitchVectorProperty ACoolerSP;
    bool CoolerSetState(int newstate);
    bool ACoolerSetState(int newstate);
    void CoolerChk();
    // heater
    ISwitch AHeaterS[2];
    ISwitchVectorProperty AHeaterSP;
    INumber PHeaterN;
    INumberVectorProperty PHeaterNP;
    bool AHeaterSetState(int newstate);
    bool HeaterSetPower(int pwr);
    void HeaterChk();
};
