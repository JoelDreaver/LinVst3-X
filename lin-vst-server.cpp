/*  dssi-vst: a DSSI plugin wrapper for VST effects and instruments
    Copyright 2004-2007 Chris Cannam

    This file is part of linvst.

    linvst is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>

#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <time.h>

#include <sched.h>

#define stricmp strcasecmp
#define strnicmp strncasecmp

/*
#ifdef __WINE__
//#else
#define __cdecl
#endif
*/

/*

#if VST_FORCE_DEPRECATED
#define DEPRECATED_VST_SYMBOL(x) __##x##Deprecated
#else
#define DEPRECATED_VST_SYMBOL(x) x
#endif

*/

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "remotepluginserver.h"

#include "paths.h"

#include "vst2wrapper.sdk.cpp"
#include "vst2wrapper.h"
#include "public.sdk/source/vst/hosting/module.h"

extern "C"
{
	typedef bool (PLUGIN_API *InitModuleProc) ();
	typedef bool (PLUGIN_API *ExitModuleProc) ();
}
static const Steinberg::FIDString kInitModuleProcName = "InitDll";
static const Steinberg::FIDString kExitModuleProcName = "ExitDll";

#define APPLICATION_CLASS_NAME "dssi_vst"
#ifdef TRACKTIONWM 
#define APPLICATION_CLASS_NAME2 "dssi_vst2"
#endif

#define disconnectserver 32143215

#define hidegui2 77775634

#define WM_SYNC (WM_USER + 1)
#define WM_SYNC2 (WM_USER + 2)
#define WM_SYNC3 (WM_USER + 3)
#define WM_SYNC4 (WM_USER + 4)
#define WM_SYNC5 (WM_USER + 5)
#define WM_SYNC6 (WM_USER + 6)
#define WM_SYNC7 (WM_USER + 7)

char libnamesync[4096];

using namespace std;

class RemoteVSTServer : public RemotePluginServer
{
static LRESULT CALLBACK MainProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);         
static LRESULT CALLBACK MainProc2(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam); 
LRESULT CALLBACK realWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK realWndProc2(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
          
public:
    RemoteVSTServer(std::string fileIdentifiers, std::string fallbackName);
    virtual             ~RemoteVSTServer();
   
     // virtual std::string getName() { return m_name; }
     // virtual std::string getMaker() { return m_maker; }
    virtual std::string getName();
    virtual std::string getMaker();
    
    virtual void        setBufferSize(int);
    virtual void        setSampleRate(int);
    virtual void        reset();
    virtual void        terminate();

    virtual int         getInputCount() { return vst2wrap->numinputs; }
    virtual int         getOutputCount() { return vst2wrap->numoutputs; }
    virtual int         getFlags() { if(vst2wrap->synth == true)
    mpluginptr->flags |= effFlagsIsSynth; 
    #ifdef DOUBLEP
    if(vst2wrap->doublereplacing == true)
    mpluginptr->flags |= effFlagsCanDoubleReplacing; 
    #endif
    mpluginptr->flags |= effFlagsCanReplacing; mpluginptr->flags |= effFlagsProgramChunks; return mpluginptr->flags; }
    virtual int         getinitialDelay() { return vst2wrap->initialdelay; }
    virtual int         getUID() { return vst2uid; }
    virtual int         getParameterCount() { return vst2wrap->numparams; }
    virtual std::string getParameterName(int);
    virtual std::string getParameterLabel(int);
    virtual std::string getParameterDisplay(int);    
    virtual void        setParameter(int, float);
    virtual float       getParameter(int);
    virtual void        getParameters(int, int, float *);

    virtual int         getProgramCount() { return vst2wrap->numprograms; }
    virtual int         getProgramNameIndexed(int, char *name);
    virtual std::string getProgramName();
#ifdef WAVES
    virtual int         getShellName(char *name);
#endif
    virtual void        setCurrentProgram(int);

    virtual void        showGUI();
    virtual void        hideGUI();
    virtual void        hideGUI2();      
#ifdef EMBED
    virtual void        openGUI();
#endif
    virtual void        guiUpdate();
    
    virtual int         getEffInt(int opcode, int value);
    virtual std::string getEffString(int opcode, int index);
    virtual void        effDoVoid(int opcode);
    virtual int         effDoVoid2(int opcode, int index, int value, float opt);
  
//    virtual int         getInitialDelay() {return m_plugin[pidx]->initialDelay;}
//    virtual int         getUniqueID() { return m_plugin[pidx]->uniqueID;}
//    virtual int         getVersion() { return m_plugin[pidx]->version;}

    virtual int         processVstEvents();
    virtual void        getChunk();
    virtual void        setChunk();
    virtual void        canBeAutomated();
    virtual void        getProgram();
    virtual void        EffectOpen();
//    virtual void        eff_mainsChanged(int v);

    virtual void        process(float **inputs, float **outputs, int sampleFrames);
#ifdef DOUBLEP
    virtual void        processdouble(double **inputs, double **outputs, int sampleFrames);
    virtual bool        setPrecision(int);  
#endif
#ifndef VESTIGE
    virtual bool        getOutProp(int);
    virtual bool        getInProp(int);
#endif    

#ifdef MIDIEFF
    virtual bool        getMidiKey(int);
    virtual bool        getMidiProgName(int);
    virtual bool        getMidiCurProg(int);
    virtual bool        getMidiProgCat(int);
    virtual bool        getMidiProgCh(int);
    virtual bool        setSpeaker();
    virtual bool        getSpeaker();
#endif
#ifdef CANDOEFF 
    virtual bool        getEffCanDo(std::string);
#endif 

    virtual void        setDebugLevel(RemotePluginDebugLevel level) { debugLevel = level; }

    virtual bool        warn(std::string);

    virtual void        waitForServer();
    virtual void        waitForServerexit();
        
     static VstIntPtr VESTIGECALLBACK hostCallback(AEffect *plugin, VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt)    
     {
RemoteVSTServer *remote5;		 
VstIntPtr rv = 0;
    
     switch (opcode)
     {  
     case audioMasterVersion:
     rv = 2400;
     break;
        
     default:
     {
	 if(plugin)
	 {
     remote5 = (RemoteVSTServer*)plugin->resvd2;
     if(remote5)
     {
     rv = remote5->hostCallback2(plugin, opcode, index, value, ptr, opt);            
     }
     }
     }
     break;           
     }    
     remote5 = nullptr;       
     return rv; 
     } 
    
     VstIntPtr hostCallback2(AEffect *plugin, VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt);
    
    static DWORD WINAPI AudioThreadMain(void* parameter)
    {
    RemoteVSTServer* remote1 = (RemoteVSTServer*)parameter;
    
    while (!remote1->exiting)
    {
    remote1->dispatchProcess(50);
    }    
    // param.sched_priority = 0;
    // (void)sched_setscheduler(0, SCHED_OTHER, &param);
    remote1->audfin = 1;
    remote1 = nullptr;
 //   ExitThread(0);
    return 0;
    }

    static DWORD WINAPI GetSetThreadMain(void* parameter)
    {
    RemoteVSTServer* remote2 = (RemoteVSTServer*)parameter;
 
    while (!remote2->exiting)
    {
    remote2->dispatchGetSet(50);
    }
    // param.sched_priority = 0;
    // (void)sched_setscheduler(0, SCHED_OTHER, &param);
    remote2->getfin = 1;
    remote2 = nullptr;
//    ExitThread(0);
    return 0;
    }

    static DWORD WINAPI ParThreadMain(void* parameter)
    {
    RemoteVSTServer* remote3 = (RemoteVSTServer*)parameter;

    while (!remote3->exiting)
    {
    remote3->dispatchPar(50);
    }
    // param.sched_priority = 0;
    // (void)sched_setscheduler(0, SCHED_OTHER, &param);
    remote3->parfin = 1;
    remote3 = nullptr;
//    ExitThread(0);
    return 0;
    }

    void StartThreadFunc()
    {
    DWORD ThreadID;
    ThreadHandle[0] = CreateThread(0,0,AudioThreadMain,(LPVOID)this,CREATE_SUSPENDED,&ThreadID);
    }
    
    void StartThreadFunc2()
    {
    DWORD ThreadID;
    ThreadHandle[1] = CreateThread(0,0,GetSetThreadMain,(void *)this,CREATE_SUSPENDED,&ThreadID);
    }    
    
    void StartThreadFunc3()
    {
    DWORD ThreadID;
    ThreadHandle[2] = CreateThread(0,0,ParThreadMain,(void *)this,CREATE_SUSPENDED,&ThreadID);
    }   
      
    HWND                hWnd;
    WNDCLASSEX          wclass;
#ifdef TRACKTIONWM  
    WNDCLASSEX          wclass2;
    POINT               offset;
#endif    
    HINSTANCE libHandle;
    HANDLE  ThreadHandle[3];
    UINT_PTR            timerval;
    bool                haveGui;
#ifdef EMBED
    HANDLE handlewin;
    struct winmessage
    {
    int handle;
    int width;
    int height;
    int winerror;    
    } winm2;    
    winmessage *winm;
#endif
    int guiupdate;
    int guiupdatecount;
    int guiresizewidth;
    int guiresizeheight;
    ERect               *rect;
    int                 setprogrammiss;
    int                 hostreaper;
    int                 melda;      
    int                 wavesthread;
#ifdef EMBED
#ifdef TRACKTIONWM  
    int                 hosttracktion;
#endif
#endif
    
    VstEvents           vstev[VSTSIZE];
    AEffect             mplugin;
    AEffect*            mpluginptr;    
    bool                exiting;
    bool                exiting2;
    bool                effectrun;
    bool                inProcessThread;
    bool                guiVisible;
    int                 parfin;
    int                 audfin;
    int                 getfin;
    int                 hidegui; 
    
    Steinberg::Vst::Vst2Wrapper *vst2wrap;
    int vst2uid;
    Steinberg::IPluginFactory* factory;       
    
    std::string         deviceName2;
    std::string         bufferwaves;

    MSG msg;
    
    char wname[4096];
    char wname2[4096]; 
      
    int pidx;
    
    int plugerr; 
    
    RemotePluginDebugLevel  debugLevel;

    std::string         m_name;
    std::string         m_maker;
   
    HANDLE  ghWriteEvent;
    HANDLE  ghWriteEvent2;
    HANDLE  ghWriteEvent3;   
    HANDLE  ghWriteEvent4;   
    HANDLE  ghWriteEvent5;   
    HANDLE  ghWriteEvent7;     
};
 
 struct threadargs {
  char fileInfo[4096];
  char libname[4096];
  int idx;
}threadargs2;  

int finishloop = 1;
int plugincount = 0;
int realplugincount = 0;

DWORD threadIdvst[128000];

HANDLE ThreadHandlevst[128000];

RemoteVSTServer *remoteVSTServerInstance2[128000];

HMODULE remotelib[128000];

HWND hWndvst[128000];

UINT_PTR timerval[128000];

DWORD mainThreadId;

WNDCLASSEX wclass[128000];
      
