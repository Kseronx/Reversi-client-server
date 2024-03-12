#include<SFML/Graphics.hpp>
#include<SFML/Window.hpp>
#include<SFML/System.hpp>
#include<utility>
#include<iostream>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <windows.h>
#include <windows.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <ws2tcpip.h>
#include <errno.h>
#include <vector>
#pragma comment(lib, "ws2_32.lib")


// Inicjalizacja wątku 
std::thread communicationThread;
bool isThreadEnding = false;

// Adres IP serwera do połączenia
const char* ipAddress = "192.168.1.31";

// Deskryptor gniazda klienta
int socketDescriptor;

// Funkcja dostosowująca do własnych potrzeb odczyt danych z gniazda
char* customRead(int size) {
    // Inicjalizacja bufora i zmiennych pomocniczych
    char* buffer = new char[size];
    int remainingBytes = size;
    char* currentPosition = buffer;

    try {
        // Odbieranie danych z gniazda w pętli
        while (remainingBytes > 0) {
            int received = recv(socketDescriptor, currentPosition, remainingBytes, 0);

            // Obsługa błędów odbioru danych
            if (received <= 0) {
                throw std::string("Error, connection not working");
            }

            remainingBytes -= received;
            currentPosition += received;
        }
    }
    catch (const std::string& exception) {
        // Zwolnienie zaalokowanej pamięci w przypadku błędu
        delete[] buffer;
        throw exception;
    }

    return buffer;
}

// Funkcja dostosowująca do własnych potrzeb zapis danych do gniazda
void customWrite(int size, const char* message) {
    // Inicjalizacja zmiennych pomocniczych
    int remainingBytes = size;
    const char* currentPosition = message;

    try {
        // Wysyłanie danych do gniazda w pętli
        while (remainingBytes > 0) {
            int sent = send(socketDescriptor, currentPosition, remainingBytes, 0);

            // Obsługa błędów wysyłania danych
            if (sent < 0) {
                closesocket(socketDescriptor);
                throw std::string("Error, connection not working");
            }

            remainingBytes -= sent;
            currentPosition += sent;
        }
    }
    catch (const std::string& exception) {
        throw exception;
    }
}

// Klasa reprezentująca grę Reversi
class ReversiGame {
public:
    // Okno graficzne sfml
    sf::RenderWindow gameWindow;

    // Plansza gry Reversi
    int gameBoard[8][8];

    // Wymiary okna gry
    int windowWidth = 640;
    int windowHeight = 640;

    // Flaga testowa
    bool isTest1 = false;

    // Flaga informująca o zakończeniu gry
    bool isGameOver;

    // Kolor gracza
    char playerColor;

    // Mutex do synchronizacji wątku
    std::mutex communicationMutex;

    // Zmienna przechowująca zwycięzcę gry
    char gameWinner;

    // Flaga informująca o kolejce danego gracza
    bool isPlayerTurn;

    // Zmienna przechowująca tymczasowy ruch gracza
    int temporaryMove;

    // Flaga informująca o zatrzymaniu gry
    bool isStop = false;

