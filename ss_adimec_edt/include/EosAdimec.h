/**
   This is the device controller class for the Adimec camera.

   It implements the following SCIP power-controller commands:

   INFO[]:       
            Lists all valid commands

   GETGAIN[]:
            Queries the camera for the current digital gain setting
            Note: Analog gain not supported by the Adimec 1600c
                  Ignore the analog gain commands in the manual.
            
   GETOFFSET[]:
            Queries the camera for the current output offset

   GETRGB[]:
            Queries the camera for the current white balance values, RGB format
  
   SETGAIN[strValue]:
            Sets the cameras digital gain to strValue. Valid values are 100-800
            Note: Analog gain not supported by the Adimec 1600c
                  Ignore the analog gain commands in the manual.
  
   SETOFFSET[strValue]:
            Sets the cameras output offset to strValue. Valid values are 0-4095
   
   SETRGB[strRedValue,strGreenValue,strBlueValue]:
            Sets the cameras white balance values. Valid values are 100-399, input order 
            Red,Green,Blue

   GETFP[]:
        Get frame period

   SETFP[nFpVal]:
        Set frame period (1-4000)

   GETIT[]:
        Get integration time

   SETIT[nItVal]:
        Set integration time (1-4000)

   
   SAVESETTINGS[]
            Saves the current camera configuration as the new camera default settings.

 */
#pragma once

#include <stdio.h>  
#include <fcntl.h>  
#include <stdarg.h>
#include <sys/timeb.h>


#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/bind.hpp>

#include <memory>
#include <functional>
#include <atomic>


extern "C" {
    //#include <edtinc.h>
        //#include <multicam.h>
}

#ifdef _BUILD_MQTT_
#include "EosDeviceMqtt2.h"
#else
#include "EosDevice.h"
#endif

#include "CamLinkComms.h"

typedef unsigned char BYTE;

/*Adimec message flags. These values are specific to the Adimec camera. The extra A is 
 * to deconflict the namespace with those in the EDT files*/
#define ANUL            0
#define ASTX            64
#define AETX            13
#define AACK            6
#define ANAK            21

#define TIMEOUTVAL      3

const int SERBUFSIZE=128;
/**
   This class implements the UIB power-distribution PCB interface
 */
class EosAdimec : public EosDevice
{

  public:
      struct AdimecValues
      {
          std::string strGain;
          std::string strOffset;
          std::string strOutputRes;
          std::string strRGB[3];
          std::string strFramePeriod;
          std::string strIntegrationTime;
      };

    // For configuration checking only.
    EosAdimec(const std::string& strConfigFile);

    /**
      Constructor for operational system
      Note: Arg order modified to avoid EosDevice constructor
      ambiguities.
      @param strConfigFile -- name of device configuration file
      @param strNamedPipeFromMain -- named pipe: EosMain-->EosDevice
      @param strNamedPioeToMain   -- named pipe: EosDevice-->EosMain
      @param nDeviceId -- SCIP device ID.
      @param ClientComms:  specifies ClientSocket or SerialPort comm object
     */
    EosAdimec(const std::string& strConfigFile,
                   const std::string& strNamedPipeFromMain,
                   const std::string& strNamedPipeToMain,
                   const int nDeviceId,
                   ClientComms::E_CLIENT_COMM_TYPE eClientCommType=ClientComms::eClientCommSerial);

    /**
      Constructor for development/testing purposes only... 
        @param nDeviceId -- SCIP device ID.
        @param strNamedPipeFromMain -- named pipe: EosMain-->EosDevice
        @param strNamedPioeToMain   -- named pipe: EosDevice-->EosMain
        @param strTcpClientIp -- Device IP address
        @param nTcpClientPort -- Device listening port
        @param bUseExistingNamedPipes true to use existing pipes if available.
        @param bCommandLineMode -- set to true for command-line/debug mode.
        @param ClientComms:  specifies ClientSocket or SerialPort comm object
     */
    EosAdimec(const int nDeviceId,
              const std::string& strNamedPipeFromMain,
              const std::string& strNamedPipeToMain,
              const std::string& strTcpClientIp,
              const int& nTcpClientPort,
              const bool bUseExistingNamedPipes=true,
              const bool bCommandLineMode=false,
              ClientComms::E_CLIENT_COMM_TYPE eClientCommType=ClientComms::eClientCommSerial);

