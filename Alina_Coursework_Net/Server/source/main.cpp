#include <SFML/network.hpp>
#include "Command_Type.hpp"
#include <sstream>
#include <iostream>
#include <list>
#include <map>
#include <algorithm>

struct User {
    std::string name;
    unsigned short id;
    std::list<unsigned short> block_list;
    User() {}
    User(std::string name, unsigned short id) : name(name), id(id), block_list() {}
};

struct Message {
    Command_type command_type = Command_type::Null;
    std::string message = "";
    unsigned short receiver = 0, sender = 0;
};

class Server {
    sf::UdpSocket udpSocket;
    sf::TcpListener listener;
    sf::SocketSelector selector;
    std::map<unsigned short, User> users;
    std::map<unsigned short, sf::TcpSocket> users_socket;
    bool run;

    void AddUser(sf::TcpSocket& socket) {
        std::string user_name;
        sf::Packet packet;
        if (socket.receive(packet) == sf::Socket::Status::Done) {
            packet >> user_name;
            unsigned short clientPort = socket.getRemotePort();
            users[clientPort] = User(user_name, clientPort);
            packet.clear();
            packet << "Ok";
            socket.send(packet);
            std::cout << "Пользователь " << user_name << " (ID: " << clientPort << ") добавлен" << std::endl;

            // Выводим всех пользователей
            std::cout << "Текущие пользователи в сети:" << std::endl;
            for (auto& [id, u] : users) {
                std::cout << "  - " << u.name << " (ID: " << id << ")" << std::endl;
            }
        }
    }

    void NewClient() {
        sf::TcpSocket socket;
        if (listener.accept(socket) != sf::Socket::Status::Done) {
            std::cerr << "Ошибка подключения" << std::endl;
            return;
        }
        std::cout << "Новое подключение с адреса:" << socket.getRemoteAddress().value()
            << ':' << socket.getRemotePort() << std::endl;

        socket.setBlocking(false);
        AddUser(socket);

        selector.add(socket);
        users_socket[socket.getRemotePort()] = std::move(socket);
    }

    void HelloUdp() {
        std::optional<sf::IpAddress> receiveIp;
        unsigned short receivePort;
        sf::Packet packet;
        std::string message;

        if (udpSocket.receive(packet, receiveIp, receivePort) != sf::Socket::Status::Done) {
            return;
        }

        packet >> message;
        if (message != "Hello") return;

        packet.clear();
        packet << "Hello" << listener.getLocalPort();
        udpSocket.send(packet, receiveIp.value(), receivePort);
        std::cout << "Разрешение на подключение отправлено на " << receiveIp.value() << std::endl;
    }

    void ProcessClient(unsigned short user_id) {
        sf::TcpSocket& socket = users_socket[user_id];
        sf::Packet packet;

        if (socket.receive(packet) != sf::Socket::Status::Done) {
            return;
        }

        std::int8_t cmd;
        unsigned short receiver;
        std::string msg_text;
        packet >> cmd >> receiver >> msg_text;

        Command_type command = Command_type(cmd);
        std::cout << "Получена команда " << int(cmd) << " от " << user_id << std::endl;

        std::string response;

        switch (command) {
        case Command_type::Users: {
            std::stringstream ss;
            ss << "=== СПИСОК ПОЛЬЗОВАТЕЛЕЙ ===\n";
            for (auto& [id, user] : users) {
                ss << user.name << " (ID: " << id << ")\n";
            }
            response = ss.str();
            break;
        }
        case Command_type::Block: {
            if (users.count(receiver)) {
                users[user_id].block_list.push_back(receiver);
                response = "Пользователь " + std::to_string(receiver) + " заблокирован";
            }
            else {
                response = "Пользователь не найден";
            }
            break;
        }
        case Command_type::Unblock: {
            if (users.count(receiver)) {
                users[user_id].block_list.remove(receiver);
                response = "Пользователь " + std::to_string(receiver) + " разблокирован";
            }
            else {
                response = "Пользователь не найден";
            }
            break;
        }
        case Command_type::SendMessage: {
            if (users.count(receiver)) {
                // Проверка блокировки
                bool blocked = false;
                for (int b : users[receiver].block_list) {
                    if (b == user_id) blocked = true;
                }

                if (blocked) {
                    response = "Вы заблокированы этим пользователем";
                }
                else {
                    sf::Packet msgPacket;
                    std::string fullMsg = users[user_id].name + ": " + msg_text;
                    msgPacket << fullMsg;
                    users_socket[receiver].send(msgPacket);
                    response = "Сообщение отправлено";
                }
            }
            else {
                response = "Пользователь не найден";
            }
            break;
        }
        default:
            response = "Неизвестная команда";
        }

        // Отправляем ответ
        sf::Packet respPacket;
        respPacket << response;
        socket.send(respPacket);
        std::cout << "Ответ отправлен: " << response << std::endl;
    }

public:
    Server(unsigned short udpPort, unsigned short listenerPort) : run(true) {
        udpSocket.bind(udpPort);
        listener.listen(listenerPort);

        udpSocket.setBlocking(false);
        listener.setBlocking(false);

        selector.add(udpSocket);
        selector.add(listener);

        std::cout << "Сервер запущен" << std::endl;
    }

    void Run() {
        while (run) {
            selector.wait();

            if (selector.isReady(udpSocket)) {
                HelloUdp();
            }

            if (selector.isReady(listener)) {
                NewClient();
            }

            for (auto& [id, socket] : users_socket) {
                if (selector.isReady(socket)) {
                    ProcessClient(id);
                }
            }
        }
    }
};

int main() {
    system("chcp 1251>nul");
    Server server(3000, 3001);
    server.Run();
    return 0;
}