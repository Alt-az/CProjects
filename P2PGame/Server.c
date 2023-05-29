//Oddano 24.11.2022
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <ncurses.h>
#include <ctype.h>
#include <semaphore.h>
#include <math.h>
pthread_mutex_t mutex;

int howManyPlayers=2;
int rounds=0;
int howManyMonsters=0;
struct AllowToMove{
    unsigned char flag:1;
}allowToMovePlayer[2],allowToMoveMonsters;
struct Loot{
    int budget;
    int x,y;
};
struct Map{
    char world[128][128];
    int endOfMapX;
    int endOfMapY;
    int campsiteXY[2];
    int lootCount;
    struct Loot *loot;
}cleanmap;
enum types{HUMAN,CPU};
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
}player[2];
struct Monster{
    char mapPlayerView[5][5];
    int x,y;
    unsigned char inBushes:2;
}*monster;
struct ServerPackage{
    char mapPlayerView[5][5];
    struct Player player;
    int endOfMapX;
    int round;
    int PID;
};
struct PlayerPackage{
    char playerActionButton;
    int PID;
};
void spawnPlayer(int whichOne){
    int x=0,y=0;
    while(cleanmap.world[y][x]!=' '){
        srand(time(NULL));
        x=(rand() % cleanmap.endOfMapX);
        y=(rand() % cleanmap.endOfMapY);
    }
    player[whichOne].y=y;
    player[whichOne].x=x;
    player[whichOne].spawnpointy=y;
    player[whichOne].spawnpointx=x;
}
void movePlayer(struct ServerPackage* serverPack,char button){
    if(serverPack->player.ID>=1 && serverPack->player.ID<=2 && allowToMovePlayer[serverPack->player.ID - 1].flag == 1 && button != ' ' && isalpha(button)) {
        if(serverPack->player.inBushes!=1) {
            if(serverPack->player.inBushes==2){
                serverPack->player.inBushes=0;
            }
            switch (button) {
                case 'w':
                    if (serverPack->mapPlayerView[1][2] != '|') {
                        serverPack->player.y--;
                    }
                    break;
                case 's':
                    if (serverPack->mapPlayerView[3][2] != '|') {
                        serverPack->player.y++;
                    }
                    break;
                case 'a':
                    if (serverPack->mapPlayerView[2][1] != '|') {
                        serverPack->player.x--;
                    }
                    break;
                case 'd':
                    if (serverPack->mapPlayerView[2][3] != '|') {
                        serverPack->player.x++;
                    }
                    break;
                default:
                    break;
            }
        }
        else if(serverPack->player.inBushes==1){
            serverPack->player.inBushes=2;
        }
        allowToMovePlayer[serverPack->player.ID - 1].flag=0;
    }
}
void actionHandler(struct ServerPackage* serverPack){
    if(cleanmap.world[serverPack->player.y][serverPack->player.x]=='c'){
        serverPack->player.carried++;
        cleanmap.world[serverPack->player.y][serverPack->player.x]=' ';
    }
    else if(cleanmap.world[serverPack->player.y][serverPack->player.x]=='t'){
        serverPack->player.carried+=10;
        cleanmap.world[serverPack->player.y][serverPack->player.x]=' ';
    }
    else if(cleanmap.world[serverPack->player.y][serverPack->player.x]=='T'){
        serverPack->player.carried+=50;
        cleanmap.world[serverPack->player.y][serverPack->player.x]=' ';
    }
    else if(cleanmap.world[serverPack->player.y][serverPack->player.x]=='A'){
        serverPack->player.budget+=serverPack->player.carried;
        serverPack->player.carried=0;
    }
    else if(cleanmap.world[serverPack->player.y][serverPack->player.x]=='#' && serverPack->player.inBushes==0){
        serverPack->player.inBushes=1;
    }
    for(int i=0;i<cleanmap.lootCount;i++){
        if(serverPack->player.x==cleanmap.loot[i].x && serverPack->player.y==cleanmap.loot[i].y){
            serverPack->player.carried+=cleanmap.loot[i].budget;
            cleanmap.loot[i].budget=0;
        }
    }
    for(int i=0;i<howManyPlayers;i++){
        if(player[i].y==serverPack->player.y && player[i].x==serverPack->player.x && serverPack->player.ID!=player[i].ID) {
            struct Loot* tmp;
            tmp=realloc(cleanmap.loot,sizeof(struct Loot)*(cleanmap.lootCount+1));
            if(tmp!=NULL){
                cleanmap.loot=tmp;
                cleanmap.lootCount++;
            }
            cleanmap.loot[cleanmap.lootCount-1].y=serverPack->player.y;
            cleanmap.loot[cleanmap.lootCount-1].x=serverPack->player.x;
            cleanmap.loot[cleanmap.lootCount-1].budget=serverPack->player.carried+player[i].carried;
            serverPack->player.carried=0;
            player[i].carried=0;
            serverPack->player.y=serverPack->player.spawnpointy;
            serverPack->player.x=serverPack->player.spawnpointx;
            serverPack->player.deaths++;
            player[i].x=player[i].spawnpointx;
            player[i].y=player[i].spawnpointy;
            player[i].deaths++;
        }
    }
    for(int i=0;i<howManyMonsters;i++){
        if(monster[i].y==serverPack->player.y && monster[i].x==serverPack->player.x) {
            struct Loot* tmp;
            tmp=realloc(cleanmap.loot,sizeof(struct Loot)*(cleanmap.lootCount+1));
            if(tmp!=NULL){
                cleanmap.loot=tmp;
                cleanmap.lootCount++;
            }
            cleanmap.loot[cleanmap.lootCount-1].y=serverPack->player.y;
            cleanmap.loot[cleanmap.lootCount-1].x=serverPack->player.x;
            cleanmap.loot[cleanmap.lootCount-1].budget=serverPack->player.carried+player[i].carried;
            serverPack->player.y=serverPack->player.spawnpointy;
            serverPack->player.x=serverPack->player.spawnpointx;
            serverPack->player.deaths++;
        }
    }
}
void displayWorldMap(){
    for(int i=0;i<128;i++){
        for(int j=0;j<128;j++){
            if(cleanmap.world[i][j]=='|'){
                attron(A_STANDOUT);
                mvprintw(i,j," ");
                attroff(A_STANDOUT);
            }
            else {
                mvprintw(i,j,"%c",cleanmap.world[i][j]);
            }
        }
    }
    for(int i=0;i<howManyPlayers;i++){
        if(player[i].y>=0 && player[i].x>=0) {
            mvprintw(player[i].y,player[i].x,"%c",(char)(player[i].ID+'0'));
        }
    }
    for(int i=0;i<howManyMonsters;i++){
        if(monster[i].y>=0 && monster[i].x>=0) {
            mvprintw(monster[i].y,monster[i].x,"*");
        }
    }
    for(int i=0;i<cleanmap.lootCount;i++){
        if(cleanmap.loot[i].budget!=0){
            mvprintw(cleanmap.loot[i].y,cleanmap.loot[i].x,"D");
        }
    }
}
void displayStats(){
    mvprintw(1,cleanmap.endOfMapX+3,"Server's PID: %d",getpid());
    mvprintw(2,cleanmap.endOfMapX+4,"Campsite X/Y: %d/%d",cleanmap.campsiteXY[0],cleanmap.campsiteXY[1]);
    mvprintw(3,cleanmap.endOfMapX+4,"Round number: %d",rounds);
    mvprintw(5,cleanmap.endOfMapX+3,"Parameter:   Player1  Player2");
    mvprintw(6,cleanmap.endOfMapX+4,"PID");
    mvprintw(6,cleanmap.endOfMapX+17,"                             ");
    mvprintw(7,cleanmap.endOfMapX+17,"                             ");
    mvprintw(8,cleanmap.endOfMapX+17,"                             ");
    mvprintw(9,cleanmap.endOfMapX+17,"                             ");
    mvprintw(12,cleanmap.endOfMapX+21,"                             ");
    mvprintw(13,cleanmap.endOfMapX+21,"                             ");
    if(player[0].ID!=-1){
        mvprintw(6,cleanmap.endOfMapX+17,"%d",player[0].PID);
        if(player[0].type==HUMAN){
            mvprintw(7,cleanmap.endOfMapX+17,"HUMAN");
        }
        mvprintw(8,cleanmap.endOfMapX+17,"%d/%d",player[0].x,player[0].y);
        mvprintw(9,cleanmap.endOfMapX+17,"%d",player[0].deaths);
        mvprintw(12,cleanmap.endOfMapX+21,"%d",player[0].carried);
        mvprintw(13,cleanmap.endOfMapX+21,"%d",player[0].budget);
    }else{
        mvprintw(6,cleanmap.endOfMapX+17,"-");
        mvprintw(7,cleanmap.endOfMapX+17,"-");
        mvprintw(8,cleanmap.endOfMapX+17,"--/--");
        mvprintw(9,cleanmap.endOfMapX+17,"-");
        mvprintw(12,cleanmap.endOfMapX+21,"-");
        mvprintw(13,cleanmap.endOfMapX+21,"-");
    }
    if(player[1].ID!=-1){
        mvprintw(6,cleanmap.endOfMapX+26,"%d",player[1].PID);
        if(player[1].type==HUMAN){
            mvprintw(7,cleanmap.endOfMapX+26,"HUMAN");
        }
        mvprintw(8,cleanmap.endOfMapX+26,"%d/%d",player[1].x,player[1].y);
        mvprintw(9,cleanmap.endOfMapX+26,"%d",player[1].deaths);
        mvprintw(12,cleanmap.endOfMapX+30,"%d",player[1].carried);
        mvprintw(13,cleanmap.endOfMapX+30,"%d",player[1].budget);
    }else{
        mvprintw(6,cleanmap.endOfMapX+26,"-");
        mvprintw(7,cleanmap.endOfMapX+26,"-");
        mvprintw(8,cleanmap.endOfMapX+26,"--/--");
        mvprintw(9,cleanmap.endOfMapX+26,"-");
        mvprintw(12,cleanmap.endOfMapX+30,"-");
        mvprintw(13,cleanmap.endOfMapX+30,"-");
    }
    mvprintw(7,cleanmap.endOfMapX+4,"Type");
    mvprintw(8,cleanmap.endOfMapX+4,"Curr X/Y");
    mvprintw(9,cleanmap.endOfMapX+4,"Deaths");
    mvprintw(11,cleanmap.endOfMapX+4,"Coins");
    mvprintw(12,cleanmap.endOfMapX+8,"carried");
    mvprintw(13,cleanmap.endOfMapX+8,"brought");
    mvprintw(15,cleanmap.endOfMapX+3,"Legend: ");
    mvprintw(16,cleanmap.endOfMapX+4,"12 - players");
    attron(A_STANDOUT);
    mvprintw(17,cleanmap.endOfMapX+4," ");
    attroff(A_STANDOUT);
    mvprintw(17,cleanmap.endOfMapX+5,"    - wall");
    mvprintw(18,cleanmap.endOfMapX+4,"#    - bushes (slow down)");
    mvprintw(19,cleanmap.endOfMapX+4,"*    - wild beast");
    mvprintw(20,cleanmap.endOfMapX+4,"c    - one coin                  D - dropped treasure");
    mvprintw(21,cleanmap.endOfMapX+4,"t    - treasure (10 coins)");
    mvprintw(22,cleanmap.endOfMapX+4,"T    - large treasure (50 coins)");
    mvprintw(23,cleanmap.endOfMapX+4,"A    - campsite");
}
void createMap(){
    for(int i=0;i<128;i++){
        for(int j=0;j<128;j++){
             cleanmap.world[i][j]=0;
        }
    }
    FILE* fp = fopen("Map.txt","r");
    unsigned char h;
    cleanmap.lootCount=0;
    for(int i=0;i<128;i++){
        for(int j=0;j<128;j++){
            h= (char)fgetc(fp);
            if(h!='\n'){
                cleanmap.world[i][j]=(char)h;
                if(cleanmap.world[i][j]=='A'){
                    cleanmap.campsiteXY[0]=j;
                    cleanmap.campsiteXY[1]=i;
                }
            }
            else{
                cleanmap.endOfMapX=j;
                cleanmap.endOfMapY=i;
                break;
            }
            if(feof(fp)){
                fclose(fp);
                return;
            }
        }
    }
    fclose(fp);
}
void assignMonsterMap(){
    for(int o=0;o<howManyMonsters;o++){
        for (int i = monster[o].y - 2,k=0; i <= (monster[o].y + 2) && k < 5; i++,k++) {
            for (int j = monster[o].x - 2,l=0; j <= (monster[o].x + 2) && l < 5; j++,l++) {
                if(i>=0 && j>=0){
                    monster[o].mapPlayerView[k][l] = cleanmap.world[i][j];
                    for(int g=0;g<cleanmap.lootCount;g++){
                        if(cleanmap.loot[g].budget!=0 && cleanmap.loot[g].y==i && cleanmap.loot[g].x==j){
                            monster[o].mapPlayerView[k][l]='D';
                        }
                    }
                    for(int g=0;g<2;g++){
                        if(player[g].y==i && player[g].x==j){
                            monster[o].mapPlayerView[k][l]=(char)(player[g].ID+'0');
                        }
                    }
                    for(int g=0;g<howManyMonsters;g++){
                        if(monster[g].y==i && monster[g].x==j) {
                            monster[o].mapPlayerView[k][l]='*';
                        }
                    }
                }
            }
        }
    }
}
void assignPlayerMap(struct ServerPackage* serverPackage){
    for (int i = serverPackage->player.y - 2,k=0; i <= (serverPackage->player.y + 2) && k < 5; i++,k++) {
        for (int j = serverPackage->player.x - 2,l=0; j <= (serverPackage->player.x + 2) && l < 5; j++,l++) {
            if(i>=0 && j>=0){
                serverPackage->mapPlayerView[k][l] = cleanmap.world[i][j];
                for(int g=0;g<cleanmap.lootCount;g++){
                    if(cleanmap.loot[g].budget!=0 && cleanmap.loot[g].y==i && cleanmap.loot[g].x==j){
                        serverPackage->mapPlayerView[k][l]='D';
                    }
                }
                for(int g=0;g<2;g++){
                    if(player[g].y==i && player[g].x==j){
                        serverPackage->mapPlayerView[k][l]=(char)(player[g].ID+'0');
                    }
                }
                for(int g=0;g<howManyMonsters;g++){
                    if(monster[g].y==i && monster[g].x==j) {
                        serverPackage->mapPlayerView[k][l]='*';
                    }
                }
            }
        }
    }
}
void playerInit(int count){
    player[count].x=1;
    player[count].y=1;
    player[count].ID=count+1;
    player[count].budget=0;
    player[count].carried=0;
    player[count].deaths=0;
    player[count].type=HUMAN;
    player[count].inBushes=0;
    spawnPlayer(count);
}
void* holdconnection(void*arg){
    int connfd=*(int*)arg;
    int whichPlayer=*((int*)(arg+4));
    struct ServerPackage serverPackage;
    struct PlayerPackage playerPackage;
    playerPackage.playerActionButton=0;
    playerInit(whichPlayer);
    while(1)
    {
        pthread_mutex_lock(&mutex);
        serverPackage.player=player[whichPlayer];
        assignPlayerMap(&serverPackage);
        serverPackage.endOfMapX=cleanmap.endOfMapX;
        serverPackage.round=rounds;
        serverPackage.PID=getpid();
        if(recv(connfd,NULL,1, MSG_PEEK | MSG_DONTWAIT)==0){
            player[whichPlayer].ID=-1;
            player[whichPlayer].x=-1;
            player[whichPlayer].y=-1;
            close(connfd);
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        write(connfd, &serverPackage, sizeof(struct ServerPackage));
        read(connfd, &playerPackage, sizeof(struct PlayerPackage));
        movePlayer(&serverPackage, playerPackage.playerActionButton);
        actionHandler(&serverPackage);
        serverPackage.player.PID=playerPackage.PID;
        player[whichPlayer]=serverPackage.player;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}
void whichDirection(int x,int y,int* directionx,int* directiony){
    int localMonsterX=2,localMonsterY=2;
    if(localMonsterX < x && localMonsterY < y){
        *directionx=1;
        *directiony=1;
    }
    if(localMonsterX < x && localMonsterY == y){
        *directionx=1;
        *directiony=0;
    }
    if(localMonsterX == x && localMonsterY < y){
        *directionx=0;
        *directiony=1;
    }
    else if(localMonsterX > x && localMonsterY > y){
        *directionx=-1;
        *directiony=-1;
    }
    else if(localMonsterX > x && localMonsterY == y){
        *directionx=-1;
        *directiony=0;
    }
    else if(localMonsterX == x && localMonsterY > y){
        *directionx=0;
        *directiony=-1;
    }
    else if(localMonsterX < x && localMonsterY > y){
        *directionx=1;
        *directiony=-1;
    }
    else if(localMonsterX > x && localMonsterY < y){
        *directionx=-1;
        *directiony=1;
    }
}
void newMonster(){
    int x=0, y=0;
    while(cleanmap.world[y][x]!=' '){
        srand(time(NULL));
        x=(rand() % cleanmap.endOfMapX);
        y=(rand() % cleanmap.endOfMapY);
    }
    struct Monster* tmp;
    tmp=realloc(monster,sizeof(struct Monster)*(howManyMonsters+1));
    if(tmp!=NULL){
        monster=tmp;
    }
    monster[howManyMonsters].x=x;
    monster[howManyMonsters].y=y;
    for (int i = monster[howManyMonsters].y - 2,k=0; i <= (monster[howManyMonsters].y + 2) && k < 5; i++,k++) {
        for (int j = monster[howManyMonsters].x - 2,l=0; j <= (monster[howManyMonsters].x + 2) && l < 5; j++,l++) {
            if(i>=0 && j>=0){
                monster[howManyMonsters].mapPlayerView[k][l] = cleanmap.world[i][j];
            }
        }
    }
    howManyMonsters++;
}
int isTherePlayer(int whichOne,int x,int y){
    int distance= (int)sqrt(((x - 2) * (x - 2)) + ((y - 2) * (y - 2)));
    int directionx,directiony;
    whichDirection(x,y,&directionx,&directiony);
    if(directiony!=0 && directionx!=0) {
        for (int i = 1; i < distance && (2 + (i * directiony))>=0 && (2 + (i * directiony)) < 5 && (2 + (i * directionx))>=0 && (2 + (i * directionx)) < 5; i++) {
            if (monster[whichOne].mapPlayerView[2 + (i * directiony)][2 + (i * directionx)] == '|') {
                return 0;
            }
        }
        if(isdigit(monster[whichOne].mapPlayerView[2 + (distance * directiony)][2 + (distance * directionx)])){
            return 1;
        }
    }
    else if(directiony==0){
        for (int i = 1; i < distance && (2 + (i * directionx))>=0 && (2 + (i * directionx)) < 5; i++) {
            if (monster[whichOne].mapPlayerView[2][2 + (i * directionx)] == '|') {
                return 0;
            }
        }
        if(isdigit(monster[whichOne].mapPlayerView[2][2 + (distance * directionx)])){
            return 1;
        }
    }
    else if(directionx==0){
        for (int i = 1; i < distance && (2 + (i * directiony))>=0 && (2 + (i * directiony)) < 5; i++) {
            if (monster[whichOne].mapPlayerView[2 + (i * directiony)][2] == '|') {
                return 0;
            }
        }
        if(isdigit(monster[whichOne].mapPlayerView[2 + (distance * directiony)][2])){
            return 1;
        }
    }
    return 0;
}
void whereIsPlayer(int whichOne,int *where){
    for(int i=0;i<5;i++){
        for(int j=0;j<5;j++){
            if(isdigit(monster[whichOne].mapPlayerView[i][j])){
                where[0]=i;
                where[1]=j;
                return;
            }
        }
    }
    where[0]=-1;
    where[1]=-1;
}
int canSeePlayer(int whichOne){
    int where[2];
    where[0]=-2;
    where[1]=-2;
    whereIsPlayer(whichOne,where);
    if(where[0]==-1 && where[1]==-1){
        return 0;
    }
    if(isTherePlayer(whichOne,where[1],where[0])){
        return 1;
    }
    else{
        return 0;
    }
}
void moveInDirection(int whichOne,int direction){
    if(monster[whichOne].inBushes!=1) {
        if(monster[whichOne].inBushes==2){
            monster[whichOne].inBushes=0;
        }
        switch (direction) {
            case 1:
                if (monster[whichOne].mapPlayerView[2][3] != '|') {
                    monster[whichOne].x++;
                }
                break;
            case 2:
                if (monster[whichOne].mapPlayerView[2][1] != '|') {
                    monster[whichOne].x--;
                }
                break;
            case 3:
                if (monster[whichOne].mapPlayerView[3][2] != '|') {
                    monster[whichOne].y++;
                }
                break;
            case 4:
                if (monster[whichOne].mapPlayerView[1][2] != '|') {
                    monster[whichOne].y--;
                }
                break;
            default:
                break;
        }
    }else{
        monster[whichOne].inBushes=2;
    }
}
void chasePlayer(int whichOne){
    int where[2];
    whereIsPlayer(whichOne,where);
    int directionx,directiony;
    whichDirection(where[1],where[0],&directionx,&directiony);
    if(directionx==0 || directiony==0) {
        if (monster[whichOne].mapPlayerView[2 + directiony][2 + directionx] != '|') {
            monster[whichOne].x += directionx;
            monster[whichOne].y += directiony;
        }
    }else {
        if (monster[whichOne].mapPlayerView[2 + directiony][2] != '|') {
            monster[whichOne].y += directiony;
        }else if(monster[whichOne].mapPlayerView[2][2 + directionx] != '|'){
            monster[whichOne].x += directionx;
        }
    }
}
int moveMonsters(){
    srand(time(NULL));
    for(int i=0;i<howManyMonsters;i++){
        if(cleanmap.world[monster[i].y][monster[i].x]=='#' && monster[i].inBushes==0){
            monster[i].inBushes=1;
        }
        if(!canSeePlayer(i)){
            int direction=(rand() % 5);
            moveInDirection(i,direction);
        }
        else{
            chasePlayer(i);
        }
    }
}
void* monstersThread(void*arg){
    while(1){
        if(allowToMoveMonsters.flag==1){
            allowToMoveMonsters.flag=0;
            assignMonsterMap();
            moveMonsters();
        }
    }
}
void* displayThread(void*arg){
    while(1){
        displayWorldMap();
        displayStats();
        refresh();
    }
}
void* keyHandler(void*arg){
    char h;
    while(1){
        h=getch();
        if(h=='B' || h=='b'){
            newMonster();
        }
        else if(h=='c' || h=='C' || h=='T' || h=='t'){
            int x=0, y=0;
            while(cleanmap.world[y][x]!=' '){
                srand(time(NULL));
                x=(rand() % cleanmap.endOfMapX);
                y=(rand() % cleanmap.endOfMapY);
            }
            cleanmap.world[y][x]=h;
        }
        else if(h=='Q' || h=='q'){
            pthread_mutex_destroy(&mutex);
            free(cleanmap.loot);
            free(monster);
            endwin();
            exit(0);
        }
    }
}
void* manageConnections(void*arg){
    pthread_t th1,th2;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(5000);

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    int whichPlayer;
    listen(listenfd, 10);
    while(1){
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        whichPlayer=-1;
        for(int i=0;i<howManyPlayers;i++){
            if(player[i].ID==-1){
                whichPlayer=i;
                break;
            }
        }
        if(whichPlayer!=-1){
            write(connfd, "P", sizeof(char));
            if(whichPlayer==0){
                int data[2];
                data[0]=connfd;
                data[1]=whichPlayer;
                pthread_join(th1,NULL);
                pthread_create(&th1, NULL, holdconnection, data);
            }else if(whichPlayer==1){
                int data[2];
                data[0]=connfd;
                data[1]=whichPlayer;
                pthread_join(th2,NULL);
                pthread_create(&th2, NULL, holdconnection, data);
            }
        }
        else{
            write(connfd, "N", sizeof(char));
            close(connfd);
        }
    }
}
int main(int argc, char *argv[])
{
    initscr();
    pthread_t th3,th4,th5,th6;

    player[0].ID=-1;
    player[1].ID=-1;
    player[0].x=-1;
    player[0].y=-1;
    player[1].x=-1;
    player[1].y=-1;
    curs_set(0);
    noecho();
    cbreak();
    allowToMovePlayer[0].flag=0;
    allowToMovePlayer[1].flag=0;
    pthread_mutex_init(&mutex, NULL);
    createMap();
    pthread_create(&th3, NULL, displayThread, NULL);
    pthread_create(&th4, NULL, manageConnections, NULL);
    pthread_create(&th5, NULL, monstersThread, NULL);
    pthread_create(&th6, NULL, keyHandler, NULL);
    while (1) {
        rounds++;
        allowToMoveMonsters.flag = 1;
        allowToMovePlayer[0].flag = 1;
        allowToMovePlayer[1].flag = 1;
        sleep(1);
    }
    pthread_join(th3,NULL);
    pthread_join(th4,NULL);
    pthread_join(th5,NULL);
    pthread_join(th6,NULL);
    pthread_mutex_destroy(&mutex);
    free(cleanmap.loot);
    endwin();
}