    // Konstruktor klasy ReversiGame
    ReversiGame() {
        // Inicjalizacja planszy gry Reversi
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                gameBoard[i][j] = 2; // null
            }
        }
        gameBoard[3][3] = 1; // biały
        gameBoard[4][4] = 1;
        gameBoard[3][4] = 0; // czarny
        gameBoard[4][3] = 0;
        isGameOver = false;
    }

    // Funkcja wysyłająca ruch gracza do serwera
    void sendPlayerMove(int move) {
        const int bufferSize = 2;
        char buffer[bufferSize];

        // Konwersja ruchu na format znakowy i wysłanie do serwera
        buffer[0] = static_cast<char>(move / 10) + '0';
        buffer[1] = static_cast<char>(move % 10) + '0';
        customWrite(bufferSize, buffer);
    }

    // Funkcja odbierająca stan planszy od serwera
    void receiveBoardState() {
        char* receivedData = customRead(64);

        // Konwersja otrzymanych danych na stan planszy
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                switch (receivedData[i * 8 + j]) {
                case 'e':
                    gameBoard[i][j] = 2;
                    break;
                case 'w':
                    gameBoard[i][j] = 1;
                    break;
                case 'b':
                    gameBoard[i][j] = 0;
                    break;
                }
            }
        }

        delete receivedData;
    }

    // Funkcja odbierająca informację o turze od serwera
    void receiveTurnInfo() {
        char* receivedData = customRead(1);

        try {
            // Sprawdzenie, czy teraz jest tura danego gracza
            if (receivedData[0] == playerColor) {
                isPlayerTurn = true;
            }
            else if (receivedData[0] != 'f') {
                isPlayerTurn = false;
            }
            else {
                isPlayerTurn = false;
                isGameOver = true;
            }
        }
        catch (const std::string& exception) {
            delete[] receivedData;
            throw exception;
        }

        delete[] receivedData;
    }

    // Funkcja rysująca planszę gry w oknie sfml
    void drawGameBoard() {
        // Czyszczenie okna gry
        gameWindow.clear(sf::Color::Black);

        // Inicjalizacja kształtu prostokąta reprezentującego pojedyncze pole planszy
        sf::RectangleShape cellShape;
        cellShape.setSize(sf::Vector2f((windowWidth / 8.0) - 1.0, (windowHeight / 8.0) - 1.0));
        cellShape.setFillColor(sf::Color::Magenta);
        cellShape.setOutlineColor(sf::Color::Cyan);

        // Inicjalizacja kształtu koła reprezentującego pionek
        sf::CircleShape blackCircle(25.0);
        blackCircle.setFillColor(sf::Color::Black);
        blackCircle.setOutlineColor(sf::Color::Black);

        sf::CircleShape whiteCircle(25.0);
        whiteCircle.setFillColor(sf::Color::White);
        whiteCircle.setOutlineColor(sf::Color::White);

        // Rysowanie planszy gry
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                cellShape.setPosition(sf::Vector2f(static_cast<float>(j * (windowWidth / 8)), static_cast<float>(i * (windowHeight / 8))));
                gameWindow.draw(cellShape);

                // Rysowanie pionków na planszy
                if (gameBoard[i][j] == 0) // czarny
                {
                    blackCircle.setPosition(sf::Vector2f(static_cast<float>(j * (windowWidth / 8) + 15), static_cast<float>(i * (windowWidth / 8) + 15)));
                    gameWindow.draw(blackCircle);
                }
                else if (gameBoard[i][j] == 1) // biały
                {
                    whiteCircle.setPosition(sf::Vector2f(static_cast<float>(j * (windowWidth / 8) + 15), static_cast<float>(i * (windowWidth / 8) + 15)));
                    gameWindow.draw(whiteCircle);
                }
            }
        }

        // Wyświetlenie okna gry
        gameWindow.display();
    }

    // Funkcja obsługująca pętlę gry Reversi
    void gameLoop() {
        try {
            // Inicjalizacja okna gry SFML
            gameWindow.create(sf::VideoMode(windowWidth, windowHeight), "Reversi");
            drawGameBoard();

            while (true) {
                // Obsługa zakończenia gry
                if (isGameOver) {
                    shutdown(socketDescriptor, SD_BOTH);
                    WSACleanup();
                    gameWindow.close();
                    sf::RenderWindow endWindow(sf::VideoMode(400, 400), "GAME OVER, GREEN = WIN, RED = DEFEAT");
                    endWindow.setFramerateLimit(10);
                    sf::Color backgroundColor;

                    // Ustalenie koloru tła w zależności od wyniku gry
                    if (gameWinner == playerColor) {
                        backgroundColor = sf::Color::Green;
                    }
                    else if (gameWinner == 'd') {
                        backgroundColor = sf::Color::Black;
                    }
                    else {
                        backgroundColor = sf::Color::Red;
                    }

                    // Rysowanie ekranu końcowego
                    sf::RectangleShape background(sf::Vector2f(400, 400));
                    background.setPosition(0, 0);
                    background.setFillColor(backgroundColor);
                    endWindow.clear();
                    endWindow.draw(background);
                    endWindow.display();
                    sf::Event endEvent;

                    // Oczekiwanie na zamknięcie okna końcowego
                    while (endWindow.isOpen()) {
                        while (endWindow.pollEvent(endEvent)) {
                            if (endEvent.type == sf::Event::Closed) {
                                endWindow.close();
                            }
                        }
                    }
                    break;
                }

                // Obsługa zdarzeń w grze
                handleGameEvents();
                drawGameBoard();

                // Obsługa zakończenia wątku komunikacyjnego
                if (isThreadEnding) {
                    shutdown(socketDescriptor, SD_BOTH);
                    WSACleanup();
                    return;
                }
            }
        }
        catch (std::string e) {
            std::cout << "Game loop error";
        }
    }

    // Funkcja obsługująca zdarzenia w grze
    void handleGameEvents() {
        sf::Event event;
        while (gameWindow.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                isThreadEnding = true;
                shutdown(socketDescriptor, SD_BOTH);
                WSACleanup();
                gameWindow.close();
                isStop = true;
                break;
            }

            // Obsługa ruchu myszką
            if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                sf::Vector2i mousePosition = sf::Mouse::getPosition(gameWindow);

                int x = mousePosition.x;
                int y = mousePosition.y;

                int cellSizeX = windowWidth / 8;
                int cellSizeY = windowHeight / 8;

                // Przeliczenie współrzędnych na indeksy planszy
                if (x >= 0 && x <= windowWidth && y >= 0 && y <= windowHeight) {
                    int move_x = x / cellSizeX;
                    int move_y = (y / cellSizeY) * 8;

                    temporaryMove = move_x + move_y;
                    isTest1 = true;
                }
            }
        }
    }

    // Funkcja obsługująca pętlę komunikacyjną
    void communicationLoop() {
        try {
            // Inicjalizacja WinSock
            WSADATA wsaData;
            struct sockaddr_in socketAddress;

            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                throw std::string("WSAStartup failed");
            }

            memset(&socketAddress, 0, sizeof(socketAddress));
            socketAddress.sin_family = AF_INET;
            inet_pton(AF_INET, ipAddress, &socketAddress.sin_addr);
            socketAddress.sin_port = htons(1234);

            // Utworzenie gniazda i nawiązanie połączenia
            if ((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                throw std::string("Error creating socket");
            }

            if (connect(socketDescriptor, (struct sockaddr*)&socketAddress, sizeof socketAddress) < 0) {
                throw std::string("Connection error");
            }

            // Odczyt koloru przypisanego graczowi
            char* colorBuffer = customRead(1);

            try {
                switch (colorBuffer[0]) {
                case 'b':
                    playerColor = 'b';
                    break;
                case 'w':
                    playerColor = 'w';
                    break;
                default:
                    throw std::string("Invalid color received");
                }
            }
            catch (const std::string& exception) {
                delete[] colorBuffer;
                throw exception;
            }

            delete[] colorBuffer;

            // Inicjalizacja stanu planszy i informacji o turze
            receiveBoardState();
            receiveTurnInfo();

            while (true) {
                // Obsługa ruchu gracza i odbioru informacji od serwera
                if (isPlayerTurn && !isGameOver) {
                    while (true) {
                        sendPlayerMove(temporaryMove);

                        try {
                            char* moveBuffer = customRead(1);
                            bool flag = (moveBuffer[0] == 'g');
                            delete[] moveBuffer;

                            if (flag) {
                                break;
                            }
                            else if (isStop) {
                                return;
                            }
                        }
                        catch (const std::string& exception) {
                            std::cout << exception << std::endl;
                            return;
                        }
                    }
                }
                else if (isGameOver) {
                    // Odbiór informacji o wyniku gry
                    char* winnerBuffer = customRead(1);
                    gameWinner = winnerBuffer[0];
                    delete[] winnerBuffer;
                    break;
                }

                // Obsługa zakończenia wątku gry
                if (isThreadEnding) {
                    std::cout << "Closing thread in game loop\n";
                    shutdown(socketDescriptor, SD_BOTH);
                    WSACleanup();
                    return;
                }
                // Odbiór stanu planszy i informacji o turze
                receiveBoardState();
                receiveTurnInfo();
            }
        }
        catch (const std::string& e) {
            std::cout << "Connection lost";
        }
    }
};
    int main(int argc, char* argv[]) {
        if (argc > 1) {
            ipAddress = argv[1];
        }

        ReversiGame reversiGame;
        communicationThread = std::thread(&ReversiGame::communicationLoop, std::ref(reversiGame));
        reversiGame.gameLoop();

        while (true) {
            // Oczekiwanie na zakończenie wątku komunikacyjnego
            reversiGame.communicationMutex.lock();
            if (isThreadEnding == true) {
                reversiGame.communicationMutex.unlock();
                communicationThread.join();
                break;
            }
            reversiGame.communicationMutex.unlock();
        }

        return 0;
    }