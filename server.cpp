#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include<iostream>
#include<string>
#include<algorithm>
#include<vector>
#include<iterator>
#include<map>
#include <cerrno>
#include <iostream>

#define WAITING_QUEUE 30

// Struktura przechowująca parametry wątku dla gry Reversi
struct ReversiThreadParams {
    int* firstPlayerSocket;   // Wskaźnik na gniazdo pierwszego gracza
    int* opponentPlayerSocket; // Wskaźnik na gniazdo drugiego gracza
    int* waitingPlayerFlag;    // Wskaźnik na flagę oczekującego gracza
};

// Numer portu, na którym serwer Reversi będzie nasłuchiwał
int PORT_NUMBER = 1234;

// Funkcja do wysyłania danych przez gniazdo Reversi
// Wysyła dane o wielkości 'size' poprzez gniazdo gracza 'player' z użyciem protokołu write.
// Zwraca ilość wysłanych bajtów w przypadku sukcesu, a -1 w przypadku błędu.
int sendToReversiSocket(int player, const char* message, int size) {
    int remainingBytes = size;
    const char* currentPosition = message;

    while (remainingBytes > 0) {
        int bytesSent = write(player, currentPosition, remainingBytes);

        if (bytesSent <= 0) {
            return -1; // Błąd podczas wysyłania danych
        }

        remainingBytes -= bytesSent;
        currentPosition += bytesSent;
    }

    return size; 
}

// Funkcja do odczytywania danych z gniazda Reversi
// Odczytuje dane o wielkości 'size' poprzez gniazdo gracza 'player' z użyciem protokołu read.
// Zwraca wskaźnik do bufora zawierającego odczytane dane.
char* readFromReversiSocket(int player, int size) {
    char* buffer = new char[size];
    int remainingBytes = size;
    char* currentPosition = buffer;

    while (remainingBytes > 0) {
        int bytesRead = read(player, currentPosition, remainingBytes);

        if (bytesRead <= 0) {
            delete[] buffer;
            throw player; // Błąd podczas odczytywania danych
        }

        remainingBytes -= bytesRead;
        currentPosition += bytesRead;
    }

    return buffer;
}

// Klasa reprezentująca grę Reversi
class ReversiGame {
private:
    // Funkcja pomocnicza do uzyskiwania indeksu na planszy Reversi
    // Przyjmuje współrzędne (x, y) i zwraca indeks w jednowymiarowej tablicy reprezentującej planszę.
    int getBoardIndex(int x, int y) {
        return 8 * x + y;
    }

    // Funkcja pomocnicza do modyfikowania planszy Reversi
    // Modyfikuje planszę zaczynając od pozycji (x, y) w kierunku dir za pomocą wektorów dirX i dirY.
    // Zmienia kolor pionków na planszy.
    bool modifyReversiBoard(char color, int x, int y, int* dirX, int* dirY, int dir, int count, char t[8][8]) {
        for (int j = 0; j < count; j++) {
            t[x + dirX[dir] * j][y + dirY[dir] * j] = color;
        }
        return true;
    }

public:
    char boardState[64]; // Stan planszy Reversi

