#include "MainGameWindow.h"

void MainGameWindow::init(const std::string nickname, Client& client) {
    setMusicAndSound();
    m_Client = &client;
    
    m_Window.create(sf::VideoMode(1200, 800), "SkyFall Showdown", sf::Style::Close);
    m_Window.setFramerateLimit(30);
    m_Window.setMouseCursorGrabbed(false);

    m_isCursorGrabbed = false;
    m_closeSettingsWindowFlag.store(false);
    m_inGameSettings = false;
    m_Game.setBlockActions(true);

    m_gameSettingsMenu.init(m_Window);

    /* get the enemy nickname */
    if (!handleEnemyNickname()) {
        quitGame();
        return;
    }
    m_myNickname.setString(nickname);
    
    m_sessionInfoStruct.m_playerNick = nickname;
    getSessionInfo();

    m_udpPositionPacket = std::make_unique<NetUdpPacket>(m_sessionInfoStruct.m_playerNick,
        NetUdpPacket::UdpMessages::GAME_MESSAGE,
        m_sessionInfoStruct.m_sessionUUID,
        NetPacket::NetMessages::SET_PLAYER_POSITION,
        nullptr,
        0,
        0);
    m_positionPacketCounter = 1;
    m_enemyPositionPacketCounter = 0;

    initSprites();

    /* try to get the default position of the player and enemy player*/
    if (!initPlayerAndEnemyPosition()) {
        quitGame();
        return;
    }

    /* start the threads functions to listen for game messages */
    handleMessages();

    sf::Event event;
    m_displayWindow = true;
    sf::Clock sprintClock;

    std::cout << "\nPlayer: " << nickname << "\nGameSessionUUID: " << m_sessionInfoStruct.m_sessionUUID;  //DEBUG
    while (m_displayWindow) {
        while (m_Window.pollEvent(event)) {
            if (m_inGameSettings) {
                m_gameSettingsMenu.handleMouseButtonPressed(event, m_Window, m_inGameSettings, m_youPlayer);
            }
            if (event.type == sf::Event::Closed) {
                quitGame();
                return;
            }
            handleMouseClick(event);
            handleKeyBoards(event);
        }
        if (m_Game.getGameState() == Game::GameStates::RUNNING) {
            update(sprintClock.restart());
        }
        else if (m_Game.getGameState() == Game::GameStates::END) {
            g_aSingleton.getCountdownSound().stop();
            if (!m_Game.hasEnemyQuit()) {
                NetUtils::Tcp::write_(*m_Client->getSocket(), NetPacket(NetPacket::NetMessages::GAME_END, nullptr, 0));
            }
            m_closeSettingsWindowFlag.store(true);

            m_endGameWindow.init(m_Window, m_Game, m_myNickname, m_vsText, m_enemyNickname);
            m_Window.close();
            g_aSingleton.getBattleMusic().stop();
            return;
        }
        if (m_inGameSettings) {
            m_gameSettingsMenu.draw(m_Window);
        }
        else {
            draw();
        }
    }
}

bool MainGameWindow::initPlayerAndEnemyPosition() {
    NetPacket packet = NetUtils::Tcp::read_(*m_Client->getSocket());
    // player position
    if (packet.getMsgType() == NetPacket::NetMessages::SET_PLAYER_POSITION) {
        sf::Vector2f pos = NetGameUtils::getSfvector2f(packet.getData());

        m_Game.setPlayerPosition(pos);
        m_youPlayer.setPosition(pos);
    }
    // enemy position
    packet = NetUtils::Tcp::read_(*m_Client->getSocket());
    if (packet.getMsgType() == NetPacket::NetMessages::SET_PLAYER_POSITION) {
        sf::Vector2f pos = NetGameUtils::getSfvector2f(packet.getData());

        m_Game.setEnemyPosition(pos);
        m_enemyPlayer.setPosition(pos);

        return true;
    }
    return false;
}