RemoteVSTServer::RemoteVSTServer(std::string fileIdentifiers, std::string fallbackName) :
    RemotePluginServer(fileIdentifiers),
    m_name(fallbackName),
    m_maker(""),
    setprogrammiss(0),
    hostreaper(0),
#ifdef WAVES
    wavesthread(0),
#endif
#ifdef EMBED
#ifdef TRACKTIONWM  
    hosttracktion(0),
#endif
#endif
    haveGui(true),
    timerval(0),
    exiting(false),
    exiting2(false),
    effectrun(false),
    inProcessThread(false),
    guiVisible(false),
    parfin(0),
    audfin(0),
    getfin(0),
    guiupdate(0),
    guiupdatecount(0),
    guiresizewidth(500),
    guiresizeheight(200),
    melda(0),
    winm(0),
    hWnd(0),
    hidegui(0),
    libHandle(0),
    ghWriteEvent(0),
    ghWriteEvent2(0),
    ghWriteEvent3(0),
    ghWriteEvent4(0),
    ghWriteEvent5(0),
    ghWriteEvent7(0),
    pidx(0),
    plugerr(0),
    vst2wrap(0),
    factory(0),
    mpluginptr(&mplugin),
    vst2uid(0),    
    debugLevel(RemotePluginDebugNone)
{
    ThreadHandle[0] = 0;
    ThreadHandle[1] = 0;
    ThreadHandle[2] = 0;

#ifdef EMBED	
     /*
     winm = new winmessage;  
     if(!winm)
     starterror = 1;
     */
     winm = &winm2;      
#endif	    
}

LRESULT CALLBACK RemoteVSTServer::MainProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
RemoteVSTServer* remote7;

	if (msg == WM_NCCREATE)
	{	
    remote7 = (RemoteVSTServer*)(((LPCREATESTRUCT)lParam)->lpCreateParams);
	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)remote7);
	if(remote7)
	return remote7->realWndProc(hWnd,msg,wParam,lParam);	
	else
    return DefWindowProc(hWnd, msg, wParam, lParam);	
	}
	else
	{
	remote7 = (RemoteVSTServer*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if(remote7)
	return remote7->realWndProc(hWnd,msg,wParam,lParam);
	else
    return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}         
         
LRESULT CALLBACK RemoteVSTServer::MainProc2(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
RemoteVSTServer* remote8;

	if (msg == WM_NCCREATE)
	{	
    remote8 = (RemoteVSTServer*)(((LPCREATESTRUCT)lParam)->lpCreateParams);
	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)remote8);
	if(remote8)
	return remote8->realWndProc2(hWnd,msg,wParam,lParam);	
	else
    return DefWindowProc(hWnd, msg, wParam, lParam);	
	}
	else
	{
	remote8 = (RemoteVSTServer*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if(remote8)
	return remote8->realWndProc2(hWnd,msg,wParam,lParam);
	else
    return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}                  

LRESULT CALLBACK RemoteVSTServer::realWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
    switch (msg)
    {
    case WM_CLOSE:
#ifndef EMBED
         if (!exiting && guiVisible)
	 {
         hidegui = 1;
//         hideGUI();	 
         return 0;
	 }
#endif
    break;
		    
    case WM_TIMER: 
/*
         m_plugin[pidx]->dispatcher ( m_plugin[pidx], effEditIdle, 0, 0, NULL, 0);  
        //    printf("timer %d\n", pidx);              
*/   
     break;
       	
    default:
    return DefWindowProc(hWnd, msg, wParam, lParam);		   
    }
 return 0;
}        
    
LRESULT CALLBACK RemoteVSTServer::realWndProc2(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
    switch (msg)
    {
    case WM_CLOSE:
    break;
		    
    case WM_TIMER:
    break;
       	
    default:
    return DefWindowProc(hWnd, msg, wParam, lParam);		   
    }
 return 0;    
    } 
            
static VstIntPtr VESTIGECALLBACK hostCallback3(AEffect *plugin, VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt)    
{
RemoteVSTServer *remote5;		 
VstIntPtr rv = 0;

     switch (opcode)
     {       
     case audioMasterVersion:
     rv = 2400;
     break;
        
     default:
     {
	 if(plugin)
	 {
     remote5 = (RemoteVSTServer*)plugin->resvd2;
     if(remote5)
     {
     rv = remote5->hostCallback2(plugin, opcode, index, value, ptr, opt);            
     }
     }
     }
     break;           
     }    
     remote5 = nullptr;       
     return rv; 
}  

static VstIntPtr VESTIGECALLBACK hostCallback5(AEffect *plugin, VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt)    
{		 
VstIntPtr rv = 0;
    
     switch (opcode)
     {  
     case audioMasterVersion:
     rv = 2400;
     break;
        
     default:
     break;           
     }    
            
     return rv; 
}          
      
std::string RemoteVSTServer::getName()
{
      char buffer[512];
      memset(buffer, 0, sizeof(buffer));
      vst2wrap->getEffectName (buffer);  	
      if (buffer[0])
      {
      m_name = buffer; 	      
      if(strlen(buffer) < (kVstMaxEffectNameLen - 7))
      m_name = m_name + " [vst3]"; 
      }         	
      return m_name;
}
  
std::string RemoteVSTServer::getMaker()
{
      char buffer[512];
      memset(buffer, 0, sizeof(buffer));
      vst2wrap->getVendorString (buffer);
      if (buffer[0])
      m_maker = buffer;
      return m_maker;
}

void RemoteVSTServer::EffectOpen()
{
    if (debugLevel > 0)
        cerr << "dssi-vst-server[1]: opening plugin" << endl;	
        
    sched_yield();    
    vst2wrap->suspend ();

    sched_yield();     
    vst2wrap->setBlockSize (bufferSize);
    sched_yield();     
    vst2wrap->setSampleRate (sampleRate);        
    
    char buffer[512];
    memset(buffer, 0, sizeof(buffer));
	
    string buffer2 = getMaker();
    strcpy(buffer, buffer2.c_str());

/*
    if (strncmp(buffer, "Guitar Rig 5", 12) == 0)
        setprogrammiss = 1;
    if (strncmp(buffer, "T-Rack", 6) == 0)
        setprogrammiss = 1;
*/
/*		
    if(strcmp("MeldaProduction", buffer) == 0)
    {
    melda = 1;	
    wavesthread = 1;
    }
*/	
#ifdef WAVES2
    if(strcmp("Waves", buffer) == 0)
    {
    mpluginptr->flags |= effFlagsHasEditor;
    haveGui = true;
    wavesthread = 1;
    }
#endif
/*
    if (strncmp(buffer, "IK", 2) == 0)
        setprogrammiss = 1;
*/	
#ifdef TRACKTIONWM 
	if (haveGui == true)
    {
		
        offset.x = 0;
        offset.y = 0;   
        		
    	memset(&wclass2, 0, sizeof(WNDCLASSEX));
        wclass2.cbSize = sizeof(WNDCLASSEX);
        wclass2.style = 0;
	    // CS_HREDRAW | CS_VREDRAW;
        wclass2.lpfnWndProc = DefWindowProc;
        wclass2.cbClsExtra = 0;
        wclass2.cbWndExtra = 0;
        wclass2.hInstance = GetModuleHandle(0);
        wclass2.hIcon = LoadIcon(GetModuleHandle(0), APPLICATION_CLASS_NAME2);
        wclass2.hCursor = LoadCursor(0, IDI_APPLICATION);
        // wclass2.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wclass2.lpszMenuName = "MENU_DSSI_VST2";
        wclass2.lpszClassName = APPLICATION_CLASS_NAME2;
        wclass2.hIconSm = 0;

        if (!RegisterClassEx(&wclass2))
        {       
        UnregisterClassA(APPLICATION_CLASS_NAME, GetModuleHandle(0)); 
        guiVisible = false;        
        winm->handle = 0;
        winm->width = 0;
        winm->height = 0;
        tryWrite(&m_shm[FIXED_SHM_SIZE], winm, sizeof(winmessage));       
        return;
        }

        RECT offsetcl, offsetwin;

        HWND hWnd2 = CreateWindow(APPLICATION_CLASS_NAME2, "LinVst", WS_CAPTION, 0, 0, 200, 200, 0, 0, GetModuleHandle(0), 0);
        GetClientRect(hWnd2, &offsetcl);
        GetWindowRect(hWnd2, &offsetwin);
        DestroyWindow(hWnd2);

        offset.x = (offsetwin.right - offsetwin.left) - offsetcl.right;
        offset.y = (offsetwin.bottom - offsetwin.top) - offsetcl.bottom;
     
        UnregisterClassA(APPLICATION_CLASS_NAME2, GetModuleHandle(0));
}        
#endif

    struct amessage
    {
        int flags;
        int pcount;
        int parcount;
        int incount;
        int outcount;
        int delay;
    } am;

     //   am.flags = plugin->flags;
     //   am.flags = getFlags();
        // remoteVSTServerInstance->mpluginptr->flags;
     //   am.pcount = getProgramCount();
    //    am.parcount = getParameterCount();
        am.incount = getInputCount();
        am.outcount = getOutputCount();
        am.delay = getinitialDelay();
        /*
#ifdef DOUBLEP
        if(vst2wrap->doublereplacing == true)
        am.flags |= effFlagsCanDoubleReplacing; 
        am.flags |= effFlagsCanReplacing; 
#else
        am.flags &= ~effFlagsCanDoubleReplacing;
        am.flags |= effFlagsCanReplacing; 
#endif   
*/

    if((am.incount != m_numInputs) || (am.outcount != m_numOutputs) || (am.delay != m_delay))
    {  


// printf("servup\n");     

    memcpy(&m_shm3[FIXED_SHM_SIZE3], &am, sizeof(am));

    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)audioMasterIOChanged);
   
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();	
    
}

    sched_yield();    	
    vst2wrap->resume ();	
	
    effectrun = true;	
}

RemoteVSTServer::~RemoteVSTServer()
{
DWORD dwWaitResult;	
  
	sched_yield();
	ghWriteEvent5 = 0;
    ghWriteEvent5 = CreateEvent(NULL, TRUE, FALSE, "wm_syncevent5");
    while(0 == PostThreadMessage(mainThreadId, WM_SYNC5, (WPARAM)pidx, (LPARAM) wname))
    sched_yield();
    dwWaitResult = WaitForSingleObject(ghWriteEvent5, 20000); 
    CloseHandle(ghWriteEvent5);
	sched_yield();	
     
    waitForServerexit();	
     
    waitForClient2exit();
    waitForClient3exit();
    waitForClient4exit();
    waitForClient5exit();

	sched_yield();	

/*
#ifdef EMBED		
    if (winm)
    delete winm; 
#endif	
*/
}

void RemoteVSTServer::process(float **inputs, float **outputs, int sampleFrames)
{
    inProcessThread = true;
 
    vst2wrap->processReplacing (inputs, outputs, sampleFrames);
 
    inProcessThread = false;
}

#ifdef DOUBLEP
void RemoteVSTServer::processdouble(double **inputs, double **outputs, int sampleFrames)
{
    inProcessThread = true;

    vst2wrap->processDoubleReplacing(inputs, outputs, sampleFrames);

    inProcessThread = false;
}

