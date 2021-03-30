#include <ctime>
#include <cstring>
#include <fstream>
#include <iostream>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#define LISTS_FOLDER "/.todo_lists/"
#define LISTS_SUFFIX ".txt"
#define TIME_STR_LEN 10 // length of YYYY-MM-DD string
#define SUFFIX_LEN 4

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define COLOR_RESET "\033[0m"
#define BOLD "\033[1m"

using namespace std;

enum command {
    r, // read
    add,
    rm, // remove
    cplt, // complete
    help
};

void printUsage() {
    cerr << "Usage: todo command" << endl;
    cerr << "A list of commands can be found by running \"todo help\"" << endl;
}

// given a file path, generates a temporary file by adding .temp after it. 
// and store the temp file path and the opened temp stream in the references
// provided.
// returns true on a successful operation, false otherwise
bool getTempOutputStream(const string& filepath,
                         string& temppath, ofstream* tempStream) {
    temppath = filepath;
    temppath.append(".temp");
    tempStream->open(temppath, ios::out);
    if (tempStream->fail()) {
        cerr << "Could not create a temporary file at: " << temppath << endl;
        return false;
    } else {
        return true;
    }
}

bool outputTodoLists(string& filepath, struct tm* curr_time) {
    ifstream file;
    file.open(filepath, ios_base::in);
    if (file.fail()) {
        cerr << "Could not open the list file at: " << filepath << endl;
        return false;;
    }

    // displays date
    char timeStr[128];
    strftime(timeStr, 128, "%B %d, %Y, %A", curr_time);
    cout << BOLD << timeStr << COLOR_RESET << endl;

    // output each item at a time
    string item;
    unsigned int count = 1;
    bool completed = false;
    while (getline(file, item)) {
        if (item == "____below_are_completed____") {
            completed = true;
            continue;
        }
        if (item.size() == 0) {
            continue;
        } else if (!completed) {
            cout << RED << count++ << ". " << item << COLOR_RESET << endl;
        } else {
            cout << GREEN << item << COLOR_RESET << endl;
        }
    }
    file.close();
    return true;
}

// remove a task
// returns true if a deletion was made, false otherwise
bool removeTask(string& filepath, unsigned int taskNum, string* removedTask) {
    ifstream file;
    file.open(filepath, ios::in);
    if (file.fail()) {
        cerr << "Could not edit the list file at: " << filepath << endl;
        return false;
    }
    
    // create a temp file as a buffer
    string temppath;
    ofstream temp;
    if (!getTempOutputStream(filepath, temppath, &temp)) {
        return false;
    }

    // copying to temp file, skipping the target task
    string item;
    int taskCount = 1;
    bool deleted = false; // a deletion is made
    while (getline(file, item)) {
        if (item.size() == 0) {
            continue;
        } else if (taskCount++ == taskNum) { // line to skip
            deleted = true;
            if (removedTask) {
              *removedTask = item;
            }
            continue;
        } else { // copy everything else
            temp << item << endl;
        }
    }
    file.close();
    temp.close();

    // replacing the old file
    remove(filepath.c_str());
    rename(temppath.c_str(), filepath.c_str());
    
    return deleted;
}

bool addTask(string& filepath, const string& taskStr) {
    fstream file;
    file.open(filepath, ios::in);
    if (file.fail()) {
        // create the file if it doesn't exist
        file.open(filepath, ios::out);
        if (file.fail()) {
            cerr << "The file location cannot be accessed: " << filepath;
            return false;
        }
        file << "____below_are_completed____" << endl;
        file.close();

        // reopen the file as input
        file.open(filepath, ios::in);
    }
    
    // create a temp file as a buffer
    string temppath;
    ofstream temp;
    if (!getTempOutputStream(filepath, temppath, &temp)) {
        return false;
    }

    string item;
    bool completed = false;
    while (getline(file, item)) {
        // append the new task at the end
        if (item == "____below_are_completed____") {
            completed = true;
            temp << taskStr << endl;
        }

        // copy everything else
        if (item.size() == 0) {
            continue;
        } else {
            temp << item << endl;
        }
    }

    file.close();
    temp.close();

    // replacing the old file
    remove(filepath.c_str());
    rename(temppath.c_str(), filepath.c_str());

    return true;
}

