#include "data.h"

static int callback(void* data, int argc, char** argv, char** colNames) 
{ 
    fprintf(stderr, "%s: ", (const char*)data); 
  
    for (int i = 0; i < argc; i++) { 
        printf("%s = %s\n", colNames[i], argv[i] ? argv[i] : "NULL"); 
    } 
  
    printf("\n"); 
    return 0; 
} 

int sqlExecute(sqlite3 *db, std::string command) {
    int sqlError = 0;
    char* errMsg;
    sqlError = sqlite3_exec(db, command.c_str(), NULL, 0, &errMsg);
    if (sqlError != SQLITE_OK) {
        std::cout << "Failed to execute SQL command." << std::endl;
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

int checkPassword(sqlite3 *db, std::string passwd) {
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
    return 0;
}

bool setPassword(sqlite3 *db, std::string password) {
    char* hashedPw = (char*) malloc(crypto_pwhash_STRBYTES);
    if (hashPassword(password, hashedPw) < 0) {
        return false;
    }
    std::string strHash(hashedPw);
    std::string sqlCommand = "INSERT INTO Keys (PASSWORD) \nVALUES(" + strHash + ");";
    if (sqlExecute(db, sqlCommand) < 0) {
        return false;
    }
    return true;
}