void MainGameWindow::handleMessages() {
    std::thread t([this] {
        NetPacket packet;
        float playerPositionInfo[3];

        while (true) {
            try {
                packet = NetUtils::Tcp::read_(*m_Client->getSocket());

                switch (packet.getMsgType()) {
                case NetPacket::NetMessages::IDLE:
                    break;

                case NetPacket::NetMessages::GAME_END:
                    m_Client->close();
                    return;

                case NetPacket::NetMessages::QUIT_GAME:
                    m_Client->close();
                    m_Game.handleEnemyQuit();
                    return;

                case NetPacket::NetMessages::SET_PLAYER_POSITION:
                    m_enemyPlayer.stopMove();
                    m_enemyPlayer.setPosition(NetGameUtils::getSfvector2f(packet.getData()));
                    break;

                case NetPacket::NetMessages::PLAYER_POSITION:
                    std::memcpy(&playerPositionInfo[0], &packet.getData()[0], sizeof(float));
                    std::memcpy(&playerPositionInfo[1], &packet.getData()[sizeof(float)], sizeof(float));
                    std::memcpy(&playerPositionInfo[2], &packet.getData()[sizeof(float)*2], sizeof(float));

                    m_enemyPlayer.setTarget(sf::Vector2f(playerPositionInfo[0], playerPositionInfo[1]));
                    if (playerPositionInfo[2]) {
                        m_enemyPlayer.startSprint(true);
                    }
                    break;

                case NetPacket::NetMessages::ENEMY_COLLISION:
                    m_youPlayer.setHitByEnemy(true);
                    m_enemyPlayer.stopMove();
                    m_Game.handlePlayerCollision((Player::CollisionSide)packet.getData()[0], m_youPlayer, *m_Client);
                    break;

                case NetPacket::NetMessages::GAME_STARTED:
                    m_Game.waitRound(m_waitRoundText, g_aSingleton.getCountdownSound());
                    m_Game.setDamageAreasCords(NetGameUtils::getDamageAreasCoordinates(packet.getData()));
                    /* set the vector of damages areas */
                    m_Game.startGame(m_damageAreasVector);
                    m_Game.startTimer(m_gameTimer);
                    break;

                case NetPacket::NetMessages::ENEMY_COLLISION_W_DAMAGE_AREA:
                    m_youPlayer.stopMove();
                    m_enemyPlayer.stopMove();

                    m_youPlayer.resetSprint();
                    m_Game.handleNewRound(Game::GameEntities::ENEMY);
                    m_Game.waitRound(m_waitRoundText, g_aSingleton.getCountdownSound());

                    m_youPlayer.setPosition(m_Game.getStartPlayerPosition());
                    m_enemyPlayer.setPosition(m_Game.getEnemyStartPosition());
                    break;
                }
            }
            catch (const boost::system::system_error& ex) {
                std::cerr << "\nError in handleMessages() | " << ex.what();
                return;
            }
        }
    });
    t.detach();
}

void MainGameWindow::updateRechargeBar() {
    m_rechargeBarProgress = m_youPlayer.getClock().getElapsedTime().asSeconds() / m_youPlayer.getSprintTimeout();

    if (m_rechargeBarProgress > 1.0f) {
        m_rechargeBarProgress = 1.0f;
    }
    m_rechargeBar.setSize(sf::Vector2f(static_cast<float>(170 * m_rechargeBarProgress), 30.f));
}

void MainGameWindow::checkPlayerWindowBorders(Player& player) {
    sf::FloatRect playerBounds = player.getGlobalBounds();
    sf::Vector2f position = player.getPosition();

    if (playerBounds.left < 0.f) {
        player.setPosition(playerBounds.width / 2, position.y);
        player.stopMove();
    }
    else if ((playerBounds.left + playerBounds.width) > 1200.f) {
        player.setPosition(1200.f - playerBounds.width, position.y);
        player.stopMove();
    }
    else if (playerBounds.top < 110.f) {
        player.setPosition(position.x, 110);
        player.stopMove();
    }
    else if ((playerBounds.top + playerBounds.height) > 800.f) {
        player.setPosition(position.x, 800.f - playerBounds.height);
        player.stopMove();
    }
}

void MainGameWindow::handleMouseClick(sf::Event& event) {
    if (event.type == sf::Event::MouseButtonPressed && !m_inGameSettings) {
        const sf::Vector2i mousePosition{ sf::Mouse::getPosition(m_Window) };
        const sf::Vector2f mouseCoord{ m_Window.mapPixelToCoords(mousePosition) };

        if (event.mouseButton.button == sf::Mouse::Left) {
            m_Game.handlePlayerMovement(m_youPlayer, mouseCoord, false, *m_Client);
        }
        else if (event.mouseButton.button == sf::Mouse::Right) {
            m_Game.handlePlayerMovement(m_youPlayer, mouseCoord, true, *m_Client);
        }
    }
}

void MainGameWindow::handleKeyBoards(sf::Event& event) {
    if (event.type == sf::Event::KeyPressed && !m_inGameSettings) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::E)) {
            if (!m_youPlayer.hitByEnemy() && m_youPlayer.isSprinting()) {
                m_youPlayer.stopMove();
                float p[]{ m_youPlayer.getPosition().x, m_youPlayer.getPosition().y, 0 };
                NetUtils::Tcp::write_(*m_Client->getSocket(), NetPacket(NetPacket::NetMessages::SET_PLAYER_POSITION, reinterpret_cast<const uint8_t*>(p), sizeof(p)));
            }
        }
        else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
            m_inGameSettings = !m_inGameSettings;
        }
        else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Tab) {
            m_isCursorGrabbed = !m_isCursorGrabbed;
            m_Window.setMouseCursorGrabbed(m_isCursorGrabbed);
        }
    }
}