  /**  Do-nothing constructor for unit-testing, etc. */
  EosAdimec(void);

  /**  Standard destructor */
  virtual ~EosAdimec();


  /** Safety clamp on "leftover" buffer so as to 
      guarantee that it never gets too big. */
  static const size_t MAX_LEFTOVER_CHARS=256;  /**  Max# chars to carry over to next read. */
  static const size_t MAX_BUFFER_SIZE=16384;   /**  Max allowable input command buffer size. */
  
  /**
     A "do-nothing" stub for this device. 
     @return returns 0
   */
  virtual int DeviceSafeMode(void); 
  
  // #################### UNIT TESTING BEGIN ##########################
  /**
     Generic command parser unit test
     @param vStrCmd -- a vector of command strings (output)
     @param pStrGenericCommand -- a string containing 1 or more commands (input)
     @return int -- number of commands found in pStrGenericCommand
   */
  int UnitTestParseGenericCommandBase(std::vector<std::string>& vStrCmd,
                                      std::string* pStrGenericCommand);

  /**
     Generic-command add unit test
     @param vStrIn -- vector of command strings
   */
  void UnitTestAddGenericCommand(const std::vector<std::string>& vStrIn);

  /** 
    Command-translation unit test
  */
  int UnitTestTranslateGenericCommand(void);

  /**
     "Custom" function unit test
     @param vStrIn -- vector of command strings
   */
  int UnitTest_FptrCustom(const std::vector<std::string> vStrIn);

  
  /** Unit-test-only command queue 1 (primary) */
  std::vector<std::vector<unsigned char> > UnitTestCircBuf1Commands(void);
  /** Unit-test-only command queue 2 (secondary) */
  std::vector<std::vector<unsigned char> > UnitTestCircBuf2Commands(void);

  // ###################### UNIT TESTING END ###########################

  protected:
  bool m_bSaveSettingsOnExit;
  
  // ##########################################################
  // #### Begin pure virtual fcns inherited from EosDevice. ###
  // ##########################################################

  /**
     Customize per the specific device
     Create a command lookup table for the eospower device. 
  */
  virtual void InitCommandTemplate(void);

  /**
     A do-nothing stub for this device. 
   */
  virtual void InitializeDevice(void);
  
  /**
     Send all device commands in the command queue
     to the device. 
     @return 1 if commands can be sent; 0 otherwise
  */
  virtual int SendQueuedDeviceCommands(void);

  /**
     A "do-nothing" stub for this device 
  */
  virtual int SendDeviceCommand(void);

  /**
      Convert SCIP commands to power-board commands. 
      @return 1 if commands can be translated
  */
  virtual int TranslateGenericCommand(void);

  /**
     Convert power-board response messages to SCIP generic
     response messages. 
     @return int always 0 
  */
  virtual int TranslateDeviceResponse(void);

  // ########## Primary SPAC commands begin ##########


  /**
     Send a probe message -- if the device is ready for use, it will
     send back a DEVICE_UP[] response.
     @param vStrArgsIn -- a vector of tokens/strings (SCIP command + args)
     @return int 0 if valid command, -1 otherwise 
  */
  int _FptrProbe(const std::vector<std::string>& vStrArgs);

  /**
     Disable device-probe mode, if necessary
     @param vStrArgsIn -- a vector of tokens/strings (SCIP command + args)
     @return int 0 if valid command, -1 otherwise 
   */
  int _FptrDisableProbe(const std::vector<std::string>& vStrArgs);

  /**
     Force a serial-port reconnect
     @param vStrArgsIn -- 1 element vector (cmd name, no args)
     @return int 0.
   */
  int _FptrReconnect(const std::vector<std::string>& vStrArgs);

