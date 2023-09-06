#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define CONFIG_FILE "/.config/pm/config"
#define PROJECT_LIST_FILE "/.config/pm/projects"
#define MAX_PATH_LENGTH 128
#define MAX_COMMAND_LENGTH 512

static char rsync_path[MAX_PATH_LENGTH];
static struct stat st = {0};

struct ProjectInfo {
  char name[MAX_PATH_LENGTH];
  time_t last_rsync_time;
  char description[MAX_PATH_LENGTH];
};

static const char *args[][4] = {
    {"new", "n", "<folder-name> [description]",
     "Create a new project and add it to the list."},
    {"rsync", "r", "<folder> || all (empty)",
     "Rsync a folder from source to destination."},
    {"remove", "rm", "<project-name>", "Remove a project from the list."},
    {"list", "ls", "", "List all projects."},
    {"help", "h", "", "Show this help message."},
    {"open", "o", "<project-name>", "Open a project."},
};

FILE *open_file(const char *filename);
void help();
void new_project(const char *folder_name, const char *description);
void open_project(const char *folder_name);
void set_rsync_path();
void rsync_folder(const char *folder_name);
int match_args(char *argv[]);
void remove_project(const char *folder_name);
void list_projects();

int main(int argc, char *argv[]) {
  if (argc < 2) {
    help();
    return 0;
  }

  int match = match_args(argv);

  switch (match) {
  case 0: // new
    if (argc == 3) {
      new_project(argv[2], "");
    } else if (argc == 4) {
      new_project(argv[2], argv[3]);
    } else {
      fprintf(stderr, "Invalid number of arguments for 'new' command.\n");
    }
    break;
  case 1: // rsync
    set_rsync_path();
    rsync_folder(argv[2]);
    break;
  case 2: // remove
    remove_project(argv[2]);
    break;
  case 3: // list
    list_projects();
    break;
  case 4: // help
    help();
    break;
  case 5: // open
    open_project(argv[2]);
    break;
  default:
    help();
    break;
  }

  return 0;
}

void help() {
  printf("\033[1;36mUsage:\033[0m \n");
  for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); i++) {
    printf("\033[1m\t%s\033[0m [\033[1;34m%s\033[0m] \033[1;32m%s\033[0m "
           "\033[1m%s\033[0m\n",
           args[i][0], args[i][1], args[i][2], args[i][3]);
  }
}

