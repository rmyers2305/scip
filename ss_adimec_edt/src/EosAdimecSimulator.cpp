
#include "EosAdimecSimulator.h"

// For configuration checking
EosAdimecSimulator::EosAdimecSimulator(const std::string& strConfigFile)
    : EosDeviceSimulator(strConfigFile)
{
    std::cerr<<"Constructing EosAdimecSimulator "<<std::endl;
    
    //Not saving any of the Config info as it doesn't seem to be used anywhere 
    //outside of the constructor
    
    m_pEosAdimecConfiguration=NULL;
    m_pEosAdimecConfiguration = new EosSlaveCameraConfiguration(strConfigFile);
    if(NULL==m_pEosAdimecConfiguration)
    {
        EosException eexcpt(1,"Null EosAdimecConfiguration pts",
                                __FILE__,__LINE__);
        throw eexcpt;
    }
    
    m_EosAdimecConfigInfo = m_pEosAdimecConfiguration->ExtractConfigInfo();

    // RWM need this to access the proper device profile.
    m_strEosDeviceModel = m_EosAdimecConfigInfo.strDeviceModel;

    bool bProfile=ExtractDeviceProfileBase();
    if(!bProfile)
    {
	EosException excp(1,"Could not extract device profile for "+m_strEosDeviceModel,
			  __FILE__,__LINE__);
	throw excp;
    }

    delete m_pEosAdimecConfiguration;

}


// Operational Constructor
EosAdimecSimulator::EosAdimecSimulator(const std::string& strConfigFile, 
                                               const std::string& strNamedPipeFromMain, 
                                               const std::string& strNamedPipeToMain,
                                               const int nDeviceId,
                                               ClientComms::E_CLIENT_COMM_TYPE eClientCommType)
    
    : EosDeviceSimulator(strConfigFile,strNamedPipeFromMain,strNamedPipeToMain,
                          nDeviceId,eClientCommType)
{
    std::cerr<<"Constructing EosAdimecSimulator "<<std::endl;
    
    //Not saving any of the Config info as it doesn't seem to be used anywhere 
    //outside of the constructor
    
    m_pEosAdimecConfiguration=NULL;
    m_pEosAdimecConfiguration = new EosSlaveCameraConfiguration(strConfigFile);
    if(NULL==m_pEosAdimecConfiguration)
    {
        EosException eexcpt(1,"Null EosAdimecConfiguration pts",
                                __FILE__,__LINE__);
        throw eexcpt;
    }
    
    m_EosAdimecConfigInfo = m_pEosAdimecConfiguration->ExtractConfigInfo();
    
    delete m_pEosAdimecConfiguration;

    m_strEosDeviceModel=m_EosAdimecConfigInfo.strDeviceModel;
    
    m_strTcpClientIp=m_EosAdimecConfigInfo.strTcpClientIp;
    m_nTcpClientPort=m_EosAdimecConfigInfo.nTcpClientPort;

    m_nAutoRecoverMode=m_EosAdimecConfigInfo.nAutoRecoverMode;
    
    m_strEosDeviceType=std::string("SS");


    m_pEosBaseConfigInfo = dynamic_cast<EosConfigInfo*>(&m_EosAdimecConfigInfo);

    // Use strDeviceType as part of an MQTT subscribe topic, if needed.
    // strDeviceType not specified in config file. Set it here
    m_pEosBaseConfigInfo->strDeviceType=m_strEosDeviceType;

    m_bCommandLineMode=true;

    m_nGain=0;
    m_nOffset=0;
    m_nRed=0;
    m_nGreen=0;
    m_nBlue=0;
    
    InitCommandTemplate();
                         
    
}

/* Creates a table of generic commaned ->device-command mappings*/

void EosAdimecSimulator::InitCommandTemplate(void)
{

    m_mapCommandTemplate[EosCmd::SETGAIN]=&EosAdimecSimulator::_FptrSetGain;
    m_mapCommandTemplate[EosCmd::SETOFFSET]=&EosAdimecSimulator::_FptrSetOffset;
    m_mapCommandTemplate[EosCmd::SETRGB]=&EosAdimecSimulator::_FptrSetRgb;

    m_mapCommandTemplate[EosCmd::GETGAIN]=&EosAdimecSimulator::_FptrGetGain;
    m_mapCommandTemplate[EosCmd::GETOFFSET]=&EosAdimecSimulator::_FptrGetOffset;
    m_mapCommandTemplate[EosCmd::GETRGB]=&EosAdimecSimulator::_FptrGetRgb;
        
}


int EosAdimecSimulator::TranslateGenericCommand(){

    if(m_vStrGenericCommand.size()<1){
        return UNIX_ERROR_STATUS;
    }

    
    if(m_mapCommandTemplate.find(m_vStrGenericCommand[0]) != m_mapCommandTemplate.end())
    {
	std::cout<<__FUNCTION__<<"(): Processing cmd "<< m_vStrGenericCommand[0] <<std::endl;

	int nRetVal;
        
        nRetVal = m_mapCommandTemplate[m_vStrGenericCommand[0]](this,m_vStrGenericCommand);
        
// RWM_DISABLED        gettimeofday(&m_tvLastCommand,NULL);
        
        return nRetVal;
    }
    else{
        return UNIX_ERROR_STATUS;
    }
}



/***************************************
 ******* BEGIN FUNCTION POINTERS ********
 ***************************************/