bool RemoteVSTServer::setPrecision(int value)
{
bool retval;

     retval = vst2wrap->setProcessPrecision(value);

     return retval;       
}    
#endif

#ifndef VESTIGE
bool RemoteVSTServer::getInProp(int index)
{
VstPinProperties ptr;
bool retval;

        retval = vst2wrap->getInputProperties(index, &ptr);

        tryWrite(&m_shm2[FIXED_SHM_SIZE2 - sizeof(VstPinProperties)], &ptr, sizeof(VstPinProperties));

        return retval;       
}

bool RemoteVSTServer::getOutProp(int index)
{
VstPinProperties ptr;
bool retval;

        retval = vst2wrap->getOutputProperties(index, &ptr);

        tryWrite(&m_shm2[FIXED_SHM_SIZE2 - sizeof(VstPinProperties)], &ptr, sizeof(VstPinProperties));

        return retval;         
}
#endif

#ifdef MIDIEFF
bool RemoteVSTServer::getMidiKey(int index)
{
MidiKeyName ptr;
bool retval;

        retval = m_plugin->dispatcher(m_plugin, effGetMidiKeyName, index, 0, &ptr, 0);
        tryWrite(&m_shm2[FIXED_SHM_SIZE2 - sizeof(MidiKeyName)], &ptr, sizeof(MidiKeyName));

        return retval;       
}

bool RemoteVSTServer::getMidiProgName(int index)
{
MidiProgramName ptr;
bool retval;

        retval = m_plugin->dispatcher(m_plugin, effGetMidiProgramName, index, 0, &ptr, 0);
        tryWrite(&m_shm2[FIXED_SHM_SIZE2 - sizeof(MidiProgramName)], &ptr, sizeof(MidiProgramName));

        return retval;       
}

bool RemoteVSTServer::getMidiCurProg(int index)
{
MidiProgramName ptr;
bool retval;

        retval = m_plugin->dispatcher(m_plugin, effGetCurrentMidiProgram, index, 0, &ptr, 0);
        tryWrite(&m_shm2[FIXED_SHM_SIZE2 - sizeof(MidiProgramName)], &ptr, sizeof(MidiProgramName));

        return retval;       
}

bool RemoteVSTServer::getMidiProgCat(int index)
{
MidiProgramCategory ptr;
bool retval;

        retval = m_plugin->dispatcher(m_plugin, effGetMidiProgramCategory, index, 0, &ptr, 0);
        tryWrite(&m_shm2[FIXED_SHM_SIZE2 - sizeof(MidiProgramCategory)], &ptr, sizeof(MidiProgramCategory));

        return retval;       
}

bool RemoteVSTServer::getMidiProgCh(int index)
{
bool retval;

        retval = m_plugin->dispatcher(m_plugin, effHasMidiProgramsChanged, index, 0, 0, 0);

        return retval;       
}

bool RemoteVSTServer::setSpeaker() 
{
VstSpeakerArrangement ptr;
VstSpeakerArrangement value;
bool retval;

       tryRead(&m_shm2[FIXED_SHM_SIZE2 - (sizeof(VstSpeakerArrangement)*2)], &ptr, sizeof(VstSpeakerArrangement));

       tryRead(&m_shm2[FIXED_SHM_SIZE2 - sizeof(VstSpeakerArrangement)], &value, sizeof(VstSpeakerArrangement));

       retval = m_plugin->dispatcher(m_plugin, effSetSpeakerArrangement, 0, (VstIntPtr)&value, &ptr, 0);

       return retval;  
}       

bool RemoteVSTServer::getSpeaker() 
{
VstSpeakerArrangement ptr;
VstSpeakerArrangement value;
bool retval;

       retval = m_plugin->dispatcher(m_plugin, effSetSpeakerArrangement, 0, (VstIntPtr)&value, &ptr, 0);

       tryWrite(&m_shm2[FIXED_SHM_SIZE2 - (sizeof(VstSpeakerArrangement)*2)], &ptr, sizeof(VstSpeakerArrangement));

       tryWrite(&m_shm2[FIXED_SHM_SIZE2 - sizeof(VstSpeakerArrangement)], &value, sizeof(VstSpeakerArrangement));
 
       return retval;  
}     
#endif	

#ifdef CANDOEFF
bool RemoteVSTServer::getEffCanDo(std::string ptr)
{
        if(m_plugin->dispatcher(m_plugin, effCanDo, 0, 0, (char*)ptr.c_str(), 0))
        return true;
        else
        return false;
}
#endif

int RemoteVSTServer::getEffInt(int opcode, int value)
{
int retval;

    if(opcode == effGetPlugCategory)
    {
    if(vst2wrap->synth == true)
    return 2;
    else 
    return 1;
    }
    
    if(opcode == effMainsChanged)
    {
    if(value == 0)
    vst2wrap->suspend ();
    if(value == 1)
    vst2wrap->resume ();
    return 0;
    }  
    
    if(opcode == effSetProcessPrecision)
    {
    retval = vst2wrap->setProcessPrecision(value);
    return retval;			
    }	      
}

void RemoteVSTServer::effDoVoid(int opcode)
{
    if (opcode == 78345432)
    {
//        hostreaper = 1;
         return;
    }
	
    if (opcode == effClose)
    {
    waitForServerexit();
    
         // usleep(500000);
    terminate();

	return;    
    }
  
     if(opcode == effStartProcess)
    vst2wrap->startProcess ();
    
    if(opcode == effStopProcess)
    vst2wrap->stopProcess ();
}

int RemoteVSTServer::effDoVoid2(int opcode, int index, int value, float opt)
{	
	/*
    if(opcode == hidegui2)
    {
    hidegui = 1;  
#ifdef XECLOSE    
    while(hidegui == 1)
    {    
    sched_yield();
    } 
#endif       
    }  		
	
    if(opcode == effMainsChanged)
    {
    if(value == 0)
    vst2wrap->suspend ();
    if(value == 1)
    vst2wrap->resume ();
    }
    */
    
  #ifdef EMBED
#ifdef TRACKTIONWM  
    if (opcode == 67584930)
    {
        hosttracktion = 1;
        return offset.y;
    }	
    if (opcode == 67584931)
    {
        return offset.x;
    }	    
#endif	
#endif
    
}

std::string RemoteVSTServer::getEffString(int opcode, int index)
{
    char name[512];
    memset(name, 0, sizeof(name));

    if(opcode == effGetParamDisplay)
    vst2wrap->getParameterDisplay (index, name);
 
    if(opcode == effGetParamLabel)
    vst2wrap->getParameterLabel (index, name);
    
    return name;
}

void RemoteVSTServer::setBufferSize(int sz)
{
    if (bufferSize != sz)
    {
    vst2wrap->suspend ();    
    vst2wrap->setBlockSize (sz);
    vst2wrap->resume ();
    bufferSize = sz;
    }
   
    if (debugLevel > 0)
        cerr << "dssi-vst-server[1]: set buffer size to " << sz << endl;
}

void RemoteVSTServer::setSampleRate(int sr)
{
    if (sampleRate != sr)
    {
    vst2wrap->suspend ();
    vst2wrap->setSampleRate (sr);
    vst2wrap->resume ();
    sampleRate = sr;
    }

    if (debugLevel > 0)
        cerr << "dssi-vst-server[1]: set sample rate to " << sr << endl;
}

void RemoteVSTServer::reset()
{
    cerr << "dssi-vst-server[1]: reset" << endl;
}

void RemoteVSTServer::terminate()
{
    exiting = true;	
	
  //  cerr << "RemoteVSTServer::terminate: setting exiting flag" << endl;
}

std::string RemoteVSTServer::getParameterName(int p)
{
    char name[512];
    memset(name, 0, sizeof(name));       
    vst2wrap->getParameterName(p, name);
    return name;
}

std::string RemoteVSTServer::getParameterDisplay(int p)
{
    char name[512];
    memset(name, 0, sizeof(name));       
    vst2wrap->getParameterDisplay(p, name);
    return name;
}

std::string RemoteVSTServer::getParameterLabel(int p)
{
    char name[512];
    memset(name, 0, sizeof(name));       
    vst2wrap->getParameterLabel(p, name);
    return name;
}

void RemoteVSTServer::setParameter(int p, float v)
{ 
    vst2wrap->setParameter(p, v);
}

float RemoteVSTServer::getParameter(int p)
{ 
    return vst2wrap->getParameter(p);
}

void RemoteVSTServer::getParameters(int p0, int pn, float *v)
{
}

int RemoteVSTServer::getProgramNameIndexed(int p, char *name)
{
    if (debugLevel > 1)
        cerr << "dssi-vst-server[2]: getProgramName(" << p << ")" << endl;

    int retval = 0;
    char nameret[512];
    memset(nameret, 0, sizeof(nameret));
    retval = vst2wrap->getProgramNameIndexed (0, p, nameret);    
    strcpy(name, nameret); 
    return retval;
}

std::string
RemoteVSTServer::getProgramName()
{
    if (debugLevel > 1)
        cerr << "dssi-vst-server[2]: getProgramName()" << endl;

    char name[512];
    memset(name, 0, sizeof(name));  
    vst2wrap->getProgramName (name);  
    return name;
}

void
RemoteVSTServer::setCurrentProgram(int p)
{
    if (debugLevel > 1)
        cerr << "dssi-vst-server[2]: setCurrentProgram(" << p << ")" << endl;

/*
    if ((hostreaper == 1) && (setprogrammiss == 1))
    {
        writeIntring(&m_shmControl5->ringBuffer, 1);
        return;
    }
*/

     if (p < vst2wrap->numprograms)
     vst2wrap->setProgram(p);
}

bool RemoteVSTServer::warn(std::string warning)
{
    if (hWnd)
        MessageBox(hWnd, warning.c_str(), "Error", 0);
    return true;
}