  /*
    Set the camera to its factory defaults for its current operation.
    Will not power back up with factory defaults.
   */
  int _FptrLoadFactoryDefaults(const std::vector<std::string>& vStrArgs);

  /*
    Set the camera permanently back to its factory defaults (will
    power up with the factory defaults).
   */
  int _FptrRestoreFactoryDefaults(const std::vector<std::string>& vStrArgs);

  // Determine whether to save settings on exit. Default is no.
  // Enable saving settings on exit only with an arg of "y", "Y", or "1"
  int _FptrSaveSettingsOnExitMode(const std::vector<std::string> & vStrArgs);

  /**
   A wrapper call around _FptrPowerControl()
   Wrapper implemented so we can call RequestPowerStatus()
   after a call to _FptrPowerControl() 
   @param vStrArgsIn -- a vector of tokens/strings (SCIP command + args)
   @return int 0 if valid command, -1 otherwise 
  */
  int _FptrSetGainLevel(const std::vector<std::string>& vStrArgs);

  /**
     Calls EOSPower::RequestPowerStatus(void)
     @param vStrArgsIn -- a vector of tokens/strings (SCIP command + args)
     @return int 0 if valid command, -1 otherwise 
  */
  int _FptrSetOffsetLevel(const std::vector<std::string>& vStrArgs);

  /**
     CallsEOSPower::PowerControl
     @param vStrArgsIn -- a vector of tokens/strings (SCIP command + args)
                          first token is command, second is port#, third is 1/0 (on/off)
     @return int 0 if valid command, -1 otherwise 
  */
  int _FptrSetRGBLevel(const std::vector<std::string>& vStrArgs);

  /** 
      Calls EOSPower::TurnOffAllPower(void)
      @param vStrArgsIn -- a vector of tokens/strings (SCIP command + args)
                           First token is the command.
      @return int 0 if valid command, -1 otherwise 
  */
  int _FptrGetLevel(const std::vector<std::string>& vStrArgs);
  
  int _FptrListCommandInfo(const std::vector<std::string>& vStrArgs);
  
  int _FptrSaveSettings(const std::vector<std::string>& vStrArgs);
  
  int _FptrSetOutputResolution(const std::vector<std::string>& vStrArgs);

  int _FptrGetOutputResolution(const std::vector<std::string>& vStrArgs);

  // Get/Set frame period time.
  // Frame period time value (arg1) range 1->4000, units of 10/20 usec.
  // GETFP[],SETFP[nFpVal]
  int _FptrGetFramePeriod(const std::vector<std::string>& vStrArgs);
  int _FptrSetFramePeriod(const std::vector<std::string>& vStrArgs);

  // Get/Set integration time
  // Frame integration time (arg1) range 1->4000, units of 10/20 usec.
  // GETIT[],SETIT[nItVal]
  int _FptrGetIntegrationTime(const std::vector<std::string>& vStrArgs);
  int _FptrSetIntegrationTime(const std::vector<std::string>& vStrArgs);

  int _FptrGetTemperature(const std::vector<std::string>& vStrArgs);

  int _FptrGetPixelCorrect(const std::vector<std::string>& vStrArgs);
  int _FptrSetPixelCorrect(const std::vector<std::string>& vStrArgs);

  int _FptrSetAgCorrect(const std::vector<std::string>& vStrArgs);
  int _FptrGetAgCorrect(const std::vector<std::string>& vStrArgs);

  int _FptrSetImageFormat(const std::vector<std::string>& vStrArgs);
  int _FptrGetImageFormat(const std::vector<std::string>& vStrArgs);

  // ################################################
  // ###### BOOST FUNCTION POINTERS END #############
  // ################################################

  
  int m_nAdimecId;
  
  
  AdimecValues *m_pAdimec;
  
 /**
    Initialize the Adimec settings struct.
  */
 int InitAdimecStruct();

 /**
    Save the Adimec settings in the device.
  */
 int SaveSettings();


