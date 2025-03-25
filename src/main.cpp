#include "data.h"
#include "guiserver.h"

int main(int argc, char* argv[]) {
    if (argc == 2) {
        std::string flag(argv[1], MAX_NAME);
        if (flag[0] == '-') {
            flag.erase(0, 1);
        }
        if (flag == "install") {
            std::cout << "*** INSTALLATION ***" << std::endl;
            std::string pw;
            std::cout << "Please set password:" << std::endl;
            std::cin >> std::setw(MAX_NAME) >> pw;
            
            // create SQL database
            sqlite3* database;
            int sqlError = 0;
            sqlError = sqlite3_open(DBFILE, &database);
            if (sqlError) {
                std::cout << "Could not create database." << std::endl;
                return -1;
            }
            std::string sqlCommand = "CREATE TABLE Keys (\nPASSWORD BINARY NOT NULL\nPRIMARY KEY (PASSWORD);";
            if (sqlExecute(database, sqlCommand) < 0) {
                std::cout << "Failed to create table." << std::endl;
                sqlite3_close(database);
                return -1;
            }
            if (!setPassword(database, pw)) {
                std::cout << "Setting password failed." << std::endl;
                sqlite3_close(database);
                return -1;
            }
            sqlite3_close(database);
        }
        else if (flag == "run") {
            std::cout << "Initializing..." << std::endl;
            runServer();
        }
    }
    else {
        std::cout << "Usage:\ninstall\n\tUse flag install to set password and to start use." << std::endl;
        std::cout << "run\n\tUse flag run to run the program." << std::endl;
    }
    return 0;
}
