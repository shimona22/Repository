#include "gpiolib_addr.h"
#include "gpiolib_reg.h"
#include "gpiolib_reg.c"

#include <stdint.h>
#include <stdio.h>			//for the printf() function
#include <fcntl.h>
#include <linux/watchdog.h> 		//needed for the watchdog specific constants
#include <unistd.h> 			//needed for sleep
#include <sys/ioctl.h> 			//needed for the ioctl function
#include <stdlib.h> 			//for atoi
#include <time.h> 			//for time_t and the time() function
#include <sys/time.h>          		//for gettimeofday()
#include <errno.h>
#include <string.h>

#define PRINT_MSG(file, time, programName, str, int) \
	do{ \
			fprintf(file, "%s : %s : %s : %d\n", time, programName, str, int); \
			fflush(file); \
	}while(0)
	
#define PRINT_MSG_2(file, time, programName, str) \
do{ \
		fprintf(file, "%s : %s : %s", time, programName, str); \
		fflush(file); \
}while(0)

//This function will find the time
void getTime(char* buffer)
{
	//Create a timeval struct named tv
  	struct timeval tv;

	//Create a time_t variable named curtime
  	time_t curtime;

	//Get the current time and store it in the tv struct
  	gettimeofday(&tv, NULL); 

	//Set curtime to be equal to the number of seconds in tv
  	curtime=tv.tv_sec;

	//This will set buffer to be equal to a string that is
	//equivalent to the current date, in a month, day, year and
	//the current time in 24 hour notation.
  	strftime(buffer, 40,"%m-%d-%Y  %T.",localtime(&curtime));
}

//HARDWARE DEPENDENT CODE BELOW

//This function will initial the GPIO pins
GPIO_Handle initializeGPIO(FILE* logFile, char time[], char programName[])
{
	GPIO_Handle gpio;
	gpio = gpiolib_init_gpio();
	if(gpio == NULL)
	{
		getTime(time);
		PRINT_MSG_2(logFile, time, programName, "Could not initialize GPIO");
	}
	return gpio;	
}

//This function should accept the diode number (1 or 2) and output
//a 0 if the laser beam is not reaching the diode, a 1 if the laser
//beam is reaching the diode or -1 if an error occurs.

#define LASER1_PIN_NUM 4 	//This will replace LASER1_PIN_NUM with 4 when compiled
#define LASER2_PIN_NUM 18	//This will replace LASER2_PIN_NUM with 18 when compiled

int laserDiodeStatus(GPIO_Handle gpio, int diodeNumber)
{
	if(gpio == NULL)
	{
		return -1;
	}

	if(diodeNumber == 1)
	{
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));

		if(level_reg & (1 << LASER1_PIN_NUM))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	
	else if(diodeNumber == 2)
	{
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));

		if(level_reg & (1 << LASER2_PIN_NUM))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	
	else
	{
		return -1;
	}
	
}

//END OF HARDWARE DEPENDENT CODE

//////////////////////////////////////////////

//Compare function to compare buffer array with certain expected strings

int compare(char buffer[])
{
	int i = 0;
	char wd[18] = {"WATCHDOG_TIMEOUT "};
	char lf[9] = {"LOGFILE "};
	char sf[11] = {"STATSFILE "};

	int size = 0;

	while(buffer[i] != '=')
	{
		size++;
		i++;
	}

	int j = 0;
	if (buffer[j] == 'W')
	{
		for (int i = 0; i< size ; i++)
		{
			if (buffer[i] != wd[i])	
			{
				return 0;
			}

		}
		
		return 1;
	}
	
	else if(buffer[j] == 'L')
	{
		for (int i = 0; i< size; i++)
		{
			if (buffer[i] != lf[i])
			{
				return 0;
			}
		}
	
		return 2;
	}

	else if (buffer[j] == 'S')
	{
		for (int i = 0; i< size; i++)
		{
			if (buffer[i] != sf[i])
			{
				return 0;
			}
		}
		
		return 3;
	}	
	
	else
	{
		return 0;
	}
}

