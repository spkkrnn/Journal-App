#include "data.h"

static char* tmpKey = nullptr;
static int tmpFeed = -1;

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
    size_t origLen = 0;
    time_t timestamp = 0;
    std::string nonceB64;
    unsigned char* jentry;
    for (i = 0; i < argc; i++) {
        std::string row = (argv[i] ? argv[i] : "NULL");
        switch (colNames[i][0]) {
            case 'T':
                timestamp = std::stoi(row, nullptr);
                break;
            case 'N':
                nonceB64 = row;
                break;
            case 'O':
                origLen = std::stoi(row, nullptr);
                break;
            case 'J':
                if (origLen > 0 && !nonceB64.empty()) {
                    size_t entryLen = origLen - crypto_secretbox_MACBYTES + 1;
                    jentry = (unsigned char*) calloc(entryLen, sizeof(char));
                    decodeAndDecrypt(jentry, row, nonceB64, origLen);
                    printf("Date: %s%s\n\n", ctime(&timestamp), jentry);
                    free(jentry);
                    jentry = nullptr;
                    nonceB64.clear();
                    origLen = 0;
                }
                break;
        }
    }
    return 0;
}

static int callbackWrite(void* data, int argc, char** argv, char** colNames) 
{
    if (tmpFeed < 0) {
        std::cout << "No client feed." << std::endl;
        return -1;
    }
    int i;
    size_t origLen = 0;
    time_t timestamp = 0;
    std::string nonceB64;
    unsigned char* jentry;
    for (i = 0; i < argc; i++) {
        std::string row = (argv[i] ? argv[i] : "NULL");
        switch (colNames[i][0]) {
            case 'T':
                timestamp = std::stoi(row, nullptr);
                break;
            case 'N':
                nonceB64 = row;
                break;
            case 'O':
                origLen = std::stoi(row, nullptr);
                break;
            case 'J':
                if (origLen > 0 && !nonceB64.empty()) {
                    size_t entryLen = origLen - crypto_secretbox_MACBYTES + 1;
                    size_t buffLen = entryLen + PADDING;
                    jentry = (unsigned char*) calloc(entryLen, sizeof(char));
                    char buffer[buffLen] = {0};
                    decodeAndDecrypt(jentry, row, nonceB64, origLen);
                    snprintf(buffer, buffLen, "<div>Date: %s<br><br>%s</div>", ctime(&timestamp), jentry);
                    if (write(tmpFeed, buffer, strnlen(buffer, buffLen)) < 0) {
                        perror("error writing journal entry");
                        close(tmpFeed);
                        free(jentry);
                        return -1;
                    }
                    free(jentry);
                    jentry = nullptr;
                    nonceB64.clear();
                    origLen = 0;
                }
                break;
        }
    }
    return 0; 
}

void freeTmp() {
    free(tmpKey);
    tmpKey = nullptr;
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
    freeTmp();
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
    freeTmp();
    return 0;
}

std::string deriveKey(sqlite3* db, std::string pw, bool tmpCopy) {
    char* salt = (char*) calloc(crypto_pwhash_SALTBYTES, sizeof(char));
    if (getSalt(db, salt) < 0) {
        free(salt);
        return "";
    }
    unsigned char key[crypto_box_SEEDBYTES];
    if (crypto_pwhash(key, sizeof(key), pw.c_str(), pw.length(), (unsigned char*) salt, crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_DEFAULT) != 0) {
        std::cout << "Out of memory!" << std::endl;
        free(salt);
        return "";
    }
    free(salt);
    if (tmpCopy) {
        tmpKey = (char*) malloc(crypto_box_SEEDBYTES);
        strncpy(tmpKey, (char*) key, crypto_box_SEEDBYTES);
    }
    std::string keyStr((char*) key, crypto_box_SEEDBYTES);
    return keyStr;
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

int decodeAndDecrypt(unsigned char* decrypted, std::string encoded, std::string nonce, const size_t cryptedLen) {
    unsigned char* encrypted = (unsigned char*) calloc(cryptedLen, sizeof(char));
    const size_t codedLen = encoded.length();
    if (sodium_base642bin(encrypted, cryptedLen, encoded.c_str(), codedLen, nullptr, nullptr, nullptr, ENCTYPE) < 0) {
        std::cout << "More space required or could not fully parse." << std::endl;
    }
    unsigned char* nonceBytes = (unsigned char*) calloc(crypto_secretbox_NONCEBYTES, sizeof(char));
    const size_t nonceLen = nonce.length();
    if (sodium_base642bin(nonceBytes, crypto_secretbox_NONCEBYTES, nonce.c_str(), nonceLen, nullptr, nullptr, nullptr, ENCTYPE) < 0) {
        std::cout << "More space required or could not fully parse nonce." << std::endl;
    }
    int retval = 0;
    char key[crypto_box_SEEDBYTES];
    memcpy(key, tmpKey, crypto_box_SEEDBYTES);
    if (crypto_secretbox_open_easy(decrypted, encrypted, (unsigned long long) cryptedLen, nonceBytes, (unsigned char *) key) != 0) {
        std::cout << "Decryption error!" << std::endl;
    }
    free(encrypted);
    free(nonceBytes);
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
    const std::string sqlCommand = "INSERT INTO JENTRIES (TIME, NONCE, ORIGLEN, JENTRY) VALUES(" + std::to_string(saveTime) + ", \'" + std::string(encodedNonce, nonceLen-1) + "\', " + std::to_string(cryptedLen) + ", \'" + std::string(encodedEntry, entryLen-1) + "\');";
    int retval = sqlExecute(db, sqlCommand, false);
    return retval;
}

int printEntries(sqlite3* db, std::string pw) {
    //tmpKey = (char*) malloc(crypto_box_SEEDBYTES);
    deriveKey(db, pw, true);
    int sqlError = 0;
    char* errMsg;
    std::string sqlCommand = "SELECT * FROM JENTRIES;";
    sqlError = sqlite3_exec(db, sqlCommand.c_str(), callbackPrint, 0, &errMsg);
    if (sqlError != SQLITE_OK) {
        std::cout << "Failed to execute SQL command." << std::endl;
        sqlite3_free(errMsg);
        freeTmp();
        return -1;
    }
    freeTmp();
    return 0;
}

int sendEntries(sqlite3* db, std::string key, int clientFeed) {
    tmpKey = (char*) malloc(crypto_box_SEEDBYTES);
    strncpy(tmpKey, (char*) key.c_str(), crypto_box_SEEDBYTES);
    tmpFeed = clientFeed;
    int sqlError = 0;
    char* errMsg;
    std::string sqlCommand = "SELECT * FROM JENTRIES;";
    sqlError = sqlite3_exec(db, sqlCommand.c_str(), callbackWrite, 0, &errMsg);
    if (sqlError != SQLITE_OK) {
        std::cout << "Failed to execute SQL command." << std::endl;
        sqlite3_free(errMsg);
        freeTmp();
        return -1;
    }
    tmpFeed = -1;
    freeTmp();
    return 0;
}