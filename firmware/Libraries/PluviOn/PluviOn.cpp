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
#include <FS.h> // FS must be the first

#define PLV_DEBUG_ENABLED true
#include "PluviOn.h"

PluviOn::PluviOn() {}

/**
 * Create a file in File System
 * 
 * @param directory Directory path
 * @param filename File Name
 * @return true if file was created successfully, otherwise false.
 */
boolean PluviOn::FSCreateFile(String directory, String filename){

    PLV_DEBUG_(F("Creating file: \""));
    PLV_DEBUG_(directory);
    PLV_DEBUG_(F("/"));
    PLV_DEBUG_(filename);
    PLV_DEBUG(F("\""));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    boolean success = true;

    // Mount filepath
    String filepath  = directory;
           filepath += "/";
           filepath += filename;

    // Open/Create file
    File f = SPIFFS.open(filepath, "w");
    if (f) {
        PLV_DEBUG(F("SUCCESS: File created."));
        f.close();
    } else {
        success = false;
        PLV_DEBUG(F("ERROR: Fail creating file."));
    }

    return success;
}

/**
 * Create a file in File System
 * 
 * @param directory Directory path
 * @param filename File Name
 * @return true if file was created successfully, otherwise false.
 */
boolean PluviOn::FSCreateFile(String directory, int filename){

    PLV_DEBUG_(F("Creating file: \""));
    PLV_DEBUG_(directory);
    PLV_DEBUG_(F("/"));
    PLV_DEBUG_(filename);
    PLV_DEBUG(F("\""));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    boolean success = true;

    // Mount filepath
    String filepath  = directory;
           filepath += "/";
           filepath += filename;

    // Open/Create file
    File f = SPIFFS.open(filepath, "w");
    if (f) {
        PLV_DEBUG(F("SUCCESS: File created."));
        f.close();
    } else {
        success = false;
        PLV_DEBUG(F("ERROR: Fail creating file."));
    }

    return success;
}

/**
 * Create a file in File System
 * 
 * @param directory Directory path
 * @param filename File Name
 * @return true if file was created successfully, otherwise false.
 */
boolean PluviOn::FSCreateFile(String directory, unsigned long filename){

    PLV_DEBUG_(F("Creating file: \""));
    PLV_DEBUG_(directory);
    PLV_DEBUG_(F("/"));
    PLV_DEBUG_(filename);
    PLV_DEBUG(F("\""));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    boolean success = true;

    // Mount filepath
    String filepath  = directory;
           filepath += "/";
           filepath += filename;

    // Open/Create file
    File f = SPIFFS.open(filepath, "w");
    if (f) {
        PLV_DEBUG(F("SUCCESS: File created."));
        f.close();
    } else {
        success = false;
        PLV_DEBUG(F("ERROR: Fail creating file."));
    }

    return success;
}

/**
 * Write content to a file
 * 
 * @param directory Directory path
 * @param filename File Name
 * @param content File Content
 * @return true if content was created successfully, otherwise false.
 */
boolean PluviOn::FSWriteToFile(String directory, int filename, String content) {

    PLV_DEBUG_(F("Creating file: \""));
    PLV_DEBUG_(directory);
    PLV_DEBUG_(F("/"));
    PLV_DEBUG_(filename);
    PLV_DEBUG(F("\""));

    PLV_DEBUG_(F("Content to write: \""));
    PLV_DEBUG_(content);
	PLV_DEBUG(F("\""));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    boolean success = true;

    // Mount filepath
    String filepath  = directory;
           filepath += "/";
           filepath += filename;

    // Open/Create file
    File f = SPIFFS.open(filepath, "w");
    if (f) {
        PLV_DEBUG(F("SUCCESS: File created."));

		// Check if the content was saved successfully
		if (f.println(content)) {
			PLV_DEBUG(F("SUCCESS: Content saved to file."));
		} else {
			success = false;
		}

        f.close();
    } else {
        success = false;
        PLV_DEBUG(F("ERROR: Fail creating file."));
    }

    return success;
}

/**
 * Deletes all files from the given directory.
 * 
 * @param directory Directory absolute path
 *
 * @return true if all files was deleted successfully, otherwise false.
 */
boolean PluviOn::FSDeleteFiles(String directory){
    
    PLV_DEBUG_(F("Deleting files from directory: \""));
    PLV_DEBUG_(directory);
    PLV_DEBUG(F("\""));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    boolean success = true;

    // Open directory
    Dir dir = SPIFFS.openDir(directory);

    // Remove all files
    while (dir.next()) {

        PLV_DEBUG_(F("Removing file : "));
        PLV_DEBUG(dir.fileName());
        
        if (!SPIFFS.remove(dir.fileName())) {
            PLV_DEBUG(F("ERROR during file deletion!"));
            success = false;
        }
    }

    return success;
}

/**
 * Delete file.
 * 
 * @param directory Directory absolute path
 * @param filename Filename
 *
 * @return true if file was deleted successfully, otherwise false.
 */
boolean PluviOn::FSDeleteFile(String directory, int filename) {

    PLV_DEBUG_(F("Deleting file: \""));
    PLV_DEBUG_(directory);
    PLV_DEBUG_(F("/"));
    PLV_DEBUG_(filename);
    PLV_DEBUG(F("\""));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    boolean success = true;

    // Mount filepath
    String filepath  = directory;
           filepath += "/";
           filepath += filename;

    PLV_DEBUG_(F("Removing file : "));
	PLV_DEBUG(filepath);

    // Remove file
    if (!SPIFFS.remove(filepath)) {
    	success = false;
        PLV_DEBUG(F("ERROR: Fail deleting file."));
    }

    return success;
}

