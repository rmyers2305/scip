/**
 * This module communicates with the Adimec camera. Module is based off of EosPower. 
 * Talks to the camera through the EURESYS framegrabber and their libraries. 
 * Currently comunications to the camera MUST go through the EURESYS libraries(path to libclseremc.so must be defined) due to the way the 
 * framegrabber communicates with the camera. Currently supports getting/setting 
 * gain, digital offset, and rgb white balance values in the camera. 
 */

#include "EosAdimec.h"

using namespace std::placeholders;

// For configuration checking only.
EosAdimec::EosAdimec(const std::string& strConfigFile)
    :EosDevice(strConfigFile)
{
    m_strConfigFile=strConfigFile;

    // Set these to avoid destructor ugliness
    m_pAdimec=NULL;
    m_pEosBaseConfigInfo=NULL;
    m_bSaveSettingsOnExit=false;
    
    EosSlaveCameraConfiguration* pEosSensorConfiguration
        = new EosSlaveCameraConfiguration(strConfigFile);
    if(NULL==pEosSensorConfiguration)
    {
        EosException excp(1,"Could not allocate EosDomeConfiguration object",
                          __FILE__,__LINE__);
        throw excp;
    }
    
    m_EosSensorConfigInfo = pEosSensorConfiguration->ExtractConfigInfo();

    // RWM need this to access the proper device profile.
    m_strEosDeviceModel = m_EosSensorConfigInfo.strDeviceModel;

    bool bProfile=ExtractDeviceProfileBase();
    if(!bProfile)
    {
        EosException excp(1,"Could not extract device profile for "+m_strEosDeviceModel,
                          __FILE__,__LINE__);
        throw excp;
    }
    
    delete pEosSensorConfiguration;
    
    return;
}


// Operational system constructor -- will read/parse system config file
EosAdimec::EosAdimec(const std::string& strConfigFile,
                     const std::string& strNamedPipeFromMain,
                     const std::string& strNamedPipeToMain,
                     const int nDeviceId,
                     ClientComms::E_CLIENT_COMM_TYPE eClientCommType)
    : EosDevice(strConfigFile,strNamedPipeFromMain,strNamedPipeToMain,nDeviceId,
                eClientCommType)
{

    m_abShutdownFlag=false;
    
    EosSlaveCameraConfiguration*  pSensorConfig =
        new EosSlaveCameraConfiguration(strConfigFile);

    m_EosSensorConfigInfo = pSensorConfig->ExtractConfigInfo();

    delete pSensorConfig;

    m_strEosDeviceType=std::string("SS");

    m_strEosDeviceModel=m_EosSensorConfigInfo.strDeviceModel;

    m_nAdimecId=nDeviceId;

    // Use direct named-pipe comms for the base-class GETRANGES[] operations
    m_bDirectPipeComms=true;

    m_pAdimec=new EosAdimec::AdimecValues();

    // RWM new 2022/03/15
    m_pSerialComms = new CamLinkCommsEdt(m_EosSensorConfigInfo.nEdtChannel);
    m_pSerialComms->InitConnection();
    
    // If we fail to connect here, don't exit the program.
    // The remote client can send a RECONNECT[] command later to try again.
    
    // Set Camera Link Serial Port Baudrate 
    // Baud rate is now set in InitializeDevice()

    // tcpClientIp will be pressed into service as a serial-port name when
    // we connect directly to a serial port.
    // Enable this for serial-port mode: m_strTcpClientIp=m_EosDeviceUibConfigInfo.strSerialPort; 
    m_strTcpClientIp=m_EosSensorConfigInfo.strTcpClientIp; 
  
    m_nTcpClientPort=m_EosSensorConfigInfo.nTcpClientPort; 

    // m_nAutoRecoverMode enables auto-recovery procedures
    // for non-responsive devices.
    m_nAutoRecoverMode=m_EosSensorConfigInfo.nAutoRecoverMode;
    
    //m_circBufAdimecFirst.set_capacity(FIRST_QUEUE_SIZE);
    //m_circBufAdimecSecond.set_capacity(SECOND_QUEUE_SIZE);

    m_vucRawDeviceResponse.clear();
    m_vucProcessingDeviceResponse.clear();

    m_bSaveSettingsOnExit=true;
    
    InitCommandTemplate();

}

// ########################################################################
// ##### These constructors are for test/development purposes only ########
// ########################################################################

// A "do-not-very-much" constructor for unit-testing...
// Not for operational use
EosAdimec::EosAdimec() : EosDevice()
{
    m_strEosDeviceType=std::string("SS");

    //m_circBufAdimecFirst.set_capacity(FIRST_QUEUE_SIZE);
    //m_circBufAdimecSecond.set_capacity(SECOND_QUEUE_SIZE);

    m_vucRawDeviceResponse.clear();
    m_vucProcessingDeviceResponse.clear();
    
    m_bSaveSettingsOnExit=true;
    
    InitCommandTemplate();

    return;
  
}

// For test/development only -- not for operational use
EosAdimec::EosAdimec(const int nDeviceId,
                     const std::string& strNamedPipeFromMain,
                     const std::string& strNamedPipeToMain,
                     const std::string& strTcpClientIp,
                     const int& nTcpClientPort,
                     const bool bUseExistingNamedPipes,
                     const bool bCommandLineMode,
                     ClientComms::E_CLIENT_COMM_TYPE eCommType)
    : EosDevice(nDeviceId, strNamedPipeFromMain,strNamedPipeToMain,
                strTcpClientIp, nTcpClientPort,
                bUseExistingNamedPipes,bCommandLineMode,eCommType)
{
    std::cerr<<"Constructing EosAdimec "<<std::endl;
    
    //m_circBufAdimecFirst.set_capacity(FIRST_QUEUE_SIZE);
    //m_circBufAdimecSecond.set_capacity(SECOND_QUEUE_SIZE);
  
    m_strEosDeviceType=std::string("SS");
    m_nEosDeviceId=nDeviceId;

    m_bSaveSettingsOnExit=true;
    
    InitCommandTemplate();

    return;
  
}
//################################################
//#### End Test/Development-only constructors ####
//################################################



EosAdimec::~EosAdimec(void)
{
    /*Need to store the camera settings if SCIP gets power cycled*/
    /* Only if the save settings on exit flag is true */
    if(m_bSaveSettingsOnExit)
    {
        // Have gotten errors on the first "save-settings" tries
        // write, so do it twice here to be sure.
        std::string strResp;
        PdvSerialWrite("@SC");
        PdvSerialRead(strResp);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // A short pause to be sure
        PdvSerialWrite("@SC");
        PdvSerialRead(strResp);
    }
    
    return;
}

// For the power-controller device, a "do nothing" stub.
void EosAdimec::InitializeDevice(void)
{

    // This method is executed by EosDevice::RunBase()
    
    // This method is for device initialization stuff that can't go into the constructor.
    // Device profile information isn't available until EosDevice::RunBase() is launched.
    // i.e. m_EosDeviceProfile isn't populated until RunBase() is launched.
    // So initialization that depends on device profile info must be put here,
    // not in the constructor.

    // Give the device a little time after connecting before we proceed.
    ::usleep(100000);

    // For the Adimec, we want to make sure that the bit depth is
    // set appropriately.
    try
    {
        if(m_EosDeviceProfile.m_bBitDepthDefined)
        {
            int nBitDepth=m_EosDeviceProfile.m_nBitDepth;
            if((nBitDepth>0) && (nBitDepth<=32))
            {
                // Build a vStrArg vector for HandleSetResolution() below.
                std::vector<std::string> vStrArgs;
                vStrArgs.push_back("dummy_placeholder");
                vStrArgs.push_back(boost::lexical_cast<std::string>(nBitDepth));
                HandleSetResolution(vStrArgs);
            }
        }
    }
    catch(...)
    {
        // boost::lexical_cast can throw exceptions in exceptional cases,
        // so catch(...) here (and carry on) out of an abundance of caution.
    }

    return;
};

