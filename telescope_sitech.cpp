/*******************************************************************************
 Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

/*
localport
192.168.51.122
7624
192.168.51.51
1956
1929
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <memory>

#include "telescope_sitech.h"
#include "indicom.h"


// We declare an auto pointer to ScopeSiTech.
std::unique_ptr<ScopeSiTech> telescope_sitech(new ScopeSiTech());

#define	GOTO_RATE	5				/* slew rate, degrees/5 */
#define	SLEW_RATE	0.5				/* slew rate, degrees/s */
#define FINE_SLEW_RATE  0.1                             /* slew rate, degrees/s */
#define SID_RATE	0.004178			/* sidereal rate, degrees/s */

#define GOTO_LIMIT      5.5                             /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT      1                               /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */
#define FINE_SLEW_LIMIT 0.5                             /* Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees */

#define	POLLMS		250				/* poll period, ms */

#define RA_AXIS         0
#define DEC_AXIS        1
#define GUIDE_NORTH     0
#define GUIDE_SOUTH     1
#define GUIDE_WEST      0
#define GUIDE_EAST      1

#define MIN_AZ_FLIP     180
#define MAX_AZ_FLIP     200

#define MYSCOPE "SiTechScopeA"
#define NAD -99999999.99

#define STELLAR_DAY 86164.098903691
#define TRACKRATE_SIDEREAL ((360.0 * 3600.0) / STELLAR_DAY)
#define SOLAR_DAY 86400
#define TRACKRATE_SOLAR ((360.0 * 3600.0) / SOLAR_DAY)
#define TRACKRATE_LUNAR 14.511415


void ISPoll(void *p);
void DansLog(char * stg);


void ISGetProperties(const char *dev)
{
        telescope_sitech->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        telescope_sitech->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        telescope_sitech->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        telescope_sitech->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}
void ISSnoopDevice (XMLEle *root)
{
   telescope_sitech->ISSnoopDevice(root);
}
ScopeSiTech::ScopeSiTech()
{
    //ctor
    currentRA=0;
    currentDEC=60;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");   

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT,4);
    setTelescopeConnection(CONNECTION_TCP);

    /* initialize random seed: */
      srand ( time(NULL) );
      printf("Hellothere!\n");
}

ScopeSiTech::~ScopeSiTech()
{

}

const char * ScopeSiTech::getDefaultName()
{
    return (char *) MYSCOPE;
}
//192.168.51.511956 
bool ScopeSiTech::initProperties()
{
//    DansLog((char * ) "InitProperties");
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[RA_AXIS], "GUIDE_RATE_WE", "W/E Rate", "%g", 0, 1, 0.1, 0.3);
    IUFillNumber(&GuideRateN[DEC_AXIS], "GUIDE_RATE_NS", "N/S Rate", "%g", 0, 1, 0.1, 0.3);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0, IPS_IDLE);


    IUFillSwitch(&SlewRateS[SLEW_GUIDE], "SLEW_GUIDE", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_CENTERING], "SLEW_CENTERING", "Centering", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_FIND], "SLEW_FIND", "Find", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_MAX], "SLEW_MAX", "Max", ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 4, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Tracking Mode
    IUFillSwitch(&TrackModeS[TRACK_SIDEREAL], "TRACK_SIDEREAL", "Sidereal", ISS_OFF);
    IUFillSwitch(&TrackModeS[TRACK_SOLAR], "TRACK_SOLAR", "Solar", ISS_OFF);
    IUFillSwitch(&TrackModeS[TRACK_LUNAR], "TRACK_LUNAR", "Lunar", ISS_OFF);
    IUFillSwitch(&TrackModeS[TRACK_CUSTOM], "TRACK_CUSTOM", "Custom", ISS_OFF);
    IUFillSwitchVector(&TrackModeSP, TrackModeS, 4, getDeviceName(), "TELESCOPE_TRACK_MODE", "Track Mode", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Custom Tracking Rate
    IUFillNumber(&TrackRateN[0],"TRACK_RATE_RA","RA (arcsecs/s)","%.6f",-16384.0, 16384.0, 0.000001, 15.041067);
    IUFillNumber(&TrackRateN[1],"TRACK_RATE_DE","DE (arcsecs/s)","%.6f",-16384.0, 16384.0, 0.000001, 0);
    IUFillNumberVector(&TrackRateNP, TrackRateN,2,getDeviceName(),"TELESCOPE_TRACK_RATE","Track Rates", MAIN_CONTROL_TAB, IP_RW,60,IPS_IDLE);

     // Let's simulate it to be an F/7.5 120mm telescope
    ScopeParametersN[0].value = 120;
    ScopeParametersN[1].value = 900;
    ScopeParametersN[2].value = 120;
    ScopeParametersN[3].value = 900;

    TrackState=SCOPE_IDLE;
//enum TelescopeParkData  { PARK_NONE, PARK_RA_DEC, PARK_AZ_ALT, PARK_RA_DEC_ENCODER, PARK_AZ_ALT_ENCODER };
  //  SetParkDataType(PARK_RA_DEC);

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);
//    DansLog((char * ) "InitProperties--Done");

    return true;
}

