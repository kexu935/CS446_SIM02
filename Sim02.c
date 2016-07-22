// Program Header Information ////////////////////////////////////////
/**
* @file Sim02.c
*
* @brief program for SIM02
* 
* @details Simulation of several programs OS simulator
*
* @version 2.00
*          C.S. Student (1 April 2016)
*          Initial development of SIM02
*
* @note None
*/
// Program Description/Support /////////////////////////////////////
/*
 This phase will require the creation of a batch program OS simulator named
Sim02, using four states (Enter/Start, Ready, Running, Exit). It will accept
the meta-data for several programs (i.e., potentially unlimited number), run
the programs one at a time, and then end the simulation.
*/
// Precompiler Directives //////////////////////////////////////////
//
/////  NONE
//
// Header Files ///////////////////////////////////////////////////
//
   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>
   #include <unistd.h>
   #include <time.h>
   #include <stdbool.h>
   #include <pthread.h>
//
// Global Constant Definitions ////////////////////////////////////
//
   #define BILLION   1E9 
//
// Class Definitions //////////////////////////////////////////////
//
   struct meta
      {
       // struct that records meta data
       int proc_num;
       char component;
       char operation[10];
       int cyc_time;
       struct meta *next;
      };

   struct pcb_table
      {
       // struct that records pcb information read from config file
       int processNum;
       int processorCycleTime;
       int monitorCycleTime;
       int hardDriveCycleTime;
       int printerCycleTime;
       int keyboardCycleTime;
       int totalTime;
       int logMode;
       char scheduling[10];
       char dataFile[15];
       char outputFile[15];
       struct meta* metaHead;
       struct pcb_table* next;
      };

   struct logLine
      {
       // struct that records log to print and write to file
       double time;
       char comment[40];
       struct logLine *next;
      };

   struct thread_arg
      {
       // struct that pass as a parameter to thread
       struct meta metaData;
       struct pcb_table pcb;
      };
//
// Free Function Prototypes ///////////////////////////////////////
//
   void readConfig( char* fileName, struct pcb_table* pcb );
   void dataInput( char* fileName, struct meta** metaList, 
                  struct pcb_table** pcbList, struct pcb_table myPcb );
   void sjfSort( struct pcb_table** pcbList );
   void* thread_perform( void* argument );
   int calcTime( struct meta metaData, struct pcb_table pcb );
   double timeLap( struct timespec startTime, struct timespec endTime );
   void delay( clock_t wait );
   void recordLog( struct logLine** list, struct logLine** currentLog, 
                  double time, char memo[40] );
   void printLog( struct logLine* currentLog );
   bool checkMeta( struct meta* currentMeta, char comment[40] );
   bool checkLogEnd( struct meta* currentMeta, char comment[40] );
   void outputToFile( struct logLine* list, struct pcb_table pcb );
   void readString( char* des, char* line );
   void readNum( int* des, char* line );
   void freeMetaList( struct meta* head );
   void freeLogList( struct logLine* head );
   void freePcbList( struct pcb_table* head);
   void swapPcb( struct pcb_table** pcbA, struct pcb_table** pcbB );
