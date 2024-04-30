#include "MainGameWindow.h"

void MainGameWindow::init(const std::string nickname, std::shared_ptr<Client> client) {
    this->client = client;

    this->windowPtr = std::make_shared<sf::RenderWindow>();
    this->windowPtr->create(sf::VideoMode(900, 600), "SkyFall Showdown", sf::Style::Close);
    this->windowPtr->setFramerateLimit(60);
   
    /* get the enemy nickname */
    if (!handleEnemyNickname()) {
        this->quitGame();
        return;
    }
    this->myNickname.setString(nickname);

    /* start the thread to listen for game messages */
    std::thread t(&MainGameWindow::handleMessages, this);
    t.detach();

    setTextures();
    initSprites();

    initPlayerAndEnemyStats();

    sf::Event event;
    while (windowPtr->isOpen()) {
        while (windowPtr->pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                this->quitGame();
                return;
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                ;
            }
            handlePlayerMovement(event);
        }
        draw();
    }
}

void MainGameWindow::initPlayerAndEnemyStats() {
    std::random_device dev;
    std::mt19937 rng(dev());
    /* generate the X and Y*/
    std::uniform_int_distribution<std::mt19937::result_type> xPositionGenerator(100, 800);
    std::uniform_int_distribution<std::mt19937::result_type> yPositionGenerator(100, 500);

    int xPosition = xPositionGenerator(rng), yPosition = yPositionGenerator(rng);
    /* set the player to a random position*/
    youPlayer.setPosition(xPosition, yPosition);

    /* TO-DO ( SEND THE POSITION AND RECEIVE THE ENEMY POSITION ) */
}

void MainGameWindow::handleMessages() {
    NetPacket packet;

    while (true) {
        try {
            packet = NetUtils::read_(*this->client->getSocket());
        }
        catch (const boost::system::system_error& ex) {
            std::cerr << "\nError in handleMessages() | " << ex.what();
            return;
        }
    }
}

void MainGameWindow::handlePlayerMovement(sf::Event& event) {
    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Key::W) {
            if (youPlayer.getPosition().y - 10 >= 0) {
                youPlayer.setPosition(youPlayer.getPosition().x, youPlayer.getPosition().y - 10);
            }
        }
        if (event.key.code == sf::Keyboard::Key::A) {
            if (youPlayer.getPosition().x - 10 >= 0) {
                youPlayer.setPosition(youPlayer.getPosition().x - 10, youPlayer.getPosition().y);
            }
        }
        if (event.key.code == sf::Keyboard::Key::S) {
            if (youPlayer.getPosition().y + 10 < 470) {
                youPlayer.setPosition(youPlayer.getPosition().x, youPlayer.getPosition().y + 10);
            }
        }
        if (event.key.code == sf::Keyboard::Key::D) {
            if (youPlayer.getPosition().x + 10 < 832) {
                youPlayer.setPosition(youPlayer.getPosition().x + 10, youPlayer.getPosition().y);
            }
        }
    }
}

void MainGameWindow::quitGame() {
    this->client->close();
    this->windowPtr->close();
}

void MainGameWindow::draw() {
    this->windowPtr->clear();

    this->windowPtr->draw(myNickname);
    this->windowPtr->draw(enemyNickname);
    this->windowPtr->draw(youPlayer);

    this->windowPtr->display();
}

void MainGameWindow::setTextures() {

}

void MainGameWindow::initSprites() {
    myNickname.setFont(FontManager::fredokaOne);
    myNickname.setCharacterSize(35);
    myNickname.setPosition(20, (windowPtr->getSize().y - 60));
    myNickname.setFillColor(sf::Color(31, 110, 2));
   
    enemyNickname.setFont(FontManager::fredokaOne);
    enemyNickname.setCharacterSize(35);
    enemyNickname.setPosition(((windowPtr->getSize().x - enemyNickname.getGlobalBounds().width) - 20), (windowPtr->getSize().y - 60));
    enemyNickname.setFillColor(sf::Color(110, 6, 2));

    youPlayer.setSize(sf::Vector2f(70.f, 70.f));
    youPlayer.setFillColor(sf::Color(2, 35, 89));

    enemyPlayer.setSize(sf::Vector2f(70.f, 70.f));
    enemyPlayer.setPosition(sf::Vector2f(300.f, 200.f));
    enemyPlayer.setFillColor(sf::Color(2, 35, 89));
}

bool MainGameWindow::handleEnemyNickname() {
    try {
        NetPacket p = NetUtils::read_(*this->client->getSocket());
        this->enemyNickname.setString(p.getStr());
        return true;
    }
    catch (const boost::system::system_error& e) {
        std::cerr << "\nError in handle enemy nickname | " << e.what() << "\n";
        return false;
    }
}