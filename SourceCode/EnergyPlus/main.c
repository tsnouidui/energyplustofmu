// Methods for Functional Mock-up Unit Export of EnergyPlus.
///////////////////////////////////////////////////////
// \file   main.c
//
// \brief  FMI functions for FMU export project.
//
// \author Thierry Stephane Nouidui,
//         Simulation Research Group,
//         LBNL,
//         TSNouidui@lbl.gov
//
// \date   2012-08-03
//
///////////////////////////////////////////////////////

// define the model identifier name used in for
// the FMI functions
#define MODEL_IDENTIFIER SmOffPSZ
// define the FMI version supported
#define FMIVERSION "1.0"
#define NUMFMUsMax 10000
#define PATHLEN 10000
#define MAXHOSTNAME  10000
#define MAX_MSG_SIZE 1000
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "utilSocket.h" 
#include "fmiPlatformTypes.h"
#include "fmiFunctions.h"
#include "xml_parser_cosim.h"
#include "defines.h"
#include "reader.h" 
#include <errno.h>

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#include<sys/stat.h>
#include <process.h>
#include <windows.h>
//This is the sleeptime
#define TS 1000
#else
#include <stdarg.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h> /* pid_t */
#include <sys/ioctl.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
//This is the sleeptime
#define TS 1
#endif

typedef struct idfFmu_t {
	int index;
	fmiCallbackFunctions functions;
	char instanceName[PATHLEN];
	char* fmuLocation;
	char* mID;
	char *mGUID;
	int numInVar;
	int numOutVar;
	int sockfd;
	int newsockfd;

	fmiReal timeout; 
	fmiBoolean visible;
	fmiBoolean interactive;
	fmiBoolean loggingOn;

	int firstCallGetReal;
	int firstCallSetReal;
	int firstCallDoStep;

	int firstCallFree;
	int firstCallTerm;
	int firstCallIni;
	int firstCallRes;
	int flaGetWri;
	int flaGetRea;
	int preInDoStep;
	int preInGetReal;
	int preInSetReal;
	int preInFree;
	int preInTerm;
	int preInIni;
	int preInRes;
	int flaWri;
	int flaRea;
	int readReady;
	int writeReady;
	int timeStepIDF;
	int getCounter;
	int setCounter;
	ModelDescription* md;

	fmiReal *inVec;
	fmiReal *outVec;
	fmiReal tStartFMU;
	fmiReal tStopFMU;
	fmiReal nexComm;
	fmiReal simTimSen;
	fmiReal simTimRec;
	fmiReal communicationStepSize;
	fmiReal curComm;

#ifdef _MSC_VER
	int  pid ;
	HANDLE  handle_EP;
#else
	pid_t  pid;
#endif
} idfFmu_t;

static int sumProc;
static int retValIns;
static int zI = 0;
static int insNum = 0;
static int firstCallIns = 1;
char instanceName[PATHLEN];
char preInstanceName[PATHLEN];

static int arrsize = 0;
idfFmu_t **fmuInstances;
static int fmuLocCoun = 0;
#define DELTA 10


////////////////////////////////////////////////////////////////////////////////////
/// create a list of pointer to FMUs
///
///\param s The Pointer to FMU.
////////////////////////////////////////////////////////////////////////////////////
static void addfmuInstances(idfFmu_t* s){
	idfFmu_t **temp;
	if(fmuLocCoun == arrsize){
		temp = (idfFmu_t**)malloc(sizeof(idfFmu_t*) * (DELTA + arrsize));
		arrsize += DELTA;
		memcpy(temp, fmuInstances, fmuLocCoun);
		free(fmuInstances);
		fmuInstances = temp;
	}
	fmuInstances[fmuLocCoun++] = s;
}

