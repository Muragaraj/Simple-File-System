#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <zlib.h> // For compression and decompression
#include <termios.h> // For real time color updates

#define MAX_PATH_LENGTH 2048

enum nodeType {File, Folder, Symlink};

// Define Google colors using ANSI escape codes
const char* YELLOW = "\033[38;5;226m"; // Google Yellow
const char* CYAN = "\033[36m";         // Cyan for folders
const char* BLUE = "\033[38;5;33m";    // Google Blue for symlinks
const char* GREEN = "\033[38;5;46m"; // Google Green
const char* RESET = "\033[0m";       // Reset to default

typedef struct node {
    enum nodeType type;
    char* name;
    int numberOfItems;
    size_t size;
    time_t date;
    char* content;
    struct node* previous;
    struct node* parent;
    struct node* next;
    struct node* child;
    char* symlinkTarget; // For symbolic links
} node;

// Function to create a new folder in the current directory
// void make_dir(node* currentFolder, char* command, char* currentPath);

// Function to create a new file in the current directory
// void touch(node* currentFolder, char* command, char* currentPath);

// Function to list files and folders in the current directory
void ls(node* currentFolder);

// Function to recursively list files and folders in the current directory
void lsrecursive(node* currentFolder, int indentCount);

// Function to edit the content of an existing file
void edit(node* currentFolder, char* command);

// Function to print the current directory's full path
void pwd(char* path);

// Function to change the current directory
node* cd(node* currentFolder, char* command, char** path, node *root);

// Function to move up to the parent directory
node* cdup(node* currentFolder, char** path);

// Function to free a node (and its children)
void freeNode(node* freeingNode);

// Function to remove a node (file or folder)
void removeNode(node* removingNode);

// Function to remove a file or folder
void rm(node* currentFolder, char* command);

// Function to move a node (file or folder) to another location
void mov(node* currentFolder, char* command);

// Function to count the total number of files in the entire directory tree
int countFiles(node* folder);

// // Function to save the directory structure to a file or compressed file
// void saveDirectory(node* folder, void* file);

// // Function to load the directory structure from a file or compressed file
// node* loadDirectory(gzFile gz, node* parent);

// Function to merge two directories, resolving conflicts interactively
void mergeDirectories(node* destFolder, node* srcFolder);

// Function to create a symbolic link to an existing file or folder
int createSymlink(node* currentFolder, char* sourcePath, char* linkName, node* root);

// Function to sort files and folders in the current directory by name or date
void sortDirectory(node* folder, const char* criterion);

// Function to compress the entire directory structure into a compressed file
void compressDirectory(node* folder, const char* filename);

// Function to decompress a file and restore the directory structure
node* decompressDirectory(const char* filename);

// Function to get a node
node* getNode(node *currentFolder, char* name, enum nodeType type);
node* getNodeTypeless(node *currentFolder, char* name);

// Function to read and display the contents of a file
void echo(node* currentFolder, char* fileName, node* root);

// Function to display coloful nodes
void displayNode(node* item);

char* getString() {
    size_t size = 10;
    char* str = (char*)malloc(size);
    size_t len = 0;
    int ch;

    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (len + 1 >= size) {
            size += 16;
            str = (char*)realloc(str, size);
        }
        str[len++] = ch;
    }
    str[len] = '\0';
    return str;
}

// Week 2: Count Total Files
int countFiles(node* folder) {
    if (!folder) return 0;

    int count = 0;
    if (folder->type == File) {
        count++;
    }

    node* currentNode = folder->child;
    while (currentNode) {
        count += countFiles(currentNode);
        currentNode = currentNode->next;
    }
    return count;
}

int countFolders(node* folder) {
    if (!folder) return 0;

    int count = 0;
    if (folder->type == Folder) {
        count++;
    }

    node* currentNode = folder->child;
    while (currentNode) {
        count += countFiles(currentNode);
        currentNode = currentNode->next;
    }
    return count;
}

void getRealPath(node* currentFolder, char* realPath) {
    // Temporary buffer to build the path
    char tempPath[1024] = "";
    node* folder = currentFolder;

    while (folder != NULL && strcmp(folder->name, "/") != 0) {
        // Prepend the current folder's name
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "/%s", folder->name);
        strcat(tempPath, buffer);

        // Move to the parent folder
        folder = folder->parent;
    }

    // Start from root if the current folder is the root node
    if (folder == NULL) {
        snprintf(realPath, 1024, "/");
    } else {
        // Reverse the constructed path to get the correct real path
        snprintf(realPath, 1024, ".");
        strncat(realPath, tempPath, 1024 - strlen(realPath) - 1);
    }
}

node* parsePath(node* currentFolder, char* path, node* root) {
    // Handle absolute path
    if (path[0] == '/') {
        currentFolder = root;
        path++; // Skip the initial '/'
    }

    char* token = strtok(path, "/");
    while (token != NULL) {
        if (strcmp(token, "..") == 0) {
            // Move to the parent directory
            if (currentFolder->parent) {
                currentFolder = currentFolder->parent;
            } else {
                printf("Already at the root directory.\n");
            }
        } else if (strcmp(token, ".") == 0) {
            // Stay in the current directory (do nothing)
        } else {
            // Use getNodeTypeless to find the child node by name
            node* nextFolder = getNodeTypeless(currentFolder, token);
            if (nextFolder) {
                currentFolder = nextFolder;
            } else {
                printf("Error: Directory or file '%s' not found.\n", token);
                return NULL;
            }
        }
        token = strtok(NULL, "/");
    }

    return currentFolder;
}


