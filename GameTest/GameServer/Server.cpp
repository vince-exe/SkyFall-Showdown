#include "Server.h"

Server::Server(unsigned int tcpPort, unsigned int udpPort, unsigned int maxConnections, unsigned int clearUselessThreadsTime, unsigned int udpRequestTimeout, unsigned int threadsNumber) {
	m_acceptorPtr = std::make_unique<tcp::acceptor>(m_ioService, tcp::endpoint(tcp::v4(), tcpPort));
	m_udpServerSocket = std::make_shared<udp::socket>(m_ioService, udp::endpoint(udp::v4(), udpPort));

	m_udpPort = udpPort;
	m_maxConnections = maxConnections;
	m_doRoutines = true;
	m_clearUselessThreadsTime = clearUselessThreadsTime; 
	m_udpRequestTimeout = udpRequestTimeout;

	for (int i = 0; i < threadsNumber; ++i) {
		m_threadPool.emplace_back([this]() {
			processUDPMessages();
		});
	}

	std::cout << "\nSuccessfully created a ThreadPool of: " << threadsNumber;
}

void Server::listenUDPConnections() {
	std::cout << "\nStarted listening for UDP connections\n";
	while (true) {
		std::shared_ptr<udp::endpoint> remoteEndpoint = std::make_shared<udp::endpoint>(udp::v4(), m_udpPort);
		try {
			NetPacket packet = NetUtils::Udp::read_(*m_udpServerSocket, *remoteEndpoint);
			if (packet.getMsgType() == NetPacket::NetMessages::CONNECTION_UDP_MESSAGE) {
				std::string nick(reinterpret_cast<const char*>(&packet.getData()[0]), packet.getDataSize());

				m_udpConnectionsMap.insert(nick, std::pair<bool, std::shared_ptr<boost::asio::ip::udp::endpoint>>(true, remoteEndpoint));
				m_udpConnectionsCv.notify_all();
			}
			else if (packet.getMsgType() == NetPacket::NetMessages::GAME_UDP_MESSAGE) {
				m_udpMessagesQueue.push(std::make_shared<NetPacket>(packet));
				m_threadPoolCv.notify_all();
			}
		}
		catch (UdpUtils::errors::InvalidBytesLength& e) {
			std::cerr << e.what();
		}
		catch (boost::system::system_error& e) {
			std::cerr << "\nBoost system error: " << e.what();
		}
	}
}

void Server::processUDPMessages() {
	while (true) {
		std::unique_lock<std::mutex> lock(m_threadPoolMtx);
		m_threadPoolCv.wait(lock, [this] { return !m_udpMessagesQueue.empty(); });

		if (!m_udpMessagesQueue.empty()) {
			std::shared_ptr<NetPacket> packet = m_udpMessagesQueue.front();
			m_udpMessagesQueue.pop();

			lock.unlock();

			UdpUtils::GameMessage message = UdpUtils::deserializeUDPMessage(packet->getData());
			m_gameSessionsMap.get(message.m_gameSessionID)->handleUDPMessage(message, packet);
		}
	}
}

bool Server::waitUDPConnection(std::string& nick) {
	std::cout << "\nAspettando una UDP-CONNECTION sul nickname: " << nick;
	bool success;
	std::mutex mutex;

	std::thread waitThread([this, &nick, &success, &mutex] {
		while (true) {
			std::unique_lock<std::mutex> lock(mutex);
			
			if (m_udpConnectionsCv.wait_for(lock, std::chrono::seconds(m_udpRequestTimeout), [this, &nick] { return m_udpConnectionsMap.get(nick).first; })) {
				success = true;
				return;
			}
			else {
				success = false;
				return;
			}
		}
		});
	waitThread.join();

	return success;
}

void Server::accept() {
	while (true) {
		std::shared_ptr<tcp::socket> socket = std::make_shared<tcp::socket>(m_ioService);
		m_acceptorPtr->accept(*socket);
		
		// SERVER FULL
		if (m_usersMap.size() >= m_maxConnections) {
			NetUtils::Tcp::write_(*socket, NetPacket(NetPacket::NetMessages::SERVER_FULL, nullptr, 0));
			socket.reset();
			continue;
		}
		else {
			NetUtils::Tcp::write_(*socket, NetPacket(NetPacket::NetMessages::IDLE, nullptr, 0));
		}
		
		std::thread t(&Server::handleClient, this, socket);
		/* with detach the thread code is execute indipendedly */
		t.detach();
	}
}