////////////////////////////////////////////////////////////////////////////////////
/// write socket description file
///
///\param porNum The port number.
///\param hostName The host name.
///\return 0 if no error occurred.
////////////////////////////////////////////////////////////////////////////////////
int write_socket_cfg(int portNum, const char* hostName)
{
	FILE *fp;
	fp = fopen("socket.cfg", "w");
	if (fp == NULL) {
		printf("Can't open socket.cfg file!\n");
		exit(42);  // STL error code: File not open.
	}

	/////////////////////////////////////////////////////////////////////////////
	// write socket configuration file
	fprintf(fp, "<\?xml version=\"1.0\" encoding=\"ISO-8859-1\"\?>\n");
	fprintf(fp, "<BCVTB-client>\n");
	fprintf(fp, "  <ipc>\n");
	fprintf(fp, "    <socket port=\"%d\" hostname=\"%s\"/>\n", portNum, hostName);
	fprintf(fp, "  </ipc>\n");
	fprintf(fp, "</BCVTB-client>\n");
	fclose(fp);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
/// copy resources file into the calculation folder
///
///\param str the resource file.
///\return 0 if no error occurred.
////////////////////////////////////////////////////////////////////////////////////
static int copy_res (fmiString str)
{
	char *tmp_str;
	int retVal;
	tmp_str = (char*)(calloc(sizeof(char), strlen(str) + 30));

#ifdef _MSC_VER
	sprintf(tmp_str, "xcopy %s%s%s %s /Y", "\"", str,"\"", ".");
#else
	sprintf(tmp_str, "cp %s%s%s %s --force", "\"", str,"\"", ".");
#endif
	retVal = system (tmp_str);
	free (tmp_str);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
/// start simulation
///\return 0 if no error occurred.
////////////////////////////////////////////////////////////////////////////////////
int start_sim(fmiComponent c)
{
	idfFmu_t* _c = (idfFmu_t*)c;

#ifdef _MSC_VER
	FILE *fpBat;
#else
	char *str;
#endif
	FILE *fp;

	// check whether weather file exists.
	fp = fopen(FRUNWEAFILE, "r");

#ifdef _MSC_VER
	fpBat = fopen("EP.bat", "w");
	if (fp != NULL){
		// write the command string
		fprintf(fpBat, "Epl-run.bat %s %s %s %s %s %s %s %s %s %s %s", fmuInstances[_c->index]->mID,
			fmuInstances[_c->index]->mID, "idf", FRUNWEAFILE, "EP", "N", "nolimit", "Y", "Y", "N", "1");
	}
	else
	{
		// write the command string
		fprintf(fpBat, "Epl-run.bat %s %s %s %s %s %s %s %s %s %s %s", fmuInstances[_c->index]->mID,
			fmuInstances[_c->index]->mID, "idf", "\" \"", "NONE", "N", "nolimit", "Y", "Y", "N", "1");
	}
	fclose (fpBat);
	fmuInstances[_c->index]->handle_EP = (HANDLE)_spawnl(P_NOWAIT, "EP.bat", "EP.bat", NULL); 
	if (fmuInstances[_c->index]->handle_EP > 0 ) {
		return 0;
	}
	else {
		return 1;
	}

#else
	if (fp != NULL){
		str = (char*)(calloc(sizeof(char), strlen(fmuInstances[_c->index]->mID) + strlen (FRUNWEAFILE) + 1 + 200));
		sprintf(str, "runenergyplus %s %s", fmuInstances[_c->index]->mID, FRUNWEAFILE);
	}
	else
	{
		str = (char*)(calloc(sizeof(char), strlen(fmuInstances[_c->index]->mID) + 1 + 200));
		sprintf(str, "runenergyplus %s", fmuInstances[_c->index]->mID);
	}

	// execute the command string
	int retVal = system(str);
	if(fp) {
		fclose(fp);
	}
	free (str);
	return retVal;
#endif

}

///////////////////////////////////////////////////////////////////////////////
/// FMI status
///
///\param status FMI status.
///////////////////////////////////////////////////////////////////////////////
//#if 0
static const char* fmiStatusToString(fmiStatus status){
	switch (status){
	case fmiOK:      return "ok";
	case fmiWarning: return "warning";
	case fmiDiscard: return "discard";
	case fmiError:   return "error";		
	case fmiPending: return "pending";	
	default:         return "?";
	}
}
//#endif


///////////////////////////////////////////////////////////////////////////////
/// FMU logger
///
///\param c FMI component.
///\param instanceName FMI string.
///\param status FMI status.
///\param category FMI string.
///\param message Message to be recorded.
///////////////////////////////////////////////////////////////////////////////
//#if 0
static void fmuLogger(fmiComponent c, fmiString instanceName, fmiStatus status,
	fmiString category, fmiString message, ...) {
		char msg[MAX_MSG_SIZE];
		char* copy;
		va_list argp;

		// Replace C format strings
		va_start(argp, message);
		vsprintf(msg, message, argp);

		// Replace e.g. ## and #r12#
		copy = strdup(msg);
		free(copy);

		// Print the final message
		if (!instanceName) instanceName = "?";
		if (!category) category = "?";
		printf("%s %s (%s): %s\n", fmiStatusToString(status), instanceName, category, msg);
}
//#endif


////////////////////////////////////////////////////////////////
///  This method is used to get the fmi types of platform
///\return fmiPlatform.
////////////////////////////////////////////////////////////////
DllExport const char* fmiGetTypesPlatform()
{
	return fmiPlatform;
}

////////////////////////////////////////////////////////////////
///  This method is used to get the fmi version
///\return fmiVersion.
////////////////////////////////////////////////////////////////
DllExport const char* fmiGetVersion()
{   // This function always returns 1.0
	return FMIVERSION;
}

////////////////////////////////////////////////////////////////
///  This method is used to instantiated the FMU
///
///\param instanceName modelIdentifier.
///\param fmuLocation fmuLocation.
///\param fmumimeType fmumimeType.
///\param timeout communication timeout value in milli-seconds.
///\param visible flag to executes the FMU in windowless mode.
///\param interactive flag to execute the FMU in interactive mode.
///\param loggingOn flag to enable or disable debug.
////////////////////////////////////////////////////////////////
DllExport fmiComponent fmiInstantiateSlave(fmiString instanceName,
	fmiString fmuGUID, fmiString fmuLocation,
	fmiString mimetype, fmiReal timeout, fmiBoolean visible,
	fmiBoolean interactive, fmiCallbackFunctions functions,
	fmiBoolean loggingOn)
{
	int retVal;

	fmiString mID;
	fmiString mGUID;

	char *xml_file_p;

	fmiComponent c = (fmiComponent)calloc(1, sizeof(struct idfFmu_t));
	idfFmu_t* _c = (idfFmu_t*)c;

	_c->index = retValIns;
	addfmuInstances (_c);
	insNum++;
	retValIns=insNum;

	// save current folder for late comparison
	strncpy (preInstanceName, instanceName, strlen(instanceName));

	// check whether the path to the resources folder has been provided
	if((fmuLocation == NULL) || (strlen(fmuLocation) == 0)) {
		fmuLogger(0, instanceName, fmiFatal, "Fatal Error", "The path"
			" to the resources folder: %s is not specified!\n", fmuLocation);
		exit(1);
	}
	// copy instanceName to the FMU
	strcpy(fmuInstances[_c->index]->instanceName, instanceName);

	// allocate memory for fmuLocation 
	fmuInstances[_c->index]->fmuLocation = (char *)calloc(sizeof(char), strlen (fmuLocation) + 1);
	// copy the fmuLocation to the FMU
	strcpy(fmuInstances[_c->index]->fmuLocation, fmuLocation);

	// change the directory to make sure that FMUs are not overwritten
	retVal = chdir(fmuLocation);
	if (retVal!=0){
		fmuLogger(0, instanceName, fmiFatal, "Fatal Error", "The path"
			" to the resources folder: %s is not valid!\n", fmuLocation);
		exit(1);
	}

	// create path to xml file
	xml_file_p = (char *)calloc(sizeof(char), strlen (fmuLocation) + strlen (XML_FILE) + 1);
	sprintf(xml_file_p, "%s%s", fmuLocation, XML_FILE);

	// get model description of the FMU
	fmuInstances[_c->index]->md = parse(xml_file_p);
	if (!fmuInstances[_c->index]->md) {
		fprintf(stderr, "Failed to get the modelDescription\n");
		exit(1);
	}

	// gets the modelID of the FMU
	mID = getModelIdentifier(fmuInstances[_c->index]->md);

	// copy model ID to FMU
	fmuInstances[_c->index]->mID = (char *)calloc(sizeof(char), strlen (mID) + 1);
	strcpy(fmuInstances[_c->index]->mID, mID);
	printfDebug("The FMU modelIdentifier is %s.\n", mID);

	// get the model GUID of the FMU
	mGUID = getString(fmuInstances[_c->index]->md, att_guid);
	printfDebug("The FMU modelGUID is %s.\n", mGUID);

	fmuInstances[_c->index]->mGUID = (char *)calloc(sizeof(char), strlen (mGUID) + 1);
	strcpy(fmuInstances[_c->index]->mGUID, mGUID);

	// check whether the model is exported for FMI version 1.0
	if(strcmp(getString(fmuInstances[_c->index]->md, att_fmiVersion), FMIVERSION) != 0){

		fmuLogger(0, instanceName, fmiFatal, "Fatal Error", "Wrong FMI version"
			" FMI version 1.0 is currently supported!\n");
		exit(1);
	}

	// check whether GUIDs are consistent with modelDescription file
	if(strcmp(fmuGUID, mGUID) != 0)
	{
		fmuLogger(0, instanceName, fmiFatal, "Fatal Error", "The given"
			" GUID %s is not equal to the GUID of the binary"
			" (%s)!\n", fmuGUID, mGUID);
		exit(1);
	}

	fmuInstances[_c->index]->timeout = timeout;
	fmuInstances[_c->index]->visible = visible;
	fmuInstances[_c->index]->interactive = interactive; 
	fmuInstances[_c->index]->loggingOn = loggingOn;

	// set the debug for the FMU instance
	setDebug (fmuInstances[_c->index]->loggingOn );

	// free xml_file_p;
	free (xml_file_p);
	// assign number to be used in initialize
	return(c); 
}

////////////////////////////////////////////////////////////////
///  This method is used to initialize the FMU
///
///\param c FMU instance.
///\param tStart simulation start time.
///\param StopTimeDefined stop time define.
///\param tStop simulation stop time.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiInitializeSlave(fmiComponent c, fmiReal tStart, fmiBoolean StopTimeDefined, fmiReal tStop)
{
	int retVal;
	char *resources_p;
	char *varcfg_p;
	idfFmu_t* _c = (idfFmu_t *)c;


#ifdef _MSC_VER
	int sockLength;

#else
	struct stat stat_p;
	socklen_t sockLength;
#endif
	struct sockaddr_in   server_addr;
	int                  port_num;
	char                 ThisHost[10000];
	struct  hostent *hp;

#ifdef _MSC_VER
	WORD wVersionRequested = MAKEWORD(2,2);
	WSADATA wsaData;
#endif

#ifndef _MSC_VER
	mode_t process_mask = umask(0);
#endif 

	// save start of the simulation time step
	fmuInstances[_c->index]->tStartFMU = tStart;
	// save end of smulation time step
	fmuInstances[_c->index]->tStopFMU = tStop;

	// initialize structure variables
	fmuInstances[_c->index]->firstCallGetReal       = 1;
	fmuInstances[_c->index]->firstCallSetReal       = 1;
	fmuInstances[_c->index]->firstCallDoStep        = 1;
	fmuInstances[_c->index]->firstCallFree          = 1;
	fmuInstances[_c->index]->firstCallTerm          = 1;
	fmuInstances[_c->index]->firstCallIni           = 1;
	fmuInstances[_c->index]->firstCallRes           = 1;
	fmuInstances[_c->index]->flaGetWri              = 1;

	fmuInstances[_c->index]->flaGetRea = 0;
	fmuInstances[_c->index]->preInDoStep = 0;
	fmuInstances[_c->index]->preInGetReal = 0;
	fmuInstances[_c->index]->preInSetReal = 0;
	fmuInstances[_c->index]->preInFree = 0;
	fmuInstances[_c->index]->preInTerm = 0;
	fmuInstances[_c->index]->preInIni = 0;
	fmuInstances[_c->index]->preInRes = 0;
	fmuInstances[_c->index]->flaWri = 0;
	fmuInstances[_c->index]->flaRea = 0;
	fmuInstances[_c->index]->numInVar  = -1;
	fmuInstances[_c->index]->numOutVar = -1;
	fmuInstances[_c->index]->getCounter = 0;
	fmuInstances[_c->index]->setCounter = 0;

	// change the directory to make sure that FMUs are not overwritten
	if (fmuInstances[_c->index]->firstCallIni || fmuInstances[_c->index]->preInIni!= fmuInstances[_c->index]->index) {
		retVal = chdir(fmuInstances[_c->index]->fmuLocation);
	}
	if (retVal!=0){
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, "Fatal Error", 
			"The path to the resources folder: %s is not valid!\n", fmuInstances[_c->index]->fmuLocation);
		return fmiFatal;
	}
	fmuInstances[_c->index]->preInIni = fmuInstances[_c->index]->index;

	///////////////////////////////////////////////////////////////////////////////////
	// create the socket server

#ifdef _MSC_VER
	// initializes winsock  /************* Windows specific code ********/
	if (WSAStartup(wVersionRequested, &wsaData)!= 0)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName , fmiFatal, 
			"Fatal Error", "WSAStartup failed with error %ld!\n", WSAGetLastError());
		WSACleanup();
		return fmiFatal;
	}
	// check if the version is supported
	if (LOBYTE(wsaData.wVersion)!= 2 || HIBYTE(wsaData.wVersion)!= 2 )
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, 
			"Fatal Error", "Could not find a usable WinSock DLL for WinSock version %u.%u!\n",
			LOBYTE(wsaData.wVersion),HIBYTE(wsaData.		wVersion));
		WSACleanup();
		return fmiFatal;
	}
