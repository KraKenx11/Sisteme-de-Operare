#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

int keep_running = 1;

void handle_signal(int sig) {
    if (sig == SIGINT) {
        const char *msg = "\n[Monitor] S-a primit SIGINT. Monitorizarea se opreste...\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        keep_running = 0;
    } else if (sig == SIGUSR1) {
        const char *msg = "[Monitor] Alerta: Un nou raport a fost adaugat in sistem!\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
}

int main() {
    int fd = open(".monitor_pid", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Eroare la crearea fisierului .monitor_pid");
        return 1;
    }
    
    char pid_str[32];
    int len = sprintf(pid_str, "%d\n", getpid());
    write(fd, pid_str, len);
    close(fd);

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sa.sa_flags = SA_RESTART; 
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Eroare la sigaction pentru SIGINT");
        return 1;
    }
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Eroare la sigaction pentru SIGUSR1");
        return 1;
    }

    printf("[Monitor] A pornit cu PID-ul %d. Astept semnale...\n", getpid());

    while (keep_running) {
        pause();
    }

    unlink(".monitor_pid");
    printf("[Monitor] Cleanup efectuat (.monitor_pid sters). Program terminat.\n");

    return 0;
}