#include "MainGameWindow.h"

void MainGameWindow::init(const std::string nickname, std::shared_ptr<Client> client) {
    this->client = client;
    
    this->windowPtr = std::make_shared<sf::RenderWindow>();

    this->windowPtr->create(sf::VideoMode(1200, 800), "SkyFall Showdown", sf::Style::Close);
    this->windowPtr->setFramerateLimit(60);

    this->closeSettingsWindowFlag.store(false);
    this->inGameSettings = false;
    m_Game.setBlockActions(true);

    /* get the enemy nickname */
    if (!handleEnemyNickname()) {
        this->quitGame();
        return;
    }
    this->myNickname.setString(nickname);

    initSprites();
    
    /* try to get the default position of the player and enemy player*/
    if (!initPlayerAndEnemyPosition()) {
        this->quitGame();
        return;
    }

    /* start the thread to listen for game messages */
    std::thread t(&MainGameWindow::handleMessages, this);
    t.detach();

    sf::Event event;
    this->displayWindow = true;
    sf::Clock sprintClock;
    
    resolvePlayerSprint();

    while (this->displayWindow) {
        while (windowPtr->pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                this->quitGame();
                return;
            }
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                this->inGameSettings = true;
                if (gameSettingsMenu.init(windowPtr, &closeSettingsWindowFlag)) {
                    return;
                }
                this->inGameSettings = false;
            }
            handleMouseClick(event);
            handleKeyBoards(event);
        }
        if (m_Game.getGameState() == Game::GameStates::RUNNING) {
            update(sprintClock.restart());
        }
        else if (m_Game.getGameState() == Game::GameStates::END) {
            if (!m_Game.hasEnemyQuit()) {
                NetUtils::write_(*this->client->getSocket(), NetPacket(NetPacket::NetMessages::GAME_END, nullptr, 0));
            }
            this->closeSettingsWindowFlag.store(true);

            endGameWindow.init(*this->windowPtr, m_Game, this->myNickname, this->vsText, this->enemyNickname);
            this->windowPtr->close();
            return;
        }
        draw();
    }
}

bool MainGameWindow::initPlayerAndEnemyPosition() {
    NetPacket packet = NetUtils::read_(*this->client->getSocket());
    if (packet.getMsgType() == NetPacket::NetMessages::PLAYER_POSITION) {
        sf::Vector2f pos = NetGameUtils::getSfvector2f(packet);

        m_Game.setPlayerStartPosition(pos);
        this->youPlayer->setPosition(pos);
    }

    packet = NetUtils::read_(*this->client->getSocket());
    if (packet.getMsgType() == NetPacket::NetMessages::PLAYER_POSITION) {
        this->enemyPlayer->setPosition(NetGameUtils::getSfvector2f(packet));

        return true;
    }
    return false;
}

void MainGameWindow::handleMessages() {
    NetPacket packet;
    while (true) {
        try {
            packet = NetUtils::read_(*this->client->getSocket());

            switch (packet.getMsgType()) {
            case NetPacket::NetMessages::GAME_END:
                this->client->close();
                return;
            /* if the enemy quit */
            case NetPacket::NetMessages::QUIT_GAME:
                this->client->close();
                m_Game.handleEnemyQuit();
                return;

            case NetPacket::NetMessages::PLAYER_POSITION:
                this->enemyPlayer->setPosition(NetGameUtils::getSfvector2f(packet));
                break;

            case NetPacket::NetMessages::ENEMY_COLLISION:
                this->youPlayer->handleEnemyCollision((Player::CollisionSide)packet.getData()[0]);
                break;

            case NetPacket::NetMessages::GAME_STARTED:
                m_Game.waitRound(m_waitRoundText);
                m_Game.setDamageAreasCords(NetGameUtils::getDamageAreasCoordinates(packet));
                /* set the vector of damages areas */
                m_Game.startGame(m_damageAreasVector);
                m_Game.startTimer(m_gameTimer);
                break;
            
            case NetPacket::NetMessages::ENEMY_COLLISION_W_DAMAGE_AREA:
                this->youPlayer->stopMove();
                m_Game.waitRound(m_waitRoundText);
                m_Game.handleNewRound(Game::GameEntities::ENEMY);

                /* set the player to the start position and send the position to the enemy */
                this->youPlayer->setPosition(m_Game.getStartPlayerPosition());
                float p[] = { this->youPlayer->getPosition().x, this->youPlayer->getPosition().y };
                NetUtils::write_(*this->client->getSocket(), NetPacket(NetPacket::NetMessages::PLAYER_POSITION, reinterpret_cast<const uint8_t*>(p), sizeof(p)));
                break;
            }
        }
        catch (const boost::system::system_error& ex) {
            std::cerr << "\nError in handleMessages() | " << ex.what();
            return;
        }
    }
}