int EosAdimecSimulator::_FptrSetGain(const std::vector<std::string>& vStrArgs)
{

    if(vStrArgs.size()<2)
    {
        std::cerr<<"Invalid number of args for move command"<<std::endl;
        return UNIX_ERROR_STATUS;
    }
    
    std::cout<<__FUNCTION__<<"(): Processing cmd "
	     << vStrArgs[0]<<"["<<vStrArgs[1]<<"]" <<std::endl;
   
    try
    {
	m_nGain=boost::lexical_cast<int>(vStrArgs[1]);
	if(m_nGain<0)
	    m_nGain=0;
	else if(m_nGain>MAX_GAIN)
	    m_nGain=MAX_GAIN;

    }
    catch(...)
    {
	std::cerr<<__FUNCTION__<<"(): Exception thrown..."<<std::endl;
	
	return UNIX_ERROR_STATUS;
    }

    // Send back the gain value
    _FptrGetGain(vStrArgs);
    
    
    return UNIX_OK_STATUS;
}

int EosAdimecSimulator::_FptrSetOffset(const std::vector<std::string>& vStrArgs){

    if(vStrArgs.size()<2)
    {
        std::cerr<<"Invalid number of args for move command"<<std::endl;
        return UNIX_ERROR_STATUS;
    }
    
    std::cout<<__FUNCTION__<<"(): Processing cmd "
	     << vStrArgs[0]<<"["<<vStrArgs[1]<<"]" <<std::endl;
   
    try
    {
	m_nOffset=boost::lexical_cast<int>(vStrArgs[1]);
	if(m_nOffset<0)
	    m_nOffset=0;
	else if(m_nOffset>MAX_OFFSET)
	    m_nOffset=MAX_OFFSET;
    }
    catch(...)
    {
	std::cerr<<__FUNCTION__<<"(): Exception thrown..."<<std::endl;
	
	return UNIX_ERROR_STATUS;
    }

    // Send back the offset value
    _FptrGetOffset(vStrArgs);
    
    
    return UNIX_OK_STATUS;
}

int EosAdimecSimulator::_FptrSetRgb(const std::vector<std::string>& vStrArgs)
{
    if(vStrArgs.size()<4)
    {
        std::cerr<<"Invalid number of args for SetRgbcommand"<<std::endl;
        return UNIX_ERROR_STATUS;
    }
    
    std::cout<<__FUNCTION__<<"(): Processing cmd "
	     << vStrArgs[0]<<"["<<vStrArgs[1]<<"]" <<std::endl;
   
    try
    {
	m_nRed=boost::lexical_cast<int>(vStrArgs[1]);
	if(m_nRed<0)
	    m_nRed=0;
	else if(m_nRed>MAX_RGB)
	    m_nRed=MAX_RGB;

	m_nGreen=boost::lexical_cast<int>(vStrArgs[2]);
	if(m_nGreen<0)
	    m_nGreen=0;
	else if(m_nGreen>MAX_RGB)
	    m_nGreen=MAX_RGB;

	m_nBlue=boost::lexical_cast<int>(vStrArgs[3]);
	if(m_nBlue<0)
	    m_nBlue=0;
	else if(m_nBlue>MAX_RGB)
	    m_nBlue=MAX_RGB;

    }
    catch(...)
    {
	std::cerr<<__FUNCTION__<<"(): Exception thrown..."<<std::endl;

	return UNIX_ERROR_STATUS;
    }

    // Send back the gain value
    _FptrGetRgb(vStrArgs);

    
    return UNIX_OK_STATUS;
}



int EosAdimecSimulator::_FptrGetGain(const std::vector<std::string>& vStrArgs)
{
    std::cout<<__FUNCTION__<<"():  Getting Gain setting... "<<std::endl;

    char cGenericResponse[33];
    
    ::memset(cGenericResponse,'\0',33);
    ::snprintf(cGenericResponse,32,
            " SS%3.3d|%s[%d] \n",
	       m_nEosDeviceId,
	       EosResp::GAINPOS.c_str(),
	       m_nGain);

    m_vStrGenericResponse.push_back(std::string(cGenericResponse));
    
    return UNIX_OK_STATUS;
}

int EosAdimecSimulator::_FptrGetOffset(const std::vector<std::string>& vStrArgs)
{
    std::cout<<__FUNCTION__<<"():  Getting Offset setting... "<<std::endl;

    char cGenericResponse[33];
    
    ::memset(cGenericResponse,'\0',33);
    ::snprintf(cGenericResponse,32,
	       " SS%3.3d|%s[%d] \n",
	       m_nEosDeviceId,
	       EosResp::OFFSETPOS.c_str(),
	       m_nOffset);

    m_vStrGenericResponse.push_back(std::string(cGenericResponse));
    
    return UNIX_OK_STATUS;
}

int EosAdimecSimulator::_FptrGetRgb(const std::vector<std::string>& vStrArgs)
{

    std::cout<<__FUNCTION__<<"():  Getting RGB settings... "<<std::endl;
    
    char cGenericResponse[33];
    
    ::memset(cGenericResponse,'\0',33);
    ::snprintf(cGenericResponse,32,
	       " SS%3.3d|%s[%d,%d,%d] \n",
	       m_nEosDeviceId,
	       EosResp::RGB.c_str(),
	       m_nRed,m_nGreen,m_nBlue);

    

    m_vStrGenericResponse.push_back(std::string(cGenericResponse));
    
    return UNIX_OK_STATUS;
}


