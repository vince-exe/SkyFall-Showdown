#include "OptionsMainMenu.h"

void OptionsMainMenu::init(std::shared_ptr<sf::RenderWindow> windowPtr, std::shared_ptr<Music> backgroundMusicPtr, PopupReturnValues& checker) {
    this->windowPtr = windowPtr;

    sf::Cursor defaultCursor;
    defaultCursor.loadFromSystem(sf::Cursor::Arrow);
    windowPtr->setMouseCursor(defaultCursor);
    
    setTextures();
    initSprites(backgroundMusicPtr);

    bool requestExit = false;
    sf::Event event;
    while (windowPtr->isOpen() && !requestExit) {
        while (windowPtr->pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                windowPtr->close();
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                checker = PopupReturnValues::BACK;
                return;
            }
            handleMouseButtons(backgroundMusicPtr, event, requestExit, checker);
        }
        draw();
    }
}

void OptionsMainMenu::draw() {
    windowPtr->clear();
    windowPtr->draw(backBtn);
    windowPtr->draw(slider);
    windowPtr->draw(volumeText);
   
    for (int i = 0; i < 10; i++) {
        windowPtr->draw(checkPoints[i]);
    }
    windowPtr->display();
}

void OptionsMainMenu::handleMouseButtons(std::shared_ptr<Music> backgroundMusicPtr, sf::Event& event, bool& requestExit, PopupReturnValues& checker) {
    sf::Vector2f position = windowPtr->mapPixelToCoords(sf::Mouse::getPosition(*windowPtr));

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        checkVolumeLevel(backgroundMusicPtr, position);

        if (backBtn.isInside(position)) {
            requestExit = true;
            checker = PopupReturnValues::BACK;
            return;
        }
    }
}

void OptionsMainMenu::checkVolumeLevel(std::shared_ptr<Music> backgroundMusicPtr, sf::Vector2f& position) {
    int volumeLevel = 0;

    sf::Vector2i mousePosition = sf::Mouse::getPosition(*windowPtr);
    for (int i = 0; i < 10; i++) {
        if (checkPoints[i].getGlobalBounds().contains(position)) {
            checkPoints[oldVolumeIndex].setFillColor(defCheckpointColor);
            oldVolumeIndex = i;
            
            checkPoints[i].setFillColor(selectedCheckpointColor);
            backgroundMusicPtr->setVolume(volumeLevel);
            SettingsManager::setInt_("VolumeMenu", volumeLevel);
        }
        volumeLevel += 10;
    }
}

void OptionsMainMenu::setTextures() {
    volumeText.setTexture(MainMenuTextureManager::optionsTextVolume);
    backBtn.setTexture(MainMenuTextureManager::cancelText);
}

void OptionsMainMenu::initSprites(std::shared_ptr<Music> backgroundMusicPtr) {
    slider.setSize(sf::Vector2f(500, 10));
    slider.setFillColor(sf::Color(163, 163, 163));
    /* center the sprite */
    slider.setPosition((windowPtr->getSize().x - slider.getSize().x) / 2, slider.getGlobalBounds().height + 105);

    sf::Vector2u spriteSize = volumeText.getTexture().getSize();
    float x = (windowPtr->getSize().x - spriteSize.x) / 2.0f;
    float y = slider.getPosition().y - 100;
    volumeText.getSprite().setPosition(x, 30);

    x = (windowPtr->getSize().x - backBtn.getTexture().getSize().x) / 2.0f;
    backBtn.getSprite().setPosition(20, windowPtr->getSize().y - backBtn.getSprite().getGlobalBounds().height - 20);

    for (int i = 0; i < 10; ++i) {
        checkPoints[i].setSize(sf::Vector2f(15, 50));
        checkPoints[i].setFillColor(defCheckpointColor);
        checkPoints[i].setPosition(sf::Vector2f(slider.getPosition().x + i * (slider.getSize().x / 9), slider.getPosition().y - 21));
    } 

    /* set the current level of music*/
    int volume = backgroundMusicPtr->getVolume();
    if ( volume == 0) {
        checkPoints[0].setFillColor(selectedCheckpointColor);
        this->oldVolumeIndex = 0;
    }
    else {
        checkPoints[volume / 10].setFillColor(selectedCheckpointColor);
        this->oldVolumeIndex = volume / 10;
    }
}