// Function to read and display the contents of a file
void echo(node* currentFolder, char* fileName, node* root) {
    // Find the node with the given file name in the current folder
    node* targetNode = getNodeTypeless(currentFolder, fileName);
    if (targetNode == NULL) {
        printf("Error: File '%s' not found.\n", fileName);
        return;
    }

    // If the node is a symlink, resolve it to its target
    if (targetNode->type == Symlink) {
        char* targetPath = targetNode->symlinkTarget;
        printf("Following symlink '%s' -> '%s'\n", fileName, targetPath);

        // Resolve the symlink path
        targetNode = parsePath(currentFolder, targetPath, root);
        if (targetNode == NULL) {
            printf("Error: Target of symlink '%s' not found.\n", fileName);
            return;
        }
    }

    // Ensure the resolved node is a file
    if (targetNode->type != File) {
        printf("Error: '%s' is not a file.\n", fileName);
        return;
    }

    // Construct the real file path
    char realPath[MAX_PATH_LENGTH];
    getRealPath(targetNode->parent, realPath);
    char fullPath[MAX_PATH_LENGTH];
    int n = snprintf(fullPath, sizeof(fullPath), "%s/%s", realPath, targetNode->name);

    // Check if the output was truncated
    if (n < 0 || n >= (int)sizeof(fullPath)) {
        fprintf(stderr, "Error: Path too long for file '%s'.\n", fileName);
        return;
    }

    // Open and read the file contents
    FILE* file = fopen(fullPath, "r");
    if (file == NULL) {
        printf("Error: Could not open file '%s'.\n", fullPath);
        return;
    }

    printf("Contents of '%s':\n", fullPath);
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        printf("%s", buffer);
    }

    fclose(file);
}


void saveDirectoryToFile(node* folder, FILE* file, int depth) {
    if (!folder) return;

    // Indentation for better readability
    for (int i = 0; i < depth; i++) fprintf(file, "  ");
    fprintf(file, "{\n");

    // Write folder/file properties
    for (int i = 0; i <= depth; i++) fprintf(file, "  ");
    fprintf(file, "\"type\": \"%s\",\n", folder->type == Folder ? "Folder" : (folder->type == File ? "File" : "Symlink"));

    for (int i = 0; i <= depth; i++) fprintf(file, "  ");
    fprintf(file, "\"name\": \"%s\",\n", folder->name);

    for (int i = 0; i <= depth; i++) fprintf(file, "  ");
    fprintf(file, "\"size\": %zu,\n", folder->size);

    for (int i = 0; i <= depth; i++) fprintf(file, "  ");
    fprintf(file, "\"date\": %ld", folder->date);

    if (folder->type == File && folder->content) {
        fprintf(file, ",\n");
        for (int i = 0; i <= depth; i++) fprintf(file, "  ");
        fprintf(file, "\"content\": \"%s\"", folder->content);
    }

    if (folder->type == Symlink) {
        fprintf(file, ",\n");
        for (int i = 0; i <= depth; i++) fprintf(file, "  ");
        fprintf(file, "\"symlinkTarget\": \"%s\"", folder->symlinkTarget);
    }

    // Children
    fprintf(file, ",\n");
    for (int i = 0; i <= depth; i++) fprintf(file, "  ");
    fprintf(file, "\"children\": [\n");

    node* current = folder->child;
    while (current) {
        saveDirectoryToFile(current, file, depth + 1);
        current = current->next;
        if (current) fprintf(file, ",\n");
    }

    // Closing children array
    fprintf(file, "\n");
    for (int i = 0; i <= depth; i++) fprintf(file, "  ");
    fprintf(file, "]\n");

    // Closing this node
    for (int i = 0; i < depth; i++) fprintf(file, "  ");
    fprintf(file, "}");
}

void saveDirectory(node* root, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error: Could not open file '%s' for saving.\n", filename);
        return;
    }

    saveDirectoryToFile(root, file, 0);
    fprintf(file, "\n"); // Final newline for cleanliness
    fclose(file);

    printf("Directory structure saved to '%s'.\n", filename);
}


node* loadDirectoryFromFile(FILE* file, node* parent) {
    char line[1024];
    node* firstChild = NULL;
    node* previousSibling = NULL;

    while (fgets(line, sizeof(line), file)) {
        // End of the current folder's children
        if (strstr(line, "]")) break;

        // Check for opening brace indicating a new node
        if (strstr(line, "{")) {
            // Create a new node
            node* newNode = malloc(sizeof(node));
            newNode->parent = parent;
            newNode->child = NULL;
            newNode->next = NULL;
            newNode->previous = previousSibling;
            newNode->symlinkTarget = NULL;
            newNode->numberOfItems = 0;

            // Parse node properties
            while (fgets(line, sizeof(line), file) && !strstr(line, "}")) {
                if (strstr(line, "\"type\":")) {
                    if (strstr(line, "Folder")) {
                        newNode->type = Folder;
                    } else if (strstr(line, "File")) {
                        newNode->type = File;
                    } else if (strstr(line, "Symlink")) {
                        newNode->type = Symlink;
                    }
                } else if (strstr(line, "\"name\":")) {
                    char name[256];
                    sscanf(line, " \"name\": \"%255[^\"]\"", name);
                    newNode->name = strdup(name);
                } else if (strstr(line, "\"size\":")) {
                    sscanf(line, " \"size\": %zu", &newNode->size);
                } else if (strstr(line, "\"date\":")) {
                    sscanf(line, " \"date\": %ld", &newNode->date);
                } else if (strstr(line, "\"symlinkTarget\":")) {
                    char target[256];
                    sscanf(line, " \"symlinkTarget\": \"%255[^\"]\"", target);
                    newNode->symlinkTarget = strdup(target);
                } else if (strstr(line, "\"content\":")) {
                    char content[1024];
                    sscanf(line, " \"content\": \"%1023[^\"]\"", content);
                    newNode->content = strdup(content);
                } else if (strstr(line, "\"children\":")) {
                    // Recursively load children
                    newNode->child = loadDirectoryFromFile(file, newNode);
                    newNode->numberOfItems = countFiles(newNode);// + countFolders(newNode); // Update count
                }
            }

            // Link sibling nodes properly
            if (!firstChild) {
                firstChild = newNode; // First child under this parent
            } else {
                previousSibling->next = newNode; // Link as a sibling
                newNode->previous = previousSibling; // Update previous pointer for the new node
            }
            previousSibling = newNode; // Update the last sibling pointer

            // Debug output to track structure
            printf("Loaded: %s (%s)\n", newNode->name,
                   (newNode->type == Folder ? "Folder" :
                   (newNode->type == File ? "File" : "Symlink")));
        }
    }

    return firstChild;
}