void MainGameWindow::quitGame() {
    m_Client->close();
    g_aSingleton.getBattleMusic().stop();
    g_aSingleton.getCountdownSound().stop();
    m_displayWindow = false;

    if (m_inGameSettings) {
        m_closeSettingsWindowFlag.store(true);
    }
}

void MainGameWindow::setMusicAndSound() {
    g_aSingleton.getBattleMusic().setVolume(g_sSingleton.getValue(SkyfallUtils::Settings::MUSIC_VOLUME).GetInt());
    g_aSingleton.getBattleMusic().play();
    g_aSingleton.getBattleMusic().loop(true);
}

void MainGameWindow::update(sf::Time deltaTime) {
    m_youPlayer.update(deltaTime, m_enemyPlayer.getRect());
    m_enemyPlayer.update(deltaTime, m_youPlayer.getRect());

    updateRechargeBar();
    if (m_youPlayer.isMoving() || m_enemyPlayer.isMoving()) {
        checkPlayerWindowBorders(m_youPlayer);
        checkPlayerWindowBorders(m_enemyPlayer);

        if (m_youPlayer.isSprinting() && m_youPlayer.isEnemyHit()) {
            Player::CollisionSide cL = m_youPlayer.getCollidedSide();
            
            NetUtils::Tcp::write_(*m_Client->getSocket(), NetPacket(NetPacket::NetMessages::ENEMY_COLLISION, (uint8_t*)&cL, sizeof(cL)));
            m_youPlayer.stopMove();
            m_youPlayer.resetEnemyHit();
        }

        /* check if the player collided with a damage area */
        if (m_Game.checkCollision(m_damageAreasVector.at(m_Game.getCurrentRound()), m_youPlayer)) {
            m_youPlayer.resetSprint();
            m_youPlayer.stopMove();
            m_enemyPlayer.stopMove();
            m_Game.waitRound(m_waitRoundText, g_aSingleton.getCountdownSound());
            m_Game.handleNewRound(Game::GameEntities::PLAYER);

            NetUtils::Tcp::write_(*m_Client->getSocket(), NetPacket(NetPacket::NetMessages::ENEMY_COLLISION_W_DAMAGE_AREA, nullptr, 0));

            m_youPlayer.setPosition(m_Game.getStartPlayerPosition());
            m_enemyPlayer.setPosition(m_Game.getEnemyStartPosition());
        }
    }
}

void MainGameWindow::draw() {
    m_Window.clear();

    m_Window.draw(m_myNickname);
    m_Window.draw(m_enemyNickname);
    m_Window.draw(m_youPlayer);
    m_Window.draw(m_enemyPlayer);
    m_Window.draw(m_rechargeBarBorder);
    m_Window.draw(m_rechargeBar);
    m_Window.draw(m_vsText);

    for (int i = 0; i < m_Game.getPlayerLife(); i++) {
        m_Window.draw(m_youHealth[i]);
    }
    for (int i = 0; i < m_Game.getEnemyLife(); i++) {
        m_Window.draw(m_enemyHealth[i]);
    }
    if (m_Game.getGameState() == Game::GameStates::RUNNING) {
        m_Window.draw(m_gameTimer);
        for (sf::CircleShape& shape : m_damageAreasVector.at(m_Game.getCurrentRound())) {
            m_Window.draw(shape);
        }
    }
    if (m_Game.areActionsBlocked()) {
        m_Window.draw(m_waitRoundText);
    }
    m_Window.draw((m_isCursorGrabbed) ? m_lockBtn : m_unlockBtn);

    m_Window.display();
}

void MainGameWindow::getSessionInfo() {
    std::vector<uint8_t> data = NetUtils::Tcp::read_(*m_Client->getSocket()).getData();
    std::copy(data.begin(), data.end(), m_sessionInfoStruct.m_sessionUUID.begin());
}