    // Konstruktor klasy ReversiGame
    // Inicjalizuje początkowy stan planszy, umieszczając po dwa pionki na środku planszy dla dwóch graczy.
    ReversiGame(char initialColor1, char initialColor2) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                boardState[getBoardIndex(i, j)] = 'e';
            }
        }

        boardState[getBoardIndex(3, 3)] = initialColor1;
        boardState[getBoardIndex(4, 4)] = initialColor1;
        boardState[getBoardIndex(3, 4)] = initialColor2;
        boardState[getBoardIndex(4, 3)] = initialColor2;
    }

    // Sprawdzenie, czy gracz ma możliwe ruchy w grze Reversi
    // Przeszukuje planszę w poszukiwaniu możliwych ruchów dla danego koloru pionków.
    // Zwraca true, jeśli gracz ma co najmniej jeden możliwy ruch, w przeciwnym razie false.
    bool hasValidReversiMove(char color) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                if (isValidReversiMove(color, i, j, true)) {
                    return true;
                }
            }
        }
        return false;
    }

    // Sprawdzenie, czy ruch w grze Reversi jest możliwy
    // Sprawdza, czy ruch na daną pozycję (x, y) dla danego koloru jest możliwy.
    // Jeśli test jest ustawiony na true, zwraca tylko informację o możliwości ruchu.
    // Jeśli test jest ustawiony na false, dokonuje rzeczywistej modyfikacji planszy.
    // Zwraca true, jeśli ruch jest możliwy, w przeciwnym razie false.
    bool isValidReversiMove(char color, int x, int y, bool test) {
        char t[8][8];

        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                t[i][j] = boardState[getBoardIndex(i, j)];
            }
        }

        char opponentColor = (color == 'b') ? 'w' : 'b';

        if (t[x][y] != 'e') {
            return false;
        }

        bool state[8] = { false };

        t[x][y] = color;

        int dirX[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
        int dirY[] = { -1, -1, 0, 1, 1, 1, 0, -1 };

        // Sprawdzenie dla każdego z ośmiu kierunków
        for (int dir = 0; dir < 8; dir++) {
            int i = 1;
            while (x + dirX[dir] * i >= 0 && x + dirX[dir] * i < 8 &&
                y + dirY[dir] * i >= 0 && y + dirY[dir] * i < 8 &&
                t[x + dirX[dir] * i][y + dirY[dir] * i] == opponentColor) {
                i++;
            }

            if (x + dirX[dir] * i >= 0 && x + dirX[dir] * i < 8 &&
                y + dirY[dir] * i >= 0 && y + dirY[dir] * i < 8 &&
                t[x + dirX[dir] * i][y + dirY[dir] * i] == color) {

                int nx = x + dirX[dir];
                int ny = y + dirY[dir];

                if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8 && t[nx][ny] == color) {
                    state[dir] = false;
                }
                else {
                    state[dir] = true;
                    modifyReversiBoard(color, x, y, dirX, dirY, dir, i, t);
                }
            }
        }

        bool flag = false;

        for (int i = 0; i < 8; i++) {
            flag |= state[i];
        }

        if (test) {
            return flag;
        }

        if (flag) {
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    boardState[getBoardIndex(i, j)] = t[i][j];
                }
            }
            return true;
        }

        return false;
    }

    // Liczenie pionków danego koloru w grze Reversi
    // Przeszukuje planszę i zlicza ilość pionków o danym kolorze.
    int countReversiPawns(char color) {
        int pawnCount = 0;
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                if (boardState[getBoardIndex(i, j)] == color) {
                    pawnCount++;
                }
            }
        }
        return pawnCount;
    }

    // Określanie zwycięzcy gry Reversi
    // Sprawdza ilość pionków każdego z graczy i zwraca odpowiedniego zwycięzcę lub remis.
    char determineReversiWinner() {
        int whitePawns = countReversiPawns('w');
        int blackPawns = countReversiPawns('b');

        if (whitePawns > blackPawns) {
            return 'w';
        }
        else if (whitePawns == blackPawns) {
            return 'd';
        }
        else {
            return 'b';
        }
    }

    // Aktualizacja tury gry Reversi
    // Aktualizuje turę gry Reversi na podstawie dostępnych ruchów.
    // Jeśli gracz nie ma dostępnych ruchów, zmienia turę na 'f' (koniec gry).
    char updateReversiTurn(ReversiGame& reversiBoard, char turn) {
        if (!reversiBoard.hasValidReversiMove('w') && !reversiBoard.hasValidReversiMove('b')) {
            return 'f';
        }
        else if (!reversiBoard.hasValidReversiMove('w') && turn == 'w') {
            return 'b';
        }
        else if (!reversiBoard.hasValidReversiMove('b') && turn == 'b') {
            return 'w';
        }

        return turn;
    }
};

// Inicjalizacja warunku i mutexu dla gry Reversi
pthread_cond_t reversiPlayerCondition;
pthread_mutex_t reversiMutex;

