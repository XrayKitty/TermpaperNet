#include <SFML/Network.hpp>
#include "Command_Type.hpp"
#include <iostream>
#include <queue>
#include <thread>
#include <chrono>

struct Message {
    Command_type comand;
    unsigned short resiverId;
    std::string message;

    Message(Command_type com = Command_type::Null, unsigned short res = 0, std::string mess = "") {
        comand = com;
        resiverId = res;
        message = mess;
    }
};

class Client {
    sf::TcpSocket tcpSocket;
    sf::UdpSocket udpSocket;
    unsigned short portHello;
    std::queue<std::string> newMessage;
    sf::SocketSelector selector;
    bool connected;

public:
    Client(unsigned short port = 3000) : portHello(port), connected(false) {
        tcpSocket.setBlocking(false);
        selector.add(tcpSocket);
    }

    std::string GetNewMessage() {
        std::string message;
        while (!newMessage.empty()) {
            message += newMessage.front() + "\n";
            newMessage.pop();
        }
        return message;
    }

    bool IsConnect() { return connected; }
    size_t GetCountNewMessage() const { return newMessage.size(); }

    std::string Connect(std::string userName) {
        if (connected) return "Уже подключен";

        udpSocket.bind(sf::Socket::AnyPort);
        udpSocket.setBlocking(true);

        // Broadcast
        sf::Packet packet;
        packet << std::string("Hello");
        udpSocket.send(packet, sf::IpAddress::Broadcast, portHello);

        // Ждем ответ
        std::optional<sf::IpAddress> serverIp;
        unsigned short serverPort;
        packet.clear();
        if (udpSocket.receive(packet, serverIp, serverPort) != sf::Socket::Status::Done) {
            return "Нет ответа от сервера";
        }

        std::string response;
        unsigned short tcpPort;
        packet >> response >> tcpPort;
        if (response != "Hello") return "Неверный ответ";

        // TCP подключение
        tcpSocket.setBlocking(true);
        if (tcpSocket.connect(serverIp.value(), tcpPort) != sf::Socket::Status::Done) {
            return "Ошибка TCP подключения";
        }

        // Отправка имени
        packet.clear();
        packet << userName;
        if (tcpSocket.send(packet) != sf::Socket::Status::Done) {
            tcpSocket.disconnect();
            return "Ошибка отправки имени";
        }

        // Подтверждение
        packet.clear();
        if (tcpSocket.receive(packet) != sf::Socket::Status::Done) {
            tcpSocket.disconnect();
            return "Нет подтверждения";
        }

        packet >> response;
        if (response == "Ok") {
            connected = true;
            tcpSocket.setBlocking(false);
            return "Соединение установлено";
        }

        tcpSocket.disconnect();
        return "Отказ сервера";
    }

    std::string Disconected() {
        if (!connected) return "Не подключен";
        connected = false;
        tcpSocket.disconnect();
        return "Отключено";
    }

    std::string SendMessage(Message& message) {
        if (!connected) return "Не подключен";
        if (message.comand == Command_type::SendMessage && message.message.empty()) {
            return "Пустое сообщение";
        }

        sf::Packet packet;
        packet << static_cast<std::int8_t>(message.comand) << message.resiverId << message.message;

        if (tcpSocket.send(packet) == sf::Socket::Status::Done) {
            return "Отправлено";
        }
        return "Ошибка отправки";
    }

    std::string Update() {
        if (!connected) return "Не подключен";

        int receivedCount = 0;
        sf::Packet packet;

        // Проверяем входящие сообщения в цикле
        while (true) {
            sf::Socket::Status status = tcpSocket.receive(packet);
            if (status == sf::Socket::Status::Done) {
                std::string msg;
                packet >> msg;
                newMessage.push(msg);
                receivedCount++;
                packet.clear();
            }
            else if (status == sf::Socket::Status::NotReady) {
                // Нет больше данных для чтения
                break;
            }
            else if (status == sf::Socket::Status::Disconnected) {
                connected = false;
                return "Соединение разорвано";
            }
            else {
                // Другая ошибка
                break;
            }
        }

        if (receivedCount > 0) {
            return "Получено " + std::to_string(receivedCount) + " сообщений";
        }
        return "Нет новых сообщений";
    }
};

int main() {
    system("chcp 1251>nul");

    std::string userName;
    unsigned short port;

    std::cout << "Введите имя: ";
    std::cin >> userName;
    std::cout << "Порт: ";
    std::cin >> port;

    Client client(port);
    bool running = true;

    while (running) {
        std::cout << "\n=====================================\n"
            << "Пользователь: " << userName << "\n"
            << "Статус: " << (client.IsConnect() ? "В сети" : "Не в сети") << "\n"
            << "Сообщений: " << client.GetCountNewMessage() << "\n"
            << "=====================================\n"
            << "1. Подключиться\n"
            << "2. Отключиться\n"
            << "3. Список пользователей\n"
            << "4. Отправить сообщение\n"
            << "5. Заблокировать\n"
            << "6. Разблокировать\n"
            << "7. Показать сообщения\n"
            << "8. Обновить\n"
            << "0. Выход\n"
            << "Выбор: ";

        int choice;
        std::cin >> choice;

        switch (choice) {
        case 1:
            std::cout << client.Connect(userName) << std::endl;
            break;

        case 2:
            std::cout << client.Disconected() << std::endl;
            break;

        case 3: {
            if (!client.IsConnect()) {
                std::cout << "Не подключен!\n";
                break;
            }
            Message msg(Command_type::Users, 0, "");
            std::cout << "Запрос: " << client.SendMessage(msg) << std::endl;

            // Ждем ответ
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // Обновляем и получаем ответ
            std::cout << client.Update() << std::endl;

            std::string usersList = client.GetNewMessage();
            if (!usersList.empty()) {
                std::cout << "\n" << usersList << std::endl;
            }
            else {
                std::cout << "Нет ответа от сервера\n";
            }
            break;
        }

        case 4: {
            if (!client.IsConnect()) {
                std::cout << "Не подключен!\n";
                break;
            }
            unsigned short toId;
            std::string text;
            std::cout << "ID: ";
            std::cin >> toId;
            std::cin.ignore();
            std::cout << "Сообщение: ";
            std::getline(std::cin, text);

            Message msg(Command_type::SendMessage, toId, text);
            std::cout << client.SendMessage(msg) << std::endl;
            break;
        }

        case 5: {
            if (!client.IsConnect()) break;
            unsigned short id;
            std::cout << "ID для блокировки: ";
            std::cin >> id;
            Message msg(Command_type::Block, id, "");
            std::cout << client.SendMessage(msg) << std::endl;
            break;
        }

        case 6: {
            if (!client.IsConnect()) break;
            unsigned short id;
            std::cout << "ID для разблокировки: ";
            std::cin >> id;
            Message msg(Command_type::Unblock, id, "");
            std::cout << client.SendMessage(msg) << std::endl;
            break;
        }

        case 7:
            std::cout << client.GetNewMessage() << std::endl;
            break;

        case 8:
            if (!client.IsConnect()) {
                std::cout << "Не подключен!\n";
                break;
            }
            std::cout << client.Update() << std::endl;
            break;

        case 0:
            running = false;
            break;
        }
    }

    return 0;
}