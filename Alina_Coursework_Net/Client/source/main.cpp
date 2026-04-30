#include <SFML/Network.hpp>
#include "Command_Type.hpp"
#include <iostream>
#include <queue>

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

    // Функция вернет текст сообщения полученного от сервера в случае ошибки вернет пустую строку
    std::string ResiveMessage() {
        std::string message;
        sf::Packet packet;
        sf::Socket::Status status;
        status = tcpSocket.receive(packet);
        if (status == sf::Socket::Status::Disconnected) return "";
        if (status == sf::Socket::Status::Error) return "";

        packet >> message;
        return message;
    }

public:
    Client(unsigned short port = 3000): portHello(port) {
        selector.add(tcpSocket);
    }

    std::string GetNewMessage() {
        std::string message;
        while (!newMessage.empty()) {
            std::string mess = newMessage.front();
            message += mess + '\n';
            newMessage.pop();
        }
        return message;
    }

    bool IsConnect() {
        return tcpSocket.getRemotePort();
    }

    size_t GetCountNewMessage() const { return newMessage.size(); }

    std::string Connect(std::string userName) {
        // проверка на то что соединение уже установлено
        if (IsConnect()) return "Подключесние установлено!";

        // Служебные переменные
        sf::Socket::Status status;
        sf::Packet packet;
        std::string message;
        unsigned short port;
        std::optional<sf::IpAddress> ipAddress;

        // Привязка сокета
        udpSocket.bind(sf::Socket::AnyPort);

        // Отправка приветственного сообщения серверу
        message = "Hello";
        packet << message;

        uint32_t ip = sf::IpAddress::getLocalAddress().value().toInteger();
        std::cout << "Ip адрес компьютера:" << sf::IpAddress::getLocalAddress().value() << std::endl;
        uint32_t mask = 0;
        uint32_t broadcast;
        switch (ip >> 24) {
        case 192: mask = 0xffffff00; break;
        case 172: mask = 0xffff0000; break;
        //case 10: mask = 0xff000000; break;
        case 10: mask = 0xfffff000; break;
        default: mask = 0xffffffff;
        }

        broadcast = ip | ~mask;
        std::cout << sf::IpAddress::IpAddress(broadcast).toString() << std::endl;
        status = udpSocket.send(packet, sf::IpAddress::IpAddress(broadcast), portHello);
        if (status != sf::Socket::Status::Done) return "Серверу не удалось отправить привественное сообщение";

        // Получение приветствия от сервера и его проверка
        packet.clear();
        status = udpSocket.receive(packet, ipAddress, port);
        if (status != sf::Socket::Status::Done) return "Не удалось получить приветственное сообщение от сервера";
        packet >> message >> port;
        if (message != "Hello") return "Сервер отправил некорректное приветствие";

        // Подключение к серверу
        status = tcpSocket.connect(ipAddress.value(), port);
        if (status != sf::Socket::Status::Done) return "Ошибка при подключении к серверу";

        // отпарвка имени пользователя
        packet.clear();
        packet << userName;
        status = tcpSocket.send(packet);
        if (status != sf::Socket::Status::Done) {
            tcpSocket.disconnect();
            return "Серверу не удалось отправить имя пользователя";
        }
        // получение подтверждения от сервера что имя пользователя корректно
        status = tcpSocket.receive(packet);
        if (status != sf::Socket::Status::Done) {
            tcpSocket.disconnect();
            return "Имя пользователя получено некорректно";
        }
        packet >> message;
        if (message == "Ok") return "Соединение установлено"; 
        tcpSocket.disconnect();
        return "Сообщение неккоректно";    }

    std::string Disconected() {
        if (!IsConnect()) {
            return "Сервер не подключен";
        }
        tcpSocket.disconnect();
        return "Отключение успешно выполнено";
    }

    std::string SendMessage(Message& message) {
        if (message.message.empty()) return "Вы не можете отправить пустое сообщение";
        sf::Packet packet;
        sf::Socket::Status status;
        packet << std::int8_t(message.comand) << message.resiverId << message.message;
        status = tcpSocket.send(packet);

        if (status == sf::Socket::Status::Error) return "Произошла неизвестная ошибка при отправке";
        if (status == sf::Socket::Status::Disconnected) return "Соединение было разорвано при отправке";
        return "Сообщение успешно доставлено";
    }

    std::string Update() {
        while (selector.isReady(tcpSocket)) {
            std::string message = ResiveMessage();
            if (message.empty()) return "Ошибка получения сообщения";
            newMessage.push(message);
        }
        return "Успешно полученный сообщения";
    }
};