node* loadDirectory(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open file '%s' for loading.\n", filename);
        return NULL;
    }

    // node* root = malloc(sizeof(node));
    // root->type = Folder;
    // root->name = strdup("/");
    // root->parent = NULL;
    // root->child = NULL;
    // root->next = NULL;
    // root->previous = NULL;
    // root->symlinkTarget = NULL;
    // root->numberOfItems = 0;

    // Load the directory tree from the file
    node* loadedRoot = loadDirectoryFromFile(file, NULL);
    loadedRoot->numberOfItems = countFiles(loadedRoot);
    fclose(file);

    // Ensure the loaded root has the correct parent-child structure
    if (loadedRoot) {
        printf("Directory structure loaded from '%s'.\n", filename);
        return loadedRoot;
    } else {
        printf("Error: Failed to load directory structure from '%s'.\n", filename);
        return NULL;
    }
}


// // Week 2: Save Directory Structure
// // Function to save the directory to either a regular file or compressed file
// void saveDirectory(node* folder, void* file) {
//     if (!folder) return;

//     gzFile gz = (gzFile)file;

//     // Save folder/file details
//     if (gz) {
//         gzprintf(gz, "%d %s %ld %ld\n", folder->type, folder->name, folder->size, folder->date);
//     }

//     // Save file content if present
//     if (folder->type == File && folder->content) {
//         if (gz) {
//             gzprintf(gz, "CONTENT:%s\n", folder->content);
//         }
//     }

//     // Recursively save child nodes
//     node* currentNode = folder->child;
//     while (currentNode) {
//         saveDirectory(currentNode, file);
//         currentNode = currentNode->next;
//     }

//     // Mark the end of the folder
//     if (gz) {
//         gzprintf(gz, "END\n");
//     }
// }


// // Week 2: Load Directory Structure
// // Function to load the directory from either a regular file or compressed file
// node* loadDirectory(gzFile gz, node* parent) {
//     char line[1024];

//     while (gzgets(gz, line, sizeof(line))) {
//         if (strncmp(line, "END", 3) == 0) break;

//         node* newNode = malloc(sizeof(node));
//         newNode->parent = parent;
//         newNode->child = NULL;
//         newNode->next = NULL;
//         newNode->previous = NULL;
//         newNode->symlinkTarget = NULL;

//         // Parse node details
//         char nameBuffer[256];
//         sscanf(line, "%d %s %zu %ld", (int*)&newNode->type, nameBuffer, &newNode->size, &newNode->date);
//         newNode->name = strdup(nameBuffer);

//         // Load content if present
//         if (newNode->type == File && gzgets(gz, line, sizeof(line)) && strncmp(line, "CONTENT:", 8) == 0) {
//             newNode->content = strdup(line + 8);
//             newNode->content[strlen(newNode->content) - 1] = '\0';

//             // Create real file
//             char path[1024];
//             snprintf(path, sizeof(path), "%s/%s", parent->name, newNode->name);
//             FILE* file = fopen(path, "w");
//             if (file) {
//                 fprintf(file, "%s", newNode->content);
//                 fclose(file);
//             }
//         } else if (newNode->type == File) {
//             newNode->content = NULL;
//         }

//         // Create real folder if applicable
//         if (newNode->type == Folder) {
//             char path[1024];
//             snprintf(path, sizeof(path), "%s/%s", parent->name, newNode->name);
//             mkdir(path, 0755);
//         }

//         // Recursively load children
//         newNode->child = loadDirectory(gz, newNode);

//         // Add to parent's children
//         if (parent->child == NULL) {
//             parent->child = newNode;
//         } else {
//             node* last = parent->child;
//             while (last->next) last = last->next;
//             last->next = newNode;
//             newNode->previous = last;
//         }
//     }

//     return parent ? parent->child : NULL;
// }

// Week 3: Rename Node
void renameNode(node* currentNode, const char* newName) {
    if (!currentNode) {
        printf("Error: Node does not exist.\n");
        return;
    }

    // Check for conflicting names in the same directory
    node* sibling = currentNode->parent ? currentNode->parent->child : NULL;
    while (sibling) {
        if (sibling != currentNode && strcmp(sibling->name, newName) == 0) {
            printf("Error: A node with the name '%s' already exists in the current directory.\n", newName);
            return;
        }
        sibling = sibling->next;
    }

    // Free the old name and assign the new name
    free(currentNode->name);
    currentNode->name = strdup(newName);
    printf("Renamed to '%s'\n", currentNode->name);
}


// Week 3: Display Full Path
void displayFullPath(node* currentNode) {
    if (!currentNode) return;

    if (currentNode->parent) {
        displayFullPath(currentNode->parent);
    }
    printf("/%s", currentNode->name);
}

node* getNode(node *currentFolder, char* name, enum nodeType type) {

    if (currentFolder->child != NULL) {

        node *currentNode = currentFolder->child;

        while (currentNode->next != NULL) {

            if (strcmp(name, currentNode->name) == 0 && currentNode->type == type) {
                return currentNode;
            }
            currentNode = currentNode->next;
        }

        if (strcmp(name, currentNode->name) == 0 && currentNode->type == type) {
            return currentNode;
        } else return NULL;

    } else return NULL;
}

node* getNodeTypeless(node *currentFolder, char* name) {

    if (currentFolder->child != NULL) {

        node *currentNode = currentFolder->child;

        while (currentNode->next != NULL) {

            if (strcmp(name, currentNode->name) == 0) {
                return currentNode;
            }
            currentNode = currentNode->next;
        }

        if (strcmp(name, currentNode->name) == 0) {
            return currentNode;
        } else return NULL;

    } else return NULL;
}