#endif  /************* End of Windows specific code *******/

	fmuInstances[_c->index]->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	// check for errors to ensure that the socket is a valid socket.
	if (fmuInstances[_c->index]->sockfd == INVALID_SOCKET)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, 
			"Fatal Error", "Opening socket failed"
			" sockfd = %d!\n", fmuInstances[_c->index]->sockfd);
		return fmiFatal;
	}

	printfIntDebug("The sockfd is %d.\n", fmuInstances[_c->index]->sockfd);
	// initialize socket structure server address information
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;                 // Address family to use
	server_addr.sin_port = htons(0);                  // Port number to use
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any IP address

	// bind the socket
	if (bind(fmuInstances[_c->index]->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, 
			"Fatal Error", "bind() failed!\n");
		closeipcFMU (&(fmuInstances[_c->index]->sockfd));
		return fmiFatal;
	}

	// get socket information information
	sockLength = sizeof(server_addr);
	if ( getsockname (fmuInstances[_c->index]->sockfd, (struct sockaddr *)&server_addr, &sockLength)) {
		fmuLogger(0,  fmuInstances[_c->index]->instanceName, fmiFatal,
			"Fatal Error", "Get socket name failed!\n");
		return fmiFatal;
	}

	// get the port number
	port_num= ntohs(server_addr.sin_port);
	printfIntDebug("The port number is %d.\n", port_num);

	// get the hostname information
	gethostname(ThisHost, MAXHOSTNAME);
	if  ((hp = gethostbyname(ThisHost)) == NULL ) {
		fmuLogger(0,  fmuInstances[_c->index]->instanceName, fmiFatal, 
			"Fatal Error", "Get host by name failed!\n");
		return fmiFatal;
	}

	// write socket cfg file
	retVal = write_socket_cfg (port_num, ThisHost);
	printfDebug("This hostname is %s.\n", ThisHost);
	if  (retVal != 0) {
		fmuLogger(0,  fmuInstances[_c->index]->instanceName, fmiFatal, 
			"Fatal Error", "Write socket cfg failed!\n");
		return fmiFatal;
	}
	// listen to the port
	if (listen(fmuInstances[_c->index]->sockfd, 1) == SOCKET_ERROR)
	{
		fmuLogger(0,  fmuInstances[_c->index]->instanceName, fmiFatal, "Fatal Error", "listen() failed!\n");
		closeipcFMU (&(fmuInstances[_c->index]->sockfd));
		return fmiFatal;
	}
	printfIntDebug("TCPServer Server waiting for clients on port: %d.\n", port_num);

#ifndef _MSC_VER

	fmuInstances[_c->index]->pid = fork();
	if (fmuInstances[_c->index]->pid < 0)
	{
		perror("Fork failed!\n");
		exit(1);
	}
	if (fmuInstances[_c->index]->pid != 0 )
	{
		fmuInstances[_c->index]->newsockfd = accept(fmuInstances[_c->index]->sockfd, NULL, NULL);
		printDebug ("The connection has been accepted!\n");
	}
#endif