// complete a task.
// returns true if an edit to the list is made, false otherwise
bool completeTask(string& filepath, unsigned int taskNum, struct tm* currTime) {
    string removedTask;
    if (removeTask(filepath, taskNum, &removedTask)) {
        fstream file;
        file.open(filepath, ios::app);
        file << removedTask << endl;
        file.close();
        return true;
    } else {
        return false;
    }
}

int main(int argc, char* argv[]) {
    // argument check
    if (argc < 2) {
        printUsage();
        return EXIT_FAILURE;
    }

    // identify the command
    command userCommand;
    if (!strncmp(argv[1], "read", 5) ||
        !strncmp(argv[1], "r", 2)) {
        userCommand = r;
    } else if (!strncmp(argv[1], "add", 4) ||
               !strncmp(argv[1], "a", 2)) {
        userCommand = add;
    } else if (!strncmp(argv[1], "remove", 7) ||
               !strncmp(argv[1], "rm", 3)) {
        userCommand = rm;
    } else if (!strncmp(argv[1], "complete", 9) ||
               !strncmp(argv[1], "c", 2)) {
        userCommand = cplt;
    } else if (!strncmp(argv[1], "help", 5)) {
        userCommand = help;
    } else {
        cerr << "Invalid command" << endl;
        return EXIT_FAILURE;
    }

    // determines the user's home directory
    const char* homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }

    // find the current time
    time_t currTimeRaw = time(0);
    if (currTimeRaw == (time_t) -1) {
        perror("Could not fetch the current time: ");
    }
    struct tm *currTime = localtime(&currTimeRaw);

    // generate a filename for the list today, and the full path
    char filename[TIME_STR_LEN + SUFFIX_LEN + 1];
    size_t len = strftime(filename, TIME_STR_LEN+1, "%F", currTime);
    if (!len) {
        perror("Error interpreting the current time: ");
    } else {
        strncpy(filename + len, LISTS_SUFFIX, SUFFIX_LEN+1);
    }
    string filepath(homedir);
    filepath.append(LISTS_FOLDER);
    filepath.append(filename);

    int taskNum; // the num for the task to be modified
    string task; // the task to add (in the case of command add)
    bool success; // success or failure in operation
    switch (userCommand) {
        case r:
            success = outputTodoLists(filepath, currTime);
            break;

        case add:
            // checking input
            if (argc < 3) {
                cerr << "Error: which task do you want to remove?" << endl;
                return EXIT_FAILURE;
            }

            // combine all arguments from argv[2] to assemble a task
            task.append(argv[2]);
            for (int i = 3; i < argc; ++i) {
                task.append(" ");
                task.append(argv[i]);
            }
            
            success = addTask(filepath, task);
            cout << "Task added:" << endl;

            // displaying the todo lists for convenience
            outputTodoLists(filepath, currTime);
            break;

        case rm:
            // checking input
            if (argc < 3) {
                cerr << "Error: which task do you want to remove?" << endl;
                return EXIT_FAILURE;
            }

            if (!sscanf(argv[2], "%d", &taskNum) || taskNum <= 0) {
                cerr << "Error: invalid task number." << endl;
                return EXIT_FAILURE;
            }
            
            if (success = removeTask(filepath, taskNum, NULL)) {
                cout << "List updated as follows:" << endl;
            } else {
                cout << "the task number was not present, list is unchanged:" << endl;
            }

            // displaying the todo lists for convenience
            outputTodoLists(filepath, currTime);
            break;

        case cplt:
            // checking input
            if (argc < 3) {
                cerr << "Error: which task do you want to complete?" << endl;
                return EXIT_FAILURE;
            }

            if (!sscanf(argv[2], "%d", &taskNum) || taskNum <= 0) {
                cerr << "Error: invalid task number." << endl;
                return EXIT_FAILURE;
            }

            if (success = completeTask(filepath, taskNum, NULL)) {
                cout << "List updated as follows:" << endl;
            } else {
                cout << "the task number was not present, list is unchanged:" << endl;
            }

            // displaying the todo lists for convenience
            outputTodoLists(filepath, currTime);
            break;
        case help:
            cout << "--- WORK IN PROGRESS ---" << endl;
            break;
    }

    return EXIT_SUCCESS;
}