  /**
     Map the baud rate to an index #, 1->9600,2->19200,etc.
   */
  virtual int BaudRate2Id(int nBaudRate);
  
 /**
    Diagnostic fcn: Dump the Adimec device responses to the screen.
  */

 void PrintAdimecResponse(char buf[], int length);
 /**
    Make sure that the Adimec white-offset value is within range.
  */

 int ValidateOffset(int nTempGain);
 /**
    Make sure that the Adimec RGB settings are within range.
  */
 int ValidateRGB(const std::vector<std::string>& vStrArgs);

 // Make sure frame period setting is within range (1-4000) (units 20 or 40 usec)
 int ValidateFramePeriod(int nFramePeriod);

 // Make sure integrationtime setting is within range (1-4000) (units 20 or 40 usec)
 int ValidateIntegrationTime(int nIntegrationTime);

 /**
    Send a SCIP-format device response message to the main controller.
  */
 void ShipToSCIP(std::string strMsg,std::string strValue);

 /**
    Compose a SCIP-format device-response message.
  */
 int ComposeDevResp(const char buf[], int length, std::string& strResp);

 /**
    Get the Adimec gain, offset, or rgb levels.
  */
 int GetLevel(const char cValue,std::string& strCmd, std::string& strResp);

 /**
    Set the Adimec gain level.
  */
 int SetGainLevel(int nTempGain, std::string& strGainMsg,const std::string strGainValue);

 /**
    Set the Adimec offset level.
  */
 int SetOffsetLevel(const std::string& strNewValue,std::string& strOffsetCmd);

 /**
    Set the Adimec frame period (1-4000).
  */
 int SetFramePeriod(const std::string& strNewValue,std::string& strOffsetCmd);

 /**
    Set the Adimec Integration time (1-4000).
  */
 int SetIntegrationTime(const std::string& strNewValue,std::string& strOffsetCmd);

 /**
    Set the Adimec rgb levels.
  */
 int SetRGBLevel(const std::vector<std::string>& vStrArgs,std::string& strNewRGB);

 int SetResolution(int nTempResolution, std::string& strResolutionMsg, const std::string strResolutionValue);




/**
    Wrappers around the above Set commands.
    These take in the boost::function pointer _Fptr
    argements (std::vector<std::string> format) and convert
    to formats suitable for the above Set commands.
  */
 int HandleReconnect(const std::vector<std::string>& vStrArgs);

 int HandleGetLevel(const std::vector<std::string>& vStrArgs);

 int HandleSetGainLevel(const std::vector<std::string>& vStrArgs);

 int HandleSetRGBLevel(const std::vector<std::string>& vStrArgs);

 int HandleSetOffsetLevel(const std::vector<std::string>& vStrArgs);

 int HandleSetResolution(const std::vector<std::string>& vStrArgs);
 int HandleGetResolution(const std::vector<std::string>& vStrArgs);

 int HandleGetFramePeriod(const std::vector<std::string>& vStrArgs);
 int HandleSetFramePeriod(const std::vector<std::string>& vStrArgs);

 int HandleGetIntegrationTime(const std::vector<std::string>& vStrArgs);
 int HandleSetIntegrationTime(const std::vector<std::string>& vStrArgs);

 int HandleGetTemperature(const std::vector<std::string>& vStrArgs);

 int HandleGetPixelCorrect(const std::vector<std::string>& vStrArgs);
 int HandleSetPixelCorrect(const std::vector<std::string>& vStrArgs);

 int HandleGetAgCorrect(const std::vector<std::string>& vStrArgs);
 int HandleSetAgCorrect(const std::vector<std::string>& vStrArgs);

 int HandleGetImageFormat(const std::vector<std::string>& vStrArgs);
 int HandleSetImageFormat(const std::vector<std::string>& vStrArgs);

 // Calls Euresys clSerial fcns to force a reconnect.
 /// int ResetSerialConnection(void);

 int PdvSerialWrite(const std::string& strCmd);
 int PdvSerialRead(std::string& strResp);
 
 int m_nTimeOutCount;

