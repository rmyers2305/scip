#pragma once

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   EosAdimecSimulator.h
 * Author: eoslab
 *
 * Created on April 5, 2016, 4:23 PM
 */

#ifdef _BUILD_MQTT_
#include "EosDeviceSimulatorMqtt2.h"
#else
#include "EosDeviceSimulator.h"
#endif

class EosAdimecSimulator : public EosDeviceSimulator{
  public:

    // For configuration checking
    EosAdimecSimulator(const std::string& strConfigFile);
    

    // Operational constructor
    EosAdimecSimulator(const std::string& strConfigFile,
		       const std::string& strNamedPipeFromMain,
		       const std::string& strNamedPipeToMain,
		       const int nDeviceId,
		       ClientComms::E_CLIENT_COMM_TYPE eClientCommType);
        
    virtual ~EosAdimecSimulator(void){delete m_pEosAdimecConfiguration;};
        
        
    struct timeval m_tvSimulatedLast;
    struct timeval m_tvSimulatedNow;
        
  protected:

    static const int MAX_OFFSET=4096;
    static const int MAX_GAIN=800;
    static const int MAX_RGB=255;
	

    EosSlaveCameraConfiguration* m_pEosAdimecConfiguration;
            
    EosSlaveCameraConfigInfo m_EosAdimecConfigInfo;
  
            
    //**************************************************
    //******* INHERITED PURE-VIRTUAL METHODS ***********
    //**************************************************
        
    virtual void InitCommandTemplate(void);
        
    virtual void InitializeDevice(void){};
        
    virtual int TranslateGenericCommand(void);
	
    virtual int DeviceSafeMode(void){return 0;};
	
    virtual int Shutdown(void){return 0;};
	    
    virtual void UpdateDeviceStatus(void){ return; };
    
    //*************************************************
    //******* END VIRTUAL FUNCTIONS *******************
    //*************************************************
        
    void BuildSimulatedResponse(void);
        
        
        
    //*************************************************
    //******* BEGIN FCN PTR WRAPPERS ******************
    //*************************************************
        
    int _FptrSetGain(const std::vector<std::string>& strArgsIn);
    int _FptrSetOffset(const std::vector<std::string>& strArgsIn);
    int _FptrSetRgb(const std::vector<std::string>& strArgsIn);

    int _FptrGetGain(const std::vector<std::string>& strArgsIn);
    int _FptrGetOffset(const std::vector<std::string>& strArgsIn);
    int _FptrGetRgb(const std::vector<std::string>& strArgsIn);
            
    //*************************************************
    //******* END FUNCTOR WRAPPERS ********************
    //*************************************************

    int m_nGain;
    int m_nOffset;
    int m_nRed;
    int m_nGreen;
    int m_nBlue;

    std::map<std::string,
	boost::function<int (EosAdimecSimulator*, 
			     const std::vector<std::string>) > > 
	m_mapCommandTemplate;


};