void make_dir(node* currentFolder, char* command) {
    if (strtok(command, " ") != NULL) {
        char* folderName = strtok(NULL, " ");
        if (folderName != NULL) {
            // Check if the folder already exists in the virtual tree
            if (getNodeTypeless(currentFolder, folderName) == NULL) {
                // Create the folder in the virtual file system
                currentFolder->numberOfItems++;
                node* newFolder = (node*)malloc(sizeof(node));
                if (currentFolder->child == NULL) {
                    currentFolder->child = newFolder;
                    newFolder->previous = NULL;
                } else {
                    node* currentNode = currentFolder->child;
                    while (currentNode->next != NULL) {
                        currentNode = currentNode->next;
                    }
                    currentNode->next = newFolder;
                    newFolder->previous = currentNode;
                }

                char* newFolderName = strdup(folderName);
                newFolder->name = newFolderName;
                newFolder->type = Folder;
                newFolder->numberOfItems = 0;
                newFolder->size = 0;
                newFolder->date = time(NULL);
                newFolder->content = NULL;
                newFolder->parent = currentFolder;
                newFolder->next = NULL;
                newFolder->child = NULL;

                printf("Folder '%s' added to the virtual filesystem.\n", newFolder->name);

                // Get the real path and create the folder in the real file system
                char realPath[1024];
                getRealPath(currentFolder, realPath);
                char fullPath[1024];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", realPath, folderName);

                if (mkdir(fullPath, 0755) == 0) {
                    printf("Folder '%s' created in the real filesystem.\n", fullPath);
                } else {
                    perror("Error creating folder in the real filesystem");
                }
            } else {
                fprintf(stderr, "'%s' already exists in the current directory!\n", folderName);
            }
        }
    }
}

void touch(node* currentFolder, char* command, char* currentPath) {
    if (strtok(command, " ") != NULL) {
        char* fileName = strtok(NULL, " ");
        if (fileName != NULL) {
            if (getNodeTypeless(currentFolder, fileName) == NULL) {
                currentFolder->numberOfItems++;
                node* newFile = malloc(sizeof(node));
                // ... (Virtual tree addition remains unchanged)

                // Construct the real path
                char realPath[MAX_PATH_LENGTH];
                getRealPath(currentFolder, realPath);
                char fullPath[MAX_PATH_LENGTH];
                int n = snprintf(fullPath, sizeof(fullPath), "%s/%s", realPath, fileName);

                // Check for truncation
                if (n < 0 || n >= (int)sizeof(fullPath)) {
                    fprintf(stderr, "Error: Path too long for file '%s'.\n", fileName);
                    return;
                }

                FILE* file = fopen(fullPath, "w");
                if (file) {
                    fclose(file);
                    printf("File '%s' created in the real filesystem.\n", fullPath);
                } else {
                    printf("Error: Could not create file '%s'.\n", fullPath);
                }
            } else {
                fprintf(stderr, "'%s' already exists in the current directory!\n", fileName);
            }
        }
    }
}

void ls(node *currentFolder) {
    if (currentFolder->child == NULL) {
        printf("___Empty____\n");
        return;
    }

    node *currentNode = currentFolder->child;

    while (currentNode != NULL) {
        struct tm *date_time = localtime(&currentNode->date);
        char dateString[26];
        strftime(dateString, 26, "%d %b %H:%M", date_time);

        if (currentNode->type == Folder) {
            printf("%s%d items\t%s\t%s/%s\n", CYAN, currentNode->numberOfItems, dateString, currentNode->name, RESET);
        } else if (currentNode->type == File) {
            printf("%s%dB\t%s\t%s%s\n", YELLOW, (int)currentNode->size, dateString, currentNode->name, RESET);
        } else if (currentNode->type == Symlink) {
            printf("%s\t%s\t%s%s\n", BLUE, dateString, currentNode->name, RESET);
        }

        currentNode = currentNode->next;
    }
}


void lsrecursive(node *currentFolder, int indentCount) {
    if (currentFolder->child == NULL) {
        for (int i = 0; i < indentCount; ++i) {
            printf("\t");
        }
        if (indentCount != 0) {
            printf("└─");
        }
        printf("___Empty____\n");
    } else {
        const char* YELLOW = "\033[38;5;226m"; // Google Yellow for files
        const char* CYAN = "\033[36m";         // Cyan for folders
        const char* BLUE = "\033[38;5;33m";    // Google Blue for symlinks
        const char* RESET = "\033[0m";         // Reset color

        node *currentNode = currentFolder->child;

        while (currentNode != NULL) {
            for (int i = 0; i < indentCount; ++i) {
                printf("\t");
            }
            if (indentCount != 0) {
                printf("└─");
            }

            struct tm *date_time = localtime(&currentNode->date);
            char dateString[26];
            strftime(dateString, 26, "%d %b %H:%M", date_time);

            if (currentNode->type == Folder) {
                // Print folder with cyan color
                printf("%s%d items\t%s\t%s%s\n", CYAN, currentNode->numberOfItems, dateString, currentNode->name, RESET);
            } else if (currentNode->type == File) {
                // Print file with yellow color
                printf("%s%dB\t%s\t%s%s\n", YELLOW, (int)currentNode->size, dateString, currentNode->name, RESET);
            } else if (currentNode->type == Symlink) {
                // Print symlink with blue color
                printf("%s\t%s\t%s%s\n", BLUE, dateString, currentNode->name, RESET);
            }

            // Recursively call lsrecursive for child folders
            if (currentNode->type == Folder) {
                lsrecursive(currentNode, indentCount + 1);
            }

            currentNode = currentNode->next;
        }
    }
}

