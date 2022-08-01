/**
   This is the UIB power-distribution PCB controller application.
   Command-line args are:
   1) eospower_config.cfg    (configuration file)
   2) pin                    (input named-pipe)
   3) pout                   (output named-pipe)
   4) deviceId               (integer device ID number)

   It can be run in "stand-alone" mode or as part of the
   integrated SCIP system.

 */

#include "EosVersion.h"
#include "EosUsage.h"
#include "EosAdimecSimulator.h"

std::string strProcName_g;

EosAdimecSimulator* pEos_g; // Need this global for the signal handler fcns

void CheckConfiguration(char* pConfig)
{
    EosAdimecSimulator* pEos=NULL;
    try
    {
	std::string strConfigFile=std::string(pConfig);
	pEos=new EosAdimecSimulator(strConfigFile);
	std::cout<<std::endl;
	std::cout<<"If we got this far, no configuration errors were found."
		 <<std::endl;
	std::cout<<std::endl;
	return;
    }
    catch(EosDeviceException &eos)
    {
	std::cerr<<"Exception message: "<<eos.GetExceptionMessage()<<std::endl;
	std::cerr<<"Exception ID: "<<eos.GetExceptionId()<<std::endl;
    }
    catch(EosException &eos1)
    {
	eos1.PrintExceptionStatus();
    }
    if(pEos)
	delete pEos;
    
    return;
}

int main(int argc, char**argv)
{

    void SigPipeHandler(int sig);
    void SigIntHandler(int sig);
  
    strProcName_g=std::string(argv[0]);
    
    // Need this to keep the app from crashing if it writes to
    // a SCIP named-pipe when SCIP main isn't running.
    ::signal(SIGPIPE, SIG_IGN); // Just ignore the SIGPIPE signal

    ::signal(SIGINT,SigIntHandler); // For graceful shutdown on SIGINT

    if(EosUsage(argv,argc)!=0)
	return 1;

    if(2==argc)
    {
	CheckConfiguration(argv[1]);
	return 0;
    }

    EosAdimecSimulator* pEos=NULL;

    try
    {

      // enable for TCP client comms
      pEos=new EosAdimecSimulator(argv[1],argv[2],argv[3],atoi(argv[4]),
                                  ClientComms::eClientCommUsb);

      if(NULL==pEos)
      {
        throw EosDeviceException
            (std::string("Could not allocate EosAdimec"),
             __LINE__);
      }
        
      pEos_g=pEos;

      // Set the lazy-loop sleep duration
      struct timeval tv_selDel;
      tv_selDel.tv_sec=1;
      tv_selDel.tv_usec=0;
      
      // Classify the power-device as a "sensor" device
      // (for lack of anything better)
      pEos->SetDeviceType("ss");
      
      // Runs forever (or until crash/interrupt)
      std::cout<<__FUNCTION__<<"(): Launching RunBase "<<std::endl;
      pEos->RunBase(&tv_selDel);
      
    }
    catch(EosDeviceException &eos)
    {
        std::cerr<<"Exception message: "<<eos.GetExceptionMessage()<<std::endl;
        std::cerr<<"Exception ID: "<<eos.GetExceptionId()<<std::endl;
    }

    delete pEos;
    pEos=NULL;
    
    return 0;
    
}


void SigPipeHandler(int sig)
{
    std::cerr<<"Caught a SIGPIPE "<<std::endl;

    return;
}

void SigIntHandler(int sig)
{
    std::cerr<<"Caught a SIGINT "<<std::endl;
    std::cerr<<__FILE__<<":  Set device to safe mode and shut down..."<<std::endl;

    // Make darned sure that the Sensor is shut down
    if(NULL!=pEos_g)
    {

      // This will initiate a clean shutdown of the EosNamedPipe
      // and EosDevice objects.
	delete pEos_g; 

      // Don't want to invoke destructor from an interrupt 
      // routine -- unpredictable things can happen.
      //      delete pEos_g;

    }
    
    exit(1);

    return;
}