#ifdef _MSC_VER
	fmuInstances[_c->index]->pid = 0;
#endif

	// get the number of input variables of the FMU
	if (fmuInstances[_c->index]->numInVar ==-1)
	{
		fmuInstances[_c->index]->numInVar = getNumInputVariablesInFMU (fmuInstances[_c->index]->md);
	}
	printfIntDebug("The number of input variables is %d!\n", fmuInstances[_c->index]->numInVar);

	// get the number of output variables of the FMU
	if (fmuInstances[_c->index]->numOutVar ==-1)
	{
		fmuInstances[_c->index]->numOutVar =  getNumOutputVariablesInFMU (fmuInstances[_c->index]->md);
	}
	printfIntDebug("The number of output variables is %d!\n", fmuInstances[_c->index]->numOutVar);

	if ( (fmuInstances[_c->index]->numInVar + fmuInstances[_c->index]->numOutVar) == 0){
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, "Fatal Error", 
			"The FMU has no input and output variables. Please check the model description file!\n");
		return fmiFatal;
	}

	// fmiInitialize just active when pid of the child is invoked
	if ((fmuInstances[_c->index]->pid == 0)){

		// determine resources folder path
		resources_p = (char *)calloc(sizeof(char), strlen (fmuInstances[_c->index]->fmuLocation) + strlen (RESOURCES) + 1);
		sprintf(resources_p, "%s%s", fmuInstances[_c->index]->fmuLocation, RESOURCES);
		// create the input and weather file for the run
		retVal = createRunInFile(fmuInstances[_c->index]->tStartFMU , fmuInstances[_c->index]->tStopFMU, 
			fmuInstances[_c->index]->mID, resources_p);
		if  (retVal != 0) {

			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, 
				"Fatal Error", "The Slave could not be initialized!\n");
			fprintf(stderr, "Can't create input file cfg!\n");
			return fmiFatal;
		}
#ifndef _MSC_VER
		// create a directory and copy the weather file into it
		if (!stat (FRUNWEAFILE, &stat_p))
		{
			if (stat ("WeatherData", &stat_p))
			{
				char *str;
				mkdir ("WeatherData", S_IRWXU | S_IRWXG | S_IRWXO);
				str = (char *)calloc(sizeof(char), strlen (FRUNWEAFILE) + strlen ("WeatherData/") + 50);
				sprintf(str, "cp %s %s --force", FRUNWEAFILE, "WeatherData/");
				retVal = system (str);
				free(str);
			}
			// set environment variable for weather file
			setenv ("ENERGYPLUS_WEATHER", "WeatherData", 0);
		}
#endif
		// determine variables cfg path
		varcfg_p = (char *)calloc(sizeof(char), strlen (resources_p) + strlen (VARCFG) + 1);
		sprintf(varcfg_p, "%s%s", resources_p, VARCFG);

		// copy the variables.cfg from the resources to the calc folder
		retVal = copy_res (varcfg_p);
		if  (retVal != 0) {
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, 
				"Fatal Error", "The Slave could not be initialized!\n");
			fprintf(stderr, "Can't find variables.cfg in resources folder. check if the variables.cfg exists!\n");
			return fmiFatal;
		}
		// free resources folder path
		free (resources_p);
		// free variables.cfg path
		free (varcfg_p);
#ifndef _MSC_VER
		umask(process_mask);
#endif
		// start the simulation
		retVal = start_sim(c);
#ifdef _MSC_VER
		fmuInstances[_c->index]->newsockfd = accept(fmuInstances[_c->index]->sockfd, NULL, NULL);
		printDebug ("The connection has been accepted!\n");
#endif
		// check whether the simulation could start successfully
#ifndef _MSC_VER
		if  (retVal < 0) {
#else
		if  (retVal > 0) {
#endif
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, 
				"Fatal Error", "The Slave could not be initialized!");
			fprintf(stderr, "Can't start EnergyPlus. check if EnergyPlus is installed and on the system path!\n");
			return fmiFatal;
		}

#ifdef _MSC_VER
		fmuInstances[_c->index]->pid = 1;
#endif
		// reset firstCallIni
		if (fmuInstances[_c->index]->firstCallIni) 
		{
			fmuInstances[_c->index]->firstCallIni = 0;
		}
		return fmiOK;
	}
	return fmiOK;
} 