//
// Main Function Implementation ///////////////////////////////////
//
   int main( int argc, char* argv[] )
      {
       struct meta *metaList = NULL;   // meta linked list
       struct pcb_table myPCB = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };   // pcb table
       struct pcb_table *pcbList = NULL;   // pcb linked list
       struct logLine *logList = NULL;   // log linked list
       struct logLine *currentLog = logList;   // pointer to current log
       struct thread_arg myThreadArg = { 0, 0 };   // thread parameter
       struct timespec startTime, endTime;   // timer
       bool isThread = false;   // check if the meta a thread
       bool logNeedEnd = false;   // check if the log needs a end log
       bool toMonitor = false;   // check if the log needs to print on screen
       bool toFile = false;   // check if the log needs to output to file
       double totalTime;   // time range
       char logComment[40];   // comment inside log
       int currentProcess = 0;   // current running process
       pthread_t myThread;   // thread

       // read config file
       readConfig( argv[1], &myPCB );

       // check if need to print to monitor or file
       if(myPCB.logMode == 1 || myPCB.logMode == 2 )
          toMonitor = true;
       if(myPCB.logMode == 1 || myPCB.logMode == 3 )
          toFile = true;

       // read meta data file
       dataInput( myPCB.dataFile, &metaList, &pcbList, myPCB);

       // if the mode if SJF, sort all the processes
       if( strcmp( myPCB.scheduling, "SJF" ) == 0)
           sjfSort(&pcbList);

       // start timer
       clock_gettime( CLOCK_REALTIME, &startTime );

       // loop through the pcb linked list
       for( ; pcbList != NULL; pcbList = pcbList -> next )
          {
             for( metaList = pcbList -> metaHead, currentProcess  = metaList -> proc_num; 
                  metaList!= NULL && metaList -> proc_num == currentProcess; 
                  metaList = metaList -> next)
                {
                 // lap timer
                 clock_gettime( CLOCK_REALTIME, &endTime );
                 totalTime = timeLap( startTime, endTime );

                 // check if the meta is a thread
                 if( isThread = checkMeta( metaList, logComment ) )
                    {
                     // if it is, create a thread
                     myThreadArg.metaData = *metaList;
                     myThreadArg.pcb = myPCB;
                     pthread_create( &myThread, NULL, thread_perform, (void*) &myThreadArg );
                     pthread_join( myThread, NULL );
                    }

                 // record this log and print on monitor if needed
                 recordLog( &logList, &currentLog, totalTime, logComment );
                 if( toMonitor )
                    printLog( currentLog );

                 // lap timer
                 clock_gettime( CLOCK_REALTIME, &endTime );
                 totalTime = timeLap( startTime, endTime );

                 // check if the log needs a end log
                 if( logNeedEnd = checkLogEnd( metaList, logComment ) )
                    {
                     // if it needs, record and print on monitor if needed
                     recordLog( &logList, &currentLog, totalTime, logComment );
                     if( toMonitor )
                        printLog( currentLog );
                    }
                }
          }   // end of loop

       // output the logs to file if needed
       if( toFile )
          outputToFile( logList, myPCB );

       // deallocation all the linked lists
       freeMetaList( metaList );
       freeLogList( logList );
       freePcbList( pcbList );

       return 0;
      }   // end of main

