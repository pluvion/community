/**
 * Pluvi.On is a library for the ESP8266/Arduino/Pluvi.On platform
 * 
 *       FILE: PluviOn.cpp
 *    VERSION: 1.0.0
 *    LICENSE: Creative Commons 4
 *    AUTHORS:
 *             Hugo Santos <hugo@pluvion.com.br>
 *             Pedro Godoy <pedro@pluvion.com.br>
 * 
 *       SITE: https://www.pluvion.com.br
 */
#ifndef PluviOn_h
#define PluviOn_h

extern "C" {
    #include "user_interface.h"
}

#if PLV_DEBUG_ENABLED
#define PLV_DEBUG_(text) { Serial.print( (text) ); }
#else
#define PLV_DEBUG_(text) {}
#endif

#if PLV_DEBUG_ENABLED
#define PLV_DEBUG_HEADER(text) {  Serial.println(F("\n")); Serial.println((text)); Serial.println(F("===========================================")); }
#else
#define PLV_DEBUG_HEADER(text) {}
#endif

#if PLV_DEBUG_ENABLED
#define PLV_DEBUG(text) { Serial.println( (text) ); }
#else
#define PLV_DEBUG(text) {}
#endif

// Serial debug
#if PLV_DEBUG_ENABLED
#define PLV_DEBUG_SETUP(baudrate) { Serial.begin( (baudrate) ); }
#else
#define PLV_DEBUG_SETUP(baudrate) {}
#endif

class PluviOn
{
    public:
        PluviOn();


        /**
         * Create a file in File System
         * 
         * @param directory Directory path
         * @param filename File Name
         * @return true if file was created successfully, otherwise false.
         */
        boolean       FSCreateFile(String directory, String filename);

        /**
         * Create a file in File System
         * 
         * @param directory Directory path
         * @param filename File Name
         * @return true if file was created successfully, otherwise false.
         */
        boolean       FSCreateFile(String directory, int filename);

        /**
         * Create a file in File System
         * 
         * @param directory Directory path
         * @param filename File Name
         * @return true if file was created successfully, otherwise false.
         */
        boolean       FSCreateFile(String directory, unsigned long filename);

        /**
         * Write content to a file
         * 
         * @param directory Directory path
         * @param filename File Name
         * @param content File Content
         * @return true if content was created successfully, otherwise false.
         */
        boolean       FSWriteToFile(String directory, int filename, String content);

        /**
         * Deletes all files from the given directory.
         * 
         * @param directory Directory absolute path
         *
         * @return true if all files was deleted successfully, otherwise false.
         */
        boolean       FSDeleteFiles(String directory);

        /**
         * Delete file.
         * 
         * @param directory Directory absolute path
         * @param filename Filename
         *
         * @return true if file was deleted successfully, otherwise false.
         */
        boolean       FSDeleteFile(String directory, int filename);

        /**
         * Read int value from File System
         *
         * @param filepath Directory filepath
         * @return The number readed, or 0 if it fails
         */
        unsigned int  FSReadInt(String filepath);

        /**
         * Read float value from File System
         *
         * @param filepath Directory filepath
         * @return The number readed, or 0 if it fails
         */
        float         FSReadFloat(String filepath);

        /**
         * Read unsigned long value from File System
         *
         * @param filepath Directory filepath
         * @return The number readed, or 0 if it fails
         */
        unsigned long FSReadULong(String filepath);

        /**
         * Read String value from File System
         *
         * @param filepath Directory filepath
         * @return The String readed, or "" (blank) if it fails
         */
        String        FSReadString(String filepath);

        /**
         * Print the system's file list
         */
        void           FSPrintFileList();

        /**
         * Format file system
         */
        void           FSFormat();        

        /**
         * Convert bytes in to KB and MB.
         *
         * @param bytes float number to be converted
         * @param prefix the unit to be converted.
         * @return the number formatted acording to the specified unit
         */
        float          bytesConverter(float bytes, char prefix);
};

#endif