//State machine for reading the config file

void readConfig(FILE* configFile, int* timeout, char* logFileName, char* StatsFileName)
{
	enum STATE {START, HASH, NOTHASH, WATCHDOG, LOGFILE, STATSFILE, DONE, ERROR, INPUTLOG, INPUTSTAT, INPUTWD};
	int i = 0; 
	char buffer[255];
	*timeout = 0;
	int j = 0;
	int k = 0;
	int ans = 0;

	enum STATE s = START;

	char *end_of_file = fgets(buffer, 255, configFile); //char pointer to the current line ; fgets will keep assigning some value until it reaches NULL

	while(end_of_file != NULL)
	{
		switch (s)
		{
			case START:
				if (buffer[i] == '#')
				{
					end_of_file = fgets(buffer, 255, configFile);
					i = 0;
					s = HASH;
				}
			
				else
				{
					s = NOTHASH;
				}
				break;

			case HASH:
				if (buffer[i] == '#')
				{
					end_of_file = fgets(buffer, 255, configFile);
					i = 0;
					s = HASH;
				}

				else if (buffer[i] == '\n')
				{
					end_of_file = fgets(buffer, 255, configFile);
					i = 0;
					s = HASH;
				}
				
				else
				{
					s = NOTHASH;
				}

				break;

			case NOTHASH:
				ans = compare(buffer); 

				if (ans == 1)
				{
					i++;
					s = WATCHDOG;
				}

				else if(ans == 2)
				{
					i++;
					s = LOGFILE;
				}

				else if (ans == 3)
				{
					i++;
					s= STATSFILE;
				}

				else if (ans == 0)
				{
					
					s = ERROR;
				}

				break;

			case WATCHDOG:
				if (buffer[i] == '=') 
				{
					i++;
					s = INPUTWD;
				}

				else if (buffer[i] == 0) 
				{
					*timeout = 15; //default timeout
					s = DONE;
				}

				else
				{
					i++;    //keep looping till you find the = or NULL
					s = WATCHDOG;
				}
				
				break;

			case LOGFILE:
				if (buffer[i] == '=')
				{
					i++;
					s = INPUTLOG;
				}

				else if (buffer[i] == 0) //in case reaches NULL and = still not found
				{
					char log[100] = {"/home/pi/LaserFinal.log"};
					int m = 0;
					while (log[m] != '\0')
					{
						logFileName[m] = log[m];
						m++;
					}

					logFileName[m] = '\0';
					s = DONE;
				}

				else 
				{
					i++;             //keep looping till  you find the = or NULL
					s = LOGFILE;
				}
				break;

			case STATSFILE:
				if (buffer[i] == '=')
				{
					i++;
					s = INPUTSTAT;
				}

				else if (buffer[i] == 0)   //in case reaches NULL and = still not found
				{
					char stats [100]= {"/home/pi/LaserFinal.stats"};
					int k = 0;
					while(stats[k] != 0)
					{
						StatsFileName[k] = stats[k];
						k++;
					}

					StatsFileName[k] = '\0';

					 s= DONE;
				}

				else 
				{
					i++;
					s = STATSFILE;     //keep looping till  you find the = or NULL
				}
				break;

			case INPUTLOG:
				if (buffer[i] != ' ' && buffer[i] != '=' && buffer[i] != '\n' && buffer[i] != 0) //add all data into the list
				{
					logFileName[j] = buffer[i];
					i++;
					j++;
					
					s = INPUTLOG;
				}

				else if (buffer[i] == ' ') //ignore whitespace
				{
					i++;
					s = INPUTLOG;
				}

				else if (buffer[i] == '\n') 
				{
					logFileName[j] = 0; //add null terminator at end
					s = DONE;
				}

				else 
				{
					s = ERROR;
				}
				
				break;

			case INPUTSTAT:
				if (buffer[i] != ' ' && buffer[i] != '=' && buffer[i] != '\n' && buffer[i] != 0)  //insert data into location pointer
				{
					StatsFileName[k] = buffer[i];
					i++;
					k++;
					s = INPUTSTAT;
				}
				else if (buffer[i] == ' ') //ignore whitespace
				{ 
					i++;
					s = INPUTSTAT;
				}

				else if(buffer[i] == '\n') 
				{
					StatsFileName[k] = 0; //add null terminator at end
					s = DONE;
				}

				else 
				{
					s = ERROR;
				}
				
				break;

			case INPUTWD:
				if (buffer[i] == ' ')
				{
					i++;
					s = INPUTWD;
				}

				else if(buffer[i] >= '0' && buffer[i] <= '9')
				{

					*timeout = (*timeout *10) + (buffer[i] - '0');
					i++;
					s = INPUTWD;
				}

				else if (buffer[i] == '\n') 
				{
					end_of_file = fgets(buffer, 255, configFile);
					s = HASH;
					i = 0;

					if (*timeout < 1 || *timeout > 15)
					{
						*timeout = 10;
					}
				}

				else 
				{
					s = ERROR;
				}
				
				break;

			case DONE:
			end_of_file = fgets(buffer, 255, configFile);
			i = 0;
			s = HASH;
			break;

			case ERROR:
			end_of_file = NULL;
			break;

			default:
			break;
		}

	}
}