//
// Free Function Implementation ///////////////////////////////////
/**
* @brief Function reads config file 
*
* @details Function reads config file and record to pcb table
*
* @pre char* fileName contains the name of config file
*
* @pre struct* pcb contains the pcb table
*
* @post if the config file doesn't exist, end the program
*
* @post all pcb information recorded in pcb table
*
* @return None
*
*/
   void readConfig( char* fileName, struct pcb_table* pcb )
      {
       FILE* filePtr;   // file pointer
       char line[40];   // string holds each line of file
       char temp[15];   // temp string

       // open file and read
       filePtr = fopen( fileName, "r" );

       // if the file doesn't exist
       if( filePtr == NULL )
          {
           printf( "CONFIGURATION FILE NOT FOUND!\n" );
           exit( 1 );
          }

       // otherwise
       else
          {
           // loop to each line
           while( fgets( line, sizeof(line), filePtr ) )
              {
               // ingore these lines
               if( strncmp( line, "Start Simulator Configuration File", 15 ) == 0
                || strncmp( line, "Version/Phase: ", 10 ) == 0
                || strncmp( line, "End Simulator Configuration File", 15 ) == 0 )
                  continue;

               // read and record meta file name
               if( strncmp( line, "File Path: ", 10 ) == 0 )
                  readString( pcb -> dataFile, line );

               // read and record scheduling mode
               if( strncmp( line, "CPU Scheduling Code: ", 10 ) == 0 )
                  {
                   readString( pcb-> scheduling, line);

                   // check if the scheduling mode supported
                   if( strcmp( pcb-> scheduling, "FCFS") == 0 
                    || strcmp( pcb-> scheduling, "SJF") == 0 )
                      continue;
                   else
                      {
                       printf("Scheduling Mode Does Not Support!\n");
                       exit( 1 );
                      }
                  }

               // read and record processor cycle time
               if( strncmp( line, "Processor cycle time (msec): ", 10 ) == 0 )
                  readNum( &( pcb -> processorCycleTime ), line );

               // read and record monitor cycle time
               if( strncmp( line, "Monitor display time (msec): ", 10 ) == 0 )
                  readNum( &( pcb -> monitorCycleTime ), line );

               // read and record hard drive cycle time
               if( strncmp( line, "Hard drive cycle time (msec): ", 10 ) == 0 )
                  readNum( &( pcb -> hardDriveCycleTime ), line );

               // read and record printer cycle time
               if( strncmp( line, "Printer cycle time (msec): ", 10 ) == 0 )
                  readNum( &( pcb -> printerCycleTime ), line );

               // read and record keyboard cycle time
               if( strncmp( line, "Keyboard cycle time (msec): ", 10 ) == 0 )
                  readNum( &( pcb -> keyboardCycleTime ), line );

               // read and record log mode
               if( strncmp( line, "Log: ", 4 ) == 0 )
                  {
                   readString( temp, line );

                   if( strncmp( temp, "Log to Both", 11 ) == 0 )
                      pcb -> logMode = 1;
                   if( strncmp( temp, "Log to Monitor", 14 ) == 0 )
                      pcb -> logMode = 2;
                   if( strncmp( temp, "Log to File", 11 ) == 0 )
                      pcb -> logMode = 3;
                  }

               // read and record output file name
               if( strncmp( line, "Log File Path: ", 10 ) == 0 )
                   readString( pcb -> outputFile, line );
              }   // end of loop
          }

      // close file
      fclose(filePtr);
      }   // end of func

/**
* @brief Function reads string in a line
*
* @details Function reads string in a line and copy
*
* @pre char* des contains the string to store
*
* @pre char* line contains the line
*
* @post the certain part of string is read and saved
*
* @return None
*
*/
    void readString( char* des, char* line )
       {
        int charIndex;   // index of each character
        int stringIndex = 0;   // index of string

        // loop through the stirng 
        for( charIndex = 0; line[charIndex] != '\n'; charIndex ++ )
           {
            if( line[charIndex] == ':' )
               {
                do
                   {
                    charIndex ++;
                   }
                while( line[charIndex] == ' ' );
                while( line[charIndex] != '\n' )
                   {
                    des[stringIndex] = line[charIndex];
                    charIndex ++;
                    stringIndex ++;
                   }
                des[stringIndex] = '\0';
               }
           }   // end of loop
       }   // end of func

/**
* @brief Function reads a number in a line
*
* @details Function reads a number in a line and copy
*
* @pre int* des contains the number to store
*
* @pre char* line contains the line
*
* @post the certain part of number is read and saved
*
* @return None
*
*/
    void readNum( int* des, char* line )
       {
        int charIndex;   // index of each character

        // loop through the string
        for( charIndex = 0; line[charIndex] != '\n'; charIndex ++ )
           {
            if( line[charIndex] == ':' )
               {
                do
                   {
                    charIndex ++;
                   }
                while( line[charIndex] == ' ' );
                while( line[charIndex] != ' ' && line[charIndex] != '\n' )
                   {
                    *des = *des * 10 + line[charIndex] - '0';
                    charIndex ++;
                   }
               }
           }   // end of loop
       }   // end of func