void MainGameWindow::initSprites() {
    m_rechargeBarBorder.setSize(sf::Vector2f(170.f, 30.f));
    m_rechargeBarBorder.setPosition(1000.f, 30.f);
    m_rechargeBarBorder.setFillColor(sf::Color::Transparent);
    m_rechargeBarBorder.setOutlineThickness(3);
    m_rechargeBarBorder.setOutlineColor(sf::Color(128, 103, 36));

    m_rechargeBar.setSize(sf::Vector2f(0.f, 30.f));
    m_rechargeBar.setPosition(1000.f, 30.f);
    m_rechargeBar.setFillColor(sf::Color(196, 154, 39));

    m_myNickname.setFont(g_fSingleton.getFredokaOne());
    m_myNickname.setCharacterSize(35);
    m_myNickname.setPosition(20.f, 22.f);
    m_myNickname.setFillColor(sf::Color(31, 110, 2));

    m_vsText.setFont(g_fSingleton.getFredokaOne());
    m_vsText.setCharacterSize(30);
    m_vsText.setPosition(m_myNickname.getGlobalBounds().left + m_myNickname.getGlobalBounds().width + m_vsText.getGlobalBounds().width + 20.f, 24.f);
    m_vsText.setFillColor(sf::Color(219, 219, 219));
    m_vsText.setString("vs");

    m_enemyNickname.setFont(g_fSingleton.getFredokaOne());
    m_enemyNickname.setCharacterSize(35);
    m_enemyNickname.setPosition(m_vsText.getGlobalBounds().left + m_vsText.getGlobalBounds().width + 20.f, 22.f);
    m_enemyNickname.setFillColor(sf::Color(110, 6, 2));

    m_gameTimer.setFont(g_fSingleton.getFredokaOne());
    m_gameTimer.setCharacterSize(30);
    m_gameTimer.setPosition(600.f, m_gameTimer.getGlobalBounds().height + (m_rechargeBarBorder.getGlobalBounds().height / 2) + 4.f);
    m_gameTimer.setFillColor(sf::Color(219, 219, 219));

    m_waitRoundText.setFont(g_fSingleton.getFredokaOne());
    m_waitRoundText.setCharacterSize(80);
    m_waitRoundText.setFillColor(sf::Color(255, 255, 255));
    m_waitRoundText.setPosition((m_Window.getSize().x / 2.f) - (m_waitRoundText.getGlobalBounds().width / 2.f), (m_Window.getSize().y / 3.f) - (m_waitRoundText.getGlobalBounds().height / 2.f));

    m_youPlayer.setSize(sf::Vector2f(70.f, 70.f));
    m_youPlayer.setColor(sf::Color(2, 35, 89));
    m_youPlayer.setIndicator(sf::Color(31, 110, 2), 8.0f);
    m_youPlayer.setSpeed(200.f);
    m_youPlayer.setSprint(800.f, 5.f);
    m_youPlayer.setDebugMode(std::strcmp(g_sSingleton.getValue(SkyfallUtils::Settings::DEBUG_MODE).GetString(), "ON") == 0);

    m_enemyPlayer.setSize(sf::Vector2f(70.f, 70.f));
    m_enemyPlayer.setColor(sf::Color(2, 35, 89));
    m_enemyPlayer.setIndicator(sf::Color(110, 6, 2), 8.0f);
    m_enemyPlayer.setSpeed(200.f);
    m_enemyPlayer.setSprint(800.f, 5.f);
    
    float youHealthPosX = 850.f;
    float enemyHealthPosX = 720.f;

    for (int i = 0; i < 3; i++) {
        m_youHealth[i].setSize(sf::Vector2f(18.f, 18.f));
        m_youHealth[i].setFillColor(sf::Color(31, 110, 2));
        m_youHealth[i].setPosition(youHealthPosX, m_youHealth[i].getGlobalBounds().height + m_rechargeBarBorder.getGlobalBounds().height / 2);

        m_enemyHealth[i].setSize(sf::Vector2f(18.f, 18.f));
        m_enemyHealth[i].setPosition(enemyHealthPosX, m_enemyHealth[i].getGlobalBounds().height + m_rechargeBarBorder.getGlobalBounds().height / 2);
        m_enemyHealth[i].setFillColor(sf::Color(110, 6, 2));

        youHealthPosX += 30.f;
        enemyHealthPosX += 30.f;
    }

    for (int i = 0; i < 3; i++) {
        std::vector<sf::CircleShape> vec;
        for (int j = 0; j < 6; j++) {
            vec.push_back(sf::CircleShape());

            vec.at(j).setRadius(60.f);
            vec.at(j).setOutlineThickness(8.f);
            vec.at(j).setFillColor(sf::Color(120, 36, 14));
            vec.at(j).setOutlineColor(sf::Color(82, 20, 5));
        }
        m_damageAreasVector.push_back(vec);
    }

    m_lockBtn.setTexture(g_tSingleton.getLockBtn());
    m_unlockBtn.setTexture(g_tSingleton.getUnlockBtn());

    m_lockBtn.getSprite().setScale(0.18f, 0.18f);
    m_lockBtn.getSprite().setPosition(480.f, 0.1f);
    m_unlockBtn.getSprite().setPosition(480.f, 0.1f);
    m_unlockBtn.getSprite().setScale(0.18f, 0.18f);
}

bool MainGameWindow::handleEnemyNickname() {
    try {
        NetPacket p = NetUtils::Tcp::read_(*m_Client->getSocket());
        m_enemyNickname.setString(NetGameUtils::getString(p.getData()));
        return true;
    }
    catch (const boost::system::system_error& e) {
        std::cerr << "\nError in handle enemy nickname | " << e.what() << "\n";
        return false;
    }
}