// Funkcja obsługująca pokój gry Reversi dla dwóch graczy
void* reversiGameRoom(void* parameters) {
    // Pobranie parametrów wątku Reversi
    ReversiThreadParams* threadParams = static_cast<ReversiThreadParams*>(parameters);

    // Inicjalizacja gniazd graczy i flagi oczekującego gracza
    int* lightPlayerSocket = threadParams->firstPlayerSocket;
    int* shadowPlayerSocket = threadParams->opponentPlayerSocket;
    int* waitingPlayerFlag = threadParams->waitingPlayerFlag;

    // Ustalenie gniazd dla graczy światła i cienia
    int reversiLightPlayer = *lightPlayerSocket;
    int reversiShadowPlayer;
    *waitingPlayerFlag = 1;

    // Inicjalizacja obiektu gry Reversi
    ReversiGame reversiGame('w', 'b');

    // Oczekiwanie na drugiego gracza
    pthread_mutex_lock(&reversiMutex);
    pthread_cond_wait(&reversiPlayerCondition, &reversiMutex);
    reversiShadowPlayer = *shadowPlayerSocket;
    *waitingPlayerFlag = 0;
    pthread_mutex_unlock(&reversiMutex);

    // Utworzenie mapy przechowującej gniazda przeciwnych graczy
    std::map<int, int> reversiOppositePlayersMap;
    reversiOppositePlayersMap[reversiShadowPlayer] = reversiLightPlayer;
    reversiOppositePlayersMap[reversiLightPlayer] = reversiShadowPlayer;

    try {
        // Wysłanie informacji o kolorze pionków dla każdego gracza
        if (sendToReversiSocket(reversiLightPlayer, "w", 1) < 0 || sendToReversiSocket(reversiShadowPlayer, "b", 1) < 0) {
            throw - 1;
        }

        // Wysłanie początkowego stanu planszy dla obu graczy
        if (sendToReversiSocket(reversiLightPlayer, reversiGame.boardState, 64) < 0 || sendToReversiSocket(reversiShadowPlayer, reversiGame.boardState, 64) < 0) {
            throw - 2;
        }

        // Inicjalizacja tury gry
        char reversiTurn[] = "w";
        while (true) {
            char* reversiMove;
            // Aktualizacja tury i wysłanie informacji o ruchu do obu graczy
            reversiTurn[0] = reversiGame.updateReversiTurn(reversiGame, reversiTurn[0]);
            if (sendToReversiSocket(reversiLightPlayer, reversiTurn, 1) < 0 || sendToReversiSocket(reversiShadowPlayer, reversiTurn, 1) < 0) {
                throw - 3;
            }

            // Zakończenie gry w przypadku braku możliwych ruchów
            if (reversiTurn[0] == 'f') {
                char reversiWinner = reversiGame.determineReversiWinner();
                char reversiResult[2] = { reversiWinner, '\0' };
                if (sendToReversiSocket(reversiShadowPlayer, reversiResult, 2) < 0 || sendToReversiSocket(reversiLightPlayer, reversiResult, 2) < 0) {
                    throw - 4;
                }
                break;
            }

            // Określenie aktualnego gracza
            char reversiCurrentPlayer = (reversiTurn[0] == 'w') ? 'w' : 'b';

            // Oczekiwanie na ruch od gracza
            while (true) {
                reversiMove = readFromReversiSocket((reversiCurrentPlayer == 'w') ? reversiLightPlayer : reversiShadowPlayer, 2);
                if (!reversiMove) {
                    throw - 5;
                }
                int reversiTmp = 10 * (reversiMove[0] - '0') + (reversiMove[1] - '0');

                // Sprawdzenie poprawności ruchu
                bool reversiDecision = reversiGame.isValidReversiMove(reversiCurrentPlayer, reversiTmp / 8, reversiTmp % 8, false);

                if (reversiDecision) {
                    // Wysłanie informacji o poprawnym ruchu
                    if (sendToReversiSocket((reversiCurrentPlayer == 'w') ? reversiLightPlayer : reversiShadowPlayer, "g", 1) < 0) {
                        throw - 6;
                    }
                    break;
                }
                else {
                    // Wysłanie informacji o błędnym ruchu
                    if (sendToReversiSocket((reversiCurrentPlayer == 'w') ? reversiLightPlayer : reversiShadowPlayer, "m", 1) < 0) {
                        throw - 7;
                    }
                }
            }

            // Wysłanie zaktualizowanego stanu planszy po ruchu
            if (sendToReversiSocket(reversiLightPlayer, reversiGame.boardState, 64) < 0 || sendToReversiSocket(reversiShadowPlayer, reversiGame.boardState, 64) < 0) {
                throw - 8;
            }

            // Zmiana tury
            reversiTurn[0] = (reversiTurn[0] == 'w') ? 'b' : 'w';
        }
    }
    catch (int reversiCaughtPlayer) {
        try {
            // Zamknięcie gniazda gracza, który wywołał wyjątek
            if (reversiOppositePlayersMap.find(reversiCaughtPlayer) != reversiOppositePlayersMap.end()) {
                std::cout << "error";
                close(reversiOppositePlayersMap[reversiCaughtPlayer]);
            }
        }
        catch (int reversiNestedPlayer) {
            return NULL;
        }
    }
    // Zwolnienie pamięci i zakończenie wątku
    delete threadParams;
    return NULL;
}

