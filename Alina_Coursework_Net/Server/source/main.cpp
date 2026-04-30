#include <SFML/network.hpp>
#include "Command_Type.hpp"
#include <sstream>
#include <iostream>
#include <list>

struct User {
	std::string name;
	unsigned short id;
	std::list<unsigned short> block_list;
	User() {}
	User(std::string name, unsigned short id): name(name), id(id), block_list() {}
};

struct Message {
	Command_type command_type = Command_type::Null;
	std::string message = "";
	unsigned short receiver = 0, sender = 0;
	std::string GetMessage() const {
		return message;
	}
	friend sf::Packet& operator<< (sf::Packet& packet, Message& message);
	friend sf::Packet& operator>> (sf::Packet& packet, Message& message);
};

sf::Packet& operator<< (sf::Packet& packet, Message& message) {
	return packet << message.message;
}

sf::Packet& operator>> (sf::Packet& packet, Message& message) {
	std::int8_t command_type;
	packet >> command_type >> message.receiver >> message.message;
	message.command_type = Command_type(command_type);
	return packet;
}

class Server {
	sf::UdpSocket udpSocket;
	sf::TcpListener listener;
	sf::SocketSelector selector;
	std::map<unsigned short, User> users;
	std::map<unsigned short, sf::TcpSocket> users_socket;
	bool run;

	void BlockingSocket(sf::Socket& socket) {
		socket.setBlocking(true);
	}

	void AddSocketSelector(sf::Socket& socket) {
		selector.add(socket);
	}

	void AddUser(sf::TcpSocket& socket) {
		std::string user_name;
		sf::Packet packet;
		if (socket.receive(packet) == sf::Socket::Status::Done) {
			packet >> user_name;
			users[socket.getLocalPort()] = User(user_name, socket.getLocalPort());
			packet.clear();
			packet << "Ok";
			socket.send(packet);
		}
	}

	bool IsUserIdExist(unsigned short userId) {
		return users.count(userId);
	}

	void NewClient() {	
		sf::TcpSocket socket;
		if (listener.accept(socket) != sf::Socket::Status::Done) {
			std::cerr << "Ошибка подключения с адреса:" << socket.getRemoteAddress().value() << ':' << socket.getRemotePort() << std::endl;
			return;
		}
		std::cout << "Новое подключение с адреса:" << socket.getRemoteAddress().value() << ':' << socket.getRemotePort() << std::endl;
		BlockingSocket(socket);
		AddUser(socket);

		selector.add(socket);
		users_socket[socket.getLocalPort()] = std::move(socket);

	}

	void HelloUdp() {
		std::optional<sf::IpAddress> receiveIp;
		unsigned short receivePort;
		sf::Packet packet;
		std::string message;

		if (udpSocket.receive(packet, receiveIp, receivePort) != sf::Socket::Status::Done) {
			std::cerr << "Не удалось получить пакет на UDP" << std::endl;
			return;
		}

		packet >> message;
		if (message != "Hello")
			return;

		packet.clear();
		packet << "Hello";
		packet << listener.getLocalPort();
		if (udpSocket.send(packet, receiveIp.value(), receivePort) == sf::Socket::Status::Done)
			std::cout << "Разрешение на подклчюение отправлено" << std::endl;
		else
			std::cerr << "Не удалось отправить ответный пакет на адрес " << receiveIp.value() << ':' << receivePort << std::endl;
	}

	Message ReceiveMessage(unsigned short user_id) {
		sf::TcpSocket& socket = users_socket[user_id];
		sf::Packet packet;
		Message message;
		sf::Socket::Status status;
		status = socket.receive(packet);

		if (status == sf::Socket::Status::Disconnected) {
			selector.remove(socket);
			users_socket.erase(user_id);
			users.erase(user_id);
			return Message();
		}
		if (status == sf::Socket::Status::Error) {
			std::cerr << "Ошибка получения пакета от адреса " << socket.getRemoteAddress().value() << ':' << socket.getRemotePort() << std::endl;
			return Message();
		}

		packet >> message;
		message.sender = socket.getRemotePort();
		return message;
	}