// Map SCIP commands to the device-controller functions
// that will be executed.
void EosAdimec::InitCommandTemplate(void)
{
    //Query valid commands
    m_mapCommandTemplate[EosCmd::INFO] =
        &EosAdimec::_FptrListCommandInfo;

    m_mapCommandTemplate[EosCmd::PROBE] =
        &EosAdimec::_FptrProbe;

    m_mapCommandTemplate[EosCmd::DISABLE_PROBE] =
        &EosAdimec::_FptrDisableProbe;

    m_mapCommandTemplate[EosCmd::PROBE_DISABLE] =
        &EosAdimec::_FptrDisableProbe;

    m_mapCommandTemplate[EosCmd::RECONNECT] =
        &EosAdimec::_FptrReconnect;

    m_mapCommandTemplate[EosCmd::LOADFACTORYSETTINGS] =
        &EosAdimec::_FptrLoadFactoryDefaults;

    m_mapCommandTemplate["LFS"] =
        &EosAdimec::_FptrLoadFactoryDefaults;
    m_mapCommandTemplate["LFD"] =
        &EosAdimec::_FptrLoadFactoryDefaults;

    m_mapCommandTemplate[EosCmd::RESTOREFACTORYSETTINGS] =
        &EosAdimec::_FptrRestoreFactoryDefaults;

    m_mapCommandTemplate[EosCmd::SAVESETTINGSONEXIT] =
        &EosAdimec::_FptrSaveSettingsOnExitMode;

    m_mapCommandTemplate["SSOE"] =
        &EosAdimec::_FptrSaveSettingsOnExitMode;
    
    // Set Adimec Digital FineGain Level
    m_mapCommandTemplate[EosCmd::SETGAIN] =
        &EosAdimec::_FptrSetGainLevel;

    // Set Adimec Output Offset
    m_mapCommandTemplate[EosCmd::SETOFFSET] =
        &EosAdimec::_FptrSetOffsetLevel;
  
    // Set Adimec White Balance Gain
    m_mapCommandTemplate[EosCmd::SETRGB] =
        &EosAdimec::_FptrSetRGBLevel;
    
    // Query Adimec Digital Fine Gain level
    m_mapCommandTemplate[EosCmd::GETGAIN] =
        &EosAdimec::_FptrGetLevel;
    
    //Query Adimec Output Offset
    m_mapCommandTemplate[EosCmd::GETOFFSET]=
        &EosAdimec::_FptrGetLevel;
    
    //Query Adimec White Balance
    m_mapCommandTemplate[EosCmd::GETRGB] =
        &EosAdimec::_FptrGetLevel;
    
    //Save Current Camera Settings
    m_mapCommandTemplate[EosCmd::SAVESETTINGS]=
        &EosAdimec::_FptrSaveSettings;
    
    //Save Current Camera Output Resolution
    m_mapCommandTemplate[EosCmd::SETOPR]=
        &EosAdimec::_FptrSetOutputResolution;
    
    m_mapCommandTemplate[EosCmd::GETOPR]=
        &EosAdimec::_FptrGetOutputResolution;

    // Set/get camera frame period (1-4000)
    m_mapCommandTemplate[EosCmd::SET_FRAMEPERIOD]=
        &EosAdimec::_FptrSetFramePeriod;
    
    m_mapCommandTemplate[EosCmd::GET_FRAMEPERIOD]=
        &EosAdimec::_FptrGetFramePeriod;

    // Set/get camera integration time (1-4000)
    m_mapCommandTemplate[EosCmd::SET_INTTIME]=
        &EosAdimec::_FptrSetIntegrationTime;
    
    m_mapCommandTemplate[EosCmd::GET_INTTIME]=
        &EosAdimec::_FptrGetIntegrationTime;

    m_mapCommandTemplate["GETTEMP"]=
        &EosAdimec::_FptrGetTemperature;
    m_mapCommandTemplate["GET_TEMP"]=
        &EosAdimec::_FptrGetTemperature;

    m_mapCommandTemplate["SET_PIXC"]=
        &EosAdimec::_FptrSetPixelCorrect;
    m_mapCommandTemplate["SET_PIXCORRECT"]=
        &EosAdimec::_FptrSetPixelCorrect;

    m_mapCommandTemplate["GET_PIXC"]=
        &EosAdimec::_FptrGetPixelCorrect;
    m_mapCommandTemplate["GET_PIXCORRECT"]=
        &EosAdimec::_FptrGetPixelCorrect;

    m_mapCommandTemplate["SETAGCORRECT"]=
        &EosAdimec::_FptrSetAgCorrect;
    m_mapCommandTemplate["SET_AGC"]=
        &EosAdimec::_FptrSetAgCorrect;
    m_mapCommandTemplate["SET_AGCORRECT"]=
        &EosAdimec::_FptrSetAgCorrect;

    m_mapCommandTemplate["GETAGCORRECT"]=
        &EosAdimec::_FptrGetAgCorrect;
    m_mapCommandTemplate["GET_AGC"]=
        &EosAdimec::_FptrGetAgCorrect;
    m_mapCommandTemplate["GET_AGCORRECT"]=
        &EosAdimec::_FptrGetAgCorrect;

    m_mapCommandTemplate["GET_IMGFMT"]=
        &EosAdimec::_FptrGetImageFormat;

    m_mapCommandTemplate["SET_IMGFMT"]=
        &EosAdimec::_FptrSetImageFormat;
    
    return;
}



// For this device, a do-nothing stub
int EosAdimec::SendDeviceCommand(void)
{
    return 0;
}

// For this device, a do-nothing stub
int EosAdimec::DeviceSafeMode(void)
{
    return 0;
}

// Translate SCIP commands into device-specific commands
int EosAdimec::TranslateGenericCommand(void)
{
  
    try
    {
    
        if(m_vStrGenericCommand.size()<1)
        {
            // Nothing to parse
            return UNIX_ERROR_STATUS;
        }
    
        // Check to see if we have this generic command in our template
        if(m_mapCommandTemplate.find(m_vStrGenericCommand[0]) != m_mapCommandTemplate.end())
        {
            int nRetVal;
      
            // Each generic command is mapped to an associated device-specific command via
            // boost::function
            nRetVal=m_mapCommandTemplate[m_vStrGenericCommand[0]](this,m_vStrGenericCommand);
      
      
            // Capture time that the valid command was received/parsed
            // RWM 2020/03/19 moved to base class:SetTimeOfLastCommandBase();
      
            return nRetVal;
        }
        else
        {
            // Generic command not found.
            // Do nothing and return an error value.
            return UNIX_ERROR_STATUS;
        }
    }
    catch(EosDeviceException &excep)
    {
        excep.DumpExceptionInfo();
    
        std::string strExcept=excep.GetExceptionMessage();
        unsigned int istr;
        for(istr=0; istr<strExcept.size(); istr++)
        {
            if(strExcept[istr]==' ')
                strExcept[istr]='_';
        }
    
        if(m_pPipeComms!=NULL)
            m_pPipeComms->Write(strExcept+std::string(" \n"));
    
        return UNIX_ERROR_STATUS;
    }
    catch(...)
    {
        // Catch any other exception that might otherwise crash the app
        return UNIX_ERROR_STATUS;
    }
  
    return UNIX_OK_STATUS;
}


// ######################## BEGIN BOOST FUNCTION PTRS (For Command Map) ####################//


