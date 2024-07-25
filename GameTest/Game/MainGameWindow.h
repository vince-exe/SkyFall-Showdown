#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>

#include "Entity.h"
#include "FontManager.h"
#include "Client.h"
#include "Player.h"
#include "../GameServer/network_utilities.h"
#include "network_game_utilities.h"
#include "Game.h"
#include "TextureManager.h"
#include "GameSettingsMenu.h"
#include "EndGameWindow.h"
#include "SettingsManager.h"
#include "AudioManager.h"

class MainGameWindow {
private:
	sf::RenderWindow m_Window;
	Client* m_Client;
	Game m_Game;

	sf::Text m_myNickname, m_enemyNickname, m_vsText, m_waitRoundText, m_gameTimer;

	float m_rechargeBarProgress;
	sf::RectangleShape m_rechargeBarBorder;
	sf::RectangleShape m_rechargeBar;

	sf::RectangleShape m_youHealth[3];
	sf::RectangleShape m_enemyHealth[3];

	Player m_youPlayer, m_enemyPlayer;

	bool m_displayWindow, m_inGameSettings;
	std::atomic<bool> m_closeSettingsWindowFlag;

	GameSettingsMenu m_gameSettingsMenu;
	EndGameWindow m_endGameWindow;

	std::vector<std::vector<sf::CircleShape>> m_damageAreasVector;

private:
	void setMusicAndSound(AudioManager& audioManager, SettingsManager& settingsManager);

	void update(sf::Time deltaTime, AudioManager& audioManager);

	void draw();

	void initSprites(FontManager& fontManager, SettingsManager& settingsManager);

	void handleKeyBoards(sf::Event event);

	void handleMouseClick(sf::Event& event);

	void quitGame(AudioManager& audioManager);

	void updateRechargeBar();

	void resolvePlayerSprint();

	void checkPlayerWindowBorders();

	bool handleEnemyNickname();

	void handleMessages(AudioManager& audioManager);

	bool initPlayerAndEnemyPosition();

public:
	void init(const std::string nickname, Client& client, TextureManager& textureManager, FontManager& fontManager, SettingsManager& settingsManager, AudioManager& audioManager);
};