// Funkcja ustawiająca obsługę sygnału SIGPIPE
void setupReversiSignalHandler() {
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        exit(1);
    }
}

// Funkcja rozpoczynająca serwer gry Reversi
void startReversiServer(int PORT, int& lightPlayer, int& shadowPlayer, int& waitingPlayer) {
    while (true) {
        pthread_t thread[1];
        pthread_cond_init(&reversiPlayerCondition, NULL);
        pthread_mutex_init(&reversiMutex, NULL);
        int reversiSocket, reversiClientSocket;
        socklen_t reversiTmp;
        struct sockaddr_in reversiAddr, reversiClientAddr;
        memset(&reversiAddr, 0, sizeof(struct sockaddr));
        reversiAddr.sin_family = AF_INET;
        reversiAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        reversiAddr.sin_port = htons(PORT);
        reversiSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (reversiSocket < 0) {
            exit(1);
        }
        int reversiBind = bind(reversiSocket, (struct sockaddr*)&reversiAddr, sizeof(struct sockaddr));
        if (reversiBind < 0) {
            exit(1);
        }
        int reversiListen = listen(reversiSocket, WAITING_QUEUE);

        if (reversiListen < 0) {
            exit(1);
        }
        while (true) {
            reversiTmp = sizeof(struct sockaddr);
            reversiClientSocket = accept(reversiSocket, (struct sockaddr*)&reversiClientAddr, &reversiTmp);
            if (reversiClientSocket < 0) {
                exit(1);
            }
            pthread_mutex_lock(&reversiMutex);
            if (waitingPlayer == 0) {
                lightPlayer = reversiClientSocket;
                ReversiThreadParams* reversiArgs = new ReversiThreadParams{ &lightPlayer, &shadowPlayer, &waitingPlayer };
                pthread_create(&thread[0], NULL, &reversiGameRoom, static_cast<void*>(reversiArgs));
                pthread_mutex_unlock(&reversiMutex);
            }
            else {
                shadowPlayer = reversiClientSocket;
                pthread_mutex_unlock(&reversiMutex);
                pthread_cond_signal(&reversiPlayerCondition);
            }
        }
        close(reversiSocket);
    }
}

// Funkcja główna programu
int main(int argc, char* argv[]) {
    // Inicjalizacja zmiennych przechowujących gniazda graczy oraz flagę oczekującego gracza
    int lightPlayer = 0;
    int shadowPlayer = 0;
    int waitingPlayer = 0;

    // Ustawienie portu na podstawie argumentu wywołania lub wartości domyślnej
    if (argc > 1) {
        PORT_NUMBER = strtol(argv[1], NULL, 10);
    }

    // Konfiguracja obsługi sygnału SIGPIPE
    setupReversiSignalHandler();

    // Uruchomienie serwera gry Reversi
    startReversiServer(PORT_NUMBER, lightPlayer, shadowPlayer, waitingPlayer);

    return 0;
}