void RemoteVSTServer::showGUI()
{
DWORD dwWaitResult;	
	
    winm->winerror = 0;	

    if ((haveGui == false) || (guiVisible == true))
    {
        winm->handle = 0;
        winm->width = 0;
        winm->height = 0;
        tryWrite(&m_shm[FIXED_SHM_SIZE], winm, sizeof(winmessage));     
        return;
    }
		
      memset(wname, 0, 4096);
      memset(wname2, 0, 4096);

      sprintf(wname2, "%d", pidx);
      strcpy(wname, m_name.c_str());
      strcat(wname, wname2);      
      
      ghWriteEvent = 0;
 
      ghWriteEvent = CreateEvent(NULL, TRUE, FALSE, "wm_syncevent");
   
      while(0 == PostThreadMessage(mainThreadId, WM_SYNC, (WPARAM)pidx, (LPARAM) wname))
      sched_yield();
    
      dwWaitResult = WaitForSingleObject(ghWriteEvent, 20000); 
  //     dwWaitResult = MsgWaitForMultipleObjects(1, &ghWriteEvent, TRUE, 20000, QS_ALLEVENTS);

      sched_yield();
          
      CloseHandle(ghWriteEvent);
 
      if(dwWaitResult != WAIT_OBJECT_0)
      {
      cerr << "dssi-vst-server: ERROR: Failed to create window!\n" << endl;
      guiVisible = false;
      winm->handle = 0;
      winm->width = 0;
      winm->height = 0;
      tryWrite(&m_shm[FIXED_SHM_SIZE], winm, sizeof(winmessage));
      return;		
	  }
	  
	  sched_yield();
	  
	  if(hWndvst[pidx] == 0)
      {
      cerr << "dssi-vst-server: ERROR: Failed to create window!\n" << endl;
      guiVisible = false;
      winm->handle = 0;
      winm->width = 0;
      winm->height = 0;
      tryWrite(&m_shm[FIXED_SHM_SIZE], winm, sizeof(winmessage));
      return;		
	  }	  

#ifdef TRACKTIONWM  	  	  
      if(hosttracktion == 1)
      {
      SetWindowPos(hWndvst[pidx], HWND_TOP, GetSystemMetrics(SM_XVIRTUALSCREEN) + offset.x, GetSystemMetrics(SM_YVIRTUALSCREEN) + offset.y, rect->right - rect->left, rect->bottom - rect->top, 0); 
      }
   	  else
	  {        
      SetWindowPos(hWndvst[pidx], HWND_TOP, GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), rect->right - rect->left, rect->bottom - rect->top, 0);
	  }	
#else
      SetWindowPos(hWndvst[pidx], HWND_TOP, GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), rect->right - rect->left, rect->bottom - rect->top, 0);
#endif	    
	  
      winm->width = rect->right - rect->left;
      winm->height = rect->bottom - rect->top;
      winm->handle = (long int) GetPropA(hWndvst[pidx], "__wine_x11_whole_window");
       
      tryWrite(&m_shm[FIXED_SHM_SIZE], winm, sizeof(winmessage));
      
      guiresizewidth = rect->right - rect->left;
      guiresizeheight = rect->bottom - rect->top;     
      
      sched_yield();
}

void RemoteVSTServer::hideGUI2()
{
    hidegui = 1;  
//#ifdef XECLOSE    
    while(hidegui == 1)
    {    
    sched_yield();
    } 
//#endif        				
}	
	
void RemoteVSTServer::hideGUI()
{
DWORD dwWaitResult;	

if((haveGui == false) || (guiVisible == false))
return;
 
      sched_yield();
      ghWriteEvent2 = 0;
      ghWriteEvent2 = CreateEvent(NULL, TRUE, FALSE, "wm_syncevent2");
 
      while(0 == PostThreadMessage(mainThreadId, WM_SYNC2, (WPARAM)pidx, 0))
      sched_yield();
  
      dwWaitResult = WaitForSingleObject(ghWriteEvent2, 20000); 
   //     dwWaitResult = MsgWaitForMultipleObjects(1, &ghWriteEvent2, TRUE, 20000, QS_ALLEVENTS); 
   
      sched_yield();     
      CloseHandle(ghWriteEvent2);
   
      guiVisible = false;
      hidegui = 0; 
       
      sched_yield();        
}

#ifdef EMBED
void RemoteVSTServer::openGUI()
{
DWORD dwWaitResult;	

        sched_yield();
        ghWriteEvent3 = 0;      
        ghWriteEvent3 = CreateEvent(NULL, TRUE, FALSE, "wm_syncevent3");
 
        while (0 == PostThreadMessage(mainThreadId, WM_SYNC3, (WPARAM)pidx, (LPARAM) wname))
        sched_yield();
  
        dwWaitResult = WaitForSingleObject(ghWriteEvent3, 20000);  
    //    dwWaitResult = MsgWaitForMultipleObjects(1, &ghWriteEvent3, TRUE, 20000, QS_ALLEVENTS);
    
        sched_yield();     
        CloseHandle(ghWriteEvent3);       
       
        guiVisible = true;
        sched_yield();          
}
#endif

int RemoteVSTServer::processVstEvents()
{
    int         els;
    int         *ptr;
    int         sizeidx = 0;
    int         size;
    VstEvents   *evptr;

    ptr = (int *) m_shm2;
    els = *ptr;
    sizeidx = sizeof(int);

    if (els > VSTSIZE)
        els = VSTSIZE;

    evptr = &vstev[0];
    evptr->numEvents = els;
    evptr->reserved = 0;

    for (int i = 0; i < els; i++)
    {
        VstEvent* bsize = (VstEvent*) &m_shm2[sizeidx];
        size = bsize->byteSize + (2*sizeof(VstInt32));
        evptr->events[i] = bsize;
        sizeidx += size;
    }
  
    vst2wrap->processEvents (evptr);

    return 1;
}

void RemoteVSTServer::getChunk()
{
#ifdef CHUNKBUF
    int bnk_prg = readIntring(&m_shmControl3->ringBuffer);    
    int sz = vst2wrap->getChunk ((void**) &chunkptr, bnk_prg ? true : false);    
if(sz >= CHUNKSIZEMAX)
{
    writeInt(&m_shm[FIXED_SHM_SIZE], sz);
    return;
}
else
{
    if(sz < CHUNKSIZEMAX)    
    tryWrite(&m_shm[FIXED_SHM_SIZECHUNKSTART], chunkptr, sz);
    writeInt(&m_shm[FIXED_SHM_SIZE], sz);
    return;
}
#else
    void *ptr;
    int bnk_prg = readIntring(&m_shmControl3->ringBuffer);  
    int sz = vst2wrap->getChunk ((void**) &ptr, bnk_prg ? true : false);  
    if(sz < CHUNKSIZEMAX)
    tryWrite(&m_shm[FIXED_SHM_SIZECHUNKSTART], ptr, sz);
    writeInt(&m_shm[FIXED_SHM_SIZE], sz);
    return;
#endif
}

void RemoteVSTServer::setChunk()
{
#ifdef CHUNKBUF
    int sz = readIntring(&m_shmControl3->ringBuffer);
    if(sz >= CHUNKSIZEMAX)
{
    int bnk_prg = readIntring(&m_shmControl3->ringBuffer);
    void *ptr = chunkptr2;    
    int r = vst2wrap->setChunk (ptr, sz, bnk_prg ? true : false);    
    free(chunkptr2);
    writeInt(&m_shm[FIXED_SHM_SIZE], r);
    return;
}
else
{
    int bnk_prg = readIntring(&m_shmControl3->ringBuffer);
    void *ptr = &m_shm[FIXED_SHM_SIZECHUNKSTART];    
    int r = vst2wrap->setChunk (ptr, sz, bnk_prg ? true : false);      
    writeInt(&m_shm[FIXED_SHM_SIZE], r);
    return;
}
#else
    int sz = readIntring(&m_shmControl3->ringBuffer);
    int bnk_prg = readIntring(&m_shmControl3->ringBuffer);
    void *ptr = &m_shm[FIXED_SHM_SIZECHUNKSTART]; 
    int r = vst2wrap->setChunk (ptr, sz, bnk_prg ? true : false); 
    writeInt(&m_shm[FIXED_SHM_SIZE], r);
    return;
#endif
}

void RemoteVSTServer::canBeAutomated()
{
    int param = readIntring(&m_shmControl5->ringBuffer);
    int r = vst2wrap->canParameterBeAutomated (param);
    writeInt(&m_shm[FIXED_SHM_SIZE], r);
}

void RemoteVSTServer::getProgram()
{  
    int r = vst2wrap->getProgram(); 
    writeInt(&m_shm[FIXED_SHM_SIZE], r);
}

#ifdef SEM
void
RemoteVSTServer::waitForServer()
{
    if(m_386run == 0)
    {
    sem_post(&m_shmControl->runServer);

    timespec ts_timeout;
    clock_gettime(CLOCK_REALTIME, &ts_timeout);
    ts_timeout.tv_sec += 60;
    if (sem_timedwait(&m_shmControl->runClient, &ts_timeout) != 0) {
         if(m_inexcept == 0)
         RemotePluginClosedException();
    }
    }
    else
    {
    fpost(&m_shmControl->runServer386);

    if (fwait(&m_shmControl->runClient386, 60000)) {
         if(m_inexcept == 0)
	 RemotePluginClosedException();
    }
   }
}

void
RemoteVSTServer::waitForServerexit()
{
    if(m_386run == 0)
    {
    sem_post(&m_shmControl->runServer);
    }
    else
    {
    fpost(&m_shmControl->runServer386);
   }
}
#else
void
RemoteVSTServer::waitForServer()
{
    fpost(&m_shmControl->runServer);

    if (fwait(&m_shmControl->runClient, 60000)) {
         if(m_inexcept == 0)
	 RemotePluginClosedException();
    }
}

void
RemoteVSTServer::waitForServerexit()
{
    fpost(&m_shmControl->runServer);
}
#endif

