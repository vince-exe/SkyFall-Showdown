#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Game.h"
#include "Entity.h"
#include "extern_variables.h"

class EndGameWindow {
private:
	sf::RenderWindow* m_Window;
	Game* m_Game;

	bool m_closeWindow;
	sf::Text m_resultsText, m_gameText, m_enemyQuitText;
	
	Entity m_doneButton;

private:
	void initTextures();

	void initSprites();

	void draw(sf::Text& playerNickText, sf::Text& vsText, sf::Text& enemyNickText);

	void handleMouseButtons(sf::Event& event);

public:
	void init(sf::RenderWindow& window, Game& game, sf::Text& playerNickText, sf::Text& vsText, sf::Text& enemyNickText);
};