void edit(node* currentFolder, char* command) {
    if (strtok(command, " ") != NULL) {
        char* fileName = strtok(NULL, " ");
        if (fileName != NULL) {
            node* editingNode = getNode(currentFolder, fileName, File);
            if (editingNode) {
                printf("Enter new content for '%s':\n", fileName);
                char* content = getString();

                // Update memory
                if (editingNode->content) {
                    free(editingNode->content);
                }
                editingNode->content = strdup(content);
                editingNode->size = strlen(content);
                editingNode->date = time(NULL);

                // Write to the real file
                char path[1024];
                snprintf(path, sizeof(path), "%s/%s", currentFolder->name, fileName);
                FILE* file = fopen(path, "w");
                if (file) {
                    fprintf(file, "%s", content);
                    fclose(file);
                    printf("Content written to file '%s' in the real filesystem.\n", path);
                } else {
                    printf("Error: Could not write to file '%s'.\n", path);
                }

                free(content);
            } else {
                printf("File '%s' not found.\n", fileName);
            }
        }
    }
}

void clear() {
    #ifdef _WIN32
        system("cls"); // Windows-specific command to clear the screen
    #else
        system("clear"); // Unix/Linux/Mac command to clear the screen
    #endif
}


void pwd(char *path) {
    if (path && strlen(path) > 0) {
        // Print the path directly without trimming the last character
        printf("%s\n", path);
    } else {
        // Default to root if the path is not set or invalid
        printf("/\n");
    }
}

node* cd(node *currentFolder, char *command, char **path, node *root) {
    if (strtok(command, " ") != NULL) {
        char* targetPath = strtok(NULL, " ");
        if (targetPath != NULL) {
            // Check if the path is absolute
            if (targetPath[0] == '/') {
                // Navigate to root explicitly for absolute paths
                currentFolder = root;
                *path = realloc(*path, sizeof(char) * 2);
                strcpy(*path, "/");
                targetPath++; // Skip the initial '/'
            }

            // Tokenize the path and navigate step-by-step
            char* token = strtok(targetPath, "/");
            while (token != NULL) {
                if (strcmp(token, "..") == 0) {
                    // Navigate to the parent directory
                    if (currentFolder->parent != NULL) {
                        currentFolder = currentFolder->parent;
                        // Update the path
                        char* lastSlash = strrchr(*path, '/');
                        if (lastSlash && lastSlash != *path) {
                            *lastSlash = '\0';
                        } else {
                            strcpy(*path, "/");
                        }
                    } else {
                        printf("Already at the root directory.\n");
                    }
                } else if (strcmp(token, ".") == 0) {
                    // Stay in the current directory (no-op)
                } else {
                    // Navigate to a child directory
                    node* destinationFolder = getNode(currentFolder, token, Folder);
                    if (destinationFolder != NULL) {
                        currentFolder = destinationFolder;

                        // Update the path
                        size_t newPathLength = strlen(*path) + strlen(destinationFolder->name) + 2;
                        *path = realloc(*path, sizeof(char) * newPathLength);
                        if (strcmp(*path, "/") == 0) {
                            strcat(*path, token);
                        } else {
                            strcat(strcat(*path, "/"), token);
                        }
                    } else {
                        fprintf(stderr, "There is no '%s' folder in the current directory!\n", token);
                        return currentFolder;
                    }
                }

                token = strtok(NULL, "/");
            }
        } else {
            printf("Error: No path provided.\n");
        }
    }
    return currentFolder;
}

node* cdup(node *currentFolder, char **path) {

    size_t newPathLength = strlen(*path) - strlen(currentFolder->name);

    while (currentFolder->previous != NULL) {
        currentFolder = currentFolder->previous;
    }
    if (currentFolder->parent != NULL ) {

        *path = (char *) realloc(*path, sizeof(char)* newPathLength);
        (*path)[newPathLength-1] = '\0';

        currentFolder = currentFolder->parent;
        return currentFolder;
    } else {
        return currentFolder;
    }
}

void freeNode(node *freeingNode) {

    if (freeingNode->child != NULL) {

        node* currentNode = freeingNode->child;

        while (currentNode->next != NULL) {
            node* nextNode = currentNode->next;
            freeNode(currentNode);
            currentNode = nextNode;
        }
        freeNode(currentNode);
    }
    free(freeingNode->name);
    free(freeingNode->content);
    free(freeingNode);

}

void removeNode(node *removingNode) {
    if (removingNode->parent != NULL){
        if (removingNode->next != NULL) {
            removingNode->next->parent = removingNode->parent;
            removingNode->parent->child = removingNode->next;
            removingNode->next->previous = NULL;
        } else {
            removingNode->parent->child = NULL;
        }
    } else {
        if (removingNode->next != NULL) {
            removingNode->previous->next = removingNode->next;
            removingNode->next->previous  = removingNode->previous;
        } else {
            removingNode->previous->next = NULL;
        }
    }
}

void rm(node* currentFolder, char* command) {
    if (strtok(command, " ") != NULL) {
        char* nodeName = strtok(NULL, " ");
        if (nodeName != NULL) {
            node* removingNode = getNodeTypeless(currentFolder, nodeName);
            if (removingNode) {
                printf("Do you really want to remove '%s' and its content? (y/n)\n", nodeName);
                char* answer = getString();
                if (strcmp(answer, "y") == 0) {
                    // Remove from memory
                    currentFolder->numberOfItems--;
                    removeNode(removingNode);
                    freeNode(removingNode);

                    // Remove from real filesystem
                    char path[1024];
                    snprintf(path, sizeof(path), "%s/%s", currentFolder->name, nodeName);
                    if (removingNode->type == Folder) {
                        if (rmdir(path) == 0) {
                            printf("Folder '%s' removed from the real filesystem.\n", path);
                        } else {
                            perror("Error removing folder from the real filesystem");
                        }
                    } else if (removingNode->type == File) {
                        if (remove(path) == 0) {
                            printf("File '%s' removed from the real filesystem.\n", path);
                        } else {
                            perror("Error removing file from the real filesystem");
                        }
                    }
                }
                free(answer);
            } else {
                printf("Node '%s' not found.\n", nodeName);
            }
        }
    }
}


void moveNode(node *movingNode, node *destinationFolder) {

    if (destinationFolder->child == NULL) {
        destinationFolder->child = movingNode;
        movingNode->previous = NULL;
        movingNode->parent = destinationFolder;
        movingNode->next = NULL;
    } else {

        node *currentNode = destinationFolder->child;
        while (currentNode->next != NULL) {
            currentNode = currentNode->next;
        }

        currentNode->next = movingNode;
        movingNode->previous = currentNode;
        movingNode->parent = NULL;
        movingNode->next = NULL;
    }
    destinationFolder->numberOfItems++;
}