/////////////////////////////////////////////////

//This function will output the number of times each laser was broken
//and it will output how many objects have moved into and out of the room.

//laser1Count will be how many times laser 1 is broken (the left laser).
//laser2Count will be how many times laser 2 is broken (the right laser).
//numberIn will be the number  of objects that moved into the room.
//numberOut will be the number of objects that moved out of the room.
void outputMessage(int laser1Count, int laser2Count, int numberIn, int numberOut, char time[], FILE* statsFile, char programName[])
{
	PRINT_MSG(statsFile, time, programName, "The number of times Laser 1 was broken is ", laser1Count);
	PRINT_MSG(statsFile, time, programName, "The number of times Laser 2 was broken is ", laser2Count);
	PRINT_MSG(statsFile, time, programName, "The number of objects that have entered the room is ", numberIn);
	PRINT_MSG(statsFile, time, programName, "The number of objects that have exited the room is ", numberOut);
	PRINT_MSG_2(statsFile, time, programName, "The program is still running\n");
}

//This function maintains a state machine to maintain the count of the number of times the lasers are broken 
//and the number of objects that entered / exited the room. 
enum State { START1, BOTH_NOT_BROKEN, LASER1_BROKEN_FIRST, LASER2_BROKEN_FIRST, ONLY_LASER1_BROKEN, ONLY_LASER2_BROKEN, BOTH_BROKEN, LASER1_NOT_BROKEN, LASER2_NOT_BROKEN };

