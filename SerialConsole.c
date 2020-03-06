
/**************************************************************************//**
* @file        SerialConsole.c
* @ingroup 	   Serial Console
* @brief       This file has the code necessary to run the CLI and Serial Debugger. It initializes an UART channel and uses it to receive command from the user
*				as well as print debug information.
* @details     This file has the code necessary to run the CLI and Serial Debugger. It initializes an UART channel and uses it to receive command from the user
*				as well as print debug information.
*				
*				The code in this file will:
*				--Initialize a SERCOM port (SERCOM # ) to be an UART channel operating at 115200 baud/second, 8N1
*				--Register callbacks for the device to read and write characters asynchronously as required by the CLI
*				--Initialize the CLI and Debug Logger data structures
*				
*				Usage:
*		
*
* @copyright   
* @author      
* @date        January 26, 2019
* @version		0.1
*****************************************************************************/

/******************************************************************************
* Includes
******************************************************************************/
#include "SerialConsole.h"

/******************************************************************************
* Defines
******************************************************************************/
#define RX_BUFFER_SIZE 512														///<Size of character buffer for RX, in bytes
#define TX_BUFFER_SIZE 512														///<Size of character buffer for TX, in bytes
#define MAX_RX_BUFFER_LENGTH   5

/******************************************************************************
* Structures and Enumerations
******************************************************************************/
cbuf_handle_t cbufRx;															///<Circular buffer handler for receiving characters from the Serial Interface
cbuf_handle_t cbufTx;															///<Circular buffer handler for transmitting characters from the Serial Interface

char latestRx;																	///< Holds the latest character that was received
char latestTx;																	///< Holds the latest character to be transmitted.
volatile uint8_t rx_buffer[MAX_RX_BUFFER_LENGTH];
char tempBuffer[64];															///< Temporary buffer to copy the data from cbufRx.
char setNameBuffer[64];															///< Temporary buffer to copy the data from tempBuffer 
																				///< during setDeviceName function call

/******************************************************************************
*  Callback Declaration
******************************************************************************/
void usart_write_callback(struct usart_module *const usart_module);				//Callback for when we finish writing characters to UART
void usart_read_callback(struct usart_module *const usart_module);				//Callback for when we finish reading characters from UART

/******************************************************************************
* Local Function Declaration
******************************************************************************/
static void configure_usart(void);
static void configure_usart_callbacks(void);


/******************************************************************************
* Global Local Variables
******************************************************************************/
struct usart_module usart_instance;
char rxCharacterBuffer[RX_BUFFER_SIZE];											///<Buffer to store received characters
char txCharacterBuffer[TX_BUFFER_SIZE];											///<Buffer to store characters to be sent
enum eDebugLogLevels currentDebugLevel = LOG_INFO_LVL;							///<Variable that holds the level of debug log messages to show. 
																				//	Defaults to showing all debug values
int enterFlag = 0;																///<Flag to detect press of Enter key
int backspaceFlag = 0;															///<Flag to detect press of Backspace key
int setDeviceFlag = 0;															///<Flag to detect press of Enter key during naming in setDeviceName function
int count = 0;																	///<Counter to keep track of the buffers

/******************************************************************************
* Global Functions
******************************************************************************/


/**************************************************************************//**
* @fn			void InitializeSerialConsole(void)
* @brief		Initializes the UART - sets up the SERCOM to act as UART and registers the callbacks for
*				asynchronous reads and writes.
* @details		Initializes the UART - sets up the SERCOM to act as UART and registers the callbacks for
*				asynchronous reads and writes. 
* @note			Call from main once to initialize Hardware.
*****************************************************************************/

void InitializeSerialConsole(void)
{
	
	//Initialize circular buffers for RX and TX
	cbufRx = circular_buf_init((uint8_t*)rxCharacterBuffer, RX_BUFFER_SIZE);
	cbufTx = circular_buf_init((uint8_t*)txCharacterBuffer, RX_BUFFER_SIZE);

	//Configure USART and Callbacks
	configure_usart();
	configure_usart_callbacks();
	
	usart_read_buffer_job(&usart_instance, (uint8_t*) &latestRx, 1);			//<Kicks off constant reading of characters
		

	//Add any other calls you need to do to initialize your Serial Console
}