////////////////////////////////////////////////////////////////
///  This method is used to do the time stepping the FMU
///
///\param c FMU instance.
///\param currentCommunicationPoint communication point.
///\param communicationStepSize communication step size.
///\param newStep flag to accept or refect communication step.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiDoStep(fmiComponent c, fmiReal currentCommunicationPoint, fmiReal communicationStepSize, fmiBoolean newStep)
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		int retVal;
		FILE *fp;

		// get current communication point
		fmuInstances[_c->index]->curComm = currentCommunicationPoint;
		// get current communication step size
		fmuInstances[_c->index]->communicationStepSize = communicationStepSize;
		// assign current communication point to value to be sent
		fmuInstances[_c->index]->simTimSen = fmuInstances[_c->index]->curComm;
		if (fmuInstances[_c->index]->firstCallDoStep || (fmuInstances[_c->index]->index!=fmuInstances[_c->index]->preInDoStep)){
			// change the directory to make sure that FMUs are not overwritten
			retVal = chdir(fmuInstances[_c->index]->fmuLocation);
		}
		// save previous index of doStep
		fmuInstances[_c->index]->preInDoStep = fmuInstances[_c->index]->index;

		// check if timeStep is defined
		if (fmuInstances[_c->index]->firstCallDoStep){
			// initialize the nexComm value to start communication point
			fmuInstances[_c->index]->nexComm = currentCommunicationPoint;
			if((fp = fopen(FTIMESTEP, "r")) != NULL) {
				retVal = fscanf(fp, "%d", &(fmuInstances[_c->index]->timeStepIDF));
			}
			else
			{
				fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, "Fatal Error", 
					"The Slave could not be initialized!\n");
				fprintf(stderr, "Can't read time step file!\n");
				return fmiFatal;
			}
		}

		// check for the first communication instant
		if (fmuInstances[_c->index]->firstCallDoStep && (fabs(fmuInstances[_c->index]->curComm - 
			fmuInstances[_c->index]->tStartFMU) > 1e-10))
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, "Fatal", 
				"fmiDoStep: An error occured in a previous call. First communication time != tStart from fmiInitialize!\n");
			return fmiFatal;
		}
		// check if FMU needs to reject time step
		if(!newStep)
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, "Fatal", 
				"fmiDoStep: FMU can not reject time steps.");
			return fmiFatal;
		}

		// check whether the communication step size is different from null
		if (fmuInstances[_c->index]->communicationStepSize == 0)
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, 
				"Fatal", "fmiDoStep: An error occured in a previous call. CommunicationStepSize cannot be null!\n");
			return fmiFatal;
		}

		// check whether the communication step size is different from time step in input file
		if ( fabs(fmuInstances[_c->index]->communicationStepSize - (3600/fmuInstances[_c->index]->timeStepIDF)) > 1e-10)
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, "Fatal", "fmiDoStep:"
				" An error occured in a previous call. CommunicationStepSize is different from time step in input file!\n");
			return fmiFatal;
		}

		// check whether communication point is valid
		if ((fmuInstances[_c->index]->curComm) < 0 || ((fmuInstances[_c->index]->firstCallDoStep == 0) 
			&& (fmuInstances[_c->index]->curComm > fmuInstances[_c->index]->nexComm))){
				fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiFatal, "Fatal", "fmiDoStep:"
					" An error occured in a previous call. Communication point must be positive and monoton increasing!\n");
				return fmiFatal;
		}

		// check whether current communication point is valid
		if ((fmuInstances[_c->index]->firstCallDoStep == 0)
			&& (fabs(fmuInstances[_c->index]->curComm - fmuInstances[_c->index]->nexComm) > 1e-10))
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, "Error", "fmiDoStep: "
				"Current communication point: %f is not equals to the previous simulation time + "
				"communicationStepSize: %f. Simulation will terminate!\n",
				fmuInstances[_c->index]->curComm, fmuInstances[_c->index]->communicationStepSize);
		}

		// check end of simulation
		if (fmuInstances[_c->index]->curComm == fmuInstances[_c->index]->tStopFMU){
			// set the communication flags to 1 to send stop signal to EnergyPlus
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiWarning, 
				"fmiWarning", "fmiDoStep: Current communication point: %f of FMU instance: %s "
				"is equals to end of simulation: %f. Simulation will terminate!\n", 
				fmuInstances[_c->index]->curComm, fmuInstances[_c->index]->instanceName, fmuInstances[_c->index]->tStopFMU);
			fmiFreeSlaveInstance (c);
			return fmiWarning;
		}

		// check if current communication is larger than end of simulation
		if (fmuInstances[_c->index]->curComm > fmuInstances[_c->index]->tStopFMU){
			// set the communication flags to 1 to send stop signal to EnergyPlus
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, "Error", "fmiDoStep:"
				" Current communication point: %f is larger than end of simulation time: %f. Simulation will terminate!\n", 
				fmuInstances[_c->index]->curComm, fmuInstances[_c->index]->tStopFMU);
			return fmiError;
		}

		// check end of simulation
		if (fmuInstances[_c->index]->curComm + 
			fmuInstances[_c->index]->communicationStepSize > fmuInstances[_c->index]->tStopFMU){
				// set the communication flags to 1 to send stop signal to EnergyPlus
				fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, "Error", "fmiDoStep: "
					"Current communication point: %f  + communicationStepsize: %f  is larger than "
					"end of simulation time: %f. Simulation will terminate!\n", 
					fmuInstances[_c->index]->curComm, fmuInstances[_c->index]->communicationStepSize,  
					fmuInstances[_c->index]->tStopFMU);
				return fmiError;
		}

		// check if outputs are got
		if ((fmuInstances[_c->index]->firstCallDoStep == 1) 
			&& !fmuInstances[_c->index]->readReady) 
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, 
				"fmiError", "fmiDoStep: An error occured in a previous call. "
				"All outputs of FMU instance %s are not get before first call of fmiDoStep. "
				"Please call fmiGetReal before doing the first step!\n", 
				fmuInstances[_c->index]->instanceName);
			return fmiError;
		}

		// check if inputs are set
		if ((fmuInstances[_c->index]->firstCallDoStep == 1) 
			&& !fmuInstances[_c->index]->writeReady) 
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, 
				"fmiError", "fmiDoStep: An error occured in a previous call. "
				"All inputs of FMU instance %s are not set before first call of fmiDoStep. "
				"Please call fmiSetReal before doing the first step!\n", 
				fmuInstances[_c->index]->instanceName);
			return fmiError;
		}

		// check if outputs are got
		if ((fmuInstances[_c->index]->firstCallDoStep == 0) 
			&& !fmuInstances[_c->index]->readReady) 
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, 
				"fmiError", "fmiDoStep: An error occured in a previous call. All outputs of FMU instance %s are not get!\n", 
				fmuInstances[_c->index]->instanceName);
			return fmiError;
		}

		// check if inputs are set
		if ((fmuInstances[_c->index]->firstCallDoStep == 0) 
			&& !fmuInstances[_c->index]->writeReady) 
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, 
				"fmiError", "fmiDoStep: An error occured in a previous call. All inputs of FMU instance %s are not set!\n", 
				fmuInstances[_c->index]->instanceName);
			return fmiError;
		}
		// check whether all input and outputs are available and then do the time stepping
		if (fmuInstances[_c->index]->firstCallDoStep
			||
			((fmuInstances[_c->index]->firstCallDoStep == 0) 
			&& fmuInstances[_c->index]->readReady 
			&& fmuInstances[_c->index]->writeReady)
			&& fmuInstances[_c->index]->curComm <= (fmuInstances[_c->index]->tStopFMU - 
			fmuInstances[_c->index]->communicationStepSize)) {
				if (fmuInstances[_c->index]->flaWri != 1){
					fmuInstances[_c->index]->flaGetWri = 1;
					fmuInstances[_c->index]->flaGetRea = 1;
					retVal = writetosocketFMU(&(fmuInstances[_c->index]->newsockfd), &(fmuInstances[_c->index]->flaWri),
						&fmuInstances[_c->index]->numInVar, &zI, &zI, &(fmuInstances[_c->index]->simTimSen),
						fmuInstances[_c->index]->inVec, NULL, NULL);
				}
				fmuInstances[_c->index]->readReady = 0;
				fmuInstances[_c->index]->writeReady = 0;
				fmuInstances[_c->index]->setCounter = 0;
				fmuInstances[_c->index]->getCounter = 0;
		}

		// calculate next communication point
		fmuInstances[_c->index]->nexComm = fmuInstances[_c->index]->curComm + fmuInstances[_c->index]->communicationStepSize;
		// set the firstcall flag to zero
		if (fmuInstances[_c->index]->firstCallDoStep)
		{
			fmuInstances[_c->index]->firstCallDoStep = 0;
		}
		return fmiOK;
	}
	return fmiOK;
}  

////////////////////////////////////////////////////////////////
///  This method is used to cancel a step in the FMU
///
///\param c FMU instance.
///\return fmiWarning if error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiCancelStep(fmiComponent c)
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiWarning, 
			"Warning", "The function fmiCancelStep(..) is not provided!\n");
		return fmiWarning;
	}
	return fmiWarning;
}

////////////////////////////////////////////////////////////////
///  This method is used to terminate the FMU instance
///
///\param c FMU instance.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiTerminateSlave(fmiComponent c)
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		int retVal;
#ifndef _MSC_VER
		int status;