bool Server::handleUserNickname(std::shared_ptr<tcp::socket> socket, const std::string& nick) {
	try {
		// if the nickname already exist 
		if (nicknameAlreadyExist(nick)) {
			std::cout << "\nClient [ IP ]: " << socket->remote_endpoint().address().to_string() << " [ NICK ]: " << nick << " | refused ( nick already exist )";

			NetUtils::Tcp::write_(*socket, NetPacket(NetPacket::NetMessages::NICK_EXITS, nullptr, 0));
			return false;
		}
		else {
			// creates the User
			m_usersMap.insert(nick, std::make_shared<User>(nick, socket));
			std::cout << "\nClient [ IP ]: " << socket->remote_endpoint().address().to_string() << "[ " << nick << " ] " << nick << " | accepted.";
			// after this message the client will send the UDP_CONNECTION request
			NetUtils::Tcp::write_(*socket, NetPacket(NetPacket::NetMessages::CLIENT_ACCEPTED, nullptr, 0));
			return true;
		}
	}
	catch (const boost::system::system_error& ex) {
		// temporary catch solution debug 
		std::cout << "\nCatch in handle client... (handleNickname func)";
		m_usersMap.erase(nick);

		return false;
	}
}

void Server::handleClient(std::shared_ptr<tcp::socket> socket) {
	std::string nick;
	try {
		/* read the nickname */
		NetPacket packet = NetUtils::Tcp::read_(*socket);
		nick = std::string(packet.getData().begin(), packet.getData().end());

		/* check if there is another user with the same nickname and in case handle it. Otherwise create the user */
		if (!handleUserNickname(socket, nick)) {
			socket->close();
			return;
		}
		m_udpConnectionsMap.insert(nick, std::pair<bool, std::shared_ptr<boost::asio::ip::udp::endpoint>>(false, nullptr));

		/* (blocking)waits for x seconds or until a udp connection comes from the client */
		if (waitUDPConnection(nick)) {
			std::cout << "\nConnessione UDP riuscita sul thread: " << nick;
			NetUtils::Tcp::write_(*socket, NetPacket(NetPacket::NetMessages::UDP_CONNECTION_SUCCESS, nullptr, 0));
		}
		else {
			std::cout << "\nConnessione UDP non riuscita sul thread: " << nick;
			NetUtils::Tcp::write_(*socket, NetPacket(NetPacket::NetMessages::IDLE, nullptr, 0));
			socket->close();
			m_usersMap.erase(nick);
			m_udpConnectionsMap.erase(nick);
			return;
		}

		NetPacket::NetMessages netMsg;
		while (true) {
			netMsg = NetUtils::Tcp::read_(*socket).getMsgType();

			if (netMsg == NetPacket::NetMessages::MATCHMAKING_REQUEST) {
				MatchmakingRequestStates state = handleMatchmaking(socket, nick);
				if (state == MatchmakingRequestStates::FOUND) {
					std::thread t(&Server::gameSessionThread, this, nick);
					t.detach();
				}
				else if(state == MatchmakingRequestStates::WAIT) {
					/* start the thread to listen if the client wants to undo the matchmaking */
					m_tempThreadsManager.push(TemporaryThread(std::make_shared<std::thread>(&Server::handleUndoMatchmaking, this, socket, nick), false));
					m_tempThreadsManager.back().getThread()->detach();
				}
				return;
			}
		}
	}
	catch (const boost::system::system_error& ex) {
		std::cerr << "\nCatch in handle client [ Server.cpp ] | " << ex.what() << "\n";
		socket->close();
		m_usersMap.erase(nick);
		m_udpConnectionsMap.erase(nick);
		return;
	}

}

Server::MatchmakingRequestStates Server::handleMatchmaking(std::shared_ptr<tcp::socket> socket, const std::string nick) {
	if (m_matchmakingQueue.empty()) {
		m_matchmakingQueue.push(m_usersMap.get(nick));
		try {
			NetUtils::Tcp::write_(*socket, NetPacket(NetPacket::NetMessages::WAIT_FOR_MATCH, nullptr, 0));

			std::cout << "\nClient [nick]: " << nick << " in queue for a match.";
			return MatchmakingRequestStates::WAIT;
		}
		catch (const boost::system::system_error& ex) {
			std::cerr << "\nCatch in handleMatchmaking [ Server.cpp ] | " << ex.what() << "\n";
			socket->close();
			m_usersMap.erase(nick);
			m_udpConnectionsMap.erase(nick);
			return MatchmakingRequestStates::ERR;
		}
	}
	return MatchmakingRequestStates::FOUND;
}

