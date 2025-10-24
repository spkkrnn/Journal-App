#include "guiserver.h"

void printHelp() {
    std::cout << "Usage:\ninstall\n\tWith the install flag you can set a password and the database is created." << std::endl;
    std::cout << "run\n\tUse flag run to run the program." << std::endl;
    std::cout << "show\n\tPrints all journal entries unless a time window is provided. You may print entries between specific months with arguments in MMYY format." << std::endl;
    std::cout << "\tExample: show 0925 1125\n\tPrints entries from September, October and November 2025." << std::endl;
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

time_t convertTime(std::string userTime, bool end=false) {
    if (userTime.length() != 4) return -1;
    int month = std::stoi(userTime.substr(0, 2), nullptr) - 1; // ADD: exception handling
    int year = std::stoi(userTime.substr(2, 2), nullptr) + 100;
    struct tm dateTime = {.tm_mon = month, .tm_year = year};
    if (end) { // Could just increment month by one?
        int lastDay = MDAYS[month];
        if (month == 1 && year % 4 == 0) { // leap year
            lastDay++;
        }
        dateTime.tm_mday = lastDay;
        dateTime.tm_min = 59;
        dateTime.tm_sec = 60;
    }
    return mktime(&dateTime);
}

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        std::string flag(argv[1]);
        while (flag[0] == '-') {
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
        else if (flag == "show" || flag == "print") {
            std::string pw;
            std::cout << "Please type password to open journal:" << std::endl;
            std::cin >> std::setw(MAX_NAME) >> pw;
            hidePassword(pw.length());
            if (pw.length() < MINPWLEN) {
                std::cout << "Invalid password." << std::endl;
            }
            else if (checkPassword(database, pw) == 1) {
                if (argc > 2) {
                    time_t start = convertTime(argv[2]);
                    time_t end = (argc > 3) ? convertTime(argv[3], true) : std::time(nullptr);
                    std::cout << start << " and " << end << std::endl;
                    if (start > 0 && end > 0 && end > start) {
                        setTimes(start, end);
                    }
                    else {
                        std::cout << "Invalid time(s) to search. Use flag help for syntax." << std::endl;
                    }
                }
                std::cout << "Printing journal entries...\n" << std::endl;
                if (printEntries(database, pw) < 0) {
                    sqlite3_close(database);
                    return -1;
                }
                setTimes(0, 0);
            }
        }
        else if (flag == "reset") {
            std::cout << "*** PASSWORD RESET ***" << std::endl;
            std::string oldPw;
            std::cout << "Please type the current password:" << std::endl;
            std::cin >> std::setw(MAX_NAME) >> oldPw;
            hidePassword(oldPw.length());
            if (oldPw.length() < MINPWLEN) {
                std::cout << "Invalid password." << std::endl;
            }
            else if (checkPassword(database, oldPw) == 1) {
                std::string newPw;
                std::cout << "Set a new password:" << std::endl;
                std::cin >> std::setw(MAX_NAME) >> newPw;
                hidePassword(newPw.length());
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