void mov(node *currentFolder, char *command) {

    char* nodeName;
    char* destinationName;

    if (strtok(command, " ") != NULL) {
        nodeName = strtok(NULL, " ");
        if (nodeName != NULL) {
            destinationName = strtok(NULL, " ");
            if (destinationName != NULL) {
                if (strtok(NULL, " ")) {
                    return;
                } else {

                    node* movingNode = getNodeTypeless(currentFolder, nodeName);
                    node* destinationFolder = getNode(currentFolder, destinationName, Folder);

                    if (destinationFolder != NULL && movingNode != NULL && destinationFolder != movingNode) {

                        removeNode(movingNode);
                        moveNode(movingNode, destinationFolder);
                    } else {
                        fprintf(stderr, "Something you made wrong!\n");
                    }
                }
            }
        }
    }
}

// Helper functions for comparing and swapping nodes (used for sorting)
int compareNodesByName(const void* a, const void* b) {
    node* nodeA = *(node**)a;
    node* nodeB = *(node**)b;
    return strcmp(nodeA->name, nodeB->name);
}

int compareNodesByDate(const void* a, const void* b) {
    node* nodeA = *(node**)a;
    node* nodeB = *(node**)b;
    return difftime(nodeA->date, nodeB->date);
}

void sortDirectory(node* folder, const char* criterion) {
    if (!folder || folder->child == NULL) return;

    // Count the number of child nodes
    int count = 0;
    node* current = folder->child;
    while (current) {
        count++;
        current = current->next;
    }

    // Populate an array of child nodes
    node** nodesArray = malloc(count * sizeof(node*));
    current = folder->child;
    for (int i = 0; i < count; i++) {
        nodesArray[i] = current;
        current = current->next;
    }

    // Sort the array based on the criterion
    if (strcmp(criterion, "name") == 0) {
        qsort(nodesArray, count, sizeof(node*), compareNodesByName);
    } else if (strcmp(criterion, "date") == 0) {
        qsort(nodesArray, count, sizeof(node*), compareNodesByDate);
    }

    // Re-link the sorted nodes back into the tree
    folder->child = nodesArray[0];
    folder->child->previous = NULL;
    for (int i = 0; i < count - 1; i++) {
        nodesArray[i]->next = nodesArray[i + 1];
        nodesArray[i + 1]->previous = nodesArray[i];
    }
    nodesArray[count - 1]->next = NULL;

    free(nodesArray);
    printf("Directory sorted by %s.\n", criterion);
}

// Function to merge two directories, resolving any conflicts interactively
void mergeDirectories(node* destFolder, node* srcFolder) {
    if (!destFolder || !srcFolder || srcFolder->type != Folder || destFolder->type != Folder) return;

    node* current = srcFolder->child;
    while (current) {
        node* next = current->next;
        int choice;
        // Check for conflicts (same name)
        node* existing = getNodeTypeless(destFolder, current->name);
        if (existing) {
            printf("Conflict detected: %s already exists. Choose an option:\n", current->name);
            printf("1. Skip\n2. Rename\n3. Overwrite\n");
            
            // Declare and initialize the choice variable
            scanf("%d", &choice);
            getchar(); // Consume the newline character

            if (choice == 1) {
                // Skip the conflicting file/folder
                printf("Skipping %s\n", current->name);
            } else if (choice == 2) {
                // Rename the new file/folder
                char newName[256];
                printf("Enter a new name for %s: ", current->name);
                fgets(newName, sizeof(newName), stdin);
                newName[strcspn(newName, "\n")] = '\0'; // Remove newline
                free(current->name);
                current->name = strdup(newName);
                printf("Renamed to %s\n", current->name);
            } else if (choice == 3) {
                // Overwrite the existing file/folder
                printf("Overwriting %s\n", current->name);
                removeNode(existing); // Remove the existing node
            } else {
                // Handle invalid input
                printf("Invalid choice. Skipping %s.\n", current->name);
                return;
            }
        }

        // Move the current node to the destination folder
        if (!existing || choice == 3) {
            current->parent = destFolder;
            if (destFolder->child == NULL) {
                destFolder->child = current;
                current->previous = NULL;
            } else {
                node* last = destFolder->child;
                while (last->next) last = last->next;
                last->next = current;
                current->previous = last;
            }
        }
        current = next;
    }
    printf("Directories merged.\n");
}

// Handle symbolic links
int createSymlink(node* currentFolder, char* sourcePath, char* linkName, node* root) {
    // Use parsePath to find the source node
    node* sourceNode = parsePath(currentFolder, sourcePath, root);
    if (sourceNode == NULL) {
        printf("Error: Source '%s' not found.\n", sourcePath);
        return -1;
    }

    // Check if a node with the linkName already exists in the current folder
    node* existingNode = getNodeTypeless(currentFolder, linkName); // Use getNodeTypeless
    if (existingNode != NULL) {
        printf("Error: A node with the name '%s' already exists.\n", linkName);
        return -1;
    }

    // Create the new symlink node
    node* newLink = malloc(sizeof(node));
    if (!newLink) {
        printf("Error: Memory allocation failed.\n");
        return -1;
    }

    newLink->type = Symlink;
    newLink->name = strdup(linkName);
    newLink->symlinkTarget = strdup(sourcePath); // Store the target path as a string
    newLink->size = 0; // Size for symlinks can be 0 as it points to another node
    newLink->date = time(NULL); // Set current time as the creation date
    newLink->child = NULL;
    newLink->next = NULL;
    newLink->parent = currentFolder;

    // Add the new symlink to the current folder's child list
    if (currentFolder->child == NULL) {
        currentFolder->child = newLink;
    } else {
        node* lastChild = currentFolder->child;
        while (lastChild->next != NULL) {
            lastChild = lastChild->next;
        }
        lastChild->next = newLink;
    }

    printf("Symbolic link '%s' -> '%s' created.\n", linkName, sourcePath);
    return 0;
}