void ScopeSiTech::ISGetProperties (const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

    if(isConnected())
    {
        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);
        defineNumber(&GuideRateNP);
//        defineNumber(&EqPENV);
//        defineSwitch(&PEErrNSSP);
//        defineSwitch(&PEErrWESP);
    }


    return;
}

bool ScopeSiTech::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        if (IsTracking)
        {
//            IUResetSwitch(&TrackModeSP);
//            TrackModeS[TRACK_SIDEREAL].s = ISS_ON;
            TrackModeSP.s = IPS_OK;
        }
        else
        {
//            IUResetSwitch(&TrackModeSP);
            TrackModeSP.s = IPS_IDLE;
        }

        defineSwitch(&TrackModeSP);
        defineNumber(&TrackRateNP);

        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);
        defineNumber(&GuideRateNP);

    }
    else
    {
        deleteProperty(TrackModeSP.name);
        deleteProperty(TrackRateNP.name);

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.name);
    }

    return true;
}
#define MAXSOCKETBUFLEN 512
#define SITECHTIMEOUT 3
static char SendBuf[MAXSOCKETBUFLEN];
static char RcvBuf[MAXSOCKETBUFLEN];
static char myLogStg[1000];
static ofstream myDbgFile;
void DansLog(char * stg)
{
    if(true)
    {
    myDbgFile.open("/tmp/DansDebug",ios::app );
    myDbgFile << stg;
    myDbgFile << "\n";
    myDbgFile.close();
    }
}
bool ScopeSiTech::Handshake()
{
    int rc=0, nbytes_written=0, nbytes_read=0;
//    DansLog((char * ) "StartHandshake");
    memset(SendBuf,'\0',MAXSOCKETBUFLEN);
    memset(RcvBuf,'\0',MAXSOCKETBUFLEN);
    strncpy(SendBuf, "ReadScopeStatus\r\n", MAXSOCKETBUFLEN);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", SendBuf);
//    sprintf(myLogStg,"inHandShk. SendBuf=%s", SendBuf);DansLog(myLogStg);

    if ( (rc = tty_write_string(PortFD, SendBuf, &nbytes_written)) != TTY_OK)
    {
//        sprintf(myLogStg,"inHandShk.WriteError. nbyteswritted=%d", nbytes_written);DansLog(myLogStg);
        DEBUG(INDI::Logger::DBG_ERROR, "Error writing to the SiTechExe TCP server.");
        return false;
    }
//    sprintf(myLogStg,"inHandShk.Normal. nbyteswritted=%d rc=%d", nbytes_written, rc);DansLog(myLogStg);

    int rcr = tty_read_section(PortFD, RcvBuf, '\n', SITECHTIMEOUT, &nbytes_read);

//    if ( (rc == rcr ) != TTY_OK)
    if ( rcr != TTY_OK)
    {
//        sprintf(myLogStg,"inHandShk.ReadError. nbytesread=%d rcr=%d rs=%s", nbytes_read,rcr,RcvBuf);DansLog(myLogStg);
        DEBUG(INDI::Logger::DBG_ERROR, "Error reading from SiTechExe TCP server.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", RcvBuf);

//    sprintf(myLogStg,"inHandShk.B4SetupVars. nbytesread=%d rcr=%d rs=%s", nbytes_read,rcr,RcvBuf);DansLog(myLogStg);
    SetUpVarsFromReturnString(RcvBuf, true);
    sprintf(myLogStg,"inHandShkAfterSetupVars. RA=%f rcvBuf=%s",currentRA,RcvBuf);DansLog(myLogStg);
    if (!IsCommunicatingWithController)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "No Communication From SiTechExe To Controller!");
    }
    else if(IsInBlinky)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Controller in manual (Blinky) mode!");
    }
    else
    {
        char tst[66];
        sprintf(tst," SiTech is in %d mode!", TrackState );
        DEBUG(INDI::Logger::DBG_ERROR, tst );
    }

    SetParked(IsParked);
    SetTimer(POLLMS);

    return true;
}
char * ScopeSiTech::GetStringFromSerial(char * Send)
{
    int rc=0, nbytes_written=0, nbytes_read=0;
    memset(SendBuf,'\0',MAXSOCKETBUFLEN);
    memset(RcvBuf,'\0',MAXSOCKETBUFLEN);
    strncpy(SendBuf, Send, MAXSOCKETBUFLEN);
    strncat(SendBuf,"\r\n",MAXSOCKETBUFLEN);
//    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", SendBuf);
//    sprintf(myLogStg,"GSFS SendStrg=[%s]",SendBuf);DansLog((char*)myLogStg);
//TTY_OK=0, TTY_READ_ERROR=-1, TTY_WRITE_ERROR=-2, TTY_SELECT_ERROR=-3, TTY_TIME_OUT=-4,
//TTY_PORT_FAILURE=-5, TTY_PARAM_ERROR=-6, TTY_ERRNO = -7};
//    int rcr = tty_read(PortFD, RcvBuf, 1, 0, &nbytes_read);//clear buff
//    sprintf(myLogStg,"GSFS Pre-NormalRead nbytsrd=%d rcr=%d RcvStrg=[%s]",nbytes_read,rcr,RcvBuf);DansLog((char*)myLogStg);

    if ( (rc = tty_write_string(PortFD, SendBuf, &nbytes_written)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error writing to the SiTechExe TCP server.");
        return NULL;
    }
    int rcr = tty_read_section(PortFD, RcvBuf, '\n', SITECHTIMEOUT, &nbytes_read);
//    if ( (rc == rcr) != TTY_OK)
    if ( rcr != TTY_OK)
    {
//        sprintf(myLogStg,"GSFS ErrRead nbytsrd=%d rc=%drcr=%d RcvStrg=%s",nbytes_read,rc,rcr,RcvBuf);DansLog((char*)myLogStg);
        char stsg[777];
        sprintf(stsg, "Error readingg from SiTechExe TCP server. Sent=%s nByts=%d errNum=%d",SendBuf, nbytes_read, rcr);
        DEBUG(INDI::Logger::DBG_ERROR, SendBuf);

        return NULL;
    }
//    sprintf(myLogStg,"GSFS FirstNormalRead nbytsrd=%d rc=%drcr=%d RcvStrg=[%s]",nbytes_read,rc,rcr,RcvBuf);DansLog((char*)myLogStg);
    if(nbytes_read > 0 && nbytes_read < 4)
    {
        rcr = tty_read_section(PortFD, RcvBuf, '\n', SITECHTIMEOUT, &nbytes_read);
        if ( rcr != TTY_OK)
        {
//            sprintf(myLogStg,"GSFS ErrRead2 nbytsrd=%d rc=%drcr=%d RcvStrg=[%s]",nbytes_read,rc,rcr,RcvBuf);DansLog((char*)myLogStg);
            DEBUG(INDI::Logger::DBG_ERROR, "Error2 reading from SiTechExe TCP server.");
            return NULL;
        }
    }
//    sprintf(myLogStg,"GSFS SecondNormalRead nbytsrd=%d rc=%drcr=%d RcvStrg=[%s]",nbytes_read,rc,rcr,RcvBuf);DansLog((char*)myLogStg);

//    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", RcvBuf);
    return RcvBuf;
}
static char MessageFromScope[1500];
static char ErrorMessage[1500];
static int LastScopeStt = -1;
bool ScopeSiTech::SetUpVarsFromReturnString(char * ScopeAnswer, bool PrintBools)
{
    /* All numbers are separated by a ';'
     * First is an integer, with bits in it as follows:
     * (001) Bit  0 = true if initialized
     * (002) Bit  1 = true if tracking
     * (004) Bit  2 = true if slewing, or slew settling time
     * (008) Bit  3 = true if Parking
     * (016) Bit  4 = true if Parked
     * (032) Bit  5 = true if "looking east" (GEM scope)
     * (064) Bit  6 = true if either servo is in manual (blinky mode)
     * (128) Bit  7 = true if Comm to Controller is bad
     * (256) Bit  8 = true if Limit Switch + in Primary Axis Activated
     * (512) Bit  9 = true if Limit Switch - in Primary Axis Activated
     *(1024) Bit 10 = true if Limit Switch + in Secondary Axis Activated
     *(2048) Bit 11 = true if Limit Switch - in Secondary Axis Activated
     *(4096) Bit 12 = true if Home Switch Primary Axis Activated
     *(8192) Bit 13 = true if Home Switch Secondary Axis Activated
    *(16384) Bit 14 = true if HandPad Pan Mode
    *(32768) Bit 15 = true if HandPad Guide Mode
    * Next is RA (hours)
    * Next is Dec (Hours)
    * Next is Altitude (degs)
    * Next is Azimuth (degs)
    * Next is Primary axisPositionDegsPrimary
    * Next is Primary axisPositionDegsSecondary
    * Next is Scope Sidereal Time (hours).
    * Next is Scope Julian Day.
    * Next is ScopeTime (hours)
    * Next is a possible string message.
*/
    if(ScopeAnswer == NULL) return false;
    int Len = strlen(ScopeAnswer);
    if(Len < 3) return false;
    int ScopeStt = atoi(ScopeAnswer);
    IsInitialized = ((ScopeStt & 1) > 0);
    IsTracking = ((ScopeStt & 2) > 0);
    IsSlewing = ((ScopeStt & 4) > 0);
    IsParking = ((ScopeStt & 8) > 0);
    IsParked = ((ScopeStt & 16) > 0);
    IsLookingEast = ((ScopeStt & 32) > 0);
    IsInBlinky = ((ScopeStt & 64) > 0);
    IsCommunicatingWithController = ((ScopeStt & 128) == 0);

    if(PrintBools && LastScopeStt != ScopeStt)
    {
        sprintf(myLogStg,"ScopeStt=%d IsInit=%d IsTrack=%d IsSlew=%d IsParking=%d IsParked=%d IsLookingEast=%d IsInBlinky=%d IsComm=%d",
            ScopeStt, IsInitialized, IsTracking, IsSlewing, IsParking, IsParked, IsLookingEast, IsInBlinky, IsCommunicatingWithController );
        DansLog((char*)myLogStg);
    }
    char * ptr = ScopeAnswer;
    int NextSemColonLoc=0;
    while(NextSemColonLoc < Len)
    {
       if(ScopeAnswer[NextSemColonLoc] == ';') break;
       NextSemColonLoc++;
    }
    ptr = ScopeAnswer + NextSemColonLoc;
    Len --; if (Len < 1) Len = 1;
    size_t offset = 0;
    long p_sa = (long) (ptr - ScopeAnswer);
//    sprintf(myLogStg,"SetErrFrmRetStg NextSemColonLoc=%d Len=%d p-sa=%ld scpanwr=%s",NextSemColonLoc,Len,p_sa,ptr);DansLog((char*)myLogStg);
    p_sa = (long) (ptr - ScopeAnswer);
    if(ptr - ScopeAnswer < Len) currentRA = stod(ptr + 1, &offset); ptr += offset + 1;
//    sprintf(myLogStg,"SetErrFrmRetStg NextSemColonLoc=%d Len=%d p-sa=%ld ra=%f scpanwr=%s",NextSemColonLoc,Len,p_sa,currentRA,ptr);DansLog((char*)myLogStg);
    if(ptr - ScopeAnswer < Len) currentDEC = stod(ptr + 1, &offset); ptr += offset + 1;
//    sprintf(myLogStg,"SetErrFrmRetStg NextSemColonLoc=%d Len=%d p-sa=%ld de=%f scpanwr=%s",NextSemColonLoc,Len,p_sa,currentDEC,ptr);DansLog((char*)myLogStg);
    if(ptr - ScopeAnswer < Len) currentAlt = stod(ptr+1, &offset); ptr += offset + 1;
//    sprintf(myLogStg,"SetErrFrmRetStg NextSemColonLoc=%d Len=%d p-sa=%ld al=%f scpanwr=%s",NextSemColonLoc,Len,p_sa,currentAlt,ptr);DansLog((char*)myLogStg);
    if(ptr - ScopeAnswer < Len) currentAz = stod(ptr+1, &offset); ptr += offset + 1;
    if(ptr - ScopeAnswer < Len) axisPositionDegsPrimary = stod(ptr+1, &offset); ptr += offset + 1;
    if(ptr - ScopeAnswer < Len) axisPositionDegsSecondary = stod(ptr+1, &offset); ptr += offset + 1;
    if(ptr - ScopeAnswer < Len) scopeSiderealTime = stod(ptr+1, &offset); ptr += offset + 1;
    if(ptr - ScopeAnswer < Len) scopeJulianDay = stod(ptr+1, &offset); ptr += offset + 1;
    if(ptr - ScopeAnswer < Len) scopeTime = stod(ptr+1,  &offset); ptr += offset + 1;
//    sprintf(myLogStg,"SetErrFrmRetStg NextSemColonLoc=%d Len=%d p-sa=%ld ScpTm=%f scpanwr=%s",NextSemColonLoc,Len,p_sa,scopeTime,ptr);DansLog((char*)myLogStg);
    if(strlen(ptr) > 3)strncpy(MessageFromScope,ptr,499);
//    sprintf(myLogStg,"SetErrFrmRetStg NextSemColonLoc=%d Len=%d p-sa=%ld ScpTm=%f Mess=%s",NextSemColonLoc,Len,p_sa,scopeTime,MessageFromScope);DansLog((char*)myLogStg);
  //enum TelescopeStatus { SCOPE_IDLE, SCOPE_SLEWING, SCOPE_TRACKING, SCOPE_PARKING, SCOPE_PARKED };
    if (IsParking) TrackState = SCOPE_PARKING;
    else if (IsParked) TrackState = SCOPE_PARKED;
    else if(IsSlewing) TrackState = SCOPE_SLEWING;
    else if(IsTracking) TrackState = SCOPE_TRACKING;
    else TrackState = SCOPE_IDLE;

    if(PrintBools && LastScopeStt != ScopeStt)
    {
        sprintf(myLogStg,"TrackState=%d", TrackState );
        DansLog((char*)myLogStg);
    }
    LastScopeStt = ScopeStt;
    return true;
}
/*
bool ScopeSiTech::Disconnect()
{

    DEBUG(INDI::Logger::DBG_SESSION, "Telescope SiTech is offline.");
}
*/
static bool inReadScopeStatus = false;
bool ScopeSiTech::ReadScopeStatus()
{
    if(inReadScopeStatus) return false;
    inReadScopeStatus = true;
    SetUpVarsFromReturnString(GetStringFromSerial((char *)"ReadScopeStatus"), true);

    static struct timeval ltv;
    struct timeval tv;
    double dt=0, da_ra=0, da_dec=0, dx=0, dy=0, ra_guide_dt=0, dec_guide_dt=0;
    static double last_dx=0, last_dy=0;
    int nlocked, ns_guide_dir=-1, we_guide_dir=-1;
    char RA_DISP[64], DEC_DISP[64], RA_GUIDE[64], DEC_GUIDE[64], RA_PE[64], DEC_PE[64], RA_TARGET[64], DEC_TARGET[64];

    if (IsTracking)
    {
        IUResetSwitch(&TrackModeSP);
        TrackModeSP.s = IPS_OK;
    }
    else
    {
        IUResetSwitch(&TrackModeSP);
        TrackModeSP.s = IPS_IDLE;
    }
    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday (&tv, NULL);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
    ltv = tv;

    fs_sexa(RA_DISP, fabs(dx), 2, 3600 );
    fs_sexa(DEC_DISP, fabs(dy), 2, 3600 );

    fs_sexa(RA_GUIDE, fabs(ra_guide_dt), 2, 3600 );
    fs_sexa(DEC_GUIDE, fabs(dec_guide_dt), 2, 3600 );

    fs_sexa(RA_TARGET, targetRA, 2, 3600);
    fs_sexa(DEC_TARGET, targetDEC, 2, 3600);


    if ((dx!=last_dx || dy!=last_dy || ra_guide_dt || dec_guide_dt))
    {
        last_dx=dx;
        last_dy=dy;
        //DEBUGF(INDI::Logger::DBG_DEBUG, "dt is %g\n", dt);
        DEBUGF(INDI::Logger::DBG_DEBUG, "RA Guide Correction (%g) %s -- Direction %s", ra_guide_dt, RA_GUIDE, ra_guide_dt > 0 ? "East" : "West");
        DEBUGF(INDI::Logger::DBG_DEBUG, "DEC Guide Correction (%g) %s -- Direction %s", dec_guide_dt, DEC_GUIDE, dec_guide_dt > 0 ? "North" : "South");
    }

//    if (ns_guide_dir != -1 || we_guide_dir != -1)
  //      IDSetNumber(&EqPENV, NULL);


    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, currentRA, 2, 3600);
    fs_sexa(DecStr, currentDEC, 2, 3600);

    DEBUGF(DBG_SCOPE, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(currentRA, currentDEC);
    inReadScopeStatus = false;
    return true;
}

bool ScopeSiTech::Goto(double r,double d)
{
    if (IsParked)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

   ln_equ_posn lnradec;
   lnradec.ra = (currentRA * 360) / 24.0;
   lnradec.dec =currentDEC;

   char sStr[128];
   sprintf(sStr,"GoTo %f %f",r,d);
   SetUpVarsFromReturnString(GetStringFromSerial((char *)sStr), true);

   EqNP.s    = IPS_BUSY;

   DEBUGF(INDI::Logger::DBG_SESSION,"Slewing to RA: %s - DEC: %s", RAStr, DecStr);
   return true;
}
bool ScopeSiTech::Sync(double ra, double dec)
{
char cStr[1164];
    sprintf(cStr,"Sync %f %f 0",ra,dec);//the zero is call up the sitech init window.  1 = offset instantaneous, 2 = load calibration instantaneos
    SetUpVarsFromReturnString(GetStringFromSerial((char *)cStr), true);
    if(strstr(MessageFromScope,"Accepted") == NULL)
    {
        sprintf(ErrorMessage, "Sync is rejected. Reason=%s",MessageFromScope);
        DEBUG(INDI::Logger::DBG_SESSION,ErrorMessage);
        return false;
    }
    currentRA  = ra;
    currentDEC = dec;
    DEBUG(INDI::Logger::DBG_SESSION,"Sync is successful.");
    EqNP.s    = IPS_OK;
    return true;
}

bool ScopeSiTech::Park()
{
    SetUpVarsFromReturnString(GetStringFromSerial((char *)"Park 0"), true);//the zero is regular park, could do 1 or 2 as well.
    if(strstr(MessageFromScope,"Park") == NULL)
    {
        sprintf(ErrorMessage, "Park is rejected. Reason=%s",MessageFromScope);
        DEBUG(INDI::Logger::DBG_SESSION,ErrorMessage);
        return false;
    }
    sprintf(ErrorMessage, "ParkOk. Mess=%s",MessageFromScope);
    DEBUG(INDI::Logger::DBG_SESSION, ErrorMessage);
    return true;
}

bool ScopeSiTech::UnPark()
{
    if (INDI::Telescope::isLocked())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Cannot unpark mount when dome is locking. See: Dome parking policy, in options tab");
        return false;
    }
    if (!IsParked)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Cannot unpark mount when already unparked");
        return false;
    }

    SetUpVarsFromReturnString(GetStringFromSerial((char *)"UnPark"), true);
    if(strstr(MessageFromScope,"UnPark") == NULL)
    {
        sprintf(ErrorMessage, "UnPark is rejected. Reason=%s",MessageFromScope);
        DEBUG(INDI::Logger::DBG_SESSION,ErrorMessage);
        return false;
    }
    sprintf(ErrorMessage, "UnParkOk. Mess=%s",MessageFromScope);
    DEBUG(INDI::Logger::DBG_SESSION, ErrorMessage);
    SetParked(false);
    return true;
}
bool ScopeSiTech::setSiTechTracking(bool enable, bool isSidereal, double raRate, double deRate)
{
    int on      = enable ? 1 : 0;
    int ignore  = isSidereal ? 1 : 0;

    char pCMD[MAXRBUF];

    snprintf(pCMD,MAXRBUF, "SetTrackMode %d %d %f %f", on, ignore, raRate, deRate);
    SetUpVarsFromReturnString(GetStringFromSerial((char *)pCMD), true);

    if(strstr(MessageFromScope, "SetTrackMode") == NULL) return false;
    return true;
}