 CamLinkCommsEdt* m_pSerialComms;
 
  /** Info read from Adimec configuration file. */
  EosSlaveCameraConfigInfo m_EosSensorConfigInfo;

  /**
     This creates the mapping between SCIP commands and device-controller
     command functions.  Each command fcn takes a vector of strings as
     an arg list.  The first vector element is the name of the SCIP command;
     the remaining vector elements are the SCIP command args. */
  std::map<std::string, 
    boost::function<int (EosAdimec*, const std::vector<std::string>&) > > 
    m_mapCommandTemplate;

};

//////////////////////////////////////////////////////////////////////
//  Error Codes
//////////////////////////////////////////////////////////////////////
#define CL_ERR_NO_ERR                           0
#define CL_ERR_BUFFER_TOO_SMALL                 -10001
#define CL_ERR_MANU_DOES_NOT_EXIST              -10002
#define CL_ERR_UNABLE_TO_OPEN_PORT              -10003
#define CL_ERR_PORT_IN_USE                      -10003
#define CL_ERR_TIMEOUT                          -10004
#define CL_ERR_INVALID_INDEX                    -10005
#define CL_ERR_INVALID_REFERENCE                -10006
#define CL_ERR_ERROR_NOT_FOUND                  -10007
#define CL_ERR_BAUD_RATE_NOT_SUPPORTED          -10008
#define CL_ERR_UNABLE_TO_LOAD_DLL               -10098
#define CL_ERR_FUNCTION_NOT_FOUND               -10099

// Baud rates
#define CL_BAUDRATE_9600                        1
#define CL_BAUDRATE_19200                       2
#define CL_BAUDRATE_38400                       4
#define CL_BAUDRATE_57600                       8
#define CL_BAUDRATE_115200                      16
#define CL_BAUDRATE_230400                      32
#define CL_BAUDRATE_460800                      64
#define CL_BAUDRATE_921600                      128

#ifdef __GNUC__
#define CLSEREMC_API
#define CLSEREMC_CC
#else

#ifdef CLSEREMC_EXPORTS
#define CLSEREMC_API __declspec(dllexport)
#else
#define CLSEREMC_API __declspec(dllimport)
#endif

#define CLSEREMC_CC __cdecl

#endif 

#ifdef __cplusplus
extern "C" {
#endif
        CLSEREMC_API int CLSEREMC_CC clSerialInit(unsigned long SerialIndex, void** SerialRefPtr);
        CLSEREMC_API int CLSEREMC_CC clSerialWrite(void* SerialRef, char* Buffer, unsigned long* BufferSize, unsigned long SerialTimeout);
        CLSEREMC_API int CLSEREMC_CC clSerialRead(void* SerialRef, char* Buffer, unsigned long* BufferSize, unsigned long SerialTimeout);
        CLSEREMC_API int CLSEREMC_CC clSerialClose(void* SerialRef);

        CLSEREMC_API int CLSEREMC_CC clGetManufacturerInfo(char* ManufacturerName, unsigned int* BufferSize, unsigned int *Version);
        CLSEREMC_API int CLSEREMC_CC clGetNumSerialPorts(unsigned int* NumSerialPorts);
        CLSEREMC_API int CLSEREMC_CC clGetSerialPortIdentifier(unsigned long SerialIndex, char* PortId, unsigned long* BufferSize);
        CLSEREMC_API int CLSEREMC_CC clGetSupportedBaudRates(void *SerialRef, unsigned int* BaudRates);
        CLSEREMC_API int CLSEREMC_CC clSetBaudRate(void* SerialRef, unsigned int BaudRate);
        CLSEREMC_API int CLSEREMC_CC clGetErrorText(int ErrorCode, char *ErrorText, unsigned int *ErrorTextSize);
        CLSEREMC_API int CLSEREMC_CC clGetNumBytesAvail(void *SerialRef, unsigned int *NumBytes);
        CLSEREMC_API int CLSEREMC_CC clFlushInputBuffer(void *SerialRef);
#ifdef __cplusplus
};
#endif