// Compression (using zlib)
// void compressDirectory(node* folder, const char* filename) {
//     if (!folder) return;

//     FILE* file = fopen(filename, "wb");
//     if (!file) {
//         printf("Error: Unable to create compressed file '%s'.\n", filename);
//         return;
//     }

//     gzFile gzfile = gzdopen(fileno(file), "wb");
//     if (!gzfile) {
//         fclose(file);
//         printf("Error: Unable to open compressed stream for '%s'.\n", filename);
//         return;
//     }

//     // Save the directory structure into the compressed stream
//     saveDirectory(folder, gzfile);
//     gzclose(gzfile);

//     printf("Directory compressed to '%s'.\n", filename);
// }


// // Decompression (using zlib)
// node* decompressDirectory(const char* filename) {
//     FILE* file = fopen(filename, "rb");
//     if (!file) {
//         printf("Error: Unable to open compressed file '%s'.\n", filename);
//         return NULL;
//     }

//     gzFile gzfile = gzdopen(fileno(file), "rb");
//     if (!gzfile) {
//         fclose(file);
//         printf("Error: Unable to open compressed stream for '%s'.\n", filename);
//         return NULL;
//     }

//     // Create the root node
//     node* folder = malloc(sizeof(node));
//     folder->type = Folder;
//     folder->name = strdup("/");
//     folder->parent = NULL;
//     folder->child = NULL;
//     folder->next = NULL;
//     folder->previous = NULL;
//     folder->symlinkTarget = NULL;

//     // Load the directory structure from the compressed stream
//     folder->child = loadDirectory(gzfile, folder);
//     gzclose(gzfile);

//     printf("Directory decompressed from '%s'.\n", filename);
//     return folder;
// }

void displayPrompt(const char* path) {
    printf("┌──[%s%s%s]\n└─%s>%s ", BLUE, path, RESET, GREEN, RESET);
}

// [Not Working] 
// // Function to enable raw mode for real-time input
// void enableRawMode() {
//     struct termios raw;
//     tcgetattr(STDIN_FILENO, &raw);
//     raw.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
//     tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
// }

// // Function to restore the terminal to default mode
// void disableRawMode() {
//     struct termios raw;
//     tcgetattr(STDIN_FILENO, &raw);
//     raw.c_lflag |= (ICANON | ECHO); // Enable canonical mode and echo
//     tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
// }

// // [Not Working] Function to read input in real-time with yellow color
// char* getRealTimeInput() {
//     enableRawMode();

//     char* input = malloc(1024);
//     size_t length = 0;

//     printf("%s", YELLOW); // Set input color to yellow

//     char c;
//     while (read(STDIN_FILENO, &c, 1) == 1 && c != '\n') {
//         if (c == 127) { // Handle backspace
//             if (length > 0) {
//                 length--;
//                 printf("\b \b"); // Move cursor back and overwrite character
//             }
//         } else {
//             input[length++] = c;
//             printf("%c", c); // Print each character as it's typed
//         }
//     }

//     input[length] = '\0'; // Null-terminate the input
//     printf("%s\n", RESET); // Reset color and print newline

//     disableRawMode();

//     return input;
// }

void displayNode(node* item) {
    if (item->type == File) {
        printf("%s%s%s\n", YELLOW, item->name, RESET);
    } else if (item->type == Folder) {
        printf("%s%s/%s\n", CYAN, item->name, RESET);
    } else if (item->type == Symlink) {
        printf("%s%s@%s\n", BLUE, item->name, RESET);
    }
}