/**
* @brief Function reads meta data file 
*
* @details Function reads meta data and record
*
* @pre char* fileName contains the name of data file
*
* @pre struct** metaList contains the meta linked list
*
* @pre struct** pcbList contains the pcb linked list
*
* @pre struct myPcb contains the pcb table
*
* @post if the data file doesn't exist, end the program
*
* @post all meta data information recorded
*
* @return None
*
*/
   void dataInput( char* fileName, struct meta** metaList , struct pcb_table** pcbList, 
                   struct pcb_table myPcb)
      {
       FILE* filePtr;   // file pointer
       char line[100];   // string holds each line of file
       int charIndex;   // index to string
       int processNum = 0;   // the rank of process
       struct meta tempMeta;   // temp meta
       struct meta* metaListEnd = *metaList;   // meta pointer to end of linked list
       struct pcb_table* pcbListEnd = *pcbList;   // pcb pointer to end of linked list

       // open file
       filePtr = fopen( fileName, "r" );

       // if the file doesn't exist
       if( filePtr == NULL )
          {
           printf( "META DATA FILE NOT FOUND!\n" );
           exit( 1 );
          }

       // otherwise
       else
          {
           // loop to get each line
           while( fgets( line, sizeof( line ), filePtr ) )
              {
               // ignore these lines
               if( strncmp( line, "Start Program Meta-Data Code:", 15 ) == 0
                || strncmp( line, "End Program Meta-Data Code.", 15 ) == 0 )
                  continue;

               // loop to record each meta data
               for( charIndex = 0; charIndex < sizeof( line ); charIndex ++ )
                  {
                   while( line[charIndex] == ' ' )
                      charIndex ++;
                   if( line[charIndex] == '\n' )
                      break;

                   // read component
                   tempMeta.component = line[charIndex]; 

                   charIndex ++;
                   if( line[charIndex] == '(' ) 
                      charIndex ++;
                      int k;

                   // read operation
                   for( k=0; line[charIndex] != ')' ; charIndex ++, k ++ )
                      tempMeta.operation[k] = line[charIndex];
                   tempMeta.operation[k] = '\0';
                   charIndex ++;

                   // read cycle time
                   tempMeta.cyc_time = 0;
                   for( ; line[charIndex] != ';' && line[charIndex] != '.'; charIndex ++ )
                      tempMeta.cyc_time = tempMeta.cyc_time * 10 + line[charIndex] - '0';


                   // decide the rank the process
                   if( tempMeta.component == 'A' && strcmp( tempMeta.operation, "start" ) == 0 )
                      processNum ++;
                   if( tempMeta.component != 'S' )
                      tempMeta.proc_num = processNum;
                   else
                      tempMeta.proc_num = 0;

                   // set up meta and copy data above
                   struct meta* metaPtr = ( struct meta* ) malloc ( sizeof( struct meta ) );
                   metaPtr -> component = tempMeta.component;
                   strcpy( metaPtr -> operation, tempMeta.operation );
                   metaPtr -> cyc_time = tempMeta.cyc_time;
                   metaPtr -> proc_num = tempMeta.proc_num;
                   metaPtr -> next = NULL;
                   if( *metaList == NULL )
                      *metaList = metaPtr;
                   else
                      metaListEnd -> next = metaPtr;
                   metaListEnd = metaPtr;

                   // if the meta represent a begin of process, set up a pcb for it
                   if( tempMeta.component == 'S' || strcmp( tempMeta.operation, "start" ) == 0 )
                      {
                       struct pcb_table* pcbPtr = ( struct pcb_table* ) malloc 
                                                                      ( sizeof ( struct pcb_table) );
                       pcbPtr -> processNum = tempMeta.proc_num;
                       strcpy( pcbPtr -> dataFile, myPcb.dataFile);
                       strcpy( pcbPtr -> scheduling, myPcb.scheduling);
                       pcbPtr -> processorCycleTime = myPcb.processorCycleTime;
                       pcbPtr -> monitorCycleTime = myPcb.monitorCycleTime;
                       pcbPtr -> hardDriveCycleTime = myPcb.hardDriveCycleTime;
                       pcbPtr -> printerCycleTime = myPcb.printerCycleTime;
                       pcbPtr -> keyboardCycleTime = myPcb.keyboardCycleTime;
                       pcbPtr -> totalTime = myPcb.totalTime;
                       pcbPtr -> logMode = myPcb.logMode;
                       strcpy( pcbPtr -> outputFile, myPcb.outputFile );
                       pcbPtr -> next = NULL;
                       pcbPtr -> metaHead = metaPtr;
                       if( *pcbList == NULL )
                          *pcbList = pcbPtr;
                       else
                          pcbListEnd -> next = pcbPtr;
                       pcbListEnd = pcbPtr;
                      }

                   // update total execution time in pcb
                   pcbListEnd -> totalTime += calcTime( tempMeta, myPcb );
                  }   // end of loop
              }
          }   // end of loop

       // close file
       fclose(filePtr);
      }   // end of func

