#include "MainMenuTextureManager.h"

sf::Texture MainMenuTextureManager::backText;
sf::Texture MainMenuTextureManager::exitText;
sf::Texture MainMenuTextureManager::matchText;
sf::Texture MainMenuTextureManager::settingsText;
sf::Texture MainMenuTextureManager::doneText;
sf::Texture MainMenuTextureManager::optionsTextVolume;
sf::Texture MainMenuTextureManager::confirmationExitText;
sf::Texture MainMenuTextureManager::connectionText;
sf::Texture MainMenuTextureManager::nicknameText;
sf::Texture MainMenuTextureManager::volumeTextLevel[10];
sf::Texture MainMenuTextureManager::menuMsg[4];
sf::Texture MainMenuTextureManager::connectText;
sf::Texture MainMenuTextureManager::cancelText;
sf::Texture MainMenuTextureManager::invalidFormatText;
sf::Texture MainMenuTextureManager::undoMatchText;
sf::Texture MainMenuTextureManager::mainLobbyText;

bool MainMenuTextureManager::init() {
    /* load the volume numbers */
    int pathNumber = 0;
    std::string pathStr;
    for (int i = 0; i < 10; i++) {
        pathStr = "assets/VolumeNumbers/" + std::to_string(pathNumber) + "_Text.png";
        if (!volumeTextLevel[i].loadFromFile(pathStr)) { return false; }
        pathNumber += 10;
    }
    pathNumber = 0;
    pathStr = "";
    /* load the menu texts */
    for (int i = 0; i < 4; i++) {
        pathStr = "assets/MenuTexts/" + std::to_string(pathNumber) + ".png";
        if (!menuMsg[i].loadFromFile(pathStr)) { return false; }
        pathNumber++;
    }

    return mainLobbyText.loadFromFile("assets/MenuTexts/mainLobbyText.png") && backText.loadFromFile("assets/Buttons/backText.png") && exitText.loadFromFile("assets/Buttons/exitText.png")
        && matchText.loadFromFile("assets/Buttons/matchText.png") && settingsText.loadFromFile("assets/Buttons/settingsText.png")
        && doneText.loadFromFile("assets/Buttons/doneText.png") && optionsTextVolume.loadFromFile("assets/VolumeNumbers/music_text.png") && confirmationExitText.loadFromFile("assets/Messages-Texts/img.png")
        && nicknameText.loadFromFile("assets/Messages-Texts/nick_img.png") && connectText.loadFromFile("assets/Buttons/connectText.png")
        && cancelText.loadFromFile("assets/Buttons/cancelText.png") && connectionText.loadFromFile("assets/Messages-Texts/connect_to_server.png")
        && invalidFormatText.loadFromFile("assets/Messages-Texts/invalid_format.png") && undoMatchText.loadFromFile("assets/Buttons/undoMatchText.png");
}