void new_project(const char *folder_name, const char *description) {
  char path[MAX_PATH_LENGTH];
  sprintf(path, "%s/%s", getenv("HOME"), PROJECT_LIST_FILE);
  FILE *file = open_file(path);

  while (fscanf(file, "%s", path) != EOF) {
    if (strcmp(path, folder_name) == 0) {
      printf("\033[1;31mProject already exists\033[0m\n");
      fclose(file);
      return;
    }
  }

  fprintf(file, "%s/%s\n", getcwd(NULL, 0), folder_name);
  fclose(file);

  if (stat(folder_name, &st) == -1)
    mkdir(folder_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  struct ProjectInfo project;
  strncpy(project.name, folder_name, sizeof(project.name));
  project.last_rsync_time = time(NULL);
  strncpy(project.description, description, sizeof(project.description));

  sprintf(path, "%s/%s.dat", getenv("HOME"), PROJECT_LIST_FILE);
  FILE *bin_file = fopen(path, "ab");
  if (bin_file != NULL) {
    fwrite(&project, sizeof(struct ProjectInfo), 1, bin_file);
    fclose(bin_file);
  }

  printf("\033[1;32mProject '%s' created successfully\033[0m\n", folder_name);
}

void open_project(const char *folder_name) {
  if (folder_name == NULL) {
    list_projects();
    return;
  }
  char path[MAX_PATH_LENGTH];
  sprintf(path, "%s/%s.dat", getenv("HOME"), PROJECT_LIST_FILE);
  FILE *bin_file = open_file(path);

  struct ProjectInfo project_list[MAX_PATH_LENGTH];
  int num_projects = 0;

  while (fread(&project_list[num_projects], sizeof(struct ProjectInfo), 1,
               bin_file) == 1) {
    num_projects++;
  }
  fclose(bin_file);

  int found = 0;
  char project_path[MAX_PATH_LENGTH * 2];
  for (int i = 0; i < num_projects; i++) {
    if (strcmp(project_list[i].name, folder_name) == 0) {
      found = 1;
      char editor[MAX_PATH_LENGTH];
      char command[MAX_COMMAND_LENGTH * 2];

      sprintf(project_path, "%s/%s", getcwd(NULL, 0), project_list[i].name);

      sprintf(path, "%s/%s", getenv("HOME"), CONFIG_FILE);
      FILE *config_file = open_file(path);
      char line[MAX_PATH_LENGTH];

      while (fgets(line, sizeof(line), config_file) != NULL) {
        if (strncmp(line, "EDITOR=", 7) == 0) {
          char *value = line + 7;
          size_t len = strlen(value);
          if (value[len - 1] == '\n') {
            value[len - 1] = '\0';
          }
          strncpy(editor, value, sizeof(editor));
          break;
        }
      }
      fclose(config_file);

      if (strlen(editor) == 0) {
        sprintf(command, "xdg-open \"%s\"", project_path);
      } else {
        sprintf(command, "%s \"%s\"", editor, project_path);
      }

      system(command);
      break;
    }
  }

  if (!found) {
    printf("\033[1;31mProject '%s' not found in the list\033[0m\n",
           folder_name);
  }
}

void set_rsync_path() {
  char path[MAX_PATH_LENGTH];
  sprintf(path, "%s/%s", getenv("HOME"), PROJECT_LIST_FILE);
  FILE *file = open_file(path);
  fscanf(file, "%s", rsync_path + strlen("RSYNC_DEST="));
  fclose(file);
}

void rsync_folder(const char *folder_name) {
  char path[MAX_PATH_LENGTH];
  sprintf(path, "%s/%s", getenv("HOME"), PROJECT_LIST_FILE);
  FILE *file = open_file(path);

  while (fscanf(file, "%s", path) != EOF) {
    if (folder_name == NULL || strcmp(path, folder_name) == 0) {
      printf("Syncing %s\n", path);
      char command[MAX_COMMAND_LENGTH];
      sprintf(command, "rsync -a --filter=':- .gitignore'  %s %s >> /dev/null 2>&1", path,
              rsync_path);
      system(command);
    }
  }

  fclose(file);
}

void remove_project(const char *folder_name) {
  char path[MAX_PATH_LENGTH];
  sprintf(path, "%s/%s.dat", getenv("HOME"), PROJECT_LIST_FILE);
  FILE *bin_file = open_file(path);

  struct ProjectInfo project_list[MAX_PATH_LENGTH];
  int num_projects = 0;

  while (fread(&project_list[num_projects], sizeof(struct ProjectInfo), 1,
               bin_file) == 1) {
    num_projects++;
  }
  fclose(bin_file);

  int removed = 0;

  for (int i = 0; i < num_projects; i++) {
    if (strcmp(project_list[i].name, folder_name) == 0) {
      removed = 1;
      for (int j = i; j < num_projects - 1; j++) {
        project_list[j] = project_list[j + 1];
      }
      num_projects--;
      break;
    }
  }

  if (removed) {
    FILE *bin_file = fopen(path, "wb");
    if (bin_file != NULL) {
      for (int i = 0; i < num_projects; i++) {
        fwrite(&project_list[i], sizeof(struct ProjectInfo), 1, bin_file);
      }
      fclose(bin_file);
    }

    printf("\033[1;32mProject '%s' removed successfully\033[0m\n", folder_name);
  } else {
    printf("\033[1;31mProject '%s' not found in the list\033[0m\n",
           folder_name);
  }
}

void list_projects() {
  char path[MAX_PATH_LENGTH];
  sprintf(path, "%s/%s.dat", getenv("HOME"), PROJECT_LIST_FILE);
  FILE *bin_file = open_file(path);

  printf("\033[1;36m%-20s\t\033[1;36m%-20s\t%-20s\033[0m\n", "Project Name",
         "Timestamp", "Description");

  if (bin_file != NULL) {
    struct ProjectInfo project;
    while (fread(&project, sizeof(struct ProjectInfo), 1, bin_file) == 1) {
      struct tm *tm_info = localtime(&project.last_rsync_time);
      char timestamp[MAX_PATH_LENGTH];
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

      printf("\033[1m%-20s\t\033[0m%-20s\t%-20s\n", project.name, timestamp,
             project.description);
    }
    fclose(bin_file);
  }
}

int match_args(char *argv[]) {
  for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); i++) {
    if (strcmp(argv[1], args[i][0]) == 0 || strcmp(argv[1], args[i][1]) == 0) {
      return i;
    }
  }
  return -1;
}

FILE *open_file(const char *filename) {
  FILE *file = fopen(filename, "a+");
  if (file == NULL) {
    fprintf(stderr, "File %s does not exist\n", filename);
    exit(1);
  }
  return file;
}