//prints out all implemented device commands. Pulled from EosPower
int EosAdimec::_FptrListCommandInfo(const std::vector<std::string> &vStrArgs){
    std::map<std::string, 
             boost::function<int (EosAdimec*, const std::vector<std::string>&) > >::iterator imm;
    
    char cBuf[BUFLEN+1]; // More than big enough
    ::memset(cBuf,'\0',BUFLEN);

    std::string strResp;
    for(imm=m_mapCommandTemplate.begin(); imm!=m_mapCommandTemplate.end(); ++imm)
    {
        ::snprintf(cBuf,BUFLEN-1,
                   " SS%3.3d|%s[] \n",
                   m_nAdimecId,imm->first.c_str());

        strResp=std::string(cBuf);

        // Ship the SCIP message up to the remote client
        //nBytes=AddMessageToRecvQueue(strResp);
        //m_vStrGenericResponse.push_back(std::string(cBuf));
        m_pPipeComms->Write(std::string(cBuf));
    }
    
    return NO_RESPONSE_STATUS;
}

// Force a serial-port reconnect
int EosAdimec::_FptrReconnect(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    
    nStatus=HandleReconnect(vStrArgs);
    
    return nStatus;
}


// A simple probe operation to determine whether the device is up
// and communicating.  Basically a wrapper around a GETGAIN[] operation.
// If the device responses, then assume that it is working.
int EosAdimec::_FptrProbe(const std::vector<std::string> &vStrArgs)
{

    int nStatus=UNIX_ERROR_STATUS;

    // Will request the device (camera) gain.
    // If we get anything back from the device , will report to
    // the main controller with a DEVICE_UP[] message
    std::vector<std::string> vStrProbe;
    vStrProbe.push_back("GETGAIN");
    nStatus = HandleGetLevel(vStrProbe);

    if(UNIX_OK_STATUS==nStatus)
    {
        m_abDeviceProbe=true;
        std::string strModel;
        for(auto & ic: m_strEosDeviceModel)
        {
            strModel.push_back(::toupper(ic));
        }
        ShipToSCIP(EosResp::DEVICE_UP,strModel);
    }
  
    return nStatus;
}

// Disable the device probe flag, if needed.
int EosAdimec::_FptrDisableProbe(const std::vector<std::string> &vStrArgs) 
{

    m_abDeviceProbe=false;

    return NO_RESPONSE_STATUS;
}

/*
  Load factory defaults for current operation.
  Will not be set permanently.
 */
int EosAdimec::_FptrLoadFactoryDefaults(const std::vector<std::string> & vStrArgs)
{

    // When loading factory defaults, disable saving of settings on exit
    // in case factory defaults do something that we really don't want.
    m_bSaveSettingsOnExit=false;

    std::string strFacDefaults="@FD";
    PdvSerialWrite(strFacDefaults);
    
    // Read response (if any) to clear it out of the
    // camera response queue
    std::string strResp;
    PdvSerialRead(strResp);
    
    return NO_RESPONSE_STATUS;
}

/*
  Restore factory defaults permanently.
 */
int EosAdimec::_FptrRestoreFactoryDefaults(const std::vector<std::string> & vStrArgs)
{
    std::string strFacDefaults="@FD";
    std::string strResp;

    // Load the factory defaults.
    PdvSerialWrite(strFacDefaults);
    
    // Read response (if any) to clear it out of the
    // camera response queue
    PdvSerialRead(strResp);
    

    strFacDefaults="@SC";
    PdvSerialWrite(strFacDefaults);

    PdvSerialRead(strResp); // Clear out the Adimec buffer
    
    strFacDefaults="@SC";
    PdvSerialWrite(strFacDefaults);

    PdvSerialRead(strResp); // Clear out the Adimec buffer
    

    return NO_RESPONSE_STATUS;
}

// Determine whether to save settings on exit. Default is no.
// Enable saving settings on exit only with an arg of "y", "Y", or "1"
int EosAdimec::_FptrSaveSettingsOnExitMode(const std::vector<std::string> & vStrArgs)
{
    m_bSaveSettingsOnExit=false;
    if((vStrArgs.at(1)=="y")||(vStrArgs.at(1)=="Y")||(vStrArgs.at(1)=="1"))
    {
        m_bSaveSettingsOnExit=true;
    }
    return NO_RESPONSE_STATUS;
}