#ifdef VESTIGE
VstIntPtr RemoteVSTServer::hostCallback2(AEffect *plugin, VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt)
#else
VstIntPtr RemoteVSTServer::hostCallback2(AEffect *plugin, VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt)
#endif
{
    VstIntPtr rv = 0;
    int retval = 0;

    switch (opcode)
    {
    case audioMasterAutomate:
   //     plugin->setParameter(plugin, index, opt);
   {
    if(!exiting && effectrun)
    {	    
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    writeIntring(&m_shmControl->ringBuffer, index);
    writeFloatring(&m_shmControl->ringBuffer, opt);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
      }
     }
        break;
/*
    case audioMasterVersion:
        rv = 2400;
        break;
*/
    case audioMasterCurrentId:
#ifdef WAVES
    {
    if (!exiting)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
    }
    }
#endif
        break;

    case audioMasterIdle:
        // plugin->dispatcher(plugin, effEditIdle, 0, 0, 0, 0);
        break;

    case audioMasterGetTime:
    {	    
    if (!exiting && effectrun && m_shm3)
    {
#ifndef NEWTIME    
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    writeIntring(&m_shmControl->ringBuffer, value);   
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
/*    
  if(timeinfo)
    {
    memcpy(timeinfo, &m_shm3[FIXED_SHM_SIZE3 - sizeof(VstTimeInfo)], sizeof(VstTimeInfo));
    // printf("%f\n", timeinfo->sampleRate);
    rv = (VstIntPtr)timeinfo;
    }    
*/ 
   memcpy(timeinfo, &m_shm3[FIXED_SHM_SIZE3 - sizeof(VstTimeInfo)], sizeof(VstTimeInfo));

  //   printf("%f\n", timeinfo->sampleRate);
 
  rv = (VstIntPtr)timeinfo;
#else
/*    
  if(timeinfo)
    {
    memcpy(timeinfo, &m_shm3[FIXED_SHM_SIZE3 - sizeof(VstTimeInfo)], sizeof(VstTimeInfo));
    // printf("%f\n", timeinfo->sampleRate);
    rv = (VstIntPtr)timeinfo;
    }    
*/ 
    memcpy(timeinfo, &m_shm3[FIXED_SHM_SIZE3 - sizeof(VstTimeInfo)], sizeof(VstTimeInfo));

  //   printf("%f\n", timeinfo->sampleRate);
  
    rv = (VstIntPtr)timeinfo;    
#endif           
    }
    }   
        break;
		    
    case audioMasterProcessEvents:
        {
            VstEvents   *evnts;
            int         eventnum;
            int         *ptr2;
            int         sizeidx = 0;
            int         ok;
	
            if (!exiting && effectrun)
            {
                evnts = (VstEvents*)ptr;
		    
	        if (!evnts)
                break;
                    
                if (evnts->numEvents <= 0) 
                break;   	    

                eventnum = evnts->numEvents;

                ptr2 = (int *)m_shm3;

                sizeidx = sizeof(int);

                if (eventnum > VSTSIZE)
                    eventnum = VSTSIZE;            

                for (int i = 0; i < eventnum; i++)
                {

                    VstEvent *pEvent = evnts->events[i];
                    if (pEvent->type == kVstSysExType)
                        eventnum--;
                    else
                    {
                        unsigned int size = (2*sizeof(VstInt32)) + evnts->events[i]->byteSize;
                        memcpy(&m_shm3[sizeidx], evnts->events[i], size);
                        sizeidx += size;
                    }
                }
                *ptr2 = eventnum;

    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
  //  writeIntring(&m_shmControl->ringBuffer, value);
   
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
        }       
       }
        break;

    case audioMasterIOChanged:
    {        
    struct amessage
    {
        int flags;
        int pcount;
        int parcount;
        int incount;
        int outcount;
        int delay;
    } am;
		    
    if (!exiting && effectrun)
    {
  //      am.pcount = plugin->numPrograms;
  //      am.parcount = plugin->numParams;
        am.incount = plugin->numInputs;
        am.outcount = plugin->numOutputs;
        am.delay = plugin->initialDelay;
        /*
#ifndef DOUBLEP
        am.flags = plugin->flags;	
        am.flags &= ~effFlagsCanDoubleReplacing;
#else
        am.flags = plugin->flags;	
#endif
* */

 // printf("server %d %d %d \n", am.incount, am.outcount, am.delay);

    if((am.incount != m_numInputs) || (am.outcount != m_numOutputs) || (am.delay != m_delay))
    {  
		/*
    if((am.incount != m_numInputs) || (am.outcount != m_numOutputs))
    {
    if ((am.incount + am.outcount) * m_bufferSize * sizeof(float) < (PROCESSSIZE))
  	
	}	
   
  */
  
 // printf("delay\n");
  
    if(am.delay != m_delay)
    m_delay = am.delay;
  
    memcpy(&m_shm3[FIXED_SHM_SIZE3], &am, sizeof(am));

    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
   
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
   // }
    }
/*	    
    if((am.incount != m_numInputs) || (am.outcount != m_numOutputs))
    {
    if ((am.incount + am.outcount) * m_bufferSize * sizeof(float) < (PROCESSSIZE))
    {
    m_updateio = 1;
    m_updatein = am.incount;
    m_updateout = am.outcount;
    }
    }
    */
/*
        AEffect* update = m_plugin;
        update->flags = am.flags;
        update->numPrograms = am.pcount;
        update->numParams = am.parcount;
        update->numInputs = am.incount;
        update->numOutputs = am.outcount;
        update->initialDelay = am.delay;
*/
       }
      }
        break;

    case audioMasterSizeWindow:   
#ifdef EMBEDRESIZE	    
{
    int opcodegui = 123456789;	
#ifdef EMBED		
    if (hWndvst[pidx] && guiVisible && !exiting && effectrun && (guiupdate == 0))
    {	
    if((guiresizewidth == index) && (guiresizeheight == value))
    break;
    		
    guiresizewidth = index;
    guiresizeheight = value;

    // ShowWindow(hWnd, SW_HIDE);
    // SetWindowPos(hWnd, HWND_TOP, 0, 0, guiresizewidth, guiresizeheight, 0);
	    
#ifdef TRACKTIONWM
    if(hosttracktion == 1)
    SetWindowPos(hWndvst[pidx], HWND_TOP, GetSystemMetrics(SM_XVIRTUALSCREEN) + offset.x, GetSystemMetrics(SM_YVIRTUALSCREEN) + offset.y, index, value, 0); 
#endif	    
	    
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcodegui);
    writeIntring(&m_shmControl->ringBuffer, index);
    writeIntring(&m_shmControl->ringBuffer, value);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
//    guiupdate = 1;
    rv = 1;
    }
#else	
    if (hWndvst[pidx] && guiVisible && !exiting && effectrun && (guiupdate == 0))
    {
/*
//    SetWindowPos(hWnd, 0, 0, 0, index + 6, value + 25, SWP_NOMOVE | SWP_HIDEWINDOW);	
    SetWindowPos(hWnd, 0, 0, 0, index + 6, value + 25, SWP_NOMOVE);	
    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);
*/

    if((guiresizewidth == index) && (guiresizeheight == value))
    break;
    
    guiresizewidth = index;
    guiresizeheight = value;
    guiupdate = 1;	
    rv = 1;
    }
#endif	   
} 
#endif
        break;

    case audioMasterGetSampleRate:
    {
/*				
        if (!exiting && effectrun)
        {
        if (!sampleRate)
        {
            //  cerr << "WARNING: Sample rate requested but not yet set" << endl;
            break;
        }
        plugin->dispatcher(plugin, effSetSampleRate, 0, 0, NULL, (float)sampleRate);
        }
*/	
/*   	
    if (!exiting && effectrun)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
     }
    */
        
    if (!exiting && effectrun)
    {
    rv = sampleRate;
    }
    }
        break;

    case audioMasterGetBlockSize:
    {
/*
        if (!exiting && effectrun)
        {
        if (!bufferSize)
        {
            // cerr << "WARNING: Buffer size requested but not yet set" << endl;
            break;
        }
        plugin->dispatcher(plugin, effSetBlockSize, 0, bufferSize, NULL, 0);
        }
*/	
/*	
    if (!exiting && effectrun)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
    }
 */
 	
    if (!exiting && effectrun)
    {
    rv = bufferSize;
    }
    }
        break;

    case audioMasterGetInputLatency:
/*	
    {	    	
    if (!exiting && effectrun)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
      }
     }
*/
        break;

    case audioMasterGetOutputLatency:
/*	
    {	    	
    if (!exiting && effectrun)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
      }
     }
*/
        break;

    case audioMasterGetCurrentProcessLevel:
        // 0 -> unsupported, 1 -> gui, 2 -> process, 3 -> midi/timer, 4 -> offline
	{	    	
    if (!exiting && effectrun)
    {
        if (inProcessThread)
            rv = 2;
        else
            rv = 1;
    }
    }
        break;

    case audioMasterGetAutomationState:
    {	
    if (!exiting && effectrun)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
      }
     }
   //     rv = 4; // read/write
        break;

    case audioMasterGetVendorString:
    {
    char retstr[512];
		
    if (!exiting)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    strcpy(retstr, &m_shm3[FIXED_SHM_SIZE3]); 
    strcpy((char *)ptr, retstr);
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3 + 512], sizeof(int));
    rv = retval;
      }
     }
        break;

    case audioMasterGetProductString:
    {
    char retstr[512];

    if (!exiting)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    strcpy(retstr, &m_shm3[FIXED_SHM_SIZE3]); 
    strcpy((char *)ptr, retstr);
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3 + 512], sizeof(int));
    rv = retval;
      }
     }
        break;

    case audioMasterGetVendorVersion:
    {	
    if (!exiting)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
    }
    }    
        break;

    case audioMasterVendorSpecific:
        break;

    case audioMasterCanDo:
#ifdef CANDOEFF
    {
    int retval;
     
    if(!exiting)
    {	
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    strcpy(&remoteVSTServerInstance->m_shm3[FIXED_SHM_SIZE3], (char*)ptr);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();

    memcpy(&retval, &remoteVSTServerInstance->m_shm3[FIXED_SHM_SIZE3 + 512], sizeof(int));
    rv = retval;  
    }
    }
#else		    
		if (!strcmp((char*)ptr, "sendVstEvents")
                    || !strcmp((char*)ptr, "sendVstMidiEvent")
                    || !strcmp((char*)ptr, "receiveVstEvents")
                    || !strcmp((char*)ptr, "receiveVstMidiEvents")
#ifdef WAVES
                    || !strcmp((char*)ptr, "shellCategory")
                    || !strcmp((char*)ptr, "supportShell")
#endif
                    || !strcmp((char*)ptr, "acceptIOChanges")
                    || !strcmp((char*)ptr, "startStopProcess")
#ifdef EMBED
#ifdef EMBEDRESIZE
                    || !strcmp((char*)ptr, "sizeWindow")
#endif
#else
                    || !strcmp((char*)ptr, "sizeWindow")
#endif
                    // || !strcmp((char*)ptr, "supplyIdle")
                    )
            rv = 1;
#endif 		    
        break;

    case audioMasterGetLanguage:
        rv = kVstLangEnglish;
        break;

    case audioMasterGetDirectory:
        break;

    case audioMasterUpdateDisplay:
        break;

    case audioMasterBeginEdit:
    {	
    if (!exiting && effectrun)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    writeIntring(&m_shmControl->ringBuffer, index);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
      }
     }	    
        break;

    case audioMasterEndEdit: 
    {   		
    if (!exiting && effectrun)
    {
    writeOpcodering(&m_shmControl->ringBuffer, (RemotePluginOpcode)opcode);
    writeIntring(&m_shmControl->ringBuffer, index);
    commitWrite(&m_shmControl->ringBuffer);
    waitForServer();
    retval = 0;
    memcpy(&retval, &m_shm3[FIXED_SHM_SIZE3], sizeof(int));
    rv = retval;
      }
     }
        break;

    case audioMasterOpenFileSelector:
        break;

    case audioMasterCloseFileSelector:
        break;

    default:
	break;	    
    }
    return rv;
}

VOID CALLBACK TimerProc(HWND hWnd, UINT message, UINT idTimer, DWORD dwTime)
{
   HWND hwnderr = FindWindow(NULL, "LinVst Error");
   SendMessage(hwnderr, WM_COMMAND, IDCANCEL, 0);
}

void RemoteVSTServer::guiUpdate()
{
#ifdef EMBED
#ifdef EMBEDRESIZE
   guiupdatecount += 1;
	
   if(guiupdatecount == 2)
   {
   ShowWindow(hWndvst[pidx], SW_SHOWNORMAL);
   UpdateWindow(hWndvst[pidx]);
   guiupdate = 0;
   guiupdatecount = 0;
   }
#endif
#endif
#ifndef EMBED
//      SetWindowPos(hWnd, 0, 0, 0, guiresizewidth + 6, guiresizeheight + 25, SWP_NOMOVE | SWP_HIDEWINDOW);	
    SetWindowPos(hWndvst[pidx], 0, 0, 0, guiresizewidth + 6, guiresizeheight + 25, SWP_NOMOVE);	
    ShowWindow(hWndvst[pidx], SW_SHOWNORMAL);
    UpdateWindow(hWndvst[pidx]);
    guiupdate = 0;
#endif
}