/**************************************************************************//**
* @fn			void DeinitializeSerialConsole(void)
* @brief		Deinitlaizes the UART
* @note			
*****************************************************************************/
void DeinitializeSerialConsole(void)
{
	usart_disable(&usart_instance);
}

/**************************************************************************//**
* @fn			void SerialConsoleWriteString(char * string)
* @brief		Writes a string to be written to the uart. Copies the string to a ring buffer that is used to hold the text send to the uart
* @details		Uses the ringbuffer 'cbufTx', which in turn uses the array 'txCharacterBuffer'
* @note			Use to send a string of characters to the user via UART
*****************************************************************************/
void SerialConsoleWriteString(char * string)
{
	if(string != NULL)
	{
		for (size_t iter = 0; iter < strlen(string); iter++)
		{
			circular_buf_put(cbufTx, string[iter]);
		}

		if(usart_get_job_status(&usart_instance, USART_TRANSCEIVER_TX) == STATUS_OK)
		{
			circular_buf_get(cbufTx, (uint8_t*) &latestTx); //Perform only if the SERCOM TX is free (not busy)
			usart_write_buffer_job(&usart_instance, (uint8_t*) &latestTx, 1);
		}
	} 
}

/**************************************************************************//**
* @fn			int SerialConsoleReadCharacter(uint8_t *rxChar)
* @brief		Reads a character from the RX ring buffer and stores it on the pointer given as an argument.
*				Also, returns -1 if there is no characters on the buffer
*				This buffer has values added to it when the UART receives ASCII characters from the terminal
* @details		Uses the ringbuffer 'cbufTx', which in turn uses the array 'txCharacterBuffer'
* @param[in]	Pointer to a character. This function will return the character from the RX buffer into this pointer
* @return		Returns -1 if there are no characters in the buffer
* @note			Use to receive characters from the RX buffer (FIFO)
*****************************************************************************/
int SerialConsoleReadCharacter(uint8_t *rxChar)
{	
	return circular_buf_get(cbufRx, (uint8_t*) rxChar);
}


/*
DEBUG LOGGER FUNCTIONS
*/

/**************************************************************************//**
* @fn			eDebugLogLevels getLogLevel(void)
* @brief		Gets the level of debug to print to the console to the given argument.
*				Debug logs below the given level will not be allowed to be printed on the system
* @return		Returns the current debug level of the system.
* @note
*****************************************************************************/

enum eDebugLogLevels getLogLevel(void)
{
	return currentDebugLevel;
}

/**************************************************************************//**
* @fn			eDebugLogLevels getLogLevel(void)
* @brief		Sets the level of debug to print to the console to the given argument.
*				Debug logs below the given level will not be allowed to be printed on the system
* @param[in]   debugLevel The debug level to be set for the debug logger
* @note
*****************************************************************************/
void setLogLevel(enum eDebugLogLevels debugLevel)
{
	currentDebugLevel = debugLevel;
}


/**************************************************************************//**
* @fn			LogMessage (Students to fill out this)
* @brief
* @note
*****************************************************************************/
void LogMessage(enum eDebugLogLevels level, const char *format, ...)
{
	enum eDebugLogLevels presentLevel ;											//< Create an object of the enum eDebugLogLevels
	presentLevel = getLogLevel() ;
	
	char * LogBuffer[64];
	
	va_list cptr;
	int ret;
	
	va_start(cptr, format);
	ret = vsprintf(LogBuffer, format, cptr);									///< Use vsprintf to pass integers and other arguments as characters
	va_end(cptr);
	
	if(level > presentLevel)
	{
		setLogLevel(level);														///< Update current Log level
		SerialConsoleWriteString(LogBuffer);
	}
	else
	{
		setLogLevel(presentLevel);
	}

};

/*******************************************************************************
FUNCTION TO CALL CLI COMMANDS ALONG WITH DETECTION OF ENTER AND BACKSPACE KEY
********************************************************************************/
void callCLIcommands(void)
{
	
	if (backspaceFlag == 1)															///< Check for press of Backspace key
	{
		backspaceFlag = 0;															///< Reset flag of Backspace key
		SerialConsoleWriteString(" \b");
		tempBuffer[count-2] = tempBuffer[count];									///< Update the temporary buffer
		count = count -2;
	}
	
	else if (enterFlag == 1)														///< Check for press of Enter key
	{
		enterFlag = 0;																///< Reset flag of Enter key
		SerialConsoleWriteString("\r\n");
		CLIcommandList(tempBuffer);													///< Call the command table
		
	}
	
	
}


