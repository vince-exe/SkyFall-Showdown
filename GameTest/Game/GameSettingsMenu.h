#pragma once

#include <iostream>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Entity.h"
#include "extern_variables.h"

class GameSettingsMenu {
private:
	Entity m_backText;

public:
	GameSettingsMenu() = default;

	void setTextures();

	void setSprites(sf::RenderWindow& window);

	void draw(sf::RenderWindow& window);

	void handleMouseButtonPressed(sf::Event& event, sf::RenderWindow& window, bool& inGameSettings);
};

