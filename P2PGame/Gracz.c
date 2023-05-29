//Oddano 24.11.2022
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <pthread.h>
int rounds;
pthread_mutex_t lock;
pthread_mutex_t lock2;
enum types{HUMAN,CPU};
//Gracz
struct Player{
    int ID;
    int PID;
    enum types type;
    int x,y;
    int deaths;
    int carried;
    int budget;
    unsigned char inBushes:2;
    int spawnpointx,spawnpointy;
//    unsigned char inMove:1;
};
//
struct PlayerPackage{
    char playerActionButton;
    int PID;
}playerPackage;
struct ServerPackage{
    char mapPlayerView[5][5];
    struct Player player;
    int endOfMapX;
    int round;
    int PID;
}serverPackage;
void displayPlayerView(){
    for(int i=0;i<=51;i++){
        for(int j=0;j<=51;j++){
            if((j<serverPackage.player.x-2 || j>serverPackage.player.x+2) || (i<serverPackage.player.y-2 || i>serverPackage.player.y+2)){
                mvprintw(i,j," ");
            }
        }
    }
    for(int i=serverPackage.player.y-2,k=0;i<=(serverPackage.player.y+2) && k<5;i++,k++){
        for(int j=serverPackage.player.x-2,l=0;j<=(serverPackage.player.x+2) && l<5;j++,l++){
                if(serverPackage.mapPlayerView[k][l]=='|'){
                    attron(A_STANDOUT);
                    mvprintw(i,j," ");
                    attroff(A_STANDOUT);
                }
                else {
                    mvprintw(i,j,"%c",serverPackage.mapPlayerView[k][l]);
                }
            }
        }
    }
void displayStats(){
    mvprintw(1,serverPackage.endOfMapX+3,"Server's PID: %d",serverPackage.PID);
    mvprintw(2,serverPackage.endOfMapX+4,"Campsite unknown");
    mvprintw(3,serverPackage.endOfMapX+4,"Round number: %d",rounds);
    mvprintw(5,serverPackage.endOfMapX+3,"Player:");
    mvprintw(6,serverPackage.endOfMapX+4,"Number %d",serverPackage.player.ID);
    mvprintw(7,serverPackage.endOfMapX+4,"Type  HUMAN");
    mvprintw(8,serverPackage.endOfMapX+4,"Curr X/Y     %d/%d",serverPackage.player.x,serverPackage.player.y);
    mvprintw(9,serverPackage.endOfMapX+4,"Deaths       %d",serverPackage.player.deaths,serverPackage.player.deaths);
    mvprintw(11,serverPackage.endOfMapX+8,"Coins found     %d",serverPackage.player.carried);
    mvprintw(12,serverPackage.endOfMapX+8,"Coins brought     %d",serverPackage.player.budget);
    mvprintw(15,serverPackage.endOfMapX+3,"Legend: ");
    mvprintw(16,serverPackage.endOfMapX+4,"12 - players");
    attron(A_STANDOUT);
    mvprintw(17,serverPackage.endOfMapX+4," ");
    attroff(A_STANDOUT);
    mvprintw(17,serverPackage.endOfMapX+5,"    - wall");
    mvprintw(18,serverPackage.endOfMapX+4,"#    - bushes (slow down)");
    mvprintw(19,serverPackage.endOfMapX+4,"*    - wild beast");
    mvprintw(20,serverPackage.endOfMapX+4,"c    - one coin                  D - dropped treasure");
    mvprintw(21,serverPackage.endOfMapX+4,"t    - treasure (10 coins)");
    mvprintw(22,serverPackage.endOfMapX+4,"T    - large treasure (50 coins)");
    mvprintw(23,serverPackage.endOfMapX+4,"A    - campsite");
}
void* holdconnection(void*arg){
    int sockfd=*(int*)arg;
    playerPackage.playerActionButton=0;
    while(1)
    {
        read(sockfd, &serverPackage, sizeof(struct ServerPackage));
        rounds=serverPackage.round;
        playerPackage.PID=getpid();
        write(sockfd, &playerPackage, sizeof(struct PlayerPackage));
        pthread_mutex_lock(&lock2);
        playerPackage.playerActionButton=0;
        pthread_mutex_unlock(&lock2);
    }
    return NULL;
}
void* keyHandler(void*arg){
    char h;
    while(1){
        h=getch();
        if(h=='q' || h=='Q'){
            pthread_mutex_destroy(&lock);
            pthread_mutex_destroy(&lock2);
            endwin();
            exit(0);
        }
        pthread_mutex_lock(&lock2);
        playerPackage.playerActionButton=h;
        pthread_mutex_unlock(&lock2);
    }
}
int main(int argc, char *argv[]) {

    pthread_t th1,th2;
    int sockfd = 0;
    struct sockaddr_in serv_addr;
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Nie mozna stworzyc socketu\n");
        exit(1);
    }
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000);
    if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)<=0)
    {
        printf("Niepoprawne ip\n");
        exit(1);
    }
    initscr();
    curs_set(0);
    noecho();
    cbreak();
    pthread_mutex_init(&lock,NULL);
    pthread_mutex_init(&lock2,NULL);
    pthread_create(&th2, NULL, keyHandler, NULL);
    serverPackage.endOfMapX=51;
    char message='N';
    while(message=='N'){
        connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        read(sockfd, &message, sizeof(char));
        if(message=='N'){
            mvprintw(5,20,"Brak miejsc");
            refresh();
        }
    }
    pthread_create(&th1, NULL, holdconnection, &sockfd);
    while(1){
        displayPlayerView();
        if(serverPackage.player.x>=0){
            displayStats();
        }
        refresh();
    }
    pthread_join(th1,NULL);
    pthread_join(th2,NULL);
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&lock2);
    endwin();
    close(sockfd);
    return 0;
}
