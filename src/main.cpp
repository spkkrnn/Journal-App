#include "data.h"
#include "guiserver.h"

void printHelp() {
    std::cout << "Usage:\ninstall\n\tUse flag install to set password and to start use." << std::endl;
    std::cout << "run\n\tUse flag run to run the program." << std::endl;
    std::cout << "show\n\tYou can print all journal entries with the show flag." << std::endl;
    std::cout << "reset\n\tTo change the password, use flag reset." << std::endl;
}

void hidePassword(size_t pwLen) {
    std::cout << "\x1b[1F";
    for (unsigned int i = 0; i < pwLen; i++) {
        std::cout << "*" << std::flush;
        usleep(HIDE_PW_SPEED);
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        std::string flag(argv[1]);
        if (flag[0] == '-') {
            flag.erase(0, 1);
        }
        // check if database exists
        bool installed = std::filesystem::exists(Files::dirPath + DBFILE);
        if ((!installed) && (flag != "install")) {
            std::cout << "Program not installed! Please use flag install to set a password and create a database." << std::endl;
            return 0;
        }
        // open SQL database
        sqlite3* database;
        int sqlError = 0;
        sqlError = sqlite3_open(DBFILE, &database);
        if (sqlError) {
            std::cout << "Could not open database." << std::endl;
            return -1;
        }

        if (flag == "install") {
            if (installed) {
                std::cout << "Database found. Already installed." << std::endl;
                sqlite3_close(database);
                return 0;
            }
            std::cout << "*** INSTALLATION ***" << std::endl;
            std::string pw;
            std::cout << "Please set password:" << std::endl;
            std::cin >> std::setw(MAX_NAME) >> pw;
            if (pw.length() < MINPWLEN) {
                std::cout << "Password must be at least " << MINPWLEN << " characters. Exiting." << std::endl; 
                sqlite3_close(database);
                return 0;
            }
            hidePassword(pw.length());
            const std::string pwTable = "CREATE TABLE JKEYS(PASSWORD CHAR PRIMARY KEY NOT NULL, SALT CHAR NOT NULL );";
            if (sqlExecute(database, pwTable, false) < 0) {
                std::cout << "Failed to create table for passwords." << std::endl;
                sqlite3_close(database);
                return -1;
            }
            const std::string entriesTable = "CREATE TABLE JENTRIES(TIME TIMESTAMP PRIMARY KEY NOT NULL, NONCE CHAR NOT NULL, ORIGLEN SMALLINT NOT NULL, JENTRY TEXT NOT NULL );";
            if (sqlExecute(database, entriesTable, false) < 0) {
                std::cout << "Failed to create table for journal entries." << std::endl;
                sqlite3_close(database);
                return -1;
            }
            if (!setPassword(database, pw)) {
                std::cout << "Setting password failed." << std::endl;
                sqlite3_close(database);
                return -1;
            }
            std::cout << "Verifying that password was saved correctly." << std::endl;
            checkPassword(database, pw);
        }
        else if (flag == "run") {
            std::cout << "Initializing..." << std::endl;
            runServer(database);
        }
        else if (flag == "show") {
            std::string pw;
            std::cout << "Please type password to open journal:" << std::endl;
            std::cin >> std::setw(MAX_NAME) >> pw;
            if (pw.length() < MINPWLEN) {
                std::cout << "Invalid password." << std::endl;
            }
            else if (checkPassword(database, pw) == 1) {
                std::cout << "Printing journal entries...\n" << std::endl;
                if (printEntries(database, pw) < 0) {
                    sqlite3_close(database);
                    return -1;
                }
            }
        }
        else if (flag == "reset") {
            std::cout << "*** PASSWORD RESET ***" << std::endl;
            std::string oldPw;
            std::cout << "Please type the current password:" << std::endl;
            std::cin >> std::setw(MAX_NAME) >> oldPw;
            if (oldPw.length() < MINPWLEN) {
                std::cout << "Invalid password." << std::endl;
            }
            else if (checkPassword(database, oldPw) == 1) {
                std::string newPw;
                std::cout << "Set a new password:" << std::endl;
                std::cin >> std::setw(MAX_NAME) >> newPw;
                if (newPw.length() < MINPWLEN) {
                    std::cout << "Password must be at least " << MINPWLEN << " characters. Exiting." << std::endl; 
                }
                else if (resetPassword(database, newPw)) {
                    std::cout << "Password changed!" << std::endl;
                }
            }
        }
        else {
            printHelp();
        }
        sqlite3_close(database);
    }
    else {
        printHelp();
    }
    return 0;
}