	std::string ComandHelp(Message& mess) {
		std::stringstream sstring;
		sstring << "/help: Выводит справку по командам." << std::endl
			<< "/users: Выводит список пользователей." << std::endl
			<< "/block [id]: Блокирует пользователя по id." << std::endl
			<< "/unblock [id]: Разблокирует пользователя по id." << std::endl
			<< "/disconnect: Отключение от сервера и завершение работы" << std::endl;
		return sstring.str();
	};
	std::string ComandUsers(Message& mess) {
		std::stringstream sstr;
		for (auto [user_id, user] : users)
			sstr << user.name << ":" << user_id << std::endl;
		return sstr.str();
	}
	std::string ComandBlock(Message& mess) {
		if (!IsUserIdExist(mess.receiver)) return "Такого пользователя не существует.";
		if (!users.count(mess.receiver)) return "Пользователь не найден";
		for (auto i : users[mess.receiver].block_list)
			if (i == mess.sender) return "Пользователь уже заблокирован";
		users[mess.receiver].block_list.push_back(mess.sender);
		return "Пользователь успешно заблокирован";
	}
	std::string ComandUnBlock(Message& mess) {
		if (!IsUserIdExist(mess.receiver)) return "Такого пользователя не существует.";
		std::list<unsigned short>::iterator iter;
		User user = users[mess.receiver];
		iter = std::find(user.block_list.begin(), user.block_list.end(), mess.sender);
		if (iter == user.block_list.end())
			return "Пользователь небыл заблокирован";
		user.block_list.erase(iter);
		return "Пользователь успешно разблокирован";
	}
	std::string ComandMessage(Message& mess) {
		if (!IsUserIdExist(mess.receiver)) return "Такого пользователя не существует.";
		for (auto i : users[mess.sender].block_list)
			if (i == mess.receiver) return "Вы не можете писать данному пользователю";
		sf::Socket::Status status;
		sf::Packet packet;
		packet << mess.sender << ':' << mess.message;
		status = users_socket[mess.receiver].send(packet);

		switch (status) {
		case sf::Socket::Status::Done: return "Сообщение успешно доставлено";
		case sf::Socket::Status::Error: return "Ошибка при отправке сообщения";
		}
	};

	std::string ProcesiingMessage(Message& ReceiveMessage) {
		std::string (Server::*processing) (Message&) = nullptr;
		switch (ReceiveMessage.command_type) {
		case Command_type::Help: processing = &Server::ComandHelp; break;
		case Command_type::Users: processing = &Server::ComandUsers; break;
		case Command_type::Block: processing = &Server::ComandBlock; break;
		case Command_type::Unblock: processing = &Server::ComandUnBlock; break;
		case Command_type::SendMessage: processing = &Server::ComandMessage; break;
		case Command_type::Null: return ""; break;
		}
		return (this->*processing)(ReceiveMessage);
	}

	void SendMessage(sf::TcpSocket& socket, std::string& message) {
		if (!message.size()) return;
		sf::Packet packet;
		packet << message;
		socket.send(packet);
	}
public:
	Server(unsigned short udpPort, unsigned short listenerPort): run(true) {
		BlockingSocket(udpSocket);
		if (udpSocket.bind(udpPort) != sf::Socket::Status::Done) {
			std::cerr << "Ошибка привязки UDP сокета на порт " << udpPort << std::endl;
			exit(-1);
		}

		BlockingSocket(listener);
		if (listener.listen(listenerPort) != sf::Socket::Status::Done) {
			std::cerr << "Ошибка привязки TCP слушателя на порт " << listenerPort << std::endl;
			exit(-1);
		}

		AddSocketSelector(udpSocket);
		AddSocketSelector(listener);
	}

	~Server() {
		for (auto& [user_id, socket] : users_socket) {
			socket.disconnect();
		}
		udpSocket.unbind();
		listener.close();
	}

	void Run() {
		while (run)
			if (selector.wait()) {
				if (selector.isReady(listener)) {
					std::cout << "Кто-то пытается подключиться к слушателю" << std::endl;
					NewClient();
					continue;
				}
				if (selector.isReady(udpSocket)) {
					std::cout << "Что-то пришло на UDP" << std::endl;
					HelloUdp();
					continue;
				}
				for (auto& [user_id, socket] : users_socket) {
					if (selector.isReady(socket)) {
						std::cout << "Пакет на сокете:" << user_id << std::endl;
						Message receive_mess = ReceiveMessage(user_id);
						std::string send_mess = ProcesiingMessage(receive_mess);
						SendMessage(socket, send_mess);
						break;
					}
				}
			}
	}
};

int main() {
	system("chcp 1251>nul");
	Server server(3000, 3001);
	server.Run();
}