bool ScopeSiTech::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if(strcmp(dev,getDeviceName())==0)
    {
        // Tracking Rate
        if (!strcmp(name, TrackRateNP.name))
        {
            IUUpdateNumber(&TrackRateNP, values, names, n);
            if (currentTrackMode != TRACK_CUSTOM)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Can only set tracking rate if track mode is custom.");
                TrackRateNP.s = IPS_ALERT;
            }
            else
            {
                setSiTechTracking(true, false, TrackRateN[RA_AXIS].value, TrackRateN[DEC_AXIS].value);
                if(strstr(MessageFromScope,"SetTrackMode") == NULL)
                {
                    sprintf(ErrorMessage, "SetTrackMode is rejected. Reason=%s",MessageFromScope);
                    DEBUG(INDI::Logger::DBG_SESSION,ErrorMessage);
                    TrackRateNP.s = IPS_ALERT;
                }
                else
                {
                    TrackRateNP.s = IPS_OK;
                    sprintf(ErrorMessage, "SetTrackMode OK. Mess=%s",MessageFromScope);
                    DEBUG(INDI::Logger::DBG_SESSION, ErrorMessage);
                }
            }

            IDSetNumber(&TrackRateNP, NULL);
            return true;
        }

         if(strcmp(name,"GUIDE_RATE")==0)
         {
             IUUpdateNumber(&GuideRateNP, values, names, n);
             GuideRateNP.s = IPS_OK;
             IDSetNumber(&GuideRateNP, NULL);
             return true;
         }

         if (!strcmp(name,GuideNSNP.name) || !strcmp(name,GuideWENP.name))
         {
             processGuiderProperties(name, values, names, n);
             return true;
         }

    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}
