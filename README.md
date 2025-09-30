# Journal Application
A C++ application for making journal entries with a web GUI. The journal requires a password to log in, and the journal entries are encrypted and saved to an SQL database.

## Required libraries
Libsodium
SQLite

## Installation and usage
Can be compiled in the project's src folder like so, for example:
    g++ main.cpp guiserver.cpp guiserver.h data.cpp data.h -o main -lsqlite3 -lsodium
To create a database and set a password for your journal, run:
    ./main install
Then you can run the program and open the login page in the browser:
    ./main run
For all options, simply run *main* without arguments.

## Implemented features
- Password login with libsodium
- Database with SQLite
- User can save an entry to the database
- CSS loading animation while password is checked
- Journal entries can be printed to terminal
- Password can be changed without reinstalling
- Encryption for journal entries
- Viewing journal entries in the browser
- HTTP/1.1 chunked encoding
- Making multiple entries in a single session