#endif

		if (fmuInstances[_c->index]->firstCallFree == 0)
		{
			printf ("fmiFreeSlaveInstance(..) was already called on FMU instance %s!\n", fmuInstances[_c->index]->instanceName);
			return fmiOK;
		}

		if (fmuInstances[_c->index]->firstCallTerm == 0)
		{
			printf ("fmiTerminateSlave(..) was already called on FMU instance %s!\n", fmuInstances[_c->index]->instanceName);
			return fmiOK;
		}
		if (fmuInstances[_c->index]->firstCallTerm || (fmuInstances[_c->index]->index!=fmuInstances[_c->index]->preInTerm)){
			// change the directory to make sure that FMUs are not overwritten
			retVal = chdir(fmuInstances[_c->index]->fmuLocation);
		}
		// save previous index of doStep
		fmuInstances[_c->index]->preInTerm = fmuInstances[_c->index]->index;

		// This is needed on Windows for E+.
		// Otherwise, E+ sometimes terminates and breaks the socket connection before
		// the master read the message.
		retVal = readfromsocketFMU(&(fmuInstances[_c->index]->newsockfd), &(fmuInstances[_c->index]->flaRea),
			&(fmuInstances[_c->index]->numOutVar), &zI, &zI, &(fmuInstances[_c->index]->simTimRec), 
			fmuInstances[_c->index]->outVec, NULL, NULL);

		// send end of simulation flag
		fmuInstances[_c->index]->flaWri = 1;
		fmuInstances[_c->index]->flaRea = 1;
		retVal = exchangedoubleswithsocketFMUex (&(fmuInstances[_c->index]->newsockfd), &(fmuInstances[_c->index]->flaWri), 
			&(fmuInstances[_c->index]->flaRea), &(fmuInstances[_c->index]->numOutVar), &(fmuInstances[_c->index]->numInVar), 
			&(fmuInstances[_c->index]->simTimRec), fmuInstances[_c->index]->outVec, &(fmuInstances[_c->index]->simTimSen), 
			fmuInstances[_c->index]->inVec);
		// close socket

		closeipcFMU(&(fmuInstances[_c->index]->sockfd));
		closeipcFMU(&(fmuInstances[_c->index]->newsockfd));
#ifdef _MSC_VER
		// Clean-up winsock
		WSACleanup();
#endif
		if (fmuInstances[_c->index]->firstCallTerm){
			fmuInstances[_c->index]->firstCallTerm = 0;
		}
		// FIXME: free FMU instance does not work with Dymola 2014
		//free (_c);
		return fmiOK;
	}

	return fmiOK;
}

////////////////////////////////////////////////////////////////
///  This method is used to reset the FMU instance
///
///\param c FMU instance.
///\return fmiWarning if error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiResetSlave(fmiComponent c)
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiWarning, "Warning", 
			"fmiResetSlave(..): is not provided!\n");
		return fmiWarning;
	}
	return fmiWarning;
}

////////////////////////////////////////////////////////////////
///  This method is used to free the FMU instance
///
///\param c FMU instance.
////////////////////////////////////////////////////////////////
DllExport void fmiFreeSlaveInstance(fmiComponent c)
{
	idfFmu_t* _c = (idfFmu_t *)c;

	if (fmuInstances[_c->index]->pid != 0)
	{
		int retVal;
#ifndef _MSC_VER
		int status;
#endif
		// if Terminate has already been called, do not do anything here.
		if (fmuInstances[_c->index]->firstCallTerm == 0)
		{
			printf ("fmiTerminateSlave(..) was already called on FMU instance %s!\n", fmuInstances[_c->index]->instanceName);
			return ;
		}

		// if Free has already been called, do not do anything here.
		if (fmuInstances[_c->index]->firstCallFree == 0)
		{
			printf ("fmiFreeSlaveInstance(..) was already called on FMU instance %s!\n", fmuInstances[_c->index]->instanceName);
			return ;
		}

		if (fmuInstances[_c->index]->firstCallFree || (fmuInstances[_c->index]->index!=fmuInstances[_c->index]->preInFree)){
			// change the directory to make sure that FMUs are not overwritten
			retVal = chdir(fmuInstances[_c->index]->fmuLocation);
		}
		// save previous index of doStep
		fmuInstances[_c->index]->preInFree = fmuInstances[_c->index]->index;

		// This is needed on Windows for E+.
		// Otherwise, E+ sometimes terminates and breaks the socket connection before
		// the master read the message.
		retVal = readfromsocketFMU(&(fmuInstances[_c->index]->newsockfd), &(fmuInstances[_c->index]->flaRea),
			&(fmuInstances[_c->index]->numOutVar), &zI, &zI, &(fmuInstances[_c->index]->simTimRec), 
			fmuInstances[_c->index]->outVec, NULL, NULL);

		// send end of simulation flag
		fmuInstances[_c->index]->flaWri = 1;
		fmuInstances[_c->index]->flaRea = 1;
		retVal = exchangedoubleswithsocketFMUex (&(fmuInstances[_c->index]->newsockfd), &(fmuInstances[_c->index]->flaWri), 
			&(fmuInstances[_c->index]->flaRea), &(fmuInstances[_c->index]->numOutVar), &(fmuInstances[_c->index]->numInVar), 
			&(fmuInstances[_c->index]->simTimRec), fmuInstances[_c->index]->outVec, &(fmuInstances[_c->index]->simTimSen), 
			fmuInstances[_c->index]->inVec);
		// close socket

		closeipcFMU(&(fmuInstances[_c->index]->sockfd));
		closeipcFMU(&(fmuInstances[_c->index]->newsockfd));
		// clean-up temporary files
		findFileDelete();
#ifdef _MSC_VER
		// wait for object to terminate
		WaitForSingleObject (fmuInstances[_c->index]->handle_EP, INFINITE);
		TerminateProcess(fmuInstances[_c->index]->handle_EP, 1);
#else
		// wait for object to terminate
		waitpid(fmuInstances[_c->index]->pid, &status, WNOHANG );
		kill (fmuInstances[_c->index]->pid, SIGKILL);
#endif

#ifdef _MSC_VER
		// Clean-up winsock
		WSACleanup();
#endif
		if (fmuInstances[_c->index]->firstCallFree){
			fmuInstances[_c->index]->firstCallFree = 0;
		}
		// FIXME: free FMU instance does not work with Dymola 2014
		// free (_c);
	}

}

////////////////////////////////////////////////////////////////
///  This method is used to set the debug logging in the FMU
///
///\param c FMU instance.
///\param loggingOn LoginggOn activate/deactivate.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiSetDebugLogging  (fmiComponent c, fmiBoolean loggingOn)
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		setDebug (fmuInstances[_c->index]->loggingOn );
		return fmiOK;
	}
	return fmiOK;
}