/**
 * Read int value from File System
 *
 * @param filepath Directory filepath
 * @return The number readed, or 0 if it fails
 */
unsigned int PluviOn::FSReadInt(String filepath){

    PLV_DEBUG_(F("Reading unsigned int from filepath: \""));
    PLV_DEBUG_(filepath);
    PLV_DEBUG(F("\"")); 

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    // Open the longitude directory
    Dir dir = SPIFFS.openDir(filepath);

    if(dir.next()){

    	PLV_DEBUG_(F("File located: \""));
	    PLV_DEBUG_(dir.fileName());
	    PLV_DEBUG(F("\""));

        return dir.fileName().substring(filepath.length() + 1).toInt();
    } else {
        PLV_DEBUG(F("EMPTY DIRECTORY."));
    }

    return 0;
}

/**
 * Read float value from File System
 *
 * @param filepath Directory filepath
 * @return The number readed, or 0 if it fails
 */
float PluviOn::FSReadFloat(String filepath){

    PLV_DEBUG_(F("Reading float from filepath: \""));
    PLV_DEBUG_(filepath);
    PLV_DEBUG(F("\"")); 

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    // Open the longitude directory
    Dir dir = SPIFFS.openDir(filepath);

    if(dir.next()){

    	PLV_DEBUG_(F("File located: \""));
	    PLV_DEBUG_(dir.fileName());
	    PLV_DEBUG(F("\""));

        return dir.fileName().substring(filepath.length() + 1).toFloat();
    } else {
        PLV_DEBUG(F("EMPTY DIRECTORY."));
    }

    return 0;
}

/**
 * Read unsigned long value from File System
 *
 * @param filepath Directory filepath
 * @return The number readed, or 0 if it fails
 */
unsigned long PluviOn::FSReadULong(String filepath){

    PLV_DEBUG_(F("Reading unsigned long from filepath: \""));
    PLV_DEBUG_(filepath);
    PLV_DEBUG(F("\"")); 

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    // Open the longitude directory
    Dir dir = SPIFFS.openDir(filepath);

    if(dir.next()){

    	PLV_DEBUG_(F("File located: \""));
	    PLV_DEBUG_(dir.fileName());
	    PLV_DEBUG(F("\""));

        return strtoul(dir.fileName().substring(filepath.length() + 1).c_str(), NULL, 10);
    } else {
        PLV_DEBUG(F("EMPTY DIRECTORY."));
    }

    return 0;
}

/**
 * Read String value from File System
 *
 * @param filepath Directory filepath
 * @return The String readed, or "" (blank) if it fails
 */
String PluviOn::FSReadString(String filepath){

    PLV_DEBUG_(F("Reading String from filepath: \""));
    PLV_DEBUG_(filepath);
    PLV_DEBUG(F("\"")); 

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
        PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    // Open the longitude directory
    Dir dir = SPIFFS.openDir(filepath);

    if(dir.next()){

    	PLV_DEBUG_(F("File located: \""));
	    PLV_DEBUG_(dir.fileName());
	    PLV_DEBUG(F("\""));

        return dir.fileName().substring(filepath.length() + 1);
    } else {
        PLV_DEBUG(F("EMPTY DIRECTORY."));
    }

    return "";
}

/**
 * Print the system's list of files and directories
 */
void PluviOn::FSPrintFileList() {

    PLV_DEBUG(F("\n\nFILE LIST"));
    PLV_DEBUG(F("==========================================="));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
    	PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    Dir dir = SPIFFS.openDir("/");
    int fc = 0;
    while (dir.next()) {
        ++fc;
        PLV_DEBUG_(fc);
        PLV_DEBUG_(" - ");
        PLV_DEBUG(dir.fileName());
    }
}

/**
 * Format file system
 */
void PluviOn::FSFormat() {

    PLV_DEBUG(F("\n\nFORMATTING FILE SYSTEM"));
    PLV_DEBUG(F("==========================================="));

    // Mounts SPIFFS file system
    if (!SPIFFS.begin()) {
    	PLV_DEBUG(F("FATAL: Error mounting SPIFFS file system."));
    }

    // Format
    PLV_DEBUG(F("Formatting file system... (relax, this could take a while)"));
    PLV_DEBUG(F("Ok, \"a while\" is  -- uh -- too much generic, it takes 1m 20sec on average. Better, no? ;)"));

    int start = millis();
    if (SPIFFS.format()) {
	    PLV_DEBUG(F("SUCCESS: File system formatted successfully."));
	    PLV_DEBUG_(F("Total time: "));
	    PLV_DEBUG_((millis() - start));
	    PLV_DEBUG(F(" ms"));
    } else {
    	PLV_DEBUG(F("ERROR: Fail formatting file system."));
    }
}

/**
 * Convert bytes in to KB and MB.
 *
 * @param bytes float number to be converted
 * @param prefix the unit to be converted.
 * @return the number formatted acording to the specified unit
 */
float PluviOn::bytesConverter(float bytes, char prefix) {

    // Kilobyte (KB)
    if (prefix == 'K') {
	    return bytes / 1000;

    // Megabyte (MB)
    } else if (prefix == 'M') {
    	return bytes / 1000000;
    }
}