bool ScopeSiTech::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // Tracking Mode
        if (!strcmp(TrackModeSP.name, name))
        {
            int previousTrackMode = IUFindOnSwitchIndex(&TrackModeSP);

            IUUpdateSwitch(&TrackModeSP, states, names, n);

            currentTrackMode = IUFindOnSwitchIndex(&TrackModeSP);

            // Engage tracking?
            bool enable     = (currentTrackMode != -1);
            bool isSidereal = (currentTrackMode == TRACK_SIDEREAL);
            double dRA=0, dDE=0;
            if (currentTrackMode == TRACK_SOLAR)
                dRA = TRACKRATE_SOLAR;
            else if (currentTrackMode == TRACK_LUNAR)
                dRA = TRACKRATE_LUNAR;
            else if (currentTrackMode == TRACK_CUSTOM)
            {
                dRA = TrackRateN[RA_AXIS].value;
                dDE = TrackRateN[DEC_AXIS].value;
            }

            setSiTechTracking(enable, isSidereal, dRA, dDE);

            if(strstr(MessageFromScope,"SetTrackMode") == NULL)
            {
                sprintf(ErrorMessage, "SetTrackMode is rejected. Reason=%s",MessageFromScope);
                DEBUG(INDI::Logger::DBG_SESSION,ErrorMessage);
                TrackModeSP.s = IPS_ALERT;
                if (previousTrackMode != -1)
                {
                    IUResetSwitch(&TrackModeSP);
                    TrackModeS[previousTrackMode].s = ISS_ON;
                }
            }
            else
            {
                sprintf(ErrorMessage, "SetTrackMode OK. Mess=%s",MessageFromScope);
                DEBUG(INDI::Logger::DBG_SESSION, ErrorMessage);
                if(IsTracking) TrackModeSP.s = IPS_OK; else TrackModeSP.s = IPS_IDLE;
            }

            IDSetSwitch(&TrackModeSP, NULL);
            return true;
        }

        // Slew mode
        if (!strcmp (name, SlewRateSP.name))
        {

          if (IUUpdateSwitch(&SlewRateSP, states, names, n) < 0)
              return false;

          SlewRateSP.s = IPS_OK;
          IDSetSwitch(&SlewRateSP, NULL);
          return true;
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev,name,states,names,n);
}