bool InitModule ()   
{
	return true; 
}

bool DeinitModule ()
{
	return true; 
}

#define mchr(a,b,c,d) ( ((a)<<24) | ((b)<<16) | ((c)<<8) | (d) )

Steinberg::Vst::Vst2Wrapper *createEffectInstance2(audioMasterCallback audioMaster, HMODULE libHandle, std::string libnamepath, std::string partidx, Steinberg::IPluginFactory* factory, int *vstuid, intptr_t remoteptr)
{  
std::string libnamepath2;
bool        test;
Steinberg::FIDString cid;
Steinberg::FIDString firstcid;
int audioclasscount = 0;
char ibuf[256];
int parthit = 0;
int parthit2 = 0;
int audioclassfirst = 0;
int audioclass = 0;
int firstdone = 0;

    if(libnamepath.find(".vst3") != std::string::npos)
    {
    libnamepath.replace(libnamepath.begin() + libnamepath.find(".vst3"), libnamepath.end(), "");
    }
    else if(libnamepath.find(".Vst3") != std::string::npos)
    {
    libnamepath.replace(libnamepath.begin() + libnamepath.find(".Vst3"), libnamepath.end(), "");
    }
    else if(libnamepath.find(".VST3") != std::string::npos)
    {
    libnamepath.replace(libnamepath.begin() + libnamepath.find(".VST3"), libnamepath.end(), "");
    }
         
    auto proc = (GetFactoryProc)::GetProcAddress ((HMODULE)libHandle, "GetPluginFactory");
	
    if(!proc)
    return nullptr;
  
    factory = proc();
    
    if(!factory)
    return nullptr;
        
    Steinberg::PFactoryInfo factoryInfo;
    
    factory->getFactoryInfo (&factoryInfo);
    	
	// cerr << "  Factory Info:\n\tvendor = " << factoryInfo.vendor << "\n\turl = " << factoryInfo.url << "\n\temail = " << factoryInfo.email << "\n\n" << endl;
		
    int countclasses = factory->countClasses ();
   
    if(countclasses == 0)
    return nullptr;
    
    int idval = 0;
		   
    for (Steinberg::int32 i = 0; i < countclasses; i++)			
	{	
	Steinberg::PClassInfo classInfo;	
	
	factory->getClassInfo (i, &classInfo);
						
    if (strcmp(classInfo.category, "Audio Module Class") == 0)
    {     
    if(audioclassfirst == 0 && firstdone == 0)
    {
    audioclassfirst = i;
    firstdone = 1;
    }
                        
    audioclass = i;
    
    memset(ibuf, 0, sizeof(ibuf));
        
    if(i < 10)
    {
    sprintf(ibuf,"%d",0);
    sprintf(&ibuf[1],"%d", i);   
    }
    else    
    sprintf(ibuf,"%d", i);
    
    libnamepath2 = libnamepath + "-" + "part" + "-" + ibuf + ".so";
	 
	test = std::ifstream(libnamepath2.c_str()).good();  
     
    if((atoi(partidx.c_str()) == (10000)) && !test && (audioclasscount > 0))
    {
    parthit = 1;
         
    std::ifstream source(libnamepath + ".so", std::ios::binary);
         
    std::ofstream dest(libnamepath2, std::ios::binary);
     
    dest << source.rdbuf();
         
    source.close();
    dest.close();
    }
    else if((atoi(partidx.c_str()) == (i)) && test && (audioclasscount > 0))
    {
    parthit2 = 1;
    break;
    }
    
    audioclasscount++;
             
	} // Audio Module Class	
    } // for
 
    if((atoi(partidx.c_str()) == (10000)) && (parthit == 1))
    audioclass = audioclassfirst;
        
    if((atoi(partidx.c_str()) == (10000)) && (parthit == 0 && parthit2 == 0))
    audioclass = audioclassfirst;  
    
    Steinberg::PClassInfo classInfo2;     

    factory->getClassInfo (audioclass, &classInfo2);
    cid = classInfo2.cid;
        
    Steinberg::FIDString iid = Steinberg::Vst::IComponent::iid;       
       
    Steinberg::char8 cidString[50];
	Steinberg::FUID (classInfo2.cid).toRegistryString (cidString);
	Steinberg::String cidStr (cidString);
	// cerr << "  Class Info " << audioclass << ":\n\tname = " << classInfo2.name << "\n\tcategory = " << classInfo2.category << "\n\tcid = " << cidStr.text8 () << "\n\n" << endl;	 
	
	idval = mchr(cidStr[1], cidStr[10], cidStr[15], cidStr[20]);  
	
	int idval1 = cidStr[1] + cidStr[2] + cidStr[3] + cidStr[4];
    int idval2 = cidStr[5] + cidStr[6] + cidStr[7]+ cidStr[8];
    int idval3 = cidStr[10] + cidStr[11] + cidStr[12] + cidStr[13];
    int idval4 = cidStr[15] + cidStr[16] + cidStr[17] + cidStr[18];
    int idval5 = cidStr[20] + cidStr[21] + cidStr[22] + cidStr[23];
    int idval6 = cidStr[25] + cidStr[26] + cidStr[27] + cidStr[28];
    int idval7 = cidStr[29] + cidStr[30] + cidStr[31] + cidStr[32];
    int idval8 = cidStr[33] + cidStr[34] + cidStr[35] + cidStr[36];

    int idvalout = 0;

    idvalout += idval1;
    idvalout += idval2 << 1;
    idvalout += idval3 << 2;
    idvalout += idval4 << 3;
    idvalout += idval5 << 4;
    idvalout += idval6 << 5;
    idvalout += idval7 << 6;
    idvalout += idval8 << 7;

    idval += idvalout;
	
	*vstuid = idval;  

    return Steinberg::Vst::Vst2Wrapper::create(factory, cid, idval, audioMaster, remoteptr);
}

DWORD WINAPI VstThreadMain(LPVOID parameter)
{  
char fileInfo[4096];
char libname[4096];
int idx;
int loaderr;
DWORD dwWaitResult;	

      threadargs *args;
      args = (threadargs *) parameter;

      strcpy(fileInfo, args->fileInfo);
      strcpy(libname, args->libname);

      idx = args->idx;
      
      loaderr = 0;
      	
      string deviceName = libname;
      size_t foundext = deviceName.find_last_of(".");
      deviceName = deviceName.substr(0, foundext);
      remoteVSTServerInstance2[idx] = new RemoteVSTServer(fileInfo, deviceName);    
    
      if(!remoteVSTServerInstance2[idx])									
      {
      cerr << "ERROR: Remote VST startup failed" << endl;
      usleep(5000000);        
      sched_yield();      
      remoteVSTServerInstance2[idx] = 0;          
      sched_yield();	
      if(ThreadHandlevst[idx])
      CloseHandle(ThreadHandlevst[idx]);      
      sched_yield();   
      realplugincount--;
 //     ExitThread(0);
      return 0;  
      }
        
      if(remoteVSTServerInstance2[idx]->starterror == 1)
      {
      cerr << "ERROR: Remote VST start error" << endl;
      sched_yield();      
      if(remoteVSTServerInstance2[idx])
      {	 
      remoteVSTServerInstance2[idx]->writeOpcodering(&remoteVSTServerInstance2[idx]->m_shmControl->ringBuffer, (RemotePluginOpcode)disconnectserver);
      remoteVSTServerInstance2[idx]->commitWrite(&remoteVSTServerInstance2[idx]->m_shmControl->ringBuffer);
      remoteVSTServerInstance2[idx]->waitForServer();  
      remoteVSTServerInstance2[idx]->waitForClient2exit();
      remoteVSTServerInstance2[idx]->waitForClient3exit();
      remoteVSTServerInstance2[idx]->waitForClient4exit();
      remoteVSTServerInstance2[idx]->waitForClient5exit();
      usleep(5000000);        
      delete remoteVSTServerInstance2[idx]; 
      }	      
      sched_yield();
      remoteVSTServerInstance2[idx] = 0;          
      sched_yield();	
      if(ThreadHandlevst[idx])
      CloseHandle(ThreadHandlevst[idx]);      
      sched_yield();   
      realplugincount--;
///      ExitThread(0);
      return 0;  
      }  
            
      remoteVSTServerInstance2[idx]->pidx = idx;
      
      remoteVSTServerInstance2[idx]->ghWriteEvent7 = 0;
 
      remoteVSTServerInstance2[idx]->ghWriteEvent7 = CreateEvent(NULL, TRUE, FALSE, "wm_syncevent");
   
      while(0 == PostThreadMessage(mainThreadId, WM_SYNC7, (WPARAM)idx, (LPARAM) libname))
      sched_yield();
    
      dwWaitResult = WaitForSingleObject(remoteVSTServerInstance2[idx]->ghWriteEvent7, 20000); 
      
      CloseHandle(remoteVSTServerInstance2[idx]->ghWriteEvent7);
             
      if(remoteVSTServerInstance2[idx]->plugerr == 1)
      {
      cerr << "Load Error" << endl;
      sched_yield();      
      remoteVSTServerInstance2[idx]->writeOpcodering(&remoteVSTServerInstance2[idx]->m_shmControl->ringBuffer, (RemotePluginOpcode)disconnectserver);
      remoteVSTServerInstance2[idx]->commitWrite(&remoteVSTServerInstance2[idx]->m_shmControl->ringBuffer);
      remoteVSTServerInstance2[idx]->waitForServer();  
      remoteVSTServerInstance2[idx]->waitForClient2exit();
      remoteVSTServerInstance2[idx]->waitForClient3exit();
      remoteVSTServerInstance2[idx]->waitForClient4exit();
      remoteVSTServerInstance2[idx]->waitForClient5exit();
      usleep(5000000);    
      sched_yield();        
      if(remoteVSTServerInstance2[idx])
      delete remoteVSTServerInstance2[idx];           
      sched_yield();
      remoteVSTServerInstance2[idx] = 0;          
      sched_yield();	
      if(ThreadHandlevst[idx])
      CloseHandle(ThreadHandlevst[idx]);      
      sched_yield();   
      realplugincount--;
  //    ExitThread(0);
      return 0;  
      }  
      
      sched_yield(); 
          
      remoteVSTServerInstance2[idx]->ThreadHandle[0] = 0;
      remoteVSTServerInstance2[idx]->ThreadHandle[1] = 0;
      remoteVSTServerInstance2[idx]->ThreadHandle[2] = 0;
         
      remoteVSTServerInstance2[idx]->StartThreadFunc();
      remoteVSTServerInstance2[idx]->StartThreadFunc2();
      remoteVSTServerInstance2[idx]->StartThreadFunc3();  

      if((!remoteVSTServerInstance2[idx]->ThreadHandle[0]) || (!remoteVSTServerInstance2[idx]->ThreadHandle[1]) || (!remoteVSTServerInstance2[idx]->ThreadHandle[2])) 
      {
      cerr << "Load Error" << endl;
      sched_yield();      
      remoteVSTServerInstance2[idx]->writeOpcodering(&remoteVSTServerInstance2[idx]->m_shmControl->ringBuffer, (RemotePluginOpcode)disconnectserver);
      remoteVSTServerInstance2[idx]->commitWrite(&remoteVSTServerInstance2[idx]->m_shmControl->ringBuffer);
      remoteVSTServerInstance2[idx]->waitForServer();  
      remoteVSTServerInstance2[idx]->waitForClient2exit();
      remoteVSTServerInstance2[idx]->waitForClient3exit();
      remoteVSTServerInstance2[idx]->waitForClient4exit();
      remoteVSTServerInstance2[idx]->waitForClient5exit();
      usleep(5000000);    
      sched_yield();        
      if(remoteVSTServerInstance2[idx])
      delete remoteVSTServerInstance2[idx];           
      sched_yield();
      remoteVSTServerInstance2[idx] = 0;          
      sched_yield();	
      if(remoteVSTServerInstance2[idx]->ThreadHandle[0])
      CloseHandle(remoteVSTServerInstance2[idx]->ThreadHandle[0]);   
      if(remoteVSTServerInstance2[idx]->ThreadHandle[1])
      CloseHandle(remoteVSTServerInstance2[idx]->ThreadHandle[1]);  
      if(remoteVSTServerInstance2[idx]->ThreadHandle[2])
      CloseHandle(remoteVSTServerInstance2[idx]->ThreadHandle[2]);    
      if(ThreadHandlevst[idx])
      CloseHandle(ThreadHandlevst[idx]);      
      sched_yield();   
      realplugincount--;
  //    ExitThread(0);
      return 0;  
      }  
      
      sched_yield(); 
      
      ResumeThread(remoteVSTServerInstance2[idx]->ThreadHandle[0]);
      ResumeThread(remoteVSTServerInstance2[idx]->ThreadHandle[1]);
      ResumeThread(remoteVSTServerInstance2[idx]->ThreadHandle[2]);
          
      sched_yield();  
          
      while (!remoteVSTServerInstance2[idx]->exiting)
      {	    
      if(remoteVSTServerInstance2[idx]->hidegui == 1)
      {
      remoteVSTServerInstance2[idx]->hideGUI();
      }
    
      if(remoteVSTServerInstance2[idx]->exiting)
      break;	 
                 
      remoteVSTServerInstance2[idx]->dispatchControl(50);      
      }
           
      remoteVSTServerInstance2[idx]->vst2wrap->plugin.resvd2 = 0;  

      sched_yield();

      remoteVSTServerInstance2[idx]->waitForServerexit();
      remoteVSTServerInstance2[idx]->waitForClient2exit();
      remoteVSTServerInstance2[idx]->waitForClient3exit();
      remoteVSTServerInstance2[idx]->waitForClient4exit();
      remoteVSTServerInstance2[idx]->waitForClient5exit();	
		
   //   WaitForMultipleObjects(3, remoteVSTServerInstance2[idx]->ThreadHandle, TRUE, 5000);
      MsgWaitForMultipleObjects(3, remoteVSTServerInstance2[idx]->ThreadHandle, TRUE, 5000, QS_ALLEVENTS);
  
      for (int idx50=0;idx50<100000;idx50++)
      {
      if (remoteVSTServerInstance2[idx]->parfin && remoteVSTServerInstance2[idx]->audfin && remoteVSTServerInstance2[idx]->getfin)
      break;
      usleep(100);    
      }	

      if (remoteVSTServerInstance2[idx]->ThreadHandle[0])
      CloseHandle(remoteVSTServerInstance2[idx]->ThreadHandle[0]);

      if (remoteVSTServerInstance2[idx]->ThreadHandle[1])
      CloseHandle(remoteVSTServerInstance2[idx]->ThreadHandle[1]);

      if (remoteVSTServerInstance2[idx]->ThreadHandle[2])
      CloseHandle(remoteVSTServerInstance2[idx]->ThreadHandle[2]);
  
      sched_yield();
        
      if(remoteVSTServerInstance2[idx])
      {
      delete remoteVSTServerInstance2[idx];        
      }
      
      sched_yield();
            
      remoteVSTServerInstance2[idx] = 0;  

      if(ThreadHandlevst[idx])
      CloseHandle(ThreadHandlevst[idx]);
      
      sched_yield();
   
      realplugincount--;
     
 //     ExitThread(0);

      return 0; 
}

