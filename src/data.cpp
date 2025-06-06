#include "data.h"

static char* tmpKey = nullptr;

static int callback(void* data, int argc, char** argv, char** colNames) 
{ 
    tmpKey = (char*) malloc(crypto_pwhash_STRBYTES);
    strncpy(tmpKey, argv[0], crypto_pwhash_STRBYTES);
    return 0;
}

static int callbackSalt(void* data, int argc, char** argv, char** colNames) 
{ 
    tmpKey = (char*) malloc(crypto_pwhash_SALTBYTES);
    strncpy(tmpKey, argv[0], crypto_pwhash_SALTBYTES);
    return 0;
}

static int callbackPrint(void* data, int argc, char** argv, char** colNames) 
{ 
    int i; 
    //fprintf(stderr, "%s: ", (const char*)data); 
  
    for (i = 0; i < argc; i++) {
        std::string row = (argv[i] ? argv[i] : "NULL");
        std::cout << colNames[i] << ": " << row << std::endl;
        //printf("%s = %s\n", colNames[i], argv[i] ? argv[i] : "NULL"); 
    } 
    std::cout << std::endl;
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
        printf("%s\n", errMsg);
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

int getSalt(sqlite3* db, char* salt) {
    const std::string sqlQuery = "SELECT SALT FROM JKEYS;";
    int sqlError = 0;
    char* errMsg;
    sqlError = sqlite3_exec(db, sqlQuery.c_str(), callbackSalt, 0, &errMsg);
    if (sqlError != SQLITE_OK) {
        std::cout << "Failed to execute SQL command." << std::endl;
        sqlite3_free(errMsg);
        return -1;
    }
    strncpy(salt, tmpKey, crypto_pwhash_SALTBYTES);
    free(tmpKey);
    return 0;
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
    return 0;
}

bool setPassword(sqlite3 *db, std::string password) {
    char* hashedPw = (char*) malloc(crypto_pwhash_STRBYTES);
    unsigned char salt[crypto_pwhash_SALTBYTES];
    randombytes_buf(salt, sizeof(salt));
    if (hashPassword(password, hashedPw) < 0) {
        free(hashedPw);
        return false;
    }
    std::string strHash(hashedPw);
    free(hashedPw);
    const std::string sqlCommand = "INSERT INTO JKEYS VALUES(\'" + strHash + "\', \'" + std::string((const char*) salt, crypto_pwhash_SALTBYTES) + "\');";
    //sqlCommand.append((const char*) salt, crypto_pwhash_SALTBYTES);
    //sqlCommand += std::string("\');");
    if (sqlExecute(db, sqlCommand, false) < 0) {
        return false;
    }
    return true;
}

bool resetPassword(sqlite3 *db, std::string newPw) {
    const std::string removeCmd = "DELETE FROM JKEYS;";
    if (sqlExecute(db, removeCmd, false) < 0) { // remove old password
        return false;
    }
    char* hashedPw = (char*) malloc(crypto_pwhash_STRBYTES);
    if (hashPassword(newPw, hashedPw) < 0) {
        free(hashedPw);
        return false;
    }
    std::string strHash(hashedPw);
    free(hashedPw);
    const std::string sqlCommand = "INSERT INTO JKEYS (PASSWORD) VALUES(\'" + strHash + "\');";
    if (sqlExecute(db, sqlCommand, false) < 0) { // set new password
        return false;
    }
    return true;
}

int encryptEntry(std::string &entry, std::string &key, unsigned char* encrypted, unsigned char* nonce) {
    unsigned char* entryTxt = (unsigned char *) entry.c_str();
    randombytes_buf(nonce, sizeof(nonce));
    int retval = crypto_secretbox_easy(encrypted, entryTxt, (unsigned long long) entry.length(), nonce, (unsigned char *) key.c_str());
    return retval;
}

int decryptEntry(unsigned char* decrypted, std::string &key, unsigned char* encrypted, unsigned char* nonce, unsigned long long cryptedLen) {
    int retval = crypto_secretbox_open_easy(decrypted, encrypted, cryptedLen, nonce, (unsigned char *) key.c_str());
    return retval;
} 

int saveEntry(sqlite3* db, std::string journalEntry, std::string key) {
    // set up variables
    size_t cryptedLen = crypto_secretbox_MACBYTES + journalEntry.length();
    size_t entryLen = sodium_base64_ENCODED_LEN(cryptedLen, ENCTYPE);
    size_t nonceLen = sodium_base64_ENCODED_LEN(crypto_secretbox_NONCEBYTES, ENCTYPE);
    unsigned char encryptedEntry[cryptedLen];
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    char encodedEntry[entryLen]; 
    char encodedNonce[nonceLen];
    // encrypt and encode to base64
    if (encryptEntry(journalEntry, key, encryptedEntry, nonce) < 0) {
        std::cout << "Failed to encrypt journal entry." << std::endl;
        return -1;
    }
    sodium_bin2base64(encodedEntry, entryLen, encryptedEntry, cryptedLen, ENCTYPE);
    sodium_bin2base64(encodedNonce, nonceLen, nonce, crypto_secretbox_NONCEBYTES, ENCTYPE);
    // save to database
    std::time_t saveTime = std::time(nullptr);
    const std::string sqlCommand = "INSERT INTO JENTRIES (TIME, JENTRY, NONCE, ORIGLEN) VALUES(" + std::to_string(saveTime) + ", \'" + std::string(encodedEntry, entryLen-1) + "\', \'" + std::string(encodedNonce, nonceLen-1) + "\', " + std::to_string(cryptedLen) + ");";
    std::cout << sqlCommand << std::endl; // test print
    int retval = sqlExecute(db, sqlCommand, false);
    return retval;
}

int printEntries(sqlite3* db) {
    int sqlError = 0;
    char* errMsg;
    std::string sqlCommand = "SELECT * FROM JENTRIES;";
    sqlError = sqlite3_exec(db, sqlCommand.c_str(), callbackPrint, 0, &errMsg);
    if (sqlError != SQLITE_OK) {
        std::cout << "Failed to execute SQL command." << std::endl;
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}