bool ScopeSiTech::Abort()
{
    SetUpVarsFromReturnString(GetStringFromSerial((char *)"Abort"), true);//Stop all motion.
    if(strstr(MessageFromScope,"Abort") == NULL)
    {
        sprintf(ErrorMessage, "Abort is rejected. Reason=%s",MessageFromScope);
        DEBUG(INDI::Logger::DBG_SESSION,ErrorMessage);
        return false;
    }
    sprintf(ErrorMessage, "Abort OK. Mess=%s",MessageFromScope);
    IUResetSwitch(&TrackModeSP);
    TrackModeSP.s = IPS_IDLE;
    DEBUG(INDI::Logger::DBG_SESSION, ErrorMessage);
    return true;
}


bool ScopeSiTech::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    return true;
}

bool ScopeSiTech::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    return true;
}

IPState ScopeSiTech::GuideNorth(float ms)
{
    guiderNSTarget[GUIDE_NORTH] = ms;
    guiderNSTarget[GUIDE_SOUTH] = 0;
    return IPS_BUSY;
}

IPState ScopeSiTech::GuideSouth(float ms)
{
    guiderNSTarget[GUIDE_SOUTH] = ms;
    guiderNSTarget[GUIDE_NORTH] = 0;
    return IPS_BUSY;

}

IPState ScopeSiTech::GuideEast(float ms)
{

    guiderEWTarget[GUIDE_EAST] = ms;
    guiderEWTarget[GUIDE_WEST] = 0;
    return IPS_BUSY;

}

IPState ScopeSiTech::GuideWest(float ms)
{
    guiderEWTarget[GUIDE_WEST] = ms;
    guiderEWTarget[GUIDE_EAST] = 0;
    return IPS_BUSY;

}

bool ScopeSiTech::SetCurrentPark()
{
    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);

    return true;
}

bool ScopeSiTech::SetDefaultPark()
{
    // By default set RA to HA
    SetAxis1Park(ln_get_apparent_sidereal_time(ln_get_julian_from_sys()));

    // Set DEC to 90 or -90 depending on the hemisphere
    SetAxis2Park( (LocationN[LOCATION_LATITUDE].value > 0) ? 90 : -90);

    return true;

}

/**************************************************************************************
**
***************************************************************************************/

/**************************************************************************************
**
***************************************************************************************/
/**************************************************************************************
**
***************************************************************************************/