int laser(int pin_state_1, int pin_state_2, GPIO_Handle gpio, int *laser1Count, int *laser2Count, int *numberIn, int *numberOut, FILE* logFile, char time1[], char programName[], int watchdog, FILE* statsFile)
{	
	pin_state_1 = laserDiodeStatus(gpio, 1);	//left diode
	pin_state_2 = laserDiodeStatus(gpio, 2);	//right diode
	
	//printf("pin_state_1: %d\n", pin_state_1);
	//printf("pin_state_2: %d\n", pin_state_2);
	
	*laser1Count = 0;
	*laser2Count = 0;
	
	*numberIn = 0;
	*numberOut = 0;
	
	int left = 0;
	int right = 0;
	
	enum State s = START1;
	
	//Create a variable that represents the initial start time
	//time_t is a special variable type used by the time library for calendar times
	//the time() function will return the current calendar time. You will need to put
	//NULL as the argument for the time() function
	
	time_t startTime = time(NULL);
	time_t startTime_2 = time(NULL);
	time_t timeLimit_watchdog = 15; //must be less than or equal to 15s
	time_t timeLimit_output = 60;

	while(1)
	{ 
		ioctl(watchdog, WDIOC_KEEPALIVE, 0);
		
		//In the if condition, we check the current time minus the start time and
		//see if it is less than the number of seconds set to kick the watchdog
		if((time(NULL) - startTime) >= timeLimit_watchdog)
		{
			ioctl(watchdog, WDIOC_KEEPALIVE, 0);
			getTime(time1);
			
			//Log that the Watchdog was kicked
			PRINT_MSG_2(logFile, time1, programName, "The Watchdog was kicked\n");
			startTime = time(NULL);
			usleep(100);
		}
		
		//In the if condition, we check the current time minus the start time and
		//see if it is less than the number of seconds set to output the messages to the stats file
		if((time(NULL) - startTime_2) >= timeLimit_output)
		{
			outputMessage(*laser1Count, *laser2Count, *numberIn, *numberOut, time1, statsFile, programName);
			startTime_2 = time(NULL);
		}
			
		switch(s)
		{ 
			case START1:
				PRINT_MSG_2(logFile, time1, programName, "Start State\n");
				PRINT_MSG(logFile, time1, programName, "pin_state_1:", pin_state_1);
				PRINT_MSG(logFile, time1, programName, "pin_state_2:", pin_state_2);
				
				if(pin_state_1 == 1 && pin_state_2 == 1)
				{
					s = BOTH_NOT_BROKEN;	
				}
				
				else if(pin_state_1 == 1 && pin_state_2 == 0)
				{
					s = LASER1_NOT_BROKEN;
				}
				
				else if(pin_state_2 == 1 && pin_state_1 == 0)
				{
					s = LASER2_NOT_BROKEN;
				}
				
				else
				{
					s = BOTH_BROKEN;
				}
				
				break;

			case BOTH_NOT_BROKEN:
				if(pin_state_1 == 0 && pin_state_2 == 1)
				{
					++*laser1Count;
					++left;
					PRINT_MSG(logFile, time1, programName, "laser1Count: ", *laser1Count);
					s = LASER1_BROKEN_FIRST;
				}
				
				else if(pin_state_1 == 1 && pin_state_2 == 0)
				{
					++*laser2Count;
					++right;
					PRINT_MSG(logFile, time1, programName, "laser2Count: ", *laser2Count);
					s = LASER2_BROKEN_FIRST;
				}
				
				else if(pin_state_1 == 0 && pin_state_2 == 0)
				{
					s = BOTH_BROKEN;
				}
				
				else
				{
					s = BOTH_NOT_BROKEN;
				}
				
				break;
				
			case LASER1_BROKEN_FIRST:
				if(pin_state_1 == 0 && pin_state_2 == 0)
				{
					++*laser2Count;
					++right;
					PRINT_MSG(logFile, time1, programName, "laser2Count: ", *laser2Count);
					s = BOTH_BROKEN;
				}
				
				else if(pin_state_1 == 1 && pin_state_2 == 1)
				{
					s = BOTH_NOT_BROKEN;
				}
				
				else
				{
					s = LASER1_BROKEN_FIRST;
				}
	
				break;
				
			case LASER2_BROKEN_FIRST:
				if(pin_state_1 == 0 && pin_state_2 == 0)
				{
					++*laser1Count;
					++left;
					PRINT_MSG(logFile, time1, programName, "laser1Count: ", *laser1Count);
					s = BOTH_BROKEN;
				}
				
				else if(pin_state_1 == 1 && pin_state_2 == 1)
				{
					s = BOTH_NOT_BROKEN;
				}
				
				else
				{
					s = LASER2_BROKEN_FIRST;
				}
	
				break;
				
			case ONLY_LASER2_BROKEN:
				if(pin_state_1 == 1 && pin_state_2 == 1)
				{
					++*numberIn;
					PRINT_MSG(logFile, time1, programName, "numberIn: ", *numberIn);
					s = START1;
				}
				
				else if(pin_state_1 == 1 && pin_state_2 == 0)
				{
					s = ONLY_LASER2_BROKEN;
				}
				
				else if(pin_state_1 == 0 && pin_state_2 == 0)
				{
					++*laser1Count;
					PRINT_MSG(logFile, time1, programName, "laser1Count: ", *laser1Count);
					s = BOTH_BROKEN;
				}
				
				else
				{
					s = BOTH_NOT_BROKEN;
				}
				
				break;
				
			case ONLY_LASER1_BROKEN:
				if(pin_state_1 == 1 && pin_state_2 == 1)
				{
					++*numberOut;
					PRINT_MSG(logFile, time1, programName, "numberOut: ", *numberOut);
					s = START1;
				}
				
				else if(pin_state_1 == 0 && pin_state_2 == 1)
				{
					s = ONLY_LASER1_BROKEN;
				}
				
				else if(pin_state_1 == 0 && pin_state_2 == 0)
				{
					++*laser2Count;
					PRINT_MSG(logFile, time1, programName, "laser2Count: ", *laser2Count);
					s = BOTH_BROKEN;
				}
				
				else
				{
					s = BOTH_NOT_BROKEN;
				}
				
				break;
				
			case LASER1_NOT_BROKEN:
				if(pin_state_1 == 0 && pin_state_2 == 0)
				{
					++*laser1Count;
					PRINT_MSG(logFile, time1, programName, "laser1Count: ", *laser1Count);
					s = BOTH_BROKEN;
				}
				
				else if(pin_state_1 == 1 && pin_state_2 == 1)
				{
					s = BOTH_NOT_BROKEN;
					
				}
				
				else
				{
					s = LASER1_NOT_BROKEN;
				}
	
				break;
			
			case LASER2_NOT_BROKEN:
				if(pin_state_1 == 0 && pin_state_2 == 0)
				{
					++*laser2Count;
					PRINT_MSG(logFile, time1, programName, "laser2Count: ", *laser2Count);
					s = BOTH_BROKEN;
				}
				
				else if(pin_state_1 == 1 && pin_state_2 == 1)
				{
					s = BOTH_NOT_BROKEN;
					
				}
				
				else
				{
					s = LASER2_NOT_BROKEN;
				}
	
				break;
				
			case BOTH_BROKEN:
				if(left == right && pin_state_1 == 0 && pin_state_2 == 1)
				{
					s = ONLY_LASER1_BROKEN;
				}
				
				else if(left == right && pin_state_1 == 1 && pin_state_2 == 0)
				{
					s = ONLY_LASER2_BROKEN;
				}
				
				else if(pin_state_1 == 0 && pin_state_2 == 1)
				{
					s = LASER2_NOT_BROKEN;
				}
				
				else if(pin_state_1 == 1 && pin_state_2 == 0)
				{
					s = LASER1_NOT_BROKEN;
				}
				
				else if(pin_state_1 == 1 && pin_state_2 == 1)
				{
					s = BOTH_NOT_BROKEN;
				}
				
				else 
				{
					s = BOTH_BROKEN;
				}
	
				break;
				
			default:
				return -1;
				break;
		}
		
		// Process actions on entering the state
		switch (s) 
		{ 
			case START1:
				pin_state_1 = laserDiodeStatus(gpio, 1);	
				pin_state_2 = laserDiodeStatus(gpio, 2);
				break;
			
			case BOTH_NOT_BROKEN:
				pin_state_1 = laserDiodeStatus(gpio, 1);
				pin_state_2 = laserDiodeStatus(gpio, 2);	
				break;
			
			case BOTH_BROKEN:
				pin_state_1 = laserDiodeStatus(gpio, 1);	
				pin_state_2 = laserDiodeStatus(gpio, 2);
				break;
				
			case LASER1_BROKEN_FIRST:
				pin_state_1 = laserDiodeStatus(gpio, 1);	
				pin_state_2 = laserDiodeStatus(gpio, 2);	
				break;
				
			case LASER2_BROKEN_FIRST:
				pin_state_1 = laserDiodeStatus(gpio, 1);	
				pin_state_2 = laserDiodeStatus(gpio, 2);	
				break;
				
			case LASER1_NOT_BROKEN:
				pin_state_1 = laserDiodeStatus(gpio, 1);	
				pin_state_2 = laserDiodeStatus(gpio, 2);	
				break;
				
			case LASER2_NOT_BROKEN:
				pin_state_1 = laserDiodeStatus(gpio, 1);	
				pin_state_2 = laserDiodeStatus(gpio, 2);	
				break;
				
			case ONLY_LASER1_BROKEN:
				pin_state_1 = laserDiodeStatus(gpio, 1);	
				pin_state_2 = laserDiodeStatus(gpio, 2);	
				break;
				
			case ONLY_LASER2_BROKEN:
				pin_state_1 = laserDiodeStatus(gpio, 1);	
				pin_state_2 = laserDiodeStatus(gpio, 2);	
				break;
			  
			default:
				return -1;
				break;
		} 
	} 
}