// Remember that command args start with the *2nd* 
// element of vStrArgs (i.e. vStrArgs[1]).
// Each element below is a string element in vStrArgs
//    vStrArgs[0] --> Command string
//    vStrArgs[1] --> Offset Level (0-4095)
// Set Offset Level
int EosAdimec::_FptrSetOffsetLevel(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    try{
        if (vStrArgs.size()!=2)
        {
            return UNIX_ERROR_STATUS;
        }
        nStatus=HandleSetOffsetLevel(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;


}

// Remember that command args start with the *2nd* 
// element of vStrArgs (i.e. vStrArgs[1]).
// Each element below is a string element in vStrArgs
//    vStrArgs[0] --> Command string
//    vStrArgs[1] --> Gain Level (100-800)
// Set Gain Level
int EosAdimec::_FptrSetGainLevel(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    try{
        if (vStrArgs.size()!=2)
        {   
            ShipToSCIP(EosResp::ARGERROR,"");
            return UNIX_ERROR_STATUS;
        }

        nStatus=HandleSetGainLevel(vStrArgs);
    }
    
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}
  
// Remember that command args start with the *2nd* 
// element of vStrArgs (i.e. vStrArgs[1]).
// Each element below is a string element in vStrArgs
//    vStrArgs[0] --> Command string
//    vStrArgs[1] --> RGB value <x;x;x> (100-399)
//Sets the RGB balance
int EosAdimec::_FptrSetRGBLevel(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    try{
        if (vStrArgs.size()!=4)   
        {
            ShipToSCIP(EosResp::ARGERROR,"");
            return UNIX_ERROR_STATUS;
        }
        nStatus=HandleSetRGBLevel(vStrArgs);
    }
    
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}



// Each element below is a string element in vStrArgs
// vStrArgs[0] == command name
int EosAdimec::_FptrGetLevel(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    try{
        if (vStrArgs.size()>1)   
        {
            ShipToSCIP(EosResp::ARGERROR,"");
            return UNIX_ERROR_STATUS;
        }
        nStatus=HandleGetLevel(vStrArgs);
    }
    
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

// Each element below is a string element in vStrArgs
// vStrArgs[0] == command name
int EosAdimec::_FptrSaveSettings(const std::vector<std::string>& vStrArgs){
    
    int nStatus=UNIX_ERROR_STATUS;
    try{
        if(vStrArgs.size()>1){
            ShipToSCIP(EosResp::ARGERROR,"");
            return nStatus;
        }
        nStatus=SaveSettings();
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

// Each element below is a string element in vStrArgs
// vStrArgs[0] = command name

int EosAdimec::_FptrSetOutputResolution(const std::vector<std::string>& vStrArgs){
    
    int nStatus = UNIX_ERROR_STATUS;
    try{
        if(vStrArgs.size()!=2){
            ShipToSCIP(EosResp::ARGERROR,"");
            return nStatus;
        }
        nStatus = HandleSetResolution(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

int EosAdimec::_FptrGetOutputResolution(const std::vector<std::string>& vStrArgs){
    
    int nStatus = UNIX_ERROR_STATUS;
    try{
        if(vStrArgs.size()!=1){
            ShipToSCIP(EosResp::ARGERROR,"");
            return nStatus;
        }
        nStatus = HandleGetResolution(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

// GETFP[]
int EosAdimec::_FptrGetFramePeriod(const std::vector<std::string>& vStrArgs)
{

    int nStatus = UNIX_ERROR_STATUS;
    try{
        if(vStrArgs.size()!=1){
            ShipToSCIP(EosResp::ARGERROR,"");
            return nStatus;
        }
        nStatus = HandleGetFramePeriod(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }


    return UNIX_OK_STATUS;
}

// SETFP[nFpVal] nFpVal range 1-4000
int EosAdimec::_FptrSetFramePeriod(const std::vector<std::string>& vStrArgs)
{
    int nStatus = UNIX_ERROR_STATUS;
    try{
        if(vStrArgs.size()!=2){
            ShipToSCIP(EosResp::ARGERROR,"1-4000");
            return nStatus;
        }
        nStatus = HandleSetFramePeriod(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }


    return UNIX_OK_STATUS;
}

// GETIT[]
int EosAdimec::_FptrGetIntegrationTime(const std::vector<std::string>& vStrArgs)
{

    int nStatus = UNIX_ERROR_STATUS;
    try{
        if(vStrArgs.size()!=1){
            ShipToSCIP(EosResp::ARGERROR,"");
            return nStatus;
        }
        nStatus = HandleGetIntegrationTime(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }


    return UNIX_OK_STATUS;
}

// SETIT[nItVal] nItVal range 1-4000
int EosAdimec::_FptrSetIntegrationTime(const std::vector<std::string>& vStrArgs)
{

    int nStatus = UNIX_ERROR_STATUS;
    try{
        if(vStrArgs.size()!=2){
            ShipToSCIP(EosResp::ARGERROR,"1-4000");
            return nStatus;
        }
        nStatus = HandleSetIntegrationTime(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }


    return UNIX_OK_STATUS;
}

// GETTEMP[], GET_TEMP[]
int EosAdimec::_FptrGetTemperature(const std::vector<std::string>& vStrArgs)
{

    int nStatus = UNIX_ERROR_STATUS;
    try{
        if(vStrArgs.size()!=1){
            ShipToSCIP(EosResp::ARGERROR,"");
            return nStatus;
        }
        nStatus = HandleGetTemperature(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }


    return UNIX_OK_STATUS;
}

int EosAdimec::_FptrGetPixelCorrect(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    try{
        if (vStrArgs.size()!=1)
        {
            return UNIX_ERROR_STATUS;
        }
        nStatus=HandleGetPixelCorrect(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

int EosAdimec::_FptrSetPixelCorrect(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    try{
        if (vStrArgs.size()<2)
        {
            return UNIX_ERROR_STATUS;
        }
        nStatus=HandleSetPixelCorrect(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

int EosAdimec::_FptrGetAgCorrect(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    try
    {
        if (vStrArgs.size()!=1)
        {
            return UNIX_ERROR_STATUS;
        }
        nStatus=HandleGetAgCorrect(vStrArgs);
    }
    catch(...)
    {
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

int EosAdimec::_FptrSetAgCorrect(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    try
    {
        if (vStrArgs.size()<2)
        {
            return UNIX_ERROR_STATUS;
        }
        nStatus=HandleSetAgCorrect(vStrArgs);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

int EosAdimec::_FptrGetImageFormat(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    try
    {
        if (vStrArgs.size()!=1)
        {
            return UNIX_ERROR_STATUS;
        }
        nStatus=HandleGetImageFormat(vStrArgs);
    }
    catch(...)
    {
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

int EosAdimec::_FptrSetImageFormat(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    try
    {
        if (vStrArgs.size()<4)
        {
            return UNIX_ERROR_STATUS;
        }
        nStatus=HandleSetImageFormat(vStrArgs);
    }
    catch(...)
    {
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

// ######################## END BOOST FUNCTION PTRS (For Command Map) ####################/



/*Function that queries the camera and builds the internal struct that tracks the values
  Designed to be called in the constuctor so that we always know what the current 
  settings of the camera are*/
int EosAdimec::InitAdimecStruct()
{
    int nStatus = UNIX_ERROR_STATUS;
    std::string strCmd,strResp;

    /*Checking the current Output Resolution of the Adimec. Not adding this to
      get level as OR should not change after initialization and we don't want 
      * to give visibility to it. */
    strCmd="@OR?";
    PdvSerialWrite(strCmd);
    PdvSerialRead(strResp); // Clear out the Adimec buffer
    
    strResp="";
    strCmd="@GA?";
    PdvSerialWrite(strCmd);
    nStatus=PdvSerialRead(strResp);
    if(nStatus==UNIX_OK_STATUS)
        m_pAdimec->strGain=strResp;            
    
    strResp="";
    PdvSerialWrite("@OFS?");
    nStatus=PdvSerialRead(strResp);
    if(nStatus==UNIX_OK_STATUS)
    {
        m_pAdimec->strOffset=strResp;
    }
    
    PdvSerialWrite("@WB?");
    nStatus=PdvSerialRead(strResp); 
    if(nStatus==UNIX_OK_STATUS)
    {
        if(strResp.size()>0)
        {
            std::vector<std::string> vstrSplit;
            boost::split(vstrSplit,strResp,boost::is_any_of(","));
            for(unsigned int i=0;i<vstrSplit.size();i++){
                m_pAdimec->strRGB[i]=vstrSplit.at(i);
            }
        }
        
    }
    return nStatus;
}

/**
   Sends a command to the Adimec instructing it to save its settings.
   Composes a SCIP-format device response message and sends that
   response up the main controller.
*/
int EosAdimec::SaveSettings(){
    int nStatus=UNIX_ERROR_STATUS;
    std::string strCmd="@SC";
    std::string strResp;
    
    // Have gotten error responses on the first
    // savesettings try, so do it twice here to make sure
    // that settings get saved.
    int nStatus_wr=PdvSerialWrite(strCmd);

    PdvSerialRead(strResp); // Clear out the adimec buffer.
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    nStatus_wr=PdvSerialWrite(strCmd);

    // Read just to clear out the camlink send buffer
    PdvSerialRead(strResp); // Clear out Adimec buffer
    
    if(nStatus_wr==UNIX_OK_STATUS){
        ShipToSCIP(EosResp::SAVEDSETTINGS,"");
    }
    else{
        ShipToSCIP(EosResp::ERRORSAVINGSETTINGS,"");
    }

    return nStatus;
}


// Force a serial-port reconnect
int EosAdimec::HandleReconnect(const std::vector<std::string>& vStrArgs)
{
    // RWM MOD 2022/03/15
    // int nStatus=ResetSerialConnection();
    int nStatus=m_pSerialComms->ResetConnection();
    
    ShipToSCIP(EosResp::RECONNECTING,"");
    // No response to read when the camera is reconnected.


    return nStatus;
}

/**
   Calls the requisite functions to retrieve the 
   current gain/offset/rgb settings.
   Composes a SCIP-format device response message and sends that
   response up the main controller.
*/
int EosAdimec::HandleGetLevel(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    char cCmdChar=vStrArgs[0].at(3);
    std::string strValue, strCmd, strResp;
    
    bool bValid=false;

    nStatus=GetLevel(cCmdChar,strCmd,strResp);
    if(nStatus==UNIX_OK_STATUS)
    {    
        PdvSerialWrite(strCmd);
        nStatus = PdvSerialRead(strValue); 

        // RGB is a special case -- 3 vals returned.
        if(strResp==EosResp::RGBPOS)
        {
            // A kluge: The Adimec reports colors in BGR order, but we
            // want to send out RGBPOS[R,G,B] because that's what Sentinel
            // is expecting.
            if(strValue.size()>0)
            {
                std::string strValueRev=strValue;
                std::vector<std::string>vStrValueRev;
                boost::split(vStrValueRev,strValueRev,boost::is_any_of(","));
                if(vStrValueRev.size()>=3)
                {
                    strValue.clear();
                    strValue+=vStrValueRev.at(2)+",";
                    strValue+=vStrValueRev.at(1)+",";
                    strValue+=vStrValueRev.at(0);
                    bValid=true;
                }
            }
            if(bValid)
            {
                ShipToSCIP(strResp,strValue);
            }
            else
            {
                ShipToSCIP(EosResp::ERRORGETTINGVALUE,"");
            }
            return nStatus;
        }
        // Not RGB -- must be gain or offset.
        // single value will be returned as strValue
        // (i.e. not multiple comma-separated values)
        if(UNIX_OK_STATUS==nStatus)
            ShipToSCIP(strResp,strValue);
        else
            ShipToSCIP(EosResp::ERRORGETTINGVALUE,"");
        
    }

    return nStatus;
}

/**
   Wrapper around SetGainLevel -- translates std::vector<std::string> args
   into the format needed for SetGainLevel
   Sends the set-gain command to the Adimec.
   Composes a SCIP-format device response message and sends that
   response up the main controller.
*/
int EosAdimec::HandleSetGainLevel(const std::vector<std::string>& vStrArgs){
    std::string strGainVal,strGainMsg, strResp;
    int nStatus=UNIX_ERROR_STATUS,nTempGain=0;

    try
    {
        nTempGain=boost::lexical_cast<int>(vStrArgs[1]);
        strGainVal=vStrArgs[1];
        SetGainLevel(nTempGain,strGainMsg,strGainVal);
        
        PdvSerialWrite(strGainMsg);
        nStatus=PdvSerialRead(strResp);
    }
    //catching a possible lexical_cast exception
    catch(...)
    {
        nStatus=UNIX_ERROR_STATUS;
    }
    if(nStatus==UNIX_OK_STATUS)
    {
        ShipToSCIP(EosResp::GAINPOS,strGainVal);
    }
    else
    {
        ShipToSCIP(EosResp::ERRORSETTINGGAIN,"100-800");
    }
    return nStatus;

}

int EosAdimec::HandleSetResolution(const std::vector<std::string>& vStrArgs){
    int nStatus = UNIX_ERROR_STATUS;
    int nTempResolution;
    std::string strResolutionValue, strResolutionMsg, strResp;

    try{
        nTempResolution = boost::lexical_cast<int>(vStrArgs[1]);
        strResolutionValue = std::to_string(nTempResolution);
        nStatus=SetResolution(nTempResolution, strResolutionMsg, strResolutionValue);
        PdvSerialWrite(strResolutionMsg);
        nStatus=PdvSerialRead(strResp);
    }
    catch(...)
    {
        nStatus=UNIX_ERROR_STATUS;
    }
    if(nStatus==UNIX_OK_STATUS)
    {
        ShipToSCIP(EosResp::OUTPUTRESOLUTION,strResolutionValue);
    }
    else
    {
        ShipToSCIP(EosResp::ERRORSETTINGRESOLUTION,"8,10,12");
    }
    return nStatus;
}
int EosAdimec::HandleGetResolution(const std::vector<std::string>& vStrArgs){
    int nStatus = UNIX_ERROR_STATUS;
    std::string strResolutionMsg, strResp;

    try{
        strResolutionMsg="@OR?";
        PdvSerialWrite(strResolutionMsg);
        nStatus=PdvSerialRead(strResp);
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    if(nStatus!=UNIX_OK_STATUS)
    {
        ShipToSCIP(EosResp::ERRORGETTINGRESOLUTION,"8,10,12");
    }
    else
    {
        ShipToSCIP("OPR",strResp);
    }
    
    return nStatus;
}

/**
   Wrapper around SetRGBLevel -- translates std::vector<std::string> args
   into the format needed for SetRGBLevel.  
   Sends the set-rgb command to the Adimec.
   Composes a SCIP-format device response message and sends that
   response up the main controller.
*/
int EosAdimec::HandleSetRGBLevel(const std::vector<std::string>& vStrArgs){
    int nStatus=UNIX_ERROR_STATUS;
    std::string strNewRGB;
    std::string strScipRGB, strResp;

    try{
        nStatus=SetRGBLevel(vStrArgs,strNewRGB);
        if(nStatus==UNIX_OK_STATUS)
        {
            PdvSerialWrite(strNewRGB);
            nStatus=PdvSerialRead(strResp);
            if(nStatus==UNIX_OK_STATUS){
                strScipRGB=vStrArgs.at(1)+","+vStrArgs.at(2)+","+vStrArgs.at(3);
                ShipToSCIP(EosResp::RGB,strScipRGB);
            }
        }
        if(nStatus==UNIX_ERROR_STATUS){
            ShipToSCIP(EosResp::ERRORSETTINGRGB,"100-399,100-399,100-399");
        }
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
    
}

/**
   Wrapper around SetOffsetLevel -- translates std::vector<std::string> args
   into the format needed for SetOffsetLevel
   Sends the set-offset command to the Adimec.
   Composes a SCIP-format device response message and sends that
   response up the main controller.
*/
int EosAdimec::HandleSetOffsetLevel(const std::vector<std::string>& vStrArgs){
    int nStatus=UNIX_ERROR_STATUS;
    std::string strOffsetCmd, strResp;
    
    nStatus=SetOffsetLevel(vStrArgs[1],strOffsetCmd);
    if(nStatus==UNIX_OK_STATUS)
    {
        PdvSerialWrite(strOffsetCmd);

        nStatus = PdvSerialRead(strResp); 

        if(nStatus==UNIX_OK_STATUS)
        {
            ShipToSCIP(EosResp::OFFSETPOS,vStrArgs[1]);
        }
    }
    else{
        ShipToSCIP(EosResp::ERRORSETTINGOFFSET,"20-4095");
    }
    
    return nStatus;
}

int EosAdimec::HandleGetFramePeriod(const std::vector<std::string>& vStrArgs)
{
    int nStatus = UNIX_ERROR_STATUS;
    std::string strResp;
    std::string strFramePeriodCmd;
    std::string strFramePeriod,strMinFramePeriod;

    try{
        strFramePeriodCmd="@FM?";
        PdvSerialWrite(strFramePeriodCmd);
        strMinFramePeriod.clear();
        nStatus=PdvSerialRead(strResp);

        strMinFramePeriod=strResp;
        
        strFramePeriodCmd="@FP?";
        PdvSerialWrite(strFramePeriodCmd);
        strFramePeriod.clear();
        PdvSerialRead(strResp); 

        strFramePeriod=strResp;
    }
    catch(...){
        nStatus=UNIX_ERROR_STATUS;
    }
    if(nStatus!=UNIX_OK_STATUS)
    {
        ShipToSCIP(EosResp::ERRORGETTINGFP,"1-4000");
    }
    else
    {
        ShipToSCIP(EosResp::FRAME_PERIOD,strFramePeriod);
        ShipToSCIP("MIN_FRAME_PERIOD",strMinFramePeriod);
    }

    return nStatus;
}

int EosAdimec::HandleSetFramePeriod(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    std::string strFpCmd, strResp;

    nStatus=SetFramePeriod(vStrArgs[1],strFpCmd);
    if(nStatus==UNIX_OK_STATUS)
    {
        PdvSerialWrite(strFpCmd);

        nStatus=PdvSerialRead(strResp);

        if(nStatus==UNIX_OK_STATUS){
            ShipToSCIP(EosResp::FRAME_PERIOD,vStrArgs[1]);
        }
    }
    else{
        ShipToSCIP(EosResp::ERRORSETTINGFP,"1-4000");
    }
    
    return nStatus;
}

int EosAdimec::HandleGetIntegrationTime(const std::vector<std::string>& vStrArgs)
{
    int nStatus = UNIX_ERROR_STATUS;
    std::string strIntegrationTimeMsg, strResp;
    
    try{
        strIntegrationTimeMsg="@IT?";
        PdvSerialWrite(strIntegrationTimeMsg);
        nStatus = PdvSerialRead(strResp); 
    }
    catch(...)
    {
        nStatus=UNIX_ERROR_STATUS;
    }

    if(nStatus!=UNIX_OK_STATUS)
    {
        ShipToSCIP(EosResp::ERRORGETTINGIT,"1-4000");
    }
    else
    {
        ShipToSCIP(EosResp::INT_TIME,strResp);
    }

    return nStatus;
}

int EosAdimec::HandleSetIntegrationTime(const std::vector<std::string>& vStrArgs)
{
    int nStatus=UNIX_ERROR_STATUS;
    std::string strItCmd, strResp;
    
    nStatus=SetIntegrationTime(vStrArgs[1],strItCmd);

    if(nStatus==UNIX_OK_STATUS)
    {
        PdvSerialWrite(strItCmd);

        nStatus = PdvSerialRead(strResp); 
    }

    if(nStatus==UNIX_OK_STATUS)
    {
        ShipToSCIP(EosResp::INT_TIME,vStrArgs[1]);
    }
    else
    {
        ShipToSCIP(EosResp::ERRORSETTINGIT,"1-4000");
    }

    return nStatus;
}

int EosAdimec::HandleGetTemperature(const std::vector<std::string>& vStrArgs)
{
    int nStatus = UNIX_ERROR_STATUS;
    std::string strResp;
    std::string strGetTempCmd, strTempResp;

    try
    {
        strGetTempCmd="@TM?";
        PdvSerialWrite(strGetTempCmd);

        nStatus = PdvSerialRead(strResp); 
        strTempResp=strResp;
    }
    catch(...)
    {
        nStatus=UNIX_ERROR_STATUS;
    }

    if(nStatus!=UNIX_OK_STATUS)
    {
        ShipToSCIP("ERROR_GETTING_TEMP","");
    }
    else
    {
        ShipToSCIP("TEMP",strTempResp);
    }
    return nStatus;
}

int EosAdimec::HandleGetPixelCorrect(const std::vector<std::string>& vStrArgs)
{
    int nStatus = UNIX_ERROR_STATUS;
    std::string strResp;
    std::string strGetPixCmd,strGetNumPixCmd;
    std::string strPixResp,strNumPixResp;

    try{
        strGetPixCmd="@DPE?";
        PdvSerialWrite(strGetPixCmd);

        std::vector<char> vBuff;
        int nLength=0;
        nStatus = PdvSerialRead(strResp); 
        strPixResp=strResp;

        strGetNumPixCmd="@DP?0";
        PdvSerialWrite(strGetNumPixCmd);

        nStatus=PdvSerialRead(strResp); 
        strNumPixResp=strResp;
    }
    catch(...)
    {
        nStatus=UNIX_ERROR_STATUS;
    }
    if(nStatus!=UNIX_OK_STATUS)
    {
        ShipToSCIP("ERROR_GETTING_PIXC","");
    }
    else
    {
        ShipToSCIP("PIXCORRECT",strPixResp);
        ShipToSCIP("NUMPIXCORRECT",strNumPixResp);
    }
    return nStatus;
}

int EosAdimec::HandleSetAgCorrect(const std::vector<std::string>& vStrArgs)
{
    int nStatus = UNIX_ERROR_STATUS;
    std::string strResp;
    std::string strSetAgCmd, strAgResp;
    
    try
    {
        std::string strAgCorrect=vStrArgs.at(1);
        int nAgCorrect=boost::lexical_cast<int>(strAgCorrect);
        if((nAgCorrect<-2047)||(nAgCorrect>2047))
        {
            ShipToSCIP("ERROR_SETTING_AGCORRECT","-2047-2047");
            return UNIX_ERROR_STATUS;
        }
        strSetAgCmd="@AG"+strAgCorrect;
        PdvSerialWrite(strSetAgCmd);

        nStatus = PdvSerialRead(strResp); // Clear out Adimec buffer
        strAgResp=strResp;
    }
    catch(...)
    {
        nStatus= UNIX_ERROR_STATUS;
    }
    if(nStatus!=UNIX_OK_STATUS)
    {
        ShipToSCIP("ERROR_SETTING_AGCORRECT","-2047-2047");
    }
    else
    {
        ShipToSCIP("AGCORRECT",strAgResp);
    }

    return nStatus;
}

int EosAdimec::HandleGetAgCorrect(const std::vector<std::string>& vStrArgs)
{
    int nStatus = UNIX_ERROR_STATUS;
    std::string strResp;
    std::string strGetAgCmd;
    std::string strAgResp;

    try
    {
        strGetAgCmd="@AG?";
        PdvSerialWrite(strGetAgCmd);

        nStatus = PdvSerialRead(strResp); 
        strAgResp=strResp;
    }
    catch(...)
    {
        nStatus=UNIX_ERROR_STATUS;
    }
    if(nStatus!=UNIX_OK_STATUS)
    {
        ShipToSCIP("ERROR_GETTING_AGCORRECT","");
    }
    else
    {
        ShipToSCIP("AGCORRECT",strAgResp);
    }
    return nStatus;
}

int EosAdimec::HandleSetPixelCorrect(const std::vector<std::string>& vStrArgs)
{
    int nStatus = UNIX_ERROR_STATUS;
    std::string strResp;
    std::string strGetPixCmd, strPixResp;
    
    try
    {
        std::string strPixMode=vStrArgs.at(1);
        if((strPixMode!="0")&&(strPixMode!="1"))
        {
            ShipToSCIP("ERROR_GETPIXC","0-1");
            return UNIX_ERROR_STATUS;
        }
        strGetPixCmd="@DPE"+strPixMode;
        PdvSerialWrite(strGetPixCmd);

        nStatus = PdvSerialRead(strResp); // Clear out Adimec buffer
        strPixResp=strResp;
    }
    catch(...)
    {
        ShipToSCIP("ERROR_GETTING_PIXC","");
        nStatus=UNIX_ERROR_STATUS;
    }
    if(nStatus!=UNIX_OK_STATUS)
    {
        ShipToSCIP("ERROR_GETTING_PIXC","");
    }
    else
    {
        ShipToSCIP("PIXCORRECT",strPixResp);
    }
    return nStatus;
}

int EosAdimec::HandleGetImageFormat(const std::vector<std::string>& vStrArgs)
{
    int nStatus = UNIX_ERROR_STATUS;

    std::string strGetImgFmtCmd;
    std::string strResp;
    std::string strImgFmtResp;

    
    
    try
    {
        strGetImgFmtCmd="@FM?";
        PdvSerialWrite(strGetImgFmtCmd);

        nStatus = PdvSerialRead(strResp); 
        strImgFmtResp=strResp;
    }
    catch(...)
    {
        nStatus=UNIX_ERROR_STATUS;
    }
    if(nStatus!=UNIX_OK_STATUS)
    {
        ShipToSCIP("ERROR_GETTING_IMGFMT","");
    }
    else
    {
        std::vector<std::string> vStrImgFmtResp0,vStrImgFmtResp;
        
        boost::split(vStrImgFmtResp0,strImgFmtResp,boost::is_any_of("@;+"));

        // Tokenize the adimec FMx;y;z response to get x,y,z
        for(auto & ivec:vStrImgFmtResp0)
        {
            // Get rid of zero-length tokens
            if(ivec.size()>0)
            {
                vStrImgFmtResp.push_back(ivec);
                std::cout<<__FUNCTION__<<"(): ivec="<<ivec<<std::endl;

            }
        }
        if(vStrImgFmtResp.size()>=1)
        {
            // Need to reformat with "," delimiters
            ShipToSCIP("IMGFMT",vStrImgFmtResp.at(0));
        }
    }
    return nStatus;
}

int EosAdimec::HandleSetImageFormat(const std::vector<std::string>& vStrArgs)
{
    int nStatus = UNIX_ERROR_STATUS;
    std::string strResp;
    std::string strSetImgFmtCmd, strImgFmtResp;

    std::string strImgFmtX;
    std::string strImgFmtY;
    std::string strImgFmtZ;

    if(vStrArgs.size()>=4)
    {
        strImgFmtX=vStrArgs.at(1); // Vertical Image offset
        strImgFmtY=vStrArgs.at(2); // Vertical Image size 
        strImgFmtZ=vStrArgs.at(3); // Vertical binning factor
    }
    else
    {
        return UNIX_ERROR_STATUS;
    }

    
    try
    {
        strImgFmtX=vStrArgs.at(1); // Vertical Image offset
        strImgFmtY=vStrArgs.at(2); // Vertical Image size 
        strImgFmtZ=vStrArgs.at(3); // Vertical binning factor
        int nImgFmtX=boost::lexical_cast<int>(strImgFmtX);

        // Vertical offset must be even # scans
        if((nImgFmtX % 2) != 0)
        {
            nImgFmtX+=1;
            strImgFmtX=boost::lexical_cast<std::string>(nImgFmtX);
        }
        
        int nImgFmtY=boost::lexical_cast<int>(strImgFmtY);
        if(nImgFmtY<128)
        {
            nImgFmtY=128;
            strImgFmtY=boost::lexical_cast<std::string>(nImgFmtY);
        }
        
        int nImgFmtZ=boost::lexical_cast<int>(strImgFmtZ);

        strSetImgFmtCmd="FM"+strImgFmtX+";"+strImgFmtY+";"+strImgFmtZ;
        PdvSerialWrite(strSetImgFmtCmd);

        nStatus = PdvSerialRead(strResp);
        strImgFmtResp=strResp;
    }
    catch(...)
    {
        nStatus= UNIX_ERROR_STATUS;
    }
    if(nStatus!=UNIX_OK_STATUS)
    {
        ShipToSCIP("ERROR_SETTING_IMGFMT","-2047-2047");
    }
    else
    {
        ShipToSCIP("IMGFMT",strImgFmtResp);
    }

    return nStatus;
}

// Return an index value for the baudrate
int EosAdimec::BaudRate2Id (int nBaudRate)
{
    switch(nBaudRate)
    {
        case 9600:
            return CL_BAUDRATE_9600;
        case 19200:
            return CL_BAUDRATE_19200;
        case 38400:
            return CL_BAUDRATE_38400;
        case 57600:
            return CL_BAUDRATE_57600;
        case 115200:
            return CL_BAUDRATE_115200;
        case 230400:
            return CL_BAUDRATE_230400;
        case 460800:
            return CL_BAUDRATE_460800;
        case 921600:
            return CL_BAUDRATE_921600;
        default:
            return 0;
    }
}


/**
   Diagnostic function to check the response messages from the camera
*/
void EosAdimec::PrintAdimecResponse(char cBuff[],int nLength){
    for (int i=0;i<nLength;i++){
        if(cBuff[i]==AACK){
            printf("<ACK> ");
        }
        else if(cBuff[i]==ANAK){
            printf("<NAK> ");
        }
        else if(cBuff[i]==ASTX){
            printf("<STX> ");
        }
        else if(cBuff[i]==AETX){
            printf("<ETX> ");
        }
        else{
            printf("%c ",cBuff[i]);
        }
    }
}


/**
   Strips out transmission flags and changes message delimiters into a form that can 
   be sent on through to SCIP. Device response will be stored in strResp
*/
int EosAdimec::ComposeDevResp(const char cBuff[]/*input*/, int nLength/*input*/,
                              std::string& strResp/*output*/)
{
    int nStatus = UNIX_ERROR_STATUS;

    printf ("%s\r\n",cBuff);
    for (int i=0;i<nLength;i++){
        if(cBuff[i]==AACK){
            nStatus=UNIX_OK_STATUS;
        }
        else if(cBuff[i]==ANAK){
            nStatus=UNIX_ERROR_STATUS;
        }
        else if(cBuff[i]==ASTX){
        }
        else if(cBuff[i]==AETX){
        }
        else if(cBuff[i]=='+'){
        }
        else if(cBuff[i]==';'){
            strResp+=',';
        }
        else{
            strResp+=cBuff[i];
        }
    }
    return nStatus;
    
}



/**
   Validates that new Offset value is in the correct range and also checks that 
   it exceeds the minimum value based on the current gain setting and output resolution. 

   The manual for the Adimec provides a formula to calculate a dynamic offset but the 
   usable domain of the function is incredibly small for the range of values we would 
   provide it. The domain goes negative in less than 2 one/hundreths of a gain setting. 
   Therefore we have set the minimum offset value to the ceiling of possible minimum 
   offset values. The formula and preset values are presented here for future reference. 
   
   Formula:
   Minimum Offset >= OutputResolutionScalar *(100- CurrentGain) + (20*CurrentGain)/100
   Formula provided by Adimec operating and technical manual. 

   OutputResolutionScalars:
   12 bits = 40.95
   10 bits = 10.23
   8 bits = 255
*/
int EosAdimec::ValidateOffset(int nTempOffset){
    int nMinOffset=20;
    if((nTempOffset>=nMinOffset && nTempOffset<=4095)){
        return UNIX_OK_STATUS;
    } 
    return UNIX_ERROR_STATUS;    
}


/**
   Checks that the received White Balance RGB values are within an acceptable range. 
   Also sets the RGB values of the internal Adimec struct
*/
int EosAdimec::ValidateRGB(const std::vector<std::string>& vStrArgs){
    int nStatus=UNIX_ERROR_STATUS;
    int nTempRGB;
    try{
        for(int i=1;i<4;i++){
            nTempRGB=boost::lexical_cast<int>(vStrArgs[i]);
            if(nTempRGB<100||nTempRGB>399){
                nStatus=UNIX_ERROR_STATUS;
                break;
            }
            m_pAdimec->strRGB[i-1]=vStrArgs[i];
            nStatus=UNIX_OK_STATUS;
        }
    }
    catch(...){
        return UNIX_ERROR_STATUS;
    }
    return nStatus;
}

// Validate frame period setting.
int EosAdimec::ValidateFramePeriod(int nFramePeriod){
    int nMinFramePeriod=1;
    int nMaxFramePeriod=4000;
    if((nFramePeriod>=nMinFramePeriod && nFramePeriod<=nMaxFramePeriod)){
        return UNIX_OK_STATUS;
    } 
    return UNIX_ERROR_STATUS;    
}

// Validate frame integration time
int EosAdimec::ValidateIntegrationTime(int nIntegrationTime){
    int nMinIntegrationTime=1;
    int nMaxIntegrationTime=4000;
    if((nIntegrationTime>=nMinIntegrationTime && nIntegrationTime<=nMaxIntegrationTime)){
        return UNIX_OK_STATUS;
    } 
    return UNIX_ERROR_STATUS;    
}

/**
   Method to formulate and send messages up to SCIP. Takes a string as the 
   command that will be sent as well as a string that contains the message value.
   sample SCIP msg: pp500|<strMsg>[<strValue>]
*/
void EosAdimec::ShipToSCIP(std::string strMsg, std::string strValue){
    
    char cBuf[BUFLEN+1]; // More than big enough
    ::memset(cBuf,'\0',BUFLEN);
    std::string strResp;
    

    ::snprintf(cBuf,BUFLEN-1,
               "SS%3.3d|%s[%s]\n",
               m_nAdimecId,strMsg.c_str(),strValue.c_str());

    strResp=std::string(cBuf);

    // Ship the SCIP message up to the remote client;
    m_pPipeComms->Write(std::string(cBuf));
    

}

/**
   Sets the device command and SCIP response message when a setting is queried. 
   Determines which setting to query based off of the 4th letter in the SCIP command. All 
   SCIP query commands follow this form:
   GET<CAMERASETTING> with current choices being:
   GAIN,RGB,OFFSET
   If support for more settings is needed than the switch statement might need to change to
   a more robust method like a map. 
*/
int EosAdimec::GetLevel(const char cValue,std::string& strCmd, std::string& strResp){
    int nStatus=UNIX_OK_STATUS;
    switch (cValue){
        case 'G':
            strCmd="@GA?";
            strResp=EosResp::GAINPOS;
            break;
        case 'R':
            strCmd="@WB?";
            strResp=EosResp::RGBPOS;
            break;
        case 'O':
            strCmd="@OFS?";
            strResp=EosResp::OFFSETPOS;
            break;
        default:
            nStatus=UNIX_ERROR_STATUS;
    }
    return nStatus;
}

/**
   Compose an Adimec set-rgb command.
   strArgs contains the r,g,and b values.
   strNewRgb is the command to be sent to the adimec.
*/
int EosAdimec::SetRGBLevel(const std::vector<std::string>& vStrArgs,std::string& strNewRGB){
    int nStatus=UNIX_ERROR_STATUS;
    int nValue=0;
    strNewRGB="@WB";
    try{
        for(int i=3;i>=1;i--){
            nValue=boost::lexical_cast<int>(vStrArgs[i]);
            if(nValue<100||nValue>399){
                nStatus=UNIX_ERROR_STATUS;
                break;
            }
            nStatus=UNIX_OK_STATUS;
            m_pAdimec->strRGB[i-1]=vStrArgs[i];
            strNewRGB+=vStrArgs[i];
            if(i>1)
            {
                strNewRGB+=";";
            }
            else
            {
                strNewRGB+="\r\n";
            }
        }
        std::cout<<__FUNCTION__<<"(): strNewRGB="<<strNewRGB<<std::endl;
    }
    catch(...){
        return UNIX_ERROR_STATUS;
    }

    return nStatus;
}

/**
   Compose an Adimec gain level command.
   strGainMsg is the command to be sent to the Admec.
   nTempGain and strGainValue are the gain level (int and string format).
   nTempGain used to compare with min/max allowable gain levels.
*/
int EosAdimec::SetGainLevel(int nTempGain, std::string& strGainMsg,const std::string strGainValue){
    if(nTempGain>=100 && nTempGain<=800){
        m_pAdimec->strGain=strGainValue;
        strGainMsg = "@GA"+strGainValue;
        return UNIX_OK_STATUS;
    }
    return UNIX_ERROR_STATUS;
    
}


int EosAdimec::SetResolution(int nTempResolution, std::string& strResolutionMsg, const std::string strResolutionValue){
    if(nTempResolution ==8 ||nTempResolution==10||nTempResolution==12){
        m_pAdimec->strOutputRes=strResolutionValue;
        strResolutionMsg = "@OR"+std::to_string(nTempResolution);
        return UNIX_OK_STATUS;
    }
    return UNIX_ERROR_STATUS;
}

/**
   Compose an Adimec set-offset command.
   strNewValue is the new offset value.
   strOffsetCmd is the set-offset command to be sent to the Adimec.
*/
int EosAdimec::SetOffsetLevel(const std::string& strNewValue, std::string& strOffsetCmd){
    try{
        int nTempOffset=boost::lexical_cast<int>(strNewValue);
        if(ValidateOffset(nTempOffset)==UNIX_OK_STATUS){
            strOffsetCmd="@OFS"+strNewValue;
            m_pAdimec->strOffset=strNewValue;
            return UNIX_OK_STATUS;
        }
    }
    catch(...){
        return UNIX_ERROR_STATUS;
    }
    return UNIX_ERROR_STATUS;
}

// Valid frame period values 1-4000
int EosAdimec::SetFramePeriod(const std::string& strNewValue, std::string& strOffsetCmd){
    try{
        int nTempFp=boost::lexical_cast<int>(strNewValue);
        if(ValidateFramePeriod(nTempFp)==UNIX_OK_STATUS){
            strOffsetCmd="@FP"+strNewValue;
            m_pAdimec->strFramePeriod=strNewValue;
            return UNIX_OK_STATUS;
        }
    }
    catch(...){
        return UNIX_ERROR_STATUS;
    }
    return UNIX_ERROR_STATUS;
}

// Valid integration time values 1-4000
int EosAdimec::SetIntegrationTime(const std::string& strNewValue, std::string& strItCmd){
    try{
        int nTempIt=boost::lexical_cast<int>(strNewValue);
        if(ValidateIntegrationTime(nTempIt)==UNIX_OK_STATUS){
            strItCmd="@IT"+strNewValue;
            m_pAdimec->strIntegrationTime=strNewValue;
            return UNIX_OK_STATUS;
        }
    }
    catch(...){
        return UNIX_ERROR_STATUS;
    }
    return UNIX_ERROR_STATUS;
}


int EosAdimec::PdvSerialWrite(const std::string& strCmdIn)
{
    int nStatus;
    if(m_pSerialComms)
    {
        // Adimec needs newline termination
        std::string strCmd=strCmdIn+"\r\n";
        nStatus=m_pSerialComms->SerialWrite(strCmd);
    }
    else
        nStatus=UNIX_ERROR_STATUS;
    return nStatus;
}

int EosAdimec::PdvSerialRead(std::string& strResp)
{
    int nStatus;
    std::vector<char> vBuff;
    int nLength=0;
    strResp="";
    if(m_pSerialComms)
    {
        nStatus=m_pSerialComms->SerialRead(vBuff,nLength); // Clear out Adimec buffer
        strResp.clear();
        if(UNIX_OK_STATUS==nStatus)
            ComposeDevResp(vBuff.data(),nLength,strResp);
    }
    else
        nStatus=UNIX_ERROR_STATUS;

    return nStatus;
}

// ######### Stuff Below is for Unit Testing (test/debugging only) #######################
//                     
// Test for generic-command
void EosAdimec::UnitTestAddGenericCommand(const std::vector<std::string>& vStrIn)
{
    m_vStrGenericCommand=vStrIn;
    return;
}

// Test for correct parsing of a generic command
int EosAdimec::UnitTestParseGenericCommandBase(std::vector<std::string>& vStrCmd,
                                               std::string* pStrGenericCommand)
{
    int nStatus;
    nStatus=ParseGenericCommandBase(vStrCmd,pStrGenericCommand);
    
    return nStatus;
}

// Test for correct translation/conversion of a generic command
int EosAdimec::UnitTestTranslateGenericCommand(void)
{
    int nStatus;
   
    nStatus=TranslateGenericCommand();
    
    return nStatus;
}



/*****************************************************************************************
 Functions below are part of the standard way of communicating with a serial device from
 SCIP. The Adimec camera and EURESYS framegrabber combo must use EURESYS's libraries to 
 communicate so these functions are not used. They are stubbed out to satisfy
 * the pure virtual function requirements. */




// Pure-virtual fcn inherited from EosDevice.
// Populate it here.
// Translate hardware-device responses to SCIP generic device status messages.
// Don't think this is used as device comms is handled through EURESYS's libraries. 
// Stubbed out to satisfy the pure virtual function
int EosAdimec::TranslateDeviceResponse(void)
{

    return 0;
}


// Do-nothing stub for this device.
int EosAdimec::SendQueuedDeviceCommands(void)
{

    return UNIX_ERROR_STATUS;
}