/**************************************************************************
COMMAND LINE INTERFACE COMMANDS
***************************************************************************/

void CLIcommandList(char* stringCLI)
{																			///< When any of the functions are called, clear the temporary buffer 
																			///  and reset the counter to 0.
	if (strcmp(stringCLI,"help\r") ==0)										///< Function call for help command and definition
	{
		memset(tempBuffer, 0, sizeof(tempBuffer));
		count = 0;
		SerialConsoleWriteString("\r\n");
		SerialConsoleWriteString("Available commands are: \r\n");
		SerialConsoleWriteString("1. ver_bl\r\n");
		SerialConsoleWriteString("2. ver_app\r\n");
		SerialConsoleWriteString("3. mac\r\n");
		SerialConsoleWriteString("4. ip\r\n");
		SerialConsoleWriteString("5. devName\r\n");
		SerialConsoleWriteString("6. setDeviceName\r\n");
		SerialConsoleWriteString("7. getDeviceName \r\n");	
		SerialConsoleWriteString("\r\n");	
	} 
	
	else if (strcmp(stringCLI,"ver_bl\r") ==0)								///< Function call for bootloader version command and definition
	{
		memset(tempBuffer, 0, sizeof(tempBuffer));
		count = 0;
		SerialConsoleWriteString("\r\n");
		SerialConsoleWriteString("1.4.88\r\n");
		SerialConsoleWriteString("\r\n");
	}
	
	else if (strcmp(stringCLI,"ver_app\r") ==0)								///< Function call for firmware version command and definition
	{
		memset(tempBuffer, 0, sizeof(tempBuffer));
		count = 0;
		SerialConsoleWriteString("\r\n");
		SerialConsoleWriteString("2.8.44\r\n");
		SerialConsoleWriteString("\r\n");
	}
	
	else if (strcmp(stringCLI,"mac\r") ==0)									///< Function call for mac ID command and definition
	{
		memset(tempBuffer, 0, sizeof(tempBuffer));
		count = 0;
		SerialConsoleWriteString("\r\n");
		SerialConsoleWriteString("AA.BB.CC.DD.EE.FF\r\n");
		SerialConsoleWriteString("\r\n");
	}
	
	else if (strcmp(stringCLI,"ip\r") ==0)									///< Function call for IP address command and definition
	{
		memset(tempBuffer, 0, sizeof(tempBuffer));
		count = 0;
		SerialConsoleWriteString("\r\n");
		SerialConsoleWriteString("255.255.255.255\r\n");
		SerialConsoleWriteString("\r\n");
	}
	
	else if (strcmp(stringCLI,"devName\r") ==0)								///< Function call to retrieve default Device name and definition
	{
		memset(tempBuffer, 0, sizeof(tempBuffer));
		count = 0;
		SerialConsoleWriteString("\r\n");
		SerialConsoleWriteString("Device Name: Sourajit Das\r\n");
		SerialConsoleWriteString("\r\n");
	}
	
	else if (strcmp(stringCLI,"setDeviceName\r") ==0)						///< Function call to set Device name and definition
	{
		memset(tempBuffer, 0, sizeof(tempBuffer));
		count = 0;
		SerialConsoleWriteString("\r\n");
		SerialConsoleWriteString("Enter Device Name:\r\n");
		
		setDeviceFlag = 0;
		
		while(setDeviceFlag != 1)
		{
			for(uint8_t i = 0; i < 63; i++)
			{
				setNameBuffer[i] = tempBuffer[i]; 
			}
		}

		memset(tempBuffer, 0, sizeof(tempBuffer));
		count = 0;	
		SerialConsoleWriteString("Device name saved\r\n");
	}
	
	else if (strcmp(stringCLI,"getDeviceName\r") ==0)						///< Function call to retrieve custom Device name and definition
	{
		if (setNameBuffer[0] == NULL)										///< Check if a custom name exits or not. 
		{
			memset(tempBuffer, 0, sizeof(tempBuffer));
			count = 0;
			SerialConsoleWriteString("\r\n");
			SerialConsoleWriteString("Device Name is Sourajit Das\r\n");	///< Retrieve default Device name
			SerialConsoleWriteString("\r\n");
		}
		else
		{																	///< Retrieve custom Device name
			memset(tempBuffer, 0, sizeof(tempBuffer));
			count = 0;
			SerialConsoleWriteString("\r\n");
			SerialConsoleWriteString("Device Name is ");
			SerialConsoleWriteString(setNameBuffer);
			SerialConsoleWriteString("\r\n");
			SerialConsoleWriteString("\r\n");
		}
			
	}
	
	else																	///< Throw up error message if keywords don't match with commands in table
	{
		memset(tempBuffer, 0, sizeof(tempBuffer));
		count = 0;
		SerialConsoleWriteString("Error ! Type help for menu\r\n");
		SerialConsoleWriteString("\r\n");					
	}
	
}