////////////////////////////////////////////////////////////////
///  This method is used to set reals in the FMU instance
///
///\param c FMU instance.
///\param fmiValueReference value reference.
///\param nvr number of variables to set.
///\param value values of variables to set.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiSetReal(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiReal value[])
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		// fmiValueReference to check for input variable
		fmiValueReference vrTemp;
		ScalarVariable** vars;
		int i, k, retVal;

		if (fmuInstances[_c->index]->firstCallSetReal || (fmuInstances[_c->index]->index!=fmuInstances[_c->index]->preInSetReal)){
			// change the directory to make sure that FMUs are not overwritten
			retVal = chdir(fmuInstances[_c->index]->fmuLocation);
		}
		// save previous index of doStep
		fmuInstances[_c->index]->preInSetReal = fmuInstances[_c->index]->index;

		// allocate memory to store the input values
		if (fmuInstances[_c->index]->flaGetWri){
			//count = 0;
			fmuInstances[_c->index]->inVec = (fmiReal*)malloc(fmuInstances[_c->index]->numInVar*sizeof(fmiReal));
			fmuInstances[_c->index]->flaGetWri = 0;
		}
		vars = fmuInstances[_c->index]->md->modelVariables;
		if (!fmuInstances[_c->index]->writeReady){
			for(i=0; i<nvr; i++)
			{
				for (k=0; vars[k]; k++) {
					ScalarVariable* svTemp = vars [k];
					if (getAlias(svTemp)!=enu_noAlias) continue;
					if (getCausality(svTemp) != enu_input) continue; 
					vrTemp = getValueReference(svTemp);
					if (vrTemp == vr[i]){
						fmuInstances[_c->index]->inVec[vr[i]-1] = value[vr[i]-1]; 
						fmuInstances[_c->index]->setCounter++;
					}
				}
			}
			if (fmuInstances[_c->index]->setCounter == fmuInstances[_c->index]->numInVar)
			{
				fmuInstances[_c->index]->writeReady = 1;

			}
		}
		if (fmuInstances[_c->index]->firstCallSetReal){
			fmuInstances[_c->index]->firstCallSetReal = 0;
		}
		return fmiOK;
	}
	return fmiOK;
}

////////////////////////////////////////////////////////////////
///  This method is used to set integers in the FMU instance
///
///\param c FMU instance.
///\param fmiValueReference value reference.
///\param nvr number of variables to set.
///\param value values of variables to set.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiSetInteger(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiInteger value[])
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		if(nvr>0)
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, "Error", 
				"fmiSetInteger(..) was called. The FMU does not contain integer variables to set!\n");
			return fmiError;
		}
		return fmiOK;
	}
	return fmiOK;
}

////////////////////////////////////////////////////////////////
///  This method is used to set booleans in the FMU instance
///
///\param c FMU instance.
///\param fmiValueReference value reference.
///\param nvr number of variables to set.
///\param value values of variables to set.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiSetBoolean(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiBoolean value[])
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		if(nvr>0)
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, "Error", 
				"fmiSetBoolean(..) was called. The FMU does not contain boolean variables to set!\n");
			return fmiError;
		}
		return fmiOK;
	}
	return fmiOK;
}

////////////////////////////////////////////////////////////////
///  This method is used to set strings in the FMU instance
///
///\param c FMU instance.
///\param fmiValueReference value reference.
///\param nvr number of variables to set.
///\param value values of variables to set.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiSetString(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiString value[])
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		if(nvr>0)
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, 
				"Error", "fmiSetString(..) was called. The FMU does not contain string variables to set!\n");
			return fmiError;
		}
		return fmiOK;
	}
	return fmiOK;
}

////////////////////////////////////////////////////////////////
///  This method is used to get reals from the FMU instance
///
///\param c FMU instance.
///\param fmiValueReference value reference.
///\param nvr number of variables to get.
///\param value values of variables to get.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiGetReal(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiReal value[])
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		fmiValueReference vrTemp;
		ScalarVariable** vars;
		int i, k, retVal;

		vars = fmuInstances[_c->index]->md->modelVariables;

		if (fmuInstances[_c->index]->firstCallGetReal || (fmuInstances[_c->index]->index!=fmuInstances[_c->index]->preInGetReal)){
			// change the directory to make sure that FMUs are not overwritten
			retVal = chdir(fmuInstances[_c->index]->fmuLocation);
		}
		// save previous index of doStep
		fmuInstances[_c->index]->preInGetReal = fmuInstances[_c->index]->index;

		if (fmuInstances[_c->index]->firstCallGetReal||((fmuInstances[_c->index]->firstCallGetReal == 0) 
			&& (fmuInstances[_c->index]->flaGetRea)))  {
				// allocate memory to store the input values
				fmuInstances[_c->index]->outVec = (fmiReal*)malloc(fmuInstances[_c->index]->numOutVar*sizeof(fmiReal));
				// read the values from the server
				retVal = readfromsocketFMU(&(fmuInstances[_c->index]->newsockfd), &(fmuInstances[_c->index]->flaRea),
					&(fmuInstances[_c->index]->numOutVar), &zI, &zI, &(fmuInstances[_c->index]->simTimRec), 
					fmuInstances[_c->index]->outVec, NULL, NULL);
				// reset flaGetRea
				fmuInstances[_c->index]->flaGetRea = 0;
		}
		if (!fmuInstances[_c->index]->readReady)
		{
			for(i=0; i<nvr; i++)
			{
				for (k=0; vars[k]; k++) {
					ScalarVariable* svTemp = vars [k];
					if (getAlias(svTemp)!=enu_noAlias) continue;
					if (getCausality(svTemp) != enu_output) continue; 
					vrTemp = getValueReference(svTemp);
					if (vrTemp == vr[i]){
						value[vr[i]-10001] = fmuInstances[_c->index]->outVec[vr[i]-10001];
						fmuInstances[_c->index]->getCounter++;
					}
				}
			}
			if (fmuInstances[_c->index]->getCounter == fmuInstances[_c->index]->numOutVar)
			{
				fmuInstances[_c->index]->readReady = 1;
			}
		} 

		if(fmuInstances[_c->index]->firstCallGetReal)
		{
			fmuInstances[_c->index]->firstCallGetReal = 0;
		}
		return fmiOK;
	}

	return fmiOK;
}

////////////////////////////////////////////////////////////////
///  This method is used to get integers from the FMU instance
///
///\param c FMU instance.
///\param fmiValueReference value reference.
///\param nvr number of variables to get.
///\param value values of variables to get.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiGetInteger(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiInteger value[])
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		if(nvr>0)
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, "Error", 
				"fmiGetInteger(..) was called. The FMU does not contain integer variables to get!\n");
			return fmiError;
		}
		return fmiOK;
	}
	return fmiOK;
}

////////////////////////////////////////////////////////////////
///  This method is used to get booleans from the FMU instance
///
///\param c FMU instance.
///\param fmiValueReference value reference.
///\param nvr number of variables to get.
///\param value values of variables to get.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiGetBoolean(fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiBoolean value[])
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		if(nvr>0)
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, "Error", 
				"fmiGetBoolean(..) was called. The FMU does not contain boolean variables to get!\n");
			return fmiError;
		}
		return fmiOK;
	}
	return fmiOK;
}

