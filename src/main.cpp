#include "data.h"
#include "guiserver.h"

int main(int argc, char* argv[]) {
    if (argc == 2) {
        std::string flag(argv[1]);
        if (flag[0] == '-') {
            flag.erase(0, 1);
        }
        // check if database exists
        bool installed = std::filesystem::exists(Files::dirPath + DBFILE);
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
            std::cout << "Please set password:" << std::endl; // special characters may be a problem
            std::cin >> std::setw(MAX_NAME) >> pw;
            if (pw.length() < MINPWLEN) {
                std::cout << "Password must be at least " << MINPWLEN << " characters. Exiting." << std::endl; 
                return 0;
            }
            const std::string pwTable = "CREATE TABLE JKEYS(PASSWORD CHAR PRIMARY KEY NOT\tNULL );";
            if (sqlExecute(database, pwTable, false) < 0) {
                std::cout << "Failed to create table for passwords." << std::endl;
                sqlite3_close(database);
                return -1;
            }
            const std::string entriesTable = "CREATE TABLE JENTRIES(TIME TIMESTAMP NOT NULL, JENTRY TEXT NOT NULL );";
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
        sqlite3_close(database);
    }
    else {
        std::cout << "Usage:\ninstall\n\tUse flag install to set password and to start use." << std::endl;
        std::cout << "run\n\tUse flag run to run the program." << std::endl;
    }
    return 0;
}