void Server::gameSessionThread(const std::string nick) {
	// match this client with the last client who requested the match 
	std::shared_ptr<User> player1 = m_matchmakingQueue.front();
	player1->setUDPEndpoint(m_udpConnectionsMap.get(player1->getNick()).second);
	std::shared_ptr<User> player2 = m_usersMap.get(nick);
	player2->setUDPEndpoint(m_udpConnectionsMap.get(player2->getNick()).second);
	// remove the client from the matchmaking queue because a match has been found
	m_matchmakingQueue.pop();

	/* send the match found message */
	NetUtils::Tcp::write_(*player1->getTCPSocket(), NetPacket(NetPacket::NetMessages::MATCH_FOUND, nullptr, 0));
	NetUtils::Tcp::write_(*player2->getTCPSocket(), NetPacket(NetPacket::NetMessages::MATCH_FOUND, nullptr, 0));

	/* delete the thread because the match has been found and it's useless to have. */
	if (m_tempThreadsManager.size()) {
		m_tempThreadsManager.pop();
	}

	Sleep(1000);
	GameSession gameSession(&m_usersMap, player1, player2, m_udpServerSocket);
	// store a reference to the gameSession object in the game sessions map.
	m_gameSessionsMap.insert(player1->getNick() + player2->getNick(), std::make_shared<GameSession>(gameSession));
	// blocking operation.
	gameSession.start();
	m_gameSessionsMap.erase(player1->getNick() + player2->getNick());
}

//TO-DO: ( REDISIGN ) of this undo-matchmaking-thread
void Server::handleUndoMatchmaking(std::shared_ptr<tcp::socket> socket, const std::string nick) {
	using namespace std::chrono_literals;
	NetPacket p;

	socket->non_blocking(true);
	while (true) {
		try {
			std::this_thread::sleep_for(300ms);
	
			p = NetUtils::Tcp::read_(*socket);
			if (p.getMsgType() == NetPacket::NetMessages::UNDO_MATCHMAKING) {
				m_matchmakingQueue.pop();
				m_usersMap.erase(nick);
				m_udpConnectionsMap.erase(nick);
				/* add this thread to the cancellable threads, so it will be deleted */
				m_tempThreadsManager.increaseUselessCounter();

				std::cout << "\nClient [ " << nick << " ]: " << " undo the matchmaking";
				socket->non_blocking(false);
				return;
			}
			else if (p.getMsgType() == NetPacket::NetMessages::MATCH_FOUND) {
				socket->non_blocking(false);
				return;
			}
		}
		catch (const boost::system::system_error& e) {
			if (e.code() != boost::asio::error::would_block) {
				std::cerr << "\nCatch in listen for UndoMatchmaking: " << e.what() << std::endl;

				m_matchmakingQueue.pop();
				m_usersMap.erase(nick);
				m_udpConnectionsMap.erase(nick);

				return;
			}
		}
	}
}

bool Server::nicknameAlreadyExist(const std::string& nick) {
	auto it = m_usersMap.find(nick);

	return it != m_usersMap.end();
}

ThreadSafeUnorderedMap<std::string, std::shared_ptr<User>>& Server::getUsersMap() {
	return m_usersMap;
}

void Server::clearUselessThreads() {
	while (m_doRoutines) {
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(std::chrono::seconds(m_clearUselessThreadsTime));
		
		std::cout << "\n[ LOG ]: Useless Threads Cleared: " << m_tempThreadsManager.clearUselessThreads();
	
	}
	std::cout << "\n[ LOG ]: End ClearUselessThreads routine.";
}

void Server::startRoutines() {
	/* the thread that will host the function to clear useless threads */
	std::thread clearThread(&Server::clearUselessThreads, this);
	clearThread.detach();
	
	/* the thread that listens for udp connections */
	std::thread udpThread(&Server::listenUDPConnections, this);
	udpThread.detach();
}