int main()
{
    //Комментарий Ивана и Алины
    system("chcp 1251>nul");

    std::string userName;
    unsigned short port;
    bool run = true;

    std::cout << "Введите ваше имя полтзователя >";
    std::cin >> userName;
    std::cout << "Введите порт на который хотите подключиться >";
    std::cin >> port;

    Client user(port);
    while(run) {
        int a;
        std::cout << "Имя пользователя: " << userName << std::endl
            << "Статус подключения: " << (user.IsConnect() ? "В сети" : "Не в сети") << std::endl
            << "Колличество новых сообщений: " << user.GetCountNewMessage() << std::endl
            << std::endl << "Меню:" << std::endl
            << "1. Отправить сообщение" << std::endl
            << "2. Непрачитанные сообщения" << std::endl
            << "3. Заблокировать" << std::endl
            << "4. Разблокировать" << std::endl
            << "5. Обновить" << std::endl
            << "6. Подключиться" << std::endl
            << "7. Отключиться" << std::endl
            << "8. Список пользователей" << std::endl
            << "0. Выйти" << std::endl;
        std::cin >> a;

        unsigned short userId;
        std::string message;
        Message mess;

        switch (a) {
        case 1:
            if (!user.IsConnect()) {
                std::cout << "Вы не подключены!" << std::endl;
                break;
            }
            std::cout << "Введите id пользователя, которому хотите отправить сообщение";
            std::cin >> userId;
            std::cout << "Введите ваше сообщение" << std::endl;
            std::getline(std::cin, message);
            std::getline(std::cin, message);

            mess = { Command_type::SendMessage, userId, message };
            std::cout << user.SendMessage(mess) << std::endl;
            break;
        case 2:
            if (!user.IsConnect()) {
                std::cout << "Вы не подключены!" << std::endl;
                break;
            }
            std::cout << "У вас " << user.GetCountNewMessage()<< " новых сообщений" << std::endl;
            std::cout << user.GetNewMessage() << std::endl;
            break;
        case 3:
            if (!user.IsConnect()) {
                std::cout << "Вы не подключены!" << std::endl;
                break;
            }
            std::cout << "Введите ID пользователя, которого хотите заблокировать >";
            std::cin >> userId;
            mess = { Command_type::Block, userId, "" };
            std::cout << user.SendMessage(mess) << std::endl;
            break;
        case 4:
            if (!user.IsConnect()) {
                std::cout << "Вы не подключены!" << std::endl;
                break;
            }
            std::cout << "Введите ID пользователя, которого хотите расблокировать >";
            std::cin >> userId;
            mess = { Command_type::Unblock, userId, "" };
            std::cout << user.SendMessage(mess) << std::endl;
            break;
        case 5:
            if (!user.IsConnect()) {
                std::cout << "Вы не подключены!" << std::endl;
                break;
            }
            std::cout << user.Update() << std::endl;
            break;
        case 6:
            std::cout << user.Connect(userName) << std::endl;
            break;
        case 7:
            std::cout << user.Disconected() << std::endl;
            break;
        case 8:
            if (!user.IsConnect()) {
                std::cout << "Вы не подключены!" << std::endl;
                break;
            }
            mess = { Command_type::Users, 0 ,"" };
            std::cout << user.SendMessage(mess) << std::endl;
            break;
        case 0:
            run = false;
            break;
        }
    }
}