/**
* @brief Function sorts pcb tables by SJF
*
* @details Function sorts pcb tables according to total time using bubble sort
*
* @pre struct** pcbList contains the pcb linked list
*
* @post pcbs are sorted in increment order
*
* @return None
*
*/
    void sjfSort( struct pcb_table** pcbList )
       {
        struct pcb_table* ptr1;   // pointer to pcb
        struct pcb_table* ptr2;   // pointer to pcb

        // loop through pcb tables
        for( ptr1 = (*pcbList) -> next; ptr1 != NULL && ptr1 -> processNum != 0; ptr1 = ptr1 -> next )
           for( ptr2 = (*pcbList) -> next; ptr2 -> next -> processNum != 0; ptr2 = ptr2 -> next )
              {
               // if needs swap
               if((ptr2 -> totalTime) > (ptr2 -> next -> totalTime))
                  swapPcb(&ptr2, &(ptr2->next));
              }   // end of loop
       }   // end of func

/**
* @brief Function swaps two pcb table
*
* @details Function swaps time, procNum and pointer to meta of two pcbs
*
* @pre struct** pcbA contains one pcb
*
* @pre struct** pcbB contains the other pcb
*
* @post these two pcbs swapped
*
* @return None
*
*/
    void swapPcb(struct pcb_table** pcbA, struct pcb_table** pcbB)
       {
        int temp;   // temp int
        struct meta* tempMeta;   // temp meta

        temp = (*pcbA) -> processNum;
        (*pcbA) -> processNum = (*pcbB) -> processNum;
        (*pcbB) -> processNum = temp;

        temp = (*pcbA) -> totalTime;
        (*pcbA) -> totalTime = (*pcbB) -> totalTime;
        (*pcbB) -> totalTime = temp;

        tempMeta = (*pcbA) -> metaHead;
        (*pcbA) -> metaHead = (*pcbB) -> metaHead;
        (*pcbB) -> metaHead = tempMeta;
       }   // end of func

/**
* @brief Function creates a thread
*
* @details Function creates a thread according its cycle time
*
* @pre void* argument contains argument
*
* @post thread run for its time
*
* @return None
*
*/
   void* thread_perform( void* argument )
      {
       struct thread_arg temp;   // temp argument
       int time = 0;   // init. time

       temp = *( ( struct thread_arg * ) argument);

       // calculate running time
       time = calcTime( temp.metaData, temp.pcb );

       // delay that long time
       delay((clock_t)time);
      }   // end of func

/**
* @brief Function checks if the meta is to create a thread
*
* @details Function checks if the meta is to create a thread
*          and write the corresponding log comment
*
* @pre struct* currentMeta contains the meta data
*
* @pre char comment contains the comment of its log
*
* @post the comment that needs to print and output
*
* @return ture if the meta is to create a thread
*
* @return false if the meta is not to create a thread
*
*/
   bool checkMeta( struct meta* currentMeta, char comment[40] )
      {
       char temp[40] = { 0 };   // temp string

       // decide the comment by the component of meta
       if( currentMeta -> component == 'S' )
          {
           if( strcmp( currentMeta -> operation, "start" ) == 0 )
              strcpy( temp, "Simulator program starting" );
           if( strcmp( currentMeta -> operation, "end" ) == 0 )
              strcpy( temp, "Simulator program ending" );
          }

       if( currentMeta -> component == 'A' )
          {
           if( strcmp( currentMeta -> operation, "start" ) == 0)
              strcpy( temp, "OS: selecting next process" );
           if( strcmp( currentMeta -> operation, "end" ) == 0 )
              sprintf( temp, "OS: removing process %d", currentMeta -> proc_num );
          }

       if( currentMeta -> component == 'P' || currentMeta -> component == 'I' 
           || currentMeta -> component == 'O' )
          {
           if( currentMeta -> component == 'P' )
              sprintf( temp, "Process %d: start processing action", currentMeta -> proc_num );
           else
              {
               sprintf( temp, "Process %d: start ", currentMeta -> proc_num );
               strcat( temp, currentMeta -> operation );
               if( currentMeta -> component == 'I' )
                  strcat( temp, " input" );
               if( currentMeta -> component == 'O' )
                  strcat( temp, " ouput" );
              }
          }

       // copy the comment
       strcpy( comment, temp );

       // if the metaData is to create a thread, return true
       // otherwise, return false
       if( currentMeta -> component == 'P' || currentMeta -> component == 'I' 
           || currentMeta -> component == 'O' )
          return true;
       else
          return false;
      }   // end of func

