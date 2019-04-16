// COMP1521 18s2 mysh ... command history
// Implements an abstract data object

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "history.h"

// This is defined in string.h
// BUT ONLY if you use -std=gnu99
//extern char *strdup(const char *s);

// Command History
// array of command lines
// each is associated with a sequence number

#define MAXHIST 20
#define MAXSTR  200

#define HISTFILE ".mymysh_history"

typedef struct _history_entry {
   int   seqNumber;
   char *commandLine;
} HistoryEntry;

typedef struct _history_list {
   int nEntries;
   HistoryEntry commands[MAXHIST];
} HistoryList;

HistoryList CommandHistory;

// initCommandHistory()
// - initialise the data structure
// - read from .history if it exists

int initCommandHistory()
{
    // TODO
    int i = 0;
    // open the history file
    FILE *file = fopen(HISTFILE, "r");

    if (file != NULL) {
        char buffer[MAXSTR];
        int seqNum;
        // goes through each line of the file and stores it into buffer
        while (fgets(buffer, sizeof(buffer), file) != NULL) {
            // null terminate each buffer
            buffer[strlen(buffer)-1] = '\0';
            char *s = malloc(MAXSTR);
            // scans in sequence number and string from buffer
            sscanf(buffer, " %3d  %s\n", &seqNum, s);

            // updates how many entries in history
            CommandHistory.nEntries++;
            // initialising the sequence number of each command
            CommandHistory.commands[i].seqNumber = seqNum;
            // copying the string into each command in history
            CommandHistory.commands[i].commandLine = strdup(s);
            free(s);
            i++;
        }
        seqNum++;
        fclose(file);
        return seqNum;
    } else {
        // otherwise if file is empty
        for (int i = 0; i < MAXHIST; i++) {
            CommandHistory.commands[i].commandLine = NULL;
        }
    }

    return 1;
}

// addToCommandHistory()
// - add a command line to the history list
// - overwrite oldest entry if buffer is full

void addToCommandHistory(char *cmdLine, int seqNo)
{
   // TODO
   // if the sequence number is exceeding to show the last 20
   if (seqNo-1 >= MAXHIST) {
       // shift the history down by 1
       for (int i = 0; i < MAXHIST; i++) {
            CommandHistory.commands[i].seqNumber = 
                CommandHistory.commands[i+1].seqNumber;
            CommandHistory.commands[i].commandLine = 
                CommandHistory.commands[i+1].commandLine;
       }
       // adds latest entry
       CommandHistory.commands[MAXHIST-1].seqNumber = seqNo;
       CommandHistory.commands[MAXHIST-1].commandLine = strdup(cmdLine);
   } else {
       // otherwise sequence number keeps increasing while adding last entry
       CommandHistory.commands[seqNo-1].seqNumber = seqNo;
       CommandHistory.commands[seqNo-1].commandLine = strdup(cmdLine);
       CommandHistory.nEntries++;   
   }

}

// showCommandHistory()
// - display the list of entries

void showCommandHistory(FILE *outf)
{
   // TODO
    for (int i = 0; i < CommandHistory.nEntries; i++) {
        printf (" %3d  %s\n", CommandHistory.commands[i].seqNumber, 
        CommandHistory.commands[i].commandLine);
    }

}

// getCommandFromHistory()
// - get the command line for specified command
// - returns NULL if no command with this number

char *getCommandFromHistory(int cmdNo)
{
   // TODO
    for (int i = 0; i < CommandHistory.nEntries; i++) {
        if (cmdNo == CommandHistory.commands[i].seqNumber) {
            return CommandHistory.commands[i].commandLine;
        }
    }

   return NULL;
}

// saveCommandHistory()
// - write history to $HOME/.mymysh_history

void saveCommandHistory()
{
   // TODO
    FILE *f = fopen(HISTFILE, "w");

    if (f != NULL) {
        for (int i = 0; i < CommandHistory.nEntries; i++) {
            fprintf (f, " %3d  %s\n", CommandHistory.commands[i].seqNumber,
            CommandHistory.commands[i].commandLine);
        }
        fclose(f);
    }
}

// cleanCommandHistory
// - release all data allocated to command history

void cleanCommandHistory()
{
   // TODO
    for (int i = 0; i < CommandHistory.nEntries; i++) {
        CommandHistory.commands[i].seqNumber = 0;
        free(CommandHistory.commands[i].commandLine);
        CommandHistory.commands[i].commandLine = NULL;
    }
}