void MainGameWindow::updateRechargeBar() {
    this->rechargeBarProgress = this->youPlayer->getClock().getElapsedTime().asSeconds() / this->youPlayer->getSprintTimeout();

    if (this->rechargeBarProgress > 1.0f) {
        this->rechargeBarProgress = 1.0f;
    }
    this->rechargeBar.setSize(sf::Vector2f(static_cast<float>(170 * this->rechargeBarProgress), 30.f));
}

void MainGameWindow::resolvePlayerSprint() {
    /* 
       this piece of code ensure that when the player will sprint for the first time, it will all works fine 
       without this the first sprint of the player won't work as aspected ( i can't figure out why. )
    */
    sf::Vector2f pos = this->youPlayer->getPosition();
    this->youPlayer->setTarget(sf::Vector2f(1000, 1000));
    this->youPlayer->startSprint(false);
    this->youPlayer->setPosition(pos);
}

void MainGameWindow::checkPlayerWindowBorders() {
    sf::FloatRect playerBounds = this->youPlayer->getGlobalBounds();
    sf::Vector2f position = this->youPlayer->getPosition();

    if (playerBounds.left < 0.f) {
        this->youPlayer->setPosition(playerBounds.width / 2, position.y);
        this->youPlayer->stopMove();
    }
    else if ((playerBounds.left + playerBounds.width) > 1200.f) {
        this->youPlayer->setPosition(1200.f - playerBounds.width, position.y);
        this->youPlayer->stopMove();
    }
    else if (playerBounds.top < 110.f) {
        this->youPlayer->setPosition(position.x, 110);
        this->youPlayer->stopMove();
    }
    else if ((playerBounds.top + playerBounds.height) > 800.f) {
        this->youPlayer->setPosition(position.x, 800.f - playerBounds.height);
        this->youPlayer->stopMove();
    }
}

void MainGameWindow::handleMouseClick(sf::Event& event) {
    if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            m_Game.handlePlayerMovement(event, *this->youPlayer, *this->windowPtr, false);
        }
        else if (event.mouseButton.button == sf::Mouse::Right) {
            m_Game.handlePlayerMovement(event, *this->youPlayer, *this->windowPtr, true);
        }
    }
}

void MainGameWindow::handleKeyBoards(sf::Event event) {
    if (event.type == sf::Event::KeyPressed) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::E)) {
            this->youPlayer->stopMove();
        }
    }
}

void MainGameWindow::quitGame() {
    this->client->close();
    this->displayWindow = false;

    if (this->inGameSettings) {
        this->closeSettingsWindowFlag.store(true);
    }
}

void MainGameWindow::update(sf::Time deltaTime) {
    this->youPlayer->update(deltaTime, this->enemyPlayer->getRect());
   
    updateRechargeBar();

    if (this->youPlayer->isMoving()) {
        checkPlayerWindowBorders();
        /* send the position */
        float p[2] = { this->youPlayer->getPosition().x, this->youPlayer->getPosition().y };
        NetUtils::write_(*this->client->getSocket(), NetPacket(NetPacket::NetMessages::PLAYER_POSITION, reinterpret_cast<const uint8_t*>(p), sizeof(p)));

        if (this->youPlayer->isSprinting() && this->youPlayer->isEnemyHit()) {
            Player::CollisionSide cL = this->youPlayer->getCollidedSide();

            NetUtils::write_(*this->client->getSocket(), NetPacket(NetPacket::NetMessages::ENEMY_COLLISION, (uint8_t*)&cL, sizeof(cL)));
            this->youPlayer->stopMove();
            this->youPlayer->resetEnemyHit();
        }

        /* check if the player collided with a damage area */
        if (m_Game.checkCollision(this->m_damageAreasVector.at(m_Game.getCurrentRound()), *this->youPlayer)) {
            this->youPlayer->stopMove();
            m_Game.waitRound(m_waitRoundText);
            m_Game.handleNewRound(Game::GameEntities::PLAYER);

            NetUtils::write_(*this->client->getSocket(), NetPacket(NetPacket::NetMessages::ENEMY_COLLISION_W_DAMAGE_AREA, nullptr, 0));

            this->youPlayer->setPosition(m_Game.getStartPlayerPosition());
            float p[] = { this->youPlayer->getPosition().x, this->youPlayer->getPosition().y };

            NetUtils::write_(*this->client->getSocket(), NetPacket(NetPacket::NetMessages::PLAYER_POSITION, reinterpret_cast<const uint8_t*>(p), sizeof(p)));
        }
    }
}