/**
* @brief Function checks if the log needs log end
*
* @details Function checks if the log needs log end
*
* @pre char comment contains the comment of the log
*
* @post if the log doesn't need an end, leave it blank
*
* @post if it needs, gives it a log end
*
* @return true if the log needs a log end
*
*/
    bool checkLogEnd( struct meta* currentMeta, char comment[40] )
       {
        char* stringIndex;   // index to string
        int newLength = 0;   // length of new string

        // for the log doesn't need an end
        if( strcmp( comment, "Simulator program ending" ) == 0 
            || strncmp( comment, "OS: removing process ", 20 ) == 0 )
           return false;

        // make the end for the log needs
        else if( strcmp( comment, "Simulator program starting" ) == 0 )
           strcpy( comment, "OS: preparing all processes" );

        else if( strcmp( comment, "OS: selecting next process" ) == 0 )
           sprintf( comment, "OS: starting process %d", currentMeta -> proc_num );

        else
           {
            stringIndex = strstr( comment, "start" );
            newLength = strlen( comment ) - strlen( "start" ) + strlen( "end" );
            char temp[newLength + 1];
            memcpy( temp, comment, stringIndex - comment );
            memcpy( temp + ( stringIndex - comment ), "end", strlen( "end" ) );
            strcpy( temp + ( stringIndex - comment) + strlen( "end" ), 
                    stringIndex + strlen( "start" ) );
            strcpy( comment, temp );
           }
        return true;
       }   // end of func

/**
* @brief Function delays some time
*
* @details Function delays according to the time
*
* @pre clock_t time contains the time to delay
*
* @post delays for a certain time
*
* @return None
*
*/
   void delay( clock_t wait )
      {
       clock_t goal;   // time to stop

       goal = wait + clock();
       while( goal > clock() );
      }   // end of func

/**
* @brief Function calculates the time
*
* @details Function calculates the time according to cycle time
*
* @pre struct metaData contains meta data
*
* @pre struct pcb contains the pcb table
*
* @post the time that needs to delay created
*
* @return int time that the delay time
*
*/
   int calcTime( struct meta metaData, struct pcb_table pcb )
      {
       int time = 0;   // init. time
       char temp[10] = { 0 };   // string temp
       int numberOfCycle = 0;   // number of cycles
       int cycleTime = 0;  // cycle time

       // get number of cycles
       strcpy( temp, metaData.operation );
       numberOfCycle = metaData.cyc_time;

       // calc the time needs to delay
       if( strcmp( temp, "start" ) == 0 || strcmp( temp, "run" ) == 0 
           || strcmp( temp, "end" ) == 0 )
          cycleTime = pcb.processorCycleTime;
       if( strcmp( temp, "hard drive" ) == 0 )
          cycleTime = pcb.hardDriveCycleTime;
       if( strcmp( temp, "keyboard" ) == 0 )
          cycleTime = pcb.keyboardCycleTime;
       if( strcmp( temp, "monitor" ) == 0 )
          cycleTime = pcb.monitorCycleTime;
       if( strcmp( temp, "printer" ) == 0 )
          cycleTime = pcb.printerCycleTime;

       time = cycleTime * numberOfCycle * 1000;

       return time;
      }   // end of func

