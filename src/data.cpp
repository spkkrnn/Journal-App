#include "data.h"

static char* tmpKey = nullptr;

static int callback(void* data, int argc, char** argv, char** colNames) 
{ 
    /*fprintf(stderr, "%s: ", (const char*)data); 
    for (int i = 0; i < argc; i++) { 
        printf("%s = %s\n", colNames[i], argv[i] ? argv[i] : "NULL"); 
    }*/
    tmpKey = (char*) malloc(PWBUFSIZE);
    strncpy(tmpKey, argv[0], PWBUFSIZE);
    return 0; 
} 

int sqlExecute(sqlite3 *db, std::string command, bool returnData=false) {
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

int checkPassword(sqlite3 *db, std::string passwd) {
    std::string sqlQuery = "SELECT PASSWORD FROM JKEYS;";
    sqlExecute(db, sqlQuery, true);
    std::cout << "Saved pw: " << tmpKey << std::endl;
    free(tmpKey);
    /*FILE* hfile = fopen(HFILE_PATH, "r");
    char* rhash = (char*) malloc(crypto_pwhash_STRBYTES);
    if (fgets(rhash, crypto_pwhash_STRBYTES, hfile) == NULL) {
        fclose(hfile);
        free(rhash);
        perror("Error reading file.");
        return -1;
    }
    if (crypto_pwhash_str_verify(rhash, passwd, pwlen) != 0) {
        printf("Wrong password!\n");
    }
    else {
        printf("Password correct!\n");
    }
    fclose(hfile);
    free(rhash);*/
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
    if (crypto_pwhash_str_verify(hashed, p_passwd, pwLength) != 0) {
        perror("Invalid password.");
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
    std::string sqlCommand = "INSERT INTO JKEYS (PASSWORD) VALUES(\'" + strHash + "\');";
    if (sqlExecute(db, sqlCommand, false) < 0) {
        return false;
    }
    return true;
}