void SIGTERM_handler(int signal)
{
    if(realplugincount == 0)
    finishloop = 0;   
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdlinexxx, int cmdshow)
{
    string pathName;
    string fileName;
    char cdpath[4096];
    char *libname = 0;
    char *libname2 = 0;
    char *fileInfo = 0;
    char cmdline[4096];
    string libnamepath;
    string partidx;
    char *outname[10];
    int offset = 0;
    int idxout = 0;
    int idxout2 = 0;
    int ci = 0;    

    key_t MyKey2;
    int  ShmID2;      
    char *ShmPTR2;     
    int *sptr;
    int val5;     
    MSG msg;   
    DWORD dwWaitResult10;
     
    cerr << "DSSI VST plugin server v" << RemotePluginVersion << endl;
    cerr << "Copyright (c) 2012-2013 Filipe Coelho" << endl;
    cerr << "Copyright (c) 2010-2011 Kristian Amlie" << endl;
    cerr << "Copyright (c) 2004-2006 Chris Cannam" << endl;
    #ifdef EMBED
    #ifdef VST32SERVER
    cerr << "LinVst3-X version 3.3-32bit" << endl;
    #else
    cerr << "LinVst3-X version 3.3-64bit" << endl;    
    #endif
    #else
    #ifdef VST32SERVER
    cerr << "LinVst3-X version 3.3st-32bit" << endl;
    #else
    cerr << "LinVst3-X version 3.3st-64bit" << endl;    
    #endif    
    #endif
    
    if (signal(SIGTERM, SIGTERM_handler) == SIG_ERR) 
    {
    cerr << "SIGTERM handler error\n" << endl;
    }
  
#ifdef EMBED           
#ifdef VST32SERVER
    MyKey2 = ftok("/usr/bin/lin-vst3-server-x32.exe", 'a');     
    ShmID2 = shmget(MyKey2, 20000, IPC_CREAT | 0666);
    ShmPTR2 = (char *)shmat(ShmID2, NULL, 0); 
#else   
    MyKey2 = ftok("/usr/bin/lin-vst3-server-x.exe", 't');     
    ShmID2 = shmget(MyKey2, 20000, IPC_CREAT | 0666);
    ShmPTR2  = (char *) shmat(ShmID2, NULL, 0);
#endif  
#else
#ifdef VST32SERVER
    MyKey2 = ftok("/usr/bin/lin-vst3-server-xst32.exe", 'a');     
    ShmID2 = shmget(MyKey2, 20000, IPC_CREAT | 0666);
    ShmPTR2 = (char *)shmat(ShmID2, NULL, 0); 
#else   
    MyKey2 = ftok("/usr/bin/lin-vst3-server-xst.exe", 't');     
    ShmID2 = shmget(MyKey2, 20000, IPC_CREAT | 0666);
    ShmPTR2  = (char *) shmat(ShmID2, NULL, 0);
#endif  
#endif       
         
    if(ShmID2 == -1)
    {
    cerr << "ComMemCreateError\n" << endl;
    exit(0);
    }

    struct sched_param param;
    param.sched_priority = 1;

    int result = sched_setscheduler(0, SCHED_FIFO, &param);

    if (result < 0)
    {
    perror("Failed to set realtime priority");
    }
    
    threadargs args2;
    threadargs *args;
    args = &args2;
     
    if(ShmPTR2)
    sptr = (int *)ShmPTR2;
    else
    {
    cerr << "ComMemCreateError2\n" << endl;
    exit(0);
    }
    
#ifdef VST32SERVER
    *sptr = 1001;
#else      
    *sptr = 1002;
#endif 

    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    mainThreadId = GetCurrentThreadId();
    
    for(int cidx=0;cidx<128000;cidx++)
    {
    threadIdvst[cidx] = 0;
    ThreadHandlevst[cidx] = 0;
    remoteVSTServerInstance2[cidx] = 0;
    remotelib[cidx] = 0;
    hWndvst[cidx] = 0;
    timerval[cidx] = 0;
    }    
         
    while(finishloop == 1)
    {  
    val5 = *sptr;		
     
 //   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
 //   for (int loopidx = 0; (loopidx < 10) && PeekMessage(&msg, 0, 0, 0, PM_REMOVE); loopidx++)
//    {	
// Raymond Chen
DWORD dwTimeout = 100;
DWORD dwStart = GetTickCount();
DWORD dwElapsed;

    while ((dwElapsed = GetTickCount() - dwStart) < dwTimeout) 
    {
    DWORD dwStatus = MsgWaitForMultipleObjectsEx(0, NULL, dwTimeout - dwElapsed, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

    if (dwStatus == WAIT_OBJECT_0)   
    {	
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
    {  	    
    switch (msg.message)
    {	
    case WM_SYNC:
    {
	sched_yield();	
	int pidx = (int) msg.wParam;
	if(remoteVSTServerInstance2[pidx])
    {		
	memset(&wclass[pidx], 0, sizeof(WNDCLASSEX));
    wclass[pidx].cbSize = sizeof(WNDCLASSEX);
    wclass[pidx].style = 0;
	    // CS_HREDRAW | CS_VREDRAW;
    wclass[pidx].lpfnWndProc = DefWindowProc;
    wclass[pidx].cbClsExtra = 0;
    wclass[pidx].cbWndExtra = 0;
    wclass[pidx].hInstance = GetModuleHandle(0);
    wclass[pidx].hIcon = LoadIcon(GetModuleHandle(0), remoteVSTServerInstance2[pidx]->wname);
    wclass[pidx].hCursor = LoadCursor(0, IDI_APPLICATION);
        // wclass[pidx].hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wclass[pidx].lpszMenuName = "MENU_DSSI_VST";
    wclass[pidx].lpszClassName = remoteVSTServerInstance2[pidx]->wname;
    wclass[pidx].hIconSm = 0;

    if (!RegisterClassEx(&wclass[pidx]))
    {
    cerr << "RegClassErr\n" << endl;
    sched_yield();	     
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent);   
	break;
    }
    
    hWndvst[pidx] = 0;
				
#ifdef EMBEDDRAG
    hWndvst[pidx] = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES, remoteVSTServerInstance2[pidx]->wname, "LinVst", WS_POPUP, 0, 0, 200, 200, 0, 0, GetModuleHandle(0), 0);
#else
    hWndvst[pidx] = CreateWindowEx(WS_EX_TOOLWINDOW, remoteVSTServerInstance2[pidx]->wname, "LinVst", WS_POPUP, 0, 0, 200, 200, 0, 0, GetModuleHandle(0), 0);
#endif	

    if(!hWndvst[pidx])
    {
    cerr << "WindowCreateErr\n" << endl;
    sched_yield();	     
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent);	
    break;
    }
 
 	sched_yield();	   
    remoteVSTServerInstance2[pidx]->rect = 0;    
    remoteVSTServerInstance2[pidx]->vst2wrap->editor->getRect (&remoteVSTServerInstance2[pidx]->rect);
  	sched_yield();	     
    remoteVSTServerInstance2[pidx]->vst2wrap->editor->open (hWndvst[pidx]);
  	sched_yield();	     
    remoteVSTServerInstance2[pidx]->vst2wrap->editor->getRect (&remoteVSTServerInstance2[pidx]->rect);    
  	sched_yield();	     
	 
    if (!remoteVSTServerInstance2[pidx]->rect)
    {
    cerr << "RectErr\n" << endl;
    remoteVSTServerInstance2[pidx]->winm->winerror = 1; 	    
    sched_yield();	     
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent);    
    break;     
    }
    
    timerval[pidx] = 6788888 + pidx;
    timerval[pidx] = SetTimer(hWndvst[pidx], timerval[pidx], 80, 0);  
         
    sched_yield();	     
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent);
    }
    }
    break;
            
    case WM_SYNC2:
    {	
	sched_yield();				   
	int pidx = (int) msg.wParam;

    if(remoteVSTServerInstance2[pidx])
    {
    remoteVSTServerInstance2[pidx]->vst2wrap->editor->close ();	 	
    sched_yield();	      
    if(timerval[pidx])
    KillTimer(hWndvst[pidx], timerval[pidx]);    
    timerval[pidx] = 0;    
    sched_yield();	       
    if(hWndvst[pidx])
    DestroyWindow(hWndvst[pidx]);   
    UnregisterClassA(remoteVSTServerInstance2[pidx]->wname, GetModuleHandle(0));
   
    sched_yield();	          

	hWndvst[pidx] = 0;		
    sched_yield();	
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent2); 
    }	
    }		   
    break;	
		
    case WM_SYNC3:
    { 
	sched_yield();		 
    int pidx = (int) msg.wParam;	
	
	if(remoteVSTServerInstance2[pidx])
    {		  
    ShowWindow(hWndvst[pidx], SW_SHOWNORMAL);    
    sched_yield();	      
    // ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWndvst[pidx]);       	    
    sched_yield();	 
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent3);  
    }      
    }    
    break; 
    
    case WM_SYNC4:
    /*
    {	
	sched_yield();					   
	int pidx = (int) msg.wParam;
	
	if(remoteVSTServerInstance2[pidx])
    {		  
    m_plugin[pidx]->dispatcher( m_plugin[pidx], effOpen, 0, 0, NULL, 0);
    sched_yield();	
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent4);                      
    }      
    }
    */
    break;
   
    case WM_SYNC5:
    {	
	sched_yield();				   
	int pidx = (int) msg.wParam;
	
    if(remoteVSTServerInstance2[pidx])
    {
	if(remoteVSTServerInstance2[pidx]->vst2wrap)
	{		
    remoteVSTServerInstance2[pidx]->vst2wrap->suspend ();   
    sched_yield();    
    delete remoteVSTServerInstance2[pidx]->vst2wrap;  
    }
      
    sched_yield();	
    if (remoteVSTServerInstance2[pidx]->factory)
    remoteVSTServerInstance2[pidx]->factory->release ();      		
    if(remotelib[pidx])
    {
    ExitModuleProc exitProc = (ExitModuleProc)::GetProcAddress ((HMODULE)remotelib[pidx], kExitModuleProcName);
	if (exitProc)
    exitProc ();
    FreeLibrary(remotelib[pidx]); 
    }   		
    sched_yield();	
    
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent5);  		
    }       
    }
    break;
 
 /*   
    case WM_SYNC6:
    {	
 	sched_yield();	  		   
	int pidx = (int) msg.wParam;
	
	if(remoteVSTServerInstance2[pidx])
    {		
    if(remotelib[pidx])
    FreeLibrary(remotelib[pidx]); 
    sched_yield();	
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent6);   
    }
    }   
    break;     
*/

    case WM_SYNC7:
    {		
 	sched_yield();	
 	
	memset(libnamesync, 0, 4096);
	strcpy(libnamesync, (char *)msg.lParam);
	int pidx = (int) msg.wParam;	 
	
	if(remoteVSTServerInstance2[pidx])
    {		    
    sched_yield();	 
    
    audioMasterCallback hostCallbackFuncPtr = hostCallback3;
               
    remoteVSTServerInstance2[pidx]->vst2wrap = createEffectInstance2 (hostCallbackFuncPtr, remotelib[pidx], libnamepath, partidx, remoteVSTServerInstance2[pidx]->factory, &remoteVSTServerInstance2[pidx]->vst2uid, (intptr_t) remoteVSTServerInstance2[pidx]);
    
    if (!remoteVSTServerInstance2[pidx]->vst2wrap)
    {
    cerr << "MplugErr\n" << endl; 
    remoteVSTServerInstance2[pidx]->plugerr = 1;
    sched_yield();	
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent7);                  
    break;    
    }
    
    if(remoteVSTServerInstance2[pidx]->vst2wrap->editor)
    {
    remoteVSTServerInstance2[pidx]->haveGui = true;
    remoteVSTServerInstance2[pidx]->mpluginptr->flags |= effFlagsHasEditor;    
    }
    else 
    remoteVSTServerInstance2[pidx]->haveGui = false;
       
//    remoteVSTServerInstance2[pidx]->vst2wrap->plugin.resvd2 = (RemoteVSTServer *) remoteVSTServerInstance2[pidx];  
    sched_yield();	
    SetEvent(remoteVSTServerInstance2[pidx]->ghWriteEvent7);   
    }
    }   
    break;     

    case WM_TIMER: 
    /*
    {		
	if(msg.wParam >= 6788888)
	{			
	int valt = msg.wParam - 6788888;
		
	if(valt < plugincount)	
    m_plugin[valt]->dispatcher (m_plugin[valt], effEditIdle, 0, 0, NULL, 0);    	
    }  
    }  
    */
    break;
                      
    default:
    break;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    }  
    }
    }
      
    sched_yield();    