/**
* @brief Function gets the time range it elapses
*
* @details Function returns the real time range
*
* @pre struct startTime contains start time point
*
* @pre struct endTime contains end time point
*
* @post time range calculated and returned
*
* @return double the time range
*
*/
    double timeLap( struct timespec startTime, struct timespec endTime )
       {
        return ( endTime.tv_sec - startTime.tv_sec ) + 
               ( endTime.tv_nsec - startTime.tv_nsec ) / BILLION;
       }   // end of func

/**
* @brief Function records log 
*
* @details Function records log's time and comment
*
* @pre struct** list contains the log linked list
*
* @pre struct** currentLog contains the current log
*
* @pre double time contains the lap time
*
* @pre char memo contains commment of log
*
* @post records the log
*
* @return None
*
*/
    void recordLog( struct logLine** list, struct logLine** currentLog, double time, char memo[40] )
       {
        // allocate a new log
        struct logLine* ptr = ( struct logLine* ) malloc ( sizeof( struct logLine ) );

        // copy all data
        if( time != 0 )
           ptr -> time = time;
        if( memo != 0 )
           strcpy( ptr -> comment, memo );
        ptr -> next = NULL;

        // connect the linked list
        if( *list == NULL)
           *list = ptr;
        else
           (*currentLog) -> next = ptr;
        *currentLog = ptr;
       }   // end of func

/**
* @brief Function prints each log 
*
* @details Function prints each log to screen
*
* @pre struct currentLog contains the log
*
* @post the log printed on screen
*
* @return None
*
*/
   void printLog( struct logLine* currentLog )
      {
       printf( "%f - %s\n", currentLog -> time, currentLog -> comment );
      }   // end of func

/**
* @brief Function output to file
*
* @details Function output logs to file
*
* @pre struct currentLog contains the log
*
* @pre int numberOfLog contains the number of logs
*
* @pre struct pcb contains the pcb table
*
* @post output logs to file
*
* @return None
*
*/
    void outputToFile( struct logLine* list, struct pcb_table pcb )
       {
        FILE *filePtr;   // file pointer

        // open file and write
        filePtr = fopen( pcb.outputFile, "w" );

        // output each log
        while( list != NULL )
           {
            fprintf( filePtr, "%f", list -> time );
            fputs( " - ", filePtr );
            fprintf( filePtr, "%s", list -> comment );
            fputs( "\n", filePtr );
            list = list -> next;
           }

        // close file
        fclose( filePtr );
       }   // end of func

/**
* @brief Function free linked list
*
* @details Function deallocate meta linked list
*
* @pre struct* head contains pointer to meta list head
*
* @post linked list deallocated
*
* @return None
*
*/
    void freeMetaList(struct meta* head)
       {
        struct meta* temp;   // temp meta

        // loop through linked list and deallocate each
        while( head != NULL )
           {
            temp = head;
            head = head -> next;
            free(temp);
           }
       }

/**
* @brief Function free linked list
*
* @details Function deallocate log linked list
*
* @pre struct* head contains pointer to log list head
*
* @post linked list deallocated
*
* @return None
*
*/
    void freeLogList(struct logLine* head)
       {
        struct logLine* temp;   // temp log

        // loop through linked list and deallocate each
        while( head != NULL )
           {
            temp = head;
            head = head -> next;
            free(temp);
           }
       }

/**
* @brief Function free linked list
*
* @details Function deallocate pcb linked list
*
* @pre struct* head contains pointer to pcb list head
*
* @post linked list deallocated
*
* @return None
*
*/
    void freePcbList( struct pcb_table* head)
       {
        struct pcb_table* temp;   // temp pcb

        // loop through linked list and deallocate each
        while( head != NULL)
           {
            temp = head;
            head = head -> next;
            free(temp);
           }
       }