////////////////////////////////////////////////////////////////
///  This method is used to get strings from the FMU instance
///
///\param c FMU instance.
///\param fmiValueReference value reference.
///\param nvr number of variables to get.
///\param value values of variables to get.
///\return fmiOK if no error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiGetString (fmiComponent c, const fmiValueReference vr[], size_t nvr, fmiString  value[])
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		if(nvr>0)
		{
			fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiError, 
				"Error", "fmiGetString(..) was called. The FMU does not contain string variables to get!\n");
			return fmiError;
		}
		return fmiOK;
	}
	return fmiOK;
}

////////////////////////////////////////////////////////////////
///  This method is used to get real output
///  derivatives from the FMU instance
///
///\param c FMU instance.
///\param fmiValueReference value reference.
///\param nvr number of variables to get.
///\param order order of the derivatives.
///\param value values of variables to get.
///\return fmiWarning if error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiGetRealOutputDerivatives(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiInteger order[], fmiReal value[])
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiWarning, "Warning", 
			"fmiGetRealOutputDerivatives(): Real Output Derivatives are not provided!\n");
		return fmiWarning;
	}
	return fmiWarning;
}

////////////////////////////////////////////////////////////////
///  This method is used to set real input
///  derivatives from the FMU instance
///
///\param c FMU instance.
///\param fmiValueReference value reference.
///\param nvr number of variables to set.
///\param order order of the derivatives.
///\param value values of variables to set.
///\return fmiWarning if error occurred.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiSetRealInputDerivatives(fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiInteger order[], const fmiReal value[])
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiWarning, "Warning", 
			"fmiSetRealInputDerivatives(): Real Input Derivatives are not provided!\n");
		return fmiWarning;
	}
	return fmiWarning;
}

////////////////////////////////////////////////////////////////
///  This method is used to get FMU status
///
///\param c FMU instance.
///\param fmiStatusKind status information.
///\param value status value.
///\return fmiDiscard.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiGetStatus(fmiComponent c, const fmiStatusKind s, fmiStatus* value)
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiWarning, "Warning", 
			"fmiGetStatus(): is not provided!\n");
		return fmiWarning;
	}
	return fmiWarning;
}

////////////////////////////////////////////////////////////////
///  This method is used to get fmiGetReal status
///
///\param c FMU instance.
///\param fmiStatusKind status information.
///\param value status value.
///\return fmiDiscard.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiGetRealStatus(fmiComponent c, const fmiStatusKind s, fmiReal* value)
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiWarning, "Warning", 
			"fmiGetRealStatus(): is not provided!\n");
		return fmiWarning;
	}
	return fmiWarning;
}

////////////////////////////////////////////////////////////////
///  This method is used to get fmiGetInteger status
///
///\param c FMU instance.
///\param fmiStatusKind status information.
///\param value status value.
///\return fmiDiscard.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiGetIntegerStatus(fmiComponent c, const fmiStatusKind s, fmiInteger* value)
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)

	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiWarning, "Warning", 
			"fmiGetIntegerStatus(): is not provided!\n");
		return fmiWarning;
	}
	return fmiWarning;
}
////////////////////////////////////////////////////////////////
///  This method is used to get fmiGetBoolean status
///
///\param c FMU instance.
///\param fmiStatusKind status information.
///\param value status value.
///\return fmiDiscard.
////////////////////////////////////////////////////////////////
DllExport fmiStatus fmiGetBooleanStatus(fmiComponent c, const fmiStatusKind s, fmiBoolean* value)
{
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiWarning, "Warning", 
			"fmiGetBooleanStatus(): is not provided!\n");
		return fmiWarning;
	}
	return fmiDiscard;
}
////////////////////////////////////////////////////////////////
///  This method is used to get fmiGetString status
///
///\param c FMU instance.
///\param fmiStatusKind status information.
///\param value status value.
///\return fmiDiscard.
////////////////////////////////////////////////////////////////

DllExport fmiStatus fmiGetStringStatus (fmiComponent c, const fmiStatusKind s, fmiString* value)
{	
	idfFmu_t* _c = (idfFmu_t *)c;
	if (fmuInstances[_c->index]->pid != 0)
	{
		fmuLogger(0, fmuInstances[_c->index]->instanceName, fmiWarning, "Warning", 
			"fmiGetStringStatus(): is not provided!\n");
		return fmiWarning;
	}
	return fmiWarning;
}


/*

***********************************************************************************
Copyright Notice
----------------

Functional Mock-up Unit Export of EnergyPlus �2013, The Regents of 
the University of California, through Lawrence Berkeley National 
Laboratory (subject to receipt of any required approvals from 
the U.S. Department of Energy). All rights reserved.

If you have questions about your rights to use or distribute this software, 
please contact Berkeley Lab's Technology Transfer Department at 
TTD@lbl.gov.referring to "Functional Mock-up Unit Export 
of EnergyPlus (LBNL Ref 2013-088)".

NOTICE: This software was produced by The Regents of the 
University of California under Contract No. DE-AC02-05CH11231 
with the Department of Energy.
For 5 years from November 1, 2012, the Government is granted for itself
and others acting on its behalf a nonexclusive, paid-up, irrevocable 
worldwide license in this data to reproduce, prepare derivative works,
and perform publicly and display publicly, by or on behalf of the Government.
There is provision for the possible extension of the term of this license. 
Subsequent to that period or any extension granted, the Government is granted
for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable 
worldwide license in this data to reproduce, prepare derivative works, 
distribute copies to the public, perform publicly and display publicly, 
and to permit others to do so. The specific term of the license can be identified 
by inquiry made to Lawrence Berkeley National Laboratory or DOE. Neither 
the United States nor the United States Department of Energy, nor any of their employees, 
makes any warranty, express or implied, or assumes any legal liability or responsibility
for the accuracy, completeness, or usefulness of any data, apparatus, product, 
or process disclosed, or represents that its use would not infringe privately owned rights.


Copyright (c) 2013, The Regents of the University of California, Department
of Energy contract-operators of the Lawrence Berkeley National Laboratory.
All rights reserved.

1. Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

(1) Redistributions of source code must retain the copyright notice, this list 
of conditions and the following disclaimer.

(2) Redistributions in binary form must reproduce the copyright notice, this list
of conditions and the following disclaimer in the documentation and/or other 
materials provided with the distribution.

(3) Neither the name of the University of California, Lawrence Berkeley 
National Laboratory, U.S. Dept. of Energy nor the names of its contributors 
may be used to endorse or promote products derived from this software without 
specific prior written permission.

2. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
POSSIBILITY OF SUCH DAMAGE.

3. You are under no obligation whatsoever to provide any bug fixes, patches, 
or upgrades to the features, functionality or performance of the source code
("Enhancements") to anyone; however, if you choose to make your Enhancements
available either publicly, or directly to Lawrence Berkeley National Laboratory, 
without imposing a separate written license agreement for such Enhancements, 
then you hereby grant the following license: a non-exclusive, royalty-free 
perpetual license to install, use, modify, prepare derivative works, incorporate
into other computer software, distribute, and sublicense such enhancements or 
derivative works thereof, in binary and source code form.

NOTE: This license corresponds to the "revised BSD" or "3-clause BSD" 
License and includes the following modification: Paragraph 3. has been added.


***********************************************************************************
*/