//    usleep(1000);    
      
#ifdef VST32SERVER
    if(val5 == 645)
#else    
    if(val5 == 745)
#endif    
    {
    if(plugincount > 128000)
    continue;
    
    memset(args->fileInfo, 0, 4096);
    memset(args->libname, 0, 4096);
    
    memset(cmdline, 0, 4096);
    /*
    memset(fileInfo, 0, 4096);
    memset(libname, 0, 4096);
    memset(libname2, 0, 4096);
    */
            
    strcpy(cmdline, ShmPTR2 + sizeof(int));
    
    offset = 0;
    idxout = 0;
    ci = 0; 
    partidx = " ";

    for(idxout2=0;idxout2<10;idxout2++)    
    {
    if(outname[idxout2])
    free(outname[idxout2]);   
    }
           
    if(cmdline[0] == '\0')
    {
    continue;
   // return 1;
    }	
    
    if(cmdline)
    { 
    if (cmdline[0] == '"' || cmdline[0] == '\'') offset = 1;                 
    for(ci = offset; cmdline[ci]; ++ci)
    {
    if(cmdline[ci] == ',')
    {
    outname[idxout] = strndup(cmdline + offset, ci - offset);
    ++idxout;
    ++ci;                    
    offset = ci;
    }   
    }
    outname[idxout] = strndup(cmdline + offset, ci - offset);
    }
     
    partidx =  outname[0];   
    libname2 = outname[1];
    fileInfo = outname[2];
    int l = strlen(fileInfo);
    if (fileInfo[l-1] == '"' || fileInfo[l-1] == '\'')
    fileInfo[l-1] = '\0';
    
    // printf("fileinfo %s\n", fileInfo);    

    if (libname2 != NULL)
    {
    if ((libname2[0] == '/') && (libname2[1] == '/'))
    libname = strdup(&libname2[1]);
    else
    libname = strdup(libname2);
    }
    else
    {
    cerr << "Usage: dssi-vst-server <vstname.dll>,<tmpfilebase>" << endl;
    cerr << "(Command line was: " << cmdline << ")" << endl;	    
	continue;    
    // return 1;
    }

    if (!libname || !libname[0] || !fileInfo || !fileInfo[0])
    {
    cerr << "Usage: dssi-vst-server <vstname.dll>,<tmpfilebase>" << endl;
    cerr << "(Command line was: " << cmdline << ")" << endl;	    
	continue;    
    // return 1;
    }
    
    /*
	
    strcpy(cdpath, libname);
    pathName = cdpath;
    fileName = cdpath;
    size_t found = pathName.find_last_of("/");
    pathName = pathName.substr(0, found);
    size_t found2 = fileName.find_last_of("/");
    fileName = fileName.substr(found2 + 1, strlen(libname) - (found2 +1));
    SetCurrentDirectory(pathName.c_str());
    
    */
    
    libnamepath = " ";
    
    libnamepath = libname;

    cerr << "Loading  " << libname << endl;  
    
    remotelib[plugincount] = LoadLibraryA(libname);
    if (!remotelib[plugincount])
    {
    cerr << "LoadvstErr\n" << endl;

 	sched_yield();
#ifdef VST32SERVER
    *sptr = 647;
#else      
    *sptr = 747;
#endif 
 	sched_yield();
    continue;
    }
    
    InitModuleProc initProc = (InitModuleProc)::GetProcAddress ((HMODULE)remotelib[plugincount], kInitModuleProcName);
	if (initProc)
	{
	if (initProc () == false)
	{
	cerr << "initprocerr" << endl;
	
    if(remotelib[plugincount])
    {
    ExitModuleProc exitProc = (ExitModuleProc)::GetProcAddress ((HMODULE)remotelib[plugincount], kExitModuleProcName);
	if (exitProc)
	exitProc ();
    FreeLibrary(remotelib[plugincount]); 
    }
    
 	sched_yield(); 	               
#ifdef VST32SERVER
    *sptr = 647;
#else      
    *sptr = 747;
#endif 
 	sched_yield();
    continue;                    
    }  
    }         
        
    strcpy(args->fileInfo, fileInfo);
    strcpy(args->libname, libname);
     
    args->idx = plugincount;
    
    threadIdvst[plugincount] = 0;
    ThreadHandlevst[plugincount] = CreateThread(0, 0, VstThreadMain, args, CREATE_SUSPENDED, &threadIdvst[plugincount]);

    if (!ThreadHandlevst[plugincount])
    {
    cerr << "ThreadErr\n" << endl;

 	sched_yield();
#ifdef VST32SERVER
    *sptr = 647;
#else      
    *sptr = 747;
#endif 
 	sched_yield();
    continue;
    } 

 	sched_yield();

    ResumeThread(ThreadHandlevst[plugincount]);
 
  	sched_yield();  	           
#ifdef VST32SERVER
    *sptr = 646;
#else      
    *sptr = 746;
#endif 
 	sched_yield();
 	               
    plugincount++;
    realplugincount++;
    }
//    if((plugincount > 0) && (realplugincount == 0))
//    break;
    }
                   
    if(ShmPTR2)
    shmdt(ShmPTR2);  
         
    shmctl(ShmID2,IPC_RMID,NULL);
    
    cerr << "dssi-vst-server[1]: exiting" << endl;
	
    exit(0);
}