int main() {

    node *root = (node*) malloc(sizeof(node));

    char *rootName = (char *) malloc(sizeof(char)*2);
    strcpy(rootName, "/");
    root->type = Folder;
    root->name = rootName;
    root->numberOfItems = 0;
    root->size = 0;
    root->date = time(NULL);
    root->content =NULL;
    root->previous = NULL;
    root->parent = NULL;
    root->next = NULL;
    root->child = NULL;

    node *currentFolder = root;

    char *path = (char *) malloc(sizeof(char)*2);
    strcpy(path, "/");

     // Path to the real file system
    char currentPath[2048] = ".";
    getRealPath(currentFolder, currentPath); // Initialize to real root


    while (1) {

        displayPrompt(path);
        char *command = getString();

        // [Not Working] 
        // char *command = getRealTimeInput();

        if (strncmp(command, "mkdir", 5) == 0) {
            make_dir(currentFolder, command); // Pass the full path
        } else if (strncmp(command, "touch", 5) == 0) {
            touch(currentFolder, command, currentPath); // Pass the full path
        } else if (strcmp(command, "ls") == 0) {
            ls(currentFolder);
        } else if (strcmp(command, "lsrecursive") == 0) {
            lsrecursive(currentFolder, 0);
        } else if (strncmp(command, "edit", 4) == 0 ) {
            edit(currentFolder, command);
        } else if (strncmp(command, "clear", 5) == 0) {
            clear(); // Call the clear function
        } else if (strcmp(command, "pwd") == 0) {
            pwd(path);
        } else if (strcmp(command, "cdup") == 0) {
            currentFolder = cdup(currentFolder, &path);
        } else if (strncmp(command, "cd", 2) == 0) {
            currentFolder = cd(currentFolder, command, &path, root);
        } else if (strncmp(command, "rm", 2) == 0) {
            rm(currentFolder, command);
        } else if (strncmp(command, "mov", 3) == 0) {
            mov(currentFolder, command);
        } else if (strncmp(command, "echo", 4) == 0) {
            char* fileName = strtok(command + 5, " ");
            if (fileName) {
                echo(currentFolder, fileName, root);
            } else {
                printf("Error: No file name provided. Usage: echo <fileName>\n");
            }
        } else if (strcmp(command, "count") == 0) {
            int fileCount = countFiles(currentFolder);
            int folderCount = countFolders(currentFolder);
            printf("Files: %d\nFolders: %d\n", fileCount, folderCount);
        } else if (strcmp(command, "countFiles") == 0) {
            printf("Total files: %d\n", countFiles(root));
        } else if (strcmp(command, "countFolders") == 0) {
            printf("Total folders: %d\n", countFolders(root));
        } else if (strncmp(command, "save", 4) == 0) {
            char* filename = strtok(command + 5, " ");
            if (filename) {
                saveDirectory(root, filename);  // Save the entire directory tree to the specified file
            } else {
                printf("Error: No filename provided for saving.\n");
            }

            // char* filename = strtok(command + 5, " ");
            // if (filename) {
            //     // Use gz compression for saving the directory structure
            //     FILE* file = fopen(filename, "wb");
            //     if (file) {
            //         gzFile gzfile = gzdopen(fileno(file), "wb");
            //         if (gzfile) {
            //             saveDirectory(root, gzfile);
            //             gzclose(gzfile);
            //             printf("Directory structure compressed and saved to '%s'.\n", filename);
            //         } else {
            //             fclose(file);
            //             printf("Error: Unable to open compressed stream for '%s'.\n", filename);
            //         }
            //     } else {
            //         printf("Error: Could not create file '%s' for saving.\n", filename);
            //     }
            // } else {
            //     printf("Error: No filename provided for saving.\n");
            // }
        } else if (strncmp(command, "load", 4) == 0) {
            char* filename = strtok(command + 5, " ");
            if (filename) {
                node* loadedRoot = loadDirectory(filename);  // Load the directory tree from the specified file
                if (loadedRoot) {
                    freeNode(root);      // Free the current directory tree in memory
                    root = loadedRoot;   // Replace with the loaded directory tree
                    currentFolder = root; // Reset current folder to the root of the loaded tree
                    free(path);
                    path = strdup("/");  // Reset the path to the root
                }
            } else {
                printf("Error: No filename provided for loading.\n");
            }
            // char* filename = strtok(command + 5, " ");
            // if (filename) {
            //     // Use gz decompression for loading the directory structure
            //     FILE* file = fopen(filename, "rb");
            //     if (file) {
            //         gzFile gzfile = gzdopen(fileno(file), "rb");
            //         if (gzfile) {
            //             freeNode(root); // Free the current directory tree in memory
            //             root = (node*)malloc(sizeof(node));
            //             root->type = Folder;
            //             root->name = strdup("/");
            //             root->child = loadDirectory(gzfile, root);
            //             gzclose(gzfile);
            //             currentFolder = root; // Reset current folder to root
            //             printf("Directory structure decompressed and loaded from '%s'.\n", filename);
            //         } else {
            //             fclose(file);
            //             printf("Error: Unable to open compressed stream for '%s'.\n", filename);
            //         }
            //     } else {
            //         printf("Error: Could not open file '%s' for loading.\n", filename);
            //     }
            // } else {
            //     printf("Error: No filename provided for loading.\n");
            // }
        } else if (strncmp(command, "merge", 5) == 0) {
            char* srcName = strtok(command + 6, " ");
            char* destName = strtok(NULL, " ");
            if (srcName && destName) {
                node* srcFolder = getNode(currentFolder, srcName, Folder);
                node* destFolder = getNode(currentFolder, destName, Folder);
                if (srcFolder && destFolder) {
                    mergeDirectories(destFolder, srcFolder);
                } else {
                    printf("Error: One or both directories not found.\n");
                }
            }
        } else if (strncmp(command, "symlink", 7) == 0) {
            char* sourcePath = strtok(command + 8, " ");
            char* linkName = strtok(NULL, " ");
            if (sourcePath && linkName) {
                createSymlink(currentFolder, sourcePath, linkName, root);
            } else {
                printf("Error: Invalid arguments. Usage: symlink <source> <linkName>\n");
            }
        } else if (strncmp(command, "sortBy", 6) == 0) {
            char* criterion = strtok(command + 7, " ");
            if (criterion && (strcmp(criterion, "name") == 0 || strcmp(criterion, "date") == 0)) {
                sortDirectory(currentFolder, criterion);
            } else {
                printf("Error: Sort criterion must be 'name' or 'date'.\n");
            }
        } else if (strncmp(command, "compress", 8) == 0) {
            // char* filename = strtok(command + 9, " ");
            // if (filename) {
            //     compressDirectory(root, filename);
            // }
        } else if (strncmp(command, "decompress", 10) == 0) {
            // char* filename = strtok(command + 11, " ");
            // if (filename) {
            //     node* decompressedRoot = decompressDirectory(filename);
            //     if (decompressedRoot) {
            //         freeNode(root);
            //         root = decompressedRoot;
            //         currentFolder = root;
            //     }
            // }
        } else if (strncmp(command, "rename", 6) == 0) {
            char* oldName = strtok(command + 7, " ");
    char* newName = strtok(NULL, " ");
    if (oldName && newName) {
            // Locate the node with the old name
            node* targetNode = getNodeTypeless(currentFolder, oldName);
                if (targetNode) {
                    renameNode(targetNode, newName); // Call the updated rename function
                } else {
                    printf("Error: Node '%s' not found in the current directory.\n", oldName);
                }
            } else {
                printf("Error: Insufficient arguments. Usage: rename <oldName> <newName>\n");
            }
        } else if (strncmp(command, "fullpath", 8) == 0) {
            displayFullPath(currentFolder);
            printf("\n");
        } else if (strcmp(command, "exit") == 0){
            free(command);
            freeNode(root);
            free(path);
            break;
        } else {
            printf("Unknown command: %s\n", command);
        }

        free(command);
    }

    return 0;
}

//==11180== HEAP SUMMARY:
//==11180==     in use at exit: 0 bytes in 0 blocks
//==11180==   total heap usage: 182 allocs, 182 frees, 10,723 bytes allocated
//==11180==
//==11180== All heap blocks were freed -- no leaks are possible
//==11180==
//==11180== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
//==11180== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