void MainGameWindow::draw() {
    this->windowPtr->clear();

    this->windowPtr->draw(myNickname);
    this->windowPtr->draw(enemyNickname);
    this->windowPtr->draw(*youPlayer);
    this->windowPtr->draw(*enemyPlayer);
    this->windowPtr->draw(rechargeBarBorder);
    this->windowPtr->draw(rechargeBar);
    this->windowPtr->draw(vsText);
    
    for (int i = 0; i < m_Game.getPlayerLife(); i++) {
        this->windowPtr->draw(youHealth[i]);
    }
    for (int i = 0; i < m_Game.getEnemyLife(); i++) {
        this->windowPtr->draw(enemyHealth[i]);
    }
    if (m_Game.getGameState() == Game::GameStates::RUNNING) {
        this->windowPtr->draw(m_gameTimer);

        for (sf::CircleShape& shape : this->m_damageAreasVector.at(m_Game.getCurrentRound())) {
            this->windowPtr->draw(shape);
        }
    }
    /* draw the 3..2..1 text */
    if (m_Game.areActionsBlocked()) {
        this->windowPtr->draw(m_waitRoundText);
    }
    this->windowPtr->display();
}

void MainGameWindow::initSprites() {
    rechargeBarBorder.setSize(sf::Vector2f(170.f, 30.f));
    rechargeBarBorder.setPosition(1000.f, 30.f);
    rechargeBarBorder.setFillColor(sf::Color::Transparent);
    rechargeBarBorder.setOutlineThickness(3);
    rechargeBarBorder.setOutlineColor(sf::Color(128, 103, 36));

    rechargeBar.setSize(sf::Vector2f(0.f, 30.f));
    rechargeBar.setPosition(1000.f, 30.f);
    rechargeBar.setFillColor(sf::Color(196, 154, 39));

    myNickname.setFont(FontManager::fredokaOne);
    myNickname.setCharacterSize(35);
    myNickname.setPosition(20.f, 22.f);
    myNickname.setFillColor(sf::Color(31, 110, 2));
   
    vsText.setFont(FontManager::fredokaOne);
    vsText.setCharacterSize(30);
    vsText.setPosition(myNickname.getGlobalBounds().left + myNickname.getGlobalBounds().width + vsText.getGlobalBounds().width + 20.f, 24.f);
    vsText.setFillColor(sf::Color(219, 219, 219));
    vsText.setString("vs");

    enemyNickname.setFont(FontManager::fredokaOne);
    enemyNickname.setCharacterSize(35);
    enemyNickname.setPosition(vsText.getGlobalBounds().left + vsText.getGlobalBounds().width + 20.f, 22.f);
    enemyNickname.setFillColor(sf::Color(110, 6, 2));

    m_gameTimer.setFont(FontManager::fredokaOne);
    m_gameTimer.setCharacterSize(30);
    m_gameTimer.setPosition(600.f, m_gameTimer.getGlobalBounds().height + (rechargeBarBorder.getGlobalBounds().height / 2) + 4.f);
    m_gameTimer.setFillColor(sf::Color(219, 219, 219));

    m_waitRoundText.setFont(FontManager::fredokaOne);
    m_waitRoundText.setCharacterSize(80);
    m_waitRoundText.setFillColor(sf::Color(255, 255, 255));
    m_waitRoundText.setPosition((windowPtr->getSize().x / 2.f) - (m_waitRoundText.getGlobalBounds().width / 2.f), (windowPtr->getSize().y / 3.f) - (m_waitRoundText.getGlobalBounds().height / 2.f));
    
    youPlayer = std::make_shared<Player>(sf::Vector2f(70.f, 70.f), sf::Color(2, 35, 89), sf::Color(31, 110, 2), 8.0f, 200.f, 1000.f, 4.f);
    enemyPlayer = std::make_shared<Player>(sf::Vector2f(70.f, 70.f), sf::Color(2, 35, 89), sf::Color(110, 6, 2), 8.0f, 200.f, 1000.f, 4.f);

    float youHealthPosX = 850.f;
    float enemyHealthPosX = 720.f;

    for (int i = 0; i < 3; i++) {
        youHealth[i].setSize(sf::Vector2f(18.f, 18.f));
        youHealth[i].setFillColor(sf::Color(31, 110, 2));
        youHealth[i].setPosition(youHealthPosX, youHealth[i].getGlobalBounds().height + rechargeBarBorder.getGlobalBounds().height / 2);

        enemyHealth[i].setSize(sf::Vector2f(18.f, 18.f));
        enemyHealth[i].setPosition(enemyHealthPosX, enemyHealth[i].getGlobalBounds().height + rechargeBarBorder.getGlobalBounds().height / 2);
        enemyHealth[i].setFillColor(sf::Color(110, 6, 2));
        
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
}

bool MainGameWindow::handleEnemyNickname() {
    try {
        NetPacket p = NetUtils::read_(*this->client->getSocket());
        this->enemyNickname.setString(NetGameUtils::getString(p));
        return true;
    }
    catch (const boost::system::system_error& e) {
        std::cerr << "\nError in handle enemy nickname | " << e.what() << "\n";
        return false;
    }
}