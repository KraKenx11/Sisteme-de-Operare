#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

typedef struct Report{
    int report_id;
    char name[50];
    float latitude;
    float longitude;
    char issue_category[30];
    int severity;
    time_t timestamp;
    char description_text[104];
}Report;

//Functie pentru conversie a permisiunilor
void mode_to_string(mode_t mode, char *str){
    strcpy(str,"---------");
    if(mode & S_IRUSR) str[0] = 'r';
    if(mode & S_IWUSR) str[1] = 'w';
    if(mode & S_IXUSR) str[2] = 'x';
    if(mode & S_IRGRP) str[3] = 'r';
    if(mode & S_IWGRP) str[4] = 'w';
    if(mode & S_IXGRP) str[5] = 'x';
    if(mode & S_IROTH) str[6] = 'r';
    if(mode & S_IWOTH) str[7] = 'w';
    if(mode & S_IXOTH) str[8] = 'x';
}

//Functie pentru log-uri
void log_action(const char *district, const char *role, const char *user, const char *action) {
    char path[256];
    sprintf(path, "%s/logged_district", district);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        char buf[512];
        int len = sprintf(buf, "%ld | %s | %s | %s\n", time(NULL), role, user, action);
        write(fd, buf, len);
        close(fd);
        chmod(path, 0644);
    }
}

//Verificare permisiuni inainte de actiune 
int has_permission(const char *path, const char *role, mode_t manager_bit, mode_t inspector_bit) {
    struct stat st;
    if (stat(path, &st) < 0) return 1; //Daca nu exista, il cream

    if (strcmp(role, "manager") == 0) {
        return (st.st_mode & manager_bit) != 0;
    } else if (strcmp(role, "inspector") == 0) {
        return (st.st_mode & inspector_bit) != 0;
    }
    return 0;
}

void update_symlink(const char *district) {
    char link_name[256], target[256];
    sprintf(link_name, "active_reports-%s", district);
    sprintf(target, "%s/reports.dat", district);
    unlink(link_name);
    symlink(target, link_name);
}

// --- LOGICA DE FILTRARE (AI ASSISTED) ---

int parse_condition(const char *input, char *field, char *op, char *value) {
    char temp[256];
    strncpy(temp, input, 255);
    char *f = strtok(temp, ":");
    char *o = strtok(NULL, ":");
    char *v = strtok(NULL, ":");
    if (!f || !o || !v) return 0;
    strcpy(field, f); strcpy(op, o); strcpy(value, v);
    return 1;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity == val;
        if (strcmp(op, ">=") == 0) return r->severity >= val;
        if (strcmp(op, "<=") == 0) return r->severity <= val;
        if (strcmp(op, ">") == 0) return r->severity > val;
        if (strcmp(op, "<") == 0) return r->severity < val;
    } else if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->issue_category, value) == 0;
    } else if (strcmp(field, "inspector") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->name, value) == 0;
    }
    return 0;
}

void remove_report(const char *path, int target_id) {
    int fd = open(path, O_RDWR);
    if (fd < 0) return;

    Report r;
    off_t write_pos = 0;
    int found = 0;

    //Cautam raportul
    while (read(fd, &r, sizeof(Report)) > 0) {
        if (r.report_id == target_id) {
            found = 1;
            break;
        }
        write_pos += sizeof(Report);
    }

    if (found) {
        Report next_r;
        //Shiftam tot ce urmeaza dupa randul gasit
        while (read(fd, &next_r, sizeof(Report)) > 0) {
            off_t current_read_pos = lseek(fd, 0, SEEK_CUR); //Salvam unde am ajuns cu citirea
            
            lseek(fd, write_pos, SEEK_SET); //Mergem la poziaia unde trebuie sa scriem
            write(fd, &next_r, sizeof(Report));
            
            write_pos += sizeof(Report); //Actualizam pozitia de scriere pentru urmatoarea tura
            lseek(fd, current_read_pos, SEEK_SET); //Revenim pentru a citi urmatoarea inregistrare
        }

        //Trunchiem fisierul la noua dimensiune (vechea dimensiune - marimea unui Report)
        struct stat st;
        fstat(fd, &st);
        ftruncate(fd, st.st_size - sizeof(Report));
        printf("Succes: Raportul %d a fost eliminat.\n", target_id);
    } else {
        printf("Eroare: ID-ul %d nu a fost găsit.\n", target_id);
    }
    close(fd);
}