int main(const int argc, const char* const argv[])
{
	//Create a string that contains the program name
	const char* argName = argv[0];

	//These variables will be used to count how long the name of the program is
	int i = 0;
	int namelength = 0;

	while(argName[i] != 0)
	{
		namelength++;
		i++;
	} 

	char programName[namelength];

	i = 0;

	//Copy the name of the program without the ./ at the start of argv[0]
	while(i < namelength)
	{
		programName[i] = argName[i + 2];
		i++;
	} 	
	
	/////////////////////////////////////////////////
	
	//Create a char array that will be used to hold the time values
	char time[30];
	getTime(time);
	
	//Declare the variables that will be passed to the readConfig function
	int timeout;
	char logFileName[50];
	char statsFileName[50];
	
	//Create a file pointer named configFile
	FILE* configFile;
	//Set configFile to point to the Embedded.cfg file. It is set to read the file.
	configFile = fopen("/home/pi/Embedded.cfg", "r");

	if(!configFile)
	{
		perror("The config file could not be opened"); 	//Output a warning message if the config file cannot be opened
		
		timeout = 15;
		perror("The watchdog has been set to 15 s"); 
		
		char log[100] = {"/home/pi/LaserFinal.log"}; //Set default log file 
		int i = 0;
		while(log[i] != 0)
		{
			logFileName[i] = log[i];
			i++;
		}

		logFileName[i] = '\0';
		perror("The default log file has been opened");
		
		char stats [100]= {"/home/pi/LaserFinal.stats"}; //Set default stats fault 

		int j = 0;
		while(stats[j] != 0)
		{
			statsFileName[j] = stats[j];
			j++;
		}

		statsFileName[j] = '\0';
		perror("The default stats file has been opened");
	}
	
	else
	{
		//Call the readConfig function to read from the config file
		readConfig(configFile, &timeout, logFileName, statsFileName);
		
		//Close the configFile now that we have finished reading from it
		fclose(configFile);
	}
	
	//Create a new file pointer to point to the log file
	FILE* logFile;
	//Set it to point to the file from the config file and make it append to
	//the file when it writes to it.
	logFile = fopen(logFileName, "a");

	//Check that the log file opens properly.
	if(!logFile)
	{
		fprintf(stderr, "Could not open the log file from the config file\n");
		
		char log[100] = {"/home/pi/LaserFinal.log"};
		int i = 0;
		while(log[i] != 0)
		{
			logFileName[i] = log[i];
			i++;
		}

		logFileName[i] = '\0';

		logFile = fopen(logFileName, "a");
		if (!logFile)
		  fprintf(stderr, "Could not open the default log file\n "); 
		
		else
			fprintf(stderr, "The default log file has been opened\n");
	}
	
	else
	{
		PRINT_MSG_2(logFile, time, programName, "The log file has been opened\n");
	}
	
	//Create a new file pointer to point to the stats file
	FILE* statsFile;
	//Set it to point to the file from the config file and make it append to
	//the file when it writes to it.

	statsFile = fopen(statsFileName, "a");

	//Check that the file opens properly.
	if(!statsFile)
	{
		fprintf(stderr, "Could not open the stats file from the config file\n");
		
		char stats [100]= {"/home/pi/LaserFinal.stats"};

		int j = 0;
		while(stats[j] != 0)
		{
			statsFileName[j] = stats[j];
			j++;
		}

		statsFileName[j] = '\0';
		
		statsFile = fopen(statsFileName, "a");
		if (!statsFile)
		  fprintf(stderr, "Could not open the default stats file\n "); 
		
		else
			fprintf(stderr, "The default stats file has been opened\n");
	}
	
	else
	{
		PRINT_MSG_2(logFile, time, programName, "The stats file has been opened\n");
	}
	
	///////////////////////////////////////////////////////////
	
	//Initialize the GPIO pins
	GPIO_Handle gpio = initializeGPIO(logFile, time, programName);
	//Log that the GPIO pins have been initialized
	PRINT_MSG_2(logFile, time, programName, "The GPIO pins have been initialized\n");
	
	//Get the current time
	getTime(time);
	

	int pin_state_1 = laserDiodeStatus(gpio, 1);
	int pin_state_2 = laserDiodeStatus(gpio, 2);	
	
	/////////////////////////////////////////
	
	//This variable will be used to access the /dev/watchdog file, similar to how
	//the GPIO_Handle works
	int watchdog;

	//We use the open function here to open the /dev/watchdog file. If it does
	//not open, then we output an error message. We do not use fopen() because we
	//do not want to create a file if it doesn't exist
	if ((watchdog = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0) 
	{
		printf("Error: Couldn't open watchdog device!\n");
		return -1;
	} 
	
	//Get the current time
	getTime(time);
	//Log that the watchdog file has been opened
	PRINT_MSG_2(logFile, time, programName, "The Watchdog file has been opened\n\n");
	
	//This line uses the ioctl function to set the time limit of the watchdog timer to 15s. 
	//The time limit can not be set higher that 15 seconds
	//If we try to set it to any value greater than 15, then it will reject that
	//value and continue to use the default time limit
	ioctl(watchdog, WDIOC_SETTIMEOUT, &timeout);

	//The value of timeout will be changed to whatever the current time limit of the
	//watchdog timer is
	ioctl(watchdog, WDIOC_GETTIMEOUT, &timeout);
	
	//Get the current time
	getTime(time);
	//Log that the Watchdog time limit has been set
	PRINT_MSG_2(logFile, time, programName, "The Watchdog time limit has been set\n\n");

	//This print statement will confirm to us if the time limit has been properly
	//changed.
	PRINT_MSG(logFile, time, programName, "The watchdog timeout is ", timeout);
	
	///////////////////////////////////////////
	
	int laser1Count = 0;
	int laser2Count = 0;
	int numberIn = 0;
	int numberOut = 0;

	laser(pin_state_1, pin_state_2, gpio, &laser1Count, &laser2Count, &numberIn, &numberOut, logFile, time, programName, watchdog, statsFile);
	
	
	PRINT_MSG_2(logFile, time, programName, "The program shouldn't have reached here!\n\n");
	
	//Writing a V to the watchdog file will disable to watchdog and prevent it from
	//resetting the system
	write(watchdog, "V", 1);
	getTime(time);
	//Log that the Watchdog was disabled
	PRINT_MSG_2(logFile, time, programName, "The Watchdog was disabled\n\n");

	//Close the watchdog file so that it is not accidentally tampered with
	close(watchdog);
	getTime(time);
	//Log that the Watchdog was closed
	PRINT_MSG_2(logFile, time, programName, "The Watchdog was closed\n\n");

	//Free the gpio pins
	gpiolib_free_gpio(gpio);
	getTime(time);
	//Log that the GPIO pins were freed
	PRINT_MSG_2(logFile, time, programName, "The GPIO pins have been freed\n\n");

	//Return to end the program
	return 0;	
}


