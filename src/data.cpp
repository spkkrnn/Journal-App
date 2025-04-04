#include "data.h"

static char* tmpKey = nullptr;

static int callback(void* data, int argc, char** argv, char** colNames) 
{ 
    tmpKey = (char*) malloc(crypto_pwhash_STRBYTES);
    strncpy(tmpKey, argv[0], crypto_pwhash_STRBYTES);
    return 0;
} 

int sqlExecute(sqlite3* db, const std::string command, bool returnData=false) {
    int sqlError = 0;
    char* errMsg;
    if (returnData) {
        sqlError = sqlite3_exec(db, command.c_str(), callback, 0, &errMsg);
    }
    else {
        sqlError = sqlite3_exec(db, command.c_str(), NULL, 0, &errMsg);
    }
    if (sqlError != SQLITE_OK) {
        std::cout << "Failed to execute SQL command." << std::endl;
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

int checkPassword(sqlite3* db, std::string passwd) {
    const std::string sqlQuery = "SELECT PASSWORD FROM JKEYS;";
    int retval = 0; // magic return value
    if ((retval = sqlExecute(db, sqlQuery, true)) < 0) {
        return -1;
    }
    size_t pwLength = passwd.length();
    const char* charPw = passwd.c_str();
    if (crypto_pwhash_str_verify(tmpKey, charPw, pwLength) != 0) {
        std:: cout << "Wrong password!" << std::endl;
    }
    else {
        std::cout << "Password correct!" << std::endl;
        retval = 1;
    }
    memset(tmpKey, 0, crypto_pwhash_STRBYTES);
    free(tmpKey);
    return retval;
}

int hashPassword(std::string passwd, char* stored) {
    char hashed[crypto_pwhash_STRBYTES];
    const char* const p_passwd = (const char* const) passwd.c_str(); // crypto_pwhash_str needs const char* const
    size_t pwLength = passwd.length();
    if (crypto_pwhash_str(hashed, p_passwd, pwLength, crypto_pwhash_OPSLIMIT_MODERATE, crypto_pwhash_MEMLIMIT_MODERATE) != 0) {
        perror("Out of memory.");
        return -1;
    }
    strncpy(stored, hashed, crypto_pwhash_STRBYTES);
    std::cout << "Set pw: " << hashed << std::endl;
    return 0;
}

bool setPassword(sqlite3 *db, std::string password) {
    char* hashedPw = (char*) malloc(crypto_pwhash_STRBYTES);
    if (hashPassword(password, hashedPw) < 0) {
        free(hashedPw);
        return false;
    }
    std::string strHash(hashedPw);
    free(hashedPw);
    const std::string sqlCommand = "INSERT INTO JKEYS (PASSWORD) VALUES(\'" + strHash + "\');";
    if (sqlExecute(db, sqlCommand, false) < 0) {
        return false;
    }
    return true;
}

int saveEntry(sqlite3* db, std::string journalEntry) {
    std::time_t saveTime = std::time(nullptr);
    const std::string sqlCommand = "INSERT INTO JENTRIES VALUES(" + std::to_string(saveTime) + ", \'" + journalEntry +  "\');";
    int retval = sqlExecute(db, sqlCommand, false);
    return retval;
}