/******************************************************************************
* Local Functions
******************************************************************************/

/**************************************************************************//**
* @fn			static void configure_usart(void)
* @brief		Code to configure the SERCOM "EDBG_CDC_MODULE" to be a UART channel running at 115200 8N1
* @note			
*****************************************************************************/
static void configure_usart(void)
{
	struct usart_config config_usart;
	usart_get_config_defaults(&config_usart);

	config_usart.baudrate    = 115200;
	config_usart.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	config_usart.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	config_usart.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	config_usart.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	config_usart.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	while (usart_init(&usart_instance,
					  EDBG_CDC_MODULE, 
					  &config_usart) != STATUS_OK) 
	{

	}
	
	usart_enable(&usart_instance);
}


/**************************************************************************//**
* @fn			static void configure_usart_callbacks(void)
* @brief		Code to register callbacks
* @note
*****************************************************************************/
static void configure_usart_callbacks(void)
{
	usart_register_callback(&usart_instance, usart_write_callback, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_register_callback(&usart_instance, usart_read_callback, USART_CALLBACK_BUFFER_RECEIVED);
	usart_enable_callback(&usart_instance, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_enable_callback(&usart_instance, USART_CALLBACK_BUFFER_RECEIVED);
}






/******************************************************************************
* Callback Functions
******************************************************************************/

/**************************************************************************//**
* @fn			void usart_read_callback(struct usart_module *const usart_module)
* @brief		Callback called when the system finishes receiving all the bytes requested from a UART read job
* @note
*****************************************************************************/
void usart_read_callback(struct usart_module *const usart_module)
{

	//Order Echo
	SerialConsoleWriteString(&latestRx);
	circular_buf_put(cbufRx, (uint8_t) latestRx);									///< Add the latest read character into the RX circular Buffer
	usart_read_buffer_job(&usart_instance, (uint8_t*) &latestRx, 1);				///< Order the MCU to keep reading
	
	tempBuffer[count] = latestRx;													///< Store received characters in the temporary buffer
	count++;
		
	if (latestRx == 13)																///< Raise the flags for Enter when Enter key is pressed
	{
		enterFlag = 1;																///< Raise the flag when Enter key is pressed
		setDeviceFlag = 1;															///< Raise the flag when Enter key is pressed after entering Device name
	}
	
	if (latestRx == 8)																///< Raise the flags for Backspace when Backspace key is pressed
	{
		backspaceFlag = 1;
	}
	
/*******************************************************************************
*  We don't need this section
********************************************************************************/
	/*
	char *rx1;
	SerialConsoleReadCharacter(rx1);

	//Add latest character again, just to try getting it out again
	circular_buf_put(cbufRx, (uint8_t) latestRx); //Add the latest read character into the RX circular Buffer

	//Try method 2  (Not necessary though)
	char rx2;
	SerialConsoleReadCharacter(&rx2);
	
	*/

}


/**************************************************************************//**
* @fn			void usart_write_callback(struct usart_module *const usart_module)
* @brief		Callback called when the system finishes sending all the bytes requested from a UART read job
* @note
*****************************************************************************/
void usart_write_callback(struct usart_module *const usart_module)
{
	if(circular_buf_get(cbufTx, (uint8_t*) &latestTx) != -1)									//Only continue if there are more characters to send
	{
		usart_write_buffer_job(&usart_instance, (uint8_t*) &latestTx, 1);
	}
	
}