int main(int argc, char* argv[]){
    char *role = NULL, *user = NULL, *command = NULL, *district = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0) role = argv[++i];
        else if (strcmp(argv[i], "--user") == 0) user = argv[++i];
        else if (command == NULL) command = argv[i];
        else if (district == NULL) district = argv[i];
    }

    if (!role || !user || !command || !district) return -1;

    mkdir(district, 0750);
    chmod(district, 0750);

    char rep_path[256], cfg_path[256];
    sprintf(rep_path, "%s/reports.dat", district);
    sprintf(cfg_path, "%s/district.cfg", district);

    //Comanda ADD
    if (strcmp(command, "--add") == 0) {
        int fd = open(rep_path, O_WRONLY | O_CREAT | O_APPEND, 0664);
        Report r = {0};
        r.report_id = (int)time(NULL) % 10000;
        strncpy(r.name, user, 50);
        r.timestamp = time(NULL);
        
        printf("Latitude: "); scanf("%f", &r.latitude);
        printf("Longitude: "); scanf("%f", &r.longitude);
        printf("Category (road/lighting/etc): "); scanf("%29s", r.issue_category);
        printf("Severity (1-3): "); scanf("%d", &r.severity);
        printf("Description: "); scanf("%103s", r.description_text);
                
        write(fd, &r, sizeof(Report));
        close(fd);
        chmod(rep_path, 0664);
        
        char link_name[256];
        sprintf(link_name, "active_reports-%s", district);
        unlink(link_name);
        symlink(rep_path, link_name); 
        
        int notified = 0;
        FILE *pid_file = fopen(".monitor_pid", "r");
        if (pid_file) {
            pid_t monitor_pid;
            if (fscanf(pid_file, "%d", &monitor_pid) == 1) {
                if (kill(monitor_pid, SIGUSR1) == 0) {
                    notified = 1;
                }
            }
            fclose(pid_file);
        }
        
        if (notified) {
            log_action(district, role, user, "add_report (Monitor notified)");
        } else {
            log_action(district, role, user, "add_report (Monitor could not be informed)");
        }
    } 

    //Comanda UPDATE_THRESHOLD
    else if (strcmp(command, "--update_threshold") == 0) {
        if (strcmp(role, "manager") != 0) {
            printf("Eroare: Ai nevoie de rolul de Manager \n");
            return 1;
        }
        //Verificam permisiunile inainte de scriere
        if (!has_permission(cfg_path, role, S_IWUSR, S_IRGRP)) {
            printf("Eroare: Nu ai permisiune pentru district.cfg\n");
            return 1;
        }
        int fd = open(cfg_path, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        char *val = argv[argc-1];
        write(fd, val, strlen(val));
        close(fd);
        chmod(cfg_path, 0640);
        log_action(district, role, user, "update_threshold");
    }

    //Comanda LIST
    else if (strcmp(command, "--list") == 0) {
        struct stat st;
        if (stat(rep_path, &st) == 0) {
            char p_str[11];
            mode_to_string(st.st_mode, p_str);
            printf("Fisier: %s | Perms: %s | Size: %ld\n", rep_path, p_str, st.st_size);
        }
        int fd = open(rep_path, O_RDONLY);
        Report r;
        while (read(fd, &r, sizeof(Report)) > 0) {
            printf("ID: %d | Cat: %s | Sev: %d\n", r.report_id, r.issue_category, r.severity);
        }
        close(fd);
        log_action(district, role, user, "list");
    }

    //Comanda VIEW
    else if (strcmp(command, "--view") == 0) {
        int target = atoi(argv[argc-1]);
        int fd = open(rep_path, O_RDONLY); Report r;
        while (read(fd, &r, sizeof(Report)) > 0) {
            if (r.report_id == target) {
                printf("ID: %d\nInspector: %s\nCoords: %.2f, %.2f\nDesc: %s\n", 
                        r.report_id, r.name, r.latitude, r.longitude, r.description_text);
                break;
            }
        }
        close(fd); log_action(district, role, user, "view");
    }

    //Comanda FILTER
    else if (strcmp(command, "--filter") == 0) {
        int fd = open(rep_path, O_RDONLY);
        Report r;
        while (read(fd, &r, sizeof(Report)) > 0) {
            int all_match = 1;
            for (int i = 1; i < argc; i++) {
                char fld[32], op[8], val[64];
                if (parse_condition(argv[i], fld, op, val)) {
                    if (!match_condition(&r, fld, op, val)) all_match = 0;
                }
            }
            if (all_match) printf("Am găsit: ID %d\n", r.report_id);
        }
        close(fd);
        log_action(district, role, user, "filter");
    }

    //Comanda REMOVE_REPORT
    else if (strcmp(command, "--remove_report") == 0) {
        if (strcmp(role, "manager") != 0) {
            printf("Acces refuzat: Doar managerul poate șterge.\n");
            return 1;
        }
        remove_report(rep_path, atoi(argv[argc-1]));
        log_action(district, role, user, "remove");
    }

    //Comanda REMOVE_DISTRICT
    else if (strcmp(command, "--remove_district") == 0) {
        if (strcmp(role, "manager") != 0) {
            printf("Acces refuzat: Doar managerul poate șterge întregul district.\n");
            return 1;
        }

        char link_name[256];
        sprintf(link_name, "active_reports-%s", district);
        unlink(link_name);

        //Procesul copil
        pid_t pid = fork();

        if (pid < 0) {
            perror("Eroare la fork");
            return 1;
        } else if (pid == 0) {
            execlp("rm", "rm", "-rf", district, NULL);
            
            perror("Eroare la executarea comenzii rm");
            exit(1);
        } else {
            //Procesul părinte
            wait(NULL);
            printf("Succes: Districtul '%s' și conținutul său au fost șterse complet.\n", district);
        